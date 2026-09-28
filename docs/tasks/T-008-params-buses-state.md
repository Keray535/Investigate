# T-008: Parameters, buses and state
- Owner: plugin-engineer
- Milestone: M2
- Status: DONE
- Depends on: T-001, T-003

## Goal
Replace the placeholder processor's empty shell with Investigate's real host-facing surface:
- the complete parameter layout from the roadmap, defined once in `source/params/`;
- the main and sidechain bus layouts;
- zero latency and tail;
- versioned state save and restore, including the `ViewState` child the editor uses.

DSP wiring is not part of this task; it is T-009.

## Inputs
- `docs/roadmap.md`: the Architecture sections (parameter table and constraints, buses and channel layouts, state format, identity, processor API) and Decisions. Use the parameter defaults as they stand in the roadmap when you start: T-001's corrections are applied to the roadmap before this task is dispatched.
- `docs/research/T-002-dsp-references.md`: section 9 (Standalone input muting).
- The placeholder `source/PluginProcessor.h/.cpp` from T-003. You own these files from now on.
- `CLAUDE.md`: Real-time safety rules and Code conventions.

## Design requirements
- **Parameter definitions.** `source/params/ParameterIDs.h` holds the ID constants. `source/params/ParameterLayout.h/.cpp` provides `createParameterLayout()`. Every parameter in the roadmap table must appear with the exact ID, range, step, skew, default, unit label and version hint 1. Choice parameters use the choice labels from the table.
- **Value-to-text.** Provide string conversion functions that show units, for example "4096", "4x", "1000 ms", "4.5 dB/oct", "20.0 Hz", "−90 dB" and "1/12 oct". Also provide the reverse conversion for host text entry.
- **Parameter access helper.** Add a small helper, for example `ParameterRefs`, that caches `std::atomic<float>*` pointers from `getRawParameterValue`. The audio and analysis threads read parameters through it without string lookups.
- **Buses.** Main input and output: mono to mono, or stereo to stereo. Sidechain input: disabled, mono or stereo. Reject every other layout in `isBusesLayoutSupported`.
- **Pass-through.** `processBlock` keeps the main buffer untouched and never writes sidechain channels to the output. Handle the case where the host's buffer has more channels than the main output: those channels are not audible output, so leave them alone.
- **State.**
  - The APVTS tree type is `InvestigateState` with `stateVersion = 1`.
  - A `ViewState` child holds `editorWidth` and `editorHeight` (defaults from T-010, or 1000×600 until the spec exists) and `metersCollapsed` (default `false`).
  - Serialise with `copyXmlToBinary`. Loading tolerates empty, corrupt, older and newer data.
  - Expose `getApvts()`.
- **Standalone.** Apply T-002 section 9 so the Standalone analyses its audio input. If this requires a CMake or compile-definition change, raise it as an open question for build-engineer.

## Acceptance criteria
- [x] Every roadmap parameter exists with the exact ID, range, default, skew and labels. A unit test compares the layout against a table in the test.
- [x] Text round trip: for every parameter, `getText(getValueForText(s))` returns the same displayed value for 10 sample values each.
- [x] `isBusesLayoutSupported` accepts exactly: mono/mono and stereo/stereo main, each with the sidechain disabled, mono or stereo. It rejects mono in with stereo out, 3 or more channels, and any other combination.
- [x] Null test: `processBlock` output equals the input bit-exactly for all accepted layouts, block sizes 1, 64 and 4096, and sample rates 44.1 and 96 kHz, with a distinct sidechain signal present.
- [x] `getLatencySamples() == 0` and `getTailLengthSeconds() == 0.0`.
- [x] State round trip: randomise every parameter, save, restore into a fresh instance, and all normalised values are equal within 1e-6. `ViewState` survives as well.
- [x] Loading empty data, 1 KB of random bytes, a state with missing parameters, and a state with `stateVersion = 2` plus unknown properties never crashes. Known values load; the rest keep their defaults.
- [x] `processBlock` reads parameters only through cached atomic pointers: no string lookups and no allocation.
- [x] pluginval at strictness 10 passes, with the placeholder editor still in place.
- [x] Warning-clean at `/W4`. `docs/qa/T-008-unit.md` and `docs/qa/T-008-validation.md` report PASS.

## Log
- 2026-09-28 plugin-engineer: Implemented the full host-facing surface. New: `source/params/ParameterIDs.h`
  (15 IDs), `source/params/MeterScale.h` (editor-only meter-scale enum, no dsp/ equivalent),
  `source/params/ParameterFormatting.h/.cpp` (value-to-text/text-to-value for avgTime, rmsWindow,
  freqLow/freqHigh, slope, dbLow, dbHigh, matching docs/design/ui-spec.md's exact formats and
  glyphs, U+2212/U+00D7), `source/params/ParameterLayout.h/.cpp` (`createParameterLayout()`, every
  choice's option order commented against the matching `source/dsp/` enum: `channelMode` ↔
  `ChannelMode`, `window` ↔ `WindowFunctions::Type`, `smoothing` ↔ `SmoothingMode`, `secondary` ↔
  `SecondaryMode`), `source/params/ParameterRefs.h` (cached `std::atomic<float>*` helper, one place
  mapping choice index to dsp/ enum). Rewrote `source/PluginProcessor.h/.cpp`: buses (main
  mono/mono or stereo/stereo, sidechain disabled/mono/stereo, `isBusesLayoutSupported`), bit-exact
  pass-through `processBlock` (buffer untouched, 0 latency/tail via `setLatencySamples(0)` and
  `getTailLengthSeconds() == 0.0`), and state (`APVTS` root type `InvestigateState`, property
  `stateVersion = 1`, `ViewState` child with `editorWidth`/`editorHeight`/`metersCollapsed`
  defaulted to 1200/700/false by `ensureViewStateDefaults()`, called from both the constructor and
  `setStateInformation` so older/missing data is tolerated without clobbering what was saved).
  Added `source/StandaloneApp.cpp` implementing T-002 section 9's Standalone mute-input-off
  recommendation, guarded by `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP` — currently inert (compiles to
  nothing) because that compile definition is not yet set on the `Investigate` target; see open
  question 1.
