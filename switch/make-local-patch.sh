#!/bin/sh
# switch/cvhd-local.patch from the committed tree: sync without the patch, diff against HEAD, restore.
set -e
set -o pipefail 2>/dev/null || true
cd "$(dirname "$0")/.."
# It resets switch/ src/ tools/ to HEAD at the end: uncommitted work there would be lost (it was, once).
if [ -n "$(git status --porcelain -- switch src tools | grep -v cvhd-local.patch)" ]; then echo "commit your changes in switch/ src/ tools/ first" >&2; exit 1; fi
cp switch/cvhd-local.patch "${TMPDIR:-/tmp}/old.patch"
: > switch/cvhd-local.patch
sh switch/sync-cvhd.sh > /dev/null
git diff -R HEAD -- switch/host switch/sdk-edits.py switch/eden-shot.ps1 src/gpu tools/xenos2glsl.py tools/nro_nacp.py > "${TMPDIR:-/tmp}/new.patch"
git checkout -q HEAD -- switch src tools
cp "${TMPDIR:-/tmp}/new.patch" switch/cvhd-local.patch
wc -l switch/cvhd-local.patch
