// Copyright (C) 2022-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "elementwise_binary.hpp"
#include "kernels/divide.hpp"
#include "openvino/op/divide.hpp"

namespace ov {
namespace nvidia_gpu {

class DivideOp : public ElementwiseBinaryOp<ov::op::v1::Divide, kernel::Divide> {
public:
    using ElementwiseBinaryOp::ElementwiseBinaryOp;
};
class PythonDivideOp : public ElementwiseBinaryOp<ov::op::v1::Divide, kernel::PythonDivide> {
public:
    using ElementwiseBinaryOp::ElementwiseBinaryOp;
};

}  // namespace nvidia_gpu
}  // namespace ov
