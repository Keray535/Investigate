# T-029 validation (plugin-validator)

Verdict: **PASS (0 defects)**. All CPU and paint gates pass on the median and on the worst single run. The tightest margin is 1280x720 case (c), whose worst single run was 17.1 % against 18 % (its median is 13.4 %). Frame pacing works as specified and input still responds within one vblank. Functional re-run, screenshot comparison and pluginval pass. Two observations (fractional scaling, harness-method caveat) are in "Observations" and are not defects.

## Environment
- Windows 11 Home 10.0.26200, i7-12700H (2.67 GHz measured), Intel Iris Xe + RTX 3060 Laptop, display 1920x1080 at 100 % (so the 2560x1440 window is partly off-screen; the swap chain is full size), MSVC 2022, Release, commit 38f2aa9 plus the uncommitted T-029 working tree (SpectrumView.cpp/.h, SpectrumMath.h). Rebuilt before measuring.
- JUCE default Direct2D renderer, real on-screen window, EcoQoS throttling disabled, high priority.
- Harness: `tools/render-harness/spectrum_check.cpp`, target `InvestigateSpectrumCheck`. Changes this round (all mine, existing cases kept): case (d) `single + all-time average + SC` (secondary param = All-time avg, 5 layers); a never-repainted 1x1 corner probe inside the data rect that counts the view's own paints (hover repaints never touch the corner); `d2d` takes optional arguments `[cpuSeconds [all|WxH@scale [caseMask]]]`; new `lat` mode (input-to-paint latency); the d2d lines now print `view data-rect paints/s` and `expected paced`.
- Method as in T-012: thread cycle time over 12 s wall, a new synthetic 2048-point frame on every 60 Hz vblank, net of the same paced loop without handing frames to the view (baseline about 1.0 %), MeterPanel still a placeholder.

## Commands
```
cmake --build build --config Release --target InvestigateSpectrumCheck
build/tools/render-harness/Release/InvestigateSpectrumCheck.exe <dir> 300 d2d 12 all 15     # matrix, run 3 times (logs T-029-logs/matrix_{1,2,3}.log)
build/tools/render-harness/Release/InvestigateSpectrumCheck.exe <dir> 30 lat                 # latency (T-029-logs/lat_2.log)
build/tools/render-harness/Release/InvestigateSpectrumCheck.exe <dir> 100 d2d 12 1920x1080@1.25 5   # scaling (WxH@scale, caseMask a=1 b=2 c=4 d=8)
build/tools/render-harness/Release/InvestigateSpectrumCheck.exe <dir> 300 sw                 # software renderer (information)
build/tools/render-harness/Release/InvestigateSpectrumCheck.exe <dir> 100                    # 92 checks + screenshots (stopped when the benchmark section started)
third_party/pluginval/1.0.4/pluginval.exe --strictness-level 10 --validate build/Investigate_artefacts/Release/VST3/Investigate.vst3 --output-dir <dir>
python docs/qa/T-029-logs/cmp.py <refDir> <curDir> 48 56 888 620     # PNG comparison over the data rect
```
Logs and scripts: `docs/qa/T-029-logs/`. Side-by-sides: `docs/qa/T-029-screenshots/`.

## 1 and 2. CPU and paint matrix (Direct2D, Release, 3 full runs)
Net CPU, % of one core (3 runs: min / median / max; individual runs in brackets). Paint = SpectrumView paint wall time between the harness probe siblings (300 frames per case per run); p95 shown is the worst of the 3 runs.

| Window | Case | Net CPU min / median / max (runs) | Gate | Paint p95 worst run (limit) |
|---|---|---|---|---|
| 1280x720 | (a) single + RT max | 9.1 / **10.3** / 11.2 (11.2, 9.1, 10.3) | | 1.09 ms |
| 1280x720 | (b) dual + 2 secondaries | 12.6 / **14.1** / 15.4 (15.4, 14.1, 12.6) | | 1.23 ms |
| 1280x720 | (c) dual + 2 sec + SC + hover | 13.3 / **13.4** / 17.1 (17.1, 13.4, 13.3) | | 1.55 ms |
| 1280x720 | (d) single + all-time avg + SC | 11.2 / **11.5** / 11.5 (11.5, 11.2, 11.5) | | 1.31 ms |
| 1280x720 | max over cases | median 14.1 (case b), worst single run 17.1 (case c, run 1) | <= 18 PASS | 1.55 ms (<= 4) PASS |
| 2560x1440 | (a) single + RT max | 8.1 / **8.1** / 8.2 | | 0.80 ms |
| 2560x1440 | (b) dual + 2 secondaries | 10.3 / **10.3** / 10.4 | | 0.95 ms |
| 2560x1440 | (c) dual + 2 sec + SC + hover | 16.7 / **16.8** / 17.1 | | 2.04 ms |
| 2560x1440 | (d) single + all-time avg + SC | 9.9 / **10.2** / 11.6 | | 1.50 ms |
| 2560x1440 | max over cases | median 16.8 (case c), worst single run 17.1 (case c) | <= 20 PASS | 2.04 ms (<= 8) PASS |

