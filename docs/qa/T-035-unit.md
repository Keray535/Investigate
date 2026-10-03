# T-035 unit QA report (round 1 of 3)

Verdict: PASS (0 defects)

Scope: `source/dsp/DisplayRange.h`, `SpectrumSettings.h`, `SpectrumFrame.h`, `SpectrumMapper.h/.cpp`, `SpectrumEngine.h/.cpp` (working tree on top of HEAD 7130542).

## Test files (all new, `tests/unit/`)
- `DisplayRangeViewTest.cpp`: view clamps.
- `SpectrumZoomMapperTest.cpp`: legacy regression, view mapping, edge smoothing, overview (mapper level).
- `SpectrumZoomEngineTest.cpp`: re-publish, freeze, unfreeze, epoch, allocation, overview (engine level), `[.perf]` timing.
- `SpectrumZoomTestUtils.h`: deterministic pink noise plus tones source, `Rig` (engine + settings + audio), bitwise frame comparison.
- `LegacySpectrumMapper.h/.cpp`: renamed frozen copy of the HEAD mapper (`git show HEAD:source/dsp/SpectrumMapper.*`), used as the bit-identical baseline.

The existing T-004/T-005/display-range tests were not touched and all still pass.

## Planned tests and results
| # | Test (tag) | Result |
|---|---|---|
| 1 | clampViewFrequency: 5 homes x 200k random inputs (outside, inside, tiny, huge spans): inside home, span >= min(1/6 oct, home span), idempotent bit for bit | PASS |
| 2 | clampViewFrequency: (0,0), NaN, non-positive, inf give home; legal view passes through; reversed pair swapped; narrow view widens about log centre; outside view slides keeping span | PASS |
| 3 | clampViewDb: same random sweep (4 homes x 200k) plus (0,0), NaN, pass-through, widen, slide | PASS |
| 4 | No regression: engine vs HEAD mapper, memcmp of freqHz and every active curve, 7 channel modes x 6 smoothing modes (stereo, sidechain 2, all-time average secondary, 2048 pts, 3 publish rounds each) | PASS (42 cases) |
| 5 | No regression sweep: 44.1/48/96 kHz, fft 512..65536, Nyquist-limited range, freqLow above fHi/2, 1/2/64/513/777/2048 points, slopes, all secondary modes | PASS (7 cases) |
| 6 | View mapping at mapper level: 11 views (incl. invalid, reversed, out of range, too narrow) x 3 point counts; ends within 1e-4, strictly increasing, equals DisplayRange clamp | PASS |
| 7 | View mapping through the engine: frame viewFreqLow/High equal the clamp, ends and monotonic | PASS |
| 8 | Edge smoothing vs independent double-precision reference on the same aligned grid widened 2x per side and clamped to home: fft 1024/8192 x 6 smoothing modes x 512/2048 points x spans {1/6, 1/2, 1, 3, full} oct x positions {low edge, 0.37, high edge}; first and last 5 % within 0.05 dB; pink noise + 3 tones | PASS (360 cases, 0.05 dB) |
| 9 | Edge smoothing: interior also matches, home-edge truncation unchanged; zoomed curve vs Fit curve at the same frequencies (mean abs diff < 0.25 dB) | PASS |
| 10 | Re-publish: view change, no samples: published in the same call, frame view updated, sequence increases | PASS |
| 11 | Re-publish: no change and no hop publishes nothing; same view twice publishes once; smoothing/slope change alone publishes nothing | PASS |
| 12 | Re-publish: home-range change publishes; nothing before the analyser produced a frame | PASS |
| 13 | Re-publish: analyser state unchanged (twin engines, 6 re-publishes on A, then 4 rounds of audio: frames bit-identical, all-time-average secondary and dual mode) | PASS |
| 14 | Freeze entry: returns true, frame carries new epoch and equals the last live frame; 10 x 1 s new loud audio publishes nothing | PASS |
| 15 | Freeze: view changes re-map the snapshot, equal to a live re-map of the same state (twin) | PASS |
| 16 | Freeze: smoothing/slope changes ignored (alone and combined with a view change); home-range change re-maps | PASS |
| 17 | Freeze: fftSize, window, channelMode changes (several, with audio) do not corrupt the snapshot (twin comparison) | PASS |
| 18 | Unfreeze: live state published on the next call with no new audio, equal to a never-frozen twin | PASS |
| 19 | Frozen frames all carry the frozen epoch; a new epoch while frozen retakes the snapshot; live frames carry the current epoch; freeze on empty analyser publishes nothing | PASS |
| 20 | Freeze: analysis continues, `resetSecondary` effective after unfreeze (twin) | PASS |
| 21 | Allocation (`ScopedAllocGuard`): 4 view changes, freeze, audio while frozen, 4 view changes, home change, epoch retake, unfreeze, view reset: >= 12 publishes, 0 allocations | PASS |
| 22 | Overview: 256 points over effective home range (Nyquist-limited), strictly increasing, view independent, equals full-range curve (<= 0.1 dB at same density), home change moves it, frozen overview from snapshot, overviewPoints honoured and clamped | PASS |
| 23 | `[.perf]` re-map timing, Release | PASS, see below |

