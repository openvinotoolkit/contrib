// Copyright (C) 2021-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//
#include "tanh.hpp"

#include <cuda/dnn.hpp>
#include <cuda_operation_registry.hpp>

#include "converters.hpp"
#include "cuda/constant_factory.hpp"

namespace ov {
namespace nvidia_gpu {

TanhOp::TanhOp(const CreationContext& context,
               const std::shared_ptr<ov::Node>& node,
               IndexCollection&& inputIds,
               IndexCollection&& outputIds)
    : ActivationForwardCuDnnOpBase{
          std::make_unique<CUDA::TanhDescriptor>(), context, *node, move(inputIds), move(outputIds)} {}

OPERATION_REGISTER(TanhOp, Tanh);
}  // namespace nvidia_gpu
}  // namespace ov
