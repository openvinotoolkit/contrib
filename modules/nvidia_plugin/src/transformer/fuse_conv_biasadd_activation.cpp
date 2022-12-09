// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "openvino/cc/ngraph/itt.hpp"
#include "fuse_conv_biasadd_activation.hpp"

#include <exec_graph_info.hpp>
#include <memory>
#include <ngraph/graph_util.hpp>
#include <ngraph/node.hpp>
#include <ngraph/node_output.hpp>
#include <ngraph/opsets/opset1.hpp>
#include <ngraph/pattern/matcher.hpp>
#include <ngraph/pattern/op/label.hpp>
#include <ngraph/pattern/op/pattern.hpp>
#include "openvino/pass/pattern/op/wrap_type.hpp"
#include <ngraph/rt_info.hpp>
#include <ngraph/shape.hpp>
#include <ngraph/type/element_type.hpp>
#include <ngraph/variant.hpp>
#include <type_traits>
#include <utility>

#include "nodes/cuda_plugin_custom_node_types.hpp"
#include "nodes/fused_convolution.hpp"
#include "nodes/fused_convolution_backprop_data.hpp"

using namespace ov::pass::pattern;

using ov::nvidia_gpu::nodes::FusedConvolution;
using ov::nvidia_gpu::nodes::FusedGroupConvolution;

using ActivationMode = ov::nvidia_gpu::nodes::ActivationMode;
using FusedConvBackpropData = ov::nvidia_gpu::nodes::FusedConvBackpropData;

template <class A, class B>
std::pair<std::shared_ptr<A>, std::shared_ptr<B>> parse_eltwise_inputs(std::shared_ptr<ov::Node> node) {
    auto eltwise = std::dynamic_pointer_cast<A>(node->input(0).get_source_output().get_node_shared_ptr());
    auto constant = std::dynamic_pointer_cast<B>(node->input(1).get_source_output().get_node_shared_ptr());

    if (!eltwise) {
        eltwise = std::dynamic_pointer_cast<A>(node->input(1).get_source_output().get_node_shared_ptr());
        constant = std::dynamic_pointer_cast<B>(node->input(0).get_source_output().get_node_shared_ptr());
    }

    if (!eltwise || !constant) {
        return {nullptr, nullptr};
    }

    return {eltwise, constant};
}

template <typename TFusedConvolution>
struct FusedConvCallbacks {
    static_assert(std::is_same_v<TFusedConvolution, FusedConvolution> ||
                      std::is_same_v<TFusedConvolution, FusedGroupConvolution>,
                  "TFusedConvolution should be either FusedConvolution or FusedGroupConvolution");
    static bool fuse_convolution_with_biasadd(Matcher &m) {
        auto eltwise = m.get_match_root();
        auto [m_conv, m_const] =
            parse_eltwise_inputs<typename TFusedConvolution::BaseConvolution, ov::op::v0::Constant>(eltwise);
        if (!m_conv || !m_const) {
            return false;
        }

        if (m_conv->inputs().size() != 2) {
            return false;
        }

        if (std::dynamic_pointer_cast<ov::op::v1::Add>(eltwise) == nullptr) {
            return false;
        }

        const ov::Output<ov::Node> &data = m_conv->input(0).get_source_output();
        const ov::Output<ov::Node> &filters = m_conv->input(1).get_source_output();
        const ov::Output<ov::Node> &bias = m_const->output(0);

        constexpr auto conv_bias_rank_min{3};
        constexpr auto conv_bias_rank_max{5};
        const auto &bias_shape = bias.get_shape();
        const auto bias_rank = bias_shape.size();
        if (bias_rank < conv_bias_rank_min || bias_rank > conv_bias_rank_max) {
            return false;
        }

        const auto num_spatial_dims = m_conv->get_output_shape(0).size() - 2;
        const auto nchw_channel_dim_reverse_offset = num_spatial_dims + 1;
        const auto output_shape = m_conv->get_output_shape(0);
        if (bias_shape.at(bias_shape.size() - nchw_channel_dim_reverse_offset) !=
            output_shape.at(output_shape.size() - nchw_channel_dim_reverse_offset)) {
            return false;
        }

        if (num_spatial_dims == 3) {
            return false;  // NOTE: 3D convolution fusing was disabled due to 3d_unet bad performance
        }

        auto fused_conv = std::make_shared<TFusedConvolution>(data,
                                                              filters,
                                                              bias,
                                                              m_conv->get_strides(),
                                                              m_conv->get_pads_begin(),
                                                              m_conv->get_pads_end(),
                                                              m_conv->get_dilations(),
                                                              m_conv->get_auto_pad(),
                                                              ActivationMode::NO_ACTIVATION);
        ov::Output<ov::Node> new_conv(fused_conv);

        fused_conv->set_friendly_name(eltwise->get_friendly_name());

        ov::copy_runtime_info({m_conv, eltwise}, new_conv.get_node_shared_ptr());

        const std::string originalLayers = eltwise->get_friendly_name() + "," + m_conv->get_friendly_name();
        fused_conv->get_rt_info()[ExecGraphInfoSerialization::ORIGINAL_NAMES] = originalLayers;

        ov::replace_node(m.get_match_root(), new_conv.get_node_shared_ptr());
        return true;
    }

