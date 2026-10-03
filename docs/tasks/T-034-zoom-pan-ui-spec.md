# T-034: Zoom and pan: UI spec addendum and mockup
- Owner: ui-ux-designer
- Milestone: M3b
- Status: IN_REVIEW
- Depends on: none

## Goal
Specify spectrum zoom and pan exactly enough for gui-engineer to implement it without guessing, and show it in the HTML mockup so the user can try it before any C++ is written. The feature and its model are fixed by `docs/plans/spectrum-zoom-pan.md`:
- a view layer inside the home range set by the range chips;
- wheel and Ctrl+wheel zoom;
- left-drag and middle-drag pan;
- double-click Fit;
- Ctrl+drag box zoom;
- an overview strip and a zoom indicator.

## Inputs
- `docs/plans/spectrum-zoom-pan.md`: all of it, in particular the View model, the Interaction table and Freeze semantics. Its numbers are architect defaults. You may change them with a written reason.
- `docs/design/ui-spec.md`: §4.1 (regions), §5 (spectrum view), §7 (interaction conventions), §10 (accessibility), §12 (performance rules) and §13.3 (frame and view state).
- `docs/design/tokens.json` and `docs/design/mockup.html` (the approved T-010 mockup).
- `docs/roadmap.md`, Decisions of 2026-10-03: the editor rendering rules from T-029 (no polygons per frame, one `fillRectList` per colour, cached static parts, repaint only on a physical-pixel change). New visuals must be drawable under these rules.
- `source/gui/SpectrumMath.h`, for reference only: the current label sets (`chooseFrequencyLabelSet`, `chooseDbStep`), which no longer cover narrow spans.

## Scope
1. Add **§5.9 "Zoom and pan"** to `docs/design/ui-spec.md`. It covers:
   - the view model, in user terms;
   - every gesture in the plan's interaction table, with exact numbers: notch factors, the fine factor, the minimum spans, the drag threshold, the click defer, and the box-cancel threshold;
   - the cursors for each state: crosshair, open hand where useful, grabbing hand, and box zoom;
   - the behaviour of the hover card, crosshair and markers during a pan and a box drag;
   - what happens at Fit, where a drag does nothing.
2. Update the existing sections:
   - §5.5: the hint line text, which must still fit at the minimum size 920×540;
   - §5.6: replace the interaction table;
   - §5.7: freeze plus zoom, as in the plan's Freeze semantics;
   - §12: performance rules for continuous pan and zoom;
   - §13.3: the view state, not persisted.
3. Specify the **zoom indicator** (the visible range text, the Fit button, and the bin-spacing hint shown below 16 bins in view) and the **overview strip** (position, size, curve style, window-rectangle style, and states). Add tokens to `tokens.json` for every new colour and size.
4. **Collision rules.** Specify how the new elements coexist with the legend (§5.2), the FROZEN badge (§5.7), the hover card (§5.5), the toasts (§5.6) and the hint line, at the default size 1200×700 and the minimum size 920×540.
5. **Deep-zoom grid rules:**
   - Frequency lines and labels for every span from the home range down to 1/6 octave: at least 2 labels visible at any zoom; the existing ≥ 36 px gap between label centres kept; finer "nice" steps (for example 1, 2 and 5 × 10^n Hz) when the decade sets give too few labels; and the label text format at fine steps (for example "1.25k", "102").
   - The dB steps at a 6 dB span, adding sub-dB steps if needed, with the label format.
6. **Mockup update** (`docs/design/mockup.html`). Implement every gesture with the synthetic spectrum the mockup already uses: wheel and Ctrl+wheel zoom, gutter wheel, drag pan, double-click Fit, Ctrl+drag box zoom, the overview drag and click, and the indicator's Fit button. It must open from the file system in a browser with no server.

## Acceptance criteria
- [x] §5.9 covers every row of the plan's interaction table with exact numbers. Any deviation from the plan's defaults is listed in a "Changes from the plan" sub-section with a reason.
- [x] The indicator and the overview strip have exact geometry, using tokens, at 1200×700 and 920×540. They never cover the legend, the FROZEN badge or the toasts. The hover card's placement rules account for them.
- [x] The deep-zoom grid rules give at least 2 frequency labels and at least 2 dB labels at every allowed span, with label centres at least 36 px apart for frequency and lines at least 20 px apart for dB. The spec includes a worked table for 5 example views, including the minimum spans at the minimum size.
- [x] New text and non-text elements meet §10 contrast: at least 4.5:1 for text and at least 3:1 for non-text, with the ratios listed.
- [x] Every new element is drawable under the T-029 rendering rules. §12 says how: for example, the rectangle list per colour, the cached label images, and the repaint regions.
- [x] The mockup implements every gesture and runs from the file system. The spec's change log names T-034.
- [x] Open questions, if any, are listed in §15 with a recommended answer.
- [ ] The user approves the updated mockup. The architect records the approval in this log. T-037 does not start before it.

## Log
- 2026-10-03 vst-architect: Created from `docs/plans/spectrum-zoom-pan.md`. Documentation only, so it may run in parallel with a C++ task. Not yet dispatched: the user asked for documentation changes only for now.
- 2026-10-03 ui-ux-designer: Delivered. `docs/design/ui-spec.md`: new §5.9 (view model; view arithmetic; gestures G1 to G21 with exact numbers; cursors; hover behaviour during gestures; zoom panel = indicator + Fit button + bin-spacing row + overview strip, geometry at 1200×700, 920×540, collapsed and 2560×1600; collision rules and z-order; deep-zoom grid rules with a 6-row worked table; Fit and no-op cases; changes from the plan; mockup simplifications). Updated §4.1, §5.3, §5.5 (hint texts fit 544 px at 920×540; 4-figure hover precision below 1 octave), §5.6 (table replaced), §5.7 (freeze plus zoom, epoch), §6, §7.2, §10 (new §10.2a ratios), §11, §12.1, §13.3, §15 (questions 5 and 6), §16 change log. Folded in the T-011 corrections (fonts in `third_party/fonts/` and `InvestigateAssets`; old §15 questions 1 to 3 closed). `tokens.json` 1.1.0: groups `zoom`, `zoomLayout`, `zoomInteraction`, `deepGrid`, `cursor`; font paths corrected. New assets: `assets/ui/cursor-hand-open.svg`, `cursor-hand-grab.svg`, `cursor-zoom-box.svg`. `mockup.html`: every gesture implemented with the synthetic spectrum (now 4096 log-spaced points so zoom shows detail); the Google Fonts link was removed, so it opens from the file system with no network. Plan numbers kept (0.8/1.25, 0.95 fine, 1/6 octave, 6 dB, 4 px, 250 ms, 4 px box cancel); refinements with reasons in §5.9.10 (per-notch-equivalent factors with 1/0.95 fine out, 10 % horizontal pan, per-axis home-range rule, press-to-centre in the overview, one combined zoom panel, view repaints follow T-029 pacing). Status IN_REVIEW: the last criterion is the user's approval of the mockup, recorded by the architect.
