# T-062: Spectrum mapper: smooth low-frequency curve and wider smoothing
- Owner: dsp-engineer (confirmed by T-061 on 2026-10-06: the cause is in the mapper)
- Milestone: M3d
- Status: TODO
- Depends on: T-061; T-071 (build order: one C++ task at a time; T-071 follows T-037 and T-072)

## Goal
Make the spectrum curve smooth at every frequency and at every Smooth setting, including Off (review finding #2, high priority). Add the smoothing widths 1/2, 1, 2 and 3 octaves (review finding #6) to the DSP.

## Inputs
- `docs/research/T-061-review-bug-diagnosis.md`, section #2: the cause and the recommended interpolation method.
- `docs/plans/m3-review-spectrum-slots.md` (M3d scope, Budgets) and `docs/plans/m3-user-review-findings.md` #2 and #6.
- `source/dsp/SpectrumMapper.*`, `SpectrumSettings.h`, `SpectrumEngine.*`, `DisplayRange.h`, and the T-005, T-035 and T-039 logs (padding, margins, the overview, bit-identity rules).
- Working rules from the roadmap handoff:
  - one C++ task at a time;
  - timing assertions only in `[.perf]`;
  - `ctest --timeout 300`;
  - build in `build/` through the presets;
  - coders never edit `docs/qa/`;
  - commits stage explicit paths only;
  - check for leftover build processes before handing over.

## Scope
1. **Low-frequency interpolation.** Replace the method that produces the steps, per T-061, with the recommended smooth interpolation of the bin magnitudes. Use the log-frequency domain where the report says so. Apply it to every curve the mapper produces, the overview, the zoom margins and the freeze re-maps. Peak-preserving mapping at high frequencies, where several bins fall on one point, stays as it is unless T-061 shows it contributes.
2. **Wider smoothing.** Extend `SmoothingMode` with 1/2, 1, 2 and 3 octaves, appended after the existing values, so the existing enum values and the `smoothing` parameter indices keep their meaning. The parameter itself is extended by T-064. Smoothing padding past the view edges and the home-range clipping rule (roadmap Decision of 2026-10-03) apply as today. For wide windows, use an efficient method (for example prefix sums in the log domain), so the re-map budget holds.
3. Comments cite the method's sources.

## Acceptance criteria
- [ ] **Smoothness (#2).**
  - At 48 kHz, with Smooth Off, slope 0, and pink noise averaged to steady state, at block sizes 4096 and 65536 and 2048 visible points over 20 Hz to 20 kHz:
    - the absolute second difference of the displayed dB values between neighbouring points below 300 Hz is never larger than the 95th percentile of the same measure above 1 kHz times 1.5;
    - no two consecutive point-to-point slopes below 300 Hz differ by more than 6 dB per point.

    If T-061 proposes a better smoothness metric, use it, and record the reason in the log.
  - The same holds with every Smooth setting.
- [ ] **Accuracy.** A sine at an exact bin centre reads its level within 0.1 dB at its frequency (Smooth Off, slope 0), at 100 Hz, 1 kHz and 10 kHz. Two sines 1/3 octave apart at 60 and 76 Hz, at block 16384, stay resolved as two peaks with a dip of at least 3 dB.
- [ ] **Wider smoothing.** At 1/2, 1, 2 and 3 octaves, the smoothed white-noise curve is flat within ±0.5 dB, and a sine's smoothed peak matches the analytic fractional-octave average within 0.5 dB.
- [ ] **No other regression.** Above 1 kHz, at Smooth 1/12 octave and coarser, frames stay within 0.05 dB of the current build. Every existing test passes in Debug and Release, with tolerances adjusted only where this task intentionally changes values, each listed in the log.
- [ ] **Budget.** The worst-case re-map (every curve, 3-octave smoothing, minimum span) takes ≤ 1.0 ms median on the analysis thread (`[.perf]`). The view-change-to-frame budget (p95 ≤ 10 ms) still holds.
- [ ] 0 allocations in `processPending` after `prepare` (allocation hook). Warning-clean at `/W4`. `docs/qa/T-062-unit.md` reports PASS. Because the drawn curve changes, `docs/qa/T-062-validation.md` reports PASS on a before-and-after render at 1200×700, with an image of the low end, and pluginval at strictness 10.

## Log
- 2026-10-06 vst-architect: Created from the M3 review triage. This is the highest-priority fix in M3d. The user rejects "raise Smooth" as a workaround. The `smoothing` parameter choices are appended in T-064; until then, test the new modes through `SpectrumSettings` directly.
- 2026-10-06 vst-architect: T-061 is DONE (`docs/research/T-061-review-bug-diagnosis.md`, section #2). The owner stays dsp-engineer. What this task takes from it:
  - **Cause.** `SpectrumMapper::mapOnGrid` (`SpectrumMapper.cpp`, about lines 291 to 304) interpolates linearly in the power domain between bins wherever the points are denser than the bins. On the dB axis this draws a polyline with a corner at every bin centre. The 1/12-octave boxcar (about lines 81 to 93 and 331 to 379) is narrower than the bin spacing below about 197 Hz at block 4096 and 48 kHz, and with Smooth Off nothing hides the corners. The editor path (`reduceToColumns`, `tracePolyline`, 0.04 px tolerance) is faithful and does not change in this task.
  - **Method.** Use monotone cubic Hermite interpolation (Fritsch and Carlson 1980, known as PCHIP) of the bin levels **in dB**, over the log-frequency axis where the report says so. Convert back to power before the existing boxcar smoothing. It reproduces the bin centres exactly, and it is monotone inside each bin interval, so it never overshoots the neighbouring bins and the peak-accuracy criterion holds. Cite Fritsch and Carlson 1980 in the comments. SciPy's `PchipInterpolator` (BSD-3) may be used as a reference. Copy no non-permissive code.
  - **Smoothness metric.** The report's synthetic figures show a maximum second difference of 0.41 dB with the current method and 0.09 dB with PCHIP, at 2048 points between 50 and 300 Hz. The acceptance criteria above stay as written; if you use a different metric, record why in the log.
  - **Cost.** The report estimates about 0.1 ms per curve at 2048 points, which is not measured. Measure it in the `[.perf]` budget case and record the number.
  - At 65536 points the interpolation region ends near 217 Hz, so the defect is mostly visible at block 4096 and smaller. Test both sizes, as the criteria require.
