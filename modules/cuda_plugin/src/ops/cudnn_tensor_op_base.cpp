// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//
#include "cudnn_tensor_op_base.hpp"

#include <fmt/ostream.h>

#include <cuda_operation_registry.hpp>
#include <ngraph/op/util/attr_types.hpp>

#include "constant_factory.hpp"
#include "converters.hpp"

namespace CUDAPlugin {
namespace {

template <typename T, std::size_t N>
std::array<T, N> toArray(const ngraph::Shape& shape) {
    std::array<T, N> a;
    a.fill(static_cast<T>(1));
    if (shape.empty()) return a;
    std::copy(shape.rbegin(), shape.rend(), a.rbegin());
    return a;
}

using ShapeArray = std::array<int, CuDnnTensorOpBase::max_supported_shape_size>;

CUDA::DnnTensorDescriptor desc(const cudnnDataType_t type, ShapeArray& dims) {
    ShapeArray strides;
    strides.back() = 1;
    for (int i = dims.size() - 1; i > 0; i--) strides[i - 1] = strides[i] * dims[i];
    return {type, static_cast<int>(dims.size()), dims.data(), strides.data()};
}
}  // namespace

CuDnnTensorOpBase::CuDnnTensorOpBase(const CreationContext& context,
                                     const std::shared_ptr<ngraph::Node>& node,
                                     IndexCollection&& inputIds,
                                     IndexCollection&& outputIds,
                                     const cudnnOpTensorOp_t& opType,
                                     const cudnnNanPropagation_t& nanPropogationType)
    : OperationCuDnn{context, node, move(inputIds), move(outputIds)},
      in0(*node, IoParams::Type::INPUT, 0),
      in1(*node, IoParams::Type::INPUT, 1),
      out(*node, IoParams::Type::OUTPUT, 0),
      // According to https://docs.nvidia.com/deeplearning/cudnn/api/index.html#cudnnOpTensor
      // opTensorCompType for cudnnOpTensor should always be FLOAT unless inputs and outputs are
      // double which is not supported by CudaPlugin
      op_desc_(opType, cudnnDataType_t::CUDNN_DATA_FLOAT, nanPropogationType),
      op_type_(opType) {
    Expects(node->get_input_size() == 2);
    Expects(node->get_output_size() == 1);
    const auto& in_partial_shape0 = node->get_input_partial_shape(0);
    const auto& in_partial_shape1 = node->get_input_partial_shape(1);
    const auto& out_partial_shape = node->get_output_partial_shape(0);
    if (in0.shape_.size() > max_supported_shape_size || in1.shape_.size() > max_supported_shape_size) {
        throwIEException(
            fmt::format("Currently max supported shape size for CuDnnTensorOpBase operation "
                        "is: {} {} {}",
                        max_supported_shape_size,
                        in_partial_shape0,
                        in_partial_shape1));
    }
    if (out.shape_ != in0.shape_ && out.shape_ != in1.shape_) {
        throwIEException(
            fmt::format("Currently at least one of the input shapes: {}, {} of "
                        "CuDnnTensorOpBase operation should be"
                        "equal to the output shape: {}",
                        in_partial_shape0,
                        in_partial_shape1,
                        out_partial_shape));
    }
    const auto size = in0.array_.size();
    Expects(in1.array_.size() == size);
    Expects(out.array_.size() == size);
    bool has_0_broadcasts = false;
    bool has_1_broadcasts = false;
    for (int i = 0; i < size; ++i) {
        if (in0.array_[i] != in1.array_[i]) {
            if (in0.array_[i] == 1) {
                has_0_broadcasts = true;
            } else if (in1.array_[i] == 1) {
                has_1_broadcasts = true;
            } else {
                throwIEException(fmt::format(
                    "Unsupported shapes for CuDnnTensorOpBase operation: {} {}", in_partial_shape0, in_partial_shape1));
            }
        }
    }
    bias_index_ = 0;
    dest_index_ = 1;
    if (has_0_broadcasts || has_1_broadcasts) {
        auto broadcast_spec = node->get_autob();
        if (!(broadcast_spec == ngraph::op::AutoBroadcastSpec::NUMPY)) {
            throwIEException(
                fmt::format("Unsupported broadcast type for CuDnnTensorOpBase operation: {}", broadcast_spec.m_type));
        }
        if (has_0_broadcasts && has_1_broadcasts) {
            throwIEException(
                fmt::format("Currently CuDnnTensorOpBase operation supports "
                            "broadcasting only in one "
                            "of two input shapes: {} {}",
                            in_partial_shape0,
                            in_partial_shape1));
        }
        bias_index_ = has_0_broadcasts ? 0 : 1;
        dest_index_ = has_0_broadcasts ? 1 : 0;
    }
}

void CuDnnTensorOpBase::Execute(const InferenceRequestContext& context,
                                Inputs inputTensors,
                                Outputs outputTensors,
                                const Workbuffers&) const {
    Expects(inputTensors.size() == 2);
    Expects(outputTensors.size() == 1);
    const auto& bias_input = bias_index_ == 0 ? in0 : in1;
    const auto& dest_input = bias_index_ == 0 ? in1 : in0;

    const void* alpha1 = &NumericConst<constants::one>(out.type_);
    const void* alpha2 = &NumericConst<constants::one>(out.type_);
    const void* beta = &NumericConst<constants::zero>(out.type_);

    if (op_type_ == CUDNN_OP_TENSOR_MAX) {
        alpha1 = &NumericStrictConst<constants::one>(out.type_);
        alpha2 = &NumericStrictConst<constants::one>(out.type_);
        beta = &NumericStrictConst<constants::zero>(out.type_);
    }

    context.getThreadContext().dnnHandle().opTensor(op_desc_,
                                                    alpha1,
                                                    dest_input.desc_,
                                                    inputTensors[dest_index_].get(),
                                                    alpha2,
                                                    bias_input.desc_,
                                                    inputTensors[bias_index_].get(),
                                                    beta,
                                                    out.desc_,
                                                    outputTensors[0].get());
}

CuDnnTensorOpBase::IoParams::IoParams(const ngraph::Node& node, const Type& io_type, int index)
    : type_(convertDataType<cudnnDataType_t>(io_type == Type::INPUT ? node.get_input_element_type(index)
                                                                    : node.get_output_element_type(index))),
      shape_(io_type == Type::INPUT ? node.get_input_shape(index) : node.get_output_shape(index)),
      array_(toArray<int, max_supported_shape_size>(shape_)),
      desc_(desc(type_, array_)) {}

}  // namespace CUDAPlugin
