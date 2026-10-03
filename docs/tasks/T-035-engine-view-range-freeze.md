# T-035: Spectrum engine: view-range mapping, re-publish and freeze snapshot
- Owner: dsp-engineer
- Milestone: M3b
- Status: DONE
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
- [x] `DisplayRange` view clamps:
  - unit tests over random inputs show the result always lies inside the home range;
  - the span is at least the minimum (or equal to the home span);
  - `(0, 0)` gives the home range;
  - clamping twice gives the same result as clamping once.
- [x] **No regression when not zoomed.** With `viewFreqLow = viewFreqHigh = 0` and `frozen = false`, the published curves are bit-identical to the current implementation for the same input, in every channel mode and smoothing mode. Every existing T-004 and T-005 test passes unchanged.
- [x] **View mapping:**
  - `freqHz[0]` and `freqHz[numPoints − 1]` equal the clamped view ends within 1e-4 relative error;
  - the points are strictly increasing;
  - the frame's `viewFreqLow` and `viewFreqHigh` equal the clamped view.
- [x] **Edge smoothing.** The smoothing padding is clipped to the effective home range (architect clarification of 2026-10-03, see the log). For every smoothing mode, and for views from 1/6 octave to the full home range, the first and last 5 % of the visible points match a reference mapped over a view widened by 2× on each side (in octaves) and then clamped to the effective home range, at the same point density, within 0.05 dB. At an edge that coincides with the home range edge, the smoothing window is truncated exactly as in the current implementation. Test signals: pink noise plus three tones.
- [x] **Re-publish:**
  - a view change with no new samples publishes a frame within the next `processPending` call;
  - the analyser state is bit-identical before and after the re-publish;
  - a call with no view change and no new hop publishes nothing.
- [x] **Freeze:**
  - After freezing, 10 s of new audio changes no published curve value.
  - View changes while frozen re-map the snapshot.
  - Smoothing and slope changes while frozen do not alter the frozen frames; home-range changes do.
  - `fftSize`, window and `channelMode` changes while frozen do not corrupt the snapshot.
  - Unfreezing publishes the live state on the next call.
  - Every frame published while frozen carries the frozen epoch.
- [x] **Allocation:** an allocation-hook test shows 0 allocations across freeze, unfreeze, view changes and re-publishes, with no `fftSize`, window or layout change.
- [x] **Overview:** 256 points spanning the effective home range. It matches the full-range primary curve at the same frequencies within 0.1 dB when the point density is comparable.
- [x] **Timing (`[.perf]`):** one re-map with every curve plus the overview takes ≤ 1.0 ms as a median over 100 runs, worst case: dual mode, both secondary curves, sidechain, 1/3-octave smoothing, minimum span, `fftSize` 65536.
- [x] Warning-clean at `/W4`. `docs/qa/T-035-unit.md` reports PASS.

