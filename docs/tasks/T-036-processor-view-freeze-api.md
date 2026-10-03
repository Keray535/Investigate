# T-036: Processor: view and freeze API, analysis-thread wiring
- Owner: plugin-engineer
- Milestone: M3b
- Status: TODO
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
