#!/bin/bash
# Linux debug build in WSL: /root/gh/{src,rexglue-sdk,game}. Steps: sync, SDK edits, CLI (templates), codegen, configure, build.
#   tools/wsl-linux.sh [codegen|build]   (default: both)
set -e
R=/root/gh; SDK=$R/rexglue-sdk; SRC=$R/src; CLI=$SDK/out/linux-amd64/Release/rexglue
rsync -a --delete --exclude /game --exclude /generated/default --exclude /out --exclude /.git /mnt/d/git/dev/gh-recomp/ $SRC/
[ -f $SRC/game/default.xex ] || cp /mnt/d/git/dev/gh-recomp/game/pkg/default.xex $SRC/game/
python3 $SRC/switch/sdk-edits.py $SDK > $R/sdk-edits.log 2>&1
python3 $SRC/patches/sdk-edits-gh.py $SDK >> $R/sdk-edits.log 2>&1
cmake --build $SDK/out/build/linux-amd64 --config Release --target rexglue --parallel 12 > $R/cli-build.log 2>&1
cd $SRC
if [ "${1:-all}" != build ]; then
  $CLI codegen gh_manifest.toml ${GH_FORCE_CODEGEN:+--ignore-stamp} > $R/codegen.log 2>&1 || { tail -20 $R/codegen.log; exit 1; }
  tail -4 $R/codegen.log
fi
[ "${1:-all}" = codegen ] && exit 0
cmake --preset linux-amd64-release -DREXSDK_DIR=$SDK > $R/game-configure.log 2>&1 || { tail -20 $R/game-configure.log; exit 1; }
cmake --build out/build/linux-amd64-release --parallel 12 > $R/game-build.log 2>&1 || { grep -E "error|Error" $R/game-build.log | head -20; exit 1; }
ls -la out/build/linux-amd64-release/gh
