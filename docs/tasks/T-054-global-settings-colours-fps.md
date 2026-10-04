# T-054: Global settings, curve colours, colour wheel and colour presets, frame rate, anti-alias
- Owner: gui-engineer
- Milestone: M3c
- Status: TODO
- Depends on: T-045 (mockup approved by the user), T-053

## Goal
Add the machine-wide display preferences:
- curve colours chosen with a colour wheel, which persist across every instance and launch, with separate colour presets;
- a frame-rate cap with Adaptive pacing;
- an anti-alias switch.

The first launch shows the default colours.

## Inputs
- `docs/design/ui-spec.md`: the M3c addendum from T-045 (colour roles, derivation rules, colour editor, display preferences, performance rules), `tokens.json` and `mockup.html`.
- `docs/plans/m3c-feature-expansion.md`: design items [6] (anti-alias), [8] and [9], the Storage summary, the Budgets, and Risks 19 and 20.
- `docs/research/T-044-host-platform-constraints.md`, questions 4 and 7.
- `source/presets/UserDataPaths.h` (T-053, read-only include).
- `source/gui/SpectrumView.*`, `SpectrumMath.h` (pacing), `DesignTokens.h`, the editor's `UpdateRateLimiter` path, and the T-029 log (rendering rules and the CPU harness method).
- `tests/unit/GuiTestEnvironment.h` for on-screen tests.
- Working rules from the roadmap handoff:
  - one C++ task at a time;
  - `ctest --timeout 300`;
  - timing assertions only in `[.perf]`;
  - coders never edit `docs/qa/`;
  - CPU is the median of 3 runs with the T-012/T-029 harness.

## Scope
1. **`source/gui/GlobalSettings`:**
   - one `juce::PropertiesFile` at the `UserDataPaths` location, with an `InterProcessLock`;
   - shared by every editor in the process through `juce::SharedResourcePointer`, with change notification;
   - it holds the role colours, `frameRate` (30 to 144, default 60), `adaptivePacing` (default true) and `antiAlias` (default true);
   - it reloads when an editor opens;
   - writes are debounced to at most 2 per second, with a flush on the last editor close;
   - a missing or corrupt file gives the defaults and never throws or blocks.
2. **Curve colour roles:** Primary A, Primary B (underlay), Sidechain 1, Sidechain 2, and References 1 to 4. Use the spec's derivation rules for the secondary tones and the fills. `SpectrumView` reads its colours from the roles, not from fixed tokens. A change repaints every open editor in the process.
3. **The colour editor**, per the spec:
   - a custom `ColourWheel` component (a hue ring and a saturation and value area) with hex entry;
   - the contrast warning below 3:1 against `bg-plot`;
   - Reset to default.
4. **Colour presets:** `*.invcolours` XML files in `Presets\Colours\`, holding a name and an ARGB value per role. The user can save as, load, overwrite and delete. Names are validated as in T-053.
5. **Frame rate.**
   - The editor's update limiter caps at `min (frameRate, display refresh)`.
   - Adaptive pacing generalises the T-029 rule: in the heavy cases defined by the spec (plot larger than 2.0 Mpx, or the layer threshold), the spectrum repaints at half the cap, but never below 30 fps.
   - Numeric meter text stays at 10 Hz.
6. **Anti-alias off.** Curve strokes are drawn as pixel-snapped column spans in one `fillRectList` per colour. The fills are unchanged. The software-renderer path stays correct.

## Acceptance criteria
- [ ] **First launch** (no preference file): the colours equal the current tokens, and the rendering is pixel-identical to the pre-task build in the existing render tests.
- [ ] **Persistence.** In two separate processes in sequence (for example two Standalone launches, or a test that destroys and recreates `GlobalSettings`): a colour change, `frameRate` and `antiAlias` persist. A corrupt file gives the defaults without a crash.
- [ ] **Live update.** Two editors in one process: a colour change in one repaints the other within one editor update.
- [ ] **Colour presets.** Save, load, overwrite and delete all work. Loading a preset changes exactly the role colours. Settings presets (T-053) never change colours.
- [ ] **Frame rate.** With the display at 144 Hz, measured spectrum update rates are 30, 60 and 120 ± 5 % fps for caps of 30, 60 and 120, in a light case with Adaptive off. With Adaptive on, in a heavy case, the rate is half the cap but at least 30.
- [ ] **Budgets** at a 60 fps cap, Adaptive on, the worst displayed case (every layer the current build can show), median of 3: whole-editor message thread ≤ 25 % at 1280×720 and 2560×1440, and paint p95 ≤ 4 ms and ≤ 8 ms. Anti-alias off is not slower than on. Report the numbers at a 144 fps cap (not gated).
- [ ] No per-frame allocation in Investigate code in the curve paths. Preference writes stay ≤ 2 per second during a continuous colour drag.
- [ ] Warning-clean at `/W4`. `docs/qa/T-054-unit.md` and `docs/qa/T-054-validation.md` report PASS.

## Log
- 2026-10-03 vst-architect: Created from `docs/plans/m3c-feature-expansion.md`. Must not start before the user approves the T-045 mockup.
