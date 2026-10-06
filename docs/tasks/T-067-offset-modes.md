# T-067: Spectrum engine: offset modes
- Owner: dsp-engineer
- Milestone: M3c
- Status: TODO
- Depends on: T-047

## Goal
Add the Offset display shift that the user asked for in review finding #8. Its modes:
- **Normalize:** slot 1's peak reads 0 dB.
- **Center:** slot 1's mean level sits at the middle of the level range.
- **Spectrum N:** slot 1's mean level matches slot N's, for N = 2, 3 or 4.

It is computed on the analysis thread, time-smoothed, and additive: with Offset off, the frames are bit-identical.

## Inputs
- `docs/plans/m3-review-spectrum-slots.md`, Architecture: "Offset (#8)".
- The T-046 (slots, per-slot frame fields) and T-047 (Align, 1 Hz, 96 kHz) logs.
- `source/dsp/SpectrumEngine.*`, `SpectrumMapper.*`, `SpectrumSettings.h`, `SpectrumFrame.h`, `DisplayRange.h`.
- Working rules as in T-046.

## Scope
1. **Settings.** Add `offsetMode` (off, normalize, center, spectrum2, spectrum3, spectrum4) to the engine settings. The default is off.
2. **Measurement**, per published frame, over the **visible** frequency range (the view, not the home range or the margins), on slot 1's mapped dB values with slope and Align applied and before the offset:
   - **Normalize:** the target shift is `0 dB − max(slot 1)`.
   - **Center:** the target shift is `(dbHiEff + dbLoEff) / 2 − powerMean(slot 1)`. The power mean is taken in the power domain and converted to dB.
   - **Spectrum N:** the target shift is `powerMean(slot N) − powerMean(slot 1)`, both over the same visible points.
3. **Smoothing over time.** The applied offset follows the target through a one-pole filter with a 300 ms time constant (a named constant; refine it with a reason). It jumps straight to the target on the first frame after the mode is enabled.
4. **Missing target.** If the target slot is off, or its source is missing, the target shift becomes 0 dB, and the applied offset eases there through the same filter. The frame flags the selection as inactive.
5. **Freeze.** While frozen, the applied offset holds its value at the freeze, and a view change re-maps with that held value.
6. **Application.** Add the applied offset to slot 1's curve (and slot 1's overview) only. Publish it in the frame (`appliedOffsetDb`, `offsetInactive`).
7. **The capture (T-048)** records the applied offset without baking it into the values. Document this for T-048.

## Acceptance criteria
- [ ] **Off.** Every frame is bit-identical to the T-047 build, at Fit and zoomed, for every slot configuration.
- [ ] **Normalize.** With a steady 1 kHz sine at −20 dBFS (slope 0, Align on), slot 1's peak reads 0 ± 0.1 dB after settling. Zooming to a view that excludes the sine re-normalises to the highest visible point.
- [ ] **Center.** With steady pink noise, the power mean of slot 1's visible points lies at the middle of the effective level range within ±0.2 dB after settling.
- [ ] **Spectrum N.**
  - With slot 1 = Main and slot 3 = SC1 carrying the same noise 12 dB quieter, the applied offset settles at −12 ± 0.2 dB.
  - Each of Spectrum 2, 3 and 4 works.
  - Disabling the target slot, or disconnecting its source, eases the offset to 0 dB and sets `offsetInactive`.
- [ ] **Time behaviour.** After a 10 dB level step, the applied offset reaches 63 % of the step within 300 ms ± 15 %. It jumps on enable. It holds while frozen.
- [ ] **Robustness.** Silence, an empty view or −inf values produce no NaN or infinity, and the offset stays finite (documented silent behaviour).
- [ ] **Budget.** The worst-case re-map with Offset on stays ≤ 2.0 ms median (`[.perf]`). 0 allocations in `processPending`.
- [ ] Every existing test passes. Warning-clean at `/W4`. `docs/qa/T-067-unit.md` reports PASS.

## Log
- 2026-10-06 vst-architect: Created from the M3 review triage. The user decided that the alignment target can be any slot (Spectrum 2 to 4), not only the sidechain inputs, and that a target option is unavailable while its slot is off or its source is missing. The parameter is added in T-051, the wiring in T-052, and the selector in T-055.
