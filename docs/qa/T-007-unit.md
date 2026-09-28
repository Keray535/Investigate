# T-007: Level, true-peak and correlation meters, MeterBank — unit QA

Tester: unit-tester. Build: `cmake --build --preset debug/release --target InvestigateUnitTests`.
Run: `./build/tests/Debug|Release/InvestigateUnitTests.exe`, and `ctest --preset debug/release --output-on-failure`.

## Round 2 verdict: PASS

Round 1's Defect 1 (below) is fixed. All 6 previously-open TruePeakDetector libebur128
cross-checks now pass, including the swept sine at 96 kHz that failed round 1. The full
pre-existing suite (T-004/005/006/007) has 0 regressions in both Debug and Release, both via
the test executable directly and via CTest (122/122, 5 documented skips). Both configurations
rebuilt warning-clean at `/W4` from a full forced rebuild. `MeterBank::process()` performance is
still inside budget in isolation. See "Round 2 re-verification" below for the full detail behind
each of the 9 points the coder asked to be re-checked.

## Round 1 verdict: FAIL (1 defect)

30 of the 31 new T-007 test cases pass, in both Debug and Release, and the full pre-existing
suite (T-004/005/006, 115 test cases outside `[performance]`) still passes with 0 regressions.
One acceptance criterion — the libebur128 true-peak cross-check on a swept sine at 96 kHz — fails
by a small, reproducible margin. See Defect 1.

## Test plan (derived from the task's acceptance criteria before running anything)

1. **LevelMeter** (`tests/unit/LevelMeterTest.cpp`): 50 ms display-peak visibility for a single
   full-scale click in a 128-sample block; peakMax holds until `resetStatistics()`, `reset()`
   clears everything including RMS; RMS of a −6 dBFS sine at rmsWindow ∈ {50, 300, 3000} ms and
   sampleRate ∈ {44.1k, 192k} reads −9.03 ±0.05 dB after window×1.1; RMS shows no drift over 1
   simulated hour at 48 kHz; exact sample-clip count on a signal with a known number of |x|≥1.0
   samples; no NaN/Inf for silence, DC, full-scale noise, denormal (1e-40); block-size/sample-rate
   matrix invariance (44.1/48/88.2/96/192 kHz × block sizes 1/17/64/512/4096); `prepare()`
   repeatability.
2. **TruePeakDetector** (`tests/unit/TruePeakDetectorTest.cpp`): the task's own fs/4, 45°,
   amplitude-1.0 case (−3.01 dB sample peak, 0.0 dBTP); EBU Tech 3341 cases 15–19 at 48 kHz
   (+0.2/−0.4 dB); a documented, non-blocking exploration of cases 20–23 (see Open question 1);
   true-peak clip counter at ±0.5 dBTP around the fs/4/45° case; libebur128 `EBUR128_MODE_TRUE_PEAK`
   cross-check on 60 s full-scale pink noise, a 20 Hz–20 kHz swept sine, and the fs/4/45° case, at
   44.1/48/96 kHz within ±0.2 dB; 1x (176.4/192 kHz) reports the sample peak directly; no NaN/Inf
   across silence/DC/noise/denormal at all four oversample-factor sample rates.
3. **CorrelationMeter** (`tests/unit/CorrelationMeterTest.cpp`): L=R → +1.00, L=−R → −1.00;
   independent white noise → |r| ≤ 0.05 after 1 s; silence → 0.00, never NaN; before any data →
   0.00, never NaN; 90° phase-shifted sine pair → 0.00 ±0.02; L=R→L=−R switch crosses 0 at
   250 ±50 ms and reaches ≤ −0.95 by 600 ms; balance for −6/−12 dBFS calibrated-RMS noise →
   −6.0 ±0.2 dB after 3 s, equal levels → 0.0 ±0.2 dB, silence → 0.0, never NaN; mono short-circuit
   (correlation +1, balance 0, `isMono`); no NaN/Inf for DC/noise/denormal.
