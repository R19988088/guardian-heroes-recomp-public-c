#!/bin/bash
# Builds the host rexglue CLI from the patched SDK copy (WSL). Layout: /root/gh/{rexglue-sdk,src,game}.
set -e
SDK=/root/gh/rexglue-sdk
cd $SDK
for p in /mnt/d/git/dev/gh-recomp/patches/sdk/*.patch; do
    git apply --check "$p" 2>/dev/null && git apply "$p" && echo "applied $(basename $p)" || echo "skip $(basename $p)"
done
cmake --preset linux-amd64 -DCMAKE_C_COMPILER=clang-20 -DCMAKE_CXX_COMPILER=clang++-20 > /root/gh/cli-configure.log 2>&1
cmake --build out/build/linux-amd64 --config Release --target rexglue --parallel 12 > /root/gh/cli-build.log 2>&1
ls -la out/linux-amd64/Release/rexglue
