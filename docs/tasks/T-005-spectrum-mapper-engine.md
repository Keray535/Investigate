# T-005: SpectrumMapper, frame exchange and SpectrumEngine
- Owner: dsp-engineer
- Milestone: M1
- Status: DONE
- Depends on: T-001, T-004

## Goal
Complete the spectrum pipeline in `source/dsp/`:
- map the bin-domain power spectra from T-004 onto log-spaced display points, then apply fractional-octave smoothing, slope and dB conversion;
- define the `SpectrumFrame` struct;
- publish frames through a lock-free triple buffer;
- wrap everything in a `SpectrumEngine` facade that the processor (T-009) drives.

## Inputs
- `docs/roadmap.md`: the Architecture sections (signal flow, the modules table, "SpectrumFrame contents", the parameter table and constraints, and the threading model) and Budgets.
- `docs/research/T-002-dsp-references.md`: sections 2 (bin-to-point mapping, smoothing, slope) and 7 (triple buffer).
- `docs/research/T-001-span-feature-parity.md`: sections 2 and 7, for SPAN's smoothing and slope behaviour.
- The T-004 classes in `source/dsp/`, and `docs/qa/T-004-unit.md`.

## Design requirements
- **`SpectrumSettings`.** A plain struct holding every spectrum parameter from the roadmap table, as plain values, plus the sample rate.
- **`SpectrumMapper`.**
  - Up to 2048 display points (a fixed maximum, so no allocation per frame), log-spaced from `fLoEff` to `fHiEff`.
  - Compute the effective range with exactly the roadmap formulas, `fHiEff = min(freqHigh, sampleRate / 2)` and `fLoEff = min(freqLow, fHiEff / 2)`, without modifying the settings. Put these formulas, plus `dbLoEff = min(dbLow, dbHigh − 12)`, in one small header, for example `source/dsp/DisplayRange.h`, so the editor (T-012) uses the identical code.
  - Where points are denser than bins, interpolate using the T-002 recommendation.
  - Where bins are denser than points, use peak-preserving aggregation, unless T-002 recommends otherwise with a reason.
  - Apply fractional-octave smoothing (Off, 1/48, 1/24, 1/12, 1/6, 1/3 octave) in the point domain.
  - Apply slope in dB/oct around a 1 kHz pivot.
  - Convert to dB with a −200 dB floor. Do not clamp to the display range; the editor does that.
- **`SpectrumFrame`.** A fixed-size struct as described in the roadmap: point count, frequency per point, primary, primary B (dual), secondary (for primary), secondary B (for primary B, filled in the dual modes), sidechain, the flags `isDual`, `hasSecondary` and `hasSidechain`, the sample rate, and a sequence number. Document every field.
- **`FrameExchange`.** A lock-free triple buffer with a single producer (the analysis thread) and a single consumer (the message thread). Every buffer is allocated at construction or in `prepare`. `read(SpectrumFrame&)` returns whether the frame is newer than the last one read.
- **`SpectrumEngine`.**
  - Producer API: `pushMain(const float* const* channels, int numChannels, int numSamples)` and `pushSidechain(...)`. Both are real-time safe, noexcept and allocation-free.
  - Consumer API, analysis thread: `processPending(const SpectrumSettings&)` drains the FIFOs, runs the analysers, and publishes at most one frame per call (the newest). It returns `true` if it published.
  - Message thread: `readLatest(SpectrumFrame&)`.
  - Control: `resetSecondary()`, `prepare(sampleRate, maxBlockSize, numMainChannels, numSidechainChannels)`, and `getOverflowCount()`.

