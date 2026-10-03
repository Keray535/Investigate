#!/bin/bash
cd C:/Users/Keray/Desktop/Plugin_dev
for cfg in 1920x1080@1.1 1920x1080@1.25; do
  METER_FORCE_HZ=60 build/tools/render-harness/Release/InvestigateMeterCheck.exe /tmp/t13 cpu $cfg 5 15 12 > docs/qa/T-013-logs/cpu60_${cfg}.log 2>&1
done
echo done > docs/qa/T-013-logs/cpu60b_done.txt
