#!/usr/bin/env bash

set -e

BUILD_JOBS=${BUILD_JOBS:-$(nproc)}
BUILD_TYPE=${BUILD_TYPE:-Release}

[[ -n "${OPENVINO_HOME}" ]] || { echo "OPENVINO_HOME environment variable is expected"; exit 1; }
[[ -n "${OPENVINO_CONTRIB}" ]] || { echo "OPENVINO_CONTRIB environment variable is expected"; exit 1; }
[[ -n "${OPENVINO_BUILD_PATH}" ]] || { echo "OPENVINO_BUILD_PATH environment variable is expected"; exit 1; }
[[ -d "${OPENVINO_HOME}" ]] || { echo "OPENVINO_HOME=${OPENVINO_HOME} does not exist"; exit 1; }
[[ -d "${OPENVINO_CONTRIB}" ]] || { echo "OPENVINO_CONTRIB=${OPENVINO_CONTRIB} does not exist"; exit 1; }
[[ -d "${OPENVINO_CONTRIB}/modules/cuda_plugin" ]] || { echo "Path ${OPENVINO_CONTRIB}/modules/cuda_plugin does not exist"; exit 1; }
[[ "$1" == "--setup" || "$1" == "--build" || "$1" == "--rebuild" ]] || {
  printf "One of the following command should be provided:\n\
  - --setup (allow configure cmake project)\n\
  - --build (allow configure cmake project and then build)\n\
  - --rebuild (allow to rebuild, clean old configure, generate new one and then build)\n";
  exit 1;
}

if [[ "$1" == "--setup" || "$1" == "--build" ]]; then
  if [[ ! -d "${OPENVINO_BUILD_PATH}" ]]; then
      mkdir -p "${OPENVINO_BUILD_PATH}"
  fi
fi
if [[ "$1" == "--rebuild" ]]; then
  rm -rf "${OPENVINO_BUILD_PATH}"
  mkdir -p "${OPENVINO_BUILD_PATH}"
fi
cd "${OPENVINO_BUILD_PATH}"
cmake "${OPENVINO_HOME}" \
      -DENABLE_CUDA=ON \
      -DENABLE_TESTS=ON \
      -DBUILD_arm_plugin=OFF \
      -DBUILD_java_api=OFF \
      -DIE_EXTRA_MODULES="${OPENVINO_CONTRIB}/modules" \
      -DWHEEL_VERSION=2021.3.0 \
      -DVERBOSE_BUILD=ON \
      -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

if [[ "$1" != "--setup" ]]; then
  cmake --build "${OPENVINO_BUILD_PATH}" --target CudaFuncTests CudaUnitTests CUDAPlugin benchmark_app -j "${BUILD_JOBS}"
fi
