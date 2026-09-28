# T-004: SampleFifo, WindowFunctions, RealFft, SpectrumAnalyser - unit test report

- Tester: unit-tester
- Round: 2 of 3
- Verdict: **PASS**, 0 open defects

## Round 2 (this round)

dsp-engineer fixed defect #1 from round 1 rather than documenting it: `SampleFifo::prepare()` (`source/dsp/SampleFifo.cpp`) now sizes the underlying `juce::AbstractFifo` and each channel's storage at `capacityFrames + 1`, absorbing `AbstractFifo`'s internal one-slot reservation. `getCapacity()` is now exactly honest: an empty, freshly-prepared `SampleFifo` can accept exactly `getCapacity()` frames in one `push()`.

This invalidated the expected results of two of my own round-1 tests in `tests/unit/SampleFifoTest.cpp`, which had encoded the *old* (buggy) off-by-one behaviour as their assertions:

- `"SampleFifo::getCapacity() overstates usable capacity by one frame"` - repurposed into `"SampleFifo::getCapacity() is exactly honest (regression: one-off-by-one fix)"`. It now asserts that a push of exactly `getCapacity()` frames into an empty FIFO succeeds (`overflowCount` stays 0, `getNumReady() == getCapacity()`), and that the FIFO is then genuinely full (a further 1-frame push overflows). This is now a regression test for the fixed defect rather than documentation of it.
- `"SampleFifo overflow counter increments exactly on a full-buffer drop"` - the `kUsableCapacity = kMinCapacity - 1` constant it (and two other test cases) depended on was removed; all four call sites now use `kMinCapacity` directly, since usable capacity equals the advertised capacity after the fix. The fill/drop/fits arithmetic was shifted by one frame accordingly (fill to `kMinCapacity - 1000`, drop a 2000-frame push, then a 1000-frame push exactly fills to `kMinCapacity`, then a 1-frame push overflows).
- `"SampleFifo::pop never returns partially-written or reordered data after a drop"` - also used `kUsableCapacity`; updated to fill to `kMinCapacity` (was `kMinCapacity - 1`) before the intruder push and the pop-back check.
- `"SampleFifo::push performs no heap allocation"` and the SPSC stress test do not depend on the exact capacity boundary and needed no changes.

No other production code, test intent, or acceptance-criterion coverage changed. `getCapacity()`'s own contract (the value it returns) is unchanged; only the internal buffer sizing and, correspondingly, the boundary value used in these tests changed.

### Re-verification of acceptance criterion 12

"The overflow counter increments exactly when the consumer is paused and the FIFO is full" - re-verified under the corrected contract by the two regression tests above: `getOverflowCount()` stays at 0 for every push that fits in remaining free space (including a push of exactly the remaining free space, and a push of exactly `getCapacity()` into an empty FIFO), and increments by exactly 1 for every push that does not fit, whether the FIFO is exactly full or already full. No partial writes and no reordering, re-confirmed by `"SampleFifo::pop never returns partially-written or reordered data after a drop"` and the SPSC stress test (480,000 simulated frames, order and cross-channel lockstep both checked). **PASS.**

### Build and test results, round 2

