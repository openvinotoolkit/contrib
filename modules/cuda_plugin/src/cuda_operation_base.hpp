// Copyright (C) 2018-2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <string>
#include <vector>
#include <memory>

#include <gpu/gpu_context_api_cuda.hpp>
#include <gpu/device_pointers.hpp>
#include <ie_layouts.h>

namespace ngraph {

class Node;

}

namespace CUDAPlugin {

using InferenceEngine::gpu::InferenceRequestContext;

class IOperationExec {
 public:
  using Inputs = gsl::span<InferenceEngine::gpu::DevicePointer<const void*>>;
  using Outputs = gsl::span<InferenceEngine::gpu::DevicePointer<void*>>;

  virtual ~IOperationExec() = default;
  virtual void Execute(const InferenceRequestContext& context,
                       Inputs inputTensors,
                       Outputs outputTensors) = 0;
};

class IOperationMeta {
 public:
  virtual ~IOperationMeta() = default;
  virtual const std::string& GetName() const = 0;
  virtual gsl::span<const unsigned> GetInputIds() const = 0;
  virtual gsl::span<const unsigned> GetOutputIds() const = 0;
};

class OperationBase
    : public IOperationExec
    , public IOperationMeta
    , public std::enable_shared_from_this<OperationBase> {
 public:
  using Ptr = std::shared_ptr<OperationBase>;
  using WeakPtr = std::weak_ptr<OperationBase>;
  using IndexCollection = std::vector<unsigned>;
  OperationBase(const std::shared_ptr<ngraph::Node>& node,
                IndexCollection&& inputIds,
                IndexCollection&& outputIds);

  const std::string& GetName() const override {
    return node_name_;
  }
  gsl::span<const unsigned> GetInputIds() const override {
    return input_ids_;
  }
  gsl::span<const unsigned> GetOutputIds() const override {
    return output_ids_;
  }

 protected:
  std::string node_name_;
  const std::vector<unsigned> input_ids_;
  const std::vector<unsigned> output_ids_;
};

/**
 * @brief Downcasts a shared node pointer to a ConcreteOperator reference
 */
template<class ConcreteOperator>
ConcreteOperator& downcast(const std::shared_ptr<ngraph::Node>& node) {
  return dynamic_cast<ConcreteOperator&>(*node.get());
}
} // namespace CUDAPlugin
