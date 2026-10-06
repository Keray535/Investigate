# T-070: Compact layout and smaller minimum size
- Owner: gui-engineer
- Milestone: M3c
- Status: TODO
- Depends on: T-057

## Goal
Implement the compact layout and the smaller minimum editor size from review finding #14. Also close #16 (glitches at very small window sizes) if T-066 did not.

## Inputs
- `docs/design/ui-spec.md` §18 (T-065): the compact layout, the width breakpoints, and the new minimum size; `mockup.html`.
- `docs/plans/m3-review-spectrum-slots.md`, Architecture: "Compact layout (#14, #16)".
- `docs/research/T-061-review-bug-diagnosis.md`, section #16, and the T-066 log (whether #16 was already fixed).
- `source/PluginEditor.*`, `source/gui/LayoutMath.h`, `MeterPanel.*`, `ControlBar.*`, `SpectrumView.*`, the header components, and the T-011 and T-057 logs (the constrainer, `ViewState` size persistence, meter sections).
- Working rules as in T-066.

## Scope
1. **Minimum size.** Lower the minimum editor size to the §18 value (target: at most 560×400). The maximum (2560×1600) and the free aspect ratio are unchanged. A saved `ViewState` size below the new minimum is clamped on load.
2. **Compact layout.**
   - It switches on below the §18 width breakpoint (600 px); height never triggers it.
   - In compact layout, hide the readouts Peak, Peak Max, TP, TP Max, Headroom, RMS, Crest, Clips, TP Clips and Loudness, for the main input and for the sidechains.
   - The spectrum and the level meter stay. The correlation meter takes its compact form.
   - The header and the control bar follow the merged breakpoints of §18, including the T-045 compact header.
3. **Small-window robustness (#16).**
   - No overlapping, negative-sized or out-of-bounds components at any size from the minimum to the maximum.
   - No paint artefacts during a fast live resize, following the T-061 findings.
   - If the cause is FL Studio's wrapper, document the observed behaviour and any workaround in the validation report.
4. Leaving and entering compact layout keeps every user setting, including the meter-section toggles. It is only a presentation state, not saved in `ViewState`.

## Acceptance criteria
- [ ] **Breakpoint.** At 599 px wide, the compact layout is active; at 600 px it is not. The listed readouts are hidden, and the spectrum, the level meter and the compact correlation meter are visible (a component-visibility test at heights from the minimum to 1600).
- [ ] **Sizes.**
  - The constrainer allows exactly the §18 minimum, and a saved smaller size is clamped.
  - A layout sweep (every 10 px of width and height from the minimum to 1600×1000) finds no overlap, no zero or negative bounds, and no clipped text in visible components.
- [ ] **#16.** A scripted fast resize between the minimum and 1200×700 (60 steps per second for 5 s) in the Standalone shows no artefact in captured frames, per the T-061 criteria. The validation report records the FL Studio result.
- [ ] **Budgets.** Whole-editor message thread ≤ 25 % and paint p95 ≤ 4 ms at the minimum size and at 1280×720 (median of 3).
- [ ] **Regression.** Every existing GUI test passes, updated only where the minimum size changed (each change listed).
- [ ] Warning-clean at `/W4`. `docs/qa/T-070-unit.md` and `docs/qa/T-070-validation.md` report PASS.

## Log
- 2026-10-06 vst-architect: Created from the M3 review triage. The user specified a width-only trigger at 50 % of the default width (1200 px, so 600 px), and asked for a lower minimum height than 920×540 allows.
