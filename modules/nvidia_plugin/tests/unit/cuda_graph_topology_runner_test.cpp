// Copyright (C) 2020-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cuda_graph_topology_runner.hpp>
#include <ops/parameter.hpp>
#include <ops/result.hpp>
#include <cuda_profiler.hpp>
#include "test_networks.hpp"

using namespace ov::nvidia_gpu;
using namespace testing;

class CudaGraphTopologyRunnerTest : public Test {
protected:
    static std::map<std::string, std::size_t> PopulateInputIndices(std::shared_ptr<ov::Model> model) {
        std::map<std::string, std::size_t> inputIndices;
        for (const auto& parameter : model->get_parameters()) {
            const auto& parameter_index = model->get_parameter_index(parameter);
            inputIndices.emplace(ParameterOp::GetInputTensorName(*parameter), parameter_index);
        }
        return inputIndices;
    }

    static std::map<std::string, std::size_t> PopulateOutputIndices(std::shared_ptr<ov::Model> model) {
        std::map<std::string, std::size_t> outputIndices;
        for (auto& result : model->get_results()) {
            const auto& result_index = model->get_result_index(result->input_value(0));
            for (const auto& outputName : ResultOp::GetOutputTensorName(*result)) {
                outputIndices.emplace(outputName, result_index);
            }
        }
        return outputIndices;
    }

    static std::vector<std::shared_ptr<ov::Tensor>> PopulateTensors(const std::vector<ov::Output<ov::Node>>& nodes) {
        std::vector<std::shared_ptr<ov::Tensor>> ret;
        for (const auto& node : nodes)
            ret.push_back(std::make_shared<ov::Tensor>(node.get_element_type(), node.get_shape()));
        return ret;
    }

    std::shared_ptr<ov::Model> model_ {CreateMatMulTestNetwork()};
    CreationContext creationContext_ {{}, false};
    ThreadContext threadContext_ {{}};
    CancellationToken cancellationToken_ {};
    CudaGraphContext cudaGraphContext_ {};
    CudaGraphTopologyRunner runner_ {creationContext_, model_};
    Profiler profiler_ {false, runner_.GetSubGraph()};
    std::vector<std::shared_ptr<ov::Tensor>> inputTensors_ {PopulateTensors(model_->inputs())};
    std::vector<std::shared_ptr<ov::Tensor>> outputTensors_ {PopulateTensors(model_->outputs())};
    std::map<std::string, std::size_t> inputIndeces_ {PopulateInputIndices(model_)};
    std::map<std::string, std::size_t> outputIndeces_ {PopulateOutputIndices(model_)};
    InferenceRequestContext inferRequestContext_{inputTensors_,
                                                inputIndeces_,
                                                outputTensors_,
                                                outputIndeces_,
                                                threadContext_,
                                                cancellationToken_,
                                                profiler_,
                                                cudaGraphContext_,
                                                false};
    DeviceMemBlock deviceMemBlock_ {runner_.GetSubGraph().memoryManager().mutableTensorsMemoryModel()};
};

TEST_F(CudaGraphTopologyRunnerTest, InstantiateGraphExec) {
    runner_.UpdateContext(inferRequestContext_, deviceMemBlock_);
    EXPECT_TRUE(inferRequestContext_.getCudaGraphContext().graphExec.has_value());
}

TEST_F(CudaGraphTopologyRunnerTest, BasicRun) {
    EXPECT_NO_THROW(runner_.UpdateContext(inferRequestContext_, deviceMemBlock_));
    EXPECT_NO_THROW(runner_.Run(inferRequestContext_, deviceMemBlock_));
}

TEST_F(CudaGraphTopologyRunnerTest, CheckGraphExecIsInstantiatedOnce) {
    runner_.UpdateContext(inferRequestContext_, deviceMemBlock_);
    CUDA::GraphExec* exec = &inferRequestContext_.getCudaGraphContext().graphExec.value();
    runner_.UpdateContext(inferRequestContext_, deviceMemBlock_);
    EXPECT_EQ(&inferRequestContext_.getCudaGraphContext().graphExec.value(), exec);
}

TEST_F(CudaGraphTopologyRunnerTest, CheckMemcpyNodesArePopulated) {
    runner_.UpdateContext(inferRequestContext_, deviceMemBlock_);
    EXPECT_GT(inferRequestContext_.getCudaGraphContext().parameterNodes.size(), 0);
    EXPECT_GT(inferRequestContext_.getCudaGraphContext().resultNodes.size(), 0);
}

TEST_F(CudaGraphTopologyRunnerTest, CheckMemcpyNodesAreUpdated) {
    runner_.UpdateContext(inferRequestContext_, deviceMemBlock_);
    auto paramNodes = cudaGraphContext_.parameterNodes;
    auto resultNodes = cudaGraphContext_.resultNodes;
    std::vector<std::shared_ptr<ov::Tensor>> inputTensors {PopulateTensors(model_->inputs())};
    std::vector<std::shared_ptr<ov::Tensor>> outputTensors {PopulateTensors(model_->outputs())};
    InferenceRequestContext inferRequestContext{inputTensors,
                                                    inputIndeces_,
                                                    outputTensors,
                                                    outputIndeces_,
                                                    threadContext_,
                                                    cancellationToken_,
                                                    profiler_,
                                                    cudaGraphContext_,
                                                    false};
    runner_.UpdateContext(inferRequestContext, deviceMemBlock_);
    EXPECT_NE(cudaGraphContext_.parameterNodes, paramNodes);
    EXPECT_NE(cudaGraphContext_.resultNodes, resultNodes);
}

TEST_F(CudaGraphTopologyRunnerTest, CheckMemcpyNodesAreNotUpdatedIfPointersUnchanged) {
    runner_.UpdateContext(inferRequestContext_, deviceMemBlock_);
    auto paramNodes = cudaGraphContext_.parameterNodes;
    auto resultNodes = cudaGraphContext_.resultNodes;
    InferenceRequestContext inferRequestContext{inputTensors_,
                                                    inputIndeces_,
                                                    outputTensors_,
                                                    outputIndeces_,
                                                    threadContext_,
                                                    cancellationToken_,
                                                    profiler_,
                                                    cudaGraphContext_,
                                                    false};
    runner_.UpdateContext(inferRequestContext, deviceMemBlock_);
    EXPECT_EQ(cudaGraphContext_.parameterNodes, paramNodes);
    EXPECT_EQ(cudaGraphContext_.resultNodes, resultNodes);
}
