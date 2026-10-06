# T-057: Meter panel M3c: metering modes, styles, ballistics, section hiding
- Owner: gui-engineer
- Milestone: M3c
- Status: TODO
- Depends on: T-056

## Goal
Bring the meter strip to the M3c design:
- all 14 metering modes, with their scales;
- the Bars and Density styles;
- the integration, release and peak-hold controls driving the display;
- hiding each section on its own, while the header toggle still hides everything.

## Inputs
- `docs/design/ui-spec.md`: the M3c addendum from T-045 (scales and ticks per mode, density rendering, ballistics formulas, controls, section-hiding control and re-flow, the RMS unit per mode), together with the existing §8.
- `docs/plans/m3c-feature-expansion.md`: design items [2], [3] and [10], the "MeterSnapshot additions" section, and the Storage summary.
- The task logs of T-049 (`level`, `density`, the bin constants, and the optional per-channel loudness), T-051 (`meterScale` extension, the `MeterScale.h` helpers, `meterStyle`, the ballistics parameters, the `meterSections` default) and T-052 (the parameters reach the DSP).
- `source/gui/MeterPanel.*`, `MeterMath.h`, `LayoutMath.h`, `ViewStateStore.*`, and the T-013 log (repaint rules, update rates, the stale-snapshot logic).
- Working rules as in T-054.

## Scope
1. **Metering modes.** All 14 modes, with the scale, ticks, zones and labels from the spec:
   - the level part of the bar shows `level`, plus 3.01 dB in dBFS+3 and in the K modes;
   - in the loudness modes it shows the momentary or short-term loudness;
   - the RMS row's unit follows the mode.
2. **Ballistics.**
   - The peak part falls at 20 dB per `meterRelease`.
   - The hold tick holds for `meterPeakHold`, then falls as the spec says.
   - Both replace today's fixed 20 dB/s, 1500 ms and 30 dB/s.
   - The K-xx C modes use 600 ms.
3. **Density style.** Render `density[ch]` per the spec, with a bounded number of rectangle lists: brightness levels quantised to a fixed palette. No per-bin path objects.
4. **Controls.** Metering (the menu order per the spec), Style, Integration, Release and Peak hold, where the spec places them, with the existing chip behaviours.
5. **Section hiding.**
   - The spec's control toggles the bars, the statistics table, the loudness block and the correlation block.
   - The state is stored in `ViewState.meterSections`.
   - The remaining sections re-flow per the spec.
   - Every section hidden behaves as collapsed.
   - The header toggle (`metersCollapsed`) works independently, as today.

## Acceptance criteria
- [ ] **Modes.** For each of the 14 modes, with a synthetic snapshot, the bar heights, tick labels and readouts match the spec's formulas within 1 px and 0.1 dB. The loudness modes follow `momentary` or `shortTerm` (or the per-channel values, if T-049 added them).
- [ ] **Ballistics.** With synthetic snapshot sequences:
  - the peak part falls 20 dB in the release time ± 5 %;
  - the hold tick holds for the peak-hold time ± one update interval;
  - changing a parameter takes effect on the next update;
  - the C modes use 600 ms.
- [ ] **Density.** A synthetic histogram renders to the expected brightness at the expected bar positions (a render test). The paint uses at most the spec's number of rectangle lists.
- [ ] **Sections.**
  - Every one of the 16 combinations lays out without overlap at 920×540 and 1200×700.
  - The state survives a state round trip, including the XML string conversion.
  - The header toggle overrides the sections and restores them unchanged.
- [ ] **Regression.** Every T-013 test passes, adjusted only where the spec changed a behaviour (each adjustment listed in the log). The stale-snapshot behaviour (§8.7) also applies to `level` and `density`.
- [ ] **Budgets** with the meters moving in Density style plus the full spectrum, at a 60 fps cap, median of 3: whole-editor message thread ≤ 25 % at 1280×720 and 2560×1440, and meter paint p95 ≤ 1.5 ms at 2560×1440.
- [ ] Warning-clean at `/W4`. `docs/qa/T-057-unit.md` and `docs/qa/T-057-validation.md` report PASS.

## Log
- 2026-10-03 vst-architect: Created from `docs/plans/m3c-feature-expansion.md`.
- 2026-10-06 vst-architect: **Amended by `docs/plans/m3-review-spectrum-slots.md`** (review findings #12 and #13; the layout follows T-065 §18):
  - **Sidechain meters (#12, user decision).**
    - For each connected sidechain input (`isMeterInputActive (1)` and `(2)`, T-052), draw its level bars **next to the main bars**, using its slot colour (the slot whose source is that input; if none, the default SC1 or SC2 colour).
    - Draw its statistics, loudness and correlation rows **below the main rows**, at the lower opacity that T-065 specifies, so they cannot be confused with the main values.
    - Read the values from `getMeterSnapshot (1)` and `(2)`.
    - A disconnected sidechain hides its bars and rows, and the layout re-flows.
    - The section toggles ([3]) apply to the sidechain rows of the same section.
    - The metering mode, style and ballistics are shared with the main meter.
  - **Max crest factor (#13, read-only).** Show the main input's `crestMax` (T-049) in the control bar, to the left of the frequency range controls, per T-065. The sidechain values appear in their statistics rows. Reset clears every value.
  - **No tooltips (review #18).**
  - **More criteria:**
    - with identical signals on Main, SC1 and SC2, the three bar sets and rows show identical values;
    - connecting and disconnecting a sidechain at runtime shows and hides its meters within one editor update, with no layout overlap at 1200×700 or at the new minimum size;
    - the sidechain rows render at the specified opacity (pixel check);
    - the whole-editor budget criterion includes both sidechain meter sets.
