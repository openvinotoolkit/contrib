// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <cuda/device_pointers.hpp>
#include <cuda_operation_base.hpp>

namespace CUDAPlugin {

class ParameterOp : public OperationBase {
public:
    ParameterOp(const CreationContext& context,
                const ngraph::Node& node,
                IndexCollection&& inputIds,
                IndexCollection&& outputIds);
    void Execute(const InferenceRequestContext& context,
                 Inputs inputTensors,
                 Outputs outputTensors,
                 const Workbuffers& workbuffers) const override;
    static std::string GetInputTensorName(const ngraph::Node& node);

private:
    std::string input_tensor_name_;
};

}  // namespace CUDAPlugin
