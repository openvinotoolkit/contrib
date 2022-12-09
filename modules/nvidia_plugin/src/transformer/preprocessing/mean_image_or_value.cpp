// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "openvino/cc/ngraph/itt.hpp"
#include "transformer/preprocessing/mean_image_or_value.hpp"

#include <ngraph/opsets/opset3.hpp>
#include "openvino/pass/manager.hpp"
#include "openvino/pass/pattern/op/wrap_type.hpp"

ov::nvidia_gpu::pass::AddMeanSubtract::AddMeanSubtract(const MeanMap& inputInfoMap) {
    MATCHER_SCOPE(AddMeanSubtract);
    auto label = ov::pass::pattern::wrap_type<ngraph::opset3::Parameter>();

    matcher_pass_callback callback = [=](ov::pass::pattern::Matcher& m) {
        auto param = std::dynamic_pointer_cast<ngraph::opset3::Parameter>(m.get_match_root());
        if (!param) {
            return false;
        }

        auto it = inputInfoMap.find(param->get_friendly_name());
        if (it == inputInfoMap.end()) {
            return false;
        }

        auto mean_const = it->second;
        NGRAPH_CHECK(mean_const->get_element_type() == ov::element::f32,
                     "Mean for ",
                     param->get_friendly_name(),
                     " must have f32 type");

        auto copy_param = param->clone_with_new_inputs({});
        auto sub = std::make_shared<ngraph::opset3::Subtract>(copy_param, mean_const);

        ngraph::replace_node(param, sub);
        sub->set_argument(0, param);

        // Return true as the root node was changed
        return true;
    };

    // Register pattern with Parameter operation as a pattern root node
    auto m = std::make_shared<ov::pass::pattern::Matcher>(label, matcher_name);
    // Register Matcher
    register_matcher(m, callback);
}
