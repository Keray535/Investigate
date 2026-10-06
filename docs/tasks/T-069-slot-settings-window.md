# T-069: Slot settings window
- Owner: gui-engineer
- Milestone: M3c
- Status: TODO
- Depends on: T-055

## Goal
Build the single settings window component that edits any of the four spectrum slots (review findings #9 and #12, unified by the user into the slot model). It opens from a slot's gear in the Spectra list (T-055), and for slot 1 also from the bottom bar.

## Inputs
- `docs/design/ui-spec.md` §18 (T-065), the slot settings window and the Spectra list; `mockup.html`; `tokens.json` 1.3.0.
- `docs/plans/m3-review-spectrum-slots.md`, Architecture: "Spectrum slot model", and the Parameter changes table.
- The logs of T-051 (the generated slot parameter table and IDs), T-052 (Link resolution) and T-055 (the Spectra list and its gear entry point).
- `source/gui/` (`SettingsPopover.*`, `ParamControl.*`, `ControlBar.*`, `ModeSelector.*`, `InvestigateLookAndFeel.*`) and the T-011 log (parameter attachments, the popover pattern).
- Working rules as in T-066.

## Scope
1. **One component class, `SlotSettingsPanel`** (or the name the spec gives), parameterised by the slot index. It binds its controls to the slot's parameters through the generated ID table from `source/params/`, with no hand-written per-slot code.
2. **Controls,** per §18: Source, Channel, Type, Link, Block size, Overlap, Window, Avg time, Smooth (10 choices), Slope and Fill.
   - Slot 1: Source and Type are shown locked. Link does not exist. The controls bind to the existing slot 1 parameters, so they stay in sync with the bottom bar.
   - Slots 2 to 4: while Link is on, the analysis fields show slot 1's current values in the linked style. Editing one turns Link off for that slot and applies the edit, as one undoable gesture group.
   - A Source whose bus is inactive is shown as "no input", per spec.
3. **Opening and closing,** per spec: from the Spectra list gear, and from the bottom bar gear for slot 1. Only one window is open at a time. It closes on Escape, on an outside click, or on editor close. It stays inside the editor bounds at every size, including the compact layout.
4. **No tooltips** (review #18).

## Acceptance criteria
- [ ] **Binding.** For each slot, every control reads and writes exactly its slot's parameter (a table-driven test over the generated IDs). Changing a control in slot 3 changes no parameter of slots 1, 2 or 4.
- [ ] **Slot 1.** Its window and the bottom bar stay in sync in both directions. Source and Type cannot be changed.
- [ ] **Link.**
  - With Link on, slot 2's fields show slot 1's values, and they follow a slot 1 change within one editor update.
  - Editing slot 2's Block size turns `s2Link` off and sets `s2FftSize`.
  - Undo in the Standalone reverts both in one step.
- [ ] **Layout.** The window fits and is fully usable at 1200×700 and at the new minimum size, with no clipped control (screenshot check), and it matches §18 within the T-011 tolerance.
- [ ] **Regression.** Every existing GUI test passes. The whole-editor budgets hold with the window open: ≤ 25 %, and paint p95 ≤ 4 ms and ≤ 8 ms (median of 3).
- [ ] Warning-clean at `/W4`. `docs/qa/T-069-unit.md` and `docs/qa/T-069-validation.md` report PASS.

## Log
- 2026-10-06 vst-architect: Created from the M3 review triage. The user asked for every main-spectrum setting to be editable for the second spectrum and for each sidechain, and then for all of them to live under one roof: one window for all four slots.
