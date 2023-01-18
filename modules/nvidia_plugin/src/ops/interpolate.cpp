// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <fmt/format.h>

#include <gsl/gsl_assert>
#include <openvino/op/constant.hpp>
#include <sstream>

#include "cuda_operation_registry.hpp"
#include "interpolate_cubic.hpp"
#include "interpolate_linear.hpp"
#include "interpolate_nearest.hpp"
#include "ngraph/shape.hpp"
#include "ngraph/validation_util.hpp"

namespace ov {
namespace nvidia_gpu {

static OperationBase::Ptr interpolateFactory(const CreationContext& context,
                                             const std::shared_ptr<ngraph::Node>& in_node,
                                             OperationBase::IndexCollection&& inputIds,
                                             OperationBase::IndexCollection&& outputIds) {
    auto node = std::dynamic_pointer_cast<ov::op::v4::Interpolate>(in_node);
    Expects(node);

    using InterpolateMode = ov::op::v4::Interpolate::InterpolateMode;
    using IndexCollection = OperationBase::IndexCollection;
    std::stringstream exception_msg;

    switch (node->get_attrs().mode) {
        case InterpolateMode::nearest:
            try {
                return std::make_shared<InterpolateNearestOp>(
                    context, *node, IndexCollection{inputIds}, IndexCollection{outputIds});
            } catch (const std::exception& e) {
                exception_msg << "failed to create InterpolateNearestOp: " << e.what() << "\n";
            }
            break;
        case InterpolateMode::linear:
            try {
                return std::make_shared<InterpolateLinearOp>(
                    context, *node, IndexCollection{inputIds}, IndexCollection{outputIds});
            } catch (const std::exception& e) {
                exception_msg << "failed to create InterpolateLinearOp: " << e.what() << "\n";
            }
            break;
        case InterpolateMode::cubic:
            try {
                return std::make_shared<InterpolateCubicOp>(
                    context, *node, IndexCollection{inputIds}, IndexCollection{outputIds});
            } catch (const std::exception& e) {
                exception_msg << "failed to create InterpolateCubicOp: " << e.what() << "\n";
            }
            break;
        default:
            exception_msg << "not implemented.\n";
            break;
    };

    throwIEException(fmt::format("Interpolate node is not supported: {}", exception_msg.str()));
}

OPERATION_REGISTER_FACTORY(interpolateFactory, Interpolate);

}  // namespace nvidia_gpu
}  // namespace ov
