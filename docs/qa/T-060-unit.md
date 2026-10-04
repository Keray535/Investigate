# T-060 unit QA: sidechain bus declaration

Verdict: PASS (0 defects)

## Diff review (git diff source/)
Only `source/PluginProcessor.cpp` constructor changed: `.withInput ("Sidechain", disabled(), true)` became `.withInput ("Sidechain", stereo(), false)`, plus an updated comment. `isBusesLayoutSupported`, `prepareToPlay` and `isSidechainActive()` are untouched. Nothing on the audio path changed. `isSidechainActive()` requires `bus->isEnabled()` and channels > 0, so a stereo default layout with `isActivatedByDefault=false` still yields inactive and prepared sidechain channels 0. No real-time-safety impact (constructor only).

## Planned tests (new file tests/unit/ProcessorSidechainBusDeclarationTest.cpp, tag [T-060])
| Test | Result |
|---|---|
| Construction (Debug, no jassert): 2 input buses, bus 1 named "Sidechain", not enabled, not enabled by default, default and last-enabled layout stereo, 0 channels; main buses enabled; isSidechainActive()==false; after prepareToPlay getChannelCountOfBus(true,1)==0; processBlock on 2 ch OK; no frame reports hasSidechain | PASS (Debug and Release) |
| setBusesLayout with mono and stereo sidechain: isSidechainActive true, channel count 1/2, main bus unchanged; disabling again returns to inactive and 0 | PASS |
| enableAllBuses: active, 2 channels | PASS |
| Enabled mono/stereo sidechain is fed to SpectrumEngine (SpectrumFrame.hasSidechain observed, polled up to 10 s wall clock, same pattern as T-009 tests) | PASS |
| isBusesLayoutSupported accepts disabled/mono/stereo sidechain with mono or stereo main; rejects 3-channel sidechain | PASS |

Existing T-008 bus-layout, T-009 (`isSidechainActive() reflects a live sidechain bus-layout change...`, real-time safety with stereo sidechain) and T-036 tests pass unchanged. The prepared sidechain channel count has no public accessor, so it was checked through `getChannelCountOfBus(true,1)` and the frame's `hasSidechain`.

## Full suite
- Release: `ctest --preset release --timeout 300`: 100% passed, 0 failed of 399 (162 s).
- Debug: `ctest --preset debug --timeout 300`: 100% passed, 0 failed of 399 (164 s). A Debug jassert would abort the run; none fired on processor construction (every test constructs the processor).
- 5 Skipped tests (Tech 3341/3342 authentic programme/5.0 cases, "not run") are pre-existing, caused by absent EBU reference files; not related to T-060.
- `[.perf]` timing tests are hidden by default and were not run.
- Warnings: no warnings in the build output for the changed file or the new test.

## Not run
Sanitizers, clang-tidy and coverage were not requested and not run. pluginval is covered by `T-060-validation.md`.

## Commands
```
cmake --build --preset debug --target InvestigateUnitTests
cmake --build --preset release --target InvestigateUnitTests
build/tests/Debug/InvestigateUnitTests.exe "[T-060]"
build/tests/Release/InvestigateUnitTests.exe "[T-060]"
ctest --preset release --timeout 300
ctest --preset debug --timeout 300
```

## Open questions
none
