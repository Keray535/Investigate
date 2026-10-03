# T-013 validation, round 1 (plugin-validator)

Verdict: **FAIL (3 defects)**. D-1 and D-2 are functional deviations from the spec (small). D-3 is the paint-cost criterion, missed by a hair at 2560x1440 (p95 0.99 to 1.09 ms in quiet runs, up to 1.5 ms under background load). Everything else passes: processor-driven readouts, all four meter scales, mono and collapsed layouts, the CPU matrix (worst median 20.0 % against 25 %), the 92 spectrum checks, the spectrum screenshots, pluginval 1.0.4 at strictness 10 and the Standalone launch.

## Environment
- Windows 11 Home 10.0.26200, i7-12700H, Intel Iris Xe (primary display) + RTX 3060 Laptop, one 1920x1080 display at 100 %, on AC power. MSVC 2022, Release, commit 2828d7a plus the uncommitted T-013 working tree (`MeterPanel`, `MeterMath.h`, `SpectrumView` changes). Rebuilt before measuring; the binaries are newer than every source file.
- Real on-screen Direct2D windows, JUCE default renderer, EcoQoS off, high priority, 1 ms timer period. Audio runs on its own thread, paced in real time (512-sample blocks at 48 kHz) through `processBlock`.
- **Display refresh is 144 Hz here (Windows dynamic refresh rate, DRR).** The editor polls on the display vblank, so the meter repaint rate follows the display. T-029 was measured at a 60 Hz vblank. For comparable numbers the CPU and paint gates below were measured with the display forced to 60 Hz for the lifetime of the test process only (`METER_FORCE_HZ=60`, `ChangeDisplaySettingsEx` with `CDS_FULLSCREEN`; Windows restores the mode when the process exits, verified: the display reads 144 Hz again afterwards). The same CPU matrix was also run at the native 144 Hz for 1280x720 and 2560x1440 (section 6).
- 2560x1440 caveat (as in T-029): the window is larger than the 1920x1080 display. To keep the meter strip on screen, this round shifts the window left (top-left x = -640), so the left 640 px of the spectrum are off screen (T-029 had the right side off screen). Treat 2560x1440 numbers as provisional until confirmed on a native 1440p display.

## Harness
`tools/render-harness/meter_check.cpp`, target `InvestigateMeterCheck` (mine). It uses only public surfaces: `createEditor`, the APVTS, `processBlock` on a separate paced thread, the `MeterPanel` readout accessors, and (CPU mode only, as T-029 did) the `SpectrumView::setFrameForTesting` hook so the spectrum animates on every vblank. The panel is never driven by the tool; the editor's own `pollProcessor` path updates it.

Changes this round:
- `paint` mode rewritten. The old pair-of-probes method mispaired passes at 2560x1440 (median 0.006 ms artefact). It now uses JUCE's own `ComponentListener::componentPainted` diagnostics (`totalPaintDuration` of the `MeterPanel`, one sample per paint pass; passes under 3 ms apart are summed into one vblank). The old method is kept as `paintpair`.
- `shots` mode gained the collapsed-state screenshot (clicks the header Meters toggle).
- `METER_FORCE_HZ`, a display-rate line (`DISPLAY ... at N Hz`) and window placement for windows wider than the display.
- Paint passes are split by "a text-cell repaint was requested before this pass" (`PAINT-SPLIT`).

## Commands
```
cmake --build build --config Release --target InvestigateMeterCheck InvestigateSpectrumCheck Investigate_VST3 Investigate_Standalone
build/tools/render-harness/Release/InvestigateMeterCheck.exe <dir> func                 # processor-driven readouts, 91 checks
build/tools/render-harness/Release/InvestigateMeterCheck.exe docs/qa/T-013-screenshots shots
METER_FORCE_HZ=60 build/tools/render-harness/Release/InvestigateMeterCheck.exe <dir> paint 1280x720 12   # also 2560x1440
METER_FORCE_HZ=60 build/tools/render-harness/Release/InvestigateMeterCheck.exe <dir> cpu 1280x720@1.0 3 15 12   # WxH@scale runs caseMask seconds (also 2560x1440@1.0, 1920x1080@1.1 / 1.25 / 1.5)
build/tools/render-harness/Release/InvestigateMeterCheck.exe <dir> cycle 50
build/tools/render-harness/Release/InvestigateSpectrumCheck.exe <dir> 100               # 92 checks + screenshots (stopped when the benchmark section started)
python docs/qa/T-029-logs/cmp.py <Temp\ref> <curDir> 48 56 888 620
third_party/pluginval/1.0.4/pluginval.exe --strictness-level 10 --validate build/Investigate_artefacts/Release/VST3/Investigate.vst3 --output-dir <dir>
```
Logs: `docs/qa/T-013-logs/` (`func_9.log`, `paint_60hz.log`, `paint_mixed_hz.log`, `cpu60_*.log`, `cpu144_*.log`, `first_pass/` for the noisier first 1.1 and 1.25 runs, `spectrum_full.log`, `screens_vs_ref.txt`, `pluginval.txt`, `standalone_smoke.txt`, `cycle.log`, `run_cpu*.sh`). The earlier logs `func_1..8.log` were runs of older code and are removed; `func_9.log` is the run on the rebuilt tree.

