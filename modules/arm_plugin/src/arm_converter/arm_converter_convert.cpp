// Copyright (C) 2020-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0


#include <arm_compute/runtime/NEON/functions/NEDepthConvertLayer.h>
#include <arm_compute/runtime/NEON/functions/NECopy.h>
#include "arm_converter/arm_converter.hpp"
#include <ngraph/runtime/reference/convert.hpp>

using type = ngraph::element::Type_t;

namespace ArmPlugin {
template <> Converter::Conversion::Ptr Converter::Convert(const opset::Convert& node) {
    auto make = [&] (auto refFunction) {
        return MakeConversion(refFunction,
                              node.input(0),
                              node.output(0),
                              ngraph::shape_size(node.get_input_shape(0)));
    };

    auto src = node.input(0).get_element_type();
    auto dst = node.get_convert_element_type();

    if ((src == type::u8  && (dst == type::u16 || dst == type::i16  || dst == type::i32)) ||
        (src == type::u16 && (dst == type::u8  || dst == type::u32)) ||
        (src == type::i16 && (dst == type::u8  || dst == type::i32)) ||
        (src == type::f16 && dst == type::f32) || (src == type::f32 && dst == type::f16)) {
        return MakeConversion<arm_compute::NEDepthConvertLayer>(node.input(0),
                                                                node.output(0),
                                                                arm_compute::ConvertPolicy::SATURATE);
    } else if (src == dst) {
        return MakeConversion<arm_compute::NECopy>(node.input(0), node.output(0));
    } else {
        switch (src) {
            case ngraph::element::Type_t::u8 :
                switch (dst) {
                    case ngraph::element::Type_t::i32 :
                        return make(ngraph::runtime::reference::convert<std::uint8_t, std::int32_t>);
                    case ngraph::element::Type_t::u32 :
                        return make(ngraph::runtime::reference::convert<std::uint8_t, std::uint32_t>);
                    case ngraph::element::Type_t::f16 :
                        return make(ngraph::runtime::reference::convert<std::uint8_t, half_float::half>);
                    case ngraph::element::Type_t::f32 :
                        return make(ngraph::runtime::reference::convert<std::uint8_t, float>);
                default:
                    THROW_IE_EXCEPTION << "Unsupported convertion from " << src << " to " << dst; return {};
                }
            case ngraph::element::Type_t::i16 :
                switch (dst) {
                    case ngraph::element::Type_t::u16 :
                        return make(ngraph::runtime::reference::convert<std::int16_t, std::uint16_t>);
                    case ngraph::element::Type_t::i32 :
                        return make(ngraph::runtime::reference::convert<std::int16_t, std::int32_t>);
                    case ngraph::element::Type_t::u32 :
                        return make(ngraph::runtime::reference::convert<std::int16_t, std::uint32_t>);
                    case ngraph::element::Type_t::f16 :
                        return make(ngraph::runtime::reference::convert<std::int16_t, half_float::half>);
                    case ngraph::element::Type_t::f32 :
                        return make(ngraph::runtime::reference::convert<std::int16_t, float>);
                default:
                    THROW_IE_EXCEPTION << "Unsupported convertion from " << src << " to " << dst; return {};
                }
            case ngraph::element::Type_t::u16 :
                switch (dst) {
                    case ngraph::element::Type_t::i32 :
                        return make(ngraph::runtime::reference::convert<std::uint16_t, std::int32_t>);
                    case ngraph::element::Type_t::f16 :
                        return make(ngraph::runtime::reference::convert<std::uint16_t, half_float::half>);
                    case ngraph::element::Type_t::f32 :
                        return make(ngraph::runtime::reference::convert<std::uint16_t, float>);
                default:
                    THROW_IE_EXCEPTION << "Unsupported convertion from " << src << " to " << dst; return {};
                }
            case ngraph::element::Type_t::i32 :
                switch (dst) {
                    case ngraph::element::Type_t::u8 :
                        return make(ngraph::runtime::reference::convert<std::int32_t, std::uint8_t>);
                    case ngraph::element::Type_t::i16 :
                        return make(ngraph::runtime::reference::convert<std::int32_t, std::int16_t>);
                    case ngraph::element::Type_t::u32 :
                        return make(ngraph::runtime::reference::convert<std::int32_t, std::uint32_t>);
                    case ngraph::element::Type_t::f16 :
                        return make(ngraph::runtime::reference::convert<std::int32_t, half_float::half>);
                    case ngraph::element::Type_t::f32 :
                        return make(ngraph::runtime::reference::convert<std::int32_t, float>);
                default:
                    THROW_IE_EXCEPTION << "Unsupported convertion from " << src << " to " << dst; return {};
                }
            case ngraph::element::Type_t::u32 :
                switch (dst) {
                    case ngraph::element::Type_t::u8 :
                        return make(ngraph::runtime::reference::convert<std::uint32_t, std::uint8_t>);
                    case ngraph::element::Type_t::i32 :
                        return make(ngraph::runtime::reference::convert<std::uint32_t, std::int32_t>);
                    case ngraph::element::Type_t::f16 :
                        return make(ngraph::runtime::reference::convert<std::uint32_t, half_float::half>);
                    case ngraph::element::Type_t::f32 :
                        return make(ngraph::runtime::reference::convert<std::uint32_t, float>);
                default:
                    THROW_IE_EXCEPTION << "Unsupported convertion from " << src << " to " << dst; return {};
                }
            case ngraph::element::Type_t::f16 :
                switch (dst) {
                    case ngraph::element::Type_t::u8 :
                        return make(ngraph::runtime::reference::convert<half_float::half, std::uint8_t>);
                    case ngraph::element::Type_t::i16 :
                        return make(ngraph::runtime::reference::convert<half_float::half, std::int16_t>);
                    case ngraph::element::Type_t::i32 :
                        return make(ngraph::runtime::reference::convert<half_float::half, std::int32_t>);
                default:
                    THROW_IE_EXCEPTION << "Unsupported convertion from " << src << " to " << dst; return {};
                }
            case ngraph::element::Type_t::f32 :
                switch (dst) {
                    case ngraph::element::Type_t::u8 :
                        return make(ngraph::runtime::reference::convert<float, std::uint8_t>);
                    case ngraph::element::Type_t::i16 :
                        return make(ngraph::runtime::reference::convert<float, std::int16_t>);
                    case ngraph::element::Type_t::i32 :
                        return make(ngraph::runtime::reference::convert<float, std::int32_t>);
                default:
                    THROW_IE_EXCEPTION << "Unsupported convertion from " << src << " to " << dst; return {};
                }
            default: THROW_IE_EXCEPTION << "Unsupported Type: " << node.get_element_type(); return {};
        }
    }
}
}  //  namespace ArmPlugin
