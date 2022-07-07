Z:

cd Z:\IdeaProjects\JNativeHook\src\external\libuiohook

cmake -B .\build ^
  -G "Visual Studio 16 2019" -A x64 ^
  -D CMAKE_INSTALL_PREFIX=.\dist\windows\x86_64 ^
  -D CMAKE_VERBOSE_MAKEFILE=true ^
  -D USE_EPOCH_TIME=OFF ^
  -D BUILD_SHARED_LIBS=ON ^
  -D BUILD_DEMO=ON

cmake --build .\build ^
  --parallel 2 ^
  --config RelWithDebInfo

cmake --install .\build --config RelWithDebInfo
