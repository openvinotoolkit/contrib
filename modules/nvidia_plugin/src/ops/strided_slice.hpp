// Copyright (C) 2021-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <cuda_operation_base.hpp>
#include <openvino/op/strided_slice.hpp>

#include "kernels/strided_slice.hpp"
#include "ngraph/slice_plan.hpp"

namespace ov {
namespace nvidia_gpu {

class StridedSliceOp : public OperationBase {
public:
    using NodeOp = ov::op::v1::StridedSlice;
    StridedSliceOp(const CreationContext& context,
                   const NodeOp& stridedSliceOp,
                   IndexCollection&& inputIds,
                   IndexCollection&& outputIds);
    void Execute(const InferenceRequestContext& context,
                 Inputs inputs,
                 Outputs outputs,
                 const Workbuffers& workbuffers) const override;
    WorkbufferRequest GetWorkBufferRequest() const override;
    void InitSharedImmutableWorkbuffers(const Buffers& buffers) override;

private:
    template <typename T>
    void callKernels(const InferenceRequestContext& context,
                     Inputs inputs,
                     Outputs outputs,
                     const Workbuffers& workbuffers) const;
    template <typename T>
    void callStridedSliceKernel(const InferenceRequestContext& context,
                                const Inputs inputs,
                                Outputs outputs,
                                const Workbuffers& workbuffers) const;
    template <typename T>
    void callReverseAxesKernel(const InferenceRequestContext& context, Outputs outputs) const;
    template <typename T>
    void callReverseAxesKernel(const InferenceRequestContext& context,
                               const std::vector<size_t>& matrixShapes,
                               const std::vector<int64_t>& matrixSizes,
                               const ov::AxisSet& reverseAxes,
                               CUDA::DevicePointer<void*> buffer) const;
    void uploadDataToWorkbuffer(CUDA::DevicePointer<void*> buffer, const std::vector<int64_t>& data);

    std::vector<int64_t> getNodeConstantValues(const ov::Node* node) const;

private:
    std::vector<int64_t> src_matrix_sizes_;
    std::vector<int64_t> dst_matrix_sizes_;

    ngraph::SlicePlan slice_plan_;

    unsigned max_threads_per_block_{0};
    unsigned blocks_number_{0};
    unsigned threads_per_block_{0};
    ov::element::Type_t element_type_;

    std::optional<kernel::StridedSliceKernelOp> kernel_op_;
};

}  // namespace nvidia_gpu
}  // namespace ov
