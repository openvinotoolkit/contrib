// Copyright (C) 2020-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

package org.intel.openvino.compatibility;

public class Data extends IEWrapper {

    protected Data(long addr) {
        super(addr);
    }

    public void setLayout(Layout layout) {
        setLayout(nativeObj, layout.getValue());
    }

    public Layout getLayout() {
        return Layout.valueOf(getLayout(nativeObj));
    }

    public int[] getDims() {
        return GetDims(nativeObj);
    }

    /*----------------------------------- native methods -----------------------------------*/
    @Override
    protected native void delete(long nativeObj);

    private static native void setLayout(long addr, int layout);

    private static native int getLayout(long addr);

    private native int[] GetDims(long addr);
}
