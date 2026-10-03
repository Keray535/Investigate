# T-036 validation (black-box)

Verdict: PASS (0 defects)

## Environment
- Windows 11 (10.0.26200), MSVC VS2022 x64, Release preset, base commit f41ce15 plus uncommitted T-036 changes (`source/PluginProcessor.h/.cpp`).
- pluginval 1.0.4 (`third_party/pluginval/1.0.4/pluginval.exe`), run with GUI tests enabled (not skipped).
- Steinberg validator and the render harness were not run: no harness exists yet and the task asks only for the checks below.

## Results
| Check | Result | Evidence |
|---|---|---|
| Release build warning-clean at /W4 | PASS | `touch source/PluginProcessor.cpp; cmake --build --preset release`: PluginProcessor.cpp recompiled, 0 occurrences of "warning" or "error" in the log, 2m04s. /W4 is applied to project sources by the root `CMakeLists.txt`. |
| pluginval strictness 10 | PASS | Manual: `third_party/pluginval/1.0.4/pluginval.exe --strictness-level 10 --validate-in-process --verbose --output-dir docs/qa/T-036-logs --validate build/Investigate_artefacts/Release/VST3/Investigate.vst3`, exit 0, `SUCCESS`, 15 s. CTest `pluginval` also passed (15.73 s). Log: `docs/qa/T-036-logs/pluginval.txt`. |
| Latency reported as 0 | PASS | pluginval log: `Reported latency: 0`; unit test "Latency and tail are always zero" passes. |
| Null tests bit-exact | PASS | `ctest --preset release`: tests #344 to #347 (processBlock bit-exact pass-through: mono 1 and 64 samples, stereo 512 at 48k, stereo 4096 at 96 kHz) and #11 (bit-exact while view and freeze change) all pass. No golden files exist in `tests/golden/` (directory absent). |
| State round trip unchanged | PASS | Tests #201, #202, #250, #251, #365 pass. The diff of `source/PluginProcessor.*` adds no `getStateInformation`, `setStateInformation` or `setProperty` lines. |
| Full suite | PASS | `ctest --preset release`: 100% passed, 0 failed out of 378. The 5 EBU Tech 3341/3342 tests are skipped because the local-only audio fixture is absent (pre-existing, unrelated to T-036). |

## Defects
None.

## Performance
Not measured. The audio thread is untouched, and the unit-tester allocation-hook test covers it.

## Commands
- `cmake --build --preset release`
- `ctest --preset release`
- the pluginval command in the results table.

## Open questions
1. The Debug build was not re-checked.
2. Neither the Steinberg validator nor a render harness is available in this repo.
