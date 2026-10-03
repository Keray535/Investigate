#!/bin/bash
# T-013 whole-editor CPU matrix: forced 60 Hz (the T-029 condition), 3 runs interleaved over the 4 cases, 12 s per measurement.
cd C:/Users/Keray/Desktop/Plugin_dev
for cfg in 1280x720@1.0 2560x1440@1.0 1920x1080@1.1 1920x1080@1.25 1920x1080@1.5; do
  METER_FORCE_HZ=60 build/tools/render-harness/Release/InvestigateMeterCheck.exe /tmp/t13 cpu $cfg 3 15 12 > docs/qa/T-013-logs/cpu60_${cfg}.log 2>&1
done
echo ALLDONE > docs/qa/T-013-logs/cpu60_done.txt