    static std::pair<std::shared_ptr<TFusedConvolution>, std::shared_ptr<ov::Node>> parse_fusedconv_inputs(
        std::shared_ptr<ov::Node> add) {
        std::shared_ptr<TFusedConvolution> fused_conv = nullptr;
        std::shared_ptr<ov::Node> node = nullptr;
        node = add->input(1).get_source_output().get_node_shared_ptr();
        fused_conv =
            std::dynamic_pointer_cast<TFusedConvolution>(add->input(0).get_source_output().get_node_shared_ptr());
        if (!fused_conv) {
            node = add->input(0).get_source_output().get_node_shared_ptr();
            fused_conv =
                std::dynamic_pointer_cast<TFusedConvolution>(add->input(1).get_source_output().get_node_shared_ptr());
        }

        if (!fused_conv) {
            return {nullptr, nullptr};
        }

        return {fused_conv, node};
    }

    static bool sink_add_to_fused_convolution(Matcher &m) {
        auto add = std::dynamic_pointer_cast<ov::op::v1::Add>(m.get_match_root());
        auto [fused_conv, node] = parse_fusedconv_inputs(m.get_match_root());

        if (fused_conv->has_add_node() || fused_conv->get_activation() != ActivationMode::NO_ACTIVATION) {
            return false;
        }

        const ov::Output<ov::Node> &data = fused_conv->input(0).get_source_output();
        const ov::Output<ov::Node> &filters = fused_conv->input(1).get_source_output();
        const ov::Output<ov::Node> &bias = fused_conv->input(2).get_source_output();

        auto fused_conv_add = std::make_shared<TFusedConvolution>(data,
                                                                  filters,
                                                                  bias,
                                                                  node,
                                                                  fused_conv->get_strides(),
                                                                  fused_conv->get_pads_begin(),
                                                                  fused_conv->get_pads_end(),
                                                                  fused_conv->get_dilations(),
                                                                  fused_conv->get_auto_pad(),
                                                                  ActivationMode::NO_ACTIVATION);
        ov::Output<ov::Node> fused_conv_add_output{fused_conv_add};

        fused_conv_add->set_friendly_name(add->get_friendly_name());
        ov::copy_runtime_info({node, fused_conv}, fused_conv_add);

        auto &rt_info = fused_conv->get_rt_info();
        if (rt_info.count(ExecGraphInfoSerialization::ORIGINAL_NAMES) > 0) {
            auto &rt_info_layer_names = rt_info[ExecGraphInfoSerialization::ORIGINAL_NAMES];
            const auto original_names = rt_info_layer_names.template as<std::string>();
            const std::string original_names_with_activation = add->get_friendly_name() + "," + original_names;
            rt_info_layer_names = original_names_with_activation;
        }

        ov::replace_node(fused_conv, fused_conv_add);
        ov::replace_node(m.get_match_root(), fused_conv_add);

        return true;
    }

    static bool sink_activation_to_fused_convolution(Matcher &m, ActivationMode activation) {
        auto activationNode = m.get_match_root();
        auto fused_conv = std::dynamic_pointer_cast<TFusedConvolution>(
            activationNode->input(0).get_source_output().get_node_shared_ptr());
        if (fused_conv->get_activation() != ActivationMode::NO_ACTIVATION) {
            return false;
        }

        fused_conv->set_activation(activation);
        fused_conv->set_friendly_name(activationNode->get_friendly_name());

        auto &rt_info = fused_conv->get_rt_info();
        if (rt_info.count(ExecGraphInfoSerialization::ORIGINAL_NAMES) > 0) {
            auto &rt_info_layer_names = rt_info[ExecGraphInfoSerialization::ORIGINAL_NAMES];
            const auto original_names = rt_info_layer_names.template as<std::string>();
            const std::string original_names_with_activation =
                activationNode->get_friendly_name() + "," + original_names;
            rt_info_layer_names = original_names_with_activation;
        }

        ov::replace_node(m.get_match_root(), fused_conv);

        return true;
    }
};  // struct FusedConvCallbacks