## Results

| # | Check | Result |
|---|---|---|
| 1 | -6 dBFS 1 kHz sine, dBFS mode: peak -6.0, RMS -9.0 | PASS (L and R: peak -6.0, peak max -6.0, RMS -9.0, crest 3.0) |
| 2 | Stereo -23 dBFS sine: M, S, I = -23.0 LUFS after 3.5 s | PASS with a note (O-1): M -23.0, S -23.0 from a fresh start; I -23.0 when measured from a steady signal; from a cold start I reads -23.1 or -23.2 |
| 3 | K scales: -6 dBFS sine reads +14.0 / +8.0 / +6.0 (K-20 / K-14 / K-12); -20 dBFS sine reads 0.0 / -6.0 / -8.0 K and -23.0 in dBFS; peak cells unchanged by scale | PASS (8 checks) |
| 4 | Clip lamps and counts equal the snapshot | PASS (15 sample clips and 6 TP clips on L, R stays 0; lamp latched 3.5 s later) |
| 5 | Statistics reset clears lamps, counts and all max values within one UI frame | **FAIL**, D-1 (lamps clear in 2 to 6 ms; counts, maxima, I and LRA text clear 95 to 102 ms after the snapshot clears) |
| 6 | Headroom = 0 dBTP minus TP max | PASS (5.99 for -5.99 dBTP; -1.6 for a +1.61 dBTP sine, shown with U+2212) |
| 7 | Balance: L -6 dBFS, R -12 dBFS after 3.5 s | PASS, "L 6.0 dB" (snapshot -6.000); swapped "R 6.0 dB"; equal "C 0.0 dB" |
| 8 | Correlation +1 / -1 / about 0 | PASS: +1.000 ("+1.00"), -1.000 ("-1.00"), independent noise 0.018 ("+0.01"). Step +1 to -1: crosses zero 303 ms later, reaches -0.99 after 655 ms (500 ms DSP window + 50 ms display filter) |
| 9 | Ballistics through the processor | PASS: peak bar falls 20.0 dB/s; hold tick holds 1.3 s and falls 29.8 to 30.1 dB/s after 1.5 s; never below the bar |
| 10 | Stale rule and recovery (ui-spec 8.7) | PASS: stale flag 258 to 265 ms after the last block (rule 250 ms, vblank granularity); RMS, M, S read -inf; peak max, TP max, headroom, clips, I, LRA, M max, S max, correlation, balance hold; recovery 8.8 to 13 ms; silence with blocks running: bars fall, text -inf, maxima hold, not stale |
| 11 | Mono layout | PASS in data, with a label defect D-2: one bar, correlation and balance show dashes, left column blank, channel values on the right column (peak -6.0, RMS -9.0), no NaN. The bar label reads "MON" |
| 12 | Screenshots of dBFS, K-20, K-14, K-12, mono, collapsed | done, section 2 |
| 13 | MeterPanel paint p95 <= 1 ms, 1280x720 and 2560x1440 | **FAIL, marginal**, D-3 |
| 14 | Whole-editor CPU <= 25 %, 5 configurations | PASS, section 5 (worst median 20.0 %, worst single run 22.2 %) |
| 15 | 92 spectrum checks; spectrum screenshots match the T-029 set | PASS, section 7 |
| 16 | pluginval 1.0.4, strictness 10, GUI tests on | PASS |
| 17 | Standalone builds and launches | PASS |
| 18 | 50 open/close editor cycles with meters animating | PASS, no crash (`cycle.log`) |

`func_9.log` has 91 checks, 89 pass; the 2 failures are both D-1 (one is the single-reset check, the other the 12-repeat check).

## 2. Screenshots (`docs/qa/T-013-screenshots/`)
Full editor 1200x700 and a 2x crop of the meter strip for each (`*-strip2x.png`), music-like programme, bars and numbers live.

