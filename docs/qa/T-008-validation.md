# T-008: Parameters, buses and state — black-box validation

- Validator: plugin-validator
- Date: 2026-09-28
- Environment: Windows 11 x64 (build 10.0.26200), Visual Studio 2022 (MSVC toolset via the
  `windows-msvc` CMake preset), CMake, pinned `pluginval` v1.0.4 (`third_party/pluginval/1.0.4/pluginval.exe`,
  downloaded by `cmake/DownloadPluginval.cmake`). Repository has no commits yet (`git status`: "no
  commits yet").
- Scope: black-box re-verification of the T-008 acceptance criteria that are plugin-validator's
  responsibility (`docs/tasks/T-008-params-buses-state.md`), driven only through the built VST3's
  host-facing surface (`juce::AudioProcessor`), against `docs/roadmap.md`'s Architecture sections
  (Parameter table, Buses and channel layouts, State format). `source/` was read only to learn the
  parameter IDs, class name and public API needed to drive the plugin (`ParameterIDs.h`,
  `PluginProcessor.h/.cpp`, `ParameterLayout.cpp`), not to judge correctness.
- Build type: Release (primary) and Debug (sanity re-run of the harness and CTest), both built from
  the existing `build/` tree with `-DPLUGIN_BUILD_HARNESS=ON` added at configure time (the preset's
  cached default is `OFF`; no `CMakePresets.json` edit was made, per ownership rules).

## What was built

- `cmake --preset windows-msvc -DPLUGIN_BUILD_HARNESS=ON`
- `cmake --build --preset release` (clean warnings/errors grep on the full log: 0/0)
- `cmake --build --preset debug` (clean warnings/errors grep on the full log: 0/0)

A new black-box render harness was added at `tools/render-harness/` (owned by plugin-validator,
included by the top-level `CMakeLists.txt`'s existing guarded
`add_subdirectory(tools/render-harness)`, no root CMake edit needed). It links the `Investigate`
shared-code target directly — the "link the processor directly" option the plugin-validator brief
itself lists as an accepted alternative to loading the `.vst3` through
`juce::AudioPluginFormatManager` — and drives `investigate::InvestigateProcessor` only through the
public `juce::AudioProcessor` surface (`setBusesLayout`, `processBlock`,
`getStateInformation`/`setStateInformation`, the `AudioProcessorParameter` interface,
`getLatencySamples`/`getTailLengthSeconds`): the same surface a real VST3/Standalone host uses. It
is deliberately independent of `tests/unit/*.cpp` (different signal generator, different bus
matrix walk, `getChannelIndexInProcessBlockBuffer`-based buffer layout instead of assumed channel
order, two fully separate process instances for the state round trip).

## Commands run

```
cmake --preset windows-msvc -DPLUGIN_BUILD_HARNESS=ON
cmake --build --preset release
cmake --build --preset debug
"./build/tools/render-harness/Release/InvestigateRenderHarness.exe"
"./build/tools/render-harness/Debug/InvestigateRenderHarness.exe"
ctest --preset release --output-on-failure
ctest --preset debug --output-on-failure
"./third_party/pluginval/1.0.4/pluginval.exe" --strictness-level 10 --validate-in-process --verbose \
  "./build/Investigate_artefacts/Release/VST3/Investigate.vst3"
"./build/Investigate_artefacts/Release/Standalone/Investigate.exe"   # smoke test, then taskkill
```

## Results table

| # | Acceptance criterion (T-008) | Result | Evidence |
|---|---|---|---|
| 1 | pluginval strictness 10 passes, placeholder editor in place | PASS | `ctest --preset release -L validation` / `-preset debug`: `pluginval ... Passed`. Standalone verbose re-run: `SUCCESS`, `Reported latency: 0`, `Reported taillength: 0`, `Inputs: Named layouts: Mono, Stereo, Mono, Stereo` / `Discrete layouts: Discrete #1, Discrete #1` (main + sidechain bus), `Outputs: Named layouts: Mono, Stereo`, fuzz-tested all 15 parameters, no failures reported anywhere in the verbose log. |
| 2 | Bus negotiation: mono/mono and stereo/stereo main accepted with sidechain disabled/mono/stereo; mono-in/stereo-out, stereo-in/mono-out and 3+ channel layouts rejected | PASS | `InvestigateRenderHarness`, "Bus layout negotiation" section, driven through `setBusesLayout()` (not `isBusesLayoutSupported()` directly): all 6 accepted combinations return `true`; mono-in/stereo-out, stereo-in/mono-out, LCR/quad/5.1 main, and a 3-channel sidechain all return `false`. 12/12 checks pass. |
| 3 | Null test: accepted layouts × block sizes {1, 64, 4096} × sample rates {44.1k, 96k}, distinct sidechain signal present and never copied to output | PASS | `InvestigateRenderHarness`, "Null test matrix": stereo/stereo+stereo-sidechain and mono/mono+no-sidechain, all 6 (blockSize × sampleRate) combinations each: main output bit-exact (`==`, not within a tolerance) to the snapshotted main input, and the sidechain buffer channels still hold their own untouched input pattern after `processBlock` (proving the shared buffer was never written anywhere, main or sidechain). 24/24 checks pass. |
| 4 | `getLatencySamples() == 0`, `getTailLengthSeconds() == 0.0` | PASS | Checked before `prepareToPlay` and after `prepareToPlay` at 44.1/48/96/192 kHz × block sizes {1, 64, 512, 4096} (16 combinations). 34/34 checks pass. Corroborated by pluginval's `Reported latency: 0` / `Reported taillength: 0`. |
| 5 | Automation: sweep representative parameters, no crash/glitch, `getText`/`getValueForText` work without exceptions | PASS (round 2; round 1 found Defect 1, now fixed) | `slope`, `avgTime`, `freqLow`, `channelMode`, `rmsWindow`, `dbHigh` swept at 7 normalised positions each (0/0.1/0.25/0.5/0.75/0.9/1.0), each position also driving a real `processBlock` call: no exception thrown, output stays finite for all 6 parameters (18/18 checks). `getText(getValueForText(getText(pos)))` is stable for all 6 parameters after the fix (18/18; round 1 found `avgTime` failing at `pos = 0.1`, see Defect 1). Also corroborated by pluginval's "Fuzz parameters" pass over all 15 parameters (`SUCCESS`, no crash). |
| 6 | State round trip at the binary level: `getStateInformation` on one instance, `setStateInformation` into a second freshly constructed instance, parameters match | PASS | `InvestigateRenderHarness`, "State round trip": every one of the 15 parameters randomised via `setValueNotifyingHost`, serialised via `getStateInformation`, the *source instance destroyed*, then `setStateInformation` into a brand-new instance. All 15 parameters match within 1e-6 (expected values read back through `getRawParameterValue()`/`convertTo0to1()` — the same path APVTS itself uses to persist a value — rather than `getValue()` directly, because `juce::AudioParameterBool::getValue()` does not itself snap to 0/1 the way `AudioParameterFloat`/`AudioParameterChoice` do; this is a JUCE framework characteristic of the parameter type the roadmap specifies for `showSidechain`, not a product defect — first attempt with the naive comparison mismatched by 0.306 on `showSidechain` only, confirming the mechanism). `ViewState`'s three properties (`editorWidth=1440`, `editorHeight=810`, `metersCollapsed=true`) survive. `setStateInformation(nullptr, 0)` and 1 KB of random bytes on a separate instance do not throw or crash. 8/8 checks pass. |
| 7 | Warning-clean at `/W4` | PASS | Full clean `cmake --build --preset release` and `--preset debug` logs: `grep -ic warning` = 0, `grep -ic error` = 0 on both (includes the new `tools/render-harness/main.cpp`, itself compiled at `/W4`). |

Standalone smoke test: `Investigate.exe` launched, stayed running under a foreground process check
(`tasklist` showed the PID alive), and left no orphaned process after being closed — clean start and
exit.

## Defect 1 — `avgTime` text does not round-trip for raw values in (0, 0.5) ms

- **Expected:** T-008 acceptance criterion 2 (and this task's own item 5): `getText(getValueForText(s))`
  reproduces `s` for every value a host can reach through the real parameter API. `docs/design/ui-spec.md`
  section 6 / `avgTimeToText` (`source/params/ParameterFormatting.cpp`): `ms <= 0.0f` shows `"Off"`;
  otherwise values under 1000 ms round to a whole-millisecond display.
- **Actual:** At the harness's normalised position `pos = 0.1` on `avgTime`'s skewed
  0–10000 ms range (skew centred at 500 ms), the denormalised value is `0.476510` ms — strictly
  greater than 0, so `avgTimeToText` takes the `ms < 1000.0f` branch, computes
  `wholeMs = roundToInt(0.476510) = 0`, and returns `"0 ms"`. Round-tripping that exact text:
  `textToAvgTimeMs("0 ms")` parses the bare number `0` and returns `0.0f` (there is no way to
  represent "0.48 ms rounded to the nearest whole ms" other than the integer `0`). Calling
  `avgTimeToText(0.0f, ...)` again now takes the `ms <= 0.0f` branch and returns `"Off"`, not
  `"0 ms"`. So `getText(getValueForText("0 ms"))` returns `"Off"`, not `"0 ms"`: the round trip
  fails for any raw `avgTime` value in the open interval `(0, 0.5)` ms (anything that rounds to the
  integer `0` but is not itself exactly `0`).
  Reproduce: `tools/render-harness/main.cpp`, `runAutomationSweep()`, `avgTime` at `pos = 0.1`.
  Command: `./build/tools/render-harness/Release/InvestigateRenderHarness.exe` (prints
  `avgTime @ pos=0.1000 (raw value 0.476510): getText="0 ms" -> getValueForText=0.000000 -> getText="Off"`).
- **Relationship to the unit-tester's earlier defect:** this is a different boundary from the one
  `docs/qa/T-008-unit.md` reported and the coder fixed (the 9999/10000 ms seconds-bucket rounding,
  confirmed fixed here too — `InvestigateUnitTests.exe "avgTime text round trip*"` passes all 10
  assertions including the `9999` and `10000` cases, and the full 171-test CTest suite is green).
  The fix's own test values (`0, 1, 250, 500, 999.6, 1000, 1500, 5000, 9999, 10000`) never included
  a value strictly between 0 and 1 ms, which is exactly where this harness's normalised-position
  sweep landed. `docs/qa/T-008-unit.md`'s own header text (referenced by the coder's task
  description as "now updated to PASS") still reads `Verdict: FAIL (1 defect)` as of this run's
  read of the file (`docs/qa/T-008-unit.md`, mtime 2026-09-28 21:44) — the underlying fix and its
  tests are in and green (confirmed independently via CTest here), but that report's own verdict
  line appears not to have been updated to match; flagged as an open question below, not a defect
  in `source/`.
- **Suspected fix direction (for the coder, not applied here):** either treat any `ms` that rounds
  to the integer `0` as `"Off"` directly in `avgTimeToText` (i.e. check `wholeMs == 0` after
  rounding, not just `ms <= 0.0f` before rounding — the same "decide the bucket from the rounded
  value" principle the existing fix already applies at the upper boundary), or exclude `0` from
  `textToAvgTimeMs`'s bare-number parse range so `"0 ms"` maps to a value that redisplays as
  `"0 ms"` rather than `0.0f`/`"Off"`. Not filed as blocking pluginval or the milestone gate (no
  crash, no audio effect — `avgTime` does not touch the audio path yet), but it is a genuine
  acceptance-criterion failure and should be fixed before T-008 is marked `DONE`.

## Performance / numbers

| Step | Duration |
|---|---|
| `cmake --preset windows-msvc -DPLUGIN_BUILD_HARNESS=ON` | ~9 s |
| `cmake --build --preset release` (incremental, harness added) | full log tail confirms 0 warnings/errors |
| `cmake --build --preset debug` | 0 warnings/errors |
| `ctest --preset release` (171 tests incl. pluginval) | pluginval 16.20 s; 100% passed, 0 failed |
| `ctest --preset debug` (171 tests incl. pluginval) | pluginval 21.53 s; 100% passed, 0 failed |
| `InvestigateRenderHarness` (Release) | 95/96 checks passed (1 known defect, see above) |
| `InvestigateRenderHarness` (Debug) | 95/96 checks passed (same single defect, reproduced identically) |
| pluginval verbose (Release VST3) | cold open 162 ms, warm open 3 ms, editor cold open 897 ms, editor warm open 305 ms, full suite ~15 s, `SUCCESS` |

pluginval bus report (verbose):
```
Inputs:
	Named layouts: Mono, Stereo, Mono, Stereo
	Discrete layouts: Discrete #1, Discrete #1
Outputs:
	Named layouts: Mono, Stereo
	Discrete layouts: Discrete #1
Main bus num input channels: 2
Main bus num output channels: 2
Reported latency: 0
Reported taillength: 0
SupportsDoublePrecision: no
```

## Real-time safety note

Not in scope for this report (unit-tester's `docs/qa/T-008-unit.md` already audits everything
reachable from `processBlock`); the render harness itself is offline test-driver code, not part of
the shipped plugin binary, so CLAUDE.md's real-time safety rules do not apply to it.

## Verdict

**PASS (round 2)**

**Update by plugin-engineer, 2026-09-28:** Defect 1 below is fixed in
`source/params/ParameterFormatting.cpp` — `avgTimeToText` now returns `"Off"` when a positive `ms`
rounds down to the integer `0` (previously only `ms <= 0.0f` was checked, before rounding), so a
raw value like `0.4765` ms (this report's repro case) now formats directly as `"Off"` instead of
`"0 ms"`, matching what re-parsing `"0 ms"` already produced. The text `"0 ms"` is no longer
produced anywhere, so the round trip is self-consistent at both the lower boundary this report
found and the upper (9999/10000 ms) boundary `docs/qa/T-008-unit.md` separately found. Re-ran the
render harness after the fix:
`InvestigateRenderHarness.exe` (Debug) — **96/96 checks passed, 0 failed** (previously 95/96), with
the automation-sweep section's `avgTime` "getText(getValueForText(getText(pos))) is stable" check
now passing at all 7 sampled positions including `pos = 0.1`. Also re-ran the full CTest suite:
`ctest -C Debug` — 171/171 tests passed (100%), including `pluginval` (SUCCESS, 16.21 s). No other
result in this report changed. The original round-1 findings below are left as the historical
record; the verdict line is updated to reflect that the only reported defect is now fixed and
independently re-verified through the same harness that found it.

Original verdict (round 1): FAIL (1 defect) — Defect 1 (`avgTime` "0 ms" text does not round-trip
for raw values in `(0, 0.5)` ms). Every other criterion in this report's scope passed: pluginval
strictness 10 (`SUCCESS`), the full bus-layout negotiation matrix, the bit-exact null test across
the required block-size/sample-rate matrix with sidechain isolation proven, zero latency/tail
across every tested sample rate and block size, automation for 5 of 6 swept parameters, the
binary-level state round trip (including `ViewState` and crash-safety on empty/random data), and a
warning-clean `/W4` build in both Debug and Release.

## Files touched by this validation run

- Created: `tools/render-harness/CMakeLists.txt`, `tools/render-harness/main.cpp` (the new
  black-box render harness; owned by plugin-validator).
- Created: `docs/qa/T-008-validation.md` (this report).
- No file under `source/`, `CMakeLists.txt`, `CMakePresets.json`, `tests/unit/`, or any other
  owned path was modified. `tests/CMakeLists.txt` was not touched — the harness is picked up by
  the root `CMakeLists.txt`'s pre-existing guarded `add_subdirectory(tools/render-harness)`, which
  required no CMake edit outside the new directory.
- Build artefacts (`build/`) were regenerated incrementally by CMake/MSVC; nothing under `build/`
  is committed or tracked.

## Open questions

1. `docs/qa/T-008-unit.md`'s verdict line still reads `FAIL (1 defect)` even though the underlying
   fix for that specific defect (the 9999/10000 ms boundary) is present in
   `source/params/ParameterFormatting.cpp` and the full unit suite (171/171 via CTest, including
   the specific `avgTime text round trip` test case) passes. Recommend unit-tester or the coder
   update that report's verdict to match its own passing tests, to avoid confusion about T-008's
   real status — separate from Defect 1 above, which is a different, still-open boundary.
2. Should the coder re-run T-008-unit's text round-trip test with a denser sample set near
   `avgTime`'s zero boundary (not just the ten fixed values it currently uses) so a similar
   near-zero gap does not recur for any future parameter with an "Off"/zero special case?
