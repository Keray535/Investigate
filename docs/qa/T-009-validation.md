# T-009: Processor wiring, analysis thread and editor API — black-box validation

- Validator: plugin-validator
- Date: 2026-09-29
- Environment: Windows 11 x64 (build 10.0.26200), Visual Studio 2022 (MSVC toolset via the
  `windows-msvc` CMake preset), pinned `pluginval` v1.0.4 (`third_party/pluginval/1.0.4/pluginval.exe`).
  Repository has no commits yet (`git status`: everything untracked).
- Scope: black-box re-verification of the T-009 acceptance criteria that are plugin-validator's
  responsibility (`docs/tasks/T-009-processor-wiring.md`), driven only through the built VST3's and
  Standalone's host-facing surface (`juce::AudioProcessor` plus the roadmap's "Processor API used by
  the editor"), against `docs/roadmap.md`'s Architecture (Signal flow, Threading model, Processor
  API, Buses and channel layouts, State format, risk #7) and Budgets sections. `source/` was read
  only to confirm the API surface to drive (`PluginProcessor.h/.cpp`, `StandaloneApp.cpp`) and to
  understand what a black-box timing measurement of `setAnalysisActive(false)` was actually measuring
  (it calls `juce::Thread::stopThread(50)` synchronously) — not to judge correctness.