Gate verdict is on the median of 3 runs: 14.1 against 18 at 1280x720 (3.9 points of margin) and 16.8 against 20 at 2560x1440 (3.2 points). The worst single value anywhere is 17.1, which also passes both gates (0.9 points under 18 at 1280x720, 2.9 under 20 at 2560x1440). Run 1 was noisier than runs 2 and 3 for the 1280x720 cases (a, b, c each 1 to 4 points above the later runs), consistent with the gui-engineer's reported spread. Paint worst single frames: 1.25 to 3.29 ms (one 3.3 ms outlier at 2560x1440 (c) and (d), run 3), never near the limits.

## 3. Frame pacing: effective view repaint rate
Measured with the corner probe over the same 12 s loaded run (the probe is painted only when the data rect is repainted; hover repaints do not reach it). Paced = `SpectrumMath::pacing` computed in the harness from the data rect, scale and layer count.

| Window | Case | Layers | Paced (rule) | Effective paints/s (runs 1, 2, 3) | Expected |
|---|---|---|---|---|---|
| 1280x720 | (a) | 3 | no | 60.0, 60.0, 60.0 | 60 OK |
| 1280x720 | (b) | 5 | no | 60.0, 60.0, 60.0 | 60 OK |
| 1280x720 | (c) | 7 | yes (layers) | 29.7, 30.0, 30.0 | 30 OK |
| 1280x720 | (d) | 5 | no | 60.0, 60.0, 60.0 | 60 OK |
| 2560x1440 | (a), (b), (c), (d) | 3, 5, 7, 5 | yes (area 3.05 Mpx > 2.0) | 30.0 in all 12 runs | 30 OK |

The loop hands a frame every 16.7 ms; the view repainted every other frame when paced. Hover in (c) repaints at the vblank rate on top of that (small clip), as the gui-engineer described.

## 4. Responsiveness while paced
Method (`lat` mode): frames fed at 60 Hz (so a paced repaint is pending or just issued), each action issued 1 to 16 ms after a fed frame at a random phase, 30 trials each. Latency = call to the first paint of the view region after the call (for hover the clip must contain the new pointer position); an overlay probe above the view and one above the Freeze button record paint ticks. Left click is measured with the secondary curve on All-time max, because the reset toast (the visible change) is only shown for all-time curves (SpectrumView::mouseUp); with RT max a left click changes nothing visible.

| Case | Hover (median / p95 / max ms) | Freeze: view / button (max ms) | Left click toast (median / max) | Right click toast (median / max) |
|---|---|---|---|---|
| 1280x720 (a) unpaced reference | 3.6 / 8.5 / 9.1 | 9.2 / 8.3 | 4.3 / 7.9 | 4.9 / 11.8 |
| 1280x720 (c) paced by layers | 5.6 / 8.5 / 8.6 | 10.0 / 7.7 | 4.4 / 8.3 | 4.7 / 10.5 |
| 2560x1440 (a) paced by area | 4.8 / 7.1 / 7.4 | 9.7 / 12.4 | 5.0 / 8.4 | 4.1 / 10.2 |
| 2560x1440 (c) paced | 4.6 / 8.7 / 9.7 | 13.3 / 8.2 | 4.9 / 8.7 | 5.7 / 10.3 |

0 timeouts in 480 trials. Worst value 13.3 ms (freeze, view, 2560x1440 (c)), below one vblank (16.7 ms), and the paced cases are no slower than the unpaced reference except for the freeze badge at 2560x1440 (c) (median 8.3 against 5.0 ms), which still lands inside one vblank. PASS.
Not measured here: the time until the reset takes effect on the curve itself. That comes from the next analysis frame and is paced like any frame (worst case one extra 30 ms interval). The toast is immediate.

