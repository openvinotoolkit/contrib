// Copyright (C) 2020-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

package org.intel.openvino;

/**
 * This class represents a compiled model.
 *
 * <p>A model is compiled by a specific device by applying multiple optimization transformations,
 * then mapping to compute kernels.
 */
public class CompiledModel extends Wrapper {

    static {
        try {
            Class.forName("org.intel.openvino.NativeLibrary");
        } catch (ClassNotFoundException e) {
            throw new RuntimeException("Failed to load OpenVINO native libraries");
        }
    }

    protected CompiledModel(long addr) {
        super(addr);
    }

    /**
     * Creates an inference request object used to infer the compiled model. The created request has
     * allocated input and output tensors (which can be changed later).
     *
     * @return {@link InferRequest} object
     */
    public InferRequest create_infer_request() {
        return new InferRequest(CreateInferRequest(nativeObj));
    }

    /*----------------------------------- native methods -----------------------------------*/
    private static native long CreateInferRequest(long addr);

    @Override
    protected native void delete(long nativeObj);
}
