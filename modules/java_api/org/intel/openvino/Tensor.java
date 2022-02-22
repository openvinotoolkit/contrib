package org.intel.openvino;

public class Tensor extends Wrapper {
    public Tensor(long addr) {
        super(addr);
    }

    public Tensor(ElementType type, int[] dims, long cArray) {
        super(TensorCArray(type.getValue(), dims, cArray));
    }

    public Tensor(int[] dims, float[] data) {
        super(TensorFloat(dims, data, data.length));
    }

    public int get_size() {
        return GetSize(nativeObj);
    }

    public int[] get_shape() {
        return GetShape(nativeObj);
    }

    public float[] data() {
        return asFloat(nativeObj);
    }

    /*----------------------------------- native methods -----------------------------------*/
    private static native long TensorCArray(int type, int[] shape, long cArray);

    private static native long TensorFloat(int[] shape, float[] data, int size);

    private static native int[] GetShape(long addr);

    private static native float[] asFloat(long addr);

    private static native int GetSize(long addr);

    @Override
    protected native void delete(long nativeObj);
}
