# T-029: Editor rendering CPU: profile and reduce Direct2D message-thread load
- Owner: gui-engineer
- Milestone: M3
- Status: TODO
- Depends on: T-012

## Goal
Bring the editor's message-thread CPU load within budget, with margin left for the meter panel (T-013). T-012 met its paint-time budget by a wide margin (`paint()` p95 0.9 to 1.2 ms). Its CPU gate failed because Direct2D does rasterisation and present work after `paint()` returns. This task finds where that time goes, then reduces it. It does not change the look of the spectrum.

## Inputs
- `docs/qa/T-012-validation.md`: the CPU and paint table per case, the measurement method (`QueryThreadCycleTime` over wall time, 12 s, net of a paced baseline), and the repro command.
- `tools/render-harness/spectrum_check.cpp` (plugin-validator's harness). Its `d2d` mode reproduces the numbers. You may run it, but do not edit it; ask plugin-validator for changes.
- `source/gui/SpectrumView.*`, `source/PluginEditor.*` and the T-012 task log (frames are pulled in `pollProcessor()`, and `useGradientFill` is the spec §12 fallback switch).
- `docs/design/ui-spec.md` §12 (performance rules) and `docs/research/T-002-dsp-references.md` §8 (JUCE 8 rendering on Windows).
- The roadmap: Budgets, Risk 1, and the Decisions dated 2026-10-03.

## Measured starting point (T-012 validation round 2, Release, Direct2D, spectrum only, meter panel still a placeholder)

| Window | Case | CPU gross / net (% of one core, 60 fps) |
|---|---|---|
| 1280×720 | single + RT max (factory view) | 20.4 / 19.4 |
| 1280×720 | dual + 2 secondaries | 25.3 / 24.3 |
| 1280×720 | dual + 2 secondaries + SC + hover | 20.2 / 19.2 |
| 2560×1440 | single + RT max | 36.3 / 35.3 |
| 2560×1440 | dual + 2 secondaries | 41.9 / 40.9 |
| 2560×1440 | dual + 2 secondaries + SC + hover | 28.0 / 26.9 |

Two things in this table are unexplained. CPU grows only about 1.6x for 4x the pixels. Adding the sidechain and the hover readout lowers the CPU. Explain both before optimising. The development display is 1920×1080, so the 2560×1440 window is partly off-screen; the swap chain is still full size.

## Work, in this order
1. **Profile first.** Find where message-thread time goes per frame. Split it into at least these parts: `paint()` command recording, Direct2D `EndDraw` and flush, present and swap chain, parent or sibling repaints, and anything else above 5 % of the total. Useful tools:
   - JUCE 8's Direct2D metrics option (check the pinned 8.0.15 sources for `JUCE_DIRECT2D_METRICS` or its equivalent).
   - Windows Performance Recorder (`wpr`), which ships with Windows 11.
   - Manual timing around the peer's paint.

   Write the breakdown into the task log, with the method used.
2. **Cheap structural fixes.** Check each of these and log the effect of each:
   - Is `SpectrumView` (and every per-frame-repainted component) marked opaque, so that repainting it does not repaint the editor background beneath it?
   - Is each repaint confined to the data rect, with no whole-editor or parent repaint per vblank?
   - Is any path stroked and filled twice, or tessellated more than once per frame?
3. **Geometry.** Reduce the Direct2D tessellation load without a visible change. Options include fewer vertices per curve at high physical widths (keeping per-column peaks), simpler fill paths, and fewer separate geometries per frame. Screenshots before and after must look the same (the T-012 screenshot set is the reference).
4. **Dirty regions.** If it helps, repaint only the bands that changed. For example, split the plot into vertical strips and repaint, in each strip, only the y-range between the old and new curve envelopes plus the stroke width.
5. **Frame pacing (fallback).** Use this only if steps 2 to 4 cannot meet the gates. Cap the spectrum repaint rate at 30 fps when the plot's physical area exceeds a threshold that you choose and log. Below the threshold it stays at up to 60 fps. The meter bars keep their own rate. Report this clearly, because it is a visible trade-off that the architect will present to the user.
6. **Last resort.** If the gates are still missed, stop and return the numbers with OpenGL as an open question. Do not switch renderers.

## Acceptance criteria
Measured by plugin-validator in Release with the Direct2D renderer and the `spectrum_check` method from T-012 (net CPU, at least 12 s, new synthetic frames on every 60 Hz vblank, MeterPanel still a placeholder). Every case in this matrix is measured at both window sizes:
- single + RT max;
- dual + 2 secondaries;
- dual + 2 secondaries + SC + hover;
- single + all-time average + SC.

- [ ] A written breakdown of message-thread time per frame, before and after, in the task log.
- [ ] Spectrum-only message-thread CPU, the maximum over the case matrix: ≤ 18 % of one core at 1280×720 and ≤ 20 % at 2560×1440. The headroom up to the 25 % whole-editor budget is reserved for the meter panel (T-013).
- [ ] `SpectrumView` paint p95 is still ≤ 4 ms at 1280×720 and ≤ 8 ms at 2560×1440.
- [ ] No visual regression. The T-012 functional checks (92 harness checks) still pass, and the regenerated screenshot set matches the T-012 reference: identical curves, fills and colours. Anti-aliasing differences from the geometry changes are allowed only where the report shows them side by side.
- [ ] If frame pacing is used: the threshold and the effective frame rate per case are reported, and hover, freeze and click still respond within 1 vblank.
- [ ] The allocation rule (roadmap Decisions 2026-10-03) still holds: 0 allocations in the curve path and idle polls.
- [ ] Warning-clean at `/W4`. `docs/qa/T-029-unit.md` (allocation test and suite still green) and `docs/qa/T-029-validation.md` (CPU matrix, paint, functional re-run and pluginval at strictness 10) report PASS.
- [ ] A short "rendering rules" note in the task log for T-013, saying what the meter panel must do to stay cheap.

## QA split
- **unit-tester** writes `docs/qa/T-029-unit.md`: the allocation tests, the full suite in Debug and Release, and a review of any new repaint logic for correctness, for example that no stale band is left after a range change.
- **plugin-validator** writes `docs/qa/T-029-validation.md`: the CPU and paint matrix at both sizes, the T-012 functional re-run, screenshot comparison and pluginval at strictness 10. It extends `spectrum_check` for the fourth case if needed.
- Coders never edit `docs/qa/`. Only the testers write reports and verdicts.

## Log
- 2026-10-03 vst-architect: Created from T-012's failed message-thread CPU gate (validation rounds 1 and 2). That criterion moves here; it is not waived. The flat-fill fallback (spec §12) gave about 1 point, within noise, so the gradient is not the cost. Runs before T-013 so that the meter panel is built on the findings.
