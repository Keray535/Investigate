# T-011: Editor shell, LookAndFeel, settings controls and resizing
- Owner: gui-engineer
- Milestone: M3
- Status: DONE
- Depends on: T-008, T-010

## Goal
Replace the placeholder editor with Investigate's real editor frame. This covers:
- a custom LookAndFeel built from the design tokens;
- the region layout from the spec, with placeholder panels where SpectrumView (T-012) and MeterPanel (T-013) will go;
- every settings control bound to APVTS;
- the reset and freeze actions;
- resizing within the spec's limits, with the size persisted in `ViewState`;
- the correct analysis lifecycle calls.

## Inputs
- `docs/design/ui-spec.md`, `docs/design/tokens.json`, `docs/design/mockup.html` and `assets/ui/`.
- `docs/roadmap.md`: the Architecture sections (processor API, state format with `ViewState`, parameter table and constraints).
- `source/PluginProcessor.h` (the editor-facing API from T-009, which is DONE) and `source/params/` (IDs in `ParameterIDs.h`, display text in `ParameterFormatting.h`, the `MeterScale` enum).
- `source/dsp/DisplayRange.h` (the shared effective-range formulas; use it for the constraint display, never a local copy).
- `docs/research/T-023-third-party-notices.md` section 4 (About view notice text).
- The placeholder `source/PluginEditor.h/.cpp` from T-003, which currently wraps a `juce::GenericAudioProcessorEditor`. You own these files and `source/gui/` from now on.

## Corrections to the spec (architect, 2026-10-01)
The spec was written before T-021 finished. Where the following points conflict with `docs/design/ui-spec.md`, this task file wins:
- **Font and license files are not in `assets/ui/fonts/`** (spec §11). T-021 put them in `third_party/fonts/` and compiled them into the single `InvestigateAssets` binary-data target. The generated symbols are `BinaryData::InterMedium_ttf`, `InterSemiBold_ttf`, `JetBrainsMonoRegular_ttf`, `JetBrainsMonoMedium_ttf`, `OFLInter_txt`, `OFLJetBrainsMono_txt` and `LICENSEpffft_txt`. Use them directly; the Segoe UI and Consolas fallback and the "License text missing" placeholder are not needed.
- **Spec §15 open questions 1 to 3 are closed.** JUCE 8's open-source option is AGPLv3 (T-021 corrected `docs/dependencies.md`); the pffft license text is in binary data; the fonts are in binary data. The JUCE notice line itself stays a placeholder until M5 (T-019).

## Design requirements
- **LookAndFeel.** `source/gui/InvestigateLookAndFeel.h/.cpp` builds everything from token constants in a generated or hand-written `source/gui/DesignTokens.h`. There are no magic colour numbers in components.
- **Controls.** Use `juce::AudioProcessorValueTreeState` attachments for every parameter control in the spec. Double-click resets a control to its default. The range constraints from the roadmap (`dbHigh − dbLow ≥ 12`, `freqHigh / freqLow ≥ 2`) are shown the way the spec describes. Parameters are never rewritten by the editor to satisfy the constraints.
- **Level chip text.** The Level (dB range) chip shares one "dB" unit between its two halves, as in the spec's "−90 … +6 dB". Build it from the numeric values of `dbLow` and `dbHigh`, not from `getText()`, which always appends " dB" because hosts need a self-contained string (T-008 open question 2).
- **Placeholders.** Create `SpectrumView` and `MeterPanel` placeholder components with the final class names and files in `source/gui/`, so that T-012 and T-013 fill them in without layout changes.
- **Actions.**
  - The freeze toggle is a view-state flag. It is exposed to `SpectrumView`, but its behaviour is implemented in T-012.
  - The statistics reset calls `requestStatsReset()`.
  - The spectrum reset calls `requestSpectrumReset()`.
- **Lifecycle.** The constructor calls `setAnalysisActive(true)` and the destructor calls `setAnalysisActive(false)`. T-009 is DONE, so the full API exists.
- **Resizing.** Use the default, minimum and maximum sizes from the spec, with a corner resizer or host resizing. The size is saved to and restored from `ViewState` (`editorWidth` and `editorHeight`).
- **Meter collapse.** Implement the meter-collapse toggle from the spec. Its state persists in `ViewState` as `metersCollapsed`.
- **Assets.** Load them through the `InvestigateAssets` binary data target from T-003.
- **Fonts.** Inter v4.1 and JetBrains Mono v2.304 are license-approved (T-022, roadmap Decisions). Load them once (for example as `juce::Typeface::Ptr` members of the LookAndFeel) from the `InvestigateAssets` symbols listed under "Corrections to the spec".
- **Credits.** Provide an About or credits view, reachable as the spec defines it (for example from the Settings popover). It shows the version and the "Software" notices from the short draft in `docs/research/T-023-third-party-notices.md` section 4: fonts, JUCE, the VST3 SDK, pffft and the bundled JUCE libraries, plus the Steinberg trademark line. Follow the layout in `docs/design/ui-spec.md` section 6.2. Keep the JUCE line as the report's placeholder until the M5 licence decision; T-019 finalises it.

