# T-013: Meter panel
- Owner: gui-engineer
- Milestone: M3
- Status: DONE
- Depends on: T-009, T-011, T-029, T-030, T-031

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

3. **Pacing test hook (from T-031, small).** Add a test-only accessor to `SpectrumView`, for example `bool isFrameRepaintPendingForTesting() const noexcept`, next to the existing test hooks. unit-tester can then check the pacing deferral without a desktop window. The window-based pacing test stays as it is.

## Test environment notes (from T-031)
- On-screen GUI tests use `tests/unit/GuiTestEnvironment.h` (`pump`, `pumpUntil` capped at 10 s, `VBlankCounter`, `Watchdog`). Any new window-based test must gate on the environment and SKIP with a reason when paints or vblanks do not arrive. It must never wait without a deadline.
- Run `ctest` with `--timeout 300` until T-014 adds a default timeout to the presets.
- Windows larger than the display (for example the 2560×1440 test window on the 1920×1080 development displays) can lose repaints of off-screen regions. Treat 2560×1440 results as provisional until confirmed on a native 1440p display.

## Acceptance criteria
- [ ] Unit tests of `MeterMath`:
  - In K-20 mode, a sine at −20 dBFS peak gives an RMS display value of −20 dBFS (reported −23.01, plus 3.01), which is 0 on the K-20 scale, ±0.01.
  - In dBFS mode, the same sine displays −23.01 ±0.01.
  - The formatting of −inf, −0.04 and +0.05 values matches the spec.
- [ ] A −6 dBFS 1 kHz sine through the processor shows a peak readout of −6.0 dB and an RMS readout of −9.0 dB in dBFS mode. A stereo −23 dBFS sine shows M, S and I of −23.0 LUFS 3 s after a statistics reset, with the sine already running (clarified 2026-10-03; cold-start behaviour is tracked in T-033).
- [ ] Clip indicators latch and show counts equal to the sample and true-peak clip counts in the snapshot. The statistics reset clears them and all max values within 1 UI frame of the reset taking effect.
- [ ] Headroom shows 0 dBTP minus TP max, formatted per the spec. Balance shows −6.0 dB for L at −6 dBFS with R at −12 dBFS, after 3 s.
- [ ] The correlation bar shows +1 for L = R, −1 for L = −R, and about 0 for independent noise, with the spec's smoothing. The mono layout renders as specified.
- [ ] The rendering of each meter scale mode matches the spec. Screenshots are attached to `docs/qa/T-013-validation.md`.
- [ ] Allocation (roadmap Decisions 2026-10-03): 0 allocations by Investigate code after the first frame in the bar, correlation and ballistics paths, checked by an allocation-counting test. Text readouts rebuild their strings only when the displayed text can change (at most 10 Hz, spec §12), with ≤ 20 allocations per frame. Allocations inside JUCE's own text layout are excluded.
- [ ] Paint cost (roadmap Decisions 2026-10-03): with the editor in a real on-screen Direct2D window and meters animating, `MeterPanel` paint adds ≤ 1 ms p95 at 1280×720 and ≤ 1.5 ms p95 at 2560×1440 (the 2560×1440 limit was amended on 2026-10-03; see the Log).
- [ ] Whole-editor message-thread CPU (spectrum and meters both animating, the T-029 case matrix, net, at least 12 s, median of 3 runs) is ≤ 25 % of one core at both 1280×720 and 2560×1440 at 100 % scaling, and at 1920×1080 at 110 %, 125 % and 150 % scaling.
- [ ] Carry-over 1: the `[strict]` clip-limited paint test is un-hidden and passes. Carry-over 2: the before and after numbers at the fractional scales are in the log.
- [ ] No regression in T-012 and T-029: the 92 spectrum harness checks pass, and the spectrum screenshots match the T-029 set apart from the O-1 fix.
- [ ] Warning-clean at `/W4`. `docs/qa/T-013-unit.md` and `docs/qa/T-013-validation.md` report PASS.

## QA split
- **unit-tester** writes `docs/qa/T-013-unit.md`: the `MeterMath` tests (scales, the AES17 offset, formatting of −inf, −0.04 and +0.05), ballistics and clip-latch logic, the allocation tests for the bar, correlation and ballistics paths and for text, the un-hidden `[strict]` SpectrumView test, the design-token grep audit for the new code, and the full suite in Debug and Release.
- **plugin-validator** writes `docs/qa/T-013-validation.md`: the processor-driven readouts (−6 dBFS sine, −23 LUFS, clip counts and reset, headroom, balance, correlation, mono layout), screenshots of every meter-scale mode and of the mono layout, the meter paint cost, the whole-editor CPU matrix (extend `spectrum_check` or add a meter check in `tools/render-harness/`), the T-012 regression re-run, and pluginval at strictness 10.
- Coders never edit `docs/qa/`. Only the testers write reports and verdicts.

