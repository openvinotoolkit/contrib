// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <string>
#include <map>

#include <ie_parameter.hpp>

#include <threading/ie_istreams_executor.hpp>

namespace CUDAPlugin {

// ! [configuration:header]
using ConfigMap = std::map<std::string, std::string>;

struct Configuration {
    using Ptr = std::shared_ptr<Configuration>;

    Configuration();
    Configuration(const Configuration&)             = default;
    Configuration(Configuration&&)                  = default;
    Configuration& operator=(const Configuration&)  = default;
    Configuration& operator=(Configuration&&)       = default;

    explicit Configuration(const ConfigMap& config, const Configuration & defaultCfg = {}, const bool throwOnUnsupported = true);

    InferenceEngine::Parameter Get(const std::string& name) const;

    // Plugin configuration parameters

    int deviceId                = 0;
    bool perfCount              = true;
    std::string cuda_throughput_streams_{std::to_string(1)};
    InferenceEngine::IStreamsExecutor::Config streams_executor_config_;
};
// ! [configuration:header]

}  //  namespace CUDAPlugin
