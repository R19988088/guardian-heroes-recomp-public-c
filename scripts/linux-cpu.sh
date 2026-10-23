#!/bin/bash
# Guest CPU cost on Linux with the null GPU: plays the first stage with key presses, then samples the
# per-thread CPU time of the process for 20 s. An x86 core is several times faster than a Switch A57:
# read the numbers as a lower bound. Output: top threads by CPU%.
#   scripts/linux-cpu.sh [start_s=150] [extra gh args...]
set -u
START=${1:-150}; shift || true
ROOT=${GH_ROOT:-/root/gh}
EXE=$ROOT/src/out/build/linux-amd64-release/gh
export SDL_AUDIODRIVER=dummy LD_LIBRARY_PATH=$ROOT/rexglue-sdk/out/linux-amd64 DISPLAY=:79
Xvfb :79 -screen 0 1280x720x24 >/dev/null 2>&1 &
XVFB=$!
sleep 1
cd "$(dirname "$EXE")"
"$EXE" --game_data_root="$ROOT/game" --gpu_plugin=xenos --gpu_backend=null --fullscreen=false --audio_mute=true --mnk_mode=true \
    --license_mask=1 --headless=true --log_file=$ROOT/runs/cpu.log "$@" >$ROOT/runs/cpu.out 2>&1 &
GAME=$!
for ((t = 1; t <= START + 25; t++)); do
    sleep 1
    kill -0 $GAME 2>/dev/null || { echo "exited at $t"; break; }
    if (( t >= 12 && (t - 12) % 8 == 0 && t < START )); then
        WID=$(xdotool search --onlyvisible --class gh 2>/dev/null | head -1)
        xdotool mousemove 640 360 windowfocus "$WID" 2>/dev/null; xdotool keydown space; sleep 0.3; xdotool keyup space
    fi
    if (( t == START )); then
        for d in /proc/$GAME/task/*; do echo "$(basename $d) $(cut -d' ' -f14,15 $d/stat | awk '{print $1+$2}') $(cat $d/comm)"; done > $ROOT/runs/cpu0.txt
    fi
done
for d in /proc/$GAME/task/*; do echo "$(basename $d) $(cut -d' ' -f14,15 $d/stat | awk '{print $1+$2}') $(cat $d/comm)"; done > $ROOT/runs/cpu1.txt
HZ=$(getconf CLK_TCK)
join <(sort $ROOT/runs/cpu0.txt) <(sort $ROOT/runs/cpu1.txt) | awk -v hz=$HZ '{d=$4-$2; if (d>0) printf "%6.1f%%  %s\n", d*100/hz/25, $3}' | sort -rn | head -12
grep -E "swap|fps" $ROOT/runs/cpu.out | tail -2
kill $GAME 2>/dev/null; sleep 1; kill -9 $GAME 2>/dev/null; kill $XVFB 2>/dev/null
