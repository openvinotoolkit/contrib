// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once
#include <vector>
#include <openvino/op/op.hpp>

class VocabDecoder : public ov::op::Op {
public:
    OPENVINO_OP("VocabDecoder");

    VocabDecoder () = default;

    VocabDecoder(
        const ov::OutputVector& arguments,
        std::vector<int> skip_tokens
    ) :
        ov::op::Op(arguments) {
        std::cerr << "[ VocabDecoderConstructor ] Size: " << skip_tokens.size() << "\n";
        constructor_validate_and_infer_types();
    }

    void validate_and_infer_types() override;

    std::shared_ptr<ov::Node> clone_with_new_inputs(const ov::OutputVector& inputs) const override {
        return std::make_shared<VocabDecoder>(inputs, m_skip_tokens);
    }

    bool visit_attributes(ov::AttributeVisitor& visitor) override {
        return true;
    }

    bool evaluate(ov::TensorVector& outputs, const ov::TensorVector& inputs) const override;

    bool has_evaluate() const override {
        return true;
    }
private:
    std::vector<int> m_skip_tokens;
};
