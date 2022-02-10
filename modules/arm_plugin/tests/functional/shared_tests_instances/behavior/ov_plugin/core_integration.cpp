// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "behavior/ov_plugin/core_integration.hpp"
#include "openvino/runtime/core.hpp"

using namespace ov::test::behavior;
using namespace InferenceEngine::PluginConfigParams;

namespace {
//
// IE Class Common tests with <pluginName, deviceName params>
//

INSTANTIATE_TEST_SUITE_P(
        smoke_OVClassCommon, OVClassBasicTestP,
        ::testing::Values(std::make_pair("openvino_arm_cpu_plugin", "CPU")));

INSTANTIATE_TEST_SUITE_P(
        smoke_OVClassNetworkTestP, CoreWithModelTest_Param,
        ::testing::Values("CPU"));

INSTANTIATE_TEST_SUITE_P(
        smoke_OVClassImportExportTestP, OVClassImportExportTestP,
        ::testing::Values("HETERO:CPU"));

//
// IE Class GetMetric
//

INSTANTIATE_TEST_SUITE_P(
        smoke_OVClassGetMetricTest, OVClassPropertyTest_SUPPORTED_PROPERTIES,
        ::testing::Values("CPU", "MULTI", "HETERO", "AUTO"));

INSTANTIATE_TEST_SUITE_P(
        smoke_OVClassGetMetricTest, OVClassPropertyTest_AVAILABLE_DEVICES,
        ::testing::Values("CPU"));

INSTANTIATE_TEST_SUITE_P(
        smoke_OVClassGetMetricTest, OVClassPropertyTest_FULL_DEVICE_NAME,
        ::testing::Values("CPU", "MULTI", "HETERO", "AUTO"));

INSTANTIATE_TEST_SUITE_P(
        smoke_OVClassGetMetricTest, OVClassPropertyTest_OPTIMIZATION_CAPABILITIES,
        ::testing::Values("CPU"));

INSTANTIATE_TEST_SUITE_P(
        smoke_OVClassGetMetricTest, OVClassPropertyTest_RANGE_FOR_ASYNC_INFER_REQUESTS,
        ::testing::Values("CPU"));

INSTANTIATE_TEST_SUITE_P(
        smoke_OVClassGetMetricTest, OVClassPropertyTest_RANGE_FOR_STREAMS,
        ::testing::Values("CPU"));

INSTANTIATE_TEST_SUITE_P(
        smoke_OVClassGetMetricTest, OVClassPropertyTest_ThrowUnsupported,
        ::testing::Values("CPU", "MULTI", "HETERO", "AUTO"));

INSTANTIATE_TEST_SUITE_P(
        smoke_OVClassGetConfigTest, OVClassGetPropertyTest_ThrowUnsupported ,
        ::testing::Values("CPU", "MULTI", "HETERO", "AUTO"));

INSTANTIATE_TEST_SUITE_P(
        smoke_OVClassGetAvailableDevices, OVClassGetAvailableDevices,
        ::testing::Values("CPU"));

//
// IE Class GetConfig
//

INSTANTIATE_TEST_SUITE_P(
        smoke_OVClassGetConfigTest, OVClassGetPropertyTest,
        ::testing::Values("CPU"));


// IE Class Query network

INSTANTIATE_TEST_SUITE_P(
        smoke_OVClassQueryNetworkTest, CoreQueryModelTest_Param,
        ::testing::Values("CPU"));

// IE Class Load network

INSTANTIATE_TEST_SUITE_P(
        smoke_OVClassLoadNetworkTest, OVClassLoadNetworkTest,
        ::testing::Values("CPU"));
} // namespace

