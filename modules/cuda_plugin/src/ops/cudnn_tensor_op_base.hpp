// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <cuda_operation_base.hpp>
#include <ngraph/node.hpp>

namespace CUDAPlugin {

class CuDnnTensorOpBase : public OperationCuDnn {
  public:
    static constexpr std::size_t max_supported_shape_size = 5;

    CuDnnTensorOpBase(const std::shared_ptr<ngraph::Node>& node,
                   IndexCollection&& inputIds, IndexCollection&& outputIds,
                   const cudnnOpTensorOp_t& opType,
                   const cudnnNanPropagation_t& nanPropogationType =
                      cudnnNanPropagation_t::CUDNN_PROPAGATE_NAN);
    void Execute(const InferenceRequestContext& context, Inputs inputTensors,
                Outputs outputTensors) override;

  private:
    struct IoParams {
      const cudnnDataType_t type_;
      const ngraph::Shape shape_;
      std::array<int, 5> array_;
      CUDA::DnnTensorDescriptor desc_;
      enum class Type { INPUT, OUTPUT };

      IoParams(const ngraph::Node& node, const Type& io_type, int index);
    };

    IoParams in0;
    IoParams in1;
    IoParams out;
    CUDA::DnnOpTensorDescriptor op_desc_;
    CUDA::ScalingParameters scaling_params_;
    int bias_index_ = 0;
    int dest_index_ = 1;
};
}  // namespace CUDAPlugin