| Image | Content |
|---|---|
| `01-dbfs.png`, `01-dbfs-strip2x.png` | dBFS: ticks every 6 dB from +6 to -60, zones at -12 and -3, RMS unreferenced |
| `02-k20-strip2x.png` | K-20: ticks every 4 from +24 to -40, zones at 0 and +4 K, unit label "K-20" |
| `03-k14-strip2x.png` | K-14: ticks +20 to -44 |
| `04-k12-strip2x.png` | K-12: ticks +16 to -48 |
| `05-clipped-strip2x.png` | clip lamp lit on L, counts as red lamp cells (9 and 5), negative headroom in red |
| `06-stale-450ms-strip2x.png`, `06b-stale-4s-strip2x.png` | stale: bars falling, RMS and M/S -inf, maxima held |
| `07-min-920x540-strip2x.png` | minimum size |
| `08-1500x1000-strip2x.png` | larger size (growth rule) |
| `09-scale-200-strip2x.png`, `10-scale-110-strip2x.png` | 200 % and 110 % global scale |
| `11-mono-strip2x.png` | mono layout: one bar, "MONO" column header, correlation block dimmed with dashes |
| `12-collapsed.png` | collapsed: panel hidden (`isVisible()` = 0), plot takes the full width, toggle shows the mirrored icon; expanding again restores it |

What I checked by eye: the bar geometry is the same (-60 to +6 dBFS) in every mode; the zone colours move with the scale (K modes switch to warm at 0 K and hot at +4 K); the RMS cell shows the K value; peak/TP cells and the peak max cells stay in dBFS; the numbers in the strip agree with the table format (signed 1 decimal, U+2212, "-inf" and dashes). Defect D-2 is visible in `11-mono-strip2x.png` (label "MON").

## 3. Paint cost (MeterPanel, real Direct2D window, meters animating, display forced to 60 Hz)
Method: JUCE paint diagnostics (`totalPaintDuration` of the panel per paint pass, recording time on the message thread; the GPU present is outside it). 12 s per run after a 2 s warm-up, audio running (`stereoSidechain` layout, music with clips), 5 quiet runs per size. p95 over the vblank frames in which the panel painted.

| Size | Frames with a panel paint | Median ms | p95 ms, 5 runs | Median of the p95s | p99 | Worst |
|---|---|---|---|---|---|---|
| 1280x720 (panel 296x676) | about 56/s | 0.55 to 0.60 | 0.886, 0.822, 0.802, 0.798, 0.808 | **0.81** | 0.91 to 1.03 | 1.04 to 1.32 |
| 2560x1440 (panel 296x1396) | 30/s (paced with the spectrum) | 0.75 to 0.78 | 1.086, 1.009, 0.998, 1.001, 0.985 | **1.001** | 1.07 to 1.18 | 1.11 to 1.35 |

Limit: p95 <= 1 ms. 1280x720 passes with a 0.11 to 0.20 ms margin. 2560x1440 is at the limit: 3 of 5 runs are above 1.0 and the median is 1.001. Runs with background activity on the machine (earlier in the session) gave p95 1.0 to 1.5 ms at 1280x720 and 1.3 to 1.5 ms at 2560x1440, so the margin does not cover a loaded desktop. The text-refresh passes are not the cause: passes after a text-cell repaint request cost about the same as the others (1280x720: median 0.57 vs 0.56 ms; 2560x1440: 1.00 vs 0.83 ms), so the cost is in the bar and correlation paths, which scale with the panel height. At the native 144 Hz display the p95 values were 0.76 to 1.07 ms (1280x720) and 1.06 to 1.26 ms (2560x1440), `paint_mixed_hz.log`. The gate is not met at 2560x1440 as written: D-3.

## 4. Effective repaint rates (display forced to 60 Hz, CPU runs)
| Config | Case | Meter bar repaint requests/s (2 bars + correlation) | Text-cell repaints/s | Spectrum data-rect paints/s |
|---|---|---|---|---|
| 1280x720 | (a), (b), (d) | 133 to 136 | 66 to 71 | 55 to 57 |
| 1280x720 | (c) paced | 55 | 67 | 32 |
| 2560x1440 | (a) to (d), all paced | 54 to 55 | 64 to 72 | 32 |
| 1920x1080 @ 110 % | (a), (b), (d) / (c) | 143 / 70 | 67 to 70 | 60 / 30 |
| 1920x1080 @ 125 % | (a), (b), (d) / (c) | 146 / 56 | 66 to 71 | 59 / 30 |
| 1920x1080 @ 150 % | (a), (b), (d) / (c) | 138 / 56 | 65 to 71 | 55 to 56 / 31 |

Text cells are rebuilt at 10 Hz as the spec requires (the count is the number of changed cells repainted per second, about 7 cells per refresh). When the spectrum is paced to 30 fps, the meters follow it (rule 5), at about half the bar repaint rate.

