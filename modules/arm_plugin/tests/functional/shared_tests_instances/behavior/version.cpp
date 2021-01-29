// Copyright (C) 2020-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "multi-device/multi_device_config.hpp"
#include "hetero/hetero_plugin_config.hpp"
#include "behavior/version.hpp"

using namespace BehaviorTestsDefinitions;
namespace {
    const std::vector<std::map<std::string, std::string>> configs = {
            {}
    };

    const std::vector<std::map<std::string, std::string>> Multiconfigs = {
            {{ MULTI_CONFIG_KEY(DEVICE_PRIORITIES) , "ARM"}}
    };

    const std::vector<std::map<std::string, std::string>> Heteroconfigs = {
            {{ HETERO_CONFIG_KEY(DUMP_GRAPH_DOT) , "ARM"}}
    };

    INSTANTIATE_TEST_CASE_P(smoke_BehaviorTests, VersionTest,
                            ::testing::Combine(
                                    ::testing::Values(InferenceEngine::Precision::FP32),
                                    ::testing::Values("ARM"),
                                    ::testing::ValuesIn(configs)),
                            VersionTest::getTestCaseName);

    INSTANTIATE_TEST_CASE_P(smoke_Multi_BehaviorTests, VersionTest,
                            ::testing::Combine(
                                    ::testing::Values(InferenceEngine::Precision::FP32),
                                    ::testing::Values(CommonTestUtils::DEVICE_MULTI),
                                    ::testing::ValuesIn(Multiconfigs)),
                            VersionTest::getTestCaseName);

    INSTANTIATE_TEST_CASE_P(smoke_Hetero_BehaviorTests, VersionTest,
                            ::testing::Combine(
                                    ::testing::Values(InferenceEngine::Precision::FP32),
                                    ::testing::Values(CommonTestUtils::DEVICE_HETERO),
                                    ::testing::ValuesIn(Heteroconfigs)),
                            VersionTest::getTestCaseName);


}  // namespace