## 5. Functional re-run and screenshots
- 92 of 92 checks pass (`T-029-logs/full_cur.log`, run stopped as the benchmark section started; a second run also gave 92/92). Screenshot generation is deterministic: two runs of the current code are bit-identical in all 21 images.
- Reference: I regenerated the T-012 reference myself (HEAD `SpectrumView.cpp/.h`, `SpectrumMath.h` and HEAD harness, same build settings, D2D snapshots, working-tree files backed up and restored; md5 checked). It is bit-identical to your `Temp\ref` set in all 21 images, so that set is a valid reference.
- Comparison over the data rect (x 48..888, y 56..620 of the 1200x700 snapshots, 473 760 px). Columns are share of pixels with max channel difference of 0 / 1 / 2 to 16 / more than 16, and the largest difference:

| Image | same | 1 level | 2 to 16 | > 16 (count) | max |
|---|---|---|---|---|---|
| 01 to 07 (no sidechain) | 95.4 to 96.2 % | 3.6 to 4.4 % | 0.23 to 0.27 % | 10 to 24 | 47 to 93 |
| 08, 10, 11 (sidechain) | 95.2 to 95.3 % | 4.3 % | 0.35 to 0.39 % | 296 to 304 | 43 to 60 |
| 09 (sidechain hidden) | 95.4 % | 4.3 % | 0.27 % | 12 | 53 |
| 12 to 15 (hover) | 95.6 to 95.7 % | 3.9 % | 0.33 to 0.36 % | 211 to 288 | 53 to 60 |
| 16 toast copied, 18 frozen hover | 96.5 %, 96.3 % | 3.3 %, 3.4 % | 0.14 %, 0.28 % | 9, 8 | 149, 34 |
| 17 toast reset, 20-synth-3 | 100 % | 0 | 0 | 0 | 0 |
| 20-synth-5, placement-1khz | 99.8 %, 99.0 % | 0, 0.95 % | 0.18 %, 0.06 % | 0, 3 | 8, 29 |

This confirms your figures (95.7 % bit-identical, about 4 % at 1 level) with two small deviations from your quoted bounds: the 2-to-16 band is 0.23 to 0.39 % (your "under 0.3 %" holds only for the non-sidechain images), and the largest single-pixel differences are 93 (image 01) and 149 (image 16), not 60. Both are isolated pixels on a thin peak of the stroke: image 01 at (799, 581), image 16 at (369, 601), and in image 16 the neighbour differs by 74 (2 pixels over 40). Side by side with the 4x amplified difference (`docs/qa/T-029-screenshots/`): `sbs_sidechain_top.png` (spike top), `sbs_sidechain_foot.png` (spike foot with the primary stroke crossing it), `sbs_01_px93.png`, `sbs_16_px149.png`. Each shows ref | current | difference. The sidechain spike differences are a one-pixel column on each stroke edge (a few levels up to 60 on the outline, invisible at 1x, and the fill and gradient are unchanged); the two 93/149 pixels are single stroke pixels at a vertex. Nothing is moved, missing or recoloured; curve shapes, fills, grid, legend and text are the same.
**Judgement: anti-aliasing and gradient quantisation only, no visible regression.** (With the same reasoning as in the task criterion: differences are allowed where shown side by side.)

## 6. Fractional display scaling (unit-tester O-2)
`juce::Desktop::setGlobalScaleFactor`, editor sized in logical units for a physical 1920x1080 window (also 1280x720 at 1.0 and 1.1), 12 s per case, net CPU %, Release, D2D. (a) = single + RT max (unpaced at this size), (c) = dual + 2 sec + SC + hover (paced, 30 fps). Two passes for the main scales.

| Scale | (a) pass 1 / 2 | (c) pass 1 / 2 | Note |
|---|---|---|---|
| 1.0 | 10.4 / 10.5 | 14.1 / 14.2 | reference |
| 1.25 | 10.4 / 10.2 | 14.9 / 15.4 | no jump |
| 1.5 | 10.4 / 10.3 | 14.4 / 14.9 | no jump |
| 1.75 | 13.4 / 12.1 | 17.0 / 16.7 | +2 to 3 points |
| 2.0 | 10.3 / 9.9 | 15.4 / 15.3 | integer, no jump |
| 1.1 (single pass) | 14.9 | 17.9 | +4.5 / +3.7 points |
| 1.7, 1.8, 1.9 (a only) | 12.7, 11.8, 11.8 | | +1.4 to +2.3 |
| 1280x720 at 1.1 (two passes) vs 1.0 | 9.2, 8.9 vs 10.2 | 13.5, 13.0 vs 13.5 | no jump |