bool fuse_convolution_backprop_data_with_add(Matcher &m) {
    auto add = std::dynamic_pointer_cast<ov::op::v1::Add>(m.get_match_root());
    auto [conv_backprop_data, add_constant] =
        parse_eltwise_inputs<ov::op::v1::ConvolutionBackpropData, ov::op::v0::Constant>(add);

    const auto conv_element_type = conv_backprop_data->get_input_element_type(0);
    const auto add_element_type = add_constant->get_output_element_type(0);
    if (conv_element_type != add_element_type) {
        return false;
    }

    const auto conv_output_shape = dynamic_cast<ov::Node *>(conv_backprop_data.get())->get_output_shape(0);
    const auto add_output_shape = add_constant->get_output_shape(0);
    const auto size = ov::element::Type(conv_element_type).size();
    const auto conv_in_bytes = size * ov::shape_size(conv_output_shape);
    const auto add_in_bytes = size * ov::shape_size(add_output_shape);
    if (add_in_bytes > conv_in_bytes) {
        return false;
    }

    const ov::Output<ov::Node> &data = conv_backprop_data->input(0).get_source_output();
    const ov::Output<ov::Node> &filters = conv_backprop_data->input(1).get_source_output();
    std::shared_ptr<FusedConvBackpropData> fused_conv_backprop_data_add;

    if (3 == conv_backprop_data->get_input_size()) {
        auto output_shape = conv_backprop_data->input(2).get_source_output();
        fused_conv_backprop_data_add =
            std::make_shared<FusedConvBackpropData>(data,
                                                    filters,
                                                    output_shape,
                                                    add_constant,
                                                    conv_backprop_data->get_strides(),
                                                    conv_backprop_data->get_pads_begin(),
                                                    conv_backprop_data->get_pads_end(),
                                                    conv_backprop_data->get_dilations(),
                                                    conv_backprop_data->get_auto_pad(),
                                                    conv_backprop_data->get_output_padding());
    } else {
        fused_conv_backprop_data_add =
            std::make_shared<FusedConvBackpropData>(data,
                                                    filters,
                                                    add_constant,
                                                    conv_backprop_data->get_strides(),
                                                    conv_backprop_data->get_pads_begin(),
                                                    conv_backprop_data->get_pads_end(),
                                                    conv_backprop_data->get_dilations(),
                                                    conv_backprop_data->get_auto_pad(),
                                                    conv_backprop_data->get_output_padding());
    }

    ov::Output<ov::Node> fused_conv_backprop_data_add_output{fused_conv_backprop_data_add};

    fused_conv_backprop_data_add->set_friendly_name(add->get_friendly_name());
    ov::copy_runtime_info({conv_backprop_data, add}, fused_conv_backprop_data_add);

    auto &rt_info = conv_backprop_data->get_rt_info();
    if (rt_info.count(ExecGraphInfoSerialization::ORIGINAL_NAMES) > 0) {
        auto &rt_info_layer_names = rt_info[ExecGraphInfoSerialization::ORIGINAL_NAMES];
        const auto original_names = rt_info_layer_names.as<std::string>();
        const std::string original_names_with_activation = add->get_friendly_name() + "," + original_names;
        rt_info_layer_names = original_names_with_activation;
    }

    ov::replace_node(add, fused_conv_backprop_data_add);
    ov::replace_node(conv_backprop_data, fused_conv_backprop_data_add);

    return true;
}

ov::nvidia_gpu::pass::FuseConvolutionWithBiasAdd::FuseConvolutionWithBiasAdd() {
    MATCHER_SCOPE(FuseConvolutionWithBiasAdd);
    auto conv = wrap_type<ov::op::v1::Convolution>(consumers_count(1));
    auto add = wrap_type<ov::op::v1::Add>({conv, any_input()});

    matcher_pass_callback callback = [](Matcher &m) {
        return FusedConvCallbacks<FusedConvolution>::fuse_convolution_with_biasadd(m);
    };

    auto m = std::make_shared<Matcher>(add, matcher_name);
    register_matcher(m, callback);
}

ov::nvidia_gpu::pass::FuseGroupConvolutionWithBiasAdd::FuseGroupConvolutionWithBiasAdd() {
    MATCHER_SCOPE(FuseGroupConvolutionWithBiasAdd);
    auto conv = wrap_type<ov::op::v1::GroupConvolution>(consumers_count(1));
    auto add = wrap_type<ov::op::v1::Add>({conv, any_input()});

    matcher_pass_callback callback = [](Matcher &m) {
        return FusedConvCallbacks<FusedGroupConvolution>::fuse_convolution_with_biasadd(m);
    };

    auto m = std::make_shared<Matcher>(add, matcher_name);
    register_matcher(m, callback);
}

