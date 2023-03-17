// Copyright (C) 2020-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <ngraph/pass/graph_rewrite.hpp>

namespace ArmPlugin {
namespace pass {

class ConvertGroupConvolution: public ngraph::pass::MatcherPass {
public:
    OPENVINO_RTTI("ConvertGroupConvolution");
    ConvertGroupConvolution();
};
}  // namespace pass
}  // namespace ArmPlugin
