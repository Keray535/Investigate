# T-015: Black-box validation suite
- Owner: plugin-validator
- Milestone: M4
- Status: TODO
- Depends on: T-009, T-013, T-014, T-038

## Goal
Validate Investigate the way hosts and users will use it:
- host-level validators;
- bit-exact pass-through across layouts, sample rates and block sizes;
- state round trip;
- parameter automation under load;
- latency reporting;
- meter conformance measured through the processor;
- a Standalone smoke test.

This task gates M4 together with T-016 and T-017.

## Inputs
- `docs/roadmap.md`: Scope, Architecture (buses, parameter table, processor API, MeterSnapshot, state format) and Budgets.
- `docs/research/T-002-dsp-references.md`: section 3 (the EBU test-case table) and section 9 (Standalone input).
- `docs/tasks/T-006-loudness-meter.md` and `docs/tasks/T-007-meters-meterbank.md`, for the meter tolerances.
- The tooling from T-014: pluginval, the VST3 validator, and the render-harness hook.

## Scope
- Implement the render harness in `tools/render-harness/` and the integration tests in `tests/integration/`. Put golden files, if any, in `tests/golden/`. Instantiating the processor in-process through the shared-code target is allowed for meter conformance, because meters are not observable through the VST3 audio output.
- pluginval at strictness 10 on the Release VST3, with the editor enabled.
- The Steinberg VST3 validator on the Release VST3.
- The null test matrix, described in the acceptance criteria.
- A state round trip through the VST3 interface as a host does it.
- Automation stress.
- Meter conformance through the processor.
- A Standalone smoke test: the app launches, opens its audio settings, and analyses input according to T-002 section 9, if that can be automated. Otherwise write a manual checklist for the user.

## Acceptance criteria
- [ ] pluginval at strictness 10 passes. The VST3 validator passes with 0 errors.
- [ ] Null test: the output equals the input bit-exactly (maximum absolute difference exactly 0.0) for:
  - sample rates 44.1, 48, 88.2, 96, 176.4 and 192 kHz;
  - block sizes 1, 16, 128, 512 and 4096, plus a randomly varying block size from 1 to 4096;
  - mono and stereo main layouts;
  - sidechain disabled, mono and stereo, carrying a distinct signal;
  - analysis active and inactive.
- [ ] The reported latency is 0 samples and the tail is 0 s in every layout.
- [ ] State round trip through the host interface: randomise all parameters 20 times, save, reload into a new instance, and all values match within 1e-6 normalised. `ViewState` persists.
- [ ] Automation stress: every parameter is swept with random values each block for 60 s of audio while analysis is active. Result: no crash, no NaN or Inf in any `SpectrumFrame` or `MeterSnapshot`, and the output stays bit-exact.
- [ ] Meter conformance through the processor: every synthesisable EBU Tech 3341 and 3342 case passes within the tolerances in T-006 and T-007. The true-peak cases pass within +0.2/−0.4 dB.
- [ ] A sidechain layout change at runtime (disabled to stereo to disabled) keeps the output bit-exact, and `isSidechainActive()` follows the layout.
- [ ] **Real program material** (added 2026-10-03). The user's own track lives in the local fixture folder at `third_party/_fixtures/audio/test_audio.wav`. It is gitignored and never committed (properties in `docs/research/test-audio-inspection.md`: 48 kHz, 32-bit float, stereo, 200.8 s, sample peak −0.22 dBFS, very bass-heavy). Locate the folder through `INVESTIGATE_TEST_AUDIO_DIR` (T-032). If the file is absent, these checks SKIP with a reason, as in CI. With the file present:
  - Null test: render the whole track through the processor at 48 kHz, with stereo main, the same track on a stereo sidechain, analysis active, and block sizes 128 and randomly varying from 1 to 4096. The output equals the input bit-exactly (maximum absolute difference 0.0).
  - Loudness and peak cross-check against libebur128 (the test-only oracle, never linked into the plugin) over the whole track:
    - integrated loudness, maximum momentary and maximum short-term within ±0.1 LU;
    - LRA within ±0.2 LU;
    - true-peak maximum within ±0.2 dB;
    - the sample-peak maximum equals the file's own peak (−0.22 dBFS) within ±0.01 dB.

    Report all values side by side.
  - Spectrum sanity: the `SpectrumFrame` contains no NaN or Inf over the whole track in every channel mode. With the defaults, the average curve's energy sits mostly below 200 Hz, consistent with the inspection report.
- [ ] **Zoom and pan** (added 2026-10-03, M3b, `docs/plans/spectrum-zoom-pan.md`):
  - with analysis active, a thread drives `setSpectrumView` with random views and toggles `setSpectrumFrozen` at 60 Hz throughout one null-test configuration at 48 kHz, 128 samples, stereo plus sidechain; the output stays bit-exact;
  - the automation-stress run is repeated with the same view and freeze driving; no NaN or Inf appears in any `SpectrumFrame`, including the overview arrays;
  - the state round trip shows that no zoom or freeze value is saved, and that `ViewState` has the same properties as before M3b.
- [ ] The results are written to `docs/qa/T-015-validation.md`, with the command lines to reproduce every check.

## Log
- 2026-10-03 vst-architect: Added the zoom-and-pan checks and a dependency on T-038 (milestone M3b), so the suite validates the final UI.
