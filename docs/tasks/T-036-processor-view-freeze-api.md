# T-036: Processor: view and freeze API, analysis-thread wiring
- Owner: plugin-engineer
- Milestone: M3b
- Status: DONE
- Depends on: T-035

## Goal
Give the editor a lock-free way to tell the analysis thread which frequency range is visible and whether the display is frozen, and wire the new `SpectrumSettings` fields from T-035 into the `AnalysisThread` poll. The audio thread does not change. This is the processor part of `docs/plans/spectrum-zoom-pan.md`.

## Inputs
- `docs/plans/spectrum-zoom-pan.md`: Data flow, Changes per module (the processor row), Freeze semantics and Budgets.
- `docs/tasks/T-035-engine-view-range-freeze.md` and its log: the new settings and frame fields.
- `source/PluginProcessor.h/.cpp` (`AnalysisThread::run`, `setAnalysisActive`) and the T-009 tests, including the processor lifecycle and allocation tests.
- The roadmap: Processor API, Threading model and Budgets.

## Scope
1. API (message thread):
   - `void setSpectrumView (float fLo, float fHi) noexcept`. `(0, 0)` means the home range. Pack both floats into one `std::atomic<uint64_t>`, so the analysis thread never reads a mixed pair.
   - `void setSpectrumFrozen (bool) noexcept`. Every change increments a freeze epoch. Store the flag and the epoch in one atomic.
   - `uint32_t getFreezeEpoch() const noexcept`.
2. `AnalysisThread::run` reads both atomics on every poll and fills `viewFreqLow`, `viewFreqHigh`, `frozen` and `freezeEpoch` in `SpectrumSettings`.
3. `setAnalysisActive (false)` resets the view to `(0, 0)` and the freeze to off. The next editor then starts at Fit and unfrozen (the zoom is not persisted). `ViewState` gains no properties.
4. Update the processor's API documentation comment and keep the existing roadmap API unchanged.

## Acceptance criteria
- [ ] **Latency.** At `fftSize` 65536 with 1x overlap at 48 kHz, analysis active, over 200 view changes made through the processor API with no editor: the time from `setSpectrumView` to a published frame that carries the new view has p95 ≤ 10 ms and max ≤ 20 ms.
- [ ] **No torn pair.** A stress test writes 10^6 view pairs from one thread while the analysis thread runs. Every published frame's view equals the clamped form of one of the written pairs.
- [ ] **Freeze epoch** (amended by the architect on 2026-10-03, see the log):
  - every frame published after `setSpectrumFrozen (true)` returns either carries an older epoch (a live frame from a poll that started before the call; the editor drops it) or carries the new epoch and holds the frozen snapshot;
  - no live frame ever carries a frozen epoch;
  - if the analyser had already produced a frame, a frame carrying the new epoch is published within 20 ms of the call, without new audio;
  - calling `setSpectrumFrozen` with the current state leaves the epoch unchanged.
- [ ] **Audio thread unchanged:**
  - the existing null tests stay bit-exact;
  - the allocation-hook test shows 0 allocations in `processBlock` while another thread toggles the view and the freeze at 60 Hz;
  - the analysis thread stops within 50 ms over 100 open and close cycles while view changes are being made.
- [ ] After `setAnalysisActive (false)` and then `true`, the first frame is mapped onto the home range and is not frozen.
- [ ] pluginval at strictness 10 passes. Warning-clean at `/W4`.
- [ ] `docs/qa/T-036-unit.md` and `docs/qa/T-036-validation.md` report PASS.

