# T-044: Host and platform constraints for M3c, plus band-solo filter references
- Owner: researcher
- Milestone: M3c
- Status: TODO
- Depends on: none

## Goal
Several M3c features depend on host behaviour or on Windows platform behaviour that no one in the project has checked:
- a second VST3 sidechain bus;
- Alt and Ctrl gestures in a plugin window without keyboard focus;
- a preference file shared by several instances;
- asynchronous file dialogs inside a plugin.

The band solo also needs a sound filter design. Collect the facts and references before the design (T-045) and the DSP (T-050) start.

## Inputs
- `docs/plans/m3c-feature-expansion.md`: Scope, the design of items [1], [7], [8], [9] and [13], and Risks 15 to 19.
- `docs/roadmap.md`: Buses and channel layouts, Risk 6 (hosts with odd sidechain handling), and the Decisions on licences (no strong copyleft).
- `source/PluginProcessor.cpp`: the current `BusesProperties` and `isBusesLayoutSupported`.
- The pinned JUCE 8.0.15 sources in `third_party/juce-src/` (read only).

## Questions to answer
1. **A second sidechain bus.** For a JUCE 8 VST3 plugin with main input and output plus two optional aux input buses, answer per host:
   - Is the second bus offered, and how does the user route a track into it?
   - Hosts: **FL Studio** (the user's DAW; the plugin wrapper's input pin routing, and whether VST3 extra buses appear), Ableton Live 12, Reaper 7, Cubase or Nuendo, Studio One and Bitwig.

   Also cover:
   - how JUCE 8 declares the buses (`BusesProperties`, bus names, default enabled state);
   - an `isBusesLayoutSupported` pattern that accepts disabled, mono or stereo on each aux bus independently;
   - known pitfalls (hosts that enable every bus, hosts that require matching layouts);
   - how pluginval and the Steinberg VST3 validator treat three input buses.
2. **Alt key on Windows.** If the user holds Alt, scrolls or drags inside a plugin's child window, then releases Alt, does the host's menu bar activate (the `WM_SYSKEYUP` / `SC_KEYMENU` behaviour)?
   - Hosts: FL Studio, Reaper, Ableton Live and the JUCE Standalone.
   - Any known mitigation inside a plugin.
   - How Voxengo (Alt+drag zoom) and iZotope (Alt+click audition) avoid the problem, if documented.
3. **Modifiers without keyboard focus.** Is `juce::ModifierKeys::getCurrentModifiersRealtime()` reliable on Windows inside a plugin editor that does not have keyboard focus, so that the editor can poll Ctrl on every update (for the band preview) and detect Ctrl being released?
4. **A shared preference file.**
   - The recommended JUCE pattern for one preference file shared by every instance in a process: `juce::PropertiesFile` with an `InterProcessLock`, plus `juce::SharedResourcePointer`.
   - Behaviour across processes: FL Studio's plugin bridging, and Bitwig's sandboxing.
   - Reload and change notification options.
   - The `%APPDATA%` location conventions.
5. **File dialogs.** Known issues with `juce::FileChooser::launchAsync` (native dialogs) inside plugin editors on Windows hosts: focus, modality, DPI, and hosts that block their message loop.
6. **Band-solo filter references.**
   - The Zavalishin TPT state-variable filter ("The Art of VA Filter Design").
   - Andrew Simper's (Cytomic) SVF technical papers.
   - A band-pass built as a Linkwitz-Riley high-pass and low-pass pair, compared with resonant band-pass designs: passband flatness at narrow widths (1/12 to 1/3 octave), skirt slopes, and stability when the centre and width are modulated per sample.
   - Crossfade practice for engaging a filter without clicks.

   State the licence of any code you consider. Reject strong copyleft.
7. **A colour wheel component.** What `juce::ColourSelector` offers and lacks for a hue wheel. Any permissively licensed JUCE colour-wheel examples, with a licence check.

## Acceptance criteria
- [ ] `docs/research/T-044-host-platform-constraints.md` answers questions 1 to 7. Each answer is marked "confirmed (source)", "inferred" or "not documented".
- [ ] A host table for question 1 lists, per host: second bus offered (yes, no or unknown), routing steps, and caveats. FL Studio is covered in the most detail.
- [ ] A clear verdict on Risk 17 (Alt release). If the risk is real in a major host, the report proposes fallback modifiers for level zoom and box zoom, for the user to decide.
- [ ] A filter recommendation for T-050, with equations or citations: topology, order, how the width maps to the band edges, the expected gain at the centre for widths from 1/12 to 4 octaves, and the smoothing approach.
- [ ] Every third-party code reference carries its licence and a verdict (usable or rejected).

## Log
- 2026-10-03 vst-architect: Created for M3c. Documentation only; may run in parallel with any C++ task and with T-043. T-045 and T-050 wait for it.