## Log
- 2026-10-03 vst-architect: Created from `docs/plans/spectrum-zoom-pan.md`. Starts only after T-032's configure run has finished (one C++ task at a time). The mapper does not use the dB range, so level zoom is editor-only and is not part of this task.
- 2026-10-03 vst-architect: Clarification before dispatch. The current mapper truncates the smoothing window at the ends of the mapped range (`SpectrumMapper::mapCurve`, the boxcar `lo`/`hi` clamps). If the padding ran past the effective home range, the curves at Fit would change at both ends, which would break the bit-identical no-regression criterion. So the padding is clipped to the effective home range: at Fit there is no padding and the output is bit-identical to today; when zoomed, each visible edge is padded by up to half a smoothing window, but never past the home range edge. The result is that a zoomed curve equals the Fit curve at the same frequencies (at a higher point density), and only the home range edges keep today's truncated window. The edge-smoothing criterion was amended to match. T-032 finished its configure run, so this task may start.
- 2026-10-03 dsp-engineer: Implemented (status IN_REVIEW, handed to unit-tester).
  - **Algorithm and expected behaviour (written before coding).** The mapper spreads `numPoints` log-spaced points over the clamped view `[vLo, vHi]` (`DisplayRange::clampViewFrequency` against the effective home range). Smoothing needs the neighbours of the edge points, so the grid is extended by `half = lround ((smoothOct / 2) / step)` points on each side, clipped to the effective home range (`pad = min (half, floor ((logV - logHome) / step))`); the boxcar runs over the padded array and only the visible points go through dB and slope. At Fit `pad == 0`, so the arithmetic is the old arithmetic (exponent `logLo + step * k`, edges, prefix sum). Worst case total = 3 x numPoints - 2 (view 1/6 octave, 1/3-octave smoothing), so scratch is `3 * maxPoints`. The overview is the same pipeline on a second grid (home range, `overviewPoints`, no padding).
  - **DisplayRange.h.** `minViewSpanOctaves`, `minViewSpanDb`, `struct Range { low, high }`, `clampViewFrequency` (log domain), `clampViewDb` (linear). A legal input passes through untouched; otherwise the span is clamped to `[min (minSpan, homeSpan), homeSpan]`, the centre is kept and the window is slid into the home range; edges that reach the home range are returned as exactly the home values. Idempotent bit for bit (the legal-input test uses a 1e-9 relative tolerance on the minimum span). `(0, 0)`, non-positive or non-finite input returns the home range; a reversed pair is swapped. Scratch check over 2e6 random inputs per function: 0 range or span violations, 0 non-idempotent results.
  - **Mapper.** API kept: `computeFrequencies` (now fills the visible view points and caches the padded grid) and `mapCurve` (same signature; it ignores the `freqHz` argument and uses the cached padded grid; it re-pads if `settings.smoothing` differs from the one used in `computeFrequencies`). New: `static Ranges computeRanges (settings)` (home and clamped view), `mapOverview (binPower, numBins, binHz, settings, outFreqHz, outDb)` which returns the point count. Grids are cached by (view, home, numPoints, smoothing), so a live frame with an unchanged view rebuilds nothing.
  - **Engine.** `published` remembers the (clamped view, home range, frozen, epoch) of the last published frame. Live: a hop always publishes; with no hop it re-maps and publishes only when that key differs and the analyser has produced a frame since its last full reset (`getFramesProduced() > 0`), otherwise there is nothing valid to re-map. Re-publish only reads the analyser. Freeze: `takeSnapshot` runs on the first call for a new epoch **before the drain**, so it holds exactly the state of the last published frame; it needs `getFramesProduced() > 0`, otherwise the snapshot is invalid and nothing is published while frozen. Decision: **the freeze entry publishes the snapshot as a frame carrying the new epoch** (the key differs by epoch), so the editor, which drops old-epoch frames, shows the frozen state at once; `processPending` returns true for that call. While frozen the analyser keeps running (click-to-reset still works) and the snapshot is re-mapped with the snapshot's smoothing and slope, the live home range and the snapshot's `binHz`. A new epoch while still frozen retakes the snapshot. Unfreeze: the key differs (frozen flag and epoch), so the live state is published on the next call if the analyser has a valid frame. Snapshot: one preallocated `5 x 32769` float vector (0.66 MB) plus the metadata; no allocation on freeze, unfreeze, view change or re-publish.
  - **Frame.** New `viewFreqLow/High` (double, the clamped view), `overviewNumPoints` (int, not in the Scope but needed when `overviewPoints < 256`), `overviewFreqHz[256]`, `overviewDb[256]`, `freezeEpoch`. `frame.sampleRate` is the current settings sample rate, also for snapshot frames.
  - **Verification so far (scratch programs outside the repo, Release /O2).** Against a copy of the HEAD mapper: 0 mismatches in 3240 combinations (fft 512/4096/65536, 44.1/48/192 kHz, every smoothing mode, 1 to 2048 points, assorted ranges incl. Nyquist-limited) in the `memcmp` of `freqHz` and `primaryDb`. Re-map worst case (dual, 5 curves incl. sidechain, 1/3-octave, 1/6-octave view moving every run so the grid is rebuilt, fft 65536, 2048 points, plus overview): median 0.245 ms, max 0.39 ms (budget 1.0 ms). The existing T-004/T-005/display-range tests pass (Debug).
  - **Note for the tester (edge-smoothing reference).** The bin to point mapping is discontinuous where a point edge crosses a half-bin boundary (peak aggregation uses `lround (edge / binHz)`). A reference view whose ends differ from the real grid by float rounding (1e-7 relative) can flip that rounding and move a single-bin spike by more than 0.05 dB (seen with a lone +0.5 spike over a 1e-4 background and a 1/6-octave window). Build the reference on the same point grid (same step, aligned points), and use signals whose tones are Hann-windowed (several bins wide) as the criterion says.
  - **For T-036.** No source change is needed to keep the build working: new settings fields default to the old behaviour (`viewFreqLow = viewFreqHigh = 0`, `frozen = false`, `freezeEpoch = 0`, `overviewPoints = 256`). T-036 fills `viewFreqLow/High` (floats from the packed atomic), `frozen` and `freezeEpoch` in `AnalysisThread::run`. `SpectrumMapper::computeRanges (settings)` and `DisplayRange::clampViewFrequency/clampViewDb` are the shared clamps for the editor. The first processed call with `frozen = true` for a new epoch publishes a frame stamped with that epoch even without audio (if the analyser had produced a frame). Frames always carry `freezeEpoch` (live frames the current `settings.freezeEpoch`).
- 2026-10-03 dsp-engineer: Unit QA round 1 of 3: PASS, 0 defects (`docs/qa/T-035-unit.md`; ctest 367/367 in Debug and Release). Measured re-map (Release, `[.perf]`, worst case, one `processPending` call): median 0.303 to 0.312 ms over three runs, max about 0.47 ms, against the 1.0 ms budget. Status DONE; no validation report is needed (the plugin binary's behaviour does not change). Open points for T-036 and T-037 from the tester: (1) a change of `numPoints` or `smoothing` alone does not re-publish; it shows on the next hop. (2) If a freeze is requested when the analyser holds no valid frame, nothing is published while frozen, and the editor must cope with a frozen state that has no frame. (3) A new epoch while still frozen retakes the snapshot from the current internal analyser state, not from the frame last displayed.
