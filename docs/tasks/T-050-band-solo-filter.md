# T-050: BandSoloFilter
- Owner: dsp-engineer
- Milestone: M3c
- Status: TODO
- Depends on: T-044, T-049 (build order)

## Goal
Provide the real-time-safe band-pass filter behind the M3c band solo. While the user holds Ctrl and the left button over the spectrum, the main output plays only a band around the pointer. Engaging, sweeping and releasing must be click-free. When the filter is not engaged, the caller passes audio through untouched; this class must make that simple to guarantee.

## Inputs
- `docs/plans/m3c-feature-expansion.md`: design item [1], the Budgets (filter alone ≤ 3 µs mean per 128-sample stereo block) and Risk 16.
- `docs/research/T-044-host-platform-constraints.md`, question 6: the filter recommendation, with its references and licences.
- `CLAUDE.md`, Real-time safety rules. `source/dsp/` conventions from existing classes (for example `LevelMeter`).
- Working rules as in T-046.

## Scope
1. **`source/dsp/BandSoloFilter.h/.cpp`** (new):
   - `prepare (double sampleRate, int maxBlockSize, int numChannels)` for 1 or 2 channels;
   - `reset()`;
   - `setTarget (bool engaged, float centreHz, float widthOctaves) noexcept`, called once per block by the processor;
   - `process (float* const* channels, int numChannels, int numSamples) noexcept`, in place;
   - `isActive() const noexcept`: true while engaged or still crossfading out. When false, the processor skips `process` and leaves the buffer untouched.
2. **The response.**
   - Band edges `fLo = fc · 2^(−w/2)` and `fHi = fc · 2^(+w/2)`, clamped to 10 Hz … 0.45 · fs.
   - Topology and order per T-044. The target is a flat passband for widths of 1/3 octave or more, and skirts of at least 24 dB per octave.
   - The width ranges from 1/12 to 4 octaves.
3. **Smoothing.** The centre (in log frequency) and the width are smoothed per sample, with a time constant of about 20 ms. TPT structures, or another structure stable under per-sample modulation.
4. **Crossfade.** Engage and release use a 10 ms linear crossfade between the dry and filtered signals. The filter state resets when a new engage starts from fully released. When the release crossfade ends, `isActive()` turns false.
5. Each channel is filtered independently, with no allocation, no locks and no branches whose cost depends on the data.

## Acceptance criteria
- [ ] **Response at steady state**, measured with sines at 48 kHz and 96 kHz, with fc at 100 Hz, 1 kHz and 8 kHz:
  - gain at fc of 0 ± 1 dB for widths from 1/3 to 4 octaves, and 0 ± 3 dB from 1/12 to 1/3 octave;
  - ≤ −20 dB one octave outside each band edge, and ≤ −40 dB two octaves outside, for every width;
  - at fc near the clamps (20 Hz and 20 kHz) the response stays finite and the gain at fc is within ±3 dB.
- [ ] **Sweeps.** Sweep fc from 20 Hz to 20 kHz and back in 500 ms with pink noise at −6 dBFS, at widths of 1/12, 1 and 4 octaves. Result: no NaN or infinity, the output peak never exceeds the input peak + 6 dB, and no single-sample step exceeds what the steady-state filtered signal shows. That is, the step distribution stays within the 99.9th percentile of a non-swept run + 3 dB.
- [ ] **Engage and release.** With a 1 kHz sine at −6 dBFS and fc at 1 kHz, engage and then release. Result:
  - no discontinuity: the maximum sample-to-sample difference stays ≤ 1.1 × that of the steady sine;
  - `isActive()` turns false within 10 ms + one block of the release;
  - after that, a caller that skips `process` produces output bit-identical to the input.
- [ ] **CPU.** Mean ≤ 3 µs per 128-sample stereo block at 48 kHz, engaged and sweeping (`[.perf]`). 0 allocations in `process` and `setTarget` (allocation hook).
- [ ] Warning-clean at `/W4`. `docs/qa/T-050-unit.md` reports PASS.

## Log
- 2026-10-03 vst-architect: Created from `docs/plans/m3c-feature-expansion.md`. The processor wiring (the heartbeat failsafe, filtering after the meters and the FIFO push, writing the buffer only while active) is in T-052.
- 2026-10-04 vst-architect: Filter design decided from `docs/research/T-044-host-platform-constraints.md` (question 6):
  - Use two cascaded 2nd-order TPT state-variable band-passes (Zavalishin, "The Art of VA Filter Design"; Simper/Cytomic SVF papers), with a unity-peak output.
  - The band edges are fc·2^(±W/2), with the width W in octaves. Q per stage = 0.6436 / (2^(W/2) − 2^(−W/2)), from 11.1 at 1/12 octave to 0.17 at 4 octaves. The expected gain is 0 dB at the centre and −3 dB at the edges for every width, with 12 dB/oct skirts. Verify those numbers in the tests.
  - Smooth log2(fc) and W with a one-pole of 10 to 20 ms, and recompute the coefficients every 16 to 32 samples.
  - Engage and release with a 10 ms crossfade, and reset the filter state after release.
  - A Linkwitz-Riley high-pass and low-pass pair is rejected: it loses 11 dB at 1/12 octave and 8.5 dB at 1/3 octave.
  - Licences: the Cytomic papers are usable. Use the Zavalishin PDF as an equations reference only; do not copy its text or figures. Write the code yourself.
