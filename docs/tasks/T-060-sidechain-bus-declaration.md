# T-060: Sidechain bus declaration: a stereo default layout, inactive by default
- Owner: plugin-engineer
- Milestone: M3b
- Status: DONE
- Depends on: T-041

## Goal
`source/PluginProcessor.cpp` (constructor, about line 172) declares the sidechain as `.withInput ("Sidechain", juce::AudioChannelSet::disabled(), true)`. T-044 found that this breaks a JUCE rule:
- the `Bus` constructor asserts that the default layout is not disabled (`juce_AudioProcessor.cpp`, about line 1013, "Your default layout cannot be disabled");
- the VST3 wrapper reports a bus's channel count from `getLastEnabledLayout()`, so a bus that has never had an enabled layout may be offered to the host with no channels.

The intent in the code comment, "disabled by default", is expressed in JUCE by a real default layout plus `isActivatedByDefault = false`. Fix the declaration now, so that the M3b hands-on test in FL Studio and the later M3c sidechain work (T-046, T-052) start from a correct bus.

## Inputs
- `docs/research/T-044-host-platform-constraints.md`, question 1: the bus declaration, the `isBusesLayoutSupported` pattern and the pitfalls.
- `source/PluginProcessor.cpp`: the constructor, `prepareToPlay` (the sidechain channel count through `isSidechainActive()` and `getChannelCountOfBus (true, 1)`) and `isBusesLayoutSupported`.
- `docs/tasks/T-008-params-buses-state.md` and `docs/tasks/T-009-processor-wiring.md` (logs): the original bus decisions and the sidechain tests.
- The pinned JUCE sources in `third_party/juce-src/` (read only).

## Scope
1. Declare the sidechain as `.withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)`. Keep the bus name "Sidechain" for now; T-052 decides the names when it adds the second sidechain. Update the comment to say why: inactive by default, with a stereo default layout as JUCE requires.
2. Keep `isBusesLayoutSupported` accepting a disabled, mono or stereo sidechain, independent of the main layout. Change it only if the new declaration requires it, and record why in the log.
3. Check that `isSidechainActive()` and the prepared sidechain channel count still give 0 when the host has not enabled the bus, and 1 or 2 when it has.
4. Do not touch the second sidechain, the parameters or the state format. Those belong to T-046, T-051 and T-052.

## Acceptance criteria
- [x] No `jassert` fires in the Debug build when the processor is constructed (checked with a Debug unit test run, or in a debugger, and recorded in the log).
- [x] With the bus not enabled by the host, the output is bit-identical to before: the null test and the existing sidechain-off tests pass unchanged.
- [x] With the bus enabled (mono and stereo, through `setBusesLayout` or `enableAllBuses` in a test), the sidechain curve is fed exactly as before. The T-009 and T-036 sidechain tests pass.
- [x] `ctest --timeout 300` passes in Release and in Debug. Warning-clean at `/W4`.
- [x] pluginval at strictness 10 passes on the Release VST3. Its bus-layout tests are listed in the validation report.
- [x] `docs/qa/T-060-unit.md` and `docs/qa/T-060-validation.md` report PASS.

## Log
- 2026-10-04 vst-architect (acceptance): Both QA reports say PASS in round 1, so the task is DONE. The testers' notes are routed:
  - The render harness has no null mode, so the unit tests cover the null test here. A harness null mode belongs to T-014 and T-015.
  - The five EBU Tech 3341/3342 skips are the known missing-fixture case from T-021, not a regression.
- 2026-10-04 vst-architect: Created from the T-044 code finding (open point for plugin-engineer). A small C++ task. It runs before T-037, so the M3b hands-on test in a host uses a correctly declared sidechain. Only one C++ task runs at a time.
- 2026-10-04 plugin-engineer: Constructor now declares `.withInput ("Sidechain", stereo(), false)` with an updated comment. isBusesLayoutSupported, prepareToPlay and isSidechainActive unchanged (a non-enabled bus gives isEnabled false, so 0 channels). Release and Debug build warning-clean. unit-tester: PASS, ctest 399/399 in Release and Debug, no jassert in Debug, new tests tests/unit/ProcessorSidechainBusDeclarationTest.cpp ([T-060]). plugin-validator: PASS, pluginval strictness 10 on Release VST3, null tests bit-exact. 1 round. Reports: docs/qa/T-060-unit.md, docs/qa/T-060-validation.md.
