# T-042: Zoom modifiers: Alt+wheel for level zoom, Alt+drag for box zoom, Ctrl reserved for band solo
- Owner: ui-ux-designer
- Milestone: M3b
- Status: TODO
- Depends on: none

## Goal
The user moved two zoom gestures to free the Ctrl key for the M3c band solo:
- level zoom moves from Ctrl+wheel to **Alt+wheel**;
- box zoom moves from Ctrl+left-drag to **Alt+left-drag**.

The approved M3b spec and mockup must say this before T-037 implements the gestures. Ctrl+wheel and Ctrl+left press or drag over the data rect become reserved, and do nothing until M3c.

## Inputs
- `docs/design/ui-spec.md` §5.6, §5.9 (in particular §5.9.3 G3, G10, G13 to G15, §5.9.4 cursors, §5.9.5 hint line, §5.9.10), §7.1, §7.2, §10.2a, §12.1 and §13.3; `docs/design/mockup.html` and `tokens.json`.
- `docs/plans/m3c-feature-expansion.md`: Scope item [1], "Item-by-item design [1]", and Risks 17 and 18.
- `docs/plans/spectrum-zoom-pan.md`: the interaction table. The architect updates that plan document; this task does not edit it.

## Scope
1. **The gesture table.** Replace every Ctrl zoom gesture with its Alt equivalent:
   - Alt+wheel zooms level around the pointer, with the same factors and the same Shift fine steps;
   - Alt+left-drag box-zooms, with the same 4 px cancel rule and Esc cancel.

   The wheel over the dB or frequency gutter still zooms its axis without a modifier.
2. **The reserved Ctrl gestures.** State that Ctrl+wheel, and a Ctrl+left press or drag, over the data rect are reserved for the band solo (M3c).
   - Until M3c they do nothing: the wheel event is consumed, and a Ctrl press never pans, zooms, box-zooms or triggers the deferred spectrum reset.
   - A plain Alt+left click on the data rect (no drag) also does nothing; it is not a reset. The Alt+click reset on chips (§7.1) is unchanged.
3. **Touchpad pinch.** Windows delivers a pinch as Ctrl+wheel, so until M3c a pinch does nothing on the spectrum, and from M3c it changes the solo width. Replace the old note ("pinch zooms level") with this.
4. **Hint line, tooltips, cursors and §10.2a accessibility notes.** Update every mention. The box-zoom cursor appears with Alt instead of Ctrl.
5. **The mockup.** Change the gesture handlers to the Alt variants, and make the Ctrl variants inert.
6. Add a change-log entry in §16, citing the user decision of 2026-10-03.

## Acceptance criteria
- [ ] `docs/design/ui-spec.md` and `docs/design/mockup.html` contain no remaining instruction that zooms or box-zooms with Ctrl. A search for "Ctrl" finds only the reserved band-solo statements, the Ctrl+C text copy in the About view, and unrelated uses.
- [ ] In the mockup, Alt+wheel zooms level, Alt+left-drag box-zooms, and Ctrl+wheel or Ctrl+drag over the data rect do nothing.
- [ ] §5.6 and §5.9.3 agree row for row. The G numbers stay stable; a changed row keeps its number, with its modifier updated.
- [ ] The §16 change log names the change and the date.

## Log
- 2026-10-03 vst-architect: Created from the user's M3c decision (band solo on Ctrl+left-hold and Ctrl+wheel). Documentation only. It may run in parallel with T-041 and must be DONE before T-037 starts. No new user approval of the mockup is needed, because the user dictated this change.