## 5. Whole-editor message-thread CPU (display forced to 60 Hz)
T-029 method: thread cycle time over 12 s wall, a new synthetic 2048-point frame on every vblank, net of the same paced loop without audio or frames (baseline 4 to 7 %). Meters driven by the editor's own path with audio running. Net CPU, % of one core. Cases as in T-029: (a) single + RT max, (b) dual + 2 secondaries, (c) dual + 2 secondaries + SC + hover, (d) single + all-time average + SC. Median of 3 runs (5 runs at 110 % and 125 %, see below); in brackets min and max.

| Config | (a) | (b) | (c) | (d) | Worst median | Gate |
|---|---|---|---|---|---|---|
| 1280x720, 100 % | 7.0 (7.0 to 7.4) | 10.1 (9.9 to 10.3) | 10.8 (10.8 to 11.2) | 9.6 (9.2 to 10.5) | **10.8** | <= 25 PASS |
| 2560x1440, 100 % | 5.5 (5.0 to 5.9) | 7.4 (6.3 to 7.6) | 11.9 (10.8 to 11.9) | 6.9 (5.9 to 7.4) | **11.9** | <= 25 PASS |
| 1920x1080, 110 % (5 runs) | 12.7 (7.6 to 14.8) | 20.0 (10.9 to 22.2) | 15.9 (9.6 to 16.7) | 15.1 (9.4 to 17.6) | **20.0** | <= 25 PASS |
| 1920x1080, 125 % (5 runs) | 6.8 (6.6 to 7.0) | 10.8 (9.8 to 16.3) | 10.8 (10.6 to 12.5) | 9.1 (8.6 to 12.2) | **10.8** | <= 25 PASS |
| 1920x1080, 150 % | 9.4 (8.9 to 10.4) | 13.4 (12.8 to 14.8) | 13.2 (11.8 to 13.6) | 11.7 (11.1 to 11.8) | **13.4** | <= 25 PASS |

The worst single value anywhere in the final runs is 22.2 % (110 %, case b, run 1), also below 25. The 110 % runs drift downward from run 1 to run 5 (case (b): 22.2, 20.0, 20.6, 12.6, 10.9) while the baseline rises (4.1 to 7.2 %); that is machine state drift (clocks), not a code effect, but it means the 110 % figure of 20.0 is the less certain one. The first 3-run pass at 110 % and 125 % (kept in `first_pass/`) had two outliers, 30.4 % (110 %, case b) and 26.5 % (110 %, case d), above the gate; its medians were 23.1 % and 21.6 % (cases c). I re-ran both configurations with 5 runs on a quieter machine to get the numbers above. Fractional scales at 1920x1080 are the known elevated case from T-029 (O-A); 110 % remains the highest (about +5 to +10 points over 125 %), and its spread is large.

Gate verdict: PASS at all five configurations (median of runs; 110 % has 5 points of margin).

## 6. Native 144 Hz display (information; DRR display, nothing forced)
3 runs, net CPU %, median (min to max). The meter repaint rate follows the display: bar repaint requests rose to about 150/s (vs 136) at 1280x720.

| Config | (a) | (b) | (c) | (d) | Worst median |
|---|---|---|---|---|---|
| 1280x720 | 8.3 (8.3 to 9.2) | 12.1 (12.1 to 22.6) | 13.0 (12.9 to 13.3) | 11.1 (10.9 to 20.6) | 13.0 |
| 2560x1440 | 6.6 (6.4 to 6.8) | 9.3 (9.0 to 9.7) | 16.1 (15.8 to 16.9) | 8.6 (8.5 to 8.7) | 16.1 |

Within the gate, but 2 to 4 points above the 60 Hz numbers. The ui-spec 8.1 line "read the snapshot on every vblank (<= 60 Hz, at least 30 Hz)" reads as a cap; nothing in the editor limits the panel to 60 Hz on a 144 Hz display (observed up to 140 panel paints/s in one run, 64 to 106 in others as DRR changes the real rate). See O-2.

