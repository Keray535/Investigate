# T-037: SpectrumView: zoom, pan, Fit, indicator and deep-zoom grid
- Owner: gui-engineer
- Milestone: M3b
- Status: TODO
- Depends on: T-034 (mockup approved by the user), T-036, T-039

## Goal
Implement the zoom and pan interactions of ui-spec §5.9 in `SpectrumView`:
- the wheel and Ctrl+wheel zoom around the pointer, and the gutter wheel;
- horizontal pan, plus left-drag and middle-drag pan;
- double-click Fit, with the click reset deferred;
- the zoom indicator;
- the deep-zoom grid.

The editor stays inside the message-thread and paint budgets during a continuous pan. The overview strip and box zoom follow in T-038.

## Inputs
- `docs/design/ui-spec.md` §5.5 to §5.9, §7, §12 and §13.3, as updated by T-034, plus `docs/design/mockup.html` and `tokens.json`.
- `docs/plans/spectrum-zoom-pan.md`: View model, Data flow and Budgets.
- `docs/tasks/T-035-engine-view-range-freeze.md` and `docs/tasks/T-036-processor-view-freeze-api.md`, with their logs: the `DisplayRange` view clamps, the new frame fields, `setSpectrumView`, `setSpectrumFrozen` and the freeze epoch.
- `source/gui/SpectrumView.*`, `SpectrumMath.h`, `DisplayContext.h` and the editor's update path (`InvestigateEditor::pollProcessor` with its 60/s limiter).
- The T-029 log (rendering rules) and the T-012/T-029 CPU harness method.
- `tests/unit/GuiTestEnvironment.h` (T-031), for on-screen tests.

## Scope
1. **`source/gui/ZoomMath.h`** (new, pure):
   - anchored zoom for frequency (log) and level (linear);
   - pan;
   - Fit;
   - the home-change rule (clamp into the new home range and keep the span; Fit if it does not fit);
   - deep-zoom tick selection per §5.9.

   Clamping always goes through `DisplayRange.h`; never re-implement the limits.
2. **`SpectrumView`:**
   - Make `currentMapping()` use the view window.
   - Track the home range from `DisplayContext`, and apply the home-change rule.
3. **Gestures** per §5.9:
   - add `mouseWheelMove`: vertical wheel, Ctrl, Shift fine steps, gutter wheel, horizontal pan; the event is consumed over the data rect and the gutters;
   - left-drag and middle-drag pan, with the cursors;
   - the click-reset defer and double-click Fit;
   - right-click copy, unchanged but following the view.
4. **View propagation:**
   - call `setSpectrumView` when the frequency view changes, at most once per editor update;
   - mirror Freeze through `setSpectrumFrozen`, including auto-unfreeze;
   - drop frames whose epoch is stale;
   - until the re-mapped frame arrives, draw the current frame through the new mapping.
5. **The indicator:** the visible range, the Fit button, and the bin-spacing hint below 16 bins in view.
6. **The grid during an interaction:** draw it without rebuilding the full cache image every frame, following §12 and the T-029 rules (one rectangle list per colour, label images reused while their text is unchanged).
7. The editor opens at Fit. Nothing is written to `ViewState`.

## Acceptance criteria
- [ ] `ZoomMath` unit tests:
  - after one wheel notch, the frequency and the level under the pointer stay within 0.5 px, at 1280×720 and 2560×1440, at 100 % and 150 % scale;
  - the minimum spans hold;
  - a pan never leaves the home range;
  - Fit equals the effective home range;
  - the home-change rule behaves as specified for 6 cases: shrink, grow, shift, a Nyquist cut, a span too large, and the identity.
- [ ] Gesture tests, through simulated `juce::MouseEvent`s and test hooks:
  - every §5.9 row behaves as specified;
  - a single click calls `requestSpectrumReset()` once, 250 ± 20 ms after the release;
  - a double-click fits and does not reset;
  - a drag longer than 4 px never resets;
  - the wheel is consumed over the data rect and the gutters, and passed on elsewhere.
- [ ] **Hover and copy under zoom.** With a synthetic frame at 5 zoom levels, the hover frequency, the level and the clipboard text match the analytical values (3 significant figures for frequency, ±0.1 dB for level).
- [ ] **Responsiveness:**
  - the curves move with the gesture within the same editor update;
  - the re-mapped frame replaces the transformed one within ≤ 2 update intervals at `fftSize` 65536 with 1x overlap;
  - frames with a stale epoch are never drawn after a freeze.
