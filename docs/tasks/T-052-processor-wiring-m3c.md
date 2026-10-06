# T-052: Processor wiring for M3c
- Owner: plugin-engineer
- Milestone: M3c
- Status: TODO
- Depends on: T-046, T-047, T-048, T-049, T-050, T-051, T-067

## Goal
Connect the M3c DSP to the host and to the editor-facing API:
- the second sidechain bus;
- the new parameters in the analysis thread and in MeterBank;
- the band solo with its safety failsafe;
- the spectrum capture.

The output must stay bit-exact whenever the band solo is not engaged.

## Inputs
- `docs/plans/m3c-feature-expansion.md`:
  - the Signal flow;
  - design items [1], [2], [4], [6], [7], [10] and [12];
  - the "Processor API additions" and Threading sections;
  - the Budgets and Risks 15 and 16.
- `docs/research/T-044-host-platform-constraints.md`, question 1: the bus declaration and the `isBusesLayoutSupported` pattern.
- The task logs of T-046 (underlay, `pushSidechain2`, the prepare overload), T-047 (`align0dB` in the settings), T-048 (`requestCapture`, `readCapture`), T-049 (meter setters, the new snapshot fields), T-050 (the `BandSoloFilter` API) and T-051 (`ParameterRefs`, `MeterScale.h` helpers).
- `source/PluginProcessor.*` and the T-009 and T-036 logs (analysis-thread lifecycle, view and freeze atomics).
- Working rules as in T-051.

## Scope
1. **Buses.**
   - Add input bus 2, "Sidechain 2": disabled by default, mono or stereo.
   - `isBusesLayoutSupported` accepts each sidechain disabled, mono or stereo, independently.
   - `bool isSidechainActive (int index) const noexcept` for 0 and 1. The existing no-argument form stays and means index 0.
   - On any layout change, re-prepare the engine with all three channel counts (the T-009 pattern: stop the analysis thread first).
2. **`processBlock`.** In order:
   1. `ScopedNoDenormals`;
   2. MeterBank on the input;
   3. FIFO pushes (main, Sidechain 1, Sidechain 2) while analysis is active;
   4. then the band solo:
      - read the solo word and the heartbeat once;
      - call `BandSoloFilter::setTarget`;
      - call `process` on the main buffer **only** while the filter is active.

   The buffer is never written otherwise.
3. **Band solo API** (message thread):
   - `setBandSolo (bool active, float centreHz) noexcept`: one packed 64-bit atomic;
   - `bandSoloHeartbeat() noexcept`: increments an atomic counter;
   - `soloWidth` is read from `ParameterRefs` on the audio thread.

   **The failsafe.** The audio thread counts samples since the heartbeat last changed. While the solo is engaged and that count exceeds 250 ms worth of samples, it releases the solo through the normal crossfade. `releaseResources` and the last `setAnalysisActive (false)` also release it.
4. **Analysis-thread settings.** Fill `underlay`, `numSidechain2Channels` and `align0dB`. The engine resolves the legacy `channelMode` aliases (T-046).
5. **Meters.** Once per block, pass integration, release and peak hold to MeterBank. In the K-xx C modes, pass 600 ms for both integration and release, through the `MeterScale.h` helpers.
6. **Capture API:** `requestSpectrumCapture() noexcept` and `bool getSpectrumCapture (SpectrumCapture&) noexcept`, both on the message thread.
7. Nothing in this task touches the editor.

## Acceptance criteria
- [ ] **Null tests are bit-exact** (maximum absolute difference exactly 0.0) with the solo not engaged:
  - main mono and stereo;
  - each sidechain disabled, mono and stereo, carrying distinct signals;
  - analysis on and off;
  - 44.1 and 96 kHz;
  - block sizes 1, 128 and random 1 to 4096;
  - every new parameter randomised per block.
- [ ] **Solo.** With a test editor stub that calls `setBandSolo` and the heartbeat:
  - engaging at 1 kHz with a width of 1 octave produces the T-050 response on the output;
  - releasing returns bit-exact output within 10 ms + one block;
  - with the heartbeat stopped, the solo releases itself between 250 ms and 250 ms + one block later;
  - `releaseResources` releases it.
