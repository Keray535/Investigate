# T-047: Spectrum mapper: 1 Hz range, 96 kHz axis and Align 0 dB
- Owner: dsp-engineer
- Milestone: M3c
- Status: TODO
- Depends on: T-043, T-046

## Goal
Make the display mapping correct down to 1 Hz, and add SPAN's "Align 0 dB" option: a stationary 0 dBFS sine reads 0 dB whatever the block size, window, overlap or smoothing. With Align off, the output must not change at all.

## Inputs
- `docs/plans/m3c-feature-expansion.md`: design items [5] and [6], and Risk 22.
- `docs/research/T-043-span-m3c-reference.md`, question 4: its recommended Align method and test signal set.
- `source/dsp/SpectrumMapper.*`, `DisplayRange.h`, `SpectrumAnalyser.cpp` (the coherent-gain normalisation), `SpectrumSettings.h`, and the T-005, T-035 and T-039 logs (smoothing padding, margin points, the bin-to-point mapping).
- Working rules as in T-046.

## Scope
1. **1 Hz.**
   - The mapper and `DisplayRange` accept `freqLow` down to 1 Hz.
   - Define the display points below the first FFT bin: a monotone interpolation with no NaN or infinity, and a documented decision on whether bin 0 (DC) takes part.
   - The view clamps and the minimum spans are unchanged.
2. **Align 0 dB.**
   - Add `bool align0dB` to `SpectrumSettings` (default false).
   - When true, compensate the level so that a stationary sine's displayed peak equals its peak level in dBFS. This includes the loss from smoothing, from peak-preserving mapping, and from scalloping where the method covers it.
   - The method follows T-043, or your own derivation with a citation, documented in the header comment.
   - It applies to every curve, the overview, the freeze re-maps and, later, the capture (T-048).
3. Keep the re-map cost inside its budget.

## Acceptance criteria
- [ ] **Align off.** Every frame is bit-identical to the T-046 build, at Fit and zoomed, in every mode.
- [ ] **1 Hz.**
  - With `freqLow` = 1 Hz and every block size from 512 to 65536, frames contain no NaN or infinity, and the points are strictly increasing from 1 Hz.
  - At 48 kHz, block 65536, smoothing off, slope 0: a 5 Hz sine at −6 dBFS reads −6 ± 1.5 dB at its frequency, and a 2 Hz sine at −6 dBFS reads −6 ± 3 dB.
- [ ] **Align on**, at slope 0:
  - a stationary 1 kHz sine at 0 dBFS reads 0 ± 0.5 dB at its peak for every block size (512 to 65536), every window, every overlap and every smoothing setting;
  - sines at 100 Hz and 10 kHz read 0 ± 1.0 dB at block sizes 4096 and above;
  - −20 dBFS sines read −20 dB within the same tolerances.
- [ ] **Align on, noise.** The level of white noise rises by a predictable amount, documented in the header and tested within ±1 dB.
- [ ] **Budget.** The worst-case re-map (as in T-046) stays ≤ 1.0 ms median with Align on (`[.perf]`).
- [ ] Every existing test passes. Warning-clean at `/W4`. `docs/qa/T-047-unit.md` reports PASS.

## Log
- 2026-10-03 vst-architect: Created from `docs/plans/m3c-feature-expansion.md`. The `freqLow` parameter range changes only later, in T-051, so this task makes the DSP ready first.
- 2026-10-04 vst-architect: Align 0 dB method decided from `docs/research/T-043-span-m3c-reference.md` (question 4 and its recommendations):
  - On the analysis thread, whenever an input changes (window, block size, smoothing, slope, sample rate, the point grid), model the bin power of a 0 dBFS 1 kHz sine from the exact window transform at its fractional bin position.
  - Run that power through the real `SpectrumMapper` at slope 0, and take the negative of the displayed peak as the offset.
  - With Align off, the frames stay bit-identical to today.
  - Use the report's 10-group test signal set for the criteria. The researcher has not run or tested the method; if it fails a criterion, record the evidence and return BLOCKED with it rather than tuning tolerances.
  - SPAN confirms only that Align exists, not its exact maths, so the plan's numeric criterion is the reference.
- 2026-10-06 vst-architect: **Amended by `docs/plans/m3-review-spectrum-slots.md`** (M3 review triage):
  - **96 kHz axis (review #3).** The `freqHigh` parameter will reach 96000 Hz in T-051.
    - The mapper and `DisplayRange` accept a home range whose top lies above the Nyquist frequency. The axis spans to `freqHigh`, the data ends at `min(freqHigh, sampleRate / 2)`, and the points above Nyquist carry the "no data" sentinel (not NaN), so the editor draws nothing there.
    - Update the effective-range formulas and their comments, and keep `fLoEff = min(freqLow, fHiEff / 2)` meaningful.
    - Add criteria: no NaN or infinity at any sample rate from 44.1 to 192 kHz with `freqHigh` = 96 kHz. At 48 kHz, every point above 24 kHz is "no data". At 192 kHz, a 90 kHz sine reads its level within ±1 dB at block 4096 or larger.
  - **Per slot.** T-046 introduces per-slot mapping. 1 Hz, the 96 kHz axis and Align apply to every slot with that slot's own `binHz`, block size, window and smoothing. The Align offset is computed per distinct analysis configuration.
  - **Smoothing.** T-062 adds smoothing up to 3 octaves and the M3d curve interpolation. Align must hold for the new smoothing modes; extend the 1 kHz criterion to cover them.
  - **Budget.** The worst-case re-map budget becomes ≤ 2.0 ms for all four slots (it was 1.0 ms for the old curve set).
