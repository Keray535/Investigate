# T-064: M3d parameters and the Standalone mute default
- Owner: plugin-engineer
- Milestone: M3d
- Status: TODO
- Depends on: T-062, T-063

## Goal
Apply the parameter and Standalone changes from the user's M3 review:
- **#1:** the Standalone must not play the microphone through at launch;
- **#4:** the default level range becomes −18 to −72 dB;
- **#5:** both level edges can be set freely;
- **#6:** Smooth reaches 3 octaves.

## Inputs
- `docs/plans/m3-review-spectrum-slots.md` (M3d scope, "Parameter changes" table, Decisions) and `docs/plans/m3-user-review-findings.md` #1, #4, #5 and #6.
- Roadmap Risk 7 (amended on 2026-10-06) and the roadmap Decisions of 2026-09-29 on T-025 (the custom Standalone app, `source/StandaloneApp.cpp`).
- `source/params/` (`ParameterLayout.cpp`, `ParameterFormatting.*`, `ParameterRefs.h`), `source/PluginProcessor.*`, `source/StandaloneApp.cpp`, and the T-008, T-009, T-025, T-062 and T-063 logs.
- Working rules as in T-062.

## Scope
1. **Standalone monitoring (#1).**
   - The user hears the microphone at launch because the Standalone passes the input straight through.
   - The analyser and the meters must still receive the input. JUCE's own "Mute audio input" option feeds the processor silence, so it **must stay off** (Risk 7).
   - Instead, give the Standalone an output monitoring switch that **defaults to off**: the processor still meters and analyses the input, and only the Standalone's output is silenced.
   - Choose the mechanism and record it in the log, for example:
     - a Standalone-only flag checked in `processBlock` when `wrapperType == wrapperType_Standalone`, which clears the output after metering and the FIFO push;
     - or output muting in the custom Standalone holder.
   - The switch is persisted in the Standalone's own settings, not in the plugin state or presets, and is exposed in the Standalone's Options menu or audio settings dialog if JUCE allows it. If it can only be exposed in the editor, record that as an open question for gui-engineer (T-066).
   - **The VST3 plugin is unaffected.** It stays bit-exact pass-through.
2. **`smoothing` (#6).** Append "1/2 oct", "1 oct", "2 oct" and "3 oct", matching the T-062 `SmoothingMode` values. The existing indices are kept.
3. **`dbLow` and `dbHigh` (#4, #5).**
   - Both ranges become −150 to +30 dB, step 1, linear.
   - The defaults become −72 dB (`dbLow`) and −18 dB (`dbHigh`).
   - The formatting handles the full range.
   - The effective-range rule is already in T-063, so parameters are never rewritten.
4. Update the parameter comments and the roadmap references cited in code.

## Acceptance criteria
- [ ] **Standalone.**
  - On a fresh launch (no saved Standalone settings), the output is silent while the analyser and the meters show the input (an automated test drives the processor in the Standalone wrapper type with a test signal).
  - Turning monitoring on passes the input through bit-exact. The choice persists across a relaunch.
  - JUCE's "Mute audio input" is unchecked.
  - The manual check (open the Standalone, speak, hear nothing, and see the meters move) goes in the validation report.
- [ ] **VST3 unaffected.** The null test stays bit-exact in every layout, with analysis on and off.
- [ ] **Parameters.**
  - `smoothing` has 10 choices, and indices 0 to 5 are unchanged.
  - `dbLow` and `dbHigh` have the new ranges and defaults (a table-driven test).
  - A version-1 state saved with the old defaults (−90 and +6) loads with −90 and +6, not the new defaults.
- [ ] **Current editor.** It builds and works with every new smoothing choice and with level edges anywhere in −150 to +30, with no crash or assertion (a GUI smoke test gated through `GuiTestEnvironment.h`).
- [ ] pluginval at strictness 10 passes. Warning-clean at `/W4`. `docs/qa/T-064-unit.md` and `docs/qa/T-064-validation.md` report PASS.

## Log
- 2026-10-06 vst-architect: Created from the M3 review triage. This reverses the T-008/T-025 "start unmuted" behaviour in spirit: the analyser keeps receiving input, and only the audible output changes. The editor changes for #4 and #5 (the 3 dB grid, the chips) are in T-066.
