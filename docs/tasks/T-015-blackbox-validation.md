# T-015: Black-box validation suite
- Owner: plugin-validator
- Milestone: M4
- Status: TODO
- Depends on: T-009, T-013, T-014

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
- [ ] The results are written to `docs/qa/T-015-validation.md`, with the command lines to reproduce every check.

## Log
