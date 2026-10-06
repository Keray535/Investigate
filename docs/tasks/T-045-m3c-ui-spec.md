# T-045: M3c UI spec addendum, design tokens, assets and mockup
- Owner: ui-ux-designer
- Milestone: M3c
- Status: IN_PROGRESS
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
- 2026-10-04 vst-architect: T-042, T-043 and T-044 are DONE. Decisions made since the plan was written (also in the plan's Decisions table):
  - **User decisions:**
    - dBFS.30 spans −24 to +6 dBFS and dBFS.15 spans −9 to +6 dBFS;
    - the ballistics defaults stay integration 1.00 ms, release 750 ms and peak hold 1000 ms (so by default the level part of the bar reacts like a peak meter; the spec should make the Integration control easy to find);
    - the LUFS and LU bars show one programme loudness value, not per-channel values. Design that bar so it does not look like a per-channel L/R bar;
    - the Alt zoom gestures stay. If the hands-on test in T-037 or T-038 shows the host menu bar activating, a later small task changes the modifiers. Design nothing for that now.
  - **From T-043:** follow its "Recommendations for M3c" (R1 to R20) wherever they concern the UI, and list every one you adopt or reject in your log. The IP note applies: use generic mode names only (dBFS+3, K-20, LUFS, LU, Underlay, Density, Align 0 dB), never SPAN's texts, artwork, preset names or file formats.
  - **From T-044:**
    - The band solo is Ctrl plus left-drag (SPAN confirms it), with Ctrl+wheel setting the width. The band edges are at fc·2^(±W/2), and the filter is unity at the centre and −3 dB at the edges, so draw the edge markers at those frequencies.
    - The colour wheel: `juce::ColourSelector` has a square HSV field plus RGB and alpha sliders, and no circular wheel, hex field or presets. Choose and specify one of: an own hue-wheel component (recommended if the design wants a circle), `ColourSelector` plus an own hex field and preset row, or reFX/ColourSelector (MIT; whether it has a round wheel is not documented). Record the choice and the reason. Any third-party code goes through build-engineer.
    - File dialogs are native, asynchronous and can fall behind the host window. The flow must not assume a modal dialog.
    - Sidechain 2 may not exist in some hosts (Live, Bitwig: probably only one sidechain). Specify how the UI shows an input that the host does not provide.
- 2026-10-04 vst-architect (progress record; the ui-ux-designer session stopped before it wrote a log entry): the work is partly done and nothing is committed. The next session resumes from here.
  - **Done:**
    - `docs/design/ui-spec.md`: new §17 "M3c additions" (§17.1 to §17.21, where §17.21 lists the 20 changes from the plan), new §6.3, §10.4, §12.2 and §13.4, and "M3c" pointers in the existing sections. §5.9.3 gains rows G22-M3c, G23-M3c and G24, labelled "M3c only"; rows G1 to G23 (the M3b behaviour) are unchanged. §15 holds open questions 7 to 15, and §16 has a change-log row.
    - `docs/design/tokens.json` version 1.2.0.
    - 12 new SVGs in `assets/ui/`: `icon-refs`, `icon-refs-16`, `icon-eye-16`, `icon-eye-off-16`, `icon-section-bars-16`, `icon-section-table-16`, `icon-section-loudness-16`, `icon-section-correlation-16`, `icon-warning-16`, `icon-chevron-down-16`, `cursor-band`, `cursor-band-solo`.
  - **Design decisions so far:**
    - an own `ColourWheel` component (a hue ring, a saturation and value square, a hex field), with no third-party code;
    - a compact header below 1174 px wide, where Refs, Freeze and Reset curves become icons;
    - one Refs panel holding the reference slots and the colour editor;
    - a two-row meter header (Metering, Style and Reset; Integration and four section toggles), with Release, Peak hold and the RMS window in Settings > Meters;
    - LUFS and LU as one wide programme bar, with no Density style in those modes;
    - Sidechain 2 with a striped fill; each reference slot with its own dash pattern;
    - the band-solo readout in the hover card, tagged BAND while previewing and SOLO while listening.
  - **Draft disposition of T-043 R1 to R20 (to confirm in the next session):** adopted: R1 to R3, R5 to R7, R10 to R16, R18 to R20. R4 is partly adopted. R17 (one fill switch for every reference) is rejected. R8, R9 and R15 concern the DSP only. R15 is listed both as adopted and as DSP-only, so resolve that.
  - **Remaining:**
    1. Extend `docs/design/mockup.html` to every demonstration item in the acceptance criteria. This enables the approval gate.
    2. Fix a known mismatch: §17.9.2 and the §11 row describe `cursor-band-solo.svg` as "filled brackets and sound marks", but the file shows a filled band between the brackets.
    3. Check §17 against the new tokens and SVGs.
    4. Write the full log entry with the R1 to R20 disposition, then set the status to IN_REVIEW.
- 2026-10-06 vst-architect: **Scope narrowed by the M3 review triage** (`docs/plans/m3-review-spectrum-slots.md`). Read that plan before resuming.
  - **Superseded parts of §17.** The following parts are now covered by a new task, **T-065** (spec §18 and the mockup for the review items and the spectrum slots), which you or the next ui-ux-designer session run right after this task:
    - the **underlay** (the Channel plus Underlay header control, the underlay curve style);
    - the **per-input labels**;
    - the **Sidechain 2 trace toggle**;
    - the **colour roles** tied to Primary B.

    Do not extend the mockup for them. Mark those §17 subsections "Superseded by §18 (T-065, spectrum slots)" with a one-line pointer, and keep their text for history.
  - **Finish the rest of the current scope:**
    - mockup steps 1 to 4 above, for the remaining §17 items: band solo, metering modes and styles, ballistics, section hiding, 1 Hz, Align and anti-alias, colour wheel and colour presets (applied to generic curve roles), frame rate, reference slots, CSV, settings presets, and the compact header;
    - the cursor SVG mismatch;
    - the token check;
    - the R1 to R20 disposition.
  - **Approval.** It moves: the user approves **one combined mockup after T-065**, so set this task to DONE (not IN_REVIEW) when the narrowed scope is delivered, and note in the log that the approval is pending T-065.
  - **Scope.** Do not add the M3d review items (tooltips, the "SC" text, the 48 kHz label, All Settings duplicates, the 3 dB grid) here. T-065 covers them.
