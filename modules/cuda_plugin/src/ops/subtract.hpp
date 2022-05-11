// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <ngraph/op/subtract.hpp>

#include "elementwise_binary.hpp"
#include "kernels/subtract.hpp"

namespace CUDAPlugin {

using SubtractOp = ElementwiseBinaryOp<ngraph::op::v1::Subtract, kernel::Subtract>;

}  // namespace CUDAPlugin
