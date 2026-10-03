# T-038: SpectrumView: overview strip and box zoom
- Owner: gui-engineer
- Milestone: M3b
- Status: TODO
- Depends on: T-037

## Goal
Add the two remaining navigation aids from ui-spec §5.9:
- the **overview strip**: a mini-map, shown only while zoomed, with the primary curve over the home range and the visible window as a rectangle that can be dragged;
- **box zoom**: Ctrl+left-drag draws a rubber band and zooms both axes to it.

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
   - Ctrl+left-drag draws the rubber band per §5.9;
   - on release, both axes zoom to it through `ZoomMath` and `DisplayRange` (an axis below its minimum span is widened, centred);
   - a box under 4 px in either dimension cancels;
   - Esc cancels when the editor has keyboard focus;
   - the hover card behaves per §5.9 during the drag.

## Acceptance criteria
- [ ] The overview strip appears and disappears exactly with the zoom state.
- [ ] The window rectangle matches the view within 1 px.
- [ ] A rectangle drag pans with the pointer offset kept within 1 px; a click outside centres the view.
- [ ] The strip never covers the legend, the FROZEN badge or the toasts, at 1200×700 and 920×540.
- [ ] **Box zoom:** the resulting view equals the box within 0.5 px on both axes, or the minimum span centred on the box. A tiny box cancels, and Esc cancels. A Ctrl+drag never triggers a pan or a spectrum reset.
- [ ] **Performance:** with the overview visible during the T-037 continuous pan script, the same gates hold: whole-editor message thread ≤ 25 % at both sizes (median of 3), paint p95 ≤ 4 ms and ≤ 8 ms, and no per-frame allocation in Investigate code.
- [ ] Warning-clean at `/W4`. `docs/qa/T-038-unit.md` and `docs/qa/T-038-validation.md` report PASS.

## Log
- 2026-10-03 vst-architect: Created from `docs/plans/spectrum-zoom-pan.md`.
