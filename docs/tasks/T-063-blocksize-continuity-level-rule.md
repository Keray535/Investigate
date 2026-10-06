# T-063: Engine continuity on block-size change, and the 1 dB level-range rule
- Owner: dsp-engineer
- Milestone: M3d
- Status: TODO
- Depends on: T-061, T-062

## Goal
Two small DSP fixes from the user's M3 review:
- **#15:** changing the Block size makes the display stutter. Keep the last frame on screen until the new size produces its first full frame.
- **#5:** the level range must keep its bottom at least 1 dB below its top (it is 12 dB today). The minimum level view span of the zoom must not exceed the home span.

## Inputs
- `docs/research/T-061-review-bug-diagnosis.md`, section #15.
- `docs/plans/m3-review-spectrum-slots.md` (M3d scope, Budgets, Risk 28) and `docs/plans/m3-user-review-findings.md` #5 and #15.
- `source/dsp/SpectrumEngine.*`, `SpectrumAnalyser.*`, `DisplayRange.h`, `SpectrumMapper.*`, and the T-035, T-036, T-039 and T-062 logs.
- The roadmap's "Constraints" under the parameter table (the effective-range formulas, amended on 2026-10-06).
- Working rules as in T-062.

## Scope
1. **Continuity (#15).**
   - When `fftSize`, the window or the overlap changes, the engine must not publish an empty, silent or partially filled frame.
   - Until the analyser has a full window at the new settings, the latest published frame stays current: the editor keeps showing it, and a view change re-maps the last valid bin-domain data as today.
   - The first frame at the new size is published as soon as one full window is available. Where T-061 recommends it and it is cheap, prime the new window from the FIFO history.
   - Avoid long work on the analysis thread that blocks publishing: do any reallocation outside the per-hop path, as T-061 recommends.
   - The averaging and secondary accumulators still reset on these changes (roadmap Decision of 2026-09-28).
2. **Level rule (#5).**
   - In `DisplayRange.h`, the effective rule becomes `dbLoEff = min (dbLow, dbHigh − 1)` (was −12).
   - The minimum level view span of the zoom becomes `min (6 dB, home span)`.
   - Update every user of the old constant, and the comments.
   - The parameter ranges themselves change in T-064.

## Acceptance criteria
- [ ] **Continuity.** For every change among 4096 ↔ 65536, 512 ↔ 32768, a window change and an overlap change, with a steady signal:
  - no published frame between the change and the first new frame has a curve more than 3 dB below the previous steady curve at any point;
  - the first new frame arrives within the duration of one window at the new size (with its hop) plus 50 ms (`[.perf]` for the timing part).
- [ ] **Level rule.**
  - With `dbHigh` = −30 and `dbLow` = −30, the effective bottom is −31 dB.
  - With `dbLow` = −10 and `dbHigh` = −20, the effective bottom is −21 dB.
  - Any pair already at least 1 dB apart is unchanged.
  - The zoom clamps never produce a level view wider than the home range or narrower than `min (6 dB, home span)`.
- [ ] **No regression.**
  - For ranges at least 12 dB wide, frames are bit-identical to T-062.
  - Every existing test passes, with any tolerance change justified in the log. This includes the M3b zoom tests whose 12 dB assumptions change.
- [ ] 0 allocations in `processPending` on frames without a settings change. Warning-clean at `/W4`. `docs/qa/T-063-unit.md` reports PASS.

## Log
- 2026-10-06 vst-architect: Created from the M3 review triage. The editor side of #5 (the chips and the grid on narrow ranges) is in T-066, and the parameter ranges are in T-064.
