# T-013: Meter panel
- Owner: gui-engineer
- Milestone: M3
- Status: TODO
- Depends on: T-009, T-011, T-029, T-030

## Goal
Implement `MeterPanel`, which shows:
- L/R level bars with peak and RMS, peak hold and clip indicators with counts;
- numeric peak max, true-peak max, RMS and crest readouts;
- LUFS momentary, short-term and integrated, LRA, and max M and S;
- the correlation bar.

The panel applies the `meterScale` display offsets and wires up the statistics reset.

## Inputs
- `docs/design/ui-spec.md` (the meter strip, number formats and ballistics) and `docs/design/tokens.json`.
- `docs/roadmap.md`: Architecture (MeterSnapshot contents, the parameters `meterScale` and `rmsWindow`, processor API).
- `docs/research/T-002-dsp-references.md`: section 6 (K-system scales and the AES17 offset).
- `source/dsp/MeterSnapshot.h`, the processor API from T-009, and the `MeterPanel` placeholder from T-011 (`MeterPanel (InvestigateProcessor&, DisplayContext&)`; `getLayout()` returns the strip-relative rects tested against spec §4.4; `onStatisticsReset` fires after `requestStatsReset()`).
- `docs/tasks/T-029-editor-render-cpu.md`: its log ends with "rendering rules" for this panel. Follow them. Direct2D cost on the message thread depends on what is repainted, so repaint only the changed rectangles and never the whole editor per vblank.
- `docs/qa/T-012-validation.md` and `docs/qa/T-029-validation.md`: the CPU measurement method (`spectrum_check` in `tools/render-harness/`, `d2d` mode).

## Design requirements
- **Updates.** Poll `getMeterSnapshot()` from the same vblank or timer path as the rest of the editor, at 30 Hz or more.
- **Ballistics.** Peak hold, fall rates and clip latch are implemented in the editor, exactly as the spec defines them. They never change the snapshot values.
- **Meter scale.**
  - dBFS mode shows RMS as reported (unreferenced).
  - K-20, K-14 and K-12 add +3.01 dB (the AES17 sine reference), then place 0 on the scale at −20, −14 or −12 dBFS respectively.
  - The ticks and colour zones follow the spec.
- **Crest factor.** Computed as peak max minus RMS, or as the spec defines it.
- **Headroom.** 0 dBTP minus TP max, per the roadmap Scope.
- **Clip counts.** Show both the sample and the true-peak clip counts from the snapshot, as the spec defines.
- **Balance.** Show the L/R balance readout from `MeterSnapshot`. A negative value means the left channel is louder.
- **Collapse.** Honour the meter-collapse toggle from T-011 or the spec: when collapsed, the panel takes its minimum size and skips painting.
- **Silence.** "−inf" and silent values are formatted as the spec says. No NaN is ever shown.
- **Mono input.** When `isMono` is set, the panel shows a single level bar and dims the correlation meter, as the spec defines.
- **Math helpers.** Put the scale-mapping and formatting functions in `source/gui/MeterMath.h`, so they are unit-testable without a GUI.
- **CPU headroom.** T-029 left the spectrum at 14.1 % (1280×720) and 16.8 % (2560×1440) against the 25 % whole-editor budget. The spectrum repaints at 30 fps in its paced cases (see the T-029 log), so a meter that repaints at 60 fps adds a window present (about 4 to 6 % of a core) on the other vblanks. Follow the T-029 rendering rules, especially rule 4 (repaint a bar only when its physical-pixel value changes) and rule 5 (align with the spectrum's repaints where the spec's update rates allow it).

## Carry-over from T-029 (same owner and files)
1. **O-1, clip-edge seam.** In `SpectrumView`, `fillUnderCurve` computes `minTop` and `maxTop` over the clipped columns only, so a hover or toast repaint can leave a 1-level seam at the clip edge. Compute them over all columns (the tops array is already full width), so a clip-limited paint is bit-identical to the full paint. unit-tester then un-hides the `[strict]` test "SpectrumView: clip-limited paint is bit-identical to the full paint" and it must pass.
2. **O-A/O-2, fractional display scaling.** At 1920×1080, CPU rises by 2 to 4.5 points at 110 % and 175 % Windows scaling, but not at 125 %, 150 % or 200 %; case (c) reached 17.9 % at 110 %. A suspected cause is sprite-batch rectangles whose transformed width falls just below 1.0 px, which makes JUCE fall back from the sprite path (`juce_DirectX_windows.cpp`, around line 541). Try the small width slack (about 0.002 px) suggested in `docs/qa/T-029-unit.md` O-2, and log the before and after numbers. Keep the change only if it lowers the elevated scales and leaves the rest unchanged.

