# T-013: Meter panel
- Owner: gui-engineer
- Milestone: M3
- Status: TODO
- Depends on: T-009, T-011

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
- `source/dsp/MeterSnapshot.h`, the processor API from T-009, and the `MeterPanel` placeholder from T-011.

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
- [ ] No allocation in paint or in the update path after the first frame.
- [ ] Warning-clean at `/W4`. `docs/qa/T-013-unit.md` and `docs/qa/T-013-validation.md` report PASS.

## Log
