// Copyright (C) 2021-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "runtime.hpp"

namespace CUDA {

class Event : public Handle<cudaEvent_t> {
public:
    Event() : Handle((static_cast<__host__ cudaError_t (*)(cudaEvent_t* event)>(cudaEventCreate)), cudaEventDestroy) {}
    auto& record(const Stream& stream) {
        throwIfError(cudaEventRecord(get(), stream.get()));
        return *this;
    }
    auto&& record(const cudaStream_t& stream) {
        throwIfError(cudaEventRecord(get(), stream));
        return std::move(*this);
    }
    void synchronize() { throwIfError(cudaEventSynchronize(get())); }
    float elapsedSince(const Event& start) const { return createFirstArg(cudaEventElapsedTime, start.get(), get()); }
};

}  // namespace CUDA
