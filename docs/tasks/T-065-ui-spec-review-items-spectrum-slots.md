# T-065: UI spec §18 and mockup: review items and spectrum slots
- Owner: ui-ux-designer
- Milestone: M3c
- Status: IN_REVIEW
- Depends on: T-045

## Goal
Specify, and show in the mockup, every visual and interaction change from the user's M3 review:
- the M3d visual fixes;
- the four-slot spectrum model with its single settings window;
- the sidechain meters;
- Offset, Fill and the 96 kHz axis;
- the Max crest readout;
- the compact layout with a smaller minimum size;
- A/B.

The user approves **one combined mockup** (T-045 plus this task) before T-054 starts.

## Inputs
- `docs/plans/m3-review-spectrum-slots.md`. It is the source of truth for behaviour; this task decides the look and the interaction details.
- `docs/plans/m3-user-review-findings.md` (all findings, with the user's own wording).
- `docs/design/ui-spec.md` (including §17 from T-045 and the subsections T-045 marked "Superseded by §18"), `docs/design/tokens.json` (1.2.0), `docs/design/mockup.html`, and `assets/ui/`.
- `docs/research/T-043-span-m3c-reference.md` (SPAN conventions) and `docs/research/T-061-review-bug-diagnosis.md`, if delivered (#16 constraints at small sizes).
- The roadmap rendering rules (Decision of 2026-10-03, after T-029): no per-frame polygons, one `fillRectList` per colour.

## Scope
Write a new **§18 "M3 review and spectrum slots"** in `ui-spec.md`. Bump the tokens to 1.3.0, add SVGs as needed, and extend the mockup.

1. **The M3d visual fixes**, delivered first as §18.1, because T-066 implements them before M3c:
   - **#18:** remove every tooltip except the spectrum hover readout. List each tooltip that disappears, and say what replaces any information that only a tooltip carried (for example a unit shown in the chip text).
   - **#19:** remove the "48 kHz" label. Say whether the sample rate appears anywhere else (for example the About view), or nowhere.
   - **#20:** "All Settings" no longer repeats settings that are on the main interface. Give the resulting list.
   - **#10 and #11:** the sidechain button without the "SC" text, turquoise when active (the level-meter turquoise token). From M3c, the same rule applies to the slot visibility toggles.
   - **#4:** the spectrum level grid uses 3 dB steps at the default range. Define the step ladder for wide and narrow ranges (for example 1, 3, 6, 12 and 24 dB) with a minimum pixel spacing, at 1200×700 and at the minimum size.
   - **#5:** the chip behaviour when the user sets the bottom at or above the top (the effective bottom is top − 1 dB; the chip shows the effective value or a marker).
2. **The spectrum slots:**
   - **The "Spectra" list:** where it lives (header, a panel, or the Refs-style panel), and one row per slot: visible toggle, colour swatch, editable name (16 characters), source, type, and a gear button.
   - **The slot settings window**, one component for all four slots: Source, Channel, Type, Link, Block size, Overlap, Window, Avg time, Smooth (up to 3 octaves), Slope, Fill. Specify:
     - how Link shows slot 1's values, and that editing an analysis field turns Link off;
     - that slot 1's source and type are locked;
     - that slot 1's window opens from the bottom bar gear, and that the bottom bar keeps controlling slot 1;
     - how the window behaves inside a host: an in-editor panel or popover, not an OS window, unless you justify otherwise.
   - **Curve styles** per slot and per type (Real-time, RT max, All-time max, All-time avg), including Fill, while keeping the colour-blind-safe distinctions of §17, and the "no input" state.
   - **Default slot colours**, mapped from today's tokens (slot 1 = primary, slot 2 = secondary tone, slot 3 = sidechain, slot 4 = the §17 Sidechain 2 colour).
   - **The hover card:** which slots it lists, and the applied Offset.
3. **Offset:** the selector (Off, Normalize, Center, Spectrum 2, 3, 4), its position, the disabled and inactive states, and how the applied offset is shown.
4. **The 96 kHz axis:** grid labels up to 96 kHz, and the look of the region above the Nyquist frequency.
5. **Sidechain meters (#12, user decision):**
   - each connected sidechain's bars next to the main bars;
   - its statistics, loudness and correlation rows below the main rows, at a lower opacity: give the token value, chosen so the values stay readable at 4.5:1 or better against the panel background while staying clearly secondary;
   - how the section toggles apply;
   - the layout with zero, one or two sidechains, at 1200×700 and at the minimum size.
6. **Max crest factor (#13):** a read-only readout in the control bar, to the left of the frequency range controls.
7. **Compact layout (#14, #16):**
   - it switches on below 600 px wide, and height does not trigger it;
   - it hides Peak, Peak Max, TP, TP Max, Headroom, RMS, Crest, Clips, TP Clips and Loudness;
   - the spectrum and the level meter stay;
   - the correlation meter adapts.

   Set the new **minimum size**, at most 560 px wide and at most 400 px high, unless you justify a different value. Merge the T-045 compact header (below 1174 px) into one set of width breakpoints.
8. **A/B (#22):** the A, B and "Copy A→B" controls, the active-letter indicator, and their position relative to the Presets menu.
9. **Mockup:** extend `mockup.html` so the user can see and click through:
   - every item above;
   - the default four-slot state, which must look like today;
   - a state with all four slots and both sidechain meters;
   - the compact layout (a width slider or a toggle);
   - A/B switching.
10. Update §15 (open questions) and §16 (change log). List every token and SVG added.

## Acceptance criteria
- [ ] §18.1 (the M3d fixes) is complete and self-contained, so T-066 can implement it without the rest of §18. The log states when §18.1 was finished.
- [ ] Every scope item above has a spec subsection with measurements at 1200×700 and at the new minimum size, and with states (normal, hover, active, disabled, inactive) where relevant.
- [ ] The mockup demonstrates every item in scope item 9. The default state matches the current product's look.
- [ ] The sidechain rows meet 4.5:1 contrast at their reduced opacity. The opacity token is listed.
- [ ] tokens.json is at 1.3.0. Every new SVG is in `assets/ui/` with its use listed in the spec.
- [ ] The log records the decisions taken, the open questions for the user, and the note that the combined approval (T-045 plus T-065) is pending.

## Log
- 2026-10-06 vst-architect: Created from the M3 review triage (`docs/plans/m3-review-spectrum-slots.md`). This task runs after T-045 (same files). T-066 (M3d editor fixes) waits for §18.1 only, so write §18.1 first and log it. The other parts can follow in the same session.
- 2026-10-07 vst-architect: T-045 and T-061 are DONE. What this task takes from them:
  - **Files.** `tokens.json` is now 1.2.1 (no value changes; it marks the M3b-only and superseded tokens). Bump it to 1.3.0 here. The subsections T-045 marked "Superseded by §18" are §17.3 and §17.6 in full, and §17.2, §17.4, §17.5, §17.14.2, §17.16 and §6.3 in part. §18 must replace each of them explicitly. The header in the mockup still shows the approved M3b Mode control and SC toggle in their place; replace them with the slot model.
  - **Items handed over by T-045:**
    1. The CSV "1 Hz to 48 kHz" message and range follow the 96 kHz reference-grid decision (roadmap Decision of 2026-10-06).
    2. The note on a K-weighted max crest factor (T-043 R4) belongs with the Max crest readout (scope item 6). Max crest uses SPAN's definition (50 ms windows, maximum since reset), per the roadmap Decision of 2026-10-06.
    3. Merge the 1174 px compact-header breakpoint and the 600 px compact layout into one set of width breakpoints (scope item 7).
    4. T-043 R17 (one fill switch for every reference) was rejected in T-045. Decide whether per-slot Fill covers it, and list it as an open question for the user only if it does not.
  - **From T-061, section #16:** the FL Studio glitch probably comes from the host imposing a size below the constrainer minimum. The spec for the new minimum size must define a valid layout for every region (header, spectrum, control bar, meters) at every size from the minimum up. It must also say what the editor shows if a host imposes a smaller size anyway: the regions clamp, and nothing gets negative bounds.
  - **The mockup was never opened in a browser.** T-045 had no browser and checked the code only by reading it. After this task, the architect has build-engineer open `mockup.html` in headless Edge and report console errors. Any errors come back to ui-ux-designer as a small fix before the user reviews the combined mockup. Keep the mockup a single self-contained file with no network dependencies, so the headless check is meaningful.
  - §15 questions 7 to 15 are still open. Carry them forward, and present them with this task's own questions for the combined review.
- 2026-10-07 ui-ux-designer: **§18.1 (M3d visual fixes) is finished and self-contained; T-066 can start from it.** It was the first item written in this session (restart), before any other §18 text; this agent has no clock, so no time of day is recorded. Contents: §18.1.1 tooltips removed (table of every removed tooltip and its replacement: Block menu hints for ms and bin spacing, gutter hints for level zoom, texts moved to accessible descriptions; the hint line is kept as part of the hover readout); §18.1.2 sample-rate label removed, shown nowhere live; §18.1.3 SC toggle icon-only at (W − 336, 8, 28, 28), teal `toggle-teal` #34C6A8 when on, all states; §18.1.4 Settings shows only settings whose main control is off screen (resolved at 1200 × 700 and 920 × 540, meters shown and collapsed); §18.1.5 effective values shown in `state-warn` with an underline, editing starts from the effective value (#5); §18.1.6 level grid ladder {0.1, 0.2, 0.5, 1, 3, 6, 12, 24} dB with at most 24 steps per span, 12 px line and 20 px label minimums, resolved at three heights; §18.1.7 Smooth and Level texts; §18.1.8 checklist. Tokens 1.3.0 and the mockup follow later in this session.
- 2026-10-07 ui-ux-designer (progress): spec §18.2 to §18.18 written, §15 questions 16 to 25 and the §16 row added. Remaining: `tokens.json` 1.3.0, the five SVGs, `mockup.html`, the final log entry and status IN_REVIEW.
- 2026-10-07 ui-ux-designer (progress): `tokens.json` 1.3.0 and the five SVGs (`icon-spectra-16`, `icon-presets-16`, `icon-copy-ab-16`, `icon-lock-16`, `icon-link-16`) done; §11 rows added. Remaining: `mockup.html`, final log, IN_REVIEW.
- 2026-10-07 ui-ux-designer: **Delivered. Status IN_REVIEW. The user's combined approval (T-045 plus T-065) is still pending; T-054 does not start before it.** Nothing is committed.
  - **Files:** `docs/design/ui-spec.md` (new §18.1 to §18.18, header addendum line, §11 rows, §15 questions 16 to 25, §16 row); `docs/design/tokens.json` 1.3.0; `docs/design/mockup.html`; five new SVGs in `assets/ui/` (`icon-spectra-16`, `icon-presets-16`, `icon-copy-ab-16`, `icon-lock-16`, `icon-link-16`); this file.
  - **Decisions:**
    - Tooltips: none anywhere; texts become accessible descriptions; the hint line stays as part of the hover readout; Block menu hints and gutter hints carry the only information tooltips held (§18.1.1).
    - Sample rate: no live label anywhere (§18.1.2).
    - SC toggle and slot toggles: icon or number only, `toggle-teal` #34C6A8 when on (§18.1.3, §18.3.1).
    - Settings: a row appears only while its control is off the main window; the Settings dot means "Settings holds something" (§18.1.4, §18.12).
    - Effective values: the effective value in `state-warn` with an underline; editing starts from it; choices keep the set value in the same look (§18.1.5, §18.7).
    - Level grid: ladder {0.1, 0.2, 0.5, 1, 3, 6, 12, 24} dB, at most 24 steps per span, lines ≥ 12 px, labels ≥ 20 px (every other line otherwise) (§18.1.6).
    - Spectra list: a header group (list button plus toggles for slots 2 to 4) and a 480 px Spectra panel; slot 1 has no visibility toggle (no parameter).
    - Slot window: one in-editor popover with slot tabs, 520 × 312; Link shows slot 1's values live, and editing any analysis field unlinks with a copy of slot 1's values first; slot 1's Source and Type are locked; the slot-1 gear sits at the left of the control bar.
    - Styles: line by type, fill by source and Fill, hatch for slot 4; defaults reproduce today's look; per-slot Fill covers T-043 R17, which is therefore not an open question.
    - Offset: header chip from 900 px, Spectra panel below; options read "Match {slot name}"; disabled and inactive states; legend tag and hover line.
    - 96 kHz: the axis runs to `freqHigh` above Nyquist; the region above Nyquist is shaded in the grid image; the CSV range is 1 Hz to 96 kHz.
    - Canvas: minimum **560 × 400**; one set of width breakpoints 1060 (icon buttons), 900 (compact header), 600 (compact layout); no height breakpoint.
    - Control bar: the 2nd curve chip leaves (slot 2's type is in the Spectra list); Slope joins group A with the slot-1 gear; Max crest is the first item of group C; items hide one by one.
    - Sidechain meters: the strip stays 296 px; one bar per sidechain, 10 px; metrics blocks with a reduced row set at `meter.sc-opacity` 0.72 (pre-blended #ACB1B9 values 8.2 : 1, #7B8496 labels 4.7 : 1); a fit ladder keeps every size valid and the main metrics complete first.
    - Max crest: unweighted (SPAN's definition); T-043 R4's K-weighted variant is not adopted.
    - A/B: segmented A and B plus Copy, right of Presets; Copy acts at once with a toast.
    - Legend collision rule: obstacles (badge, zoom panel) limit each row; a row where nothing fits stays empty; the badge moves left of the zoom panel. Resolved at 560 × 400, 600 × 400, 920 × 540 (the T-037 23.5 px case) and 1200 × 700.
    - Host-imposed sizes below the minimum: layout at the minimum, `std::max (0, …)` everywhere, nothing with negative bounds (T-061 §3).
    - Popovers become 520 wide so they fit the 560 px minimum; Take and Take 2nd become one "Take ▾" menu over the slots.
  - **Handed-over items resolved:** the CSV 96 kHz message (§18.8); the K-weighted crest note (§18.11); the merged breakpoints (§18.9.1); T-043 R17 (§18.5).
  - **Mockup:** demonstrates every item of scope item 9 (default four-slot state, "All four slots and both sidechains" button, width slider and the 560 × 400 / 600 × 400 sizes, A/B), see spec §18.15. **It was not run in a browser:** this agent has no browser or shell, so it was checked by reading the code only. The architect's planned headless-Edge smoke test is needed before the user review.
  - **Open questions for the user** (carried §15 questions 7 to 15, plus the new 16 to 25; recommendations in §15): 7 band-solo edge function (T-050); 8 choice texts (T-051); 9 preset-name reason codes (T-053); 10 display refresh measurement (T-054); 11 a loudness target mark; 12 Release and Peak hold in Settings; 13 default colours of Sidechain 2 and the references; 14 a CSV sample; 15 touchpad pinch; 16 keep the hint line; 17 one bar per sidechain, main bars 13 px with two; 18 `DisplayRange.h` axis above Nyquist (T-047); 19 `maxCrest[ch]` in `MeterSnapshot` (T-049, T-052); 20 minimum 560 × 400 and the compact layout; 21 "Match {slot name}" wording; 22 Copy without confirmation; 23 the reduced sidechain metrics; 24 slot 1 always shown; 25 the new default legend texts.
- 2026-10-07 ui-ux-designer: Legend fix (headless Edge 136 at 1200 × 700): the legend rows and the measuring row are absolute children of the zero-width `#legend` wrapper, so they shrank to min-content and every entry wrapped word by word (clipped to 20 px it showed "·", "2 · RT" and a two-line "SC 1"); the slot names were never blank. `mockup.html` now gives legend rows `width:max-content`, entries `white-space:nowrap; flex:none`, truncates only an entry wider than the data width with "…", and re-measures after `document.fonts.ready`; the hover card and the Spectra panel already read the same `SN[]` names. Spec unchanged; status stays IN_REVIEW.
- 2026-10-07 vst-architect: A headless Edge 136 re-render confirms the legend fix at 1200 × 700, 920 × 540 (live and frozen, about 11 px of clearance from the badge), 600 × 400 and 560 × 400. There are no console errors (206 listener elements clicked at 4 widths). **User answers to §15** (roadmap Decisions of 2026-10-07):
  - Accepted as recommended: 11, 12, 13, 16, 20, 21, 22, 24 and 25. The architect decided 7, 8, 9, 10, 18 and 19 as recommended.
  - **Changed, 17 and 23 (the sidechain meters):** each sidechain keeps **one bar**, but its rows show **every statistic the main input shows** (the same rows and units as the main block: Peak, Peak max, TP, TP max, Headroom, RMS, Crest, Max crest, Clips, TP clips, the loudness block and correlation with balance), at `meter.sc-opacity`. Where a short window has no room, apply the existing fit ladder (rows shrink, then give way to a note), and define that ladder for the full set at 1200 × 700, 920 × 540, 600 × 400 and 560 × 400 with zero, one and two sidechains. Update §18 (the sidechain-meter subsection, §18.14 departures, and §15 question 23 as resolved), and the mockup.
  - **Revision.** The status goes back to IN_PROGRESS for this revision only. Set it to IN_REVIEW again when it is delivered. The mockup approval is still pending: the user reviews the mockup after this revision.
- 2026-10-07 ui-ux-designer: **Revision delivered (sidechain full statistics). Status IN_REVIEW. The combined mockup approval (T-045 plus T-065) is still pending.** Nothing is committed. The mockup was checked by reading only; it needs the headless Edge re-render.
  - **Files:** `docs/design/ui-spec.md` (§18.10.1 to §18.10.4, §18.9.5, §18.11, §18.14 change 2, §18.15, §18.17, §18.18, §15, §16); `docs/design/tokens.json` (`stripSidechain` revised, version stays 1.3.0 because it is not approved yet; meta note says so); `docs/design/mockup.html`; this file. No SVG changes.
  - **Decisions:**
    - One **sidechain table** at full strip width (x 12, 272 wide) below the main sections; the bars column ends above it. Full width is what makes the full set fit: a side-by-side layout for one sidechain (form A: 16 + 10 r, the ten per-channel rows with the eight per-input values beside them), a three-cell grid when Statistics is off (form B), and a pair table for two sidechains (form C: label 84 + four 47 px columns, SC 1 and SC 2 side by side on each row).
    - Rows: Peak, Peak max, TP, TP max, Headroom, RMS, Crest, **Max crest**, Clips, TP clips (Max crest is a row because the main value is in the control bar); M, S, I, M max, S max, LRA; correlation and balance. Same units and formats as the main block; alarm cells (clip lamps, negative headroom) keep full strength; everything else at `meter.sc-opacity` 0.72 (8.2 : 1 values, 4.7 : 1 labels).
    - **Fit ladder** (8 steps): SC rows 16; main rows 18; main rows 16; SC loudness and correlation give way (two sidechains); the table gives way to the note; then the existing main short forms. Resolved: 1200 × 700 shows everything with one sidechain (main rowH 18) and the two sidechains' statistics with the loudness and correlation note (main rowH 16); 920 × 540 and 600 × 400 show the note; 560 × 400 is compact with two 16 px lines (correlation, balance) per sidechain. Everything shows from H = 698 (one) and H = 760 (two). Nothing overlaps and the control bar is unchanged at every size (the strip stays 296 or 120 wide).
    - §15: 7 to 13 and 16 to 25 marked resolved as the log entry above says; 14 and 15 stay open.
  - **Mockup:** the sidechain simulation now produces every statistic (per-channel peak and TP ballistics, crest, clip counts, M, M max, S max, LRA); the three table forms, the ladder and both notes; the compact lines; a new dev-bar size "1200 x 780" where two sidechains show every row. Legend code untouched.
  - **Open question for the user (new §15 question 26):** at 1200 × 700 with two sidechains, keep both sidechains' statistics and drop their loudness and correlation rows (recommended), or keep SC 1 complete and drop SC 2's rows.
