#!/bin/bash
cd /c/Users/Keray/Desktop/Plugin_dev
EXE=build/tools/render-harness/Release/InvestigateMeterCheck.exe; D=docs/qa/T-013-logs/r2
for sz in 1280x720 2560x1440; do
  for i in 1 2 3; do METER_FORCE_HZ=60 $EXE $D paint $sz 12 2>&1 | grep -E "PAINT-PASSES.*frames" >> $D/paint_60hz.log; done
  for i in 1 2 3; do $EXE $D paint $sz 12 2>&1 | grep -E "PAINT-PASSES.*frames" >> $D/paint_144hz.log; done
done
for cfg in 1280x720@1.0 2560x1440@1.0 1920x1080@1.1; do
  METER_FORCE_HZ=60 $EXE $D cpu $cfg 3 15 12 > $D/cpu60_$cfg.log 2>&1
  $EXE $D cpu $cfg 3 15 12 > $D/cpu144_$cfg.log 2>&1
done
echo done > $D/run_done
