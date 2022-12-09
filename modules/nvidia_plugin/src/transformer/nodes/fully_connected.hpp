// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <array>
#include <ngraph/ops.hpp>
#include <ngraph/type/element_type.hpp>

namespace ov::nvidia_gpu::nodes {

class FullyConnected : public ov::op::Op {
public:
    FullyConnected(const ov::Output<Node>& A,
                   const ov::Output<Node>& B,
                   const ov::Output<Node>& C,
                   const bool& transpose_a,
                   const bool& transpose_b);

    OPENVINO_OP("FullyConnected", "nvidia_gpu");

    bool visit_attributes(ov::AttributeVisitor& visitor) override;

    std::shared_ptr<ov::Node> clone_with_new_inputs(const ov::OutputVector& new_args) const override;

    void validate_and_infer_types() override;

    bool get_transpose_a() const { return m_transpose_a; }
    bool get_transpose_b() const { return m_transpose_b; }

private:
    bool m_transpose_a;
    bool m_transpose_b;
};

}  // namespace ov::nvidia_gpu::nodes