ov::nvidia_gpu::pass::FuseConvolutionWithBiasAddAdd::FuseConvolutionWithBiasAddAdd() {
    MATCHER_SCOPE(FuseConvolutionWithBiasAddAdd);
    auto fused_convolution = wrap_type<FusedConvolution>(consumers_count(1));
    auto relu = wrap_type<ov::op::v0::Relu>(consumers_count(2));
    auto add = wrap_type<ov::op::v1::Add>({any_input(), fused_convolution});

    matcher_pass_callback callback = [](Matcher &m) {
        return FusedConvCallbacks<FusedConvolution>::sink_add_to_fused_convolution(m);
    };

    auto m = std::make_shared<Matcher>(add, matcher_name);
    register_matcher(m, callback);
}

ov::nvidia_gpu::pass::FuseGroupConvolutionWithBiasAddAdd::FuseGroupConvolutionWithBiasAddAdd() {
    MATCHER_SCOPE(FuseGroupConvolutionWithBiasAddAdd);
    auto fused_convolution = wrap_type<FusedGroupConvolution>(consumers_count(1));
    auto relu = wrap_type<ov::op::v0::Relu>(consumers_count(2));
    auto add = wrap_type<ov::op::v1::Add>({any_input(), fused_convolution});

    matcher_pass_callback callback = [](Matcher &m) {
        return FusedConvCallbacks<FusedGroupConvolution>::sink_add_to_fused_convolution(m);
    };

    auto m = std::make_shared<Matcher>(add, matcher_name);
    register_matcher(m, callback);
}

ov::nvidia_gpu::pass::SinkReluToFusedConvolution::SinkReluToFusedConvolution() {
    MATCHER_SCOPE(SinkReluToFusedConvolution);
    auto fused_convolution = wrap_type<FusedConvolution>(consumers_count(1));
    auto activation = wrap_type<ov::op::v0::Relu>({fused_convolution});

    matcher_pass_callback callback = [](Matcher &m) {
        return FusedConvCallbacks<FusedConvolution>::sink_activation_to_fused_convolution(m, ActivationMode::RELU);
    };

    auto m = std::make_shared<Matcher>(activation, matcher_name);
    register_matcher(m, callback);
}

ov::nvidia_gpu::pass::SinkSigmoidToFusedConvolution::SinkSigmoidToFusedConvolution() {
    MATCHER_SCOPE(SinkSigmoidToFusedConvolution);
    auto fused_convolution = wrap_type<FusedConvolution>(consumers_count(1));
    auto activation = wrap_type<ov::op::v0::Sigmoid>({fused_convolution});

    matcher_pass_callback callback = [](Matcher &m) {
        return FusedConvCallbacks<FusedConvolution>::sink_activation_to_fused_convolution(m, ActivationMode::SIGMOID);
    };

    auto m = std::make_shared<Matcher>(activation, matcher_name);
    register_matcher(m, callback);
}

ov::nvidia_gpu::pass::SinkTanhToFusedConvolution::SinkTanhToFusedConvolution() {
    MATCHER_SCOPE(SinkTanhToFusedConvolution);
    auto fused_convolution = wrap_type<FusedConvolution>(consumers_count(1));
    auto activation = wrap_type<ov::op::v0::Tanh>({fused_convolution});

    matcher_pass_callback callback = [](Matcher &m) {
        return FusedConvCallbacks<FusedConvolution>::sink_activation_to_fused_convolution(m, ActivationMode::TANH);
    };

    auto m = std::make_shared<Matcher>(activation, matcher_name);
    register_matcher(m, callback);
}

ov::nvidia_gpu::pass::CudaFuseConvBiasAddActivation::CudaFuseConvBiasAddActivation() {
    add_matcher<FuseConvolutionWithBiasAdd>();
    add_matcher<FuseConvolutionWithBiasAddAdd>();
    add_matcher<SinkReluToFusedConvolution>();
    add_matcher<SinkSigmoidToFusedConvolution>();
    // TODO: Uncomment when performance for FusedConvolution+BiasAdd+Tanh would be satisfied
    // add_matcher<SinkTanhToFusedConvolution>();
}

ov::nvidia_gpu::pass::CudaFuseGroupConvBiasAddActivation::CudaFuseGroupConvBiasAddActivation() {
    add_matcher<FuseGroupConvolutionWithBiasAdd>();
    add_matcher<FuseGroupConvolutionWithBiasAddAdd>();
    // TODO: Activations, should check performance first
}

ov::nvidia_gpu::pass::CudaFuseConvBackpropDataAdd::CudaFuseConvBackpropDataAdd() {
    MATCHER_SCOPE(CudaFuseConvBackpropDataAdd);
    auto conv_backprop_data =
        wrap_type<ov::op::v1::ConvolutionBackpropData>(consumers_count(1));
    auto add = wrap_type<ov::op::v1::Add>({conv_backprop_data, any_input()});

    matcher_pass_callback callback = [](Matcher &m) {
        return fuse_convolution_backprop_data_with_add(m);
    };

    auto m = std::make_shared<Matcher>(add, matcher_name);
    register_matcher(m, callback);
}