- [ ] The indicator shows exactly while the view differs from Fit. Fit restores the home range. The bin hint shows exactly when fewer than 16 bins are in view.
- [ ] The deep-zoom grid shows at least 2 frequency labels and at least 2 dB labels at every allowed span, with no overlaps and gaps per §5.9, checked through `SpectrumMath`/`ZoomMath` functions.
- [ ] **Performance.** A scripted continuous pan and zoom (one view change per editor update, at least 10 s) runs in the worst displayed case (dual mode, secondary curves, sidechain, meters moving), and meets:
  - whole-editor message-thread CPU ≤ 25 % at 1280×720 and 2560×1440 (T-012/T-029 harness, median of 3, 60 Hz reference; the real editor path at the native refresh rate is also reported);
  - paint p95 ≤ 4 ms and ≤ 8 ms;
  - no per-frame allocation in Investigate code in the pan and zoom paths (text within the ≤ 20 per frame rule).
- [ ] Closing and reopening the editor opens at Fit. The `ViewState` contents are unchanged.
- [ ] Warning-clean at `/W4`. `docs/qa/T-037-unit.md` and `docs/qa/T-037-validation.md` report PASS.

## Log
- 2026-10-03 vst-architect: Created from `docs/plans/spectrum-zoom-pan.md`. Must not start before the user approves the T-034 mockup.
- 2026-10-03 vst-architect: Decisions from the T-035 findings (`docs/tasks/T-035-engine-view-range-freeze.md` log, `docs/qa/T-035-unit.md`). They are part of this task's scope:
  - **Frozen with no frame.** If Freeze is pressed before the analyser has produced a frame, or in the same poll as an `fftSize`, window or layout reset, the engine publishes nothing while frozen. The editor then keeps the frame it was showing when Freeze was pressed and draws it through the current view mapping (no re-mapped detail). If it had no frame, the plot stays empty with the FROZEN badge shown. Test both cases.
  - **Resize and smoothing.** A change of the point count (editor resize) or of smoothing or slope alone does not trigger a re-publish; it appears on the next FFT hop, as before M3b. The editor draws the current frame through its own mapping (each point carries `freqHz`), so a resize must never show a stretched or misplaced curve while waiting. This is accepted for v1.
  - **Frame fields.** `viewFreqLow` and `viewFreqHigh` (double, the clamped view the frame was mapped for), `overviewNumPoints` with `overviewFreqHz[256]` and `overviewDb[256]` (used by T-038), and `freezeEpoch`. Live frames carry the current epoch; frozen frames carry the epoch of their freeze.
  - **Test note.** The bin-to-point mapping is discontinuous where a point edge crosses a half-bin boundary, so a single-bin spike can move by more than 0.05 dB between grids that differ by float rounding. Any test that compares an editor-side transformed curve with a re-mapped frame uses an aligned grid and signals several bins wide, as in the T-035 edge-smoothing test.
- 2026-10-03 vst-architect: T-034 delivered (spec §5.9 and the mockup; the user's approval is pending and is recorded in the T-034 log). Its open questions 5 and 6 were accepted and became T-039, so this task also depends on T-039:
  - **Margin points.** Zoomed frames carry extra points beyond the view (up to 25 % of the visible span per side, clipped to the home range), with `firstVisiblePoint` and `numVisiblePoints`. While a pan or zoom-out waits for the re-mapped frame, draw the margin points that the new view exposes. Points outside the data rect must never be drawn or written into the per-column arrays of the T-029 curve path: clamp or skip them, and test it (no out-of-range column index at any view).
  - **Bin spacing.** The bin-spacing row uses the frame's `binHz`, not sample rate / block size from the current settings.
  - **Legend and FROZEN badge at 920×540** (found by ui-ux-designer, outside T-034's scope). In dual mode with the All-time max secondary curve, Sidechain on and Freeze on, the legend may overlap the FROZEN badge by about 19 px (estimated from font metrics). Measure it in the real editor at 920×540 and 1200×700 with the real fonts and report the numbers in your log. If they overlap, do not invent a layout: record the measurement as an open question, and the architect routes a collision rule to ui-ux-designer as a separate small task. This item does not block the rest of this task.
- 2026-10-03 vst-architect: The user approved the T-034 mockup without changes. That precondition is met. This task starts when T-036 and T-039 are DONE.