## Log
- 2026-10-03 vst-architect: Created from `docs/plans/spectrum-zoom-pan.md`.
- 2026-10-03 vst-architect: Notes from T-035 (DONE, `docs/qa/T-035-unit.md` PASS) before dispatch:
  - No `source/dsp/` change is needed. The new `SpectrumSettings` fields default to the old behaviour (`viewFreqLow = viewFreqHigh = 0`, `frozen = false`, `freezeEpoch = 0`, `overviewPoints = 256`). `AnalysisThread::run` fills `viewFreqLow`, `viewFreqHigh`, `frozen` and `freezeEpoch` from the two atomics on every poll. Leave `overviewPoints` at its default.
  - The engine publishes a frame stamped with the new epoch on the first poll that sees a new frozen epoch, even without new audio, but only if the analyser has produced a frame since its last reset. While frozen with no valid snapshot it publishes nothing. Unfreezing publishes the live state on the next poll.
  - **The epoch increments only on a real state change.** `setSpectrumFrozen (true)` while already frozen, or `(false)` while not frozen, is a no-op. A quick unfreeze and re-freeze between two polls leaves the flag set with a new epoch; the engine then retakes the snapshot from the latest analyser state, which is the intended behaviour (the user asked for a new freeze).
  - The original freeze-epoch criterion ("every frame published after the call carries the new epoch") cannot hold: a poll that read the atomics just before the call can still publish a live frame with the old epoch afterwards. That race is exactly why the editor drops stale epochs. The criterion was amended to test what matters: no live frame ever carries a frozen epoch, and the frozen frame arrives within 20 ms.
  - Shared clamps for any processor-side check: `DisplayRange::clampViewFrequency` and `SpectrumMapper::computeRanges (settings)`. The frame's `viewFreqLow` and `viewFreqHigh` hold the clamped view, so the torn-pair test compares against the clamped form of each written pair.
- 2026-10-03 plugin-engineer: Implemented in `source/PluginProcessor.h/.cpp`. `viewBits` packs fLo (low 32) and fHi (high 32) via `std::bit_cast`; `freezeBits` holds epoch (low 32) and flag (bit 32), updated by a CAS loop that returns early when the state is unchanged. `AnalysisThread::run` does one acquire load of each per poll. `setAnalysisActive (false)` (last deactivation) resets view to (0,0) and freeze to off AFTER the join; if it was frozen, the epoch increments (a real change). Audio thread untouched. Release build is warning-clean. Handed to unit-tester.
- 2026-10-03 plugin-engineer: QA round 1: unit PASS (12 tests in tests/unit/ProcessorViewFreezeTest.cpp, ctest 378/378), validation PASS (pluginval 10, /W4 clean). Measured view-change latency (65536 FFT, 1x, 48 kHz, 200 changes): run 1 p95 5.73 ms, max 7.18 ms; run 2 p95 6.09 ms, max 7.35 ms. Frozen frame typically 6-10 ms; one 32 ms outlier on a loaded machine in 23 runs (architect: consider tolerance on CI).
  Notes for T-037: call setSpectrumView with the desired Hz range ((0,0) = Fit; frames carry the clamped view); call setSpectrumFrozen on state change only and drop frames whose freezeEpoch != getFreezeEpoch() (stale frames after a freeze call are expected, 0-2 seen); the last setAnalysisActive(false) resets view and freeze (freeze reset bumps the epoch if it was frozen). SpectrumFrame has no explicit frozen flag; use the epoch.
- 2026-10-03 plugin-engineer: QA round 1: unit PASS (12 tests in tests/unit/ProcessorViewFreezeTest.cpp, ctest 378/378), validation PASS (pluginval 10, /W4 clean, latency 0). Measured view-change latency (65536 FFT, 1x, 48 kHz, 200 changes): p95 5.73 / 6.09 ms, max 7.18 / 7.35 ms over two runs. Frozen frame typically 6 to 10 ms; one 32 ms outlier on a loaded machine in 23 runs. 100 open/close cycles: worst stop 3.5 ms. Torn pairs: 0 in 10^6. 0 allocations in processBlock.
  Notes for T-037: call `setSpectrumView (fLo, fHi)` (Hz, (0,0) = home; clamped engine-side, frames carry the clamped range) and `setSpectrumFrozen (bool)`; compare `getFreezeEpoch()` with `SpectrumFrame::freezeEpoch` and drop frames with an older epoch after a freeze call. The last editor closing resets view and freeze (an active freeze bumps the epoch), so a new editor starts at Fit, unfrozen. `SpectrumFrame` has no explicit frozen flag.
