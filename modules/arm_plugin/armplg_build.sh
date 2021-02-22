#!/bin/sh

BUILD_JOBS=${BUILD_JOBS:-$(nproc)}
BUILD_TYPE=${BUILD_TYPE:-Release}
UPDATE_SOURCES=${UPDATE_SOURCES:-clean}

DEV_HOME=`pwd`
OPENCV_HOME=$DEV_HOME/opencv
OPENVINO_HOME=$DEV_HOME/openvino
OPENVINO_CONTRIB=$DEV_HOME/openvino_contrib
ARM_PLUGIN_HOME=$OPENVINO_CONTRIB/modules/arm_plugin
STAGING_DIR=$DEV_HOME/armcpu_package



fail()
{
    if [ $# -lt 2 ]; then
      echo "Script internal error"
      exit 31
    fi
    retval=$1
    shift
    echo $@
    exit $retval
}

checkSrcTree()
{
    [ $# -lt 2 ] && fail

    TEST_DIR_NAME=directory
    if [ $# -gt 2 ]; then
        TEST_DIR_NAME="$3 directory"
    fi
    if ! [ -d $1 ]; then
        echo "Unable to detect $TEST_DIR_NAME at $1"
        echo "Cloning master branch only..."
        git lfs clone --recurse-submodules --single-branch --branch=master $2 $1 || fail 3 "Failed to clone $TEST_DIR_NAME. Stopping"
    else
        echo "Detected $TEST_DIR_NAME at $1"
        echo "Considering it as source directory"
        if [ "$UPDATE_SOURCES" = "reload" ]; then
            echo "Source reloading requested"
            echo "Removing existing sources..."
            rm -rf $1 || fail 1 "Failed to remove. Stopping"
            echo "Cloning master branch only..."
            git lfs clone --recurse-submodules --single-branch --branch=master $2 $1 || fail 3 "Failed to clone $TEST_DIR_NAME. Stopping"
        elif [ -d $1/build ]; then
            echo "Build directory detected at $1"
            if [ "$UPDATE_SOURCES" = "clean" ]; then
                echo "Cleanup of previous build requested"
                echo "Removing previous build results..."
                rm -rf $1/build || fail 2 "Failed to cleanup. Stopping"
            fi
        fi
    fi
    return 0
}



#Prepare sources
checkSrcTree $OPENCV_HOME https://github.com/opencv/opencv.git OpenCV
checkSrcTree $OPENVINO_HOME https://github.com/openvinotoolkit/openvino.git OpenVINO
checkSrcTree $OPENVINO_CONTRIB https://github.com/openvinotoolkit/openvino_contrib.git OpenVINO_contrib

#cleanup package destination folder
[ -e $STAGING_DIR ] && rm -rf $STAGING_DIR
mkdir -p $STAGING_DIR

#Build OpenCV
mkdir -p $OPENCV_HOME/build && \
cd $OPENCV_HOME/build && \
PYTHONVER=`ls /usr/include | grep "python3[^m]*$"` && \
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DBUILD_LIST=imgcodecs,videoio,highgui,python3 \
      -DBUILD_opencv_python2=OFF -DBUILD_opencv_python3=ON -DOPENCV_SKIP_PYTHON_LOADER=ON \
      -DPYTHON3_INCLUDE_PATH=/usr/include/${PYTHONVER}m \
      -DPYTHON3_LIBRARIES=/usr/lib/$ARCH_NAME/lib${PYTHONVER}m.so \
      -DPYTHON3_NUMPY_INCLUDE_DIRS=/usr/lib/python3/dist-packages/numpy/core/include \
      -D CMAKE_USE_RELATIVE_PATHS=ON \
      -D CMAKE_SKIP_INSTALL_RPATH=ON \
      -D OPENCV_SKIP_PKGCONFIG_GENERATION=ON \
      -D OPENCV_SKIP_PYTHON_LOADER=ON \
      -D OPENCV_SKIP_CMAKE_ROOT_CONFIG=ON \
      -D OPENCV_BIN_INSTALL_PATH=bin \
      -D OPENCV_INCLUDE_INSTALL_PATH=include \
      -D OPENCV_LIB_INSTALL_PATH=lib \
      -D OPENCV_CONFIG_INSTALL_PATH=cmake \
      -D OPENCV_3P_LIB_INSTALL_PATH=3rdparty \
      -D OPENCV_SAMPLES_SRC_INSTALL_PATH=samples \
      -D OPENCV_DOC_INSTALL_PATH=doc \
      -D OPENCV_OTHER_INSTALL_PATH=etc \
      -D OPENCV_LICENSES_INSTALL_PATH=etc/licenses \
      -D ENABLE_CXX11=ON \
      -D CMAKE_INSTALL_PREFIX=install \
      -DCMAKE_TOOLCHAIN_FILE="$OPENVINO_HOME/cmake/$TOOLCHAIN_DEFS" \
      -DCMAKE_STAGING_PREFIX=$STAGING_DIR/opencv \
      $OPENCV_HOME && \
make -j$BUILD_JOBS && \
make install && \
echo export OpenCV_DIR=\$INSTALLDIR/opencv/cmake > $STAGING_DIR/opencv/setupvars.sh && \
echo export LD_LIBRARY_PATH=\$INSTALLDIR/opencv/lib:\$LD_LIBRARY_PATH >> $STAGING_DIR/opencv/setupvars.sh && \
cd $DEV_HOME || fail 11 "OpenCV build failed. Stopping"

#Build OpenVINO
mkdir -p $OPENVINO_HOME/build && \
cd $OPENVINO_HOME/build && \
cmake -DOpenCV_DIR=$STAGING_DIR/opencv/cmake -DENABLE_OPENCV=OFF \
      -DENABLE_TESTS=ON -DENABLE_BEH_TESTS=ON -DENABLE_FUNCTIONAL_TESTS=ON \
      -DENABLE_GAPI_TESTS=OFF -DENABLE_CLDNN_TESTS=OFF \
      -DENABLE_DATA=OFF -DENABLE_MODELS=OFF -DENABLE_VALIDATION_SET=OFF -DENABLE_PRIVATE_MODELS=OFF -DENABLE_PROFILING_ITT=OFF \
      -DTHREADS_PTHREAD_ARG="-pthread" -DCMAKE_EXE_LINKER_FLAGS=-Wl,-rpath-link,$STAGING_DIR/opencv/lib -DCMAKE_INSTALL_LIBDIR=lib \
      -DENABLE_SSE42=OFF -DENABLE_MYRIAD=ON -DENABLE_GNA=OFF -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
      -DENABLE_VALIDATION_SET=OFF -DENABLE_MODELS=OFF \
      -DENABLE_LTO=ON \
      -DCMAKE_CXX_FLAGS=-latomic -DOPENCV_EXTRA_EXE_LINKER_FLAGS=-latomic \
      -DCMAKE_TOOLCHAIN_FILE="$OPENVINO_HOME/cmake/$TOOLCHAIN_DEFS" \
      -DCMAKE_STAGING_PREFIX=$STAGING_DIR \
      $OPENVINO_HOME && \
make -j$BUILD_JOBS && \
ARCHDIR=`ls $OPENVINO_HOME/bin` && \
cd $DEV_HOME || fail 12 "OpenVINO build failed. Stopping"

#OpenVINO python
[ "$UPDATE_SOURCES" = "clean" -a -e $OPENVINO_HOME/pbuild ] && rm -rf $OPENVINO_HOME/pbuild
mkdir -p $OPENVINO_HOME/pbuild && \
cd $OPENVINO_HOME/pbuild && \
cmake -DInferenceEngineDeveloperPackage_DIR=$OPENVINO_HOME/build \
      -DENABLE_PYTHON=ON \
      -DPYTHON_EXECUTABLE="/usr/bin/${PYTHONVER}m" \
      -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
      -DENABLE_DATA=OFF -DENABLE_MODELS=OFF -DENABLE_VALIDATION_SET=OFF -DENABLE_PRIVATE_MODELS=OFF \
      -DTHREADS_PTHREAD_ARG="-pthread" -DCMAKE_EXE_LINKER_FLAGS=-Wl,-rpath-link,$STAGING_DIR/opencv/lib \
      -DCMAKE_TOOLCHAIN_FILE="$OPENVINO_HOME/cmake/$TOOLCHAIN_DEFS" \
      -DCMAKE_STAGING_PREFIX=$STAGING_DIR \
      $OPENVINO_HOME/inference-engine/ie_bridges/python && \
make -j$BUILD_JOBS && \
cd $DEV_HOME || fail 13 "OpenVINO python bindings build failed. Stopping"

#ArmCPU plugin
mkdir -p $ARM_PLUGIN_HOME/build && \
cd $ARM_PLUGIN_HOME/build && \
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DInferenceEngineDeveloperPackage_DIR=$OPENVINO_HOME/build \
      -DENABLE_TESTS=ON -DENABLE_BEH_TESTS=ON -DENABLE_FUNCTIONAL_TESTS=ON \
      -DARM_COMPUTE_WERROR=0 \
      -DCMAKE_TOOLCHAIN_FILE="$OPENVINO_HOME/cmake/$TOOLCHAIN_DEFS" \
      -DCMAKE_STAGING_PREFIX=$STAGING_DIR \
      $ARM_PLUGIN_HOME && \
make -j$BUILD_JOBS && \
cd $DEV_HOME || fail 14 "ARM plugin build failed. Stopping"

#OpenVINO NGraph deployment
cd $OPENVINO_HOME/build/ngraph && \
mkdir -p $STAGING_DIR/deployment_tools && \
cmake -DCOMPONENT=ngraph \
      -DCMAKE_STAGING_PREFIX=$STAGING_DIR \
      -DCMAKE_INSTALL_PREFIX=$STAGING_DIR \
      -P cmake_install.cmake && \
cp $OPENVINO_HOME/bin/$ARCHDIR/$BUILD_TYPE/lib/libinterpreter_backend.so $STAGING_DIR/deployment_tools/ngraph/lib/libinterpreter_backend.so && \
cd $DEV_HOME || fail 15 "OpenVINO NGraph deployment failed. Stopping"



#Package structure creation
mkdir -p $STAGING_DIR/deployment_tools/inference_engine/bin/$ARCHDIR && \
find $OPENVINO_HOME/bin/$ARCHDIR/$BUILD_TYPE -maxdepth 1 -type f -exec cp -v {} $STAGING_DIR/deployment_tools/inference_engine/bin/$ARCHDIR \; && \
mkdir -p $STAGING_DIR/deployment_tools/inference_engine/lib/$ARCHDIR && \
find $OPENVINO_HOME/bin/$ARCHDIR/$BUILD_TYPE/lib -maxdepth 1 -type f -exec cp -v {} $STAGING_DIR/deployment_tools/inference_engine/lib/$ARCHDIR \; && \
cp -vr $OPENVINO_HOME/bin/$ARCHDIR/$BUILD_TYPE/lib/python_api $STAGING_DIR/python && \
cp -vr $OPENVINO_HOME/build/share $STAGING_DIR/deployment_tools/inference_engine/ && \
cp -v $OPENVINO_HOME/inference-engine/scripts/dependencies.* $OPENVINO_HOME/build/dependencies_64.txt $STAGING_DIR/ && \
mkdir -p $STAGING_DIR/python/${PYTHONVER}/openvino && \
cp -vr $OPENVINO_HOME/tools $STAGING_DIR/python/${PYTHONVER}/openvino/ && \
cp -vr $OPENVINO_HOME/inference-engine/include $STAGING_DIR/deployment_tools/inference_engine/ && \
cp -vr $OPENVINO_HOME/inference-engine/ie_bridges/c/include/c_api $STAGING_DIR/deployment_tools/inference_engine/include/ && \
mkdir -p $STAGING_DIR/deployment_tools/inference_engine/samples && \
cp -vr $OPENVINO_HOME/inference-engine/samples $STAGING_DIR/deployment_tools/inference_engine/samples/cpp && \
cp -vr $OPENVINO_HOME/inference-engine/ie_bridges/c/samples $STAGING_DIR/deployment_tools/inference_engine/samples/c && \
cp -vr $OPENVINO_HOME/inference-engine/ie_bridges/python/sample $STAGING_DIR/deployment_tools/inference_engine/samples/python && \
cp -vr $OPENVINO_HOME/inference-engine/tools $STAGING_DIR/deployment_tools/tools && \
find $OPENVINO_HOME/model-optimizer -maxdepth 1 -type f -exec cp -v {} $STAGING_DIR/deployment_tools/model-optimizer \; && \
find $OPENVINO_HOME/model-optimizer/.coveragerc -maxdepth 1 -type f -exec cp -v {} $STAGING_DIR/deployment_tools/model-optimizer \; && \
cp -vr $OPENVINO_HOME/scripts/setupvars $STAGING_DIR/bin && \
cp -vr $OPENVINO_HOME/scripts/demo $STAGING_DIR/deployment_tools/demo && \
cp -vr $OPENVINO_HOME/scripts/install_dependencies $STAGING_DIR/install_dependencies && \
cp -vr $OPENVINO_HOME/inference-engine/tools $STAGING_DIR/deployment_tools/python_tools && \
echo "=================================RPATH cleaning==================================" && \
find $STAGING_DIR/deployment_tools/inference_engine/lib/$ARCHDIR/ -maxdepth 1 -type f -name "*.so" -exec chrpath --delete {} \; && \
find $STAGING_DIR/deployment_tools/inference_engine/bin/$ARCHDIR/ -maxdepth 1 -type f -exec chrpath --delete {} \; && \
find $STAGING_DIR/python/${PYTHONVER}/openvino/inference_engine/ -maxdepth 1 -type f -name "*.so" -exec chrpath --delete {} \; || \
fail 21 "Package structure preparation failed. Stopping"

#Package creation
mkdir -p $DEV_HOME/pack && \
cd $OPENVINO_HOME/build/ && cpack -B $DEV_HOME/pack/ -G ZIP && \
cd $OPENVINO_HOME/pbuild/ && cpack -B $DEV_HOME/pack/ -G ZIP && \
rm -rf $DEV_HOME/pack/_CPack_Packages && \
7z -tzip -mmt16 a "$DEV_HOME/pack/install_pkg.zip" "$STAGING_DIR/*" && \
cd $DEV_HOME || fail 22 "Package creation failed. Stopping"

#Repackaging
mkdir -p $DEV_HOME/unpack && \
cd $DEV_HOME/unpack && \
7z x "$DEV_HOME/pack/install_pkg.zip" && \
rm -rf $DEV_HOME/pack && \
tar -czvf ../OV_ARM_package.tar.gz ./* && \
cd $DEV_HOME && \
rm -rf $DEV_HOME/unpack || \
fail 23 "Package creation failed. Nothing more to do"

exit 0
