# T-026: Hide timing tests from the default test run — unit QA report

Verdict: **PASS**

## Scope read

CLAUDE.md, `docs/tasks/T-026-perf-test-gating.md`, `docs/roadmap.md` (Budgets table and the
Decisions dated 2026-09-28 "Performance assertions inside the unit suite are noisy..." and
2026-09-29 "Timing tests use Catch2's hidden tag `[.perf]`..."), and `docs/tasks/T-025-compile-definitions.md`
open question 2 (the two timing tests flagged as flaky on the dev machine: `MeterBank::process()`
mean/p99 and `SpectrumMapper` per-frame, `SpectrumEngineTest.cpp:268`).

## Test cases identified (before writing anything)

Searched `tests/unit/` for every test that measures wall-clock time (`std::chrono::steady_clock`)
and asserts a pass/fail bound on it. Found four files, six `TEST_CASE`s:

| # | File | Test case | Old tag | Old assertion | Roadmap budget |
|---|---|---|---|---|---|
| 1 | `SpectrumEngineTest.cpp` | `Performance: SpectrumMapper 2048 points x 3 curves <= 0.3 ms per frame (Release)` | `[T-005][mapper][performance][AC8]` | `<= 0.3 ms` | SpectrumMapper ≤ 0.3 ms/frame (matches; no change) |
| 2 | `SpectrumAnalyserPerformanceTest.cpp` | `Performance: fftSize 4096, 4x overlap, stereo 48 kHz, 1 s <= 15 ms (Release)` | `[dsp][spectrum][performance][T-004]` | `<= 15 ms` | analyser default ≤ 15 ms/s (matches; no change) |
| 3 | `SpectrumAnalyserPerformanceTest.cpp` | `Performance: fftSize 65536, 8x overlap, dual mode + stereo sidechain, 1 s @ 48 kHz <= 60 ms (Release)` | `[dsp][spectrum][performance][T-004]` | `<= 60 ms` | analyser worst-case ≤ 60 ms/s (matches; no change) |
| 4 | `MeterBankTest.cpp` | `Performance: MeterBank::process() <= 10us mean, <= 30us p99 ...` | `[dsp][meterbank][performance][perf][T-024]` (already had a plain, non-hidden `[perf]` tag) | `mean <= 10 µs`, `p99 <= 30 µs` (T-024's stretch target) | MeterBank alone: mean ≤ 15 µs, p99 ≤ 40 µs → **assertion changed to the roadmap budget**; T-024's 10/30 µs now printed as an informational met/missed comparison only |
| 5 | `LoudnessMeterPerformanceTest.cpp` | `Performance: process() <= 5us mean per 128-sample stereo block at 48kHz (Release)` | `[dsp][loudness][performance][T-006]` | `<= 5 µs` mean | LoudnessMeter ≤ 5 µs mean (matches; no change) |
| 6 | `LoudnessMeterPerformanceTest.cpp` | `Performance: denormal-range input does not slow process() down` | `[dsp][loudness][performance][T-006]` | `denormalUs <= normalUs*10+5` (relative safety check, no fixed roadmap number) | No roadmap number to substitute; assertion logic kept, only hidden |

Files scanned and ruled **out** (they use `std::chrono` or the word "elapsed" but do not gate on
measured wall-clock time): `SpectrumAnalyserAveragingTest.cpp` and `LevelMeterTest.cpp` /
`CorrelationMeterTest.cpp` compute a simulated timeline from a sample index divided by sample
rate (testing decay/behaviour over synthesised time, not CPU cost); `SampleFifoTest.cpp` and
`MeterBankTest.cpp`'s stress test use `steady_clock` only to bound a stress-test duration
(1.5 s), not as a pass/fail timing assertion; a broad `grep` for `steady_clock|high_resolution_clock`
confirmed only the four files above touch wall-clock APIs at all.

Change made in every case: added Catch2's hidden tag `[.perf]` (shorthand for `[.][perf]`) to the
test case's tag list, so it is skipped by a default run and skipped by `catch_discover_tests`'
CTest registration, but is still selectable with the tag filter `"[perf]"` (Catch2 tag filters
match on the tag name regardless of the leading `.`; the dot only marks the tag hidden). Test #4's
assertion constants were changed from the T-024 stretch target (10/30 µs) to the roadmap Budgets
table value (15/40 µs); the old numbers are still measured and printed as an informational
met/missed line, not asserted. No other test's tags, assertions or logic were touched.

## Commands and results

Built only the unit test target, as instructed (`Investigate_Standalone` is known-broken per
T-025 and was not touched or worked around):

```
cmake --build --preset release --target InvestigateUnitTests
```

Built clean, 0 errors, only the four touched `.cpp` files recompiled.

### Before retagging (baseline, captured before any edit)

```
build/tests/Release/InvestigateUnitTests.exe --list-tests            -> 170 test cases
build/tests/Release/InvestigateUnitTests.exe "[perf]" --list-tests   -> 1 matching test case
                                                                         (MeterBankTest's, already
                                                                         plain-tagged [perf] but not
                                                                         hidden, so still registered)
ctest --preset release -N                                            -> Total Tests: 171
```

### After retagging

```
build/tests/Release/InvestigateUnitTests.exe --list-tests            -> 164 test cases
build/tests/Release/InvestigateUnitTests.exe "[perf]" --list-tests   -> 6 matching test cases,
                                                                         all shown with the
                                                                         [.] hidden tag
ctest --preset release -N                                            -> Total Tests: 165
```

`171 − 165 = 6`, equal to the number of retagged test cases (acceptance criterion 1 met). None of
the six timing test names appear anywhere in `ctest --preset release -N`'s output (checked with a
targeted grep for "performance", "denormal-range" and "SpectrumMapper 2048" — no matches other
than an unrelated NaN/Inf safety test name that happens to contain "denormal-range").

