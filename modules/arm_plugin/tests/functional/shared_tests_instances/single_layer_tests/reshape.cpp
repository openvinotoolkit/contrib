// Copyright (C) 2020-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0


#include <vector>

#include "single_layer_tests/reshape.hpp"
#include "common_test_utils/test_constants.hpp"

using namespace LayerTestsDefinitions;

namespace {
const std::vector<InferenceEngine::Precision> netPrecisions = {
        InferenceEngine::Precision::FP32,
        InferenceEngine::Precision::FP16
};

INSTANTIATE_TEST_CASE_P(Reshape4D, ReshapeLayerTest,
        ::testing::Combine(
                ::testing::Values(true),
                ::testing::ValuesIn(netPrecisions),
                ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                ::testing::Values(InferenceEngine::Layout::ANY),
                ::testing::Values(InferenceEngine::Layout::ANY),
                ::testing::Values(std::vector<size_t>({30, 30, 30, 30})),
                ::testing::Values(std::vector<size_t>({0, 10, 90, 30})),
                ::testing::Values("ARM"),
                ::testing::Values(std::map<std::string, std::string>({}))),
                ReshapeLayerTest::getTestCaseName);

INSTANTIATE_TEST_CASE_P(Reshape3D, ReshapeLayerTest,
        ::testing::Combine(
                ::testing::Values(true),
                ::testing::ValuesIn(netPrecisions),
                ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                ::testing::Values(InferenceEngine::Layout::ANY),
                ::testing::Values(InferenceEngine::Layout::ANY),
                ::testing::Values(std::vector<size_t>({10, 10, 10, 10})),
                ::testing::Values(std::vector<size_t>({10, 0, 100})),
                ::testing::Values("ARM"),
                ::testing::Values(std::map<std::string, std::string>({}))),
                ReshapeLayerTest::getTestCaseName);

INSTANTIATE_TEST_CASE_P(Reshape2D, ReshapeLayerTest,
        ::testing::Combine(
                ::testing::Values(false),
                ::testing::ValuesIn(netPrecisions),
                ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                ::testing::Values(InferenceEngine::Layout::ANY),
                ::testing::Values(InferenceEngine::Layout::ANY),
                ::testing::Values(std::vector<size_t>({1, 100, 1, 1})),
                ::testing::Values(std::vector<size_t>({1, 100})),
                ::testing::Values("ARM"),
                ::testing::Values(std::map<std::string, std::string>({}))),
                ReshapeLayerTest::getTestCaseName);

INSTANTIATE_TEST_CASE_P(Reshape1D, ReshapeLayerTest,
        ::testing::Combine(
                ::testing::Values(true),
                ::testing::ValuesIn(netPrecisions),
                ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                ::testing::Values(InferenceEngine::Layout::ANY),
                ::testing::Values(InferenceEngine::Layout::ANY),
                ::testing::Values(std::vector<size_t>({1, 100, 1, 1})),
                ::testing::Values(std::vector<size_t>({100})),
                ::testing::Values("ARM"),
                ::testing::Values(std::map<std::string, std::string>({}))),
                ReshapeLayerTest::getTestCaseName);
}  // namespace
