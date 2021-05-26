// Copyright (C) 2020-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <vector>

#include "hetero/query_network.hpp"
#include "ngraph_functions/builders.hpp"
#include "ngraph_functions/subgraph_builders.hpp"

namespace {
using namespace HeteroTests;

auto ConvBias = ngraph::builder::subgraph::makeConvBias();

INSTANTIATE_TEST_CASE_P(smoke_FullySupportedTopologies, QueryNetworkTest,
                        ::testing::Combine(
                                ::testing::Values("CPU", "HETERO:CPU", "MULTI:CPU"),
                                ::testing::Values(QueryNetworkTest::generateParams(ConvBias))),
                        QueryNetworkTest::getTestCaseName);
}  // namespace
