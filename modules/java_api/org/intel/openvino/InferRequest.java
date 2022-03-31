// Copyright (C) 2020-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

package org.intel.openvino;

public class InferRequest extends Wrapper {
    protected InferRequest(long addr) {
        super(addr);
    }

    public void infer() {
        Infer(nativeObj);
    }

    public void start_async() {
        StartAsync(nativeObj);
    }

    public void wait_async() {
        Wait(nativeObj);
    }

    public void set_input_tensor(Tensor input) {
        SetInputTensor(nativeObj, input.nativeObj);
    }

    public void set_output_tensor(Tensor tensor) {
        SetOutputTensor(nativeObj, tensor.nativeObj);
    }

    public Tensor get_output_tensor() {
        return new Tensor(GetOutputTensor(nativeObj));
    }

    public Tensor get_tensor(String tensorName) {
        return new Tensor(GetTensor(nativeObj, tensorName));
    }

    /*----------------------------------- native methods -----------------------------------*/
    private static native void Infer(long addr);

    private static native void StartAsync(long addr);

    private static native void Wait(long addr);

    private static native void SetInputTensor(long addr, long tensorAddr);

    private static native void SetOutputTensor(long addr, long tensorAddr);

    private static native long GetOutputTensor(long addr);

    private static native long GetTensor(long addr, String tensorName);

    @Override
    protected native void delete(long nativeObj);
}
