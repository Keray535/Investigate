# T-012 validation, ROUND 2 of 3 (flat fill, useGradientFill = false)

Verdict: **FAIL (1 defect)**. All functional checks and pluginval pass. The CPU gate still fails at 2560x1440 (28.0 % gross / 26.9 % net vs 25 %). The 1280x720 gate passes. The flat fill did not change the CPU cost measurably.

Build: Release, binary built after the source change (harness 06:10 vs SpectrumView.cpp 06:04), `useGradientFill = false` confirmed at SpectrumView.cpp:23. Same machine, display and harness as round 1.

Commands: as in round 1; full run `build/tools/render-harness/Release/InvestigateSpectrumCheck.exe docs/qa/T-012-screenshots 300` (92 checks, 0 failed, exit 0, log `docs/qa/T-012-screenshots/run.log`, overwritten with the round 2 run); pluginval 1.0.4 strictness 10 with GUI tests on: exit 0, SUCCESS.

## Round 2 gates (Direct2D, worst case = dual + 2 secondaries + SC + hover)
| Window | paint p95 (limit) | CPU gross / net (limit 25 %) | Result |
|---|---|---|---|
| 1280x720 | 0.89 ms (4) | 20.2 % / 19.2 % | PASS |
| 2560x1440 | 1.12 ms (8) | 28.0 % / 26.9 % | **FAIL (CPU)** |

## All cases (paint median / p95 / worst ms; CPU gross / net %), round 1 gross in brackets
| Window | Case | Paint | CPU |
|---|---|---|---|
| 1280x720 | single + RT max | 0.26 / 0.28 / 0.38 | 20.4 / 19.4 [20.5] |
| 1280x720 | dual + 2 secondaries | 0.41 / 0.44 / 0.48 | 25.3 / 24.3 [25.5] |
| 1280x720 | worst case | 0.81 / 0.89 / 0.92 | 20.2 / 19.2 [20.2] |
| 2560x1440 | single + RT max | 0.28 / 0.42 / 0.99 | 36.3 / 35.3 [37.2] |
| 2560x1440 | dual + 2 secondaries | 0.43 / 0.58 / 0.86 | 41.9 / 40.9 [42.2] |
| 2560x1440 | worst case | 0.85 / 1.12 / 1.74 | 28.0 / 26.9 [28.9] |

Improvement from the flat fill is about 1 point at most (within noise); the gradient was not the main cost. As in round 1, the CPU cost lies outside `paint()` (Direct2D deferred rasterisation / present of the full repainted data rect), scales with area, and the worst case is cheaper than the cases without SC and hover. Software-image numbers (information) are unchanged within 3 % of round 1 (worst case p95 10.54 / 25.77 ms).

## Functional re-run (flat fill)
All 92 checks pass again: 1 kHz placement, hover readout, freeze, left-click reset, right-click clipboard, curve/flag matrix (including exact-colour tests). Screenshots 01 to 18, 20-synth-3/5, placement-1khz and bench-software-dual-1280 regenerated in `docs/qa/T-012-screenshots/`.

## Defect (open)
1. CPU budget at 2560x1440 (check: message-thread CPU at 60 fps, 12 s). Expected at most 25 % of one core (T-012 amended criterion). Actual 28.0 % gross / 26.9 % net for the stated worst case; every 2560x1440 case is 36 to 42 %. Repro: `build/tools/render-harness/Release/InvestigateSpectrumCheck.exe docs/qa/T-012-screenshots 300 d2d`, see `DIRECT2D 2560x1440` lines. Artefact: `docs/qa/T-012-screenshots/run.log`.

## Open questions
1. Architect decision: (a) accept the CPU gate at 1280x720 only (all pass except dual+2 secondaries at 25.3 % gross / 24.3 % net, information case), (b) dirty-rect repaint of the changed range, or (c) OpenGL (not switched, per instruction). The display is 1080p, so the 1440p window is partly off-screen; a real 1440p monitor would confirm.

---
# Round 1 (kept for history)
# T-012 validation (plugin-validator), round 1 of 3

Verdict: **FAIL (1 defect)**. Everything passes except the message-thread CPU gate at 2560x1440.

## Environment
- Windows 11 Home 10.0.26200, i7-12700H (2.68 GHz measured), Intel Iris Xe + RTX 3060 Laptop, display 1920x1080, MSVC 2022, Release, commit 38f1259 plus uncommitted T-012 work.
- JUCE default Direct2D renderer in a real on-screen window. The 2560x1440 window is larger than the 1920x1080 display, so part of it is off-screen (the swap chain is still 2560x1440).
- pluginval 1.0.4 (`third_party/pluginval/1.0.4`), GUI tests on, headless not needed.
- Harness: `tools/render-harness/spectrum_check.cpp` (target `InvestigateSpectrumCheck`, built with `-DPLUGIN_BUILD_HARNESS=ON`). Process EcoQoS throttling disabled, high priority, `timeBeginPeriod(1)`.

