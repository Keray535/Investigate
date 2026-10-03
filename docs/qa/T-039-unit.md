# T-039 unit QA: pan margin and bin spacing in the frame

Verdict: PASS (0 defects). Date 2026-10-03, unit-tester.

## Test files
- `tests/unit/SpectrumMarginTest.cpp` (new, tag `[T-039]`, 17 normal test cases plus 1 hidden `[.perf]`; GENERATE expands the mapper cases to many runs).
- `tests/unit/T035SpectrumMapper.h/.cpp` (new): frozen copy of the T-035 mapper from git HEAD e90a343, class renamed `T035SpectrumMapper`; the baseline for zoomed views. The existing `LegacySpectrumMapper` (pre-zoom) is used for Fit.
- Updated: `SpectrumZoomMapperTest.cpp` (view-ends check now uses `firstVisiblePoint` / `numVisiblePoints`), `SpectrumViewRenderTest.cpp` (fixture uses `maxVisiblePoints`).

## Planned tests and results
| Acceptance criterion | Test | Result |
|---|---|---|
| Capacity, defaults, static_asserts, `spectrumMarginPointsFor == lround(0.25(N-1))` for N=1..2048 | Frame: capacity, defaults and margin point helper (+ file-level static_asserts) | PASS |
| T-035 API unchanged, visible only, also after the frame API (grid cache key) | Mapper: T-035 API output is bit-identical ... | PASS |
| Fit: no margin, bit-identical to pre-zoom mapper, firstVisible 0 | Mapper: Fit gives no margin ... | PASS |
| Fit through the engine, every channel mode (7) and 3 smoothing modes, bit-identical to T-035 mapper incl. overview; `firstVisiblePoint = 0`, `numVisiblePoints = numPoints` | Engine: Fit frames and 25 zoomed views ... | PASS (21 runs) |
| Visible points bit-identical for >= 20 views (25 per run: 1/6 oct .. home, low edge, high edge, inside), freqHz and every active curve | same engine test, plus Mapper: frame API visible points ... (2 home ranges x 3 rates x 3 FFT sizes x 5 N x 6 smoothing x 25 views) | PASS |
| Margin geometry: span within one step of target, grid equals `float(exp2(logLo + step k))` for every k within one float ulp (double domain), within home range, strictly increasing, <= 3072, margin full unless clipped and then maximal, none at a home edge | Margin geometry: span, step, home range, ordering and capacity (+ N=2048 gives exactly 3072, small N) | PASS |
| Margin values vs frame of shifted aligned view <= 0.05 dB | Margin values ... (121k points per run) | PASS, worst 0.00028 dB |
| binHz live = rate / FFT size (4 rates x 3 sizes) | binHz: live frames ... | PASS |
| binHz frozen keeps freeze value across FFT change, retake gives new, unfreeze publishes new | binHz: a frozen frame keeps ... (2 cases) | PASS |
| numPoints clamped to 2048 visible (3072, 100000, 0) | Engine: Fit frame of a processor-style request ... | PASS |
| Frozen re-maps carry margin, equal live re-map | Engine: frozen re-maps ... | PASS |
| Allocation: 0 across freeze, unfreeze, view changes, re-publishes (frames up to 3072 points seen); mapper frame API | Allocation: ... (2 cases) | PASS |
| Silence, 1e-40 denormal and unit input finite, >= -200 dB, in the margin too | Margin values are finite ... | PASS |
| `[.perf]` worst case <= 1.0 ms median | Performance: worst-case re-map with margin ... | PASS |

## Measured re-map time (Release, `[.perf]`, 100 runs each)
Worst case: dual mode, both secondary curves, sidechain, 1/3 octave, 1/6 octave minimum span moved every run, fftSize 65536, 2048 visible and 3072 total points, plus overview. Three runs: median 0.3257, 0.3262, 0.3258 ms (min 0.323, max 0.37 to 1.10 ms with one outlier). T-035 test on the same build: median 0.3253 ms. Budget 1.0 ms.

## Existing test updates
1. `View mapping through the engine: frame carries the clamped view`: the test asserted the old frame shape. It now checks the view ends at `freqHz[firstVisiblePoint]` and `freqHz[firstVisiblePoint + numVisiblePoints - 1]`. Test change, not a defect.
2. `SpectrumView: clip-limited paint matches the full paint inside the clip`: confirmed cause. Before the change: 2 failures (software renderer, scale 2, "right edge strip", 1 pixel off by 1 level, dual 0 and 1). With the fixture at `SpectrumFrame::maxVisiblePoints` (2048) all 84 assertions of the `SpectrumView*` tests pass. Decision: fixture change. A Fit frame never has more than 2048 points (the engine clamps), so 3072 points is not a shape the editor receives before T-037, and the software renderer's exact-match check (count == 0) is sensitive to the vertex count. Not a defect in the shipped editor for T-039. Note for T-037: the editor will receive frames with margin points (up to 3072), so the clip-limited exactness should be re-checked with margin frames drawn through `firstVisiblePoint` / `numVisiblePoints`.

## Regression runs
- Debug: `ctest --preset debug --timeout 300`: 395 of 395 passed (5 Tech 3341/3342 cases skipped by design). Includes T-009 and T-036 processor tests (ProcessorAnalysisLifecycleTest, ProcessorViewFreezeTest, ProcessorResetRequestsTest, ProcessorStateRobustnessTest and the others), editor and SpectrumView tests, pluginval.
- Release: `ctest --preset release --timeout 300`: 395 of 395 passed.
- Build warnings: none reported by the build for the changed files (MSVC default level of the project).

## Real-time safety audit (SpectrumMapper.cpp, SpectrumEngine publishFrame)
Allocation only in `prepare()` (`assign` at SpectrumMapper.cpp lines 20-22, 43-46). The frame API, `mapCurveFrame` and `publishFrame` contain no allocation, locks, I/O or exceptions; verified at runtime by the allocation guard (0 allocations). `jassert` calls (lines 234-277) compile out in Release.

## Observations (not defects)
- A republish is not triggered by a change of `settings.numPoints` alone (T-035 publish key: view, home range, hop). Behaviour unchanged by T-039; the clamp test uses a fresh engine per request. Open question for T-037 if the editor changes the point count at runtime.
- The margin-value test skips cases where the smoothing half width sits on a rounding tie (`lround(octaves/2/step)` for example 1 octave at N = 64): a float-rounded shifted reference view lands on the other side of the tie. This is a reference artefact (18 skipped combinations per run), not a margin defect.
- Sanitizers, clang-tidy and coverage were not run (no preset for them in this change; the change touches no threading and adds no new memory ownership).

## Commands
```
cmake --preset windows-msvc
cmake --build --preset debug
ctest --preset debug --timeout 300
cmake --build --preset release
ctest --preset release --timeout 300
build/tests/Release/InvestigateUnitTests.exe "[T-039][.perf]"
build/tests/Debug/InvestigateUnitTests.exe "[T-039]~[.perf]"
```