## 7. Regression (T-012 and T-029)
- `InvestigateSpectrumCheck <dir> 100`: **92 of 92 checks pass**, 0 failures (`spectrum_full.log`; stopped when the benchmark section started, as in T-029).
- Screenshots compared with the T-012 reference regenerated for T-029 (`Temp\ref`, bit-identical to what T-029 used), over the data rect (x 48..888, y 56..620), `screens_vs_ref.txt`: the statistics are the same as the T-029 report to the digit (images 01 to 07: 95.4 to 96.2 % identical, 3.6 to 4.4 % at 1 level, 0.23 to 0.27 % at 2 to 16 levels, 10 to 24 pixels above 16 levels, max 93; sidechain images 08, 10, 11: 296 to 304 pixels above 16 levels, max 43 to 60; hover images 12 to 15: 211 to 288 pixels; 16 max 149 and 18 max 34; 17 and `20-synth-3` bit-identical; `20-synth-5` max 8; `placement-1khz` max 29). So the spectrum output is unchanged since T-029. The O-1 seam fix did not change any of these full-paint images (the seam concerns clip-limited repaints, covered by the now un-hidden unit test). The `bench-software-dual-1280.png` image in the set is a mid-run benchmark frame (I stopped the run) and is not comparable.

## 8. pluginval and Standalone
- pluginval 1.0.4, strictness 10, GUI tests on, `build/Investigate_artefacts/Release/VST3/Investigate.vst3` (rebuilt, newer than all sources): exit 0, "SUCCESS", 16 s, no failures (`pluginval.txt`).
- Standalone `build/Investigate_artefacts/Release/Standalone/Investigate.exe`: starts, main window "Investigate" present after 6 s (142 MB working set), closes on request and exits with code 0 (`standalone_smoke.txt`). The user's hands-on review can proceed.

## Defects
**D-1 (functional, small). Reset clears the numeric readouts up to 100 ms after the lamps.** Criterion: "The statistics reset clears them and all max values within 1 UI frame of the reset taking effect."
- Expected: after the snapshot shows the reset, lamps, clip counts, peak max, TP max, headroom, M max, S max, I and LRA are all cleared within one UI frame (16.7 ms).
- Actual: the lamps clear in 2 to 6 ms (median 3.5, worst 6.6 over 12 repeats) but every text cell (counts, maxima, headroom, loudness) shows the old value until the next 10 Hz text refresh: 4 of 12 repeats took 95 to 98.5 ms, the other 8 took 2.5 to 6.6 ms (the text tick happened to fall just after the reset). Single run: lamps 5.5 ms, everything else 96.6 ms; click to panel fully cleared 105 ms.
- Cause (from the behaviour, not read from the code): the text cells are rebuilt only on the 10 Hz tick (ui-spec 8.1) and the reset does not force a refresh. `source/gui/MeterPanel.cpp`, `resetHeldValues()` (around line 219) and the Reset button handler (around line 71).
- Suggested fix: on `resetHeldValues()` (and when `update()` sees the statistics clear) force the next `update()` to refresh all text cells. This is a justified exception to the 10 Hz rule: one extra refresh per user action.
- Repro: `build/tools/render-harness/Release/InvestigateMeterCheck.exe <dir> func`, look for the two `[FAIL]` lines under "reset (Reset button)". Log: `docs/qa/T-013-logs/func_9.log` lines 83 and 88.

**D-2 (cosmetic). The mono bar label is truncated to "MON".** ui-spec 4.4 and 8.6: "labelled MONO".
- Actual: `docs/qa/T-013-screenshots/11-mono-strip2x.png` shows "MON" (the last letter is cut). The label rect is `barW + 2 * letterPad` = 30 px wide (`source/gui/MeterPanel.cpp` line 571, constants in `DesignTokens.h` line 278), which is narrower than "MONO" in the `caps` style.
- Suggested fix: give the mono label a wider rect (the bars column is 80 px wide and the bar is centred at x 44 to 66, so there is room for about 60 px).
- Repro: `InvestigateMeterCheck.exe docs/qa/T-013-screenshots shots`, open image 11.

**D-3 (performance criterion, marginal). MeterPanel paint p95 is not <= 1 ms with margin at 2560x1440.** Criterion: "MeterPanel paint adds <= 1 ms p95 at 1280x720 and 2560x1440."
- Actual: 2560x1440: p95 1.086, 1.009, 0.998, 1.001, 0.985 ms in five quiet 12 s runs (median of the p95s 1.001), up to 1.3 to 1.5 ms on a loaded machine. 1280x720: 0.80 to 0.89 ms quiet (passes), up to 1.5 ms loaded. Section 3.
- The cost scales with panel height (2560x1440 has a 1396 px tall panel) and is not caused by the 10 Hz text refresh. Candidates are the number of rectangle batches for the bars and the bar and correlation repaint rectangles. I did not read the code to find a cause.
- Repro: `METER_FORCE_HZ=60 build/tools/render-harness/Release/InvestigateMeterCheck.exe <dir> paint 2560x1440 12`, read the `PAINT-PASSES ... per frame ms ... p95` figure. Logs: `paint_60hz.log`.
- If the architect considers 1.0 ms a target with measurement noise rather than a hard bound, D-3 can be waived; I report it as not met because the median of the p95s is above 1.0 and I do not loosen criteria.

