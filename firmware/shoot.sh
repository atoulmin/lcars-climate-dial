#!/bin/bash
# Run the simulator demo and capture frames at fixed times: shoot.sh <outdir> t1 t2 ...
cd "$(dirname "$0")"
out=$1; shift; mkdir -p "$out"
SIM=${SIM:-lcars-aircon-sim}
P=.esphome/build/$SIM/.pioenvs/$SIM/program
env ${DEMO_ENV:-LCARS_DEMO=1} $P > sim-run.log 2>&1 &
pid=$!
start=$(date +%s.%N)
until hyprctl clients -j | grep -q "\"$SIM\""; do :; done
G=$(hyprctl clients -j | python3 -c "
import json,sys
for c in json.load(sys.stdin):
  if c['title']=='$SIM': print(f\"{c['at'][0]},{c['at'][1]} {c['size'][0]}x{c['size'][1]}\")")
for t in "$@"; do
  while python3 -c "import sys,time; sys.exit(0 if time.time()-$start < $t else 1)"; do :; done
  grim -g "$G" "$out/raw_$t.png"
  magick "$out/raw_$t.png" -resize 240x240! -fill none -stroke '#303030' -strokewidth 64 -draw 'circle 119.5,119.5 119.5,-32' "$out/t$t.png"
done
kill $pid
cd "$out" && magick $(ls t*.png | sort -V) +append -scale 150% strip.png
