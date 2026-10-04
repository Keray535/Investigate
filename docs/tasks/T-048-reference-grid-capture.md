# T-048: Spectrum engine: reference grid and spectrum capture
- Owner: dsp-engineer
- Milestone: M3c
- Status: TODO
- Depends on: T-047

## Goal
Reference slots ("Take") and the CSV export need a full-range, high-resolution copy of the current curves, independent of the zoom and the editor's point count. Add a shared reference grid and an on-request capture path from the analysis thread to the message thread.

## Inputs
- `docs/plans/m3c-feature-expansion.md`: design items [12] and [13], the Budgets (capture ≤ 20 ms; mapping ≤ 2 ms) and the Storage summary.
- `source/dsp/SpectrumEngine.*`, `SpectrumMapper.*`, `FrameExchange.*` and `SpectrumFrame.h`, and the T-035 log (freeze snapshot).
- Working rules as in T-046.

## Scope
1. **`source/dsp/ReferenceGrid.h`** (new, pure): 4096 points log-spaced from 1 Hz to 48 kHz, plus helpers:
   - point frequency;
   - nearest index;
   - log-frequency interpolation from an arbitrary (frequency, dB) list onto the grid, with no extrapolation;
   - a "no data" sentinel value (not NaN).

   The editor and the CSV code (gui-engineer) reuse it.
2. **`SpectrumCapture`** (new, fixed-size, trivially copyable). It holds:
   - the grid values for the primary, underlay, secondary A, secondary B, Sidechain 1 and Sidechain 2 curves, in dB, with the current smoothing and Align but **without slope**, and "no data" above the Nyquist frequency;
   - validity flags for each curve;
   - the settings in force: sample rate, block size, window, overlap, smoothing, slope, Align, the channel mode and the underlay;
   - a sequence number.
3. **`SpectrumEngine`:**
   - `requestCapture() noexcept`, callable from any thread, increments an atomic request counter;
   - the analysis thread serves a pending request on its next `processPending`, from the live bin-domain data, or from the freeze snapshot while frozen;
   - the result is published through a lock-free single-slot exchange (or a second `FrameExchange`-style buffer);
   - `readCapture (SpectrumCapture&) noexcept` reads it on the message thread and returns true once per new capture;
   - everything is preallocated in `prepare`.
4. The live frames are unaffected by a capture.

## Acceptance criteria
- [ ] With slope removed, capture values equal the live frame values at the same frequencies within 0.01 dB, for every curve, at Fit, with a smooth synthetic test signal.
- [ ] A capture requested while frozen returns the frozen data, even after the live signal changes.
- [ ] Above the Nyquist frequency, every grid point is "no data". No NaN or infinity appears anywhere.
- [ ] Request to available: p95 ≤ 20 ms at the default settings, measured from the request to `readCapture` succeeding. The mapping itself takes ≤ 2 ms on the analysis thread (`[.perf]`).
- [ ] 0 allocations in `processPending` with captures requested at 60 Hz (allocation hook). A capture never delays or drops a live frame publish.
- [ ] Every existing test passes. Warning-clean at `/W4`. `docs/qa/T-048-unit.md` reports PASS.

## Log
- 2026-10-03 vst-architect: Created from `docs/plans/m3c-feature-expansion.md`. The processor API (`requestSpectrumCapture`, `getSpectrumCapture`) is added in T-052.
