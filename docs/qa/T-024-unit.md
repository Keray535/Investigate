# T-024: True-peak follow-up (per-tier tap count, EBU cases 20-23) - unit QA

Tester: unit-tester. Build: `cmake --build --preset debug/release --target InvestigateUnitTests`.
Run: `./build/tests/Debug|Release/InvestigateUnitTests.exe`, and `ctest --preset debug/release --output-on-failure`.

## Verdict: PASS, 0 defects

All five acceptance criteria pass. The 4x tier's kernel size is exactly 12 taps/phase (no
padding), confirmed by `static_assert` plus code review of both `TruePeakDetector.h` and `.cpp`.
The full pre-existing T-004 to T-007 suite has 0 regressions in Debug and Release (151,865/151,865
assertions, 119 cases, 5 documented skips; CTest 126/126 including pluginval). All four EBU Tech
3341 cases 20-23 pass, with a two-stage oracle that validates the test signal's own synthesis
before judging the meter, exactly as the task specified. `MeterBank::process()`'s tightened
10 µs/30 µs budget passes in isolation across 3 consecutive runs. Both configurations rebuild
warning-clean at `/W4` from a forced rebuild, including all new/changed test files.

## Test plan (derived from the task's acceptance criteria and the dispatch's 5 numbered items,
before running anything)

1. Re-run `[T-007]` and the full `~[performance]~[perf]` suite in Debug and Release; confirm 0
   regressions from the `TruePeakDetector` tier-splitting change.
2. Kernel-size acceptance criterion: a `static_assert` (compile-time, strongest possible check)
   plus a runtime `CHECK` that `kItuTapsPerPhase == 12` and `kHannSincTapsPerPhase <= 25`; code
   review of `TruePeakDetector.h`/`.cpp` to confirm `kPhaseCoeffs` has no padding columns and
   `computePhase4x`'s loop bound is exactly `kItuTapsPerPhase`.
