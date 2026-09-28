# T-005: SpectrumMapper, FrameExchange, SpectrumEngine - unit test report

- Tester: unit-tester
- Round: 1 of 3
- Verdict: **PASS**, 0 defects

## Scope

Read: `CLAUDE.md`, `docs/tasks/T-005-spectrum-mapper-engine.md` (Inputs, Acceptance criteria, full Log), `docs/roadmap.md` (Architecture: signal flow, modules table, "SpectrumFrame contents", parameter table and constraints, threading model; Budgets), `docs/research/T-002-dsp-references.md` sections 2 and 7, `docs/qa/T-004-unit.md`, and all of `source/dsp/{DisplayRange.h, SpectrumSettings.h, SpectrumFrame.h, SpectrumMapper.h/.cpp, FrameExchange.h/.cpp, SpectrumEngine.h/.cpp}`, plus the T-004 `SpectrumAnalyser.h`/`SampleFifo.h` APIs these build on.

Classes under test: `DisplayRange` (free functions), `SpectrumSettings`, `SpectrumFrame`, `SpectrumMapper`, `FrameExchange`, `SpectrumEngine`.

## Planned test list (derived from the task's 10 acceptance criteria before writing any code)

1. `DisplayRange::effectiveFreqHigh/effectiveFreqLow/effectiveDbHigh/effectiveDbLow`: each roadmap formula, including the clamped and unclamped branches.
2. AC1: -6 dBFS sine, 20 Hz..20 kHz sweep, Hann/4096/2048 points/slope 0/smoothing off, via a real `SpectrumAnalyser` -> `SpectrumMapper` pipeline; peak point in [-7.5, -6.0] dB.
3. AC2: low-frequency interpolated points never exceed `max(neighbour bins) + 0.1 dB`, with random bin power and 3 RNG seeds, in a parameter regime that forces the interpolation branch across the whole range.
4. AC3: slope +4.5 dB/oct on a flat (constant-power) spectrum; exact grid alignment (freqLow=250, freqHigh=32000, numPoints=449 -> 1/64-octave steps) puts display points exactly on 250/1000/2000 Hz so the ±0.01 dB tolerance is met by construction, not chance; deltas checked against the analytic `slope * log2(f/1000)` formula.
5. AC4: 1/3-octave smoothing of a single-point impulse spreads it over a measured width (in octaves, from the actual output) within ±10% of 1/3 octave; smoothing Off reproduces the unsmoothed bin value exactly at the impulse point and stays unspread elsewhere.
6. AC5: `SpectrumEngine::processPending()` with `freqLow`/`freqHigh`/`numPoints` alternated between calls (silence pushed throughout, `secondary = allTimeMax`) - the all-time-max secondary curve retains its earlier peak, and the primary average continues its analytically-predicted exponential decay (no re-priming) rather than jumping to a near-floor value.
7. AC6: `FrameExchange` stress test with real producer/consumer `std::thread`s, 200,000 publishes, checking every field against a per-sequence pattern (torn-frame detection), monotonic non-decreasing sequence, and zero allocation (shared `ScopedAllocGuard`, extracted to `AllocGuard.h` so both `SampleFifoTest.cpp` and the new files can use it without a duplicate global `operator new`). Plus basic publish/read semantics (empty read, multi-publish coalescing, sequence monotonicity single-threaded).
8. AC7: `SpectrumEngine` producer/consumer simulation - 30 s of 48 kHz audio in 128-sample blocks, `processPending()` called every 2 blocks (~5.33 ms, close to the roadmap's ~5 ms poll), sample-count/block-count bounded rather than wall-clock; 0 overflows and the final frame's peak reflects the injected -6 dBFS 1 kHz tone.
9. AC8: `SpectrumMapper::mapCurve` performance, 2048 points x 3 curves, worst-case bin count (65536-point FFT), Release-only hard budget of ≤0.3 ms/frame (Debug measures but does not enforce, matching the T-004 pattern).
10. AC9: no NaN/Inf for silence, full-scale/DC-like, denormal-range (1e-40) and very-large bin-power inputs, both at the `SpectrumMapper` level (every smoothing mode x 4 slope values) and end-to-end through `SpectrumEngine` (dual + sidechain, silence/DC/full-scale-noise/denormal).
11. AC10: `/W4` clean for the new test files, Debug and Release.

## Files added

- `tests/unit/DisplayRangeTest.cpp` - the four `DisplayRange.h` formulas.
- `tests/unit/SpectrumMapperTest.cpp` - AC1-AC4 and mapper-level AC9.
- `tests/unit/FrameExchangeTest.cpp` - AC6 plus basic publish/read semantics.
- `tests/unit/SpectrumEngineTest.cpp` - AC5, AC7, AC8, and engine-level AC9.
- `tests/unit/AllocGuard.h` - **new shared header**, extracted from `SampleFifoTest.cpp`'s existing allocation-guard (see "Refactor" below).

## Refactor of `tests/unit/SampleFifoTest.cpp` (T-004 file, within unit-tester's ownership of `tests/unit/`)

`SampleFifoTest.cpp` (T-004, round 2) already installs a link-wide `operator new`/`delete` override behind a `ScopedAllocGuard` to check `SampleFifo::push()` for zero allocation. `FrameExchange`'s AC6 needed the same facility. A second definition of the global `operator new`/`delete` in a new file would be a multiple-definition link error, so I moved `gAllocCount`, `gAllocGuardArmed` and `ScopedAllocGuard` out of `SampleFifoTest.cpp`'s anonymous namespace into `tests/unit/AllocGuard.h` (namespace `investigate::test`, external linkage), and `SampleFifoTest.cpp` now includes that header instead of defining its own copies. The `operator new`/`delete` overloads themselves stay defined exactly once, in `SampleFifoTest.cpp`. No test behaviour changed; re-ran the full T-004 suite (Release, seed 12345) after the refactor and it is still 34 test cases / 142,674+ assertions, 0 failures, including `"SampleFifo::push performs no heap allocation"` specifically. This is a refactor of test-only code within my own ownership (`tests/unit/`), not a change to `source/dsp/`.

## Results

| # | Criterion | Test case(s) | Result |
|---|---|---|---|
| DisplayRange | 4 formulas, clamped/unclamped | 4 test cases in `DisplayRangeTest.cpp` | PASS |
| AC1 | -6 dBFS peak in [-7.5, -6.0] dB, 20 Hz..20 kHz | `SpectrumMapper: -6 dBFS sine peak reads -6.0..-7.5 dB, 20 Hz..20 kHz` (14 frequencies via `GENERATE`, including the 20 Hz and 19980 Hz edges) | PASS |
| AC2 | Interpolated point ≤ max(neighbour bins) + 0.1 dB | `SpectrumMapper: interpolated points never exceed max(neighbour bins) + 0.1 dB` (2048 points x 3 RNG seeds = 6144 checks) | PASS |
| AC3 | Slope +4.5 dB/oct: 1 kHz unchanged, 2 kHz +4.5, 250 Hz -9.0, all ±0.01 dB | `SpectrumMapper: slope +4.5 dB/oct matches the analytic dB delta exactly on a flat spectrum` | PASS - measured deltas 0.0000/+4.5000/-9.0000 dB (double precision, effectively exact) |
| AC4 | 1/3-oct smoothing spreads impulse to 1/3 oct ±10%; Off unspread | `SpectrumMapper: 1/3-octave smoothing spreads a point impulse over ~1/3 octave; Off leaves it unspread` | PASS - measured width 0.351 octaves vs 0.333 target (+5.3%, inside ±10%); Off: point value 0.00 dB (exact, `10*log10(1.0)`), ≤2 nonzero points |
| AC5 | freqLow/freqHigh/numPoints changes don't reset averaging/secondary | `SpectrumEngine: freqLow/freqHigh/numPoints changes never reset averaging or secondary state` (via `processPending`, 41 alternating calls) | PASS - all-time max retained (-3.484 dB vs the -3.486 dB peak, within 0.1 dB); primary average's measured decay (-20.49 dB) matched the analytic no-reset prediction (-20.98 dB) within 2 dB (a reset would have produced a >150 dB deviation, since `resetSecondaryOnly`/re-priming resets to `kFloorPower`) |
| AC6 | FrameExchange: no torn frames, no allocation, sequence never decreases | 6 test cases in `FrameExchangeTest.cpp`, incl. the 200,000-publish concurrent stress test | PASS - 0 torn frames, 0 out-of-order reads, 0 allocations during the stress run, across 5 repeated runs with no flakiness |
| AC7 | 30 s @ 48 kHz/128-sample blocks, ~5 ms polling, 0 overflows, signal reflected | `SpectrumEngine: 30 s at 48 kHz / 128-sample blocks with ~5 ms polling gives 0 overflows and reflects the signal` | PASS - 0 overflows; final frame peak in [-7.5, -6.0] dB for the injected -6 dBFS 1 kHz tone |
| AC8 | Mapping+smoothing+slope+dB, 2048 pts x 3 curves ≤ 0.3 ms/frame (Release) | `Performance: SpectrumMapper 2048 points x 3 curves <= 0.3 ms per frame (Release)` | PASS - measured 0.147 ms (Release), well under budget; Debug measured 0.72-0.72 ms, informational only per the T-004 pattern |
| AC9 | No NaN/Inf: silence, DC, full-scale noise, denormal | `SpectrumMapper: no NaN/Inf for extreme bin-power inputs` (4 signal types x 4 slopes x 3 smoothing modes = 48 combinations) + `SpectrumEngine: no NaN/Inf for silence, DC, full-scale noise or denormal input` (4 signals, dual mode + stereo sidechain, end-to-end) | PASS |
| AC10 | `/W4` clean | Full Debug and Release build of `InvestigateUnitTests` | PASS - 0 warnings in either configuration from the new/changed files |

Full suite (T-004 + T-005): **53 test cases, 150,817 assertions, 0 failures**, Release build, seed 12345; re-ran `[T-005]` with 3 extra seeds (1, 2, 3) and the AC6 stress test 5x standalone, no flakiness observed. Full `ctest --preset release --output-on-failure`: **54/54 passed** (53 Catch2 cases + `pluginval`, 21.87 s total).

No defects found.

## Real-time safety audit

Scope: `SpectrumEngine::pushMain`, `SpectrumEngine::pushSidechain`, `SpectrumEngine::processPending` (and its private helper `publishFrame`), `FrameExchange::publish`, `FrameExchange::read`, and everything they call (`SampleFifo::push`/`pop` - re-confirmed, not re-audited in depth since T-004 already covered it and nothing here changed it; `SpectrumMapper::computeFrequencies`/`mapCurve`; `SpectrumAnalyser::setSettings`/`pushBlock`).

- **Allocation**: `pushMain`/`pushSidechain` forward directly to `SampleFifo::push` (T-004: allocation-free, re-confirmed by rerunning `"SampleFifo::push performs no heap allocation"`). `processPending()` calls `analyser.setSettings(settings.toAnalyserSettings())` on *every* call (not only when settings change) - I initially flagged this as a possible allocation risk, but reading `SpectrumAnalyser::setSettings()` (`source/dsp/SpectrumAnalyser.cpp:128-187`) shows the FFT-plan rebuild and window-table rebuild are individually gated (`fftPlanNeedsRebuild`, `windowTableNeedsRebuild`), so an unchanged `fftSize`/`window` costs only a few comparisons and `recomputeDerivedConstants()` (no allocation). `processPending`'s own drain loop and `publishFrame` touch no container that isn't pre-sized in `prepare()` (`mainPopBuffers`/`sidechainPopBuffers` sized to `kDrainChunkFrames`, `SpectrumMapper`'s scratch sized to `maxPoints` in `mapper.prepare()`). `FrameExchange::publish`/`read` do one atomic exchange / a bounded CAS loop and a `SpectrumFrame` struct-copy assignment (fixed `std::array`s, no heap pointers) - no allocation. Confirmed empirically: `ScopedAllocGuard` around `FrameExchange::publish`/`read` in a 1000-iteration loop and around the 200,000-publish concurrent stress test both measured exactly 0 allocations.
- **Locks/waits**: none. `FrameExchange` uses only `std::atomic<uint32_t>` (exchange + CAS-loop, wait-free apart from a bounded single-producer/single-consumer retry). `SampleFifo` uses `juce::AbstractFifo`'s lock-free cursor pair (T-004). `SpectrumAnalyser`/`SpectrumMapper` have no synchronisation primitives (single-threaded-by-contract).
- **I/O/logging**: none found (`grep` for `DBG`/`std::cout`/file access across `SpectrumEngine.cpp`, `FrameExchange.cpp`, `SpectrumMapper.cpp` - no matches).
- **Exceptions**: `pushMain`/`pushSidechain`/`processPending`/`readLatest`/`resetSecondary`/`getOverflowCount` and `FrameExchange::publish`/`read`/`getWriteFrame` are all `noexcept`. `SpectrumMapper::computeFrequencies`/`mapCurve` are `noexcept`. `SpectrumAnalyser::setSettings` is not marked `noexcept` (T-004, unchanged) but is analysis-thread-only, not audio-thread; nothing in it is observed to throw.
- **`std::function`, unbounded loops, system calls**: none found. `processPending`'s drain loop is explicitly bounded (`maxIterations = kFifoCapacityFrames/kDrainChunkFrames + 2`, documented as defensive insurance against a logic error, not a hard real-time requirement since this runs on the analysis thread).
- **`juce::ScopedNoDenormals`**: not present in `SpectrumEngine`/`FrameExchange`/`SpectrumMapper` - correctly so, since none of these run on the audio thread; `pushMain`/`pushSidechain` (the only audio-thread-reachable entry points here) forward to `SampleFifo::push`, which does no floating-point arithmetic that could produce denormals (a pure memory copy), so no denormal-handling gap exists on the audio-thread side of this task's classes. `SpectrumAnalyser::pushBlock` (analysis thread, T-004) already has its own `ScopedNoDenormals` at entry.

No real-time safety violations found in the audited call graphs.

## Sanitizers

No ASan/UBSan preset exists yet (open question carried over from T-004, still owned by build-engineer / deferred to T-017). Not run this round.

## Static analysis

Not run - no `clang-tidy` config in the repo; MSVC `/analyze` is out of scope per T-017. `/W4` is clean (this task's own acceptance criterion), confirmed above.

## Coverage

No coverage tooling wired into the presets yet (same open question as T-004, deferred to T-017/build-engineer). Not measured this round.

## Exact commands used

```
cmake --preset windows-msvc
cmake --build --preset debug   --target InvestigateUnitTests
cmake --build --preset release --target InvestigateUnitTests

# T-005 suite, Debug (functional correctness; perf budgets informational only):
./build/tests/Debug/InvestigateUnitTests.exe "[T-005]" --order rand --rng-seed 12345 -r compact

# T-005 suite, Release (perf budgets enforced):
./build/tests/Release/InvestigateUnitTests.exe "[T-005]" --order rand --rng-seed 12345 -r compact

# Flakiness check, 3 extra seeds + 5x standalone AC6 stress reruns:
./build/tests/Release/InvestigateUnitTests.exe "[T-005]" --order rand --rng-seed 1 -r compact
./build/tests/Release/InvestigateUnitTests.exe "[T-005]" --order rand --rng-seed 2 -r compact
./build/tests/Release/InvestigateUnitTests.exe "[T-005]" --order rand --rng-seed 3 -r compact
./build/tests/Release/InvestigateUnitTests.exe "[AC6]" -r compact   # x5

# Full suite (T-004 + T-005), Debug and Release:
./build/tests/Debug/InvestigateUnitTests.exe   --order rand --rng-seed 12345 -r compact
./build/tests/Release/InvestigateUnitTests.exe --order rand --rng-seed 12345 -r compact

# Performance numbers:
./build/tests/Release/InvestigateUnitTests.exe "[AC8]" -r console -s

# Full CTest run (includes SmokeTest, T-004, T-005 and pluginval):
ctest --preset release --output-on-failure
```

Results: all green. Release perf: AC8 0.147 ms / 0.3 ms budget. AC7: 0 overflows over the simulated 30 s run. `ctest`: 54/54 passed, 21.87 s total.

## Open questions

1. No ASan/UBSan CMake preset exists yet - carried over from T-004, still requested from build-engineer, deferred to T-017.
2. No coverage tooling wired in yet - same as above, deferred to T-017/build-engineer.
3. AC5's roadmap wording says the acceptance criterion should be confirmed "not just by reading the code" - done via `SpectrumEngine::processPending`, but the primary-average check necessarily uses a 2 dB tolerance band around an analytic no-reset prediction (rather than an exact match) because the point-grid index nearest 2 kHz can shift by a sub-bin amount between calls with different `numPoints`/`freqLow`/`freqHigh` even when the underlying bin-domain state is untouched; a reset would miss this tolerance by roughly two orders of magnitude, so the test still has a wide margin of discriminating power. Not blocking.
