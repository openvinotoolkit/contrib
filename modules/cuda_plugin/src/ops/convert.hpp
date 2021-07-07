// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <cuda_operation_base.hpp>
#include <gpu/device_pointers.hpp>
#include <cuda/runtime.hpp>
#include <ngraph/type/element_type.hpp>

namespace CUDAPlugin {

class ConvertOp : public OperationBase {
 public:
    ConvertOp(const CUDA::Device& device, const std::shared_ptr<ngraph::Node>& node,
              IndexCollection&& inputIds, IndexCollection&& outputIds);
    void Execute(const InferenceRequestContext& context,
                 Inputs inputTensors,
                 Outputs outputTensors,
                 const Workbuffers& workbuffers) override;
    using Type_t = ngraph::element::Type_t;
    using convert_t = void(*)(const CUDA::Stream&, size_t,
                         InferenceEngine::gpu::DevicePointer<void*>,
                         InferenceEngine::gpu::DevicePointer<const void*>,
                         unsigned, unsigned);
 private:
    static convert_t getConvertKernel(Type_t output_type, Type_t input_type);
    convert_t convert_kernel_;
    size_t size_;
};

} // namespace CUDAPlugin
