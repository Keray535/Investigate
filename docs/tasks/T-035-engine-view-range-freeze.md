# T-035: Spectrum engine: view-range mapping, re-publish and freeze snapshot
- Owner: dsp-engineer
- Milestone: M3b
- Status: TODO
- Depends on: T-005, T-009

## Goal
Let the spectrum pipeline map its display points onto a zoomed frequency range at full detail, publish a re-mapped frame immediately when the view changes (without waiting for a new FFT hop), and keep a bin-domain snapshot while frozen, so zooming a frozen spectrum also shows detail. The audio thread and `processBlock` do not change. This is the DSP part of `docs/plans/spectrum-zoom-pan.md`.

## Inputs
- `docs/plans/spectrum-zoom-pan.md`: View model, Data flow, Changes per module (the DSP rows), Freeze semantics and Budgets.
- `source/dsp/DisplayRange.h`, `SpectrumSettings.h`, `SpectrumMapper.*`, `SpectrumEngine.*`, `SpectrumFrame.h`, `SpectrumAnalyser.h`, and their unit tests from T-004 and T-005.
- `docs/tasks/T-005-spectrum-mapper-engine.md`: the existing mapping and smoothing design and its criteria.
- The roadmap's Real-time safety rules (`CLAUDE.md`). The analysis thread is not the audio thread, but the freeze and re-publish paths must not allocate (roadmap Architecture: the analysis thread may allocate only on an `fftSize`, window, sample-rate or channel-count change).

## Scope
1. **`DisplayRange.h`:**
   - add `minViewSpanOctaves = 1.0 / 6.0` and `minViewSpanDb = 6.0`;
   - add pure `clampViewFrequency (lo, hi, homeLo, homeHi)` and `clampViewDb (lo, hi, homeLo, homeHi)`. They return a view inside the home range with at least the minimum span, keeping the centre where possible. A `(0, 0)` input returns the home range. They are idempotent.
2. **`SpectrumSettings`:** add `viewFreqLow` and `viewFreqHigh` (0 means the home range), `frozen`, `freezeEpoch` (`uint32_t`) and `overviewPoints` (256).
3. **`SpectrumMapper`:**
   - Map onto the clamped view.
   - Compute smoothing over a range padded by half the smoothing window on each side, and output only the visible points.
   - Preallocate the scratch in `prepare` for the worst case: the minimum span with 1/3-octave smoothing.
   - Add an overview mapping: primary A over the effective home range, at `overviewPoints` points, using the same smoothing and slope.
4. **`SpectrumEngine`:**
   - **Re-publish:** when the view range or the effective home range differs from the last published frame and no new hop was produced, re-map the latest bin-domain spectra and publish. A re-publish must not change any analyser state (averages, accumulators, frame counter).
   - **Freeze:** on the first `processPending` with `frozen == true` for a new epoch, copy the bin-domain arrays into a snapshot, together with `binHz`, `numBins`, the curve flags, the smoothing and the slope. Size the arrays in `prepare` for 65536 / 2 + 1 bins × 5 curves. While frozen:
     - keep analysing internally, so click-to-reset still works;
     - publish no live frames;
     - publish snapshot re-maps when the view or the home range changes.

     The current smoothing and slope are ignored for the snapshot (ui-spec §5.7); the home range is honoured.
   - **Unfreeze:** publish the live state on the next call, without waiting for a hop.
5. **`SpectrumFrame`:** add `viewFreqLow` and `viewFreqHigh` (as mapped), `overviewFreqHz[256]` and `overviewDb[256]`, and `freezeEpoch`. Update the size comment.

## Acceptance criteria
- [ ] `DisplayRange` view clamps:
  - unit tests over random inputs show the result always lies inside the home range;
  - the span is at least the minimum (or equal to the home span);
  - `(0, 0)` gives the home range;
  - clamping twice gives the same result as clamping once.
- [ ] **No regression when not zoomed.** With `viewFreqLow = viewFreqHigh = 0` and `frozen = false`, the published curves are bit-identical to the current implementation for the same input, in every channel mode and smoothing mode. Every existing T-004 and T-005 test passes unchanged.
- [ ] **View mapping:**
  - `freqHz[0]` and `freqHz[numPoints − 1]` equal the clamped view ends within 1e-4 relative error;
  - the points are strictly increasing;
  - the frame's `viewFreqLow` and `viewFreqHigh` equal the clamped view.
- [ ] **Edge smoothing.** For every smoothing mode, and for views from 1/6 octave to the full home range, the first and last 5 % of the visible points match a reference mapped over a view widened by 2× on each side at the same point density, within 0.05 dB. Test signals: pink noise plus three tones.
- [ ] **Re-publish:**
  - a view change with no new samples publishes a frame within the next `processPending` call;
  - the analyser state is bit-identical before and after the re-publish;
  - a call with no view change and no new hop publishes nothing.
- [ ] **Freeze:**
  - After freezing, 10 s of new audio changes no published curve value.
  - View changes while frozen re-map the snapshot.
  - Smoothing and slope changes while frozen do not alter the frozen frames; home-range changes do.
  - `fftSize`, window and `channelMode` changes while frozen do not corrupt the snapshot.
  - Unfreezing publishes the live state on the next call.
  - Every frame published while frozen carries the frozen epoch.
- [ ] **Allocation:** an allocation-hook test shows 0 allocations across freeze, unfreeze, view changes and re-publishes, with no `fftSize`, window or layout change.
- [ ] **Overview:** 256 points spanning the effective home range. It matches the full-range primary curve at the same frequencies within 0.1 dB when the point density is comparable.
- [ ] **Timing (`[.perf]`):** one re-map with every curve plus the overview takes ≤ 1.0 ms as a median over 100 runs, worst case: dual mode, both secondary curves, sidechain, 1/3-octave smoothing, minimum span, `fftSize` 65536.
- [ ] Warning-clean at `/W4`. `docs/qa/T-035-unit.md` reports PASS.

## Log
- 2026-10-03 vst-architect: Created from `docs/plans/spectrum-zoom-pan.md`. Starts only after T-032's configure run has finished (one C++ task at a time). The mapper does not use the dB range, so level zoom is editor-only and is not part of this task.
