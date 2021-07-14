// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <vector>
#include <cuda_runtime.h>
#include <gsl/gsl_assert>
#include <cuda/device.hpp>
#include <cuda_operation_registry.hpp>
#include <utility>

#include <cuda/device.hpp>

#include "details/cuda_ngraph_import.hpp"
#include "sigmoid.hpp"

namespace CUDAPlugin {

static __global__ void sigmoid(const size_t inputSize, const float *x, float *y) {
    const unsigned i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < inputSize) {
        y[i] = 1 / (1 + expf(-x[i]));
    }
}

SigmoidOp::SigmoidOp(const CUDA::CreationContext& context,
                     const std::shared_ptr<ngraph::Node>& node,
                     IndexCollection&& inputIds,
                     IndexCollection&& outputIds)
    : OperationBase(context, node, std::move(inputIds), std::move(outputIds)) {
    auto input_element_type = node->get_input_element_type(0);
    auto output_element_type = node->get_output_element_type(0);
    Expects(input_element_type.is_real());
    Expects(output_element_type.is_real());
    auto input_shape = node->get_input_shape(0);
    auto output_shape = node->get_output_shape(0);
    input_size_ = std::accumulate(input_shape.begin(), input_shape.end(), 1, std::multiplies<size_t>());
    output_size_ = std::accumulate(output_shape.begin(), output_shape.end(), 1, std::multiplies<size_t>());
}

void SigmoidOp::Execute(const InferenceRequestContext& context, Inputs inputs, Outputs outputs, 
                        const Workbuffers& workbuffers) {
    Expects(inputs.size() == 1);
    Expects(outputs.size() == 1);
    auto& tc = context.getThreadContext();
    auto& stream = tc.stream();
    const unsigned maxBlockSize = tc.device().props().maxThreadsPerBlock;
    const unsigned numBlocks = (input_size_ % maxBlockSize == 0) ?
                               (input_size_ / maxBlockSize) :
                               (input_size_ / maxBlockSize + 1);
    const unsigned threadsPerBlock = (numBlocks == 1) ? input_size_ : maxBlockSize;
    stream.run(
            numBlocks, threadsPerBlock,
            sigmoid,
            input_size_,
            static_cast<const float *>(inputs[0].get()),
            static_cast<float *>(outputs[0].get()));
}

OPERATION_REGISTER(SigmoidOp, Sigmoid);
} // namespace CUDAPlugin