### Isolated perf run (acceptance criterion 2), Release, one at a time via the tag filter

Command, run from `build/tests/Release`:

```
InvestigateUnitTests.exe "[perf]" -s
```

Result: `All tests passed (9 assertions in 6 test cases)`. Measured values (from `-s`, which
prints passing `INFO`/messages):

| Test | Measured | Roadmap budget | Result |
|---|---|---|---|
| SpectrumMapper, 2048 pts x 3 curves | 0.157866 ms/frame | ≤ 0.3 ms | PASS |
| Analyser, fftSize 4096/4x overlap, 1 s | 0.5847 ms | ≤ 15 ms | PASS |
| Analyser, fftSize 65536/8x overlap, dual + sidechain, 1 s | 4.1633 ms | ≤ 60 ms | PASS |
| MeterBank::process() | mean 5.37121 µs, p99 29.8 µs | mean ≤ 15 µs, p99 ≤ 40 µs | PASS |
| MeterBank::process(), T-024 stretch target (informational only) | same numbers | mean ≤ 10 µs, p99 ≤ 30 µs | met, met (printed only, not asserted) |
| LoudnessMeter::process() | mean 1.53406 µs/block (19999 blocks) | ≤ 5 µs mean | PASS |
| LoudnessMeter::process(), denormal-range input | normal 15.7995 µs/block, denormal 16.554 µs/block | denormal ≤ normal×10+5 (no roadmap number; relative safety check) | PASS |

This directly answers T-025's open question 2: both tests it flagged as flaky under full-suite
contention (`MeterBank::process()` and `SpectrumMapper`) pass comfortably in isolation against
the roadmap budgets, confirming the roadmap's 2026-09-28 diagnosis (contention noise under the
full suite, not a regression) and that gating `ctest` on them was the wrong mechanism — which is
what this task removes.

### Full `ctest --preset release` (acceptance criterion 4)

```
ctest --preset release --output-on-failure
```

Result: `100% tests passed, 0 tests failed out of 165`, including `pluginval` (16.29 s). No
`Investigate_Standalone` failure was hit: `ctest` runs pre-built test executables and does not
attempt to build the Standalone target, so the known link failure from T-025 (WinMain defined
twice, fixed in T-009) did not block this run. Five loudness conformance cases remain the
pre-existing, documented `(not run) (Skipped)` cases (Tech 3341 cases 6–8, Tech 3342 cases 5–6 —
surround/official-files cases out of scope per the roadmap), unrelated to this task.

## Real-time safety

No production code was touched (`source/` untouched, per CLAUDE.md ownership). Not applicable.

## Defects

None. `docs/tasks/T-025-compile-definitions.md`'s open question 2 is resolved by this task's
change, not by a code fix — the two flagged tests were correct all along; they were just gated at
the wrong level (full-suite contention instead of an isolated budget run).

## Files changed

- `tests/unit/SpectrumEngineTest.cpp` — tag only (`[.perf]` added).
- `tests/unit/SpectrumAnalyserPerformanceTest.cpp` — tag only (`[.perf]` added), both test cases.
- `tests/unit/LoudnessMeterPerformanceTest.cpp` — tag only (`[.perf]` added), both test cases.
- `tests/unit/MeterBankTest.cpp` — tag changed from plain `[perf]` to hidden `[.perf]`; assertion
  constants changed from the T-024 stretch target (10/30 µs) to the roadmap budget (15/40 µs);
  the stretch target is now printed as an informational met/missed comparison; comment rewritten
  to explain the change and cite this task.
- `docs/qa/T-026-unit.md` — this report (new).

No CMake file was touched: no change was needed, per the task's scope (hidden tags are enough for
`catch_discover_tests` to stop registering these cases).

## Open questions

None.
