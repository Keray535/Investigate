# T-065: UI spec §18 and mockup: review items and spectrum slots
- Owner: ui-ux-designer
- Milestone: M3c
- Status: TODO
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
