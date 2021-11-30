// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <fmt/format.h>

#include <cuda_operation_base.hpp>
#include <cuda_operation_registry.hpp>
#include <ngraph/op/clamp.hpp>
#include <sstream>

#include "clamp_cudnn.hpp"
#include "clipped_relu_cudnn.hpp"

namespace CUDAPlugin {

using IndexCollection = OperationBase::IndexCollection;

static OperationBase::Ptr clampFactory(const CreationContext& context,
                                       const std::shared_ptr<ngraph::Node>& node,
                                       IndexCollection&& inputIds,
                                       IndexCollection&& outputIds) {
    const ngraph::op::Clamp& node_op{downcast<const ngraph::op::Clamp>(node)};

    const IndexCollection inputs{inputIds};
    const IndexCollection outputs{outputIds};

    std::stringstream exception_msg;
    try {
        return std::make_shared<ClippedReluCuDnnOp>(
            context, node_op, IndexCollection{inputIds}, IndexCollection{outputIds});
    } catch (const std::exception& e) {
        exception_msg << "Failed to create ClippedReluCuDnnOp implementation: " << e.what();
    }
    try {
        return std::make_shared<ClampCuDnnOp>(context, node_op, IndexCollection{inputIds}, IndexCollection{outputIds});
    } catch (const std::exception& e) {
        exception_msg << "\nFailed to create ClampCuDnnOp implementation: " << e.what();
    }
    throwIEException(fmt::format("Clamp node is not supported:\n{}", exception_msg.str()));
}

OPERATION_REGISTER_FACTORY(clampFactory, Clamp)

}  // namespace CUDAPlugin
