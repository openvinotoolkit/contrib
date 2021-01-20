// Copyright (C) 2018-2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <behavior/core_threading_tests.hpp>

namespace {

const Params params[] = {
    std::tuple<Device, Config> { "ARM", { { CONFIG_KEY(PERF_COUNT), CONFIG_VALUE(YES) } } },
//   std::tuple<Device, Config> { CommonTestUtils::DEVICE_HETERO, { { "TARGET_FALLBACK", "ARM" } } },
//   std::tuple<Device, Config> { CommonTestUtils::DEVICE_MULTI, { { MULTI_CONFIG_KEY(DEVICE_PRIORITIES) , "ARM" } } }
};

const Params paramsStreams[] = {
    std::tuple<Device, Config> { "ARM", { { CONFIG_KEY(CPU_THROUGHPUT_STREAMS), CONFIG_VALUE(CPU_THROUGHPUT_AUTO) } } }
};


}  // namespace

INSTANTIATE_TEST_CASE_P(DISABLED_ARM, CoreThreadingTests, testing::ValuesIn(params));

// TODO: uncomment after merging https://github.com/openvinotoolkit/openvino/pull/3297
// INSTANTIATE_TEST_CASE_P(DISABLED_ARM, CoreThreadingTestsWithIterations,
//     testing::Combine(testing::ValuesIn(params),
//                      testing::Values(4),
//                      testing::Values(50),
//                      testing::Values(ModelClass::Default)));
//
// INSTANTIATE_TEST_CASE_P(DISABLED_ARM_Streams, CoreThreadingTestsWithIterations,
//     testing::Combine(testing::ValuesIn(paramsStreams),
//                      testing::Values(4),
//                      testing::Values(10),
//                      testing::Values(ModelClass::Default)));
