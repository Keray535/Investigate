# T-047: Spectrum mapper: 1 Hz range and Align 0 dB
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
