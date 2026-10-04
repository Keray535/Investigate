# T-055: SpectrumView M3c: channel and underlay controls, sidechain curves and labels, 1 Hz axis, Align
- Owner: gui-engineer
- Milestone: M3c
- Status: TODO
- Depends on: T-054

## Goal
Bring the spectrum side of the editor to the M3c design:
- the Channel and Underlay controls replace the seven-way Mode control;
- both sidechains show as their own curves with user-editable labels;
- the axis and the hover readout work down to 1 Hz;
- the Align 0 dB control is reachable.

## Inputs
- `docs/design/ui-spec.md`: the M3c addendum from T-045 (header layout, underlay and sidechain styles, labels, 1 Hz grid and note names, Align control, Settings popover), plus `mockup.html` and `tokens.json`.
- `docs/plans/m3c-feature-expansion.md`: design items [4], [5], [6], [7] and [11], and the Storage summary.
- The task logs of T-046 (frame fields: `isDual` now means "underlay present", and `sidechain2Db` with `hasSidechain2`), T-051 (parameters, `ViewState` defaults) and T-052 (`isSidechainActive (int)`).
- `source/gui/ModeSelector.*`, `SpectrumView.*`, `SpectrumMath.h`, `SettingsPopover.*`, `TooltipText.*` and `ViewStateStore.*`.
- Working rules as in T-054.

## Scope
1. **The header Channel control:**
   - a segmented Channel control (Mid, Side, L, R, Stereo) bound to `channelMode` indices 0 to 4, plus an Underlay choice bound to `underlay`;
   - a legacy L/R or M/S value displays as Left plus Right or Mid plus Side;
   - the next user edit of either control writes the normalised pair (`channelMode` 0 to 4 plus `underlay`) as one undoable gesture group;
   - nothing is written without a user edit;
   - auto-unfreeze fires when the effective pair changes.
2. **The sidechains:**
   - draw the Sidechain 2 curve in its spec style and role colour;
   - the toggles follow `showSidechain` and `showSidechain2`, plus `isSidechainActive (0)` and `(1)`, with the "no input" state for an inactive bus;
   - the legend and the hover card use the input labels from `ViewState` (`inputLabel0` to `inputLabel2`);
   - label editing works per the spec (at most 16 characters; empty means the default).
3. **1 Hz.** Grid labels at 1, 2 and 5 Hz, and note names below C0, per the spec. Every path that assumed a 10 Hz minimum (the chip formatting, the gutter labels, the deep-zoom ticks) works from 1 Hz.
4. **Align 0 dB.** The control per the spec, bound to `align0dB`.
5. **Settings popover:** add the rows the spec assigns to this task.
6. **Pacing layer count.** Update it to include the Sidechain 2 curve, per the spec's §12.

## Acceptance criteria
- [ ] **Channel and underlay.**
  - Every Channel and Underlay combination produces the right legend names and curves (synthetic frames).
  - A state with legacy L/R or M/S loads and displays as the equivalent pair without writing any parameter. The first user edit writes the normalised pair.
  - Undo in the Standalone reverts the edit as one step.
- [ ] **Sidechains.** Both curves draw in their own styles and colours. Each toggle hides its own curve. An inactive bus shows "no input". Labels appear in the legend and the hover card, persist through a state round trip, and are truncated or refused at 16 characters.
- [ ] **1 Hz.** At `freqLow` = 1 Hz, the grid shows labelled lines at 1, 2, 5, 10 and 20 Hz at 1200×700. The hover readout and the right-click copy give the correct frequency and note name (or the spec's placeholder) from 1 Hz up. No overlaps at 920×540.
- [ ] **Align.** The control toggles `align0dB`, and the frame visibly changes (synthetic sine frame).
- [ ] **Regression.** Every existing SpectrumView test (T-012, T-029, T-037, T-038) passes. The zoom gestures use Alt for level zoom and box zoom (T-042).
- [ ] **Budgets** with both sidechains, an underlay, secondary curves and the meters, at a 60 fps cap, Adaptive on, median of 3: whole-editor message thread ≤ 25 % at 1280×720 and 2560×1440, and paint p95 ≤ 4 ms and ≤ 8 ms, including during the T-037 continuous-pan script.
- [ ] Warning-clean at `/W4`. `docs/qa/T-055-unit.md` and `docs/qa/T-055-validation.md` report PASS.

## Log
- 2026-10-03 vst-architect: Created from `docs/plans/m3c-feature-expansion.md`.
