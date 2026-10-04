# T-049: MeterBank: integration, release, metering modes and density (additive)
- Owner: dsp-engineer
- Milestone: M3c
- Status: TODO
- Depends on: T-043, T-048 (build order)

## Goal
Add SPAN-style level-meter ballistics and the data the new metering modes and the Density style need, on the audio thread, inside the existing MeterBank budget. Every change is additive: existing snapshot fields keep their meaning.

## Inputs
- `docs/plans/m3c-feature-expansion.md`:
  - design items [2] and [10];
  - the "MeterSnapshot additions" section;
  - the Budgets: MeterBank mean ≤ 15 µs and p99 ≤ 40 µs per 128-sample stereo block.
- `docs/research/T-043-span-m3c-reference.md`, questions 1 to 3. They settle:
  - the definitions of integration, release and hold;
  - the ranges;
  - whether the LUFS and LU bars are per channel.

  If the report calls for per-channel loudness bars, the roadmap Decisions entry says so, and this task adds them.
- `source/dsp/MeterBank.*`, `MeterSnapshot.h`, `LevelMeter.*`, `LoudnessMeter.*`, and the T-006, T-007 and T-024 logs (the seqlock, the 50 ms maxima, the p99 history).
- Working rules as in T-046.

## Scope
1. **Integrated level per channel.**
   - A one-pole on power with τ = T / ln 100, where T is the integration time in ms: the "time to rise or fall 20 dB" convention.
   - Release: the integrated level falls at no more than 20 dB per release time, linear in dB.
   - Published as `level[ch]`, the maximum over the last 50 ms, in dBFS.
   - Setters `setIntegrationMs (float)` and `setReleaseMs (float)` take effect at the next block. They are single atomic stores, callable from any thread.
   - The defaults are 1.00 ms and 750 ms. The processor (T-052) passes 600 ms for both in the K-xx C modes.
2. **Density histogram per channel.**
   - 0.5 dB bins from −96 to +6 dBFS (204 bins).
   - Fed with the integrated level about once per millisecond: not per sample, to keep `log10` calls bounded.
   - Decaying with a time constant set by `setPeakHoldMs (float)` (default 1000 ms).
   - Normalised from 0 to 1 when published as `density[ch][204]`.
   - The bin layout is defined as constants that the editor can include.
3. **Per-channel loudness, only if T-043 calls for it:** `momentaryCh[ch]` and `shortTermCh[ch]` from the per-channel K-weighted signals, using the existing 400 ms and 3 s windows.
4. **Reset.** `requestReset()` also clears the density histogram. The integrated level is a running value, not a statistic, so it is not cleared.
5. Update the seqlock payload, and the `MeterSnapshot` documentation comment.

## Acceptance criteria
- [ ] **No regression.** Every existing `MeterSnapshot` field is bit-identical to the current build for the same input. Every EBU and true-peak test passes unchanged.
- [ ] **Integration timing.** Measure on the meter's internal per-sample value through a test accessor, not the 50 ms maximum. After a step from silence to steady noise:
  - at t = 1.05 T, the level is within 0.1 dB of its final value;
  - at t = 0.6 T, it is still more than 0.1 dB below it.

  This follows from τ = T / ln 100. Test T = 1, 10, 100 and 1000 ms at 44.1, 48 and 96 kHz.
- [ ] **Integration accuracy.** A steady sine reads its RMS level (−9.03 dBFS for a −6 dBFS peak sine) within ±0.1 dB in the published `level`, once T is at least 50 periods of the sine.
- [ ] **Release.** After the signal stops, `level` falls 20 dB in the release time ± 5 % (750 ms by default; tested at 10, 750 and 5000 ms).
- [ ] **Density.**
  - A steady −20 dBFS-RMS signal concentrates more than 90 % of the histogram mass within ±1 dB of −20 dB.
  - After the signal stops, the mass in those bins falls to 1/e within the hold time ± 10 %.
- [ ] **Budget.** MeterBank mean ≤ 15 µs and p99 ≤ 40 µs per 128-sample stereo block at 48 kHz with every new feature active (`[.perf]`, isolated, as in T-024). 0 allocations in `process`.
- [ ] Every existing test passes. Warning-clean at `/W4`. `docs/qa/T-049-unit.md` reports PASS.

## Log
- 2026-10-03 vst-architect: Created from `docs/plans/m3c-feature-expansion.md`. The editor applies release and hold to the peak part and the hold tick (T-057). This task covers the level part and the density data. The processor wiring is in T-052.
