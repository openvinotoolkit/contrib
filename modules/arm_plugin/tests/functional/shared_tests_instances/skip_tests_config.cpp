// Copyright (C) 2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <vector>
#include <string>

#include "functional_test_utils/skip_tests_config.hpp"

std::vector<std::string> disabledTestPatterns() {
    return {
        ".*reusableCPUStreamsExecutor.*",  //  TEST DO not support hetero case when all plugins use executers cache
        ".*Multi.*canSetExclusiveAsyncRequests.*",  // Unsupported topology
        ".*Multi.*withoutExclusiveAsyncRequests.*",  // Unsupported topology
        "ARM/CoreThreadingTests.smoke_QueryNetwork/0", // Used legacy network representation
        R"(.*GroupConvolutionLayerTest.*PB\(0\.1\)_PE\(0\.0\).*)", // Sporadic test fails
        ".*ExecGraphTests.*", // Not implemented
        R"(.*Interpolate_NearestFloorAsym.*TS=\(1\.1\.2\.2\).*)", // Not supported
        R"(Interpolate.*InterpolateMode=linear_onnx.*)", // Not supported
        R"(.*Eltwise.*eltwiseOpType=Mod.*netPRC=FP16.*)", // Failed
    };
}