4. **MeterBank** (`tests/unit/MeterBankTest.cpp`): `blockCounter` increments by exactly 1 per
   `process()`; `prepare()` resets `blockCounter` and the published snapshot; `requestReset()`
   issued from a second thread takes effect at the next `process()` call; a concurrent stress test
   (writer thread hammering `process()` at block size 1, reader thread hammering `getSnapshot()`
   for 1.5 s, with `requestReset()` interleaved) never observes a NaN or out-of-range field; no
   heap allocation in `process()`/`setRmsWindowMs()`/`requestReset()`/`getSnapshot()`
   (`ScopedAllocGuard`, T-004's link-wide `operator new` override); performance ≤15 µs
   mean/≤40 µs p99 per 128-sample stereo block at 48 kHz in Release, tagged `[performance]`; no
   NaN/Inf in the full snapshot for silence/DC/noise/denormal; `prepare()` repeatability and
   `reset()`+same-input determinism.
5. **Real-time safety audit**: read every line of `LevelMeter`, `TruePeakDetector`,
   `CorrelationMeter`, `MeterBank`, `MeterSnapshot`, `PeakHoldRing`, `DecibelUtils`, and the new
   `LoudnessMeter::resetStatistics()`, for allocation, container growth, locks, I/O, exceptions,
   `std::function`, and system calls.
6. **Build cleanliness**: forced rebuild (touch + rebuild) of Debug and Release, grep for warnings.

## Results

| # | Test case | Result |
|---|---|---|
| 1 | LevelMeter: single click visible ≥50ms in samplePeak | PASS |
| 2 | LevelMeter: peakMax holds until resetStatistics(); reset() clears all | PASS |
| 3 | LevelMeter: RMS −9.03±0.05dB, 50/300/3000ms × 44.1/192kHz | PASS (6 combinations) |
| 4 | LevelMeter: RMS no drift after 1 simulated hour | PASS |
| 5 | LevelMeter: exact sample clip count | PASS |
| 6 | LevelMeter: no NaN/Inf (silence/DC/noise/denormal) | PASS |
| 7 | LevelMeter: block-size/sample-rate matrix invariance | PASS (25 combinations) |
| 8 | LevelMeter: prepare() repeatable | PASS |
| 9 | TruePeakDetector: task's fs/4 45° case (−3.01dB / 0.0dBTP) | PASS |
| 10 | TruePeakDetector: EBU cases 15–19 at 48kHz | PASS |
| 11 | TruePeakDetector: cases 20-23 exploration | Not a pass/fail gate — see Open question 1 |
| 12 | TruePeakDetector: true-peak clip counter (±0.5dBTP) | PASS |
| 13 | TruePeakDetector: libebur128 cross-check, pink noise, 44.1/48/96kHz | PASS |
| 14 | TruePeakDetector: libebur128 cross-check, swept sine, 44.1/48kHz | PASS |
| 15 | TruePeakDetector: libebur128 cross-check, swept sine, **96kHz** | **FAIL — Defect 1** |
| 16 | TruePeakDetector: libebur128 cross-check, fs/4 45° case, 44.1/48/96kHz | PASS |
| 17 | TruePeakDetector: 1x at 176.4/192kHz = sample peak | PASS |
| 18 | TruePeakDetector: no NaN/Inf, 4 sample rates × 4 signal types | PASS |
| 19 | CorrelationMeter: L=R / L=−R | PASS |
| 20 | CorrelationMeter: independent noise |r|≤0.05 | PASS |
| 21 | CorrelationMeter: silence, before-any-data | PASS |
| 22 | CorrelationMeter: 90° phase shift | PASS |
| 23 | CorrelationMeter: L=R→L=−R crossing timing | PASS |
| 24 | CorrelationMeter: balance (asymmetric/equal/silence) | PASS |
| 25 | CorrelationMeter: mono short-circuit | PASS |
| 26 | CorrelationMeter: no NaN/Inf | PASS |
| 27 | MeterBank: blockCounter += 1 per process() | PASS |
| 28 | MeterBank: prepare() resets blockCounter/snapshot | PASS |
| 29 | MeterBank: cross-thread requestReset() | PASS |
| 30 | MeterBank: concurrent stress (process()×getSnapshot()×requestReset()) | PASS |
| 31 | MeterBank: no allocation in process() | PASS |
| 32 | MeterBank: performance ≤15µs mean/≤40µs p99 | PASS in isolation (5.43/29.3µs); noisy under full-suite load — see note below |
| 33 | MeterBank: no NaN/Inf, full snapshot | PASS |
| 34 | MeterBank: prepare() repeatable, reset()+same input deterministic | PASS |
| 35 | Full pre-existing suite (T-004/005/006), `~[performance]` | PASS, 0 regressions (115 cases, 5 documented skips) |
| 36 | Warning-clean /W4, Debug and Release, forced rebuild | PASS (0 warnings) |

**Performance-test noise**: running the *entire* suite together (151k+ assertions, including
T-004/005/006's own performance tests) makes `MeterBank::process()`'s timing noisy — one run
measured 19.99 µs mean / 107.6 µs p99 against the 15/40 µs budget, and the pre-existing T-006
`LoudnessMeter` performance test failed the same way in the same run (5.13 µs against its own
5 µs budget). Run in isolation (`InvestigateUnitTests.exe "[performance][T-007]"`), MeterBank
measured 5.43 µs mean / 29.3 µs p99, comfortably inside budget. This matches the roadmap's own
2026-09-28 Decision ("Performance assertions inside the unit suite are noisy when the full suite
runs... unit-tester tags them `[perf]`... CI excludes that tag from the pass/fail gate") — the new
test is tagged `[performance]` for the same reason and is not counted as a failure here.

## Defects

**Defect 1 — TruePeakDetector's 2x-oversampling (96 kHz) path misses the libebur128 cross-check
tolerance on a swept sine.**
- Expected (task acceptance criteria, T-007): "on ... a swept sine from 20 Hz to 20 kHz ... at
  44.1, 48 and 96 kHz, the true-peak max agrees with libebur128's `EBUR128_MODE_TRUE_PEAK` result
  within ±0.2 dB."
- Actual: at 96 kHz, `TruePeakDetector::getTruePeakMaxDb()` reads −0.6952 dBTP against
  libebur128's −0.9133 dBTP — a 0.2180 dB gap, just outside the ±0.2 dB budget. 44.1 kHz and
  48 kHz (both 4x oversampling) pass for the same swept sine, and 96 kHz passes for the other two
  signal classes (pink noise, the fs/4/45° case) — the miss is specific to 2x oversampling
  combined with wideband/sweeping content.
- Reproduce: `InvestigateUnitTests.exe "TruePeakDetector: libebur128 true-peak cross-check*" -c "sampleRate" -c 96000.0`,
  or run the whole `[ebur128]`-tagged section in `tests/unit/TruePeakDetectorTest.cpp` (the
  "swept sine 20Hz-20kHz" `SECTION`, line ~246). Deterministic (no RNG involved) — reproduces
  every run.
- Suspected location: `source/dsp/TruePeakDetector.cpp`, the 2x-oversampling path (`oversampleFactor
  == 2`, using `kPhaseIndices2x = {0, 2}` from `TruePeakDetector.h` lines 39-44 / `.cpp` lines
  39-44) — the documented engineering choice of reusing phases 0 and 2 from the 4x table, rather
  than a filter designed for 2x. `docs/research/T-007-true-peak-coefficients.md` itself flags this
  as "a reasonable, standard-consistent engineering choice, but ... your own design decision, not
  an ITU-specified filter" — this result suggests that choice is close to, but not quite within,
  the task's ±0.2 dB budget for non-stationary/wideband content specifically. Suggest either: (a)
  checking whether libebur128's own 2x implementation uses a different phase pair or a dedicated
  2x filter (its header comment only documents the oversampling *factors*, not its filter design),
  or (b) using all 4 phases at 2x (interpolate at 4x internally, decimate the reported points, or
  simply keep 4x oversampling up to 96 kHz) if a tighter margin is wanted, trading the extra CPU
  cost against the current comfortable performance budget (MeterBank runs at ~5.4 µs mean against
  a 15 µs budget, so there is headroom).
- Not fixed by this tester (white-box QA only fixes tests, never `source/`).

## Real-time safety audit

Read every line reachable from `MeterBank::process()`: `LevelMeter::process`, `TruePeakDetector::process`,
`LoudnessMeter::process` (including the new `resetStatistics()`), `CorrelationMeter::process`,
`PeakHoldRing::push`/`currentMax`, `Decibels::linearToDb`, and `MeterBank::publishSnapshot`/`getSnapshot`.

No `new`/`delete`/`malloc`, no container growth (`push_back`/`resize`/`std::string` building — all
containers are fixed-size `std::array`), no locks (`std::mutex`/`juce::CriticalSection`/`ScopedLock`),
no I/O or logging (`DBG`/`std::cout`/file access), no `std::function` construction, no thrown
exceptions, no unbounded loops (every loop is bounded by `numSamples` or a fixed ring size — 5,
300 or 12 elements), no system calls. `MeterBank::getSnapshot()`'s seqlock retry loop is the one
loop not bounded by a compile-time constant, but it runs only on non-audio-thread callers per the
class's own documentation and the task's explicit allowance ("the reader runs on a non-real-time
thread, so retrying is acceptable there") — confirmed by the concurrent stress test (#30 above)
completing in bounded time (1.5 s wall-clock) with no observed torn read across roughly 10⁷
writer iterations (block size 1) racing the reader for the full duration.

`MeterBank::publishSnapshot()`'s seqlock (odd version = write in progress, even = stable,
acquire/release-fenced) was specifically stress-tested per the task's request: a writer thread
called `process()` continuously at block size 1 (the fastest possible writer cadence) while a
reader thread called `getSnapshot()` continuously for 1.5 s, with `requestReset()` interleaved
every 10,000 samples from the writer thread itself. No NaN, no correlation outside [−1.0001, 1.0001],
and no dB field above a generous +20 dB sanity bound was ever observed. No sanitizer (ASan/TSan) is
configured in this project's CMake presets yet (T-014, milestone M4, not started) — flagged as
Open question 2 below rather than skipped silently.

## Coverage

No coverage preset exists yet (OpenCppCoverage/llvm-cov are T-014/M4 scope). Informally: every
public method of `LevelMeter`, `TruePeakDetector`, `CorrelationMeter`, `MeterBank` and the new
`LoudnessMeter::resetStatistics()` is exercised by at least one test above, including every branch
in `TruePeakDetector::process()` (oversampleFactor ∈ {1, 2, 4}) and every reset path
(`reset()` vs `resetStatistics()`, full-bank `requestReset()` vs per-meter).

## Commands used

```
cmake --build --preset debug --target InvestigateUnitTests
cmake --build --preset release --target InvestigateUnitTests
./build/tests/Debug/InvestigateUnitTests.exe "[T-007]" --reporter console
./build/tests/Release/InvestigateUnitTests.exe "[T-007]" --reporter console
./build/tests/Release/InvestigateUnitTests.exe "[performance][T-007]" -s --reporter console
./build/tests/Release/InvestigateUnitTests.exe "~[performance]" --reporter console   # full suite, regression check
touch tests/unit/*.cpp tests/unit/MeterTestUtils.h && cmake --build --preset debug/release --target InvestigateUnitTests   # /W4 forced-rebuild check
```

## Open questions

1. **EBU Tech 3341 cases 20–23 are not covered by a pass/fail test.** Two synthesis attempts
   (documented in a comment block in `tests/unit/TruePeakDetectorTest.cpp`, where the withdrawn
   `TEST_CASE` used to be) were tried and withdrawn: comparing an isolated single-cycle fs/4 burst
   against the literal 0 dBTP expected value missed by 1–1.5 dB (because a single isolated cycle
   is spectrally wide and a realistic anti-alias filter removes some of its own energy before
   decimation — an artefact of that synthesis, not of `TruePeakDetector`); a self-referential
   version (comparing against the same burst's own post-filter peak instead of the literal EBU
   value) still missed at one of the four offsets and had too much spread across offsets to trust
   as a gate. Reproducing the literal EBU signal (a fs/4 cycle spliced with phase continuity into
   a fs/6 background, exactly as the standard describes) was judged out of scope for this round —
   `docs/research/T-006-ebu-test-cases.md` itself calls this "non-trivial to synthesize precisely."
   Cases 15–19 (all passing) and the libebur128 cross-checks exercise the same interpolation
   machinery. Recommend either accepting this gap (cases 15–19 plus libebur128 give reasonable
   confidence) or a follow-up task to build a literal cases 20–23 synthesis, ideally cross-checked
   against another reference true-peak implementation (e.g. ffmpeg's `ebur128` filter, which
   likely carries this exact case in its own test suite) rather than hand-derived expected values.
2. No ASan/UBSan preset exists yet in this project (T-014 is M4, not started). The seqlock stress
   test above (#30) is the best available substitute given current tooling; recommend re-running
   it under ASan/TSan once T-014 lands, as CLAUDE.md's real-time safety rules ask for.
3. RMS/level-meter/correlation tests use CPU-time simulation (feeding pre-generated buffers through
   `process()`) rather than a real audio device or wall-clock timing for anything but the
   `[performance]`-tagged tests, per CLAUDE.md's "no wall-clock dependence" rule — flagging only
   for completeness, not as a gap.

## Round 2 re-verification (2026-09-28)

Scope: dsp-engineer's fix to `TruePeakDetector`'s 88.2/96 kHz tier (a new independently-derived
Hann-windowed-sinc filter, taps=49/factor=2, replacing the reused-ITU-phases-0+2 design; the
shared delay line grew from 12 to 25 samples, kTapsPerPhase, with the 44.1/48 kHz ITU table
zero-padded to match). Read the full updated class comment in `source/dsp/TruePeakDetector.h` and
both new log entries in `docs/tasks/T-007-meters-meterbank.md` in full before re-testing, and read
`source/dsp/TruePeakDetector.cpp`'s new coefficient tables (`kPhaseCoeffs`, `kHannSincPhaseCoeffs`)
and `process()` end to end. No test files needed to change: the existing
`tests/unit/TruePeakDetectorTest.cpp` libebur128 cross-check test already runs at all three
sample rates (44.1/48/96 kHz via `GENERATE`) and all three signal classes (pink noise, swept sine,
fs/4 45°), so round 1's existing test coverage was sufficient to re-verify the fix — no new test
cases were needed for this round, only re-running the existing ones.

| # | Re-verification item | Result |
|---|---|---|
| 1 | Swept sine 20Hz-20kHz cross-check at 96kHz (the originally-failing case) | **PASS** — own=-0.913259, ref=-0.913259 (matches coder's reported figures exactly) |
| 2 | fs/4 45° cross-check at 96kHz (broken by the coder's first attempted fix) | **PASS** — own=0.104338, ref=0.104338 (matches coder's reported figures exactly) |
| 3 | Pink noise cross-check at 96kHz | **PASS** — own=0.213617, ref=0.213617 |
| 4 | EBU Tech 3341 cases 15-19 at 48kHz | **PASS** — 10/10 assertions, all within +0.2/-0.4dB |
| 5 | Swept sine and fs/4 cross-checks at 44.1kHz and 48kHz | **PASS** — 44.1kHz: own=0.181126 vs ref=0.252428 (swept sine), own=0.0826488 vs ref=0.103525 (fs/4); 48kHz and all other combinations likewise within ±0.2dB. Full cross-check test: 27/27 assertions pass across all 3 sample rates × 3 signal classes. |
| 6 | True-peak clip counter tests | **PASS** — 3/3 assertions (+0.5dBTP: count ≥1; -0.5dBTP: count == 0) |
| 7 | `MeterBank::process()` performance, isolated | **PASS** — 3 consecutive isolated runs: 8.93/33.9us, 9.17/36.8us, 10.56/38.8us (mean/p99), all inside the 15/40us budget. Matches the coder's reported 8.92/32.9us within run-to-run noise. p99 margin against the 40us budget is thin (as low as 1.2us headroom in one run) but did not breach it in 3 runs. Confirmed the same full-suite-contention pattern documented for T-006: running only the `[T-007]` tag together (31 test cases, not even the full 151k-assertion suite) pushed the same performance test to mean=33.1us/p99=119.9us — worse contention than round 1's reported full-suite number, consistent with T-006's precedent that this metric is only meaningful in full isolation (`[performance][T-007]` alone) and is excluded from the pass/fail gate otherwise, per the roadmap's existing Decision. |
| 8 | Warning-clean /W4, Debug and Release, forced rebuild | **PASS** — touched every file in `source/dsp/` and `tests/unit/` and rebuilt both configurations from scratch; 0 warnings, 0 errors in either. |
| 9 | Full previously-existing suite (T-004/005/006/007), all 4 new test files | **PASS, 0 regressions** — Release: 115 test cases, 110 passed, 5 documented skips, 151804/151804 assertions passed. Debug: identical (115 cases, 110 passed, 5 skips, 151804/151804 assertions). CTest (`ctest --preset release`): 122/122 tests passed (117 executed + 5 skipped), including the `pluginval` label. |

No new defects found. No change to `LevelMeter`, `CorrelationMeter`, `MeterBank`'s own code, or
the seqlock/reset paths, so the round 1 scope note (correlation/level/concurrency tests not
re-run) stands — nothing in this round gave reason to suspect them.

### Commands used (round 2)

```
cmake --build --preset debug --target InvestigateUnitTests
cmake --build --preset release --target InvestigateUnitTests
./build/tests/Release/InvestigateUnitTests.exe "TruePeakDetector: libebur128 true-peak cross-check within +-0.2dB" --reporter console -s
./build/tests/Release/InvestigateUnitTests.exe "TruePeakDetector: EBU Tech 3341 true-peak cases 15-19 at 48kHz" --reporter console
./build/tests/Release/InvestigateUnitTests.exe "TruePeakDetector: true-peak clip counter" --reporter console
./build/tests/Release/InvestigateUnitTests.exe "[performance][T-007]" --reporter console -s   # x3 runs, isolated
./build/tests/Release/InvestigateUnitTests.exe "[T-007]" --reporter console                    # all T-007 cases together, for contention comparison
./build/tests/Release/InvestigateUnitTests.exe "~[performance]" --reporter console             # full suite regression check
./build/tests/Debug/InvestigateUnitTests.exe "~[performance]" --reporter console               # full suite regression check, Debug
ctest --preset release --output-on-failure
touch source/dsp/*.h source/dsp/*.cpp tests/unit/*.cpp && cmake --build --preset debug/release --target InvestigateUnitTests   # /W4 forced-rebuild check
```

### Open questions (round 2)

None new. Round 1's open questions (EBU cases 20-23 synthesis gap, no ASan/TSan preset yet) are
unchanged by this fix and still stand as recorded above.
