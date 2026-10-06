# T-038: SpectrumView: overview strip and box zoom
- Owner: gui-engineer
- Milestone: M3b
- Status: TODO
- Depends on: T-037, T-042, T-064 (build order)

## Goal
Add the two remaining navigation aids from ui-spec §5.9:
- the **overview strip**: a mini-map, shown only while zoomed, with the primary curve over the home range and the visible window as a rectangle that can be dragged;
- **box zoom**: Alt+left-drag draws a rubber band and zooms both axes to it (Alt replaced Ctrl on 2026-10-03, T-042).

## Inputs
- `docs/design/ui-spec.md` §5.9 and §12, as updated by T-034, plus `docs/design/mockup.html` and `tokens.json`.
- `docs/plans/spectrum-zoom-pan.md`: the Interaction table and Budgets.
- `docs/tasks/T-037-spectrum-zoom-pan-editor.md` and its log: `ZoomMath`, the view propagation, and the measurement scripts.
- `SpectrumFrame::overviewFreqHz` and `overviewDb` (T-035).
- The T-029 log (rendering rules) and `tests/unit/GuiTestEnvironment.h`.

## Scope
1. **The overview strip:**
   - position, size and styles per §5.9;
   - visible exactly while the view differs from Fit;
   - its curve comes from the frame's overview arrays;
   - the window rectangle reflects both axes;
   - dragging the rectangle pans, keeping the pointer's offset;
   - a click outside the rectangle centres the view on that point and keeps the span;
   - it follows the collision rules with the legend, the FROZEN badge, the hover card and the toasts.
2. **Box zoom:**
   - Alt+left-drag draws the rubber band per §5.9 (as amended by T-042);
   - on release, both axes zoom to it through `ZoomMath` and `DisplayRange` (an axis below its minimum span is widened, centred);
   - a box under 4 px in either dimension cancels;
   - Esc cancels when the editor has keyboard focus;
   - the hover card behaves per §5.9 during the drag.

## Acceptance criteria
- [ ] The overview strip appears and disappears exactly with the zoom state.
- [ ] The window rectangle matches the view within 1 px.
- [ ] A rectangle drag pans with the pointer offset kept within 1 px; a click outside centres the view.
- [ ] The strip never covers the legend, the FROZEN badge or the toasts, at 1200×700 and 920×540.
- [ ] **Box zoom:** the resulting view equals the box within 0.5 px on both axes, or the minimum span centred on the box. A tiny box cancels, and Esc cancels. An Alt+drag never triggers a pan or a spectrum reset. A Ctrl+drag does nothing (reserved for the M3c band solo).
- [ ] **Performance:** with the overview visible during the T-037 continuous pan script, the same gates hold: whole-editor message thread ≤ 25 % at both sizes (median of 3), paint p95 ≤ 4 ms and ≤ 8 ms, and no per-frame allocation in Investigate code.
- [ ] Warning-clean at `/W4`. `docs/qa/T-038-unit.md` and `docs/qa/T-038-validation.md` report PASS.

## Log
- 2026-10-03 vst-architect: Created from `docs/plans/spectrum-zoom-pan.md`.
- 2026-10-03 vst-architect: Box zoom moved from Ctrl+left-drag to **Alt+left-drag** by user decision (M3c planning). Ctrl gestures are reserved for the band solo. Follow the spec as amended by T-042. Validation: record whether releasing Alt after an Alt+drag activates a host menu, in the Standalone and in FL Studio if available (Risk 17).
- 2026-10-06 vst-architect: Build order changed by the M3 review triage (`docs/plans/m3-review-spectrum-slots.md`): this task runs after the M3d fixes T-062, T-063 and T-064, which change the mapper (curve interpolation, smoothing up to 3 octaves), the level-range rule (bottom ≥ 1 dB below the top; minimum level view span `min (6 dB, home span)`), and the default level range (−18 to −72 dB). Read their logs first.
  - The box zoom must respect the new minimum level span.
  - Do not add tooltips: review #18 removes every tooltip except the hover readout, and T-066 removes the existing ones.
