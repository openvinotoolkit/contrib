// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "transpose.hpp"
#include <cuda_operation_registry.hpp>
#include <ngraph/op/constant.hpp>
#include <algorithm>
#include <gsl/gsl_assert>
#include <cuda/cutensor.hpp>
#include <fmt/core.h>

using namespace std::string_literals;

namespace CUDAPlugin {

constexpr cudaDataType_t toDataType(const ngraph::element::Type_t& type) {
    using ngraph::element::Type_t;
    switch (type) {
    case Type_t::bf16:
        return CUDA_R_16BF;
    case Type_t::f16:
        return CUDA_R_16F;
    case Type_t::f32:
        return CUDA_R_32F;
    case Type_t::f64:
        return CUDA_R_64F;
    case Type_t::i8:
        return CUDA_R_8I;
    case Type_t::i16:
        return CUDA_R_16I;
    case Type_t::i32:
        return CUDA_R_32I;
    case Type_t::i64:
        return CUDA_R_64I;
    case Type_t::u8:
        return CUDA_R_8U;
    case Type_t::u16:
        return CUDA_R_16U;
    case Type_t::u32:
        return CUDA_R_32U;
    case Type_t::u64:
        return CUDA_R_64U;
    default:
        CUDA::throwIEException("Invalid type conversion. Attempting to convert "s
            + ngraph::element::Type {type}.c_type_string() + "to cuda type."s);
    }
}


TransposeOp::TransposeOp(const std::shared_ptr<ngraph::Node>& node,
    std::vector<unsigned>&& inputIds, std::vector<unsigned>&& outputIds) :
        OperationBase(node, std::move(inputIds), std::move(outputIds)),
        inputExtents_ { extractInputExtents(*node) },
        dimsNumber_ { inputExtents_.size() },
        outputExtents_ { extractOutputExtents(*node) },
        inputStrides_ { extractInputStrides(*node) },
        outputStrides_ { extractOutputStrides(*node) },
        inputMode_ { extractInputMode(dimsNumber_) },
        outputMode_ { tryToExtractPermutation(*node) },
        extents_ { extractExtents(inputExtents_) },
        inputElementsType_ { toDataType(node->input(0).get_element_type()) },
        permutationElementsType_ { extractPermutationElementsType(*node) },
        alpha_ { makeAlpha(inputElementsType_) } {
    inputExtents_.size();
}


void TransposeOp::Execute(const InferenceRequestContext& context,
    Inputs inputTensors, Outputs outputTensors) {
    Expects(inputTensors.size() == 1 || inputTensors.size() == 2);
    Expects(outputTensors.size() == 1);

    cutensorTensorDescriptor_t inputDesc { }, outputDesc { };
    const std::vector<int> outputMode = permutation(context, inputTensors);
    auto& threadContext = context.getThreadContext();

    cutensorInitTensorDescriptor(&threadContext.cuTensorHandle().get(),
            &inputDesc,
            dimsNumber_,
            inputExtents_.data(),
            inputStrides_.data(),
            inputElementsType_,
            CUTENSOR_OP_IDENTITY);

    cutensorInitTensorDescriptor(&threadContext.cuTensorHandle().get(),
            &outputDesc,
            dimsNumber_,
            outputExtents_.data(),
            outputStrides_.data(),
            inputElementsType_,
            CUTENSOR_OP_IDENTITY);

    CUDA::throwIfError(cutensorPermutation(&threadContext.cuTensorHandle().get(),
            &alpha_,
            inputTensors[0].get(),
            &inputDesc,
            inputMode_.data(),
            outputTensors[0].get(),
            &outputDesc,
            outputMode.data(),
            inputElementsType_,
            context.getThreadContext().stream().get()));
}


std::vector<std::int64_t> TransposeOp::extractInputExtents(const ngraph::Node& node) {
    std::vector<std::int64_t> result;
    auto inputShape = node.input(0).get_shape();
    result.reserve(inputShape.size());
    for (auto extent : inputShape)
        result.emplace_back(extent);
    return result;
}


std::vector<std::int64_t> TransposeOp::extractOutputExtents(const ngraph::Node& node) {
    std::vector<std::int64_t> result;
    auto outputShape = node.output(0).get_shape();
    result.reserve(outputShape.size());
    for (auto extent : outputShape)
        result.emplace_back(extent);
    return result;
}


std::vector<std::int64_t> TransposeOp::extractInputStrides(const ngraph::Node& node) {
    std::vector<std::int64_t> result;
    auto inputShape = node.input(0).get_shape();
    result.reserve(inputShape.size());
    const auto numInputShapeElements = inputShape.size();
    for (std::size_t i = 0; i < numInputShapeElements; i++)
        result.push_back(ngraph::row_major_stride(inputShape, i));
    return result;
}


TransposeOp::ExtentsMap TransposeOp::extractExtents(const std::vector<std::int64_t>& input_extents) {
    ExtentsMap result;
    const auto numInputExtents = input_extents.size();
    for (std::size_t i = 0; i < numInputExtents; i++)
        result.emplace(i, input_extents[i]);
    return result;
}


std::vector<int> TransposeOp::extractInputMode(std::size_t numDims) {
    std::vector<int> result;
    for (int i = 0; i < numDims; i++)
        result.emplace_back(i);
    return result;
}

std::vector<std::int64_t> TransposeOp::extractOutputStrides(const ngraph::Node& node) {
    std::vector<std::int64_t> result;
    auto outputShape = node.output(0).get_shape();
    result.reserve(outputShape.size());
    const auto numOutputShapeElements = outputShape.size();
    for (std::size_t i = 0; i < numOutputShapeElements; i++)
        result.push_back(ngraph::row_major_stride(outputShape, i));
    return result;
}

bool TransposeOp::isPermutationTensorSpecified(const ngraph::Node& node) {
    const auto numInputs = node.get_input_size();
    Expects(numInputs == 1 || numInputs == 2);
    return numInputs == 2;
}


std::optional<std::vector<int> > TransposeOp::tryToExtractPermutation(const ngraph::Node& node) {
    if (isPermutationTensorSpecified(node)) {
        auto nodeRawPtr = node.input(1).get_source_output().get_node();
        if (ngraph::is_type<const ngraph::op::Constant>(nodeRawPtr)) {
            // Typically permutation vector is small and comes from constant node.
            // That allows to optimize out copying it from device memory in most cases.
            auto constant = dynamic_cast<const ngraph::op::Constant*>(nodeRawPtr);
            return constant->cast_vector<int>();
        } else {
            return std::nullopt;
        }
    } else {
        auto result = extractInputMode(node.get_input_shape(0).size());
        std::reverse(result.begin(), result.end());
        return result;
    }
}


std::vector<int> TransposeOp::permutation(const InferenceRequestContext& context, Inputs inputTensors) const {
    if (outputMode_.has_value()) {
        return outputMode_.value();
    } else { // Copies permutation vector from device memory. cuTENSOR API requires it in host memory
        Expects(inputTensors.size() == 2);
        using ngraph::element::Type_t;
        switch (permutationElementsType_) {
        case Type_t::i8:  return downloadPermutationVector<std::int8_t>(context, inputTensors[1], dimsNumber_);
        case Type_t::i16: return downloadPermutationVector<std::int16_t>(context, inputTensors[1], dimsNumber_);
        case Type_t::i32: return downloadPermutationVector<std::int32_t>(context, inputTensors[1], dimsNumber_);
        case Type_t::i64: return downloadPermutationVector<std::int64_t>(context, inputTensors[1], dimsNumber_);
        case Type_t::u8:  return downloadPermutationVector<std::uint8_t>(context, inputTensors[1], dimsNumber_);
        case Type_t::u16: return downloadPermutationVector<std::uint16_t>(context, inputTensors[1], dimsNumber_);
        case Type_t::u32: return downloadPermutationVector<std::uint32_t>(context, inputTensors[1], dimsNumber_);
        case Type_t::u64: return downloadPermutationVector<std::uint64_t>(context, inputTensors[1], dimsNumber_);
        default:
            CUDA::throwIEException("Permutation vector is not of integer type.");
        }
    }
}


ngraph::element::Type_t TransposeOp::extractPermutationElementsType(const ngraph::Node& node) {
    Expects(node.get_input_size() > 0 && node.get_input_size() <= 2);
    if (node.get_input_size() == 1)
        return ngraph::element::Type_t::i32;
    else
        return node.get_input_element_type(1);
}


template<typename T>
inline std::vector<int> TransposeOp::downloadPermutationVector(
        const InferenceRequestContext& context,
        InferenceEngine::gpu::DevicePointer<const void*> devicePointer,
        unsigned numDims) {
    std::vector<int> result;
    result.reserve(numDims);
    std::vector<T> perm(numDims);
    context.getThreadContext().stream().download(perm.data(), devicePointer, numDims * sizeof(T));
    context.getThreadContext().stream().synchronize();
    std::copy(perm.begin(), perm.end(), std::back_inserter(result));
    return result;
}


TransposeOp::AlphaValue TransposeOp::makeAlpha(cudaDataType_t dt) {
    switch (dt) {
    case CUDA_R_16F:
        return { .fp16 = __float2half(1.f) };
    case CUDA_R_16BF:
        return TransposeOp::AlphaValue{ .bf16 = __float2bfloat16(1.f) };
    case CUDA_R_32F:
        return { .fp32 = 1.f };
    case CUDA_R_64F:
        return { .fp64 = 1. };
    case CUDA_R_8I:
        return { .i8 = 1 };
    case CUDA_R_8U:
        return { .u8 = 1 };
    case CUDA_R_16I:
        return { .i16 = 1 };
    case CUDA_R_16U:
        return { .u16 = 1 };
    case CUDA_R_32I:
        return { .i32 = 1 };
    case CUDA_R_32U:
        return { .u32 = 1 };
    case CUDA_R_64I:
        return { .i64 = 1 };
    case CUDA_R_64U:
        return { .u64 = 1 };
    }
    CUDA::throwIEException(fmt::format("Transpose operation initialization failed. Type {} is not supported.", dt));
}


OPERATION_REGISTER(TransposeOp, Transpose);

} // namespace CUDAPlugin