## Observations (no defect raised)
- **O-1, integrated loudness from a cold start.** A stereo -23 dBFS sine started on a fresh processor reads M -23.0 and S -23.0 after 3.5 s, but I -23.1 or -23.2 and LRA 0.8 to 1.1 LU, because the gated blocks that straddle the sine onset are included (the harness feeds a few silent blocks before the sine). With the signal steady and the statistics reset (the harness (B) protocol), I reads -23.0 and LRA 0.0. The acceptance line "M, S and I of -23.0 LUFS after 3 s" is therefore met only for a measurement that starts on a steady signal. The architect should confirm that this reading of the criterion is intended; if a cold start must also give -23.0 at 3 s, that is a DSP matter (dsp-engineer), not a panel one.
- **O-2, repaint rate on high-refresh displays.** The panel repaints on every vblank, so it runs at the display rate (144 Hz here), not at "<= 60 Hz" (ui-spec 8.1). CPU stays within the gate (section 6), but a 60 fps cap would restore the T-029 headroom. Architect decision.
- **O-3, M max after a reset in silence.** In one of two runs M max read "-153.7" (finite value from the DSP snapshot) instead of a dash after a reset in silence; the other run showed the dash. ui-spec 8.4 says "--" only for non-finite values, so the panel is consistent with the spec; the odd value originates in the loudness meter. Low priority, for the dsp-engineer.
- **O-4, 110 % scaling.** Remains the highest CPU configuration (section 5) with the largest spread, as T-029 saw. Within the gate.
- **O-5, 2560x1440 numbers.** Provisional (window larger than the display); this round the left part of the window is off screen instead of the right.

## Hands-on review: what to look at
- Click Reset while a clip lamp is lit and numbers are non-zero: the lamp goes out at once, the text may linger for up to 0.1 s (D-1).
- Mono input: the label under the single bar reads "MON" (D-2).
- Scale chip: switch dBFS, K-20, K-14, K-12 with music playing; the bar geometry must not jump, only zones and tick labels move; feed a -20 dBFS sine to see 0.0 / -6.0 / -8.0 on K-20 / K-14 / K-12.
- Hide the meters with the header toggle and show them again; the plot should use the whole width while hidden.
- Stop the transport (host not calling `processBlock`): after 0.25 s bars fall and RMS/M/S show "-inf" while maxima and counts hold.
- On a high-refresh display watch CPU in Task Manager while the meters run (O-2).

## Open questions
1. D-3: waive (noise-level miss, 0.001 ms on the median) or send back to gui-engineer for a cheaper bar/correlation path at tall panel heights?
2. O-1: is the "I = -23.0 after 3 s" criterion meant for a cold start?
3. O-2: cap the meter repaint at 60 Hz on faster displays?