No scale shows the large jump (a multiple of the 1.0 value) that a wholesale per-rectangle fallback would cause: 1.25 and 1.5, the scales named in your question, are indistinguishable from 1.0 and 2.0. Some other fractional scales at 1920x1080 are 1.5 to 4.5 points higher (1.1, 1.7 to 1.75), while the same scale at 1280x720 is not. I cannot tell from CPU alone whether this is a partial fallback of a few rectangles or a pixel-alignment effect; it stays inside the gates' order of magnitude but case (c) at 1920x1080 and 110 % reaches 17.9 %, which is at the 18 % figure (the gate itself is defined for 1280x720 at 100 %). Reported as an observation (O-A below), not a defect.

## 7. pluginval
pluginval 1.0.4, strictness 10, GUI tests on, `build/Investigate_artefacts/Release/VST3/Investigate.vst3` (rebuilt now): exit 0, "SUCCESS", no failures (`T-029-logs/pluginval.txt`).

## 8. Software renderer (information only)
`spectrum_check <dir> 300 sw`, p95 ms: 1280x720 worst case (dual + 2 sec + SC + hover) 10.26, dual + 2 sec + SC 9.80, dual + 2 sec 8.02, single + RT max 4.68; 2560x1440 worst case 25.23, dual + 2 sec + SC 24.72, dual + 2 sec 21.06, single + RT max 13.07. Equal to the T-012 round 2 figures (10.58 / 25.77 / 26.38) within 5 %, as expected. Allocation behaviour: see `docs/qa/T-029-unit.md`.

## Gate summary
| Criterion | Result |
|---|---|
| CPU max over cases, 1280x720 <= 18 % (median of 3) | PASS, 14.1 % (worst run 17.1 %) |
| CPU max over cases, 2560x1440 <= 20 % (median of 3) | PASS, 16.8 % (worst run 17.1 %) |
| Paint p95 <= 4 ms / <= 8 ms | PASS, 1.55 / 2.04 ms |
| Frame pacing threshold and effective rates reported; responsiveness within 1 vblank | PASS (section 3, 4) |
| 92 functional checks | PASS |
| No visual regression (screenshots) | PASS, differences are AA/quantisation (section 5) |
| pluginval strictness 10 | PASS |

## Defects
None.

## Observations (no action required for PASS)
- O-A, fractional scaling: see section 6. A cheap follow-up for the gui-engineer, if wanted: run case (c) at 1920x1080 and 110 % with the T-029 `PERF` switch for the 0.002 px slack to see whether the elevated cost goes away.
- O-B, method caveat: in the T-012 loop the invisible probe siblings are repainted every vblank, so a window present happens every vblank even when the view is paced. Pacing therefore saves the fill and stroke work but not the per-present floor (4 to 6 %). In a real session with an idle meter panel the paced result would be a little lower than these numbers; with a 60 fps meter it will be about these numbers. The gates were measured with the T-012 method as specified.
- O-C, one harness paint median of 0.00 ms in run 3 (2560x1440 (d)) is a probe-pairing artefact (paint p95 and CPU in the same row are normal).
- O-D, the 2560x1440 numbers come from a 1920x1080 display with part of the window off-screen (as in T-012). A native 1440p monitor was not available.

## Open questions
1. O-A above: accept as is, or ask the gui-engineer to look at the fractional-scale elevation at 1920x1080 (1.1, 1.7 to 1.75)?
2. T-013 will add repaints. The 2560x1440 case (c) is at 16.8 % against 20 % (3.2 points of headroom) and 1280x720 (b), unpaced, at 14.1 % against 18 %.

## Files
- `C:\Users\Keray\Desktop\Plugin_dev\docs\qa\T-029-validation.md`
- `C:\Users\Keray\Desktop\Plugin_dev\docs\qa\T-029-logs\` (matrix_1..3.log, lat_2.log, scale.log, scale_extra.log, sw.log, full_cur.log, pluginval.txt, cmp.py, crop.py)
- `C:\Users\Keray\Desktop\Plugin_dev\docs\qa\T-029-screenshots\` (sbs_sidechain_top.png, sbs_sidechain_foot.png, sbs_01_px93.png, sbs_16_px149.png)
- `C:\Users\Keray\Desktop\Plugin_dev\tools\render-harness\spectrum_check.cpp` (case (d), corner probe, `lat` mode, scale and case-mask arguments)
