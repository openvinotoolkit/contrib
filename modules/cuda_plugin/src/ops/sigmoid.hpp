// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <cuda_operation_base.hpp>
#include <gpu/device_pointers.hpp>

namespace CUDAPlugin {

class SigmoidOp : public OperationBase {
 public:
    SigmoidOp(const std::shared_ptr<ngraph::Node>& node,
              std::vector<unsigned> inputIds,
              std::vector<unsigned> outputIds);
    void Execute(const InferenceRequestContext& context,
                 Inputs inputTensors,
                 Outputs outputTensors) override;

 private:
    size_t input_size_;
    size_t output_size_;
};

} // namespace CUDAPlugin
