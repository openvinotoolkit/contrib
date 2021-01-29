// Copyright (C) 2020-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <details/ie_exception.hpp>

#include <arm_compute/runtime/NEON/functions/NEConvolutionLayer.h>
#include <arm_compute/runtime/NEON/functions/NEDepthwiseConvolutionLayer.h>
#include "arm_converter/arm_converter.hpp"

namespace ArmPlugin {

using FUNC = opset::ActivationFunction;

enum Input {Features, Weights, Bias};
template<typename Conv>
static auto ConvParamters(const Conv& node) {
    unsigned int pad_l    = node.get_pads_begin().at(D2::W);
    unsigned int pad_r    = node.get_pads_end().at(D2::W);
    unsigned int pad_t    = node.get_pads_begin().at(D2::H);
    unsigned int pad_b    = node.get_pads_end().at(D2::H);
    unsigned int stride_x = node.get_strides().at(D2::W);
    unsigned int stride_y = node.get_strides().at(D2::H);

    return std::make_pair(
        arm_compute::PadStrideInfo {stride_x, stride_y, pad_l, pad_r, pad_t, pad_b, arm_compute::DimensionRoundingType::FLOOR},
        arm_compute::Size2D {node.get_dilations().at(D2::W), node.get_dilations().at(D2::H)});
}

template<typename Conv>
static arm_compute::ActivationLayerInfo GetActivation(const Conv& node) {
    opset::ActivationInfo info = node.get_info();
    arm_compute::ActivationLayerInfo::ActivationFunction func;
    switch (info.function) {
        case FUNC::LOGISTIC        : func = arm_compute::ActivationLayerInfo::ActivationFunction::LOGISTIC; break;
        case FUNC::TANH            : func = arm_compute::ActivationLayerInfo::ActivationFunction::TANH; break;
        case FUNC::RELU            : func = arm_compute::ActivationLayerInfo::ActivationFunction::RELU; break;
        case FUNC::LU_BOUNDED_RELU : func = arm_compute::ActivationLayerInfo::ActivationFunction::LU_BOUNDED_RELU; break;
        case FUNC::LEAKY_RELU      : func = arm_compute::ActivationLayerInfo::ActivationFunction::LEAKY_RELU; break;
        case FUNC::SOFT_RELU       : func = arm_compute::ActivationLayerInfo::ActivationFunction::SOFT_RELU; break;
        case FUNC::ELU             : func = arm_compute::ActivationLayerInfo::ActivationFunction::ELU; break;
        case FUNC::ABS             : func = arm_compute::ActivationLayerInfo::ActivationFunction::ABS; break;
        case FUNC::SQRT            : func = arm_compute::ActivationLayerInfo::ActivationFunction::SQRT; break;
        case FUNC::HARD_SWISH      : func = arm_compute::ActivationLayerInfo::ActivationFunction::HARD_SWISH; break;
        case FUNC::IDENTITY        : return arm_compute::ActivationLayerInfo();
    default:
        THROW_IE_EXCEPTION << "Arm Plugin: unsupported activation function for Convolution";
    }
    return arm_compute::ActivationLayerInfo(func, info.a, info.b);
}

template<> Converter::Conversion::Ptr Converter::Convert(const opset::ArmConvolution& node) {
    arm_compute::PadStrideInfo conv_info;
    arm_compute::Size2D dilation;
    std::tie(conv_info, dilation) = ConvParamters(node);
    auto act_info = GetActivation(node);
    if (node.get_input_size() == 3) {
        return MakeConversion<arm_compute::NEConvolutionLayer>(
            node.input(Features), node.input(Weights), node.input(Bias), node.output(0),
            conv_info, arm_compute::WeightsInfo{}, dilation, act_info);
    } else {
        return MakeConversion<arm_compute::NEConvolutionLayer>(
            node.input(Features), node.input(Weights), nullptr, node.output(0),
            conv_info, arm_compute::WeightsInfo{}, dilation, act_info);
    }
}

static auto MakeWeightsArgument(const opset::GroupConvolution& node) {
    auto ngraphWeightsShape = node.input(Weights).get_shape();
    return std::make_pair(
        node.input(Weights),
        arm_compute::TensorInfo{
            ShapeCast({
                ngraphWeightsShape[1],
                ngraphWeightsShape[0]*ngraphWeightsShape[2],
                ngraphWeightsShape[3],
                ngraphWeightsShape[4]
            }),
            1,
            DataTypeCast(node.input(Weights).get_element_type())});
}

template<> Converter::Conversion::Ptr Converter::Convert(const opset::ArmGroupConvolution& node) {
    arm_compute::PadStrideInfo conv_info;
    arm_compute::Size2D dilation;
    std::tie(conv_info, dilation) = ConvParamters(node);
    auto act_info = GetActivation(node);
    if (node.get_input_size() == 3) {
        return MakeConversion<arm_compute::NEDepthwiseConvolutionLayer>(
            node.input(Features), MakeWeightsArgument(node), node.input(Bias), node.output(0),
            conv_info, 1u, act_info, dilation);
    } else {
        return MakeConversion<arm_compute::NEDepthwiseConvolutionLayer>(
            node.input(Features), MakeWeightsArgument(node), nullptr, node.output(0),
            conv_info, 1u, act_info, dilation);
    }
}
}  //  namespace ArmPlugin