## Commands
```
cmake --preset windows-msvc -DPLUGIN_BUILD_HARNESS=ON
cmake --build build --config Release --target InvestigateSpectrumCheck
build/tools/render-harness/Release/InvestigateSpectrumCheck.exe docs/qa/T-012-screenshots 300      # all checks + benchmarks, exit 0
build/tools/render-harness/Release/InvestigateSpectrumCheck.exe <dir> 300 d2d                      # Direct2D benchmark only
build/tools/render-harness/Release/InvestigateSpectrumCheck.exe <dir> 300 sw                       # software image (info)
third_party/pluginval/1.0.4/pluginval.exe --strictness-level 10 --validate build/Investigate_artefacts/Release/VST3/Investigate.vst3 --output-dir <dir>
```
Full log: `docs/qa/T-012-screenshots/run.log` (92 checks, 0 failed).

## Results per criterion
| Criterion | Result | Evidence |
|---|---|---|
| SpectrumMath unit tests | covered by `docs/qa/T-012-unit.md` (PASS) | not repeated |
| 1 kHz -6 dBFS placement | PASS | Peak vertex 995.74 Hz at x 523.19 vs 1 kHz column 523.71 (delta -0.52 px). Peak level -6.77 dB, inside the Hann scalloping bound [-7.5, -6.0] (error -0.75 dB = 4.4 px vs the exact -6.02 line). Rendered stroke apex: column delta -0.71 px, row delta -0.01 px vs the frame peak. `placement-1khz.png` |
| Hover readout | PASS | Card shows "1.00 kHz / B5 +21c / level rows" at 1 kHz, "19.9 kHz D#10 -4c" at the right edge; card flips left near the right edge, below the pointer near the top (13, 15). Format matches ui-spec 5.5. Crosshair, markers and hint visible. |
| Freeze | PASS | Sequence number 375 at the press, 375 after the click, 375 after 400 ms of audio (no update). Hover and right-click copy work on the frozen frame (`18-frozen-hover.png`). Unfreeze: first new frame after 8.9 ms. Re-freeze then `channelMode` change: unfrozen. |
| Left-click reset | PASS | All-time max peak -6.7 dB cleared to -200 dB in the first frame, 9.2 ms after the click. A 30 px left drag does not reset. Toast in `17-toast-reset.png`. |
| Right-click clipboard | PASS | 9 positions from 20 Hz to 19.9 kHz match an independent 3-significant-figure rendition of ui-spec 5.6 ("20.0", "39.9", "112", "200", "632", "2000", "10000", "19900") and `clipboardTextForX`. Card at 1 kHz shows "1.00 kHz", clipboard "1000", toast "Copied 1000 Hz" (consistent after rounding). Right-click outside the data rect leaves the clipboard untouched. |
| Curve types and flag combinations | PASS | 11 live screenshots (01 to 11) and a synthetic flat-curve matrix of 7 flag combinations with exact colour tests: primary A always; primary B only in dual; secondary A with secondary on; secondary B only dual + secondary; sidechain silhouette only with a sidechain bus and `showSidechain` on, absent when hidden. |
| Paint p95, Direct2D, worst case (dual + 2 secondaries + SC + hover), 300 frames | PASS | 1280x720: median 0.80, p95 **0.89** ms (limit 4), worst 1.04. 2560x1440: median 0.88, p95 **1.24** ms (limit 8), worst 1.55. |
| Message-thread CPU at 60 fps, worst case, 12 s | **FAIL at 2560x1440**, PASS at 1280x720 | 1280x720: 20.2 % gross / 19.1 % net. 2560x1440: **28.9 % gross / 27.9 % net** (limit 25 %). Measured at 60.0 paints/s. |
| Software-rendered Image (information only) | recorded | Table below |
| Allocation | covered by unit report | not repeated |
| pluginval strictness 10 | PASS | Exit 0, "SUCCESS", including Editor, Open editor whilst processing, Editor Automation, Fuzz parameters. |

## Paint and CPU numbers (Direct2D, Release, 300 frames paint, 12 s CPU)
Paint = wall time of `SpectrumView` paint (including children), measured with two 1x1 invisible sibling components placed just before and after the view in z-order. Note that with Direct2D this is mostly command recording; the rasterisation work is deferred, which is why the CPU gate exists.