3. Update `MeterBankTest.cpp`'s performance test to the tightened mean ≤10 µs / p99 ≤30 µs budget,
   128-sample stereo block at 48 kHz, Release only; tag both `[performance]` (kept) and `[perf]`
   (the roadmap's Decision); run 3 consecutive isolated runs and report all 3 mean/p99 numbers.
4. EBU Tech 3341 cases 20-23: design and implement the literal splice-into-fs/6-background
   synthesis at 4·fs, a Kaiser-windowed-sinc anti-alias filter verified independently to reach
   ≥90 dB stopband attenuation, downsample at offsets 0-3, a two-stage oracle (own-peak validation
   first, meter check second, libebur128 recorded for information only).
5. Real-time safety / build cleanliness: forced rebuild of both configs, grep for warnings;
   re-confirm `MeterBank::process()`'s existing `ScopedAllocGuard` test (T-007 test #31) still
   passes unchanged (no source changed except `TruePeakDetector`, so no allocation regression is
   expected, but re-run rather than assumed).

## Results

| # | Test case | Result |
|---|---|---|
| 1 | `TruePeakDetector` outputs unchanged on all pre-existing T-007 test signals (fs/4 45° case, EBU 15-19, true-peak clip counter, libebur128 cross-checks at 44.1/48/96kHz for pink noise/swept sine/fs/4, 1x sample-peak path, NaN/Inf safety) | PASS, 0 regressions - see "Re-verification of bit-identical claim" below |
| 2 | `MeterBank`/`LevelMeter`/`CorrelationMeter`/`LoudnessMeter` tests (unaffected by the source change) | PASS, 0 regressions |
| 3 | `kItuTapsPerPhase == 12` (static_assert + runtime CHECK) | PASS |
| 4 | `kHannSincTapsPerPhase <= 25` (static_assert + runtime CHECK) | PASS |
| 5 | Code review: `kPhaseCoeffs` is `array<array<float,12>,4>`, no padding columns; `computePhase4x`'s loop bound is `r < kItuTapsPerPhase` reading only `delay4x` (`array<float,12>`) | PASS - confirmed by direct reading of `TruePeakDetector.h` lines 219, 242-248 and `.cpp` lines 24-41, 242-248 |
| 6 | Kaiser anti-alias filter reaches ≥90dB stopband attenuation (independently verified via the filter's own DTFT magnitude response, not assumed from the design formula) | PASS - 155 taps, measured -104 to -128dB across the swept stopband (0.20-0.49 normalised), all ≤ -90dB |
| 7 | EBU case 20-23 oracle validation: anti-alias-filtered 4·fs signal peaks at 0.0±0.1dBFS | PASS - measured 0.0836dBFS |
| 8 | EBU case 20 (0-sample offset): `TruePeakDetector` within +0.2/-0.4dB of 0.0dBTP | PASS - see results table below |
| 9 | EBU case 21 (1-sample offset) | PASS |
| 10 | EBU case 22 (2-sample offset) | PASS |
| 11 | EBU case 23 (3-sample offset) | PASS |
| 12 | `MeterBank::process()` performance: mean ≤10µs, p99 ≤30µs, 3 isolated runs | PASS all 3 runs - see numbers below |
| 13 | `MeterBank::process()` no heap allocation (`ScopedAllocGuard`, T-007 test #31, re-run unchanged) | PASS |
| 14 | Full pre-existing suite (T-004/005/006/007), `~[performance]~[perf]`, Release | PASS, 0 regressions (119 cases, 114 passed, 5 documented skips, 151865/151865 assertions) |
| 15 | Full pre-existing suite, Debug | PASS, 0 regressions (same counts) |
| 16 | `ctest --preset release` | PASS, 126/126 (121 executed + 5 skipped), including `pluginval` |
| 17 | Warning-clean `/W4`, Debug and Release, forced rebuild of every file in `tests/unit/` | PASS (0 warnings, 0 errors, both configs) |

### EBU Tech 3341 cases 20-23 results table

Synthesis: 4·fs = 192kHz working rate, fs/6 = 8000Hz background (amplitude 0.5), fs/4 = 12000Hz
burst (amplitude 1.0, one full period, 16 working-rate samples), a 16-sample raised-cosine
crossfade in/out around the burst (chosen empirically - see "Tuning notes" below), a 1ms overall
clip fade, Kaiser anti-alias filter (cutoff 0.1125 of the working rate = 0.45·fs of the target
rate, transition to 0.15, 90dB target attenuation, 155 taps), downsampled to fs=48000Hz at offsets
0-3.

| Case | Offset | Oracle peak (dBFS, target 0.0±0.1) | Own meter (dBTP, target 0.0 +0.2/-0.4) | Pass/fail | libebur128 (dBTP, informational) |
|---|---|---|---|---|---|
| 20 | 0 | 0.0836 | -0.0993 | PASS | 0.0836 |
| 21 | 1 | 0.0836 | 0.0487 | PASS | 0.1546 |
| 22 | 2 | 0.0836 | 0.0380 | PASS | 0.1245 |
| 23 | 3 | 0.0836 | 0.0487 | PASS | 0.1546 |

The oracle peak is identical across all 4 offsets, as expected (it is measured before
downsampling - offset only changes which samples are kept afterward). Own-meter readings spread
across a 0.148dB range (-0.099 to 0.049dB) depending on offset, which is exactly the effect these
cases are designed to exercise (the interpolator recovering a true peak that lands between
different fs-rate sample positions depending on the decimation phase) - and every offset stays
inside the ±0.2/-0.4dB budget with comfortable margin (worst case 0.30dB of the 0.4dB downside
margin used, at offset 0). libebur128's own readings (informational only, not a gate) sit
0.04-0.11dB above this project's own readings at every offset but the same order of magnitude and
sign, which is a reasonable cross-check given the two implementations use independently-designed
interpolation filters (this project's literal ITU table vs. libebur128's self-generated
Hann-windowed-sinc, per the class comment's "The 88.2/96kHz filter is not a subset of the ITU
table" section - the same kind of small, expected divergence T-007 already documented between the
two filters on non-analytic signals).

**Tuning note**: the crossfade length between the fs/6 background and the fs/4 burst was widened
from an initial 4-sample (1/4-period) attempt to a 16-sample (1-full-period) raised-cosine
crossfade. The 4-sample version's higher-frequency crossfade content caused the anti-alias filter
to Gibbs-ring slightly, pushing the oracle peak to 0.107dBFS - 0.007dB outside the ±0.1dB budget
(a synthesis-side effect, not a `TruePeakDetector` defect; caught precisely because the oracle
check is a separate assertion from the meter check, exactly as the task intends). The wider,
smoother crossfade removed the overshoot (oracle settled at 0.0836dBFS) without changing the
"single period of fs/4 at amplitude 1.00" core of the case, which remains at full weight for the
entire 16-sample burst period.

### Re-verification of bit-identical claim (item 1 above)

Re-ran the pre-existing `tests/unit/TruePeakDetectorTest.cpp` suite (unchanged except for the new
kernel-size test case appended at the end) end to end: the task's own fs/4 45° case, EBU cases
15-19, the true-peak clip counter, the libebur128 cross-check (pink noise/swept sine/fs/4 at
44.1/48/96kHz), the 1x sample-peak path, and the NaN/Inf safety test all pass with the exact same
tolerances as T-007's round 2 QA report recorded, with no test-file changes needed to accommodate
the source change - which is itself evidence for the "bit-identical" claim: if the 4x tier's
output had shifted even slightly, the tightest of these tests (the libebur128 cross-checks, which
matched to 6 decimal places in T-007 round 2) would have been the first to show it, and none did.
Combined with the code-review confirmation (item 5 above) that no zero-padded tap is ever
multiplied in the new code, and the source comment's own float-arithmetic argument (`x + 0.0f ==
x`, `0.0f * y == 0.0f` exactly for every finite x/y), this is accepted as sufficient verification
without a separate bit-for-bit diff harness.

### Performance: 3 consecutive isolated runs (`[perf][T-024]`, Release)

| Run | Mean (µs) | p99 (µs) | Budget |
|---|---|---|---|
| 1 | 5.00 | 28.8 | ≤10 / ≤30 |
| 2 | 4.99 | 28.8 | ≤10 / ≤30 |
| 3 | 4.89 | 28.7 | ≤10 / ≤30 |

All 3 runs pass comfortably on mean (roughly half the 10µs budget) and pass on p99 with a
consistent ~1.2-1.3µs margin against the 30µs budget - thinner than the mean's margin, but stable
across all 3 runs (28.7-28.8µs every time, not the wide run-to-run spread T-007 saw against its
older, looser 40µs budget). This matches dsp-engineer's own pre-handoff measurement (mean~4.9-5.0µs
/ p99~28.7-28.9µs) within run-to-run noise. As with T-006/T-007's precedent, running the tag
together with the rest of the suite (not in isolation) makes this measurement noisy and is not
gated - only the isolated number above is.

## Real-time safety audit

No `source/` file changed except `TruePeakDetector.cpp`/`.h` (per the task's own scope note), and
`MeterBank.h`/`.cpp` are unchanged. Re-read `TruePeakDetector::process()`, `push4x`/`push2x`,
`computePhase4x`/`computePhase2x`, `onSubBlockComplete()`, `prepare()`/`reset()`/
`resetStatistics()` end to end for this change: no `new`/`delete`/`malloc`, no container growth
(`delay4x`/`delay2x` are both fixed-size `std::array`, sized 12 and 25 respectively, never
resized), no locks, no I/O or logging, no `std::function` construction, no thrown exceptions,
every loop bounded by a compile-time constant (`kItuTapsPerPhase`/`kHannSincTapsPerPhase`) or by
`numSamples`, no system calls. `push4x`/`push2x`'s shift loops are each a fixed, small,
compile-time-bounded loop (12 and 25 iterations respectively) - no change in kind from T-007's
single shared loop, only in which fixed bound applies to which tier.

`MeterBank::process()`'s no-allocation test (`ScopedAllocGuard`, unchanged from T-007) re-run and
still passes: 0 allocations across 100 `process()` calls plus `setRmsWindowMs()`, `requestReset()`
and `getSnapshot()`.

## Coverage

No coverage preset exists yet (OpenCppCoverage/llvm-cov are T-014/M4 scope, per T-007's QA report
- unchanged). Informally: every new/changed line in `TruePeakDetector.h`/`.cpp` (both tiers' push/
compute methods, both coefficient tables, the `process()` branch) is exercised by the pre-existing
T-007 suite (which already covers `oversampleFactor` ∈ {1, 2, 4}) plus the new cases 20-23 tests
(which exercise the 4x tier specifically, at 48kHz, with a signal shape - a composite tone with an
embedded transient - that cases 15-19's pure sinusoids do not exercise).

## Commands used

```
cmake --build --preset debug --target InvestigateUnitTests
cmake --build --preset release --target InvestigateUnitTests
./build/tests/Release/InvestigateUnitTests.exe "[T-024]" --reporter console -s
./build/tests/Release/InvestigateUnitTests.exe "[T-007]" --reporter console
./build/tests/Release/InvestigateUnitTests.exe "T-024 EBU cases 20-23: oracle*" --reporter console
./build/tests/Release/InvestigateUnitTests.exe "T-024 EBU cases 20-23: TruePeakDetector*" --reporter console -s
./build/tests/Release/InvestigateUnitTests.exe "[perf][T-024]" --reporter console -s   # x3 isolated runs
./build/tests/Release/InvestigateUnitTests.exe "~[performance]~[perf]" --reporter console   # full suite regression check, Release
./build/tests/Debug/InvestigateUnitTests.exe "[T-024]" --reporter console
./build/tests/Debug/InvestigateUnitTests.exe "~[performance]~[perf]" --reporter console   # full suite regression check, Debug
ctest --preset release --output-on-failure
touch tests/unit/*.cpp tests/unit/*.h && cmake --build --preset debug/release --target InvestigateUnitTests   # /W4 forced-rebuild check
```

## Defects

None. Source (`source/dsp/TruePeakDetector.h`/`.cpp`) reviewed and re-tested as described above;
no discrepancy from the task's design requirements or acceptance criteria was found.

## Open questions

1. **The 155-tap Kaiser anti-alias filter and the whole cases-20-23 synthesis live only in
   `tests/unit/`** (per the task's constraints), not in `source/dsp/` - this is intentional (the
   filter and splice construction are test-oracle machinery, not production DSP), flagged here
   only so a future reader of `TruePeakDetector.cpp` does not go looking there for it.
2. **Crossfade length (16 samples, one full burst period) is this tester's own synthesis choice**,
   not specified numerically by the EBU document or by the task (which says "short fade-in/out"
   without a number, unlike cases 15-19's explicit "10ms"). It was tuned empirically to pass the
   ±0.1dBFS oracle budget (see "Tuning note" above) rather than derived analytically; if a future
   task wants the exact official EBU test files instead of this synthesis, that would remove this
   design freedom entirely.
3. **Round 1 of the coder-tester loop for this task, and no defects found.** No handback to
   dsp-engineer is needed based on this round's results.
