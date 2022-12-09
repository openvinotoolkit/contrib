// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "openvino/cc/ngraph/itt.hpp"
#include "transformer/preprocessing/std_scale.hpp"

#include <ngraph/opsets/opset3.hpp>
#include "openvino/pass/pattern/op/wrap_type.hpp"
#include "openvino/pass/manager.hpp"

using namespace ov::pass::pattern;

ov::nvidia_gpu::pass::AddStdScale::AddStdScale(const ScaleMap& inputInfoMap) {
    MATCHER_SCOPE(AddStdScale);
    auto label = wrap_type<ngraph::opset3::Parameter>();

    matcher_pass_callback callback = [=](Matcher& m) {
        auto param = std::dynamic_pointer_cast<ngraph::opset3::Parameter>(m.get_match_root());
        if (!param) {
            return false;
        }

        auto it = inputInfoMap.find(param->get_friendly_name());
        if (it == inputInfoMap.end()) {
            return false;
        }

        auto scale_const = it->second;
        NGRAPH_CHECK(scale_const->get_element_type() == ov::element::f32,
                     "Scale for ",
                     param->get_friendly_name(),
                     " must have f32 type");

        auto copy_param = param->clone_with_new_inputs({});
        auto div = std::make_shared<ngraph::opset3::Divide>(copy_param, it->second);

        ngraph::replace_node(param, div);
        div->set_argument(0, param);

        // Return true as the root node was changed
        return true;
    };

    // Register pattern with Parameter operation as a pattern root node
    auto m = std::make_shared<Matcher>(label, matcher_name);
    // Register Matcher
    register_matcher(m, callback);
}
