// Copyright (C) 2020-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <details/ie_exception.hpp>

#include <arm_compute/runtime/NEON/functions/NEScale.h>
#include "arm_converter/arm_converter.hpp"


using Nearest_mode    = ngraph::op::v4::Interpolate::NearestMode;
using Transform_mode  = ngraph::op::v4::Interpolate::CoordinateTransformMode;
using InterpolateMode = ngraph::op::v4::Interpolate::InterpolateMode;


bool isSupportedConfiguration(const ngraph::op::v4::Interpolate& node) {
    auto& inp_shape = node.get_input_shape(0);
    auto& out_shape = node.get_output_shape(0);

    float scale_h = out_shape[2] / inp_shape[2];
    float scale_w = out_shape[3] / inp_shape[3];
    bool is_upsample = scale_h > 1 && scale_w > 1;

    auto& attrs = node.get_attrs();
    auto& coord_mode = attrs.coordinate_transformation_mode;
    auto& nearest_mode = attrs.nearest_mode;

    if (coord_mode == Transform_mode::asymmetric && nearest_mode == Nearest_mode::floor) {
        return true;
    }

    if (coord_mode == Transform_mode::align_corners && nearest_mode == Nearest_mode::round_prefer_ceil) {
        return true;
    }

    if (is_upsample) {
        if (coord_mode == Transform_mode::asymmetric && nearest_mode == Nearest_mode::simple) {
            return true;
        }

        bool int_factor = scale_h == static_cast<int>(scale_h) && scale_w == static_cast<int>(scale_w);
        if (int_factor && coord_mode != Transform_mode::asymmetric &&
            (nearest_mode == Nearest_mode::round_prefer_ceil || nearest_mode == Nearest_mode::round_prefer_floor)) {
            return true;
        }
    } else {
        float down_scale_h = inp_shape[2] / out_shape[2];
        float down_scale_w = inp_shape[3] / out_shape[3];
        bool int_factor = down_scale_h == static_cast<int>(down_scale_h) && down_scale_w == static_cast<int>(down_scale_w);

        if (int_factor && coord_mode != Transform_mode::align_corners && nearest_mode == Nearest_mode::simple) {
            return true;
        }

        if (int_factor && nearest_mode == Nearest_mode::round_prefer_ceil &&
            ((out_shape[2] > 1 && out_shape[3] > 1) || coord_mode != Transform_mode::half_pixel)) {
            return true;
        }
    }

    return false;
}


namespace ArmPlugin {
template<> Converter::Conversion::Ptr Converter::Convert(const opset::Interpolate& node) {
    auto& attrs = node.get_attrs();
    auto& inp_shape = node.get_input_shape(0);
    auto& out_shape = node.get_output_shape(0);

    if (inp_shape.size() != 4 || inp_shape[0] != out_shape[0] || inp_shape[1] != out_shape[1]) {
        THROW_IE_EXCEPTION << "Unsupported Interpolate op with input shape: " << inp_shape
                           << " and output shape: " << out_shape;
    }

    auto& pads_begin = attrs.pads_begin;
    auto& pads_end   = attrs.pads_end;
    if (!std::all_of(pads_begin.begin(), pads_begin.end(), [](int i){return i == 0;}) ||
        !std::all_of(pads_end.begin(), pads_end.end(), [](int i){return i == 0;})) {
        THROW_IE_EXCEPTION << "Unsupported Interpolate op with paddings";
    }

    if (attrs.antialias) {
        THROW_IE_EXCEPTION << "Unsupported Interpolate op with antialias";
    }

    auto& nearest_mode = attrs.nearest_mode;
    auto& coord_mode   = attrs.coordinate_transformation_mode;
    if (coord_mode == Transform_mode::tf_half_pixel_for_nn) {
        THROW_IE_EXCEPTION << "Unsupported Interpolate op with Interpolate::CoordinateTransformMode::tf_half_pixel_for_nn";
    }

    if (nearest_mode == Nearest_mode::ceil) {
        THROW_IE_EXCEPTION << "Unsupported Interpolate op with ceil mode";
    }

    arm_compute::InterpolationPolicy policy;
    switch (attrs.mode) {
        case opset::Interpolate::InterpolateMode::linear:
        case opset::Interpolate::InterpolateMode::linear_onnx:
            policy = arm_compute::InterpolationPolicy::BILINEAR;
            break;
        case opset::Interpolate::InterpolateMode::nearest:
            policy = arm_compute::InterpolationPolicy::NEAREST_NEIGHBOR;
            break;
        default:
            THROW_IE_EXCEPTION << "Unsupported interpolate mode";
    }

    if (policy == arm_compute::InterpolationPolicy::NEAREST_NEIGHBOR && !isSupportedConfiguration(node)) {
        THROW_IE_EXCEPTION << "Unsupported combination nearest_mode and coord_mode for Interpolate op";
    }

    arm_compute::SamplingPolicy coord = arm_compute::SamplingPolicy::TOP_LEFT;
    if ((coord_mode == Transform_mode::pytorch_half_pixel && out_shape[2] > 1 && out_shape[3] > 1) ||
        coord_mode == Transform_mode::half_pixel) {
        coord = arm_compute::SamplingPolicy::CENTER;
    }

    return MakeConversion<arm_compute::NEScale>(node.input(0),
                                                node.output(0),
                                                arm_compute::ScaleKernelInfo(policy,
                                                                             arm_compute::BorderMode::REPLICATE,
                                                                             arm_compute::PixelValue(),
                                                                             coord,
                                                                             false,
                                                                             coord_mode == Transform_mode::align_corners));
}
} //  namespace ArmPlugin