- [ ] **Meters and spectrum see the input while soloed.** The MeterSnapshot and the frames are identical, within the meters' own tolerances, with the solo engaged and not engaged, for the same input.
- [ ] **Buses.**
  - pluginval at strictness 10 passes with the three-input bus configuration.
  - A runtime layout change of Sidechain 2 (disabled, stereo, mono, disabled) keeps the output bit-exact.
  - `isSidechainActive (1)` follows the layout.
  - Frames carry `hasSidechain2` correctly.
- [ ] **Parameters reach the DSP.**
  - Changing `underlay`, `align0dB`, `meterIntegration`, `meterRelease`, `meterPeakHold`, `soloWidth` and `meterScale` changes the corresponding DSP behaviour within one analysis frame or one audio block.
  - The C modes force 600 ms.
  - Legacy L/R and M/S values still produce dual frames.
- [ ] **Capture.** `requestSpectrumCapture` followed by `getSpectrumCapture` returns a capture within 20 ms (p95) at the default settings, live and frozen.
- [ ] **Budgets** (`[.perf]`), at 48 kHz, 128 samples, stereo main plus two stereo sidechains, analysis active, solo engaged and sweeping: `processBlock` mean ≤ 20 µs and p99 ≤ 50 µs. 0 allocations in `processBlock` over 10 s with the solo toggling and parameters changing (allocation hook).
- [ ] Warning-clean at `/W4`. `docs/qa/T-052-unit.md` and `docs/qa/T-052-validation.md` report PASS.

## Log
- 2026-10-03 vst-architect: Created from `docs/plans/m3c-feature-expansion.md`. From this task on, the roadmap's bit-exact rule reads "bit-exact whenever the band solo is not engaged".
- 2026-10-04 vst-architect: T-060 fixes the existing sidechain declaration first: a stereo default layout that is inactive by default, because JUCE asserts on a disabled default layout. Declare the second sidechain the same way, `stereo()` with `isActivatedByDefault = false`, following `docs/research/T-044-host-platform-constraints.md` question 1.
  - Renaming the existing bus from "Sidechain" to "Sidechain 1" may change how hosts show or restore the routing. Decide it with evidence and record the decision in the log.
  - How pluginval and the Steinberg validator handle three input buses is not documented. Run pluginval at strictness 10 on this build, and test every combination of disabled, mono and stereo on each sidechain.
  - The user checks by hand whether FL Studio lists inputs 1 to 6; that check goes in the M3c hands-on checklist.
- 2026-10-06 vst-architect: **Amended by `docs/plans/m3-review-spectrum-slots.md`** (spectrum slots, sidechain meters, Offset):
  - **Scope 4 (analysis settings).** Fill the per-slot settings from the slot parameters (T-051), not `underlay`. Resolve Link: when on, a slot uses slot 1's analysis parameters. Pass `offsetMode` to the engine (T-067), and pass `align0dB`. Legacy `channelMode` L/R and M/S resolve to Left and Mid; **this replaces the "legacy values still produce dual frames" criterion.**
  - **Scope 5 (meters).** Run **one `MeterBank` per input**: Main always, and Sidechain 1 and Sidechain 2 only while their bus is enabled. Pass the same integration, release, peak-hold and RMS settings to every bank. `requestStatsReset()` resets every bank.
  - **New API:**
    - `MeterSnapshot getMeterSnapshot (int input) const noexcept` for 0 (Main), 1 (SC1) and 2 (SC2); the no-argument form stays and means 0;
    - `bool isMeterInputActive (int input) const noexcept`.

    Prepare and resize the banks on a layout change, off the audio thread, using the T-009 pattern.
  - **Budget change.** At 48 kHz, 128 samples, stereo main plus two stereo sidechains, three banks, analysis active, solo engaged and sweeping: `processBlock` mean ≤ **40 µs** and p99 ≤ **90 µs** (it was 20 and 50 µs). 0 allocations are still required.
  - **More criteria:**
    - each sidechain snapshot equals a stand-alone `MeterBank` fed the same signal;
    - a disabled sidechain reports inactive, and its snapshot stops updating;
    - every slot reaches the engine with its own settings, and changing `s3FftSize` with Link off changes only slot 3's `binHz`;
    - Link on makes slots 2 to 4 follow slot 1's `fftSize` within one analysis frame;
    - `offsetMode` reaches the engine within one analysis frame.