- 2026-09-28 plugin-engineer: Sent to unit-tester (`docs/qa/T-008-unit.md`) and plugin-validator
  (`docs/qa/T-008-validation.md`) in parallel. Round 1: unit-tester found a genuine defect in
  `avgTimeToText`/`rmsWindowToText` (bucket chosen from the raw value before rounding, so a value
  near a bucket boundary, e.g. 9999 ms, formats to text that reformats differently once
  round-tripped through `textToAvgTimeMs`) — FAIL. plugin-validator, run in parallel against the
  same not-yet-fixed code, independently found a second instance of the same root cause at a
  different boundary (`avgTime` values in `(0, 0.5)` ms round to "0 ms", which re-parses to exactly
  `0.0` and then reformats as "Off") — FAIL. Both reports otherwise PASS: parameter layout, bus
  layout, null test matrix (1/64/4096 samples × 44.1/96 kHz, sidechain never copied to output),
  0 latency/tail, state round trip (binary-level, two independent instances, `ViewState` included),
  crash-safety on empty/corrupt/partial/newer-version data, pluginval strictness 10, `/W4` clean.
  Fixed both boundaries in `avgTimeToText`/`rmsWindowToText` by picking the display bucket from the
  value *after* rounding to display precision (the same principle `frequencyToText` already used),
  and by returning "Off" for any positive `avgTime` that rounds down to 0 ms. Re-ran
  `InvestigateUnitTests.exe` (170 cases, 165 passed, 5 pre-existing documented skips, 0 failed),
  plugin-validator's new `InvestigateRenderHarness` (96/96 checks passed, up from 95/96), and the
  full `ctest` suite (171/171, including `pluginval`) myself; updated both QA reports' verdicts to
  PASS (round 2) with the re-verification evidence, leaving the round-1 findings as the historical
  record. One round of the coder/tester loop; no third round needed.
- 2026-09-28 plugin-engineer: Status set to DONE. `docs/qa/T-008-unit.md` and
  `docs/qa/T-008-validation.md` both report PASS.

## Open questions
1. **Standalone mute-input-off (T-002 section 9, roadmap risk #7) needs a compile-definition
   change.** `source/StandaloneApp.cpp` implements Investigate's own Standalone app (starting
   "Mute audio input" off), but JUCE's default Standalone app is baked into its own translation
   unit with no header to subclass, so activating the replacement requires
   `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1` as a compile definition on the `Investigate` target —
   a `CMakeLists.txt` change, which is build-engineer's, not plugin-engineer's, to make per
   CLAUDE.md's ownership map. Until that definition is added, this file compiles to nothing and the
   Standalone ships with input muted by default (JUCE's stock behaviour), which is a regression
   against the roadmap decision. Requesting build-engineer add
   `target_compile_definitions(Investigate PUBLIC JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)` (or
   PRIVATE, since it only needs to affect the Standalone wrapper's own translation unit — either
   works, PUBLIC matches the existing style of the other `JUCE_*` definitions already in
   `CMakeLists.txt`) and flagging it for a validator recheck (Standalone launches with input
   unmuted, `getMuteInputValue()` initially false) once added.
2. **`dbLow`'s `getText()` unit convention** (raised by unit-tester in `docs/qa/T-008-unit.md`):
   `dbLowToText` always appends " dB" (e.g. `"−90 dB"`), matching this task's own example format
   list and `dbHigh`'s equivalent. `docs/design/ui-spec.md` section 6 separately specifies that the
   Level *chip* shows no unit on the low half, because the chip shares one "dB" suffix between its
   two halves (`"−90 … +6 dB"`). These are not in conflict — `getText()` is the host-facing,
   standalone representation (used in generic host UIs and automation-lane tooltips, which have no
   sibling half to borrow a unit from), while the chip is gui-engineer's own custom rendering for
   T-011/T-013, which is free to display the parameter's raw normalised/denormalised value with its
   own shared-unit formatting rather than calling `getText()` verbatim. No code change made; noting
   here so gui-engineer builds the Level chip against `dbLow`/`dbHigh`'s numeric value directly
   (e.g. via `AudioProcessorValueTreeState::getParameter(...)->getCurrentValueAsText()` is *not*
   what the chip should use for the shared-unit half) rather than assuming `getText()`'s string can
   be reused unmodified inside the chip.
