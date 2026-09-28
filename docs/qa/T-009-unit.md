# T-009: Processor wiring, analysis thread and editor API — unit QA

Tester: unit-tester. Build: `cmake --build --preset release --target InvestigateUnitTests` (Release,
since the acceptance criteria's timing numbers are Release-only). Run: `ctest --preset release
--output-on-failure` for the registered suite; `./build/tests/Release/InvestigateUnitTests.exe
"[perf]"` for the hidden perf tests (`[.perf]`-tagged, excluded from `catch_discover_tests`/ctest by
design — see `MeterBankTest.cpp`'s own comment on this, T-026 Decision 2026-09-29).

## Verdict: PASS (round 2, after one test-file fix; source unchanged)

This picks up an interrupted round-1 session. Round-1 work already on disk when this run started:
`tests/unit/ProcessBlockPassThroughLightTest.cpp` fixed (missing `setBusesLayout` before
`prepareToPlay`, previously segfaulting two tests — now passing, confirmed again below). One test
was still failing at handoff: `"requestSpectrumReset() takes effect within one analysis frame"`
(`tests/unit/ProcessorResetRequestsTest.cpp`). Investigated and fixed in this round; see Defect 1
(test defect, not a source defect) below. No `source/` file was read-and-found-wrong and no
`source/` file was changed by me (unit-tester never edits `source/`, per `CLAUDE.md`).

Full suite after the fix: `ctest --preset release` → **172 registered tests, 167 run, 167 passed, 5
skipped** (pre-existing, documented EBU Tech 3341/3342 "authentic programme"/5.0-channel cases that
require optional reference WAV assets not present on this machine — unrelated to T-009, unchanged
from prior rounds), **0 failed**, including `pluginval` at strictness 10 (16.67s, PASS). Re-ran the
previously-flaky test 15/15 in isolation post-fix (see below) with 0 failures.

## Test plan (derived from the task's acceptance criteria before running anything)

1. **Bit-exact pass-through / null test**, analysis active and inactive, all T-008 layouts
   (`ProcessBlockPassThroughLightTest.cpp`, plus the heavier pre-existing pass-through tests).
2. **Real-time safety**: allocation-hook test over 10s of audio with parameter changes interleaved,
   stereo main + stereo sidechain, analysis active (`ProcessBlockPerformanceTest.cpp` /
   `MeterBankTest.cpp`'s allocation-guard cases), plus a manual audit of everything reachable from
   `processBlock` (below).
3. **processBlock timing budget**: ≤20µs mean, ≤50µs p99 per 128-sample block at 48kHz, stereo
   main+sidechain, analysis active, Release (`ProcessBlockPerformanceTest.cpp`, `[.perf]`).
4. **setAnalysisActive(true) → −6dBFS 1kHz sine → frame within 200ms**, meter peak −6.0±0.05dBFS.
5. **100× setAnalysisActive open/close** while audio runs concurrently: no crash, no deadlock,
   thread stop ≤50ms each time.
6. **Sample-rate change 44.1→96→48kHz while analysis active**: frames report the new rate within
   200ms, meters restart cleanly, no crash.
7. **requestStatsReset() / requestSpectrumReset()** take effect within one block / one analysis
   frame respectively, called from the message thread.
8. **isSidechainActive()** true only when the sidechain bus is enabled with ≥1 channel, including a
   live bus-layout change while analysis is active.
9. **Text round trip re-confirmation** for `avgTime`/`rmsWindow` at the display-bucket boundaries,
   including values strictly inside (0, 0.5)ms and the 9999ms boundary (T-008 round-2 fix,
   independently re-verified here).
10. **pluginval strictness 10**, **Standalone** link/launch/unmuted-input (plugin-validator's remit
    per the task file; not re-verified here beyond confirming `ctest`'s `pluginval` label still
    passes).

## Results

| # | Check | Test(s) | Result |
|---|---|---|---|
| 1 | Bit-exact pass-through, all layouts, analysis on/off | `ProcessBlockPassThroughLightTest.cpp` + heavier pass-through tests (#146–149 etc.) | PASS |
| 2 | Zero heap allocation, 10s audio, params changing, analysis active | `ProcessBlockPerformanceTest.cpp` #150 | PASS |
| 2b | Manual RT-safety audit of `processBlock` | see below | PASS, no findings |
| 3 | processBlock ≤20µs mean / ≤50µs p99, stereo+sidechain, analysis active | `[.perf]` | PASS — mean 8.30µs, p99 47.2µs |
| 4 | −6dBFS sine → frame within 200ms, peak −6.0±0.05dBFS | existing T-009 test | PASS |
| 5 | 100× open/close, no crash/deadlock, stop ≤50ms | test #157 | PASS (0.57s total) |
| 6 | Sample rate 44.1→96→48kHz while active | test #75 | PASS |
| 7a | requestStatsReset within one block | test #152 | PASS |
| 7b | requestSpectrumReset within one analysis frame | test #151 | PASS (after test fix, Defect 1) |
| 8 | isSidechainActive() correctness incl. live layout change | test #135 | PASS |
| 9 | avgTime/rmsWindow round trip incl. (0,0.5)ms, 9999ms | tests #124, #125, #155 | PASS |
| 10 | pluginval strictness 10 | test #172 | PASS (16.67s) |

## Defect 1 (test defect, fixed — not a source defect)

**Test**: `"requestSpectrumReset() takes effect within one analysis frame"`,
`tests/unit/ProcessorResetRequestsTest.cpp`.

**Expected** (roadmap acceptance criterion / task line 54): `requestSpectrumReset()` called from the
message thread takes effect within one analysis frame.

**Actual (before fix)**: `CHECK(sawLowerMax)` failed intermittently (reproduced 4/5 isolated runs
before the fix below, 0/15 after).

**Root cause, confirmed by reading `source/dsp/SpectrumEngine.cpp` and
`source/PluginProcessor.cpp`'s `AnalysisThread::run()`**: `SpectrumEngine::processPending()` checks
and consumes `spectrumResetRequested` (calling `resetSecondary()`) *once per call, before that
call's own FIFO drain* (`PluginProcessor.cpp` `AnalysisThread::run()`, the
`spectrumResetRequested.exchange(...)` check immediately precedes `engine.processPending(settings)`
in the loop body). `processPending()` itself drains at most `kDrainChunkFrames` (8192 samples = 16
hops at fftSize 512/1x overlap) per call and publishes only the newest frame
(`SpectrumEngine.h`/`.cpp`). The original test pushed 40 loud blocks (20480 samples = 40 hops, more
than two drain chunks) in a tight loop, then used `waitForFrame()`, which returns on the *first*
frame satisfying `hasSecondary` — that can be the frame from a *partial* first drain (e.g. the first
16 hops), while up to ~24 loud hops are still queued in the FIFO behind it. The test then
immediately called `requestSpectrumReset()` and pushed quiet blocks; the still-queued loud hops are
FIFO-ordered ahead of the quiet ones and get processed on the *next* `processPending()` call — which
is exactly the call that just consumed the reset — reviving the all-time-max secondary curve before
the quiet-tail assertion window even starts. Since the curve is a max, once revived it does not drop
again within the test's 200ms budget, so `sawLowerMax` never becomes true. This is a race in the
test's timing assumptions, not a defect in the reset wiring: `resetSecondary()`,
`requestSpectrumReset()`'s atomic hand-off, and the "checked before the next `processPending()` call"
contract all behave exactly as documented.

**Reproduction of the original bug** (for the record, not re-run after the fix since the test file
no longer contains it): `InvestigateUnitTests.exe "requestSpectrumReset() takes effect within one
analysis frame"` with the pre-fix 40-block loop, 4/5 isolated runs failed.

**Fix** (test-only, `tests/unit/ProcessorResetRequestsTest.cpp`):
1. Reduced the loud-tone push from 40 to 10 blocks (5120 samples = 10 hops), staying under the
   16-hop single-drain-chunk limit, so the entire loud backlog *can* be drained in one
   `processPending()` call.
2. Bounding the block count alone is not sufficient, because the `AnalysisThread` polls
   independently every 5ms and can race the test's push loop, draining a partial backlog on one call
   and the remainder on the next. So after the first `hasSecondary` frame, the test now polls for a
   60ms settle window (12× the 5ms poll period) and takes the running max across that window before
   treating it as final — since the all-time-max curve is monotonically non-decreasing until reset,
   this guarantees any still-in-flight backlog is included in `maxBefore` before `requestSpectrumReset()`
   is called, closing the race.

Re-verified: 15/15 isolated runs pass after the fix; full `ctest --preset release` also passes (see
Verdict). This is a fix to my own test, reported per "never weaken a test to make it pass, and say so
if a test itself was wrong": the test was wrong (a timing assumption, not the assertion strength),
and the fix makes it *more* strict about draining the backlog, not less.

## processBlock timing (roadmap Budgets, isolated `[.perf]` run)

`./build/tests/Release/InvestigateUnitTests.exe "[perf]"` (Release, stereo main + stereo sidechain,
analysis active, 128-sample blocks, 48kHz):

- **processBlock**: mean = **8.30µs** (budget ≤20µs), p99 = **47.2µs** (budget ≤50µs) — PASS, both
  criteria met, though p99 has only ~2.8µs of headroom against the 50µs budget on this machine.
- (Context, not a T-009 criterion) MeterBank-alone perf test in the same `[.perf]` run
  (`MeterBankTest.cpp`, T-007/T-024/T-026 scope) is flaky on this machine under the same load: p99
  failed 3 of 5 isolated re-runs (~47–48µs measured against a 40µs budget); mean always passes
  (~8.5µs vs 15µs budget). This is a pre-existing test, not owned by this task and not modified here.
  Flagged as an open question below since T-009's own processBlock p99 margin is similarly thin and
  the two are likely seeing the same underlying timer/scheduler noise on this machine.

## Real-time safety audit (`processBlock` and everything it reaches)

Read `InvestigateProcessor::processBlock` (`source/PluginProcessor.cpp:233-280`) and everything it
calls (`MeterBank::process`, `SpectrumEngine::pushMain`/`pushSidechain`, `SampleFifo::push`,
`isSidechainActive()`, `getBusBuffer`):

- `juce::ScopedNoDenormals` at block entry (`PluginProcessor.cpp:235`) — present.
- `getBusBuffer()` returns an aliasing view, no allocation.
- `meterBank.setRmsWindowMs()` / `.process()`, `spectrumEngine.pushMain()`/`.pushSidechain()` —
  covered by T-005/T-007's own no-allocation tests, and by `ProcessBlockPerformanceTest.cpp`'s
  10-second allocation-hook test (test #150) with parameter changes interleaved — 0 allocations
  observed.
- `analysisActive.load()` / `isSidechainActive()`'s `getBus()` — plain atomic load / bus-list read,
  no lock, no allocation.
- No `new`/`delete`/`malloc`, no container growth, no `std::string` building, no
  `juce::CriticalSection`/`ScopedLock` (the one `analysisLock` in this class is only ever taken from
  `setAnalysisActive`/`prepareToPlay`/`releaseResources`, never from `processBlock` — confirmed by
  reading every call site), no `DBG`/`std::cout`/file I/O, no thrown exceptions, no `std::function`
  construction, no system calls, no unbounded loops (the `numSamples <= 0` early-out is the only
  branch; the rest is straight-line code).
- **Finding: none.** `processBlock` and its call graph are real-time safe as implemented.

## Commands used

```
cmake --build --preset release --target InvestigateUnitTests
cmake --build --preset release
ctest --preset release --output-on-failure
./build/tests/Release/InvestigateUnitTests.exe "requestSpectrumReset() takes effect within one analysis frame"   # x15 after fix
./build/tests/Release/InvestigateUnitTests.exe "[perf]" -s
```

## Open questions

1. The `[.perf]` p99 timings for both `processBlock` (T-009, 47.2µs/50µs budget) and MeterBank-alone
   (T-007/T-024, ~47-48µs/40µs budget, flaky) sit close to or over budget on this development
   machine, while their means are comfortably under budget. This looks like tail-latency/scheduler
   noise (e.g. thread migration, timer resolution) rather than a per-block cost regression, since the
   mean is stable and only the top ~1% of samples are affected — but I have not root-caused it
   further, since that would mean profiling the OS scheduler/timer behaviour, which is outside a
   white-box DSP/logic test's remit. Routing to build-engineer or re-measuring on the actual CI
   machine before treating the MeterBank p99 budget as a hard gate seems appropriate; I have not
   touched `MeterBankTest.cpp` since it is outside this task's scope and was already passing/failing
   before this round started.
