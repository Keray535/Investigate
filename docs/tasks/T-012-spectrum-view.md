# T-012: SpectrumView component
- Owner: gui-engineer
- Milestone: M3
- Status: TODO
- Depends on: T-009, T-011

## Goal
Implement `SpectrumView`, the main display of Investigate:
- the frequency and dB grid;
- all spectrum curves from `SpectrumFrame`;
- the hover readout with frequency, note name with cents, and level;
- freeze;
- efficient 60 fps repainting within the paint budget.

## Inputs
- `docs/design/ui-spec.md` (the spectrum sections, curve styles, grid, hover readout and freeze) and `docs/design/tokens.json`.
- `docs/roadmap.md`: Architecture ("SpectrumFrame contents", processor API, parameter table and constraints), Budgets (editor paint, message thread), and Risk 1.
- `docs/research/T-002-dsp-references.md`: section 8 (JUCE 8 rendering on Windows).
- `source/dsp/SpectrumFrame.h`, the processor API from T-009, and the `SpectrumView` placeholder from T-011.

## Design requirements
- **Frame pulling.** Use `juce::VBlankAttachment`. On each vblank, call `getLatestSpectrumFrame`. Repaint only when a new frame arrived, the hover state changed, or the view settings changed. While frozen, stop pulling frames but keep the hover readout working on the frozen frame.
- **Axes.** A log frequency axis from `fLoEff` to `fHiEff`, and a linear dB axis from `dbLoEff` to `dbHigh`. Compute these with the shared `source/dsp/DisplayRange.h` from T-005, never with a local copy of the formulas. Values outside the range are clamped to the plot edges.
- **Static cache.** Cache the grid, labels and background in a `juce::Image`. Redraw the cache only on resize, a range change, a scale change or a sample-rate change.
- **Curves.** Reduce each curve to at most 1 to 2 vertices per physical pixel column, preserving the peaks. Draw primary, primary B (dual), secondary, secondary B and sidechain according to the spec's styles. Honour the flags `hasSecondary`, `hasSidechain` and `showSidechain`.
- **Hover readout.** Show frequency, note name with cents, and level at the cursor, formatted per the spec. Put the frequency-to-x, x-to-frequency, dB-to-y and note-name functions in a separate header (`source/gui/SpectrumMath.h`), so unit-tester can test them without a GUI.
- **Allocation.** No allocation per frame in paint or in the vblank callback. Paths and point buffers are reused.
- **Interactions** (roadmap Scope and the UI spec):
  - Left-click calls `requestSpectrumReset()`.
  - Right-click copies the frequency under the pointer to the system clipboard, formatted per the spec.
  - Freeze turns off automatically when `channelMode` changes.

## Handoff from T-011 (architect, 2026-10-02)
- `SpectrumView (InvestigateProcessor&)` already exists in `source/gui/` and covers exactly the plot region. Draw the grid and curves inside `getDataRect()`. `setFrozen` and `isFrozen` are the Freeze flag. The editor already auto-unfreezes on a `channelMode` change (verified in `docs/qa/T-011-validation.md`); keep that behaviour and do not duplicate it.
- The editor's vblank callback is `pollProcessor()` in `source/PluginEditor.cpp`. Pull frames from there, or give SpectrumView its own `juce::VBlankAttachment`; choose one and log the reason.
- The sample rate and the effective ranges are held in `DisplayContext` (`source/gui/DisplayContext.h`, an editor member). Passing it to SpectrumView is a local editor change that you own.
- **Typographic minus (small fix).** The spec writes negative values with U+2212 (−). `docs/qa/T-011-validation.md` reports that the Level chip "−90" renders like an ASCII hyphen-minus, although the T-011 code and tests use U+2212. Find out whether the string reaches the glyph renderer as U+2212 (for example, check how the literal is built into a `juce::String`; it needs `juce::CharPointer_UTF8` or `juce::String::fromUTF8`). Fix it if the string is wrong; if it is only how JetBrains Mono draws the glyph, log that and change nothing. Use the same minus in every negative value in the hover readout and the dB axis labels.
- **Accepted limitation (no work):** the editor reads `ViewState` only in its constructor, so a state load while the editor is open does not resize it until it is reopened. This is accepted for v1 (roadmap Decisions, 2026-10-02).

## Acceptance criteria
- [ ] Unit tests of `SpectrumMath`: `noteName(440.0)` gives "A4 +0c"; `noteName(261.626)` gives "C4 +0c"; `noteName(27.5)` gives "A0 +0c"; `noteName(452.6)` gives "A4 +49c" (+48.9 cents, rounded); and `noteName(452.9)` gives "A#4 −50c" (+50.03 cents rounds to the next semitone). The x↔frequency and y↔dB round trips are exact within 1e-6 relative error.
- [ ] Feeding a −6 dBFS 1 kHz sine through the processor places the peak vertex at the pixel column for 1 kHz (±1 px) and at the y position for −6 dB within the Hann scalloping bounds from T-005.
- [ ] The hover readout matches the spec's format and follows the cursor without flicker. Freeze stops curve updates within 1 frame, and unfreezing resumes them. Changing `channelMode` while frozen unfreezes the view.
- [ ] Left-click clears the all-time curves by the next frame. Right-click puts the frequency under the pointer on the clipboard, and it matches the hover readout.
- [ ] Each curve type and each flag combination (dual, secondary, sidechain on or off) renders as in the spec. Screenshots are attached to `docs/qa/T-012-validation.md`.
- [ ] Paint time, measured by rendering the editor into a `juce::Image` with an animated synthetic frame for 300 frames, is p95 ≤ 4 ms at 1280×720 and p95 ≤ 8 ms at 2560×1440, in Release on the development machine. If it is missed, report the numbers and propose OpenGL as an open question. Do not switch to OpenGL without the architect's approval.
- [ ] No allocation in paint or the vblank callback after the first frame, checked by an allocation-counting test or by audit.
- [ ] Warning-clean at `/W4`. `docs/qa/T-012-unit.md` and `docs/qa/T-012-validation.md` report PASS.

## QA split
- **unit-tester** writes `docs/qa/T-012-unit.md`: the `SpectrumMath` tests, the allocation check for paint and the vblank path, the design-token grep audit for the new code, and the full suite green in Debug and Release.
- **plugin-validator** writes `docs/qa/T-012-validation.md`: the 1 kHz sine placement test, the hover, freeze, click and right-click checks, the screenshots of each curve and flag combination, the paint benchmark (p95 at 1280×720 and 2560×1440, Release), and pluginval at strictness 10. The existing `InvestigateEditorCheck` harness target in `tools/render-harness/` (built with `-DPLUGIN_BUILD_HARNESS=ON`; the configure preset sets it OFF) can be extended for this.
- Coders never edit `docs/qa/`. Only the testers write reports and verdicts.

## Log
