// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <gsl/gsl_assert>
#include <ngraph/node.hpp>
#include <cuda_operation_registry.hpp>
#include <utility>

#include <ngraph/op/constant.hpp>
#include <ngraph/op/matmul.hpp>
#include <cublas_v2.h>
#include <cuda/cublas_handle.hpp>

#include "converters.hpp"
#include "matmul.hpp"

namespace CUDAPlugin {

MatMulOp::MatMulOp(const std::shared_ptr<ngraph::Node>& node,
                   std::vector<unsigned>&& inputIds,
                   std::vector<unsigned>&& outputIds)
    : OperationBase(node, std::move(inputIds), std::move(outputIds)) {
    auto op = dynamic_cast<ngraph::op::v0::MatMul*>(node.get());
    Expects(op);
    Expects(op->get_input_size() == 2);
    Expects(op->get_output_size() == 1);
    Expects(convertDataType<cudaDataType_t>(op->get_input_element_type(0)) == convertDataType<cudaDataType_t>(op->get_input_element_type(1)));
    data_type_ = convertDataType<cudaDataType_t>(op->get_input_element_type(0));
    compute_type_ = GetComputeType(data_type_, convertDataType<cudaDataType_t>(op->get_output_element_type(0)));
    alpha_ = ScalingAlphaConstant(compute_type_);
    beta_ = ScalingBetaConstant(compute_type_);
    auto inputAShape = op->get_input_shape(0);
    auto inputBShape = op->get_input_shape(1);
    auto outputCShape = op->get_output_shape(0);
    Expects(inputAShape.size() > 0);
    Expects(inputBShape.size() > 0);
    bool transposeA = op->get_transpose_a();
    bool transposeB = op->get_transpose_b();
    BroadcastShapes(inputAShape, transposeA, inputBShape, transposeB, outputCShape);
    int batchACount = GetNumBatches(inputAShape);
    int batchBCount = GetNumBatches(inputBShape);
    batch_count_ = std::max(batchACount, batchBCount);
    const size_t rowsA = *(inputAShape.end()-!transposeA-1);
    const size_t colsA = *(inputAShape.end()-transposeA-1);
    const size_t rowsB = *(inputBShape.end()-!transposeB-1);
    const size_t colsB = *(inputBShape.end()-transposeB-1);
    Expects(colsA == rowsB);
    m_ = rowsA;
    k_ = colsA;
    n_ = colsB;
    ld_a_ = *(inputAShape.end()-1);
    ld_b_ = *(inputBShape.end()-1);
    ld_c_ = *(outputCShape.end()-1);
    stride_a_ = (batchACount > 1) ? (m_ * k_) : 0;
    stride_b_ = (batchBCount > 1) ? (k_ * n_) : 0;
    stride_c_ = (m_ * n_);
    cublas_transpose_a_ = transposeA ? CUBLAS_OP_T : CUBLAS_OP_N;
    cublas_transpose_b_ = transposeB ? CUBLAS_OP_T : CUBLAS_OP_N;
    Ensures(m_ != 0);
    Ensures(k_ != 0);
    Ensures(n_ != 0);
    Ensures(ld_a_ != 0);
    Ensures(ld_b_ != 0);
    Ensures(ld_c_ != 0);
    Ensures(batch_count_ != 0);
    Ensures(alpha_ != nullptr);
    Ensures(beta_ != nullptr);
}

cudaDataType_t MatMulOp::GetComputeType(const cudaDataType_t abDataType, const cudaDataType_t cDataType) {
    constexpr auto SwitchCase = [](cudaDataType_t a, cudaDataType_t b) constexpr {
        return (a << 16) + b;
    };
    /**
     * NOTE: This switch is an implementation of CuBlas table for available compute types:
     * @reference https://docs.nvidia.com/cuda/cublas/index.html#cublas-GemmStridedBatchedEx
     */
    switch (SwitchCase(abDataType, cDataType)) {
        case SwitchCase(CUDA_R_16F, CUDA_R_16F): {
            return CUDA_R_16F;
        }
        case SwitchCase(CUDA_R_8I, CUDA_R_32I): {
            return CUDA_R_32I;
        }
        case SwitchCase(CUDA_R_16BF, CUDA_R_16BF):
        case SwitchCase(CUDA_R_8I, CUDA_R_32F):
        case SwitchCase(CUDA_R_16BF, CUDA_R_32F):
        case SwitchCase(CUDA_R_16F, CUDA_R_32F):
        case SwitchCase(CUDA_R_32F, CUDA_R_32F):
        case SwitchCase(CUDA_C_8I, CUDA_C_32F):
        case SwitchCase(CUDA_C_32F, CUDA_C_32F): {
            return CUDA_R_32F;
        }
        case SwitchCase(CUDA_R_64F, CUDA_R_64F):
        case SwitchCase(CUDA_C_64F, CUDA_C_64F): {
            return CUDA_R_64F;
        }
        default: THROW_IE_EXCEPTION << fmt::format("Not supported combination of A and B types [{}] with C type [{}]",
                                                   abDataType, cDataType);
    }
}

const void* MatMulOp::ScalingAlphaConstant(cudaDataType_t computeType) {
    switch (computeType) {
        case CUDA_R_64F: {
            static constexpr double alpha = 1.0;
            return &alpha;
        }
        case CUDA_R_32F: {
            static constexpr float alpha = 1.0;
            return &alpha;
        }
        case CUDA_R_16F: {
            static const __half alpha = 1.0;
            return &alpha;
        }
        case CUDA_R_32I: {
            static constexpr uint32_t alpha = 1;
            return &alpha;
        }
        default: THROW_IE_EXCEPTION << fmt::format("Not supported computeType [{}]", computeType);
    }
}

const void* MatMulOp::ScalingBetaConstant(cudaDataType_t computeType) {
    switch (computeType) {
        case CUDA_R_64F: {
            static double beta = 0.0;
            return &beta;
        }
        case CUDA_R_32F: {
            static float beta = 0.0;
            return &beta;
        }
        case CUDA_R_16F: {
            static __half beta = 0.0;
            return &beta;
        }
        case CUDA_R_32I: {
            static uint32_t beta = 0;
            return &beta;
        }
        default: THROW_IE_EXCEPTION << fmt::format("Not supported computeType [{}]", computeType);
    }
}

int MatMulOp::GetNumBatches(const ngraph::Shape& matrixShape) {
    Expects(matrixShape.size() >= 2);
    return std::accumulate(matrixShape.begin(), matrixShape.end()-2, 1, std::multiplies<size_t>());
}

void MatMulOp::BroadcastShapes(ngraph::Shape& matrixAShape,
                               bool& transposeA,
                               ngraph::Shape& matrixBShape,
                               bool& transposeB,
                               ngraph::Shape& matrixCShape) {
    /**
     * NOTE: See NGraph documentation for broadcasting:
     * @reference https://docs.openvinotoolkit.org/latest/openvino_docs_ops_matrix_MatMul_1.html
     */
    if (matrixAShape.size() == 1 && matrixBShape.size() == 1) {
        // 1D x 1D: [X] x [X] -> [1, X] x [X, 1] -> [1, 1] => [] (scalar)
        matrixAShape = ngraph::Shape{1, matrixAShape[0]};
        matrixBShape = ngraph::Shape{matrixBShape[0], 1};
        transposeA = false;
        transposeB = false;
    } else if (matrixAShape.size() == 1 && matrixBShape.size() > 1) {
        // 1D x ND: [X] x [B, ..., X, Y] -> [1, X] x [B, ..., X, Y] -> [B, ..., 1, Y] => [B, ..., Y]
        matrixAShape = ngraph::Shape{1, matrixAShape[0]};
        transposeA = false;
    } else if (matrixAShape.size() > 1 && matrixBShape.size() == 1) {
        // ND x 1D: [B, ..., X, Y] x [Y] -> [B, ..., X, Y] x [Y, 1] -> [B, ..., X, 1] => [B, ..., X]
        matrixBShape = ngraph::Shape{matrixBShape[0], 1};
        transposeB = false;
    } else if (matrixAShape.size() > 1 && matrixBShape.size() > 1) {
        // ND x ND: [B, ..., X, Y] x [B, ..., Y, Z] => [B, ..., X, Z]
        Expects(GetNumBatches(matrixAShape) == GetNumBatches(matrixBShape));
    }
    Expects(*(matrixAShape.end()-transposeA-1) == *(matrixBShape.end()-!transposeB-1));
    if (matrixAShape.size() > matrixBShape.size()) {
        matrixCShape = matrixAShape;
    } else {
        matrixCShape = matrixBShape;
    }
    *(matrixCShape.end()-2) = *(matrixAShape.end()-!transposeA-1);
    *(matrixCShape.end()-1) = *(matrixBShape.end()-transposeB-1);
}

// NOTE: Multiply the arrays A and B on GPU and save the result in C
// C(m,n) = A(m,k) * B(k,n), C is stored as row-major matrix
void MatMulOp::Execute(const InferenceRequestContext& context, Inputs inputs, Outputs outputs) {
    Expects(inputs.size() == 2);
    Expects(outputs.size() == 1);
    auto& cuBlasHandle = context.getThreadContext().cuBlasHandle();
    auto matrixA = inputs[0];
    auto matrixB = inputs[1];
    auto matrixC = outputs[0];

    /**
     * NOTE: A and B are switched in places. A returns k as leading dimension and B returns n.
     *       Such workaround is done, because cuBlas works with column-major matrices,
     *       but we need to get output row-major matrix
     *       Instead of computing C = A x B (cuBlas will return in column-major format),
     *       we compute Ct = Bt x At (where t means transposed)
     *       As result Ct would be row-major matrix
     */
    CUDA::throwIfError(cublasGemmStridedBatchedEx(
        cuBlasHandle.get(), cublas_transpose_b_, cublas_transpose_a_,
        n_, m_, k_,
        alpha_,
        matrixB.get(), data_type_, ld_b_, stride_b_,
        matrixA.get(), data_type_, ld_a_, stride_a_,
        beta_,
        matrixC.get(), data_type_, ld_c_, stride_c_,
        batch_count_,
        compute_type_,
        CUBLAS_GEMM_DEFAULT));
}

OPERATION_REGISTER(MatMulOp, MatMul);
} // namespace CUDAPlugin
