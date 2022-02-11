// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <gtest/gtest.h>

#include <cuda_config.hpp>
#include <cuda_operation_registry.hpp>
#include <ngraph/node.hpp>
#include <ngraph/op/parameter.hpp>
#include <ngraph/op/convert.hpp>

#include <ops/parameter.hpp>
#include <random>
#include <chrono>
#include <iomanip>

namespace {

struct ConvertTest : testing::Test {
    CUDA::ThreadContext threadContext { CUDA::Device {} };
    const ngraph::Shape inputTensorShape {1, 1, 3, 1024, 1024};
    InferenceEngine::BlobMap empty;

    auto create_operation(ngraph::element::Type_t input, ngraph::element::Type_t output) {
        auto param = std::make_shared<ngraph::op::v0::Parameter>(
                ngraph::element::Type(input), ngraph::PartialShape(inputTensorShape));
        const auto node = std::make_shared<ngraph::op::v0::Convert>(
                    param->output(0),
                    ngraph::element::Type(output));

        static constexpr bool optimizeOption = false;
        auto& registry = CUDAPlugin::OperationRegistry::getInstance();
        return registry.createOperation(
            CUDA::CreationContext {threadContext.device(), optimizeOption},
                node, std::vector<CUDAPlugin::TensorID> {0u}, std::vector<CUDAPlugin::TensorID> {0u});
    }
};

TEST_F(ConvertTest, DISABLED_benchmark) {
    using microseconds = std::chrono::duration<double, std::micro>;
    constexpr int kNumAttempts = 200;

    auto& stream = threadContext.stream();
    InferenceEngine::gpu::InferenceRequestContext context { empty, empty,
                threadContext };

    using Type_t = ngraph::element::Type_t;
    static constexpr auto supported_types = {
        Type_t::boolean,
        Type_t::bf16,
        Type_t::f16,
        Type_t::f32,
        Type_t::f64,
        Type_t::i8,
        Type_t::i16,
        Type_t::i32,
        Type_t::i64,
        /*Type_t::u1, convert doesn't support it*/
        Type_t::u8,
        Type_t::u16,
        Type_t::u32,
        Type_t::u64
    };
    for (auto inputIdx : supported_types) {
        for (auto outputIdx : supported_types) {
            const auto inputType = Type_t(static_cast<std::underlying_type<Type_t>::type>(inputIdx));
            const auto outputType = Type_t(static_cast<std::underlying_type<Type_t>::type>(outputIdx));
            auto op = create_operation(inputType, outputType);
            const auto input_type = ngraph::element::Type(inputType);
            const auto output_type = ngraph::element::Type(outputType);
            const auto inputBufferSize = ngraph::shape_size(inputTensorShape) * input_type.size();
            const auto ouputBufferSize = ngraph::shape_size(inputTensorShape) * output_type.size();
            const CUDA::Allocation inAlloc =  stream.malloc(inputBufferSize);
            const CUDA::Allocation outAlloc = stream.malloc(ouputBufferSize);
            std::vector<InferenceEngine::gpu::DevicePointer<const void*>> inputs { inAlloc };
            std::vector<InferenceEngine::gpu::DevicePointer<void*>> outputs { outAlloc };
            std::vector<u_char> in(inputBufferSize);
            std::random_device r_device;
            std::mt19937 mersenne_engine {r_device()};
            std::uniform_int_distribution<u_char> dist {std::numeric_limits<u_char>::min(),
                std::numeric_limits<u_char>::max()};
            auto gen = [&dist, &mersenne_engine](){
                return 10.f * dist(mersenne_engine) / std::numeric_limits<u_char>::max();
            };
            std::generate(in.begin(), in.end(), gen);
            stream.upload(inAlloc, in.data(), inputBufferSize);

            auto start = std::chrono::steady_clock::now();
            for (int i = 0; i < kNumAttempts; i++) {
                CUDAPlugin::Workbuffers workbuffers {};
                op->Execute(context, inputs, outputs, workbuffers);
                stream.synchronize();
            }
            auto end = std::chrono::steady_clock::now();
            microseconds average_exec_time = (end - start) / kNumAttempts;
            if (inputType == outputType)
                std::cout << "    ";
            std::cout << std::fixed <<
                    std::setfill('0') << "Input type:" << input_type.get_type_name() << " Output type:" << output_type.get_type_name()
                      << " Strided slice execution time: " << average_exec_time.count() << " microseconds\n";
        }
        std::cout << std::endl;
    }
}
} // namespace
