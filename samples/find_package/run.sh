#!/bin/bash -x

BUILD_TYPE=Release
if [ "$#" -ge 1 ] ; then
    BUILD_TYPE=$1
fi

C4CONF_DIR=$(cd ../.. ; pwd)
THIS_DIR=$(pwd)

BUILD_DIR=$THIS_DIR/build/$BUILD_TYPE
mkdir -p $BUILD_DIR

C4CONF_BUILD_DIR=$BUILD_DIR/c4conf-build
C4CONF_INSTALL_DIR=$BUILD_DIR/c4conf-install


# configure c4conf
cmake -S "$C4CONF_DIR" -B "$C4CONF_BUILD_DIR" "-DCMAKE_INSTALL_PREFIX=$C4CONF_INSTALL_DIR" -DCMAKE_BUILD_TYPE=$BUILD_TYPE
# build c4conf
cmake --build "$C4CONF_BUILD_DIR" --parallel --config $BUILD_TYPE
# install c4conf
cmake --build "$C4CONF_BUILD_DIR" --config $BUILD_TYPE --target install


# configure the sample
cmake -S "$THIS_DIR" -B "$BUILD_DIR" "-DCMAKE_PREFIX_PATH=$C4CONF_INSTALL_DIR" -DCMAKE_BUILD_TYPE=$BUILD_TYPE
# build and run the sample
cmake --build "$BUILD_DIR" --config $BUILD_TYPE --target run
