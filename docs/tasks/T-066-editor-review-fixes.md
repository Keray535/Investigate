# T-066: Editor review fixes
- Owner: gui-engineer
- Milestone: M3d
- Status: TODO
- Depends on: T-064, T-065 (§18.1 delivered; the mockup approval is not needed), T-038 (build order)

## Goal
Implement the editor side of the M3d review fixes:
- the 3 dB spectrum grid and the narrow-range behaviour (#4, #5);
- the sidechain button look (#10, #11);
- no tooltips except the hover readout (#18);
- no "48 kHz" label (#19);
- no duplicates in All Settings (#20);
- the small-window glitch (#16), if T-061 found a cheap fix in the editor.

## Inputs
- `docs/design/ui-spec.md` §18.1 (T-065), plus `tokens.json` 1.3.0.
- `docs/plans/m3-review-spectrum-slots.md` (M3d scope) and `docs/plans/m3-user-review-findings.md`.
- `docs/research/T-061-review-bug-diagnosis.md`, section #16.
- The logs of T-063 (the 1 dB rule, the minimum level span) and T-064 (the new `dbLow`/`dbHigh` ranges and defaults, the new smoothing choices, and any Standalone monitoring control that must appear in the editor).
- `source/gui/` (`SpectrumView.*`, `SpectrumMath.h`, `RangeChip.*`, `TooltipText.*`, `SettingsPopover.*`, `ControlBar.*`, `HeaderButton.*`, `InvestigateLookAndFeel.*`), `source/PluginEditor.*`, and the T-011, T-012, T-029, T-037 and T-038 logs.
- Working rules from the roadmap handoff:
  - one C++ task at a time;
  - the GUI tests use `GuiTestEnvironment.h`, with bounded waits;
  - the T-029 rendering rules;
  - CPU is gated as a median of 3;
  - coders never edit `docs/qa/`.

## Scope
1. **Grid (#4).** The spectrum level grid follows the §18.1 step ladder (3 dB at the default range). The level-meter grid is unchanged. The deep-zoom grid (T-037) uses the same ladder.
2. **Narrow ranges (#5).** The chips and the axis handle a bottom at or above the top per §18.1 (the effective bottom is top − 1 dB, from T-063). The Level chip accepts the full −150 to +30 dB range on both edges.
3. **Sidechain button (#10, #11).** No "SC" text; the turquoise active colour per §18.1.
4. **Tooltips (#18).** Remove every tooltip and the `TooltipWindow`, except the spectrum hover readout. Remove any code that only fed tooltips (`TooltipText.*`), if nothing else uses it.
5. **"48 kHz" (#19).** Remove the label per §18.1.
6. **All Settings (#20).** Remove the duplicated rows per §18.1.
7. **Smoothing choices.** The Smooth control shows the 10 choices from T-064.
8. **Small windows (#16).** Apply the T-061 fix if it is in the editor and fits this task. Otherwise record that it moves to T-070.
9. **Standalone monitoring.** Only if T-064 left an open question for an editor control: add it per §18.1.

## Acceptance criteria
- [ ] **Grid.**
  - At the default range (−18 to −72 dB) at 1200×700, the grid lines sit at every 3 dB, and the labels follow §18.1.
  - At −150 to +30 and at a 1 dB range, the steps follow the ladder, with no line closer than the §18.1 minimum spacing.
  - The level-meter grid is pixel-identical to before.
- [ ] **Narrow range.** With `dbLow` ≥ `dbHigh`, the plot uses top − 1 dB as the bottom, and the chip shows what §18.1 specifies. No assertion and no division by zero.
- [ ] **Tooltips.** No component shows a tooltip on a 1.5 s hover (an automated sweep over every child component). The hover readout still works.
- [ ] **Labels.** "SC" and "48 kHz" no longer appear. The sidechain button's active colour equals the turquoise token (pixel check).
- [ ] **All Settings.** It contains exactly the §18.1 list.
- [ ] **Regression.** Every existing GUI test passes, updated only where this task intentionally changes the output (each change listed in the log). The T-037 and T-038 zoom tests pass.
- [ ] **Budgets.** Whole-editor message thread ≤ 25 % at 1280×720 and 2560×1440, and paint p95 ≤ 4 ms and ≤ 8 ms (T-012/T-029 harness, median of 3).
- [ ] Warning-clean at `/W4`. `docs/qa/T-066-unit.md` and `docs/qa/T-066-validation.md` report PASS.

## Log
- 2026-10-06 vst-architect: Created from the M3 review triage. After this task, the M3d gate runs: the user re-checks #1, #2, #4, #5, #6, #10, #11, #15 and #18 to #20 in FL Studio.
