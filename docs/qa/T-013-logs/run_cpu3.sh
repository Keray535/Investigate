#!/bin/bash
cd C:/Users/Keray/Desktop/Plugin_dev
for cfg in 1280x720@1.0 2560x1440@1.0; do
  build/tools/render-harness/Release/InvestigateMeterCheck.exe /tmp/t13 cpu $cfg 3 15 12 > docs/qa/T-013-logs/cpu144_${cfg}.log 2>&1
done
echo done > docs/qa/T-013-logs/cpu144_done.txt
