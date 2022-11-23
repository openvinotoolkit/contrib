// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "single_layer_tests/variadic_split.hpp"

#include <vector>

#include "common_test_utils/test_constants.hpp"
#include "cuda_test_constants.hpp"

using namespace LayerTestsDefinitions;

namespace {

const std::vector<InferenceEngine::Precision> netPrecisions = {
    InferenceEngine::Precision::FP32,
    InferenceEngine::Precision::FP16,
};

// Sum of elements numSplits = inputShapes[Axis]
const std::vector<std::vector<size_t>> smoke_numSplits = {
    {1, 16, 5, 8},
    {2, 19, 5, 4},
    {7, 13, 2, 8},
    {5, 8, 12, 5},
    {4, 11, 6, 9},
};

INSTANTIATE_TEST_CASE_P(smoke_NumSplitsCheck,
                        VariadicSplitLayerTest,
                        ::testing::Combine(::testing::ValuesIn(smoke_numSplits),
                                           ::testing::Values(-3, -2, -1, 0, 1, 2, 3),
                                           ::testing::ValuesIn(netPrecisions),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(std::vector<size_t>({30, 30, 30, 30})),
                                           ::testing::Values(CommonTestUtils::DEVICE_NVIDIA)),
                        VariadicSplitLayerTest::getTestCaseName);

INSTANTIATE_TEST_CASE_P(yolov5_NumSplitsCheck_shape0_axis1,
                        VariadicSplitLayerTest,
                        ::testing::Combine(::testing::Values(std::vector<size_t>{1, 1, 1}),
                                           ::testing::Values(1),
                                           ::testing::ValuesIn(netPrecisions),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(std::vector<size_t>{1, 3, 40, 40, 85}),
                                           ::testing::Values(CommonTestUtils::DEVICE_NVIDIA)),
                        VariadicSplitLayerTest::getTestCaseName);

INSTANTIATE_TEST_CASE_P(yolov5_NumSplitsCheck_shape0_axis2,
                        VariadicSplitLayerTest,
                        ::testing::Combine(::testing::Values(std::vector<size_t>{10, 20, 10}),
                                           ::testing::Values(2),
                                           ::testing::ValuesIn(netPrecisions),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(std::vector<size_t>{1, 3, 40, 40, 85}),
                                           ::testing::Values(CommonTestUtils::DEVICE_NVIDIA)),
                        VariadicSplitLayerTest::getTestCaseName);

INSTANTIATE_TEST_CASE_P(yolov5_NumSplitsCheck_shape0_axis3,
                        VariadicSplitLayerTest,
                        ::testing::Combine(::testing::Values(std::vector<size_t>{5, 5, 30}),
                                           ::testing::Values(3),
                                           ::testing::ValuesIn(netPrecisions),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(std::vector<size_t>{1, 3, 40, 40, 85}),
                                           ::testing::Values(CommonTestUtils::DEVICE_NVIDIA)),
                        VariadicSplitLayerTest::getTestCaseName);

INSTANTIATE_TEST_CASE_P(yolov5_NumSplitsCheck_shape0_axis4,
                        VariadicSplitLayerTest,
                        ::testing::Combine(::testing::Values(std::vector<size_t>{20, 60, 5}),
                                           ::testing::Values(4),
                                           ::testing::ValuesIn(netPrecisions),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(std::vector<size_t>{1, 3, 40, 40, 85}),
                                           ::testing::Values(CommonTestUtils::DEVICE_NVIDIA)),
                        VariadicSplitLayerTest::getTestCaseName);

INSTANTIATE_TEST_CASE_P(yolov5_NumSplitsCheck_shape1_axis1,
                        VariadicSplitLayerTest,
                        ::testing::Combine(::testing::Values(std::vector<size_t>{1, 1, 1}),
                                           ::testing::Values(1),
                                           ::testing::ValuesIn(netPrecisions),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(std::vector<size_t>{1, 3, 20, 20, 85}),
                                           ::testing::Values(CommonTestUtils::DEVICE_NVIDIA)),
                        VariadicSplitLayerTest::getTestCaseName);

INSTANTIATE_TEST_CASE_P(yolov5_NumSplitsCheck_shape1_axis2,
                        VariadicSplitLayerTest,
                        ::testing::Combine(::testing::Values(std::vector<size_t>{5, 12, 3}),
                                           ::testing::Values(2),
                                           ::testing::ValuesIn(netPrecisions),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(std::vector<size_t>{1, 3, 20, 20, 85}),
                                           ::testing::Values(CommonTestUtils::DEVICE_NVIDIA)),
                        VariadicSplitLayerTest::getTestCaseName);

INSTANTIATE_TEST_CASE_P(yolov5_NumSplitsCheck_shape1_axis3,
                        VariadicSplitLayerTest,
                        ::testing::Combine(::testing::Values(std::vector<size_t>{2, 8, 10}),
                                           ::testing::Values(3),
                                           ::testing::ValuesIn(netPrecisions),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(std::vector<size_t>{1, 3, 20, 20, 85}),
                                           ::testing::Values(CommonTestUtils::DEVICE_NVIDIA)),
                        VariadicSplitLayerTest::getTestCaseName);

INSTANTIATE_TEST_CASE_P(yolov5_NumSplitsCheck_shape1_axis4,
                        VariadicSplitLayerTest,
                        ::testing::Combine(::testing::Values(std::vector<size_t>{65, 3, 17}),
                                           ::testing::Values(4),
                                           ::testing::ValuesIn(netPrecisions),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(std::vector<size_t>{1, 3, 20, 20, 85}),
                                           ::testing::Values(CommonTestUtils::DEVICE_NVIDIA)),
                        VariadicSplitLayerTest::getTestCaseName);

INSTANTIATE_TEST_CASE_P(yolov5_NumSplitsCheck_shape2_axis1,
                        VariadicSplitLayerTest,
                        ::testing::Combine(::testing::Values(std::vector<size_t>{1, 1, 1}),
                                           ::testing::Values(1),
                                           ::testing::ValuesIn(netPrecisions),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(std::vector<size_t>{1, 3, 80, 80, 85}),
                                           ::testing::Values(CommonTestUtils::DEVICE_NVIDIA)),
                        VariadicSplitLayerTest::getTestCaseName);

INSTANTIATE_TEST_CASE_P(yolov5_NumSplitsCheck_shape2_axis2,
                        VariadicSplitLayerTest,
                        ::testing::Combine(::testing::Values(std::vector<size_t>{13, 13, 54}),
                                           ::testing::Values(2),
                                           ::testing::ValuesIn(netPrecisions),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(std::vector<size_t>{1, 3, 80, 80, 85}),
                                           ::testing::Values(CommonTestUtils::DEVICE_NVIDIA)),
                        VariadicSplitLayerTest::getTestCaseName);

INSTANTIATE_TEST_CASE_P(yolov5_NumSplitsCheck_shape2_axis3,
                        VariadicSplitLayerTest,
                        ::testing::Combine(::testing::Values(std::vector<size_t>{7, 3, 70}),
                                           ::testing::Values(3),
                                           ::testing::ValuesIn(netPrecisions),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(std::vector<size_t>{1, 3, 80, 80, 85}),
                                           ::testing::Values(CommonTestUtils::DEVICE_NVIDIA)),
                        VariadicSplitLayerTest::getTestCaseName);

INSTANTIATE_TEST_CASE_P(yolov5_NumSplitsCheck_shape2_axis4,
                        VariadicSplitLayerTest,
                        ::testing::Combine(::testing::Values(std::vector<size_t>{1, 1, 83}),
                                           ::testing::Values(4),
                                           ::testing::ValuesIn(netPrecisions),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(std::vector<size_t>{1, 3, 80, 80, 85}),
                                           ::testing::Values(CommonTestUtils::DEVICE_NVIDIA)),
                        VariadicSplitLayerTest::getTestCaseName);

}  // namespace
