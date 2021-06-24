// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <array>
#include <ngraph/op/convolution.hpp>
#include <ngraph/type/element_type.hpp>
#include "ngraph/attribute_adapter.hpp"
#include "ngraph/ngraph_visibility.hpp"
#include "ngraph/type.hpp"

namespace CUDAPlugin::nodes {

class Conv2DBiasAddActivation : public ngraph::op::Op {
 public:

  /// Mirrors the cuDNN cudnnActivationMode_t enum
  enum class SupportedActivation {
    SIGMOID,
    RELU,
    TANH,
    CLIPPED_RELU,
    ELU,
    NO_ACTIVATION
  };

  explicit Conv2DBiasAddActivation(const ngraph::Output<Node>& data_batch,
                           const ngraph::Output<Node>& filters,
                           const ngraph::Output<Node>& bias,
                           const ngraph::Strides& strides,
                           const ngraph::CoordinateDiff& pads_begin,
                           const ngraph::CoordinateDiff& pads_end,
                           const ngraph::Strides& dilations,
                           const ngraph::op::PadType& auto_pad,
                           SupportedActivation activation);

  inline static constexpr type_info_t type_info{"ConvolutionBiasAddActivation", 0};
  const type_info_t& get_type_info() const override { return type_info; }

  bool visit_attributes(ngraph::AttributeVisitor& visitor) override;

  std::shared_ptr<ngraph::Node> clone_with_new_inputs(
      const ngraph::OutputVector& new_args) const override;

  void validate_and_infer_types() override;

  void set_activation(SupportedActivation act);
  SupportedActivation get_activation();

 private:
  /// Used for the shape validation
  ngraph::op::v1::Convolution conv_op_;
  ngraph::Shape bias_shape_;
  ngraph::element::Type bias_type_;

  SupportedActivation activation_;
};

}  // namespace CUDAPlugin::nodes