Mutation sanity check: replacing the padded reference in test 8 with an unpadded one fails 205 of 360 assertions, so the edge test discriminates.

## Timing (Release, `[.perf]`)
Worst case: lrDual, allTimeAverage secondary (two secondary curves), sidechain stereo, 1/3-octave smoothing, 1/6-octave view moved on every run (grid rebuilt), fft 65536, 2048 points, all curves plus overview, measured around one `processPending` call (re-publish):
- Median over 100 runs: 0.30 to 0.31 ms (three runs: 0.312, 0.311, 0.303 ms); min 0.30 ms, max 0.45 to 0.47 ms. Budget 1.0 ms. PASS.

## Warnings
`Investigate` target rebuilt from scratch in Debug at /W4: no warnings from `SpectrumMapper.cpp`, `SpectrumEngine.cpp` or other source files. The new test files compile without warnings in Debug and Release. (Only the known third-party pffft/libebur128 D9025 notices appear.)

## Full suite
`ctest --preset debug --timeout 300`: 367/367 passed (5 pre-existing skips). `ctest --preset release --timeout 300`: 367/367 passed.

## Real-time and allocation audit
`processBlock` is unchanged. On the analysis thread, `SpectrumEngine::processPending`, `takeSnapshot`, `publishFrame` and `SpectrumMapper::buildGrid/mapOnGrid/mapOverview` contain no `new`, `push_back`, `resize`, `assign` or locks; all `assign` calls are in `prepare()` (`SpectrumMapper.cpp:41-49`, `SpectrumEngine.cpp:30-52`). Confirmed dynamically by test 21. Sanitizers and coverage were not run (no memory or threading change beyond preallocated buffers).

## Defects
None.

## Observations and open questions
1. A change of `settings.numPoints` or `smoothing` alone (no view, home or freeze change, no hop) does not trigger a re-publish; the next hop shows it. The task scope only lists view, home range and freeze state, so this is per spec. The editor (T-036/T-037) should be aware if a point-count change on resize should show at once.
2. Freeze entry when the analyser has produced no frame (or was just reset by an fftSize/window/layout change in the same call) leaves the snapshot invalid and publishes nothing while frozen. Documented by the dsp-engineer; tested only for the empty-analyser case. Confirm that the editor behaves acceptably (frozen with nothing to show).
3. A new epoch while still frozen retakes the snapshot from the current, internally advanced analyser state, not from the frame last displayed. Not specified in the acceptance criteria; the test only asserts the epoch.
4. The edge-smoothing criterion was tested against an independent reference on the same aligned point grid (as the dsp-engineer advised); the test uses averaged pink noise plus Hann-windowed tones, not a lone single-bin spike.

## Commands
```
cmake --preset windows-msvc
cmake --build --preset debug
build/tests/Debug/InvestigateUnitTests.exe "[T-035]~[.perf]"
cmake --build --preset release
build/tests/Release/InvestigateUnitTests.exe "[.perf][T-035]"
ctest --preset debug --timeout 300
ctest --preset release --timeout 300
cmake --build build --config Debug --target Investigate -- -t:Rebuild   (warning check)
```
