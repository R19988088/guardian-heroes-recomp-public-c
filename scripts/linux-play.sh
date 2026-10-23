#!/bin/bash
# Like linux-run.sh but presses keys through xdotool on a schedule and screenshots every 5 s.
#   scripts/linux-play.sh <seconds> "<t>:<key>[,<t>:<key>...]" [extra gh args...]   e.g. 90 "45:space,60:Return"
# Needs --mnk_mode=true (added here): keyboard = pad (SDK defaults: ; or Space = A, ' or BackSpace = B,
# X or Return = Start, Z or Tab = Back, WASD = left stick, Shift+arrows = d-pad).
set -u
SECS=${1:-90}; KEYS=${2:-}; shift 2 || true
ROOT=${GH_ROOT:-/root/gh}
EXE=${GH_EXE:-$ROOT/src/out/build/linux-amd64-release/gh}
OUT=$ROOT/runs/$(date +%Y%m%d-%H%M%S)
mkdir -p "$OUT"
export SDL_AUDIODRIVER=dummy LD_LIBRARY_PATH=$ROOT/rexglue-sdk/out/linux-amd64 VK_ICD_FILENAMES=${VK_ICD_FILENAMES:-/usr/share/vulkan/icd.d/lvp_icd.json} DISPLAY=:78
Xvfb :78 -screen 0 1280x720x24 >/dev/null 2>&1 &
XVFB=$!
sleep 1
cd "$(dirname "$EXE")"
${GH_GDB:+gdb -batch -ex "set pagination off" -ex run -ex "bt 40" --args} "$EXE" --game_data_root="$ROOT/game" --gpu_plugin=xenos --fullscreen=false --audio_mute=true --mnk_mode=true \
    --log_file="$OUT/gh.log" --log_level=info "$@" >"$OUT/stdout.txt" 2>&1 &
GAME=$!
echo "gh pid $GAME, output $OUT"
declare -A K
IFS=',' read -ra PAIRS <<< "$KEYS"
for p in "${PAIRS[@]}"; do [ -n "$p" ] && K[${p%%:*}]="${K[${p%%:*}]:-} ${p#*:}"; done
for ((t = 1; t <= SECS; t++)); do
    sleep 1
    kill -0 $GAME 2>/dev/null || { echo "gh exited at ${t}s"; break; }
    if [ -n "${K[$t]:-}" ]; then
        for k in ${K[$t]}; do
            WID=$(xdotool search --onlyvisible --class gh 2>/dev/null | head -1)
            [ -z "$WID" ] && WID=$(xdotool search --onlyvisible --name "" 2>/dev/null | head -1)
            xdotool mousemove 640 360 windowfocus "$WID" 2>/dev/null; xdotool keydown $k; sleep ${GH_KEYHOLD:-0.2}; xdotool keyup $k
            echo "t=$t key $k (window $WID)"
        done
    fi
    (( t % 5 == 0 )) && import -window root "$OUT/shot-$(printf %03d $t).png" 2>/dev/null
done
[ -z "${GH_GDB:-}" ] && { kill $GAME 2>/dev/null; sleep 1; }; kill -9 $GAME 2>/dev/null; kill $XVFB 2>/dev/null
echo "--- tail"; tail -5 "$OUT/gh.log" 2>/dev/null
ls "$OUT" | wc -l; echo "$OUT"
