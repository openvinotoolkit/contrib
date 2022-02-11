// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "lstm_sequence_base.hpp"

namespace CUDAPlugin {

/**
 * @brief Implements `ngraph::op::v5::LSTMSequence` using cuDNN API
 */
class LSTMSequenceOp : public LSTMSequenceOpBase {
public:
    using NodeOp = ngraph::op::v5::LSTMSequence;
    LSTMSequenceOp(const CreationContext& context,
                   const NodeOp& node,
                   IndexCollection&& inputIds,
                   IndexCollection&& outputIds);

private:
    static LSTMSequenceOpBase::Config config();
    void setupLayoutAdapters();
};

}  // namespace CUDAPlugin