## Acceptance criteria
- [ ] A sine at −6 dBFS at any frequency from 20 Hz to 20 kHz shows a peak point value between −6.0 and −7.5 dB with Hann, at fftSize 4096 and 2048 display points, with slope 0 and smoothing off. The upper bound allows for Hann scalloping.
- [ ] Low-frequency interpolation never produces a point higher than the maximum of its two neighbouring bins plus 0.1 dB.
- [ ] Slope S = +4.5 dB/oct on flat (white) noise, averaged: the point at 1 kHz is unchanged within ±0.01 dB, the point at 2 kHz is +4.5 ±0.01 dB, and the point at 250 Hz is −9.0 ±0.01 dB, all relative to slope 0.
- [ ] Smoothing at 1/3 octave applied to a single-bin impulse spectrum spreads it over a width of 1/3 octave ±10 %. With smoothing Off, the values are identical to the unsmoothed input.
- [ ] Changing `freqLow`, `freqHigh` or the number of points between frames does not reset the averaging or secondary state.
- [ ] `FrameExchange`: a 10 s stress test at 1 kHz producer rate and 60 Hz consumer rate shows no torn frames (all fields carry the same sequence stamp), no allocation after `prepare`, and a sequence number that never decreases.
- [ ] `SpectrumEngine`: a producer at 48 kHz in 128-sample blocks plus a consumer polling every 5 ms, for 30 s, gives 0 overflows at default settings, and the frames reflect the injected signal.
- [ ] Mapping, smoothing, slope and dB conversion for 2048 points and 3 curves take ≤ 0.3 ms per frame in Release.
- [ ] No NaN or Inf in any frame for silence, DC, full-scale noise or denormal-range input.
- [ ] Warning-clean at `/W4`. `docs/qa/T-005-unit.md` reports PASS.

