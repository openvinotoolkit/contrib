package org.intel.openvino;

public class Wrapper {
    protected final long nativeObj;

    protected Wrapper(long addr) {
        nativeObj = addr;
    }

    protected long getNativeObjAddr() {
        return nativeObj;
    }

    @Override
    protected void finalize() throws Throwable {
        delete(nativeObj);
        super.finalize();
    }

    /*----------------------------------- native methods -----------------------------------*/
    protected native void delete(long nativeObj);
}
