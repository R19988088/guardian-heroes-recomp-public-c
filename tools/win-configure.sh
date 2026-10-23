#!/bin/bash
# Windows PC build with MSYS2 UCRT64 clang (C:\msys64): run from an MSYS2 UCRT64 shell.
#   tools/win-configure.sh [build|configure]
# Vulkan only: the mingw headers lack DXProgrammableCapture.h (D3D12). SDK copy: C:\proj\gh\sdk (rsync from WSL /root/gh/rexglue-sdk, with the Switch/gh edits applied);
# generated code: generated/default (copied from the WSL codegen; git-ignored).
set -e
SRC=/d/git/dev/gh-recomp
SDK=/c/proj/gh/sdk
B=/c/proj/gh/build
export PATH=/ucrt64/bin:$PATH
cmake -S $SRC -B $B -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    "-DCMAKE_C_FLAGS=-IC:/proj/gh/shim -mssse3 -msse4.1" "-DCMAKE_CXX_FLAGS=-IC:/proj/gh/shim -mssse3 -msse4.1 -fms-extensions" -DCMAKE_SYSTEM_PROCESSOR=AMD64 -DREXSDK_DIR=$SDK -DREXGLUE_ENABLE_TRACY=OFF -DREXGLUE_USE_VULKAN=ON -DREXGLUE_USE_D3D12=OFF > /c/proj/gh/win-configure.log 2>&1 || { tail -30 /c/proj/gh/win-configure.log; exit 1; }
tail -12 /c/proj/gh/win-configure.log
[ "${1:-configure}" = build ] && cmake --build $B --parallel 12 > /c/proj/gh/win-build.log 2>&1 && echo BUILD_OK || { tail -30 /c/proj/gh/win-build.log; }