## Acceptance criteria
- [ ] Unit tests of `MeterMath`:
  - In K-20 mode, a sine at −20 dBFS peak gives an RMS display value of −20 dBFS (reported −23.01, plus 3.01), which is 0 on the K-20 scale, ±0.01.
  - In dBFS mode, the same sine displays −23.01 ±0.01.
  - The formatting of −inf, −0.04 and +0.05 values matches the spec.
- [ ] A −6 dBFS 1 kHz sine through the processor shows a peak readout of −6.0 dB and an RMS readout of −9.0 dB in dBFS mode. A stereo −23 dBFS sine shows M, S and I of −23.0 LUFS after 3 s.
- [ ] Clip indicators latch and show counts equal to the sample and true-peak clip counts in the snapshot. The statistics reset clears them and all max values within 1 UI frame of the reset taking effect.
- [ ] Headroom shows 0 dBTP minus TP max, formatted per the spec. Balance shows −6.0 dB for L at −6 dBFS with R at −12 dBFS, after 3 s.
- [ ] The correlation bar shows +1 for L = R, −1 for L = −R, and about 0 for independent noise, with the spec's smoothing. The mono layout renders as specified.
- [ ] The rendering of each meter scale mode matches the spec. Screenshots are attached to `docs/qa/T-013-validation.md`.
- [ ] Allocation (roadmap Decisions 2026-10-03): 0 allocations by Investigate code after the first frame in the bar, correlation and ballistics paths, checked by an allocation-counting test. Text readouts rebuild their strings only when the displayed text can change (at most 10 Hz, spec §12), with ≤ 20 allocations per frame. Allocations inside JUCE's own text layout are excluded.
- [ ] Paint cost (roadmap Decisions 2026-10-03): with the editor in a real on-screen Direct2D window and meters animating, `MeterPanel` paint adds ≤ 1 ms p95 at 1280×720 and 2560×1440.
- [ ] Whole-editor message-thread CPU (spectrum and meters both animating, the T-029 case matrix, net, at least 12 s, median of 3 runs) is ≤ 25 % of one core at both 1280×720 and 2560×1440 at 100 % scaling, and at 1920×1080 at 110 %, 125 % and 150 % scaling.
- [ ] Carry-over 1: the `[strict]` clip-limited paint test is un-hidden and passes. Carry-over 2: the before and after numbers at the fractional scales are in the log.
- [ ] No regression in T-012 and T-029: the 92 spectrum harness checks pass, and the spectrum screenshots match the T-029 set apart from the O-1 fix.
- [ ] Warning-clean at `/W4`. `docs/qa/T-013-unit.md` and `docs/qa/T-013-validation.md` report PASS.

## QA split
- **unit-tester** writes `docs/qa/T-013-unit.md`: the `MeterMath` tests (scales, the AES17 offset, formatting of −inf, −0.04 and +0.05), ballistics and clip-latch logic, the allocation tests for the bar, correlation and ballistics paths and for text, the un-hidden `[strict]` SpectrumView test, the design-token grep audit for the new code, and the full suite in Debug and Release.
- **plugin-validator** writes `docs/qa/T-013-validation.md`: the processor-driven readouts (−6 dBFS sine, −23 LUFS, clip counts and reset, headroom, balance, correlation, mono layout), screenshots of every meter-scale mode and of the mono layout, the meter paint cost, the whole-editor CPU matrix (extend `spectrum_check` or add a meter check in `tools/render-harness/`), the T-012 regression re-run, and pluginval at strictness 10.
- Coders never edit `docs/qa/`. Only the testers write reports and verdicts.

## Log
