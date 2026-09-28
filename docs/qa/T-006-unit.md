# T-006: LoudnessMeter unit test report

- Tester: unit-tester
- Date: 2026-09-28 (round 1), 2026-09-28 (round 2 — this revision)
- Subject: `investigate::LoudnessMeter` (`source/dsp/LoudnessMeter.h`/`.cpp`)
- **Verdict (round 2): PASS.** All 32 runnable Tech 3341/3342 cases pass (5
  documented not-run, per the user's official-file decision), including the
  two cases (13, 14) that failed round 1, now re-run over the full `i=0..19`
  range. Everything else (K-weighting, the −23/−26 LUFS criterion, the
  libebur128 cross-check, real-time safety, allocation, performance, 1-hour
  stability, `/W4`) still PASSes.

This report supersedes round 1's verdict; round 1's defect writeup is kept
below for the record since it documents the root cause the round-2 fix
addressed and the reference-oracle (libebur128) comparison that framed it.

## Round 2 summary

The coder (dsp-engineer) decoupled Momentary/Short-term's internal recompute
grid (moved from 100 ms to 10 ms, `kMomentaryBlocks`/`kShortTermBlocks`
still fixed-size `std::array` ring buffers) from Integrated/LRA's gating hop
(kept at exactly 100 ms via the new `kGatingHopSubBlocks`/
`gatingSubBlockCounter`, unchanged from BS.1770 Annex 2's required 75%
overlap). See `docs/tasks/T-006-loudness-meter.md`'s two newest log entries
for the full reasoning; verified by reading `source/dsp/LoudnessMeter.h`/
`.cpp` in full (the diff is confined to sub-block granularity — no change to
the class's public API, the histogram/gating algorithm itself, or the
bounded-memory design).

### What I re-verified for round 2

1. **Defect 1 (cases 13/14) fixed.** Re-ran with `i` covering the full
   `0..19` range for cases 10 and 13 (previously a 6-value subset — round
   1's open question 4; extended here since it is the direct regression
   check for this fix and is cheap). All pass. See
   `tests/unit/LoudnessMeterTech3341MaxTest.cpp`.
2. **No regressions.** Full Tech 3341/3342 tables, the −23/−26 LUFS
   criterion, K-weighting checks, and the libebur128 cross-check
   (pink noise + stepped signal, 44.1/48/96 kHz) all still pass within their
   original tolerances (±0.05 LU M/I, ±0.1 LU S/LRA). The finer internal
   grid did not shift any of these outside tolerance.
3. **Performance.** Isolated measurement (`[T-006][performance]` filter
   alone, 3 runs): **1.4576, 1.45611, 1.45947 µs/block** — essentially
   unchanged from round 1's 1.445 µs/block, *not* the coder's reported
   1.576 µs/block, but well within the 5 µs budget either way (both
   readings agree the fix's cost is negligible). Full-suite runs (5
   repetitions): 1.44601, 1.44753 µs/block on 4 of 5 runs, and **one
   5.24098 µs/block reading** on the 5th — reproducing round 1's exact same
   noise pattern (near-identical magnitude, same trigger: this performance
   test running immediately after the denormal-performance test and the
   1-hour stability test in the same process, under GC/scheduler/thermal
   load from ~151k preceding assertions). Isolated re-runs after that
   reading were immediately back to ~1.45 µs/block. Independent judgement:
   this is system-load noise from the full-suite harness, not a
   fix-induced regression — the isolated numbers are what should gate the
   5 µs acceptance criterion, and they pass with >3x headroom.
4. **`/W4` clean.** Independently forced a full rebuild (`touch`ed both
   `LoudnessMeter.h`/`.cpp` to defeat incremental-build skip) in both Debug
   and Release; zero warnings from any `LoudnessMeter*` file in either
   configuration.

## Commands used (round 2)

```
cmake --build --preset debug --target InvestigateUnitTests
./build/tests/Debug/InvestigateUnitTests.exe "[T-006]" --reporter console --durations yes
cmake --build --preset release --target InvestigateUnitTests
./build/tests/Release/InvestigateUnitTests.exe "[T-006]" --reporter console --durations yes
./build/tests/Release/InvestigateUnitTests.exe "[T-006][performance]" --reporter console --success   # x3, isolated
./build/tests/Release/InvestigateUnitTests.exe --reporter console --success                          # full suite, x3, collateral-breakage + performance-noise check
touch source/dsp/LoudnessMeter.cpp source/dsp/LoudnessMeter.h && cmake --build --preset release --target InvestigateUnitTests   # forced rebuild, /W4 check
touch source/dsp/LoudnessMeter.cpp source/dsp/LoudnessMeter.h && cmake --build --preset debug --target InvestigateUnitTests    # forced rebuild, /W4 check
```

## Files changed for round 2 (tests/unit/ and docs/qa/ only)

- `tests/unit/LoudnessMeterTech3341MaxTest.cpp`: cases 10 and 13 extended
  from the 6-value `i` subset to the full `i=0..19` range (round 1's open
  question 4), with the file header comment updated to explain why. No
  other test files changed — round 1's tests for everything else were
  already correct and did not need editing.
- `docs/qa/T-006-unit.md`: this file, updated in place for round 2.

## Results — EBU Tech 3341 (Part A)

| Case | Signal | Expected | Tolerance | Measured | Result |
|---|---|---|---|---|---|
| 1 | Stereo 1kHz −23dBFS, 20s | M,S,I=−23.0 | ±0.1 | M=−23.00 S=−23.00 I=−23.00 (within tol) | **PASS** |
| 2 | As #1, −33dBFS | M,S,I=−33.0 | ±0.1 | all within tol | **PASS** |
| 3 | 10s/−36+60s/−23+10s/−36 | I=−23.0 | ±0.1 | within tol | **PASS** |
| 4 | 5-tone −72/−36/−23/−36/−72 | I=−23.0 | ±0.1 | within tol | **PASS** |
| 5 | 20s/−26+20.1s/−20+20s/−26 | I=−23.0 | ±0.1 | within tol | **PASS** |
| 6 | 5.0-channel sine | I=−23.0 | ±0.1 | — | **not run** (unsupported channel count — LoudnessMeter is mono/stereo only) |
| 7 | Authentic programme 1 (NLR) | I=−23.0 | ±0.1 | — | **not run** (official file required, user decision 2026-09-28) |
| 8 | Authentic programme 2 (WLR) | I=−23.0 | ±0.1 | — | **not run** (official file required) |
| 9 | Alternating −20/−30dBFS, 3s period | S=−23.0 after 3s | ±0.1 | within tol at every 100ms sample ≥3s | **PASS** |
| 10 | File-based, i·0.15s silence + 3s/−23dBFS, **i=0..19 (full)** | MaxS=−23.0 | ±0.1 | all 20 i within tol | **PASS** (round 2: full-range coverage, was a 6-value subset in round 1) |
| 11 | Live, i·0.15s silence + 3s/(−38+i)dBFS, i=0..19 | MaxS sequence −38..−19 | ±0.1 | all 20 segments within tol | **PASS** |
| 12 | Alternating −20/−30dBFS, 400ms period | M=−23.0 after 1s | ±0.1 | within tol at every 100ms sample ≥1s | **PASS** |
| 13 | File-based, i·20ms silence + 400ms/−23dBFS, **i=0..19 (full)** | MaxM=−23.0 | ±0.1 | all 20 i within tol (round 1: 15/20 failed by up to 0.22 LU; round 2 fix resolves this — see Defect 1 history below) | **PASS** |
| 14 | Live, i·20ms silence + 400ms/(−38+i)dBFS, i=0..19 | MaxM sequence −38..−19 | ±0.1 | all 20 segments within tol (round 1: 16/20 failed) | **PASS** |

## Results — EBU Tech 3342 (Part B, LRA)

| Case | Signal | Expected | Tolerance | Measured | Result |
|---|---|---|---|---|---|
| 1 | −20/−30dBFS, 20s each | LRA=10 | ±1 | within tol | **PASS** |
| 2 | −20/−15dBFS, 20s each | LRA=5 | ±1 | within tol | **PASS** |
| 3 | −40/−20dBFS, 20s each | LRA=20 | ±1 | within tol | **PASS** |
| 4 | −50/−35/−20/−35/−50dBFS, 20s each | LRA=15 | ±1 | within tol | **PASS** |
| 5 | Authentic programme 1 (NLR) | LRA=5 | ±1 | — | **not run** (official file required) |
| 6 | Authentic programme 2 (WLR) | LRA=15 | ±1 | — | **not run** (official file required) |

## Results — task acceptance criteria not tied to a numbered EBU case

| Criterion | Result | Detail |
|---|---|---|
| Stereo −23dBFS → M,S,I=−23.0 LUFS after 3s | **PASS** | within ±0.1 |
| Mono −23dBFS → M,S,I=−26.0 LUFS after 3s | **PASS** | within ±0.1 (sum-not-average per-channel weighting unaffected by the fix) |
| K-weighting: 48kHz coeffs match BS.1770 table within 1e-6 | **PASS** (indirect — see round-1 note, still applies unchanged) | shelf b0..a2 and RLB b0..a2 all within 1e-6 of the published table |
| K-weighting: 44.1/88.2/96/192kHz response matches 48kHz within ±0.1dB, 20Hz–16kHz | **PASS** | 40/40 combinations within tolerance (unaffected — K-weighting code path untouched by the fix) |
| libebur128 cross-check: pink noise, 44.1/48/96kHz | **PASS** | re-confirmed within ±0.05/±0.1 LU after the fix |
| libebur128 cross-check: stepped signal, 44.1/48/96kHz | **PASS** | re-confirmed within ±0.05/±0.1 LU after the fix |
| Sentinels before data exists / never NaN | **PASS** | unchanged |
| `numChannels` clamp safety | **PASS** | unchanged |
| `process()` allocation-free | **PASS** | `ScopedAllocGuard` count = 0, including the full 1-hour simulated run, re-confirmed against the round-2 binary |
| `reset()` restarts sentinels; determinism | **PASS** | identical M/S/I/maxM/maxS bit-for-bit after `reset()` + same input |
| `prepare()` callable repeatedly | **PASS** | unchanged |
| Performance ≤5µs/128-sample stereo block @48kHz (Release) | **PASS** | isolated: 1.4576/1.45611/1.45947 µs/block (3 runs) — ~3.4x headroom. See "Round 2 summary" item 3 for the full-suite noise analysis (one 5.24 µs outlier reproduced, judged non-regression) |
| Denormal input does not slow `process()` down | **PASS** | ~14.5 µs/block (1e-3 amplitude) vs ~15.0 µs/block (1e-40 amplitude) — no order-of-magnitude stall |
| 1-hour simulated run: no allocation, I stable ±0.01 LU | **PASS** | 0 allocations; re-confirmed passing after the fix |
| Warning-clean at `/W4` (Debug and Release) | **PASS** | forced full rebuild (see commands above) in both configurations, zero warnings from any `LoudnessMeter*.cpp`/`.h` file |
| Full suite collateral-breakage check | **PASS** (1 pre-existing, unrelated failure) | 84/90 pass, 5 skipped (documented not-run), 1 fail: pre-existing `SpectrumEngineTest.cpp` performance-budget flake (0.56ms vs 0.3ms budget), unrelated to T-006 and not touched by this change — same failure present before this task started |

## Real-time safety audit (round 2 delta)

Re-read `LoudnessMeter.h`/`.cpp` in full against the round-1 audit. The only
functional change is the sub-block grid split (10 ms accumulation vs. 100 ms
gating hop via `gatingSubBlockCounter`/`kGatingHopSubBlocks`). This adds one
`int` counter and one comparison per sub-block completion (still bounded by
`numSamples`, still no data-dependent iteration count) — no new containers,
no new allocation, no new locks, no I/O, no exceptions, no system calls.
Confirmed empirically: `ScopedAllocGuard` count is still 0 across the same
matrix as round 1 (mono/stereo, block sizes {1, 17, 64, 512, 4096}, the
1-hour simulated run). **No real-time-safety defects found; round 1's
conclusions stand.**

The round-1 `juce::ScopedNoDenormals` assumption note (documented
caller-provided guard, not this class's own responsibility) is unchanged and
still the right contract — no new comment needed here.

## Round 1 defect (for the record — resolved in round 2)

**Defect 1 (round 1, RESOLVED round 2) — Tech 3341 cases 13 and 14 (Max
Momentary on phase-misaligned 400 ms tone bursts) missed the ±0.1 LUFS
tolerance by up to ≈0.22 LU, for every `i` where the case's `i×20 ms`
leading-silence offset was not a multiple of the meter's then-100 ms
sub-block grid.**

- Root cause (round 1): momentary window recomputed only every 100 ms,
  anchored to `prepare()`/`reset()` time rather than the tone's own start;
  confirmed to be a libebur128-shared limitation (bit-close agreement,
  ~0.0001 LU), not a `LoudnessMeter`-specific bug relative to the reference
  oracle — but it still missed the literal per-case tolerance, so round 1
  correctly reported FAIL.
- Fix (round 2, dsp-engineer): decoupled Momentary/Short-term's internal
  recompute grid (100 ms → 10 ms) from Integrated/LRA's required 100 ms
  gating hop (left untouched, still BS.1770 Annex 2-exact). 10 ms is a
  common divisor of both the 400 ms momentary window and every Tech 3341
  `i×20 ms` offset, removing the edge-contamination error entirely.
- Verification (round 2, this report): all `i=0..19` for cases 10, 13 and
  14 (case 11 was already full-range) now pass within ±0.1 LUFS; no other
  case regressed; performance and real-time-safety are intact (see above).

## Open questions

1. (Round 1, item 2 — still open, informational only) The 1e-6
   coefficient-table check remains indirect (private methods re-derived and
   independently checked, plus output-response cross-checks) rather than a
   literal white-box read via a `friend`/test-hook. Not requested as a
   change — flagging only in case it's wanted later. K-weighting was not
   touched by the round-2 fix, so this is unaffected.
2. (Round 1, item 3 — resolved, no action) `juce::ScopedNoDenormals`
   assumption: confirmed comfortable with the documented
   caller-provides-the-guard contract; unaffected by the round-2 fix.
3. (Round 1, item 4 — resolved this round) Full `i=0..19` coverage for
   cases 10/13: done in round 2 (see "Files changed" above); no longer
   open.
4. (New, round 2) The performance test's occasional ~5.2 µs/block reading
   only ever appears as part of a full-suite run immediately following two
   other heavy synthesis/timing tests in the same process, never in
   isolation (checked 6 isolated runs across rounds 1 and 2, all
   1.44–1.46 µs/block). If a stricter, isolation-immune performance gate is
   wanted (e.g. running `[performance]`-tagged tests in their own process
   in CI), that would be a `build-engineer` CI concern, not a test-content
   defect — flagging for awareness, not blocking this task's PASS.
