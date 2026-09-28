# T-026: Hide timing tests from the default test run
- Owner: unit-tester
- Milestone: M2
- Status: DONE
- Depends on: T-024

## Goal
Timing assertions in the unit suite fail intermittently when the whole suite runs under CTest, because of contention between tests (seen in T-006, T-007, T-024 and T-025). The roadmap decided that timing tests must not gate the normal test run, and that budgets are enforced by dedicated runs and by T-016. Make every timing test hidden from the default run, and make its assertions use the roadmap budgets, so that `ctest` is deterministic before T-009 starts.

## Inputs
- All files in `tests/unit/`, especially the performance tests in `SpectrumAnalyserPerformanceTest.cpp`, `SpectrumEngineTest.cpp`, `LoudnessMeterPerformanceTest.cpp` and `MeterBankTest.cpp`, and any other test that asserts on time.
- The roadmap Budgets table, and the Decisions dated 2026-09-28 about `[perf]`.
- `docs/tasks/T-025-compile-definitions.md`, open question 2 (the two failing timing tests).

## Scope
- Tag every test case that asserts on elapsed time with Catch2's hidden tag `[.perf]`. That is shorthand for `[.][perf]`: hidden tests are not run by default, and `catch_discover_tests` does not register them with CTest. No CMake change is needed.
- Assertions use the roadmap budgets, not stretch targets:
  - `MeterBank::process`: mean ≤ 15 µs and p99 ≤ 40 µs;
  - `SpectrumMapper`: ≤ 0.3 ms per frame;
  - the analyser: ≤ 15 ms and ≤ 60 ms per second of audio;
  - `LoudnessMeter::process`: ≤ 5 µs mean.
  Print the measured values, and print task-level targets such as T-024's mean ≤ 10 µs and p99 ≤ 30 µs as information only, with a comparison line.
- Document in `docs/qa/T-026-unit.md` the command that runs the timing tests alone, one at a time: `InvestigateUnitTests.exe "[perf]"` from the Release output folder. Include the measured values from one isolated run.

## Acceptance criteria
- [ ] `ctest --preset release` no longer lists or runs any timing test. Count before and after; the difference equals the number of retagged tests.
- [ ] `InvestigateUnitTests.exe "[perf]"` (Release) runs exactly the retagged tests. In an isolated run they pass against the roadmap budgets.
- [ ] No non-timing test is hidden and no assertion is removed; only tags and budget constants change.
- [ ] `ctest --preset release` passes, apart from the known `Investigate_Standalone` link failure from T-025, which is not a test and is fixed in T-009. Report it if it blocks CTest.
- [ ] `docs/qa/T-026-unit.md` reports PASS, with the commands and numbers.

## Log
- 2026-09-29 unit-tester: Read CLAUDE.md, this task, the roadmap Budgets table and the Decisions
  dated 2026-09-28 ("Performance assertions inside the unit suite are noisy...") and 2026-09-29
  ("Timing tests use Catch2's hidden tag `[.perf]`..."), and T-025's open question 2. Set status
  to IN_PROGRESS.
  Searched all of `tests/unit/` for wall-clock timing assertions (`std::chrono::steady_clock`/
  `high_resolution_clock`) and found exactly 6 test cases across 4 files that gate on measured
  elapsed time: `SpectrumEngineTest.cpp` (SpectrumMapper, 1), `SpectrumAnalyserPerformanceTest.cpp`
  (analyser default + worst-case, 2), `MeterBankTest.cpp` (MeterBank::process, 1) and
  `LoudnessMeterPerformanceTest.cpp` (LoudnessMeter::process mean + denormal-safety, 2). Ruled out
  `SpectrumAnalyserAveragingTest.cpp`, `LevelMeterTest.cpp`, `CorrelationMeterTest.cpp` and
  `SampleFifoTest.cpp`: they use simulated sample-count-derived time or bound a stress test's
  duration, not a pass/fail wall-clock budget.
  Added Catch2's hidden tag `[.perf]` to all 6. One of the 6 (`MeterBankTest.cpp`) already had a
  plain, non-hidden `[perf]` tag from T-024, so it was still being registered by
  `catch_discover_tests`; retagging it closes that gap. For that same test, changed the asserted
  constants from T-024's stretch target (mean ≤ 10 µs, p99 ≤ 30 µs) to the roadmap Budgets table
  value (mean ≤ 15 µs, p99 ≤ 40 µs), per this task's scope; the stretch target is still measured
  and printed as an informational met/missed line. The other 5 test cases already asserted the
  roadmap numbers (0.3 ms, 15 ms, 60 ms, 5 µs mean, and the denormal test's relative check with no
  fixed roadmap number), so only their tags changed.
  Built only `InvestigateUnitTests` (Release), per this task's scope; did not touch
  `Investigate_Standalone`, which stays known-broken per T-025 until T-009.
  Measured before/after: `ctest --preset release -N` went from 171 to 165 registered tests
  (difference of 6, matching the retag count); `InvestigateUnitTests.exe --list-tests` went from
  170 to 164 default test cases; `InvestigateUnitTests.exe "[perf]"` selects exactly the 6 retagged
  cases (Catch2 matches a hidden tag by its name regardless of the leading dot).
  Ran `InvestigateUnitTests.exe "[perf]" -s` (Release, isolated): all 6 pass against the roadmap
  budgets — SpectrumMapper 0.158 ms (≤0.3 ms), analyser default 0.585 ms (≤15 ms), analyser
  worst-case 4.16 ms (≤60 ms), MeterBank mean 5.37 µs / p99 29.8 µs (≤15/40 µs; T-024 stretch
  target 10/30 µs also met, printed as info), LoudnessMeter mean 1.53 µs (≤5 µs), and the denormal
  check (16.6 µs vs 15.8 µs normal, well under the 10x+5 margin). This resolves T-025's open
  question 2: both tests it flagged as flaky under full-suite contention pass comfortably in
  isolation, confirming the roadmap's contention diagnosis.
  Ran `ctest --preset release --output-on-failure`: 165/165 passed (100%), including `pluginval`
  at strictness 10; the pre-existing 5 skipped EBU conformance cases (surround/official-files,
  out of scope) are unrelated. `Investigate_Standalone`'s known link failure did not block this
  run: `ctest` runs pre-built test executables and never tried to build that target.
  Wrote `docs/qa/T-026-unit.md` (PASS) with the full before/after counts, the retag table, the
  isolated-run command and measured numbers, and the full-suite result. No file under `source/`
  or any CMake file was touched. Setting status to DONE.