| Window | Case | paint median / p95 / worst (ms) | CPU gross / net (% of one core) |
|---|---|---|---|
| 1280x720 | single + RT max (factory view) | 0.27 / 0.29 / 0.30 | 20.5 / 19.5 |
| 1280x720 | dual + 2 secondaries | 0.41 / 0.45 / 0.57 | 25.5 / 24.5 |
| 1280x720 | dual + 2 sec + SC + hover (stated worst case) | 0.80 / 0.89 / 1.04 | 20.2 / 19.1 |
| 2560x1440 | single + RT max | 0.29 / 0.44 / 0.59 | 37.2 / 36.0 |
| 2560x1440 | dual + 2 secondaries | 0.43 / 0.54 / 0.83 | 42.2 / 41.2 |
| 2560x1440 | dual + 2 sec + SC + hover (stated worst case) | 0.88 / 1.24 / 1.55 | 28.9 / 27.9 |

Net = minus the same paced loop without handing frames to the view (baseline about 1 %). CPU is thread cycle time (`QueryThreadCycleTime`) over wall time.

Software-rendered `juce::SoftwareImageType` (information only, data-rect clip, p95 ms):

| Case | 1280x720 | 2560x1440 |
|---|---|---|
| single, no secondary | 3.72 | 11.11 |
| single + RT max | 4.83 | 13.02 |
| dual + 2 secondaries | 8.10 | 20.85 |
| single + RT max + SC | 6.81 | 18.57 |
| dual + 2 secondaries + SC | 10.07 | 25.61 |
| single + RT max + hover | 5.20 | 13.90 |
| dual + SC + hover | 10.58 | 26.38 |

## Defects
1. **CPU budget, 2560x1440** (check: message-thread CPU at 60 fps).
   - Expected (T-012 amended criterion, roadmap Budgets): at most 25 % of one core over at least 10 s in the worst displayed case and window.
   - Actual: 28.9 % gross (27.9 % net) at 2560x1440 for dual + 2 secondaries + SC + hover. 1280x720 passes at 20.2 %.
   - Additional finding: the stated worst case is not the most expensive case. Dual + 2 secondaries without SC and hover costs more (42 % at 2560x1440, 25.5 % gross / 24.5 % net at 1280x720), and the factory-default single + RT max view costs 37 % at 2560x1440 and 20.5 % at 1280x720. Every 2560x1440 case exceeds 25 %. The cause is Direct2D work outside `paint()` (paint itself is about 1 ms), and it scales with the repainted area, not with curve count. It is not explained by the SC or hover rows (the case with them is cheaper than the case without). Likely contributors: the full data-rect repaint per frame and the primary gradient fill (the task log mentions `useGradientFill` as a fallback switch).
   - Repro: `build/tools/render-harness/Release/InvestigateSpectrumCheck.exe <dir> 300 d2d` (about 6 minutes), look at the `DIRECT2D 2560x1440` lines.
   - Artefact: `docs/qa/T-012-screenshots/run.log`.

## Open questions
1. The criterion says "same worst case and window". I applied the CPU gate to both 1280x720 and 2560x1440, as the paint gates are. If the CPU gate is meant for 1280x720 only, the stated worst case passes (20.2 %), but dual + 2 secondaries without SC would be borderline (25.5 % gross, 24.5 % net).
2. Per the task: propose OpenGL for the CPU number, subject to architect approval, not switched. Cheaper options to try first: `useGradientFill` off, and repainting only the dirty column range when the frame changes. Please decide which route.
3. This display is 1920x1080, so the 2560x1440 window is partly off-screen. A 1440p monitor would confirm the number; the swap chain size is the same, so I expect similar results.
4. Not tested here: the `[.perf]` Catch2 tests (not required), AU/auval (macOS only).

## Files
- `C:\Users\Keray\Desktop\Plugin_dev\docs\qa\T-012-validation.md`
- `C:\Users\Keray\Desktop\Plugin_dev\docs\qa\T-012-screenshots\` (01 to 18, 20-synth-3/5, placement-1khz, run.log)
- `C:\Users\Keray\Desktop\Plugin_dev\tools\render-harness\spectrum_check.cpp` (extended: Direct2D paint probes, 12 s CPU measurement, Side-only tone for M/S distinctness)

Screenshot index: 01 single off; 02 single RT max; 03 all-time max; 04 all-time avg; 05 dual L/R off; 06 dual L/R RT max; 07 dual M/S all-time max; 08 single + sidechain; 09 sidechain hidden; 10 dual + SC + RT max; 11 single RT max + SC; 12 to 15 hover variants (centre, right edge, single, top-left); 16 copied toast; 17 reset toast; 18 frozen with hover; 20-synth-3/5 flat-curve matrices.
