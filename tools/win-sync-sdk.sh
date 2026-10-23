#!/bin/bash
# WSL: copies the edited SDK and the generated code for the Windows build (tools/win-configure.sh, MSYS2).
set -e
rsync -aL --delete --exclude /out --exclude /.git --exclude /thirdparty/moltenvk /root/gh/rexglue-sdk/ /mnt/c/proj/gh/sdk/
sed -i 's|^set(REX_TARGET_PROCESSOR "${CMAKE_SYSTEM_PROCESSOR}")|set(REX_TARGET_PROCESSOR "${CMAKE_SYSTEM_PROCESSOR}")\nif(NOT REX_TARGET_PROCESSOR AND WIN32)\n    set(REX_TARGET_PROCESSOR "AMD64")  # gh: MSYS2 leaves it empty\nendif()|' /mnt/c/proj/gh/sdk/CMakeLists.txt
rm -rf /mnt/d/git/dev/gh-recomp/generated/default
mkdir -p /mnt/d/git/dev/gh-recomp/generated/default
cp -r /root/gh/src/generated/default/. /mnt/d/git/dev/gh-recomp/generated/default/
echo synced
