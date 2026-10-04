# T-045: M3c UI spec addendum, design tokens, assets and mockup
- Owner: ui-ux-designer
- Milestone: M3c
- Status: TODO
- Depends on: T-042, T-043, T-044

## Goal
Design every M3c user-facing change as one coherent addendum to the approved UI:
- band solo;
- metering modes, style and ballistics;
- meter section hiding;
- the channel and underlay controls;
- two sidechain curves with labels;
- the 1 Hz axis;
- Align 0 dB, anti-alias and the frame rate;
- curve colours with a colour wheel and colour presets;
- settings presets;
- reference slots;
- CSV import and export.

The user approves the mockup before any M3c GUI task (T-054 to T-058) starts.

## Inputs
- `docs/plans/m3c-feature-expansion.md`, the whole document. It holds the architect's design, the parameter table, the storage summary, the budgets and the hands-on checklist.
- `docs/research/T-043-span-m3c-reference.md` and `docs/research/T-044-host-platform-constraints.md`. Where they change the plan's interpretation, follow the reports and list each change in your log.
- `docs/design/ui-spec.md` (after T-042), `tokens.json`, `mockup.html`, and `assets/ui/`.
- The T-029 rendering rules (end of `docs/tasks/T-029-editor-render-cpu.md`) and §12 of the spec. Every new visual must be drawable within them: no polygons per frame; one `fillRectList` per colour; cached images for static parts.
- IP boundaries: `docs/research/T-001-span-feature-parity.md` §8. Use your own artwork, texts and layout; SPAN behaviour only.

## Scope
Write a new spec section (for example §17 "M3c additions"), update every affected existing section, update `tokens.json`, add any new SVG icons to `assets/ui/`, and extend the mockup. It must cover:

1. **Header and layout at 920×540 and 1200×700.**
   - Place the Channel segmented control (Mid, Side, L, R, Stereo) and the Underlay choice, Sidechain 1 and Sidechain 2 toggles, a Presets menu, and an entry to the reference slots and the colour editor.
   - Everything must fit at the minimum size, or move into menus or the Settings popover with a defined rule.
   - Legacy `channelMode` values L/R and M/S show as the equivalent pair: Left plus Right, Mid plus Side.
2. **Band solo** (plan [1]):
   - the preview while Ctrl is held;
   - the audible state while Ctrl and the left button are held;
   - the outside-band tint, the band-edge markers, and the centre and width readout;
   - the Ctrl+wheel step for `soloWidth`;
   - the release conditions;
   - interaction with the hover card, the zoom panel, the toasts and Freeze;
   - the cursor;
   - accessibility: the state must be clear without relying on colour.
3. **The meter strip** (plan [2], [3], [10]):
   - scales, ticks and labels for all 14 metering modes;
   - the density rendering, drawable with a few rectangle lists;
   - the peak-hold and release display formulas;
   - controls for Metering, Style, Integration, Release and Peak hold, with formats and text entry;
   - the section-hiding control (no context menus) and how the remaining sections re-flow;
   - how the RMS row's unit follows the mode.
4. **The spectrum** (plan [4], [5], [6], [7], [12]):
   - the underlay styles (today's primary B);
   - the Sidechain 2 curve style, distinct from Sidechain 1 without relying on hue;
   - legend and hover entries using the user-editable input labels, and how labels are edited (at most 16 characters);
   - 1, 2 and 5 Hz grid labels, and note names below C0;
   - the Align 0 dB control;
   - the reference curve style (line only, distinct from every live curve), offsets, and the legend entries.
5. **Curve colours** (plan [8]):
   - the colour roles and their defaults (the current tokens);
   - the derivation rules for secondary curves and fills;
   - the colour editor: a hue wheel with a saturation and value area, hex entry, a contrast warning below 3:1 against `bg-plot`, and Reset to default;
   - colour preset save, load, overwrite and delete;
   - the live update.
6. **Display preferences** (plan [6], [9]): the frame rate (30 to 144, default 60), Adaptive, and Anti-alias; where they live; and the note that rates above 60 cost more CPU.
7. **Settings presets** (plan [14]): menu contents, the current-preset name with its modified mark, Save as with an inline name entry and its validation, overwrite and delete confirmations, and "Open presets folder".
8. **Reference slots and CSV** (plan [12], [13]):
   - the slot panel: name, visibility, offset, Take, Take 2nd, Load CSV, Save CSV, Clear;
   - "Export curves";
   - the async file dialog flow;
   - the error toasts with line numbers.
9. **The Settings popover.** Restructure it for the new rows: underlay, align, sidechain 2 trace, the metering parameters, the display preferences and solo width. Tabs or sections are allowed.
10. Update the **coverage checklist** (§13) for every new parameter, `ViewState` property, global preference, `MeterSnapshot` field and `SpectrumFrame` field in the plan.
11. Update the **accessibility** sections (§10): contrast, colour-vision checks of the default role colours, hit targets and accessible names for the new controls.
12. Update the **performance rules** (§12): the layer-count definition that drives Adaptive pacing, given the new curve layers.

## Acceptance criteria
- [ ] Every M3c item in the plan's Scope maps to a spec section, and the spec lists that mapping.
- [ ] Every new parameter in the plan's parameter table, and every control and preference in the storage summary, has a control-inventory row: location, size, label, format, default, drag and wheel behaviour, and text entry.
- [ ] Layouts are resolved with coordinates at 920×540 and 1200×700, with meters shown and collapsed, and with every section-hiding combination. Nothing overlaps.
- [ ] The default role colours pass the existing colour-vision check. Every curve type stays distinguishable without hue (by fill, width, dash or position).
- [ ] The mockup demonstrates, in a browser:
  - the band-solo preview, the solo and its tint;
  - each metering mode and the density style;
  - section hiding;
  - the channel and underlay controls;
  - the two sidechains with labels;
  - the colour wheel and colour presets;
  - the preset menu;
  - the reference slots;
  - the CSV error toast.
- [ ] Open questions are listed in the spec's §15. Changes from the plan are listed, each with a reason.
- [ ] **Gate:** the user approves the mockup. The architect records the approval in this log before T-054 starts.

## Log
- 2026-10-03 vst-architect: Created for M3c. Documentation only; may run in parallel with T-037, T-038 and the M3c DSP tasks. Ownership: only this task edits `docs/design/` and `assets/ui/` during M3c.