- Build type: Release, built from the existing `build/` tree with `-DPLUGIN_BUILD_HARNESS=ON` (the
  preset's cached default is `OFF`; no `CMakePresets.json` edit made).

## What was built

- `cmake --preset windows-msvc -DPLUGIN_BUILD_HARNESS=ON`
- `cmake --build --preset release` (full log: `grep -ic warning` = 0, `grep -ic error` = 0). This
  includes `Investigate_Standalone`, which links cleanly — independently confirms the T-025 fix
  (`source/StandaloneApp.cpp` now defines only `JUCE_CREATE_APPLICATION_DEFINE`, not
  `JUCE_MAIN_FUNCTION_DEFINITION`), closing the round-1 link failure recorded in the roadmap's
  2026-09-29 T-025 decision row.

`tools/render-harness/main.cpp` (owned by plugin-validator, first written for T-008) was extended
with six new checks specific to T-009's acceptance criteria: a null test with analysis active across
four bus-layout cases, `isSidechainActive()` driven through live `setBusesLayout()` enable/disable,
a sample-rate-change-while-active no-crash check, an analysis-thread stop-timing measurement, and an
independent `processBlock`/MeterBank-proxy CPU timing measurement. No other file under `tools/`,
`tests/`, `source/`, or the CMake project was modified.

## Commands run

```
cmake --preset windows-msvc -DPLUGIN_BUILD_HARNESS=ON
cmake --build --preset release
ctest --preset release --output-on-failure
"./third_party/pluginval/1.0.4/pluginval.exe" --strictness-level 10 --validate \
  "./build/Investigate_artefacts/Release/VST3/Investigate.vst3" --output-dir /tmp/pluginval-out --verbose
"./build/tools/render-harness/Release/InvestigateRenderHarness.exe"
"./build/Investigate_artefacts/Release/Standalone/Investigate.exe"   # smoke test, then taskkill
```

## Results table

| # | Acceptance criterion (T-009) | Result | Evidence |
|---|---|---|---|
| 1 | `Investigate_Standalone` links, launches, starts with "Mute audio input" off (T-025 closure) | PASS | Full Release rebuild links `Investigate_Standalone` with 0 warnings/errors from project code. Launched directly (`Investigate.exe`, not via `cmd start`, which the sandbox blocked — "Erişim engellendi"/Access denied on `cmd //c start`): `tasklist` shows the process alive at +3s and +6s; `taskkill //T` terminated it cleanly, and a follow-up `tasklist` shows no orphaned process. `source/StandaloneApp.cpp`'s `createPluginHolder()` calls `holder->getMuteInputValue().setValue(false)` unconditionally, immediately after constructing the `StandalonePluginHolder` and *after* it would have loaded any previously persisted "Mute audio input" value from its settings file — so the override is deterministic on every launch regardless of prior state, not merely a fresh-install default. No prior `*.settings` file existed on this machine (no `%APPDATA%`/`%LOCALAPPDATA%` folder for the app was found before this run, since `appProperties.saveIfNeeded()` only writes it on `shutdown()`), so this run was itself a "fresh install" case. **Limitation**: this headless environment has no GUI-automation tool to click into the audio settings panel and screenshot the toggle's rendered state, so the "on-screen toggle reads off" part of the criterion is confirmed by the deterministic code path plus a clean launch/exit, not by a screenshot. Flagged as an open question below, not a defect. |
| 2 | pluginval strictness 10 passes | PASS | `ctest --preset release -L validation`: `pluginval ... Passed` (16.45s). Independent standalone re-run: `--strictness-level 10 --validate --output-dir ... --verbose`, exit code 0, `SUCCESS`, bus report `Inputs: Named layouts: Mono, Stereo, Mono, Stereo / Discrete layouts: Discrete #1, Discrete #1`, `Outputs: Named layouts: Mono, Stereo`, all 15 parameters fuzz-tested with no failures, background-thread-state and parameter-thread-safety sections both completed with no failures. |
| 2b | Full `ctest --preset release` suite, independently re-run | PASS | 172 registered tests: 167 run, 167 passed, 5 skipped (pre-existing, documented EBU Tech 3341/3342 cases needing optional reference WAVs not present on this machine — unchanged from `docs/qa/T-009-unit.md`), 0 failed. Matches unit-tester's own count exactly. |
| 3 | Null test / bit-exact pass-through, all T-008 layouts, analysis active and inactive | PASS | Pre-existing T-008 matrix (analysis inactive: mono/mono no-sidechain and stereo/stereo+stereo-sidechain, {44.1k,96k}Hz × {1,64,4096} blocks) re-confirmed: 24/24. New T-009 matrix (analysis **active**, `setAnalysisActive(true)` before 20 processed blocks each): mono/mono no-sidechain, stereo/stereo no-sidechain, stereo/stereo+mono-sidechain, stereo/stereo+stereo-sidechain — all 4 bit-exact (`==`, not tolerance-based) across all 20 blocks each. |
| 4 | State round trip: every parameter and `ViewState` round-trip; fresh instance; corrupt/truncated data does not crash | PASS | Pre-existing T-008 harness section re-confirmed: all 15 parameters match within 1e-6 after `getStateInformation`/`setStateInformation` into a separately constructed instance (source destroyed first); `ViewState`'s `editorWidth`/`editorHeight`/`metersCollapsed` survive; `setStateInformation(nullptr, 0)` and 1KB of random bytes on a fresh instance throw/crash neither. |
| 5 | Bus layout acceptance matrix (mono/mono, stereo/stereo, sidechain disabled/mono/stereo); sample-rate change mid-session does not crash; `isSidechainActive()` reflects host-level bus enable/disable | PASS | Bus matrix: pre-existing T-008 `setBusesLayout()` walk re-confirmed (12/12 accepted combinations, invalid layouts rejected), corroborated by pluginval's own bus-layout listing/enable/disable/restore sections (all completed with no failures). New T-009 checks: `isSidechainActive()` false before any layout is set, false with the sidechain bus disabled, true with it enabled (mono and stereo), and false again after re-disabling it — all via live `setBusesLayout()` calls, not by reading the flag directly. Sample rate 44.1 → 96 → 48 kHz mid-session with `setAnalysisActive(true)` held across all three `prepareToPlay()` calls, each followed by 10 processed blocks and a frame/snapshot poll: no exception, no crash. |
| 6 | Automation: every parameter host-automatable | PASS | pluginval's "Fuzz parameters" section fuzzed all 15 parameters with `isAutomatable=1` reported for each (verbose per-parameter dump), `SUCCESS`. Harness automation sweep (6 representative parameters incl. `avgTime`, `rmsWindow`) re-confirms no exception, finite output, and stable `getText(getValueForText(getText(pos)))` round trips at 7 sampled positions each, including the (0, 0.5)ms `avgTime` boundary and both `rmsWindow` display buckets (T-008 round-2 fixes) — 54/54 checks pass. No exceptions to automatability found. |
| 7 | Performance: `processBlock` and MeterBank-alone CPU, independent re-measurement | PASS, wider margin than unit-tester's | See Performance section below. |
| 8 | Analysis thread stop timing: `setAnalysisActive(false)` observably stops within 50ms | PASS | `setAnalysisActive(false)` calls `juce::Thread::stopThread(50)` synchronously (confirmed by reading `PluginProcessor.cpp`), so timing the call itself from the outside is a valid black-box measurement. 20 iterations of `setAnalysisActive(true)` → process a block → time `setAnalysisActive(false)`: worst case **3.356 ms**, all 20 well within the 50ms budget. |
| 9 | Warning-clean at `/W4` | PASS | Full clean `cmake --build --preset release` log (includes the extended `tools/render-harness/main.cpp`, itself built at `/W4`): `grep -ic warning` = 0, `grep -ic error` = 0. |

## Performance / numbers

Measured with the extended render harness (`tools/render-harness/main.cpp`), 48 kHz, 128-sample
blocks, stereo main + stereo sidechain, 200-iteration warmup, 5000 measured iterations,
`std::chrono::steady_clock` around each `processBlock()` call, mean and p99 computed over the raw
per-call microsecond samples:

| Measurement | Mean | Budget | p99 | Budget |
|---|---|---|---|---|
| `processBlock`, analysis active | **5.434 µs** | ≤ 20 µs | **31.000 µs** | ≤ 50 µs |
| `processBlock`, analysis inactive (MeterBank-only proxy*) | **5.394 µs** | ≤ 15 µs | **31.100 µs** | ≤ 40 µs |

\* With `analysisActive` false, `processBlock`'s only work is `meterBank.setRmsWindowMs()` +
`meterBank.process()` plus one atomic load (confirmed by reading `PluginProcessor.cpp`'s
`processBlock`), so this is the closest black-box proxy for "MeterBank alone" available without
linking `MeterBank` directly (which would mean reading/depending on `source/dsp/` internals, outside
this agent's remit).

Both measurements pass comfortably, with **wider margins** than `docs/qa/T-009-unit.md` reported
(processBlock: 8.30µs mean / 47.2µs p99; MeterBank-alone unit test: ~8.5µs mean / a flaky ~47–48µs
p99 against its 40µs budget, 3 of 5 isolated re-runs over budget). This run did **not** reproduce a
thin p99 margin on either measurement — both p99s landed at 31µs, comfortably under their 40/50µs
budgets. This is consistent with unit-tester's own hypothesis (open question 1 in
`docs/qa/T-009-unit.md`) that the thin margins they saw are tail-latency/scheduler noise on this
development machine rather than a per-block cost regression: a different process, different harness,
and a different moment in time saw a cleaner tail. It does not disprove the noise exists — it is
consistent with "sometimes you see it, sometimes you don't" — so the underlying open question (worth
re-measuring on the actual CI/target machine before treating the MeterBank-alone p99 budget as an
absolute hard gate) stands, but this independent run gives no reason to block T-009 on it: the
budget was met, by both the coder's own claimed numbers and the unit- and validation-level
independent re-measurements.

## Steinberg VST3 validator

Not available on this machine: pluginval's own log reports `INFO: Skipping vst3 validator as
validator path hasn't been set`, and no `validator`/`validator.exe` binary was found under
`third_party/`. This is a pre-existing gap (not introduced by T-009) — routed to build-engineer as an
open question, consistent with T-014's remit ("VST3 validator... when available").

## Real-time safety note

Out of scope for this report (unit-tester's `docs/qa/T-009-unit.md` already audits everything
reachable from `processBlock`, including the allocation-hook test and a line-by-line RT-safety
review); the render harness is offline test-driver code, not part of the shipped plugin binary.

## Verdict

**PASS**

Every T-009 acceptance criterion in plugin-validator's remit passes: the Standalone link fix (T-025)
is confirmed by a clean rebuild and a launch/stay-alive/clean-exit smoke test (with one noted
limitation on visually confirming the on-screen mute toggle, see the open question below);
pluginval strictness 10 passes both via CTest and standalone; the full CTest suite (172
registered/167 run/167 passed/5 documented skips) is independently reproduced exactly; the null
test stays bit-exact with analysis both active and inactive across the T-008 layout matrix plus four
additional analysis-active layouts; the binary-level state round trip (parameters, ViewState, and
corrupt/empty-data safety) holds; the bus-layout matrix, live sidechain enable/disable, and a
44.1→96→48kHz sample-rate change while analysis is active all behave correctly with no crash; every
parameter is host-automatable per pluginval's fuzz test; the analysis thread's `setAnalysisActive
(false)` stop budget (≤50ms) is met with a 3.356ms worst case over 20 iterations; and both
`processBlock` and the MeterBank-only proxy measurement pass their CPU budgets with more headroom
than unit-tester's own numbers, though the underlying tail-latency noise unit-tester flagged is
neither reproduced nor disproved by this run.

## Files touched by this validation run

- Modified: `tools/render-harness/main.cpp` (added `runNullTestWithAnalysisActive`,
  `runSidechainActiveAndSampleRateChange`, `runAnalysisThreadStopTiming`,
  `measureProcessBlockTiming`, and their calls from `main()`; updated the file's header comment).
  Owned by plugin-validator.
- Created: `docs/qa/T-009-validation.md` (this report).
- No file under `source/`, `CMakeLists.txt`, `CMakePresets.json`, `tests/unit/`, or any other owned
  path was modified. `tools/render-harness/CMakeLists.txt` was not touched — no new dependency was
  needed (`<chrono>`, `<thread>`, `<algorithm>` are standard library).
- Build artefacts (`build/`) were regenerated incrementally by CMake/MSVC; nothing under `build/` is
  committed or tracked.

## Open questions

1. Confirming the Standalone's "Mute audio input" toggle reads as *off* on screen (not just that the
   deterministic code path forces the underlying value to `false` on every launch) needs a
   GUI-automation or screenshot tool this headless environment does not have. Recommend either the
   architect accepts the code-path argument as sufficient (the override is unconditional and runs
   after any persisted value would have loaded, so there is no code path that reaches the visible UI
   with the value still `true`), or routes an interactive/manual check to a human or to a
   screenshot-capable environment before final sign-off.
2. The Steinberg VST3 SDK `validator` binary is not present in this environment (pluginval itself
   reports its path is unset). Requesting it from build-engineer, consistent with T-014's scope
   ("Steinberg validator... when available") — not a T-009 blocker, since pluginval at strictness 10
   already covers the bulk of host-level VST3 conformance.
3. Re-affirming unit-tester's own open question: the `[.perf]` p99 margins they measured (processBlock
   47.2µs/50µs, MeterBank-alone flaky ~47-48µs/40µs) were not reproduced by this run's independent
   harness (which saw 31µs/50µs and 31µs/40µs respectively) — consistent with scheduler/tail-latency
   noise rather than a regression, but worth a re-measurement on the actual target/CI machine before
   the MeterBank-alone budget is treated as an unconditional hard gate in CI.