- Rebuilt `Investigate` and `InvestigateUnitTests`, Debug and Release, via `cmake --build --preset debug|release --target InvestigateUnitTests`: 0 warnings at `/W4` in both configurations (only files touched: `SampleFifo.cpp` (dsp-engineer, production) and `SampleFifoTest.cpp` (this round, tests) actually recompiled).
- `[T-004]`-tagged tests, Debug, seed 12345: 33 test cases, 141,648 assertions, all passed (performance tests report their Debug measurement as informational, budget not enforced in Debug, matching round-1 behaviour).
- Full suite, Release, seed 12345: **34 test cases, 142,674 assertions, 0 failures** (one more assertion than round 1's 142,673 total, from the regression test's extra `REQUIRE`s; not a behavioural change elsewhere).
- `[fifo]`-tagged tests only, Release, 3 extra seeds (1, 2, 3): 5 test cases, 131,092 assertions each run, all passed, no flakiness.
- Full `ctest --preset release --output-on-failure`: **35/35 tests passed** (34 Catch2 test cases registered individually via `catch_discover_tests` + `pluginval`, 21.38 s total).
- FFT accuracy, averaging, channel-mode and performance test results are unchanged from round 1 (not re-derived this round, per the coder's request - nothing in the `SampleFifo` buffer-sizing fix touches `SpectrumAnalyser`, `WindowFunctions`, or `RealFft`/`PffftRealFft`/`JuceRealFft`, and none of those files were rebuilt as a result of this change other than by being in the same target).

### Exact commands used, round 2

```
cmake --build --preset debug   --target InvestigateUnitTests
cmake --build --preset release --target InvestigateUnitTests

./build/tests/Debug/InvestigateUnitTests.exe "[T-004]" --order rand --rng-seed 12345 -r compact
./build/tests/Release/InvestigateUnitTests.exe --order rand --rng-seed 12345 -r compact

./build/tests/Release/InvestigateUnitTests.exe "[fifo]" --order rand --rng-seed 1 -r compact
./build/tests/Release/InvestigateUnitTests.exe "[fifo]" --order rand --rng-seed 2 -r compact
./build/tests/Release/InvestigateUnitTests.exe "[fifo]" --order rand --rng-seed 3 -r compact

ctest --preset release --output-on-failure
```

### Open questions carried over from round 1 (unchanged, not blocking)

1. No ASan/UBSan CMake preset exists yet - still requested from build-engineer, deferred to T-017.
2. No coverage tooling wired in yet - still deferred to T-017/build-engineer.

Defect #1 is now closed (fixed, not just documented). No new defects found this round.

## Round 1 (original report, kept for history)

## Scope

Read: `CLAUDE.md`, `docs/tasks/T-004-spectrum-analyser.md` (Inputs, Acceptance criteria, full Log), and all of `source/dsp/{SampleFifo,WindowFunctions,RealFft,PffftRealFft,JuceRealFft,SpectrumAnalyser}.{h,cpp}`.

Classes under test: `SampleFifo`, `WindowFunctions`, `RealFft`/`PffftRealFft`/`JuceRealFft`, `SpectrumAnalyser`.

## Planned test list (derived from the task's 14 acceptance criteria before writing any code)

1. pffft vs `juce::dsp::FFT` bin-for-bin agreement, white noise, 512/4096/65536 pts.
2. -6.00 dBFS bin-centred sine, Hann/Blackman/Hi-Res, 4096 & 65536.
3. Hi-Res leakage 20 bins away, full-scale, 65536.
4. Flat-top, -6.00 dBFS, inter-bin frequencies (5 fractional offsets).
5. Hann scalloping loss at the exact inter-bin point.
6. Side (L=R), Mid (L=-R), Left/Right isolation; plus mono-forces-single-channel and dual-mode sanity, sidechain power-mean sanity.
7. Frame count formula, overlaps 1/2/4/8, several N; plus chunking-invariance (bit-exact).
8. Averaging 20 dB/avgTime decay: avgTime in {100,1000,5000} ms x SR in {44100,96000} x overlap in {2,8}.
9. Real-time max 20 dB/avgTime decay (avgTime {500,2000} ms); all-time max monotonicity + reset; all-time average of 1s tone + 1s silence.
10. Reset triggers: channelMode/fftSize/window/sampleRate (clears) vs overlap/avgTimeMs/secondary (does not clear); public `reset()` vs `resetSecondary()` semantics.
11. NaN/Inf safety: silence, full-scale white noise, DC, denormal (1e-40), across mid/lrDual/lrPowerMean, with sidechain active.
12. `SampleFifo`: SPSC stress (480,000 simulated frames), overflow exactness, allocation-free `push()`.
13. Performance budgets, Release: 4096/4x <=15ms, 65536/8x dual+sidechain <=60ms.
14. `/W4` clean for the new test files.

## Files added

- `tests/unit/SpectrumTestUtils.h` - shared signal-generation/push/dB helpers.
- `tests/unit/RealFftAgreementTest.cpp` - criterion 1.
- `tests/unit/SampleFifoTest.cpp` - criterion 12 (+ the capacity defect below), with a link-wide `operator new`/`delete` override scoped by `ScopedAllocGuard` for the allocation-free check.
- `tests/unit/SpectrumAnalyserAccuracyTest.cpp` - criteria 2-5.
- `tests/unit/SpectrumAnalyserChannelModeTest.cpp` - criterion 6.
- `tests/unit/SpectrumAnalyserFrameCountTest.cpp` - criterion 7.
- `tests/unit/SpectrumAnalyserAveragingTest.cpp` - criteria 8-10.
- `tests/unit/SpectrumAnalyserSafetyTest.cpp` - criterion 11.
- `tests/unit/SpectrumAnalyserPerformanceTest.cpp` - criterion 13.

No production code was modified.

## Results

| # | Criterion | Test case(s) | Result |
|---|---|---|---|
| 1 | pffft/JUCE FFT agreement, 1e-4 rel. power | `PffftRealFft and JuceRealFft agree bin-for-bin on white noise` (512/4096/65536) | PASS - worst observed relative error well under 1e-4 at all three sizes |
| 2 | -6 dBFS bin-centred, Hann/Blackman/Hi-Res, 4096/65536 | `Bin-centred -6 dBFS sine reads -6.00 +-0.05 dB (...)` | PASS |
| 3 | Hi-Res leakage <= -120 dB, 65536 | `Hi-Res window leakage 20 bins from a full-scale centred sine is <= -120 dB` | PASS. Peak read ~0.00 dBFS; leakage at bin+-20 well under -120 dB. Theoretical sidelobe of a single 4-term Nuttall (1981) window is ~-93 dB; Hi-Res squares it in the time domain, giving a theoretical floor of ~-186 dB (convolving the spectrum with itself) - the -120 dB acceptance bound is a float32 FFT precision limit, exactly as the task states, not the window's own floor. |
| 4 | Flat-top -6 dBFS inter-bin, +-0.02 dB | `Flat-top: -6.00 dBFS at any inter-bin frequency ...` (5 fractional offsets) | PASS |
| 5 | Hann scalloping 1.42 +-0.1 dB | `Hann scalloping loss (sine exactly between two bins) is 1.42 +-0.1 dB` | PASS |
| 6 | Side/Mid/Left/Right isolation <= -150 dB | 4 dedicated cases + mono/dual/sidechain sanity | PASS |
| 7 | Frame count formula, all overlaps | `Frame count matches floor((N-F)/H)+1 ...`, `... identical regardless of pushBlock chunking` | PASS (exact match, not just within +-1; chunking gives bit-identical spectra) |
| 8 | Averaging 20 dB/avgTime | `Averaged level falls 20 dB at t = avgTime ...` (12 combos) | PASS - measured drop matched the analytically exact prediction within 0.5 dB in every combo |
| 9 | rtMax decay, all-time max monotonic, all-time average -3.01 dB | 3 test cases | PASS |
| 10 | Reset triggers (clears / does not clear) | 5 test cases covering `setSettings` (4 clearing triggers, 3 non-clearing) and public `reset()`/`resetSecondary()` | PASS |
| 11 | No NaN/Inf | `No NaN/Inf for silence, full-scale white noise, DC and denormal-range input` (4 signals x 3 channel modes, with active stereo sidechain) + silence-floor exactness case | PASS |
| 12 | SampleFifo stress/overflow/allocation | 5 test cases (see defect #1) | PASS (after correcting my own test assumptions - see below) |
| 13 | Performance budgets | 2 test cases, Release-only hard assertion | PASS - measured 0.53 ms (budget 15 ms) for 4096/4x, 5.30 ms (budget 60 ms) for 65536/8x dual+sidechain |
| 14 | `/W4` clean | Full Debug and Release build of `InvestigateUnitTests` | PASS - 0 warnings in either configuration |

Full suite: **34 test cases (this task) + the pre-existing `SmokeTest`, 142,673 assertions, 0 failures**, Release build, ran 3x with different Catch2 RNG seeds (1, 2, 3) with no flakiness observed (the SPSC stress test and every other test passed identically each time).

## Defect #1 (minor, informational): `SampleFifo::getCapacity()` overstates usable capacity by one frame

- **Expected**: Per `SampleFifo.h`'s own documentation ("Capacity is fixed in `prepare`"; `prepare()`'s doc: "Allocates `numChannels` arrays of `capacityFrames` floats each") and the task's design requirement ("Capacity is fixed in `prepare` and must be at least 2^17 frames per channel"), a freshly-prepared, empty `SampleFifo` should be able to accept a `push()` of exactly `getCapacity()` frames without dropping it.
- **Actual**: `SampleFifo::prepare()` passes `capacityFrames` straight through to `juce::AbstractFifo::setTotalSize()`. `AbstractFifo::getFreeSpace()` is `bufferSize - getNumReady() - 1` (`third_party/juce-src/modules/juce_core/containers/juce_AbstractFifo.cpp:44`) - it reserves one slot internally to disambiguate full/empty. So the *usable* capacity of a `SampleFifo` is `capacityFrames - 1`, not `capacityFrames`. Pushing exactly `getCapacity()` frames into an empty FIFO is therefore dropped and counted as an overflow, contrary to what the header implies.
- **Reproduce**: `tests/unit/SampleFifoTest.cpp`, test case `"SampleFifo::getCapacity() overstates usable capacity by one frame"`. Command: `./build/tests/Release/InvestigateUnitTests.exe "SampleFifo::getCapacity*"`.
- **Suspected location**: `source/dsp/SampleFifo.cpp:8-19` (`SampleFifo::prepare`) and `source/dsp/SampleFifo.h:67` (`getCapacity()`).
- **Severity/impact**: Low. With `capacityFrames >= 1<<17` (131072), the real usable capacity is 131071 - one sample short of the roadmap's stated "at least 2^17 frames" risk-5 mitigation, which is functionally immaterial (a small fraction of a millisecond of audio) and does not affect correctness, real-time safety, or any T-004 acceptance criterion (none of which pin an exact capacity number). It is a genuine documentation/contract inaccuracy, though: any caller who sizes buffers or reasoning around "I can safely push up to `getCapacity()` frames into an empty FIFO" will observe an unexpected overflow at exactly that boundary. Suggested fix (either is fine, dsp-engineer's call): (a) call `fifo.setTotalSize(newCapacityFrames + 1)` in `prepare()` so `getCapacity()` becomes exactly honest, or (b) leave the behaviour and add one sentence to `SampleFifo.h`'s class comment and `getCapacity()`'s doc noting the usable capacity is `getCapacity() - 1` (matching `juce::AbstractFifo`'s own documented convention).

No other defects found.

## Real-time safety audit (pushBlock and everything it reaches)

Read `SpectrumAnalyser::pushBlock`, `produceFrame`, `computeStreamPower`, `SampleFifo::push`/`pop`, `PffftRealFft::forward`, `JuceRealFft::forward` (test-only, never called from `pushBlock`).

- **Allocation**: none found in `pushBlock`/`produceFrame`/`computeStreamPower`/`SampleFifo::push`. `PffftRealFft::forward` does a `std::memcpy` into a pre-allocated aligned buffer (`PffftRealFft.cpp:28`), no allocation. Verified for `SampleFifo::push` with a global `operator new`/`delete` counter (`SampleFifoTest.cpp`, `"SampleFifo::push performs no heap allocation"`): 0 allocations over 200 calls. `SpectrumAnalyser::pushBlock`/`produceFrame` were not instrumented with the same counter in this round (T-017 owns the full `processBlock`-reachable allocation-hook audit per its task file) but a manual read finds no `new`/`resize`/`push_back`/string-building anywhere in the call chain - all containers are pre-sized in `prepare()`/`setSettings()` (both explicitly documented and allowed to allocate).
- **Locks/waits**: none. `SampleFifo` uses only `juce::AbstractFifo`'s lock-free index pair; `SpectrumAnalyser` has no synchronisation primitives at all (single-threaded-by-contract, per its class doc).
- **I/O/logging**: none (no `DBG`, no console/file I/O) in any of the audited functions.
- **Exceptions**: `pushBlock`, `produceFrame`, `computeStreamPower`, `SampleFifo::push`/`pop` are all `noexcept`. `PffftRealFft::forward`/`JuceRealFft::forward` are `noexcept` overrides.
- **`std::function`, unbounded loops, system calls**: none found. All loops are bounded by `numFrames`, `fftSize`, or `numBins`.
- **`juce::ScopedNoDenormals`**: present at the top of `pushBlock` (`SpectrumAnalyser.cpp:247`), matching CLAUDE.md.

No real-time safety violations found in the pushBlock/produceFrame/SampleFifo::push call graph.

## Sanitizers

No ASan/UBSan preset exists in `CMakePresets.json` (only `windows-msvc` / Debug-Release-RelWithDebInfo). Per my role, this is an **open question for build-engineer**: request an MSVC `/fsanitize=address` preset (clang-cl UBSan is a separate ask and not essential for this MSVC-only project per CLAUDE.md's technical baseline). Not run this round.

## Static analysis

Not run (no `clang-tidy` config found in the repo, and MSVC `/analyze` is out of scope for this round per the task - T-017 owns the full static-analysis pass). `/W4` is clean (see criterion 14 above), which is this task's own acceptance criterion.

## Coverage

No coverage tooling (OpenCppCoverage/llvm-cov) is wired into this project's presets. Not measured this round; flagging as an open question consistent with T-017's scope (which explicitly owns "if a coverage tool exists... measure line coverage of `source/dsp/`").

## Exact commands used

```
cmake --preset windows-msvc
cmake --build --preset debug   --target InvestigateUnitTests
cmake --build --preset release --target InvestigateUnitTests

# Full T-004 suite, Debug (functional correctness; perf budget not enforced in Debug):
./build/tests/Debug/InvestigateUnitTests.exe "[T-004]" --order rand --rng-seed 12345 -r console

# Full suite, Release (perf budget enforced):
./build/tests/Release/InvestigateUnitTests.exe --order rand --rng-seed 12345 -r console

# Flakiness check, 3 extra seeds:
./build/tests/Release/InvestigateUnitTests.exe "[T-004]" --order rand --rng-seed 1 -r compact
./build/tests/Release/InvestigateUnitTests.exe "[T-004]" --order rand --rng-seed 2 -r compact
./build/tests/Release/InvestigateUnitTests.exe "[T-004]" --order rand --rng-seed 3 -r compact

# Performance numbers:
./build/tests/Release/InvestigateUnitTests.exe "[performance]" -r console -s

# Full CTest run (includes SmokeTest and pluginval):
ctest --preset release --output-on-failure
```

Results: all green (see Results table). Release perf: 0.53 ms / 15 ms budget (4096/4x), 5.30 ms / 60 ms budget (65536/8x dual+sidechain).

## Open questions

1. No ASan/UBSan CMake preset exists yet - requested from build-engineer (see Sanitizers section). Not blocking for T-004 (T-017 is the task that owns the full-suite sanitizer run).
2. No coverage tooling wired in yet - same as above, deferred to when it exists (T-017/build-engineer).
3. Defect #1 (`SampleFifo` capacity off-by-one) - routing decision (fix vs. document) is dsp-engineer's/architect's call; does not block this task's PASS verdict since it violates no acceptance criterion.
