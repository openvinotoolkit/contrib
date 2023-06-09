// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "cuda_infer_request.hpp"

#include <cuda_fp16.h>
#include <debug.h>
#include <fmt/format.h>

#include <algorithm>
#include <description_buffer.hpp>
#include <gsl/span_ext>
#include <map>
#include <memory>
#include <string>
#include <threading/ie_executor_manager.hpp>
#include <utility>

#include "cuda_compiled_model.hpp"
#include "cuda_itt.hpp"
#include "cuda_plugin.hpp"
#include "nvidia/properties.hpp"

namespace ov {
namespace nvidia_gpu {
using namespace utils;

using Time = std::chrono::steady_clock;

namespace {
void allocate_tensor_impl(ov::Tensor& tensor, const ov::element::Type& element_type, const ov::Shape& shape) {
    if (!tensor || tensor.get_element_type() != element_type) {
        tensor = ov::Tensor(element_type, shape);
    } else {
        tensor.set_shape(shape);
    }
}
}  // namespace

CudaInferRequest::CudaInferRequest(const std::shared_ptr<const CompiledModel>& compiled_model)
    : ov::ISyncInferRequest(compiled_model),
      cancellation_token_{[this] { memory_proxy_.reset(); }},
      profiler_{compiled_model->get_property(ov::enable_profiling.name()).as<bool>(), compiled_model->get_execution_graph()},
      is_benchmark_mode_{compiled_model->get_property(ov::nvidia_gpu::operation_benchmark.name()).as<bool>()},
      use_cuda_graph_{compiled_model->get_property(ov::nvidia_gpu::internal::use_cuda_graph.name()).as<bool>()} {
    create_infer_request();
}

void CudaInferRequest::create_infer_request() {
    auto compiled_model = get_nvidia_model();
    auto device_id = std::to_string(compiled_model->config_.get_device_id());
    auto request_id = std::to_string(compiled_model->request_id_.fetch_add(1));
    std::string name = "NVIDIA" + device_id + "_" + compiled_model->model_->get_friendly_name() + "_req" + request_id;

    _profilingTask = {
        openvino::itt::handle(name + "_Preprocess"),
        openvino::itt::handle(name + "_Postprocess"),
        openvino::itt::handle(name + "_StartPipline"),
        openvino::itt::handle(name + "_WaitPipline"),
    };

    // Allocate plugin backend specific memory handles
    input_tensors_.resize(get_inputs().size());
    output_tensors_.resize(get_outputs().size());

    // Allocate input/output tensors
    for (const auto& input : get_inputs()) {
        allocate_tensor(input, [input](ov::Tensor& tensor) {
            // Can add a check to avoid double work in case of shared tensors
            allocate_tensor_impl(tensor,
                                 input.get_element_type(),
                                 input.get_partial_shape().is_dynamic() ? ov::Shape{0} : input.get_shape());
        });
    }
    for (const auto& output : get_outputs()) {
        allocate_tensor(output, [output](ov::Tensor& tensor) {
            // Can add a check to avoid double work in case of shared tensors
            allocate_tensor_impl(tensor,
                                 output.get_element_type(),
                                 output.get_partial_shape().is_dynamic() ? ov::Shape{0} : output.get_shape());
        });
    }
}

void CudaInferRequest::infer_preprocess() {
    OV_ITT_SCOPED_TASK(itt::domains::nvidia_gpu, _profilingTask[Profiler::Preprocess]);
    profiler_.start_stage();

    convert_batched_tensors();
    check_tensors();

    // Allocate host input tensors
    OPENVINO_ASSERT(get_inputs().size() == input_tensors_.size());
    for (size_t i = 0; i < get_inputs().size(); i++) {
        auto tensor = get_tensor(get_inputs()[i]);
        ov::element::Type element_type = tensor.get_element_type();
        ov::Shape shape = tensor.get_shape();
        if (tensor.is<ov::RemoteTensor>()) {
            OPENVINO_ASSERT(true, "NVIDIA plugin doesn't support remote tensor.");
        } else if (tensor.is_continuous()) {
            // No ROI extraction is needed
            input_tensors_.at(i) =
                std::make_shared<ov::Tensor>(element_type, shape, tensor.data());
        } else {
            OPENVINO_ASSERT(element_type.bitwidth() % 8 == 0,
                            "Template plugin: Unsupported ROI tensor with element type having ",
                            std::to_string(element_type.bitwidth()),
                            " bits size");
            // Perform manual extraction of ROI tensor
            // Basic implementation doesn't take axis order into account `desc.getBlockingDesc().getOrder()`
            // Performance of manual extraction is not optimal, but it is ok for template implementation
            input_tensors_.at(i) = std::make_shared<ov::Tensor>(element_type, shape);
            tensor.copy_to(*input_tensors_[i].get());
        }
    }
    // Allocate host output tensors
    OPENVINO_ASSERT(get_outputs().size() == output_tensors_.size());
    OPENVINO_ASSERT(get_outputs().size() == get_nvidia_model()->model_->get_results().size());
    for (size_t i = 0; i < get_outputs().size(); i++) {
        const auto& result = get_nvidia_model()->model_->get_results()[i];
        if (result->get_output_partial_shape(0).is_dynamic()) {
            output_tensors_.at(i) = std::make_shared<ov::Tensor>();
            continue;
        }
        auto tensor = get_tensor(get_outputs()[i]);
        ov::element::Type element_type = tensor.get_element_type();
        ov::Shape shape = tensor.get_shape();
        if (tensor.is_continuous() && !tensor.is<ov::RemoteTensor>())
            output_tensors_.at(i) = std::make_shared<ov::Tensor>(element_type, shape, tensor.data());
        else
            output_tensors_.at(i) = std::make_shared<ov::Tensor>(element_type, shape);
    }
    profiler_.stop_stage(Profiler::Preprocess);
}

void CudaInferRequest::start_pipeline(const ThreadContext& threadContext) {
    try {
        OV_ITT_SCOPED_TASK(itt::domains::nvidia_gpu, _profilingTask[Profiler::StartPipeline])
        profiler_.start_stage();
        auto compiled_model = get_nvidia_model();
        memory_proxy_ = compiled_model->memory_pool_->WaitAndGet(cancellation_token_);
        auto& memory = memory_proxy_->Get();
        auto& graph = compiled_model->get_execution_graph();
        InferenceRequestContext inferRequestContext{input_tensors_,
                                                    compiled_model->input_index_,
                                                    output_tensors_,
                                                    compiled_model->output_index_,
                                                    threadContext,
                                                    cancellation_token_,
                                                    profiler_,
                                                    is_benchmark_mode_,
                                                    use_cuda_graph_};
        graph.Run(inferRequestContext, memory);
        profiler_.stop_stage(Profiler::StartPipeline);
    } catch (...) {
        // TODO:
        // Log error once logger is available
        memory_proxy_.reset();
        throw;
    }
}

void CudaInferRequest::wait_pipeline(const ThreadContext& threadContext) {
    OV_ITT_SCOPED_TASK(itt::domains::nvidia_gpu, _profilingTask[Profiler::WaitPipeline])
    profiler_.start_stage();
    // TODO: probably all time will be spent in synchonize, out of reach of ThrowIfCanceled
    threadContext.stream().synchronize();
    memory_proxy_.reset();
    profiler_.stop_stage(Profiler::WaitPipeline);
}

void CudaInferRequest::infer_postprocess() {
    OV_ITT_SCOPED_TASK(itt::domains::nvidia_gpu, _profilingTask[Profiler::Postprocess]);
    profiler_.start_stage();

    OPENVINO_ASSERT(get_outputs().size() == output_tensors_.size());
    OPENVINO_ASSERT(get_outputs().size() == get_nvidia_model()->model_->get_results().size());
    for (size_t i = 0; i < get_outputs().size(); i++) {
        const auto& result = get_nvidia_model()->model_->get_results()[i];
        auto host_tensor = *output_tensors_[i].get();
        auto tensor = get_tensor(get_outputs()[i]);
        if (result->get_output_partial_shape(0).is_dynamic()) {
            ov::Output<const ov::Node> output{result->output(0).get_node(), result->output(0).get_index()};
            allocate_tensor(output, [host_tensor](ov::Tensor& tensor) {
                allocate_tensor_impl(tensor, host_tensor.get_element_type(), host_tensor.get_shape());
                host_tensor.copy_to(tensor);
            });
        } else if (!tensor.is_continuous()) {
            host_tensor.copy_to(tensor);
        } else if (tensor.is<ov::RemoteTensor>()) {
            OPENVINO_ASSERT(true, "NVIDIA plugin doesn't support RemoteTensor.");
        }
    }
    profiler_.stop_stage(Profiler::Postprocess);
    profiler_.process_events();
}

void CudaInferRequest::cancel() {
    cancellation_token_.cancel();
    get_nvidia_model()->get_memory_pool()->Interrupt();
}

void CudaInferRequest::infer() {
    OPENVINO_NOT_IMPLEMENTED;
}

std::shared_ptr<const CompiledModel> CudaInferRequest::get_nvidia_model() {
    auto& compiled_model = get_compiled_model();
    auto nvidia_model = std::dynamic_pointer_cast<const CompiledModel>(compiled_model);
    OPENVINO_ASSERT(nvidia_model);
    return nvidia_model;
}

void CudaInferRequest::set_tensors_impl(const ov::Output<const ov::Node> port,
                                        const std::vector<ov::Tensor>& tensors) {
    for (const auto& input : get_inputs()) {
        if (input == port) {
            m_batched_tensors[input.get_tensor_ptr()] = tensors;
            return;
        }
    }
    OPENVINO_THROW("Cannot find input tensors for port ", port);
}

std::vector<std::shared_ptr<ov::IVariableState>> CudaInferRequest::query_state() const {
    OPENVINO_NOT_IMPLEMENTED;
}

std::vector<ov::ProfilingInfo> CudaInferRequest::get_profiling_info() const {
    return profiler_.get_performance_counts();
}
}  // namespace nvidia_gpu
}  // namespace ov