## Log
- 2026-10-02 gui-engineer: Round 1 implementation. New `source/gui/MeterMath.h` (scale mapping, fixed-buffer formatting, ballistics, crest ring, correlation filter, stale detector, tick sets; no allocation), `MeterPanel` filled in (cached static layer, flat pixel-rectangle bars batched one `fillRectList` per colour, quantised repaint, 10 Hz text cells, correlation bar, mono layout, correlation tooltip, held-value reset), meter-layout constants in `DesignTokens.h` (`tokens::meter`), `TooltipText::correlation`, editor wiring (`pollProcessor` passes the snapshot after `spectrumView.pollFrame()`).
  - Carry-over 3: `SpectrumView::isFrameRepaintPendingForTesting()` added. Also `isFramePaced()` and `requestedRepaintInLastPoll()` (used by the editor to align meter repaints with the spectrum's, T-029 rule 5).
  - Carry-over 1 (O-1): the committed `fillUnderCurve` already computes `minTop`/`maxTop` over all columns (T-029 final code); `InvestigateUnitTests "[strict]"` passes on this tree. unit-tester only has to un-hide the test.
  - Carry-over 2 (O-A/O-2): the 0.002 px slack is already in the committed code (`spriteSlackPx`). Measured with `InvestigateSpectrumCheck <dir> 60 d2d 12 1920x1080@S 1` (case a, net %, 5 runs each, Release): S=1.0: slack 0.002 `13.1 12.5 10.3 12.4 13.1`, slack 0 `12.9 11.6 10.6 11.6 13.3`; S=1.1: 0.002 `14.9 14.0 15.2 13.3 15.0`, 0 `20.1 18.9 19.0 19.2 19.6`; S=1.75: 0.002 `12.2 12.7 13.6 12.6 13.4`, 0 `17.6 17.7 16.4 18.1 18.3`. So the slack is what keeps the sprite path at fractional scales (about -4.5 points at 1.1 and -5 at 1.75) and stays. Larger slack (0.02, 0.1) and snapping the plot to the physical pixel grid were also tried; neither gave a measurable gain beyond the noise (the machine had +-3 points of background noise during these runs), so neither was kept. The remaining +2 points at 1.1 and 1.75 against 1.0 is unexplained; it is the same as the T-029 numbers.
- 2026-10-03 vst-architect: The gui-engineer session stopped at the account usage limit (HTTP 429) during validation round 1. State at the stop:
  - `docs/qa/T-013-unit.md` is PASS in round 2. D-1 (26 allocations per text refresh; the limit is 20) and D-2 (a mis-encoded U+2212 in the correlation tooltip) were fixed by gui-engineer in round 2, which was not logged here. 327 tests pass in Debug and Release.
  - plugin-validator had started `tools/render-harness/meter_check.cpp` and `docs/qa/T-013-logs/func_1..8.log`. No `docs/qa/T-013-validation.md` exists yet.
  - The architect dispatches plugin-validator directly to finish validation round 1. Defects go back to gui-engineer.
  - Answer to unit-tester open question 1: yes. In K-14 and K-12 a −20 dBFS sine reads −6.0 and −8.0 K, because 0 K sits at −14 and −12 dBFS (sine-referenced RMS: −23.01 + 3.01 + N). Open question 2 is moot, because D-1 was fixed within the limit.
- 2026-10-03 vst-architect: **Validation round 1 is FAIL** (`docs/qa/T-013-validation.md`), with 3 defects and 3 observations. Everything else passes: the readouts, all scales, the mono and collapsed layouts, the 92 spectrum checks and screenshots, 50 open and close cycles, pluginval at strictness 10, and the Standalone launch. Whole-editor CPU (net, 60 Hz forced) has a worst median of 10.8 % at 1280×720, 11.9 % at 2560×1440, and 20.0 / 10.8 / 13.4 % at 1920×1080 with 110 / 125 / 150 % scaling. That passes the 25 % gate. Decisions:
  - **D-1 (reset text lag, up to 98 ms): fix.** A statistics reset is a user action, not a periodic update. It forces an immediate text refresh, which is an accepted exception to the 10 Hz text rule in spec §8.1 and §12. All cleared values must show within one UI frame.
  - **D-2 ("MONO" truncated to "MON"): fix.** Size the label rect from the text width or a token, so that "MONO" fits at every scale.
  - **D-3 (meter paint p95 of 1.001 ms against 1 ms at 2560×1440): limit amended to ≤ 1.5 ms at 2560×1440.** The architect chose the 1 ms figure as a share of the budget. The real gates, editor paint at 8 ms and message-thread CPU at 25 %, pass with a wide margin (11.9 % CPU at that size). `paint()` time is mostly command recording, and the cost grows with panel height. 1280×720 stays at ≤ 1 ms.
  - **O-2 (the editor repaints at the display's vblank rate, 144 Hz here): fix.** Spec §8.1 says at most 60 Hz. Throttle the editor's vblank handler (`pollProcessor`), or the meter's repaint requests, to at most 60 updates per second on faster displays. Spectrum pacing (30 / 60 fps) is unchanged.
  - **O-3 (M max shows "−153.7" after a reset in silence): fix in the editor.** M max and S max values below −99.9 LUFS display as the spec's silent form, the same as the live M and S ("−inf"). The DSP keeps reporting the true value.
  - **O-1 (cold start: I reads −23.1/−23.2 and LRA 0.8 to 1.1 after 3 s of a steady sine): not a GUI matter.** The criterion is clarified to mean "after a reset with the signal running", which the validator measured at −23.0. Whether `LoudnessMeter` includes partly filled gating blocks or short-term windows at the start of a measurement goes to **T-033** (dsp-engineer, M4), checked against libebur128.
  - Round 2 for gui-engineer covers D-1, D-2, O-2 and O-3. unit-tester re-checks the formatting and allocation tests. plugin-validator re-runs a targeted round: reset timing, the mono label, the repaint rate at the native 144 Hz and at 60 Hz, M max formatting, a quick CPU re-check of the worst cases at 60 Hz and 144 Hz, the 92 spectrum checks and pluginval.

- 2026-10-03 gui-engineer: Round 2 (D-1, D-2, O-2, O-3). Debug and Release build clean for plugin and tests; the only warnings are two C4996 (`Displays::userArea`) in `tools/render-harness/meter_check.cpp` (plugin-validator's file). `ctest -E pluginval --timeout 300` passes (Release; 5 Tech 3341/3342 cases skipped as before).
  - D-1: the click already forced the text timer, but the processor applies the reset a block later, so the next snapshot still carried the old maxima. `resetHeldValues()` now sets `forceTextUntilMs` (+`tokens::meter::resetForceTextMs`, 250 ms); `update()` refreshes text on every call in that window. `setCell` only marks changed cells, so allocations per refresh are unchanged.
  - D-2: the mono label rect is `tokens::meter::monoLabelW` (44 px) centred on the bar; L/R keep `barW + 2*letterPad`.
  - O-2: `MeterMath::UpdateRateLimiter` (rolling 1/60 s deadline, 1 ms tolerance) gates `InvestigateEditor::pollProcessor`. The spectrum's pacing is time-based (`minIntervalMs`), so it is unaffected; at 144 Hz the editor now runs at 60 updates per second on average.
  - O-3: `MeterMath::formatLoudnessMax` (M max, S max): "-inf" below -99.9, dash when non-finite. DSP unchanged.
  - Next: unit-tester re-check, then plugin-validator targeted round 2.
- 2026-10-03 gui-engineer: Round 2 QA results. unit-tester: PASS (round 3 section, 334/334 Debug and Release, 8 new tests). plugin-validator: PASS at the 60 Hz reference (D-1 8.4 ms median / 15.4 ms worst, MONO full at 100/110/125/150 %, meters capped at 60/s at 144 Hz, O-3 "-inf", CPU worst 9.9 / 12.9 / 20.1 %, meter paint p95 0.989 ms at 1280x720 and 1.268 ms at 2560x1440, 92/92 spectrum checks, pluginval 10 SUCCESS). Four findings for the architect, all at 144 Hz only: F-1 (the 60/s cap lands on 6.9 ms vblanks, so reset lag reached 20.8 ms against 17 ms and stale detection 273-286 ms against 250 ms), F-2 (1920x1080 at 110 %, case b, 29-32 % CPU, but the harness bypasses the editor's limiter), F-3 (paced spectrum 24 fps at 144 Hz, 2560x1440), F-4 (S max dash against M max "-inf" for an exact -inf from the DSP). Awaiting architect decisions; no further code change made. Rounds used: 2 of 3.
- 2026-10-03 vst-architect: **Accepted; status DONE.** `docs/qa/T-013-unit.md` is PASS (round 3 section). `docs/qa/T-013-validation.md` round 2 is PASS at the 60 Hz reference. Decisions on the four findings that occur only at 144 Hz:
  - **F-1, accepted, with "one UI frame" defined as one editor update interval.** With the 60/s cap on a 144 Hz vblank grid, an update interval is 13.9 or 20.8 ms. Reset clearing within one update interval (≤ 21 ms here) meets the criterion. Stale detection within 250 ms plus 2 update intervals is accepted. The difference is not visible.
  - **F-2, accepted for M3, and the real path is now gated in T-016.** The harness fed spectrum frames past the editor's limiter, so the spectrum repainted 74 to 93 times per second, which the live editor never does. The live path is capped at 60 updates per second, and the same case at 60 Hz measures 20.1 %. T-016 now measures message-thread CPU through the real editor path (`pollProcessor` with the limiter) at the display's native refresh rate.
  - **F-3, accepted for now.** The paced spectrum runs at about 24 fps instead of 30 at 144 Hz with 2560×1440. It is part of the user's frame-pacing review point. If the user finds large windows choppy, a follow-up makes the pacing interval follow the update grid, for example "every second update".
  - **F-4, no change.** It is consistent with the semantics: "−inf" means silence was measured (M max has a value after a reset in silence), and the dash means no value yet (S max needs 3 s of short-term data). This matches ui-spec §8.4.
  - **Observation (test #315 timed out once in Debug while a build ran in parallel; it passes in isolation):** added to T-017 as a watch item, to be run under CPU contention.
