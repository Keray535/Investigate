# T-033: Loudness meter at the start of a measurement (partly filled gating blocks)
- Owner: dsp-engineer
- Milestone: M4
- Status: TODO
- Depends on: T-013

## Goal
Make integrated loudness (I) and loudness range (LRA) right from the start of a measurement, not only over long programmes. In T-013 validation, a steady −23 dBFS stereo sine fed from a cold start read I = −23.1 to −23.2 LUFS and LRA = 0.8 to 1.1 LU after 3 s. After a statistics reset with the sine already running, the same signal read I = −23.0. For a steady sine, BS.1770 gives I = −23.0 and an LRA of about 0 (and LRA is undefined until enough short-term values exist). The likely cause is that gating blocks (400 ms) or short-term windows (3 s) that are only partly filled at the start of a measurement, with zeros or silence before the first sample, enter the integration.

## Inputs
- `source/dsp/LoudnessMeter.*` and its unit tests, plus `docs/tasks/T-006-loudness-meter.md` (the design: a 10 ms M/S grid and a 100 ms gating hop).
- `docs/qa/T-013-validation.md`, observation O-1, and `docs/qa/T-013-logs/`.
- ITU-R BS.1770-5 (gating blocks of 400 ms with 75 % overlap) and EBU Tech 3342 (LRA from short-term values over 3 s windows). Use the references in `docs/research/T-002-dsp-references.md` and `docs/research/T-006-ebu-test-cases.md`.
- libebur128, the test-only oracle that is already linked into the unit tests.

## Scope
1. Reproduce with a unit test: a cold-start −23 dBFS stereo 1 kHz sine. Read I, LRA, M max and S max at 0.5, 1, 2, 3, 5 and 10 s, from `LoudnessMeter` and from libebur128 on the same samples.
2. Decide from the standards whether a block or window that reaches back before the first sample of a measurement (a cold start, or after a reset) may count. Write the decision and the reasoning into the log. Also cover how M and S should read before 400 ms and 3 s have elapsed.
3. Fix `LoudnessMeter` if it deviates. The real-time rules apply: no allocation, and the reset path stays lock-free.
4. All existing EBU Tech 3341 and 3342 cases must still pass.

## Acceptance criteria
- [ ] Cold-start −23 dBFS stereo sine: I is −23.0 ±0.05 LUFS at 3 s and later. LRA is 0.0 ±0.1 LU once it is defined, and reports as undefined (non-finite) before that.
- [ ] The same values after a statistics reset with the sine running.
- [ ] I, LRA, M max and S max agree with libebur128 within ±0.05 LU at every probe time from 1 s on, for the cold start and for a 20 s music-like test signal (for example, pink noise with level steps).
- [ ] Every existing loudness test and EBU case passes. `MeterBank` timing does not regress beyond the `[.perf]` budgets.
- [ ] Warning-clean at `/W4`. `docs/qa/T-033-unit.md` reports PASS.

## Log
- 2026-10-03 vst-architect: Created from T-013 validation observation O-1. A steady-state signal after a reset reads correctly, so this only affects the first seconds of a measurement. It is still a correctness matter for a loudness meter.
