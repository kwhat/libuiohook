#!/bin/sh

cmake -B ./build \
  -G "Unix Makefiles" \
  -D CMAKE_INSTALL_PREFIX=./dist/osx/x86_64 \
  -D CMAKE_VERBOSE_MAKEFILE=true \
  -D USE_EPOCH_TIME=ON \
  -D BUILD_SHARED_LIBS=ON \
  -D BUILD_DEMO=ON

cmake --build ./build \
  --parallel 2 \
  --config RelWithDebInfo \
  --clean-first

cmake --install ./build --config RelWithDebInfo
