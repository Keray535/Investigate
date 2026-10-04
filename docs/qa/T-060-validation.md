# T-060 validation (black-box)

Verdict: PASS (0 defects)

## Environment
- Windows 11 (10.0.26200), MSVC VS2022 x64, Release preset, uncommitted working tree on top of c07d0ce.
- Release VST3 `build/Investigate_artefacts/Release/VST3/Investigate.vst3` (binary timestamp 07:22:07) is newer than `source/PluginProcessor.cpp` (07:21:00), so it contains the T-060 declaration `.withInput ("Sidechain", stereo(), false)`.
- pluginval 1.0.4 (`third_party/pluginval/1.0.4/pluginval.exe`), GUI tests not skipped.
- No Steinberg validator, no golden files (`tests/golden/` absent), no render harness null mode; the repo's null test is the bit-exact pass-through unit tests.

## Results
| Check | Result | Evidence |
|---|---|---|
| pluginval strictness 10, Release VST3 | PASS | exit 0, `SUCCESS`, 25 test groups, 15 s. Log `docs/qa/T-060-logs/pluginval.txt`. CTest #399 `pluginval` also passed. |
| Null test, bus not enabled, bit-exact | PASS | ctest #365 to #368 (mono 1 and 64 samples, stereo 512, stereo 4096 at 96 kHz) and #13 (bit-exact while view and freeze change) pass unchanged. |
| Sidechain bus tests | PASS | #16 (declared stereo default, inactive by default), #200 (enable mono and stereo via setBusesLayout / enableAllBuses), #5, #187, #340 (layout acceptance), #343, #38, #220, #369 (no allocation with sidechain). |
| Full suite, Release | PASS | `ctest --preset release --timeout 300`: 100% passed, 0 failed (log `docs/qa/T-060-logs/ctest_full.txt`). 5 EBU Tech 3341/3342 tests skipped: local audio fixture absent (pre-existing). |

## pluginval bus-layout tests (strictness 10)
- Basic bus: completed, no failure.
- Listing available buses: Inputs named layouts Mono, Stereo, Mono, Stereo (main plus sidechain), discrete Discrete #1 x2; Outputs Mono, Stereo; main bus 2 in / 2 out.
- Enabling all buses: completed, no failure (this enables the now stereo-declared sidechain).
- Disabling non-main busses: completed, no failure.
- Restoring default layout: main 2 in / 2 out, completed.
- Also run: VST3 validator group, Fuzz parameters, state and editor tests, all passed.

## Defects
None.

## Performance
Not measured (audio path unchanged).

## Commands
- `third_party/pluginval/1.0.4/pluginval.exe --strictness-level 10 --validate-in-process --verbose --output-dir docs/qa/T-060-logs --validate build/Investigate_artefacts/Release/VST3/Investigate.vst3`
- `ctest --preset release --timeout 300`

## Open questions
1. Debug ctest and the Debug jassert criterion are covered by `docs/qa/T-060-unit.md`, not re-run here.