## Files
- `C:\Users\Keray\Desktop\Plugin_dev\docs\qa\T-013-validation.md`
- `C:\Users\Keray\Desktop\Plugin_dev\docs\qa\T-013-screenshots\` (01 to 12, plus 2x strip crops)
- `C:\Users\Keray\Desktop\Plugin_dev\docs\qa\T-013-logs\`
- `C:\Users\Keray\Desktop\Plugin_dev\tools\render-harness\meter_check.cpp`


---

# Round 2 (targeted re-check of D-1, D-2, O-2, O-3, CPU, spectrum, pluginval)

Verdict: **PASS at the 60 Hz reference, with 4 findings that need an architect decision** (none is a confirmed production defect; F-1 and F-2 appear only on the 144 Hz display). D-1, D-2, O-2 and O-3 are fixed. Tree: round-2 fixes; plugin Release binary of 2026-10-03 02:38; harness rebuilt by me and warning-clean (the two C4996 `Displays::userArea` warnings are fixed, now `userBounds.toNearestInt()`).

Harness changes (`tools/render-harness/meter_check.cpp`): O-3 is now a hard check (M max and S max must show a silent form, not a number); `func` honours `METER_FORCE_HZ`; the "-inf" form counts as cleared in the reset timing; new modes `monoshots` (mono strip at several scales) and `rate` (vblank callbacks, panel and spectrum paint passes per second); the paint mode gate is 1.5 ms at heights >= 1400. Logs and run script: `docs/qa/T-013-logs/r2/`.

| # | Check | Result |
|---|---|---|
| 1 | D-1 reset timing | PASS at 60 Hz; note F-1 at 144 Hz |
| 2 | D-2 "MONO" label | PASS (100 %, 110 %, 125 %, 150 %) |
| 3 | O-2 update rate <= 60/s; spectrum pacing; T-031 pacing tests | PASS; note F-3 |
| 4 | O-3 M max / S max silent form | PASS; note F-4 |
| 5 | CPU worst cases, paint p95 | PASS at 60 Hz; 144 Hz 110 % case (b) over 25 % (F-2) |
| 6 | 92 spectrum checks | PASS (92 of 92, 0 failed) |
| 7 | pluginval strictness 10 | PASS (exit 0, SUCCESS) |

## R2.1 D-1 reset timing (`func_r2_60hz.log`, `func_r2_144hz*.log`)
Time from the snapshot showing the clear to the panel showing lamps, clip and TP-clip counts, peak max, TP max, headroom, M max, S max, I and LRA all cleared (12 repeats plus a single run; in the single run every group clears in the same update, 6.1 ms at 60 Hz).

| Display | Run | Median ms | Worst ms | Result |
|---|---|---|---|---|
| 60 Hz forced | 1 | 8.4 | **15.4** | all checks pass, 0 of 12 over one frame |
| 144 Hz native | 1 | 9.6 | 20.8 | 1 of 12 above 17 ms |
| 144 Hz native | 2 | n/r | 18.5 | 2 of 12 above 17 ms |
| 144 Hz native | 3 | n/r | below 17 | 0 of 12 above |

Round 1 was 95 to 102 ms. Click to panel cleared (single run): 8.8 ms at 60 Hz, 18.5 ms at 144 Hz.
F-1: at 144 Hz the 60/s limiter lands on whole vblanks (6.9 ms), so one update interval is 13.9 or 20.8 ms; the worst lag exceeds 16.7 ms by up to one vblank step in 2 of 3 runs. It is bounded by one update interval but strictly above "one 60 Hz frame". The 17 ms gate was not loosened, so the 144 Hz `func` runs FAIL the two "within 1 UI frame in every repeat" checks in 2 of 3 runs. The same effect moves the stale detector to 273 to 286 ms (rule 250 ms), which made one 144 Hz run fail "stale raised 250 to 285 ms" (285.7). Architect: accept as the cost of the 60/s cap on a 144 Hz display, or define the frame as one update interval.

## R2.2 D-2 mono label
`docs/qa/T-013-screenshots/r2-11-mono-100-strip3x.png`, `-110-`, `-125-`, `-150-` (3x crops of the strip, 1200x700 window, global scale 1.0, 1.1, 1.25, 1.5). Viewed 100, 110 and 150: the label under the single bar reads "MONO" in full, centred under the bar, nothing clipped. The correlation block stays dimmed with dashes.

## R2.3 O-2 update rate (`rate_60hz.log`, `rate_144hz.log`; live audio, 10 s, stereo+SC layout)

| Display | Size | vblank/s | MeterPanel paint passes/s | SpectrumView paint passes/s |
|---|---|---|---|---|
| 144 Hz | 1280x720 | 144.0 | **60.0** | 60.0 |
| 60 Hz | 1280x720 | 60.1 | 59.7 | 59.7 |
| 144 Hz | 2560x1440 | 144.1 | 24.0 | 24.0 |
| 60 Hz | 2560x1440 | 60.0 | 30.0 | 30.0 |

The panel is never painted more than 60 times per second (round 1: up to 140). Text-cell repaint requests stay at 71 to 72/s of changed cells (about 7 cells per 10 Hz refresh). Pacing tests: `ctest -R pacing` 6 of 6 pass (Release), including the T-031 window test.
F-3: at 144 Hz and 2560x1440 the paced spectrum runs at 24 fps instead of 30 (the 33 ms minimum interval is only met on the 20.8 ms update grid, so frames are 41.7 ms apart). 1280x720 is unaffected (60/s) and 60 Hz is unaffected (30/s). Architect: accept, or make the pacing grid-aware.

## R2.4 O-3 (M max / S max after a reset in silence)
60 Hz run: M max "−inf" and S max "−inf" with snapshot -148.7 / -161.7 LUFS (the round-1 "-153.7" case is now the silent form). 144 Hz runs: M max "−inf" (snapshots -146.1, -150.1) and S max "—" (dash) when the snapshot is exactly -inf.
F-4: the dash for a non-finite value follows ui-spec 8.4 (M max and S max: "—" when non-finite) and the round-2 note, so it is conformant, but the same silent state shows "−inf" in one cell and "—" in another depending on whether the DSP reports -inf or a tiny finite number. Architect: keep, or show "−inf" for both.

## R2.5 CPU and paint cost
Net message-thread CPU, % of one core, median of 3 runs (min to max in brackets where it is wide), 12 s per case, T-029 cases (a) to (d). Gate <= 25 %.

| Config | (a) | (b) | (c) | (d) | Worst median |
|---|---|---|---|---|---|
| 1280x720 100 %, 60 Hz | 7.8 | 9.2 | 9.9 | 8.5 | **9.9** |
| 2560x1440 100 %, 60 Hz | 6.4 | 6.0 | 12.9 | 7.1 | **12.9** |
| 1920x1080 110 %, 60 Hz | 12.7 | 20.1 (19.6 to 21.5) | 15.4 | 14.3 | **20.1** |
| 1280x720 100 %, 144 Hz | 7.9 | 13.3 (12.1 to 16.8) | 11.6 | 11.1 | 13.3 |
| 2560x1440 100 %, 144 Hz | 5.5 | 7.9 | 13.4 | 7.7 | 13.4 |
| 1920x1080 110 %, 144 Hz | 14.2 | **32.4 (16.6 to 32.6)** | 18.8 (10.5 to 28.8) | 17.9 (15.1 to 23.7) | **32.4** |

The 60 Hz gate is met at all three configurations.
F-2: at 144 Hz, 1920x1080 at 110 %, case (b) is over the gate: 32.4 %, and a 5-run repeat (`cpu144_1920x1080@1.1_caseB_rerun.log`) gave 28.4, 29.1, 29.7, 31.9, 27.7 (median 29.1). A caveat limits what this says about the product: the T-029 method feeds a new spectrum frame through `setFrameForTesting` on every vblank, which bypasses the editor's 60/s limiter, so the spectrum data rect is repainted 74 to 93 times per second at 144 Hz in the unpaced cases (60 at 60 Hz); the live-audio check in R2.3 shows the real path is capped at 60/s. 110 % is also the known fractional-scale outlier (T-029 O-A). I cannot attribute the 144 Hz excess to the meters from this data. Architect: waive as harness over-drive, or have me add a harness path that feeds frames through the editor's own `pollProcessor` (then repeat the 144 Hz run).

Meter paint cost (JUCE paint diagnostics, per vblank frame with a panel paint, 3 runs each; `paint_60hz.log`, `paint_144hz.log`):

| Size | Display | p95 per run, ms | Median of p95 | Gate |
|---|---|---|---|---|
| 1280x720 | 60 Hz | 1.038, 0.989, 0.857 | 0.989 | <= 1.0: PASS, one run above |
| 1280x720 | 144 Hz | 0.847, 0.866, 0.837 | 0.847 | PASS |
| 2560x1440 | 60 Hz | 1.268, 1.403, 1.213 | 1.268 | <= 1.5: PASS |
| 2560x1440 | 144 Hz | 1.094, 1.309, 1.040 | 1.094 | PASS |

The 1280x720 margin at 60 Hz is thin (0.011 ms on the median); round 1 measured 0.80 to 0.89 and the machine was noisier this time.

## R2.6 Spectrum regression
`InvestigateSpectrumCheck <dir> 100`: 92 checks, 0 failed (`spectrum_r2.log`).

## R2.7 pluginval
pluginval 1.0.4, strictness 10, GUI tests on, `build/Investigate_artefacts/Release/VST3/Investigate.vst3`: exit 0, SUCCESS (`pluginval_r2.txt`).

## R2 commands
```
build/tools/render-harness/Release/InvestigateMeterCheck.exe <dir> func            # also with METER_FORCE_HZ=60
build/tools/render-harness/Release/InvestigateMeterCheck.exe docs/qa/T-013-screenshots monoshots
build/tools/render-harness/Release/InvestigateMeterCheck.exe <dir> rate 1280x720 10  # also 2560x1440, and with METER_FORCE_HZ=60
METER_FORCE_HZ=60 build/tools/render-harness/Release/InvestigateMeterCheck.exe <dir> paint 2560x1440 12
METER_FORCE_HZ=60 build/tools/render-harness/Release/InvestigateMeterCheck.exe <dir> cpu 1920x1080@1.1 3 15 12   # 144 Hz: no env
build/tools/render-harness/Release/InvestigateSpectrumCheck.exe <dir> 100
third_party/pluginval/1.0.4/pluginval.exe --strictness-level 10 --validate build/Investigate_artefacts/Release/VST3/Investigate.vst3 --output-dir <dir>
```
Open questions: F-1 (144 Hz reset lag and stale detection one vblank over a 60 Hz frame), F-2 (144 Hz 110 % CPU: harness over-drive or real), F-3 (24 fps paced spectrum at 144 Hz, 2560x1440), F-4 (dash versus "−inf" for S max).
