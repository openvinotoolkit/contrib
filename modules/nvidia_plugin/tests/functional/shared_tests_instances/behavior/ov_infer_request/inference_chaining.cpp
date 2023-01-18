// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "behavior/ov_infer_request/inference_chaining.hpp"

#include <cuda_test_constants.hpp>

#include "common_test_utils/test_constants.hpp"

using namespace ov::test::behavior;

namespace {

const std::vector<ov::AnyMap> configs = {{}};

const std::vector<ov::AnyMap> HeteroConfigs = {{ov::device::priorities(CommonTestUtils::DEVICE_NVIDIA)}};

INSTANTIATE_TEST_SUITE_P(smoke_BehaviorTests,
                         OVInferenceChaining,
                         ::testing::Combine(::testing::Values(CommonTestUtils::DEVICE_NVIDIA),
                                            ::testing::ValuesIn(configs)),
                         OVInferenceChaining::getTestCaseName);

INSTANTIATE_TEST_SUITE_P(smoke_Hetero_BehaviorTests,
                         OVInferenceChaining,
                         ::testing::Combine(::testing::Values(CommonTestUtils::DEVICE_HETERO),
                                            ::testing::ValuesIn(HeteroConfigs)),
                         OVInferenceChaining::getTestCaseName);

}  // namespace