## Acceptance criteria
- [ ] Every settings control in the spec exists, is bound to its parameter, shows its value in the spec's format, and resets on double-click. A host automation change updates the control within 1 frame.
- [ ] The layout at the default and minimum sizes matches the spec's region proportions within ±2 px per region edge. A screenshot of each is attached to `docs/qa/T-011-validation.md`.
- [ ] Resizing is clamped to the spec's limits. The size persists across closing and reopening the editor, and across a state save and restore.
- [ ] Opening and closing the editor 50 times in pluginval, or in the validator's harness, gives no leaks (JUCE leak detector clean in Debug) and no crash.
- [ ] No component hard-codes a colour, font size or spacing value outside `DesignTokens.h` (checked by grep in unit-tester's review).
- [ ] pluginval at strictness 10 passes with the new editor.
- [ ] Warning-clean at `/W4`. `docs/qa/T-011-unit.md` and `docs/qa/T-011-validation.md` report PASS.

## QA split
- **unit-tester** writes `docs/qa/T-011-unit.md`: the design-token grep audit; unit tests of any pure helpers (for example the Level chip and value formatting, and size clamping); a check that the `ViewState` read and write code tolerates missing or corrupt properties; and the full existing suite still green in Debug and Release.
- **plugin-validator** writes `docs/qa/T-011-validation.md`: the screenshots at the default and minimum sizes (rendered offscreen, for example with `createComponentSnapshot`, and saved as PNG next to the report); the region-edge measurements against spec §4; the 50 open and close cycles; size persistence across reopen and state round trip; automation-to-control updates; and pluginval at strictness 10.
- Coders never edit `docs/qa/`. Only the testers write reports and verdicts.

## Log
- 2026-10-01 gui-engineer: Implemented the editor shell. New files in `source/gui/`: DesignTokens.h, LayoutMath.h (pure region, control-bar and meter-strip layout, size clamp), ViewStateStore.h/.cpp (tolerant ViewState read/write), ValueFormat.h/.cpp (chip text split, signed dB, Level range, sample-rate text, effective-range clamp text via DisplayRange.h), ControlMath.h/.cpp (drag, wheel, snap, 3-sig-fig), TooltipText.h/.cpp, DisplayContext.h, InvestigateLookAndFeel.h/.cpp (4 bundled typefaces, cached recoloured SVGs, menu, tooltip, scrollbar, grip, text editor), ParamControl.h/.cpp (chip/inline/field/half bound with juce::ParameterAttachment; drag, Shift fine, wheel, 250 ms click defer, menu, text entry, double-click and Alt+click reset, keyboard, automation dot, error flash), RangeChip, ModeSelector, HeaderButton, ControlBar, MeterPanel (placeholder; chrome plus Scale, RMS, Reset), SpectrumView (placeholder; bg-plot, data-rect frame, Freeze flag), SettingsPopover (Settings and About views, cached shadow, notices TextEditor), NoticesText. `PluginEditor.h/.cpp` replaced. Lifecycle: setAnalysisActive(true/false) in ctor/dtor. Size persists on every resize; metersCollapsed persists. Release Standalone builds warning-clean at /W4 and was screenshot-checked against the mockup.
- T-012 notes: `SpectrumView(InvestigateProcessor&)` covers exactly the plot region; use `getDataRect()`; `setFrozen/isFrozen` is the Freeze flag (editor auto-unfreezes on channelMode change). Pull frames on the editor's VBlankAttachment (`pollProcessor()` in PluginEditor.cpp) or add your own. Sample rate and effective range are in `DisplayContext` (editor member); pass it to SpectrumView if needed (ctor change is local to the editor).
- T-013 notes: `MeterPanel(InvestigateProcessor&, DisplayContext&)`; `getLayout()` gives strip-relative rects (barsColumn, rightColumn, rowH, tableY/H, loudnessY, correlationY, verified against ui-spec 4.4 table); `onStatisticsReset` fires after requestStatsReset().
- 2026-10-02 gui-engineer: Round 1 unit-tester FAIL (D1: ViewStateStore ignored XML-string properties after a host state round trip). Fixed in ViewStateStore.cpp (numeric strings and "true"/"1" accepted, NaN-safe clamp); moved the half-highlight padding into DesignTokens.h. Release suite 231 passed, 5 skipped. plugin-validator: PASS (121 harness checks Release and Debug, pluginval strictness 10, 0 px edge deviation). Awaiting unit-tester re-run to flip docs/qa/T-011-unit.md to PASS; status stays IN_REVIEW until then.
- 2026-10-02 gui-engineer: Round 2 unit-tester PASS (65 gui tests, 236/236 suite in Release and Debug). Both QA reports PASS. Status DONE.
