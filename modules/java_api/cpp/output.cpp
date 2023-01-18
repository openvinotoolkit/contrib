// Copyright (C) 2020-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <jni.h> // JNI header provided by JDK
#include "openvino/openvino.hpp"

#include "openvino_java.hpp"
#include "jni_common.hpp"

using namespace ov;

JNIEXPORT jstring JNICALL Java_org_intel_openvino_Output_GetAnyName(JNIEnv *env, jobject obj, jlong addr) {
    JNI_METHOD("GetAnyName",
        Output<Node> *output = (Output<Node> *)addr;
        return env->NewStringUTF(output->get_any_name().c_str());
    )
    return 0;
}

JNIEXPORT jintArray JNICALL Java_org_intel_openvino_Output_GetShape(JNIEnv *env, jobject obj, jlong addr) {
    JNI_METHOD("GetShape",
        Output<Node> *output = (Output<Node> *)addr;
        Shape shape = output->get_shape();

        jintArray result = env->NewIntArray(shape.size());
        if (!result) {
            throw std::runtime_error("Out of memory!");
        } jint *arr = env->GetIntArrayElements(result, nullptr);

        for (int i = 0; i < shape.size(); ++i)
            arr[i] = shape[i];

        env->ReleaseIntArrayElements(result, arr, 0);
        return result;
    )
    return 0;
}

JNIEXPORT void JNICALL Java_org_intel_openvino_Output_delete(JNIEnv *, jobject, jlong addr)
{
    Output<Node> *obj = (Output<Node> *)addr;
    delete obj;
}
