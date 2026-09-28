# T-011: Editor shell, LookAndFeel, settings controls and resizing
- Owner: gui-engineer
- Milestone: M3
- Status: TODO
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
- `source/PluginProcessor.h` (the API from T-008, and from T-009 if it is DONE) and `source/params/`.
- The placeholder `source/PluginEditor.h/.cpp` from T-003. You own these files and `source/gui/` from now on.

## Design requirements
- **LookAndFeel.** `source/gui/InvestigateLookAndFeel.h/.cpp` builds everything from token constants in a generated or hand-written `source/gui/DesignTokens.h`. There are no magic colour numbers in components.
- **Controls.** Use `juce::AudioProcessorValueTreeState` attachments for every parameter control in the spec. Double-click resets a control to its default. The range constraints from the roadmap (`dbHigh − dbLow ≥ 12`, `freqHigh / freqLow ≥ 2`) are shown the way the spec describes. Parameters are never rewritten by the editor to satisfy the constraints.
- **Level chip text.** The Level (dB range) chip shares one "dB" unit between its two halves, as in the spec's "−90 … +6 dB". Build it from the numeric values of `dbLow` and `dbHigh`, not from `getText()`, which always appends " dB" because hosts need a self-contained string (T-008 open question 2).
- **Placeholders.** Create `SpectrumView` and `MeterPanel` placeholder components with the final class names and files in `source/gui/`, so that T-012 and T-013 fill them in without layout changes.
- **Actions.**
  - The freeze toggle is a view-state flag. It is exposed to `SpectrumView`, but its behaviour is implemented in T-012.
  - The statistics reset calls `requestStatsReset()`.
  - The spectrum reset calls `requestSpectrumReset()`.
- **Lifecycle.** The constructor calls `setAnalysisActive(true)` and the destructor calls `setAnalysisActive(false)`. If T-009 is not DONE yet, guard these calls behind the API that exists and log what is pending.
- **Resizing.** Use the default, minimum and maximum sizes from the spec, with a corner resizer or host resizing. The size is saved to and restored from `ViewState` (`editorWidth` and `editorHeight`).
- **Meter collapse.** Implement the meter-collapse toggle from the spec. Its state persists in `ViewState` as `metersCollapsed`.
- **Assets.** Load them through the `InvestigateAssets` binary data target from T-003.
- **Fonts.** Inter v4.1 and JetBrains Mono v2.304 are license-approved (T-022, roadmap Decisions). Load them from `InvestigateAssets` once the ui-ux-designer has added the TTFs to `assets/ui/`. If they are not there yet, use the spec's fallbacks (Segoe UI, Consolas) and log it.
- **Credits.** Provide an About or credits view, reachable as the spec defines it (for example from the Settings popover). It shows the version and the "Software" notices from the short draft in `docs/research/T-023-third-party-notices.md` section 4: fonts, JUCE, the VST3 SDK, pffft and the bundled JUCE libraries, plus the Steinberg trademark line. Follow the layout in `docs/design/ui-spec.md` section 6.2. Keep the JUCE line as the report's placeholder until the M5 licence decision; T-019 finalises it.

## Acceptance criteria
- [ ] Every settings control in the spec exists, is bound to its parameter, shows its value in the spec's format, and resets on double-click. A host automation change updates the control within 1 frame.
- [ ] The layout at the default and minimum sizes matches the spec's region proportions within ±2 px per region edge. A screenshot of each is attached to `docs/qa/T-011-validation.md`.
- [ ] Resizing is clamped to the spec's limits. The size persists across closing and reopening the editor, and across a state save and restore.
- [ ] Opening and closing the editor 50 times in pluginval, or in the validator's harness, gives no leaks (JUCE leak detector clean in Debug) and no crash.
- [ ] No component hard-codes a colour, font size or spacing value outside `DesignTokens.h` (checked by grep in unit-tester's review).
- [ ] pluginval at strictness 10 passes with the new editor.
- [ ] Warning-clean at `/W4`. `docs/qa/T-011-unit.md` and `docs/qa/T-011-validation.md` report PASS.

## Log