## Log
- 2026-09-28 dsp-engineer: Read CLAUDE.md, this task file, the roadmap (Architecture, "SpectrumFrame contents", parameter table/constraints, Budgets, Decisions), T-002 §2/§7, T-001 §2/§9, ui-spec §5.1/§5.4, the T-004 `source/dsp/` classes, and `docs/qa/T-004-unit.md`. Set status to IN_PROGRESS before coding.

  **Design summary (algorithm, parameters, expected behaviour):**
  - `DisplayRange.h`: the four roadmap formulas (`fHiEff = min(freqHigh, sampleRate/2)`, `fLoEff = min(freqLow, fHiEff/2)`, `dbHiEff = dbHigh`, `dbLoEff = min(dbLow, dbHigh-12)`) as free functions on plain `double`s, no JUCE dependency, so T-012 can include it from `source/gui/` too.
  - `SpectrumSettings`: every spectrum parameter from the roadmap table (channelMode, fftSize, overlap, window, avgTimeMs, smoothing, slope, secondary, freqLow/High, dbLow/High, showSidechain) plus sampleRate, numMainChannels/numSidechainChannels (bus-layout-derived, not APVTS) and numPoints (not an APVTS parameter; the display point count for this frame). `toAnalyserSettings()` is the single place that narrows it to `SpectrumAnalyser::Settings`, so the two pipelines can't disagree about fftSize/window/channelMode/sampleRate/channel counts.
  - `SpectrumMapper`: per point, computes a log-frequency edge (half a point-step wide) and interpolates (power-domain linear interpolation between the two neighbouring bins, a convex combination so it can never exceed max(bin0,bin1)) when the edge span is narrower than one bin, or peak-aggregates the enclosed bins otherwise (T-002 §2). Fractional-octave smoothing is a power-domain boxcar over a fixed number of neighbouring points (constant in index space since points are log-spaced), computed via a prefix sum so cost is O(numPoints) regardless of window width. Slope is added in the dB domain after smoothing (`+slope * log2(f/1000)`), and the -200 dB floor is applied last (post-slope), matching T-002 §2's "post-smoothing, pre-final-dB-clamp" ordering - this is what makes the slope acceptance criterion's dB deltas exact. Expected: -6 dBFS Hann sine reads between -6.0 and -7.5 dB at its peak point; interpolated low-frequency points never exceed max(neighbour bins)+0.1 dB; 1/3-octave smoothing of an impulse spreads it to within +-10% of 1/3 octave; slope +4.5 dB/oct on flat noise gives exactly +4.5 dB at 2 kHz and -9.0 dB at 250 Hz relative to slope 0 (both to +-0.01 dB, since the additive dB-domain implementation is exact arithmetic, not an approximation).
  - `SpectrumFrame`: fixed `std::array<float,2048>` members (no heap pointers) so the struct is trivially copyable for the triple buffer; ~49 KB per frame.
  - `FrameExchange`: the classic non-blocking triple buffer (single atomic `state` packing a 2-bit slot index and a dirty bit) - one atomic exchange to publish, one bounded CAS loop to read. Never blocks either side; the consumer may skip frames.
  - `SpectrumEngine`: owns two `SampleFifo`s (capacity 2^17 frames each, roadmap risk 5), the `SpectrumAnalyser`, the `SpectrumMapper` and the `FrameExchange`. `pushMain`/`pushSidechain` forward directly to the FIFOs (already real-time-safe by T-004's contract). `processPending()` drains both FIFOs in bounded 8192-frame chunks (zero-padding the sidechain scratch on a rare FIFO desync), runs `SpectrumAnalyser::pushBlock` per chunk, and - only if at least one new analyser frame was produced - maps the current bin-domain state through `SpectrumMapper` into up to 5 curves (primary, primary B, up to two secondary curves, sidechain) and publishes once. `resetSecondary()` and `getOverflowCount()` forward to the analyser and the two FIFOs respectively.
  - Changing `freqLow`/`freqHigh`/`numPoints`/`smoothing`/`slope` never reaches `SpectrumAnalyser::setSettings()` in a way that triggers its reset (they aren't part of `toAnalyserSettings()`'s output at all), so the acceptance criterion "these changes must not reset averaging or secondary state" holds by construction, not by a special-cased check.

  **Build:** `cmake --preset windows-msvc` (glob picked up the 8 new files automatically, no CMake edits). `cmake --build --preset debug|release --target Investigate` and `--target InvestigateUnitTests`: both configurations compile with 0 warnings at `/W4`. Re-ran the full existing T-004 suite (Release, seed 12345) to confirm no regression: 34 test cases, 142674 assertions, 0 failures - unchanged from `docs/qa/T-004-unit.md`.

  **Files added:** `source/dsp/DisplayRange.h`, `source/dsp/SpectrumSettings.h`, `source/dsp/SpectrumFrame.h`, `source/dsp/SpectrumMapper.h/.cpp`, `source/dsp/FrameExchange.h/.cpp`, `source/dsp/SpectrumEngine.h/.cpp`. No T-004 files modified.

  Handing over to unit-tester (round 1). Status set to IN_REVIEW.
- 2026-09-28 dsp-engineer: unit-tester round 1 report (`docs/qa/T-005-unit.md`) is **PASS, 0 defects**. All 10 acceptance criteria verified, including AC5 (freqLow/freqHigh/numPoints changes don't reset averaging/secondary state) tested through `SpectrumEngine::processPending()` itself rather than by reading the code, AC6 (FrameExchange) with real concurrent producer/consumer threads and 200,000 publishes with per-field torn-frame detection, and AC8 (performance) measuring 0.147 ms against the 0.3 ms budget. Full suite after the addition: 53 Catch2 test cases / 150,817 assertions, 0 failures (Release), no regression in the T-004 suite. No source/dsp/ changes were requested or made. Status set to DONE; only one tester round was needed.

  **Known limitations / design notes for downstream tasks:**
  - `SpectrumEngine::prepare()` fixes the two `SampleFifo` channel counts; if the host's bus layout later changes `numMainChannels`/`numSidechainChannels`, T-009 must call `prepare()` again (off the audio thread, analysis thread stopped) rather than relying on `SpectrumSettings` alone to pick up the new counts.
  - `SpectrumEngine::processPending()` zero-pads the sidechain drain buffer on a main/sidechain FIFO desync (should not occur in normal T-009 usage, where both are pushed with matching sample counts every `processBlock` call).
  - `getOverflowCount()` returns the *sum* of the main and sidechain FIFO overflow counts, not two separate counters, per the task's single-method design requirement.
  - `showSidechain` lives in `SpectrumSettings` (per "every spectrum parameter from the roadmap table") but is not read by `SpectrumEngine`/`SpectrumMapper`: the sidechain curve is always computed whenever the analyser has an active sidechain, and the editor (T-012) is expected to decide visibility from `hasSidechain() && showSidechain && isSidechainActive()`, per the UI spec.
