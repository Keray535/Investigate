# T-039: Spectrum engine: pan margin and bin spacing in the frame
- Owner: dsp-engineer
- Milestone: M3b
- Status: TODO
- Depends on: T-035 (and T-036 for build order only: one C++ task at a time)

## Goal
Make each zoomed frame carry curve points for a margin beyond the visible view, so that a fast pan or a zoom-out does not show an empty strip at the leading edge while the editor waits for the re-mapped frame. Also make each frame carry the bin spacing of the data it was mapped from, so the zoom panel's bin-spacing row is correct while frozen and just after a block-size change. These close open questions 5 and 6 of `docs/design/ui-spec.md` §15, which the architect accepted on 2026-10-03.

## Inputs
- `docs/design/ui-spec.md` §15 questions 5 and 6, §5.9 (the zoom panel's bin-spacing row) and §12.1 ("Curves until the re-mapped frame arrives").
- `docs/plans/spectrum-zoom-pan.md`: View model, Data flow and Budgets.
- `docs/tasks/T-035-engine-view-range-freeze.md` and its log (the padded grid, the clipped smoothing padding, the snapshot, the publish key), and `docs/qa/T-035-unit.md`.
- `source/dsp/SpectrumMapper.*`, `SpectrumEngine.*`, `SpectrumFrame.h`, `SpectrumSettings.h`, `DisplayRange.h`, and the T-035 tests in `tests/unit/` (`SpectrumZoomMapperTest.cpp`, `SpectrumZoomEngineTest.cpp`, `SpectrumZoomTestUtils.h`).

## Scope
1. **Pan margin.** For a clamped view `[vLo, vHi]` with `N` visible points and the log step `step = log2 (vHi / vLo) / (N − 1)`:
   - extend the published grid by `m = lround (0.25 × (N − 1))` points on each side, so the margin covers 25 % of the visible span in octaves per side;
   - clip each side's margin to the effective home range (fewer points on that side, never a point outside the home range);
   - **extend the same grid; do not re-space it.** Compute every point as `log2 (vLo) + step × k` for `k` from `−mLo` to `N − 1 + mHi`, with exactly the T-035 arithmetic for the visible `k`, so the visible points stay bit-identical to the T-035 output;
   - the smoothing padding from T-035 is applied beyond the margin, still clipped to the home range;
   - snapshot re-maps (while frozen) carry the margin too.
   At Fit both margins are 0, so the output is bit-identical to today.
2. **Frame fields.**
   - `numPoints` becomes the total published point count, margin included. Add `firstVisiblePoint` and `numVisiblePoints` so the editor can find the visible part without searching. `viewFreqLow` and `viewFreqHigh` keep their meaning (the clamped view).
   - Raise the point capacity to 3072 (`2048 + 2 × 512`), so `N = 2048` with both margins fits. Update the size comment and add a `static_assert` that ties the capacity to the maximum `N`.
   - Add `double binHz`: the bin spacing of the data the frame was mapped from (sample rate / block size). Frozen frames carry the snapshot's value, so a block-size change while frozen does not change it.
3. **Overview:** unchanged.
4. Keep the existing processor and editor building and passing unchanged. The editor never sets a view before T-037, so it never receives margin points. Do not edit `source/PluginProcessor.*` or `source/gui/`.

## Acceptance criteria
- [ ] **No regression at Fit.** With no view set, the published frames are bit-identical to the T-035 output in every channel mode and smoothing mode, with `firstVisiblePoint = 0` and `numVisiblePoints = numPoints`. Every existing test passes; tests change only where they assert the new fields or the new capacity.
- [ ] **Visible points unchanged.** For at least 20 zoomed views (spans from 1/6 octave to the home range, at the low edge, the high edge and inside), the visible points (`freqHz` and every active curve) are bit-identical to the T-035 output for the same view.
- [ ] **Margin geometry.**
  - The published points span `[max (homeLo, vLo / 2^(0.25 S)), min (homeHi, vHi × 2^(0.25 S))]`, where S is the visible span in octaves, within one grid step.
  - The margin step equals the visible step within 1e-9 relative.
  - No point lies outside the effective home range, and the points are strictly increasing.
  - At most 3072 points are published.
- [ ] **Margin values.** The margin points match a frame mapped for a view shifted so that they are visible, within 0.05 dB (aligned grid and signals several bins wide, as in the T-035 edge-smoothing test).
- [ ] **binHz.** Live frames carry sample rate / block size. A frozen frame keeps the freeze-time value after a block-size change while frozen. Unfreezing publishes the new value.
- [ ] **Allocation:** 0 allocations across freeze, unfreeze, view changes and re-publishes, as in the T-035 test.
- [ ] **Timing (`[.perf]`):** the T-035 worst case with the margin included (dual mode, both secondary curves, sidechain, 1/3-octave smoothing, minimum span, `fftSize` 65536, plus the overview) takes ≤ 1.0 ms per re-map, as a median over 100 runs.
- [ ] Warning-clean at `/W4`. `docs/qa/T-039-unit.md` reports PASS.

## Log
- 2026-10-03 vst-architect: Created from the T-034 open questions 5 (curve margin while panning) and 6 (bin spacing in the frame), both accepted as recommended by ui-ux-designer. A separate task, because T-035 is DONE and passed QA. Runs after T-036 and before T-037. The margin is 25 % per side, because one zoom-out notch (span × 1.25, anchored at the pointer) exposes at most 25 % of the old span on one side, and it covers a pan of up to a quarter of the view per editor update.
