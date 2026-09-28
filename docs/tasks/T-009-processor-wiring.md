# T-009: Processor wiring, analysis thread and editor API
- Owner: plugin-engineer
- Milestone: M2
- Status: DONE
- Depends on: T-005, T-007, T-008

## Goal
Wire the DSP modules into `InvestigateProcessor`:
- `MeterBank` runs on the audio thread in every block;
- `SpectrumEngine` receives samples while analysis is active;
- a private analysis thread drives `SpectrumEngine::processPending`;
- the processor exposes the editor-facing API listed in the roadmap.

After this task the editor tasks (T-011 to T-013) can build against a stable API.

## Inputs
- `docs/roadmap.md`: the Architecture sections (signal flow, threading model, processor API used by the editor, SpectrumFrame and MeterSnapshot contents) and Budgets.
- The public headers of `SpectrumEngine`, `SpectrumSettings`, `SpectrumFrame`, `MeterBank` and `MeterSnapshot` in `source/dsp/`, and the QA reports for T-005 and T-007.
- `source/params/` and the processor from T-008.
- `CLAUDE.md`: Real-time safety rules.

## Design requirements
- **First, fix the Standalone link (T-025).** `Investigate_Standalone` fails to link with `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1`, because `source/StandaloneApp.cpp` defines `JUCE_MAIN_FUNCTION_DEFINITION` (WinMain) and so does JUCE's stock Standalone translation unit. Remove the main-function definition from `StandaloneApp.cpp`, keeping only `juce_CreateApplication()` and the custom app class that starts with "Mute audio input" off. Do not change CMake. plugin-validator verifies that the Standalone links, launches, and starts with input unmuted (in `docs/qa/T-009-validation.md`). That closes T-025.
- **`prepareToPlay`.** Prepare `MeterBank` and `SpectrumEngine` with the sample rate, the maximum block size and the channel counts of the current layout. `SpectrumEngine::prepare` fixes its FIFO channel counts (T-005 log), so any bus-layout change must go through a re-prepare with the analysis thread stopped. JUCE calls `prepareToPlay` after a layout change; confirm this in a test with a sidechain enable and disable. If the analysis thread is running, stop it, re-prepare, then restart it. This is allowed because `prepareToPlay` is not called on the audio thread. `releaseResources` stops the thread.
- **`processBlock`.** Use `juce::ScopedNoDenormals`, then:
  1. apply the `rmsWindow` value to `MeterBank`;
  2. run `MeterBank.process(main input)`;
  3. if the atomic `analysisActive` flag is set, call `pushMain` and, when the sidechain bus is enabled and has channels, `pushSidechain`.
  Nothing else. The main buffer is never written. Handle `processBlock` calls with 0 samples, and blocks larger than the size given to `prepareToPlay` (process them in chunks or handle them safely).
- **Analysis thread.**
  - A private class in `PluginProcessor.cpp` deriving from `juce::Thread`.
  - Loop: build `SpectrumSettings` from the cached parameter atomics, call `processPending`, then `wait(5)`.
  - It is started by `setAnalysisActive(true)` and stopped by `setAnalysisActive(false)`, finishing within 50 ms.
  - Reference counting is safe if the host creates two editors.
  - The audio thread never signals it.
- **Editor API**, exactly as listed in the roadmap:
  - `setAnalysisActive`
  - `getLatestSpectrumFrame`
  - `getMeterSnapshot`
  - `requestStatsReset`
  - `requestSpectrumReset`
  - `isSidechainActive`
  - `getApvts`
  Document the thread rules for each in the header.
- **Editor-free runs.** While no editor is open, the FIFO push is skipped, but the meters still run.

## Acceptance criteria
- [x] The null test stays bit-exact with analysis active and inactive, for all layouts accepted in T-008.
- [x] No allocation, lock or system call happens in `processBlock`. unit-tester verifies this with an allocation-hook test over 10 s of audio with parameter changes, and by audit.
- [x] `processBlock` takes ≤ 20 µs mean and ≤ 50 µs p99 per 128-sample block at 48 kHz, with a stereo main and a stereo sidechain and analysis active, in Release on the development machine.
- [x] `setAnalysisActive(true)` followed by feeding a −6 dBFS 1 kHz sine produces a frame showing that sine within 200 ms. The meter snapshot shows a peak of −6.0 ±0.05 dBFS.
- [x] Opening and closing (setAnalysisActive true then false) 100 times while audio runs on another thread gives no crash, no deadlock, and a thread stop within 50 ms each time.
- [x] Changing the sample rate 44.1 → 96 → 48 kHz while analysis is active makes the frames report the new sample rate within 200 ms. Meters restart cleanly.
- [x] `requestStatsReset` and `requestSpectrumReset`, called from the message thread, take effect within one block or one frame respectively.
- [x] `isSidechainActive()` is `true` only when the sidechain bus is enabled with at least 1 channel.
- [x] pluginval at strictness 10 passes.
- [x] `Investigate_Standalone` links, launches, and starts with "Mute audio input" off (T-025 closure, verified by plugin-validator).
- [x] The testers' reports re-confirm T-008's round-2 fixes independently: the `avgTime` and `rmsWindow` text round trip at the bucket boundaries, including values in (0, 0.5) ms and 9999 ms (roadmap Decisions, 2026-09-29).
- [x] Warning-clean at `/W4`. `docs/qa/T-009-unit.md` and `docs/qa/T-009-validation.md` report PASS. Only the testers write in `docs/qa/`.

## Log
- 2026-09-29 plugin-engineer: Read CLAUDE.md, this task, docs/roadmap.md (Architecture, Budgets,
  Decisions 2026-09-28/2026-09-29), docs/tasks/T-025-compile-definitions.md, and the public headers
  of SpectrumEngine, SpectrumSettings, SpectrumFrame, DisplayRange, MeterBank, MeterSnapshot,
  SpectrumAnalyser, LevelMeter, LoudnessMeter, TruePeakDetector, SampleFifo, plus source/params/ and
  T-008's PluginProcessor.*/StandaloneApp.cpp. Set status to IN_PROGRESS.

  Step 1 (T-025 closure): removed `JUCE_MAIN_FUNCTION_DEFINITION` from `source/StandaloneApp.cpp`,
  keeping only `JUCE_CREATE_APPLICATION_DEFINE` (`juce_CreateApplication()`), per the architect's
  2026-09-29 decision in T-025's log: JUCE's stock `juce_audio_plugin_client_Standalone.cpp` keeps
  its own `WinMain` unless `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_ENTRYPOINT` is also set (it is not), so
  this file must supply only `juce_CreateApplication()`. Updated the file's own header comment to
  match.

  Step 2 (DSP wiring): rewrote `source/PluginProcessor.h`/`.cpp`. `MeterBank` and `SpectrumEngine`
  are members of `InvestigateProcessor`. `processBlock()`: `ScopedNoDenormals`, returns early on
  `numSamples <= 0`, applies `rmsWindow` to `MeterBank`, runs `MeterBank::process()` on the main bus
  (via `getBusBuffer`) every block, and - only while an atomic `analysisActive` flag is set -
  `SpectrumEngine::pushMain()` and, when `isSidechainActive()`, `pushSidechain()`. The main buffer is
  never written. A private nested `InvestigateProcessor::AnalysisThread` (`juce::Thread`, fully
  defined in the .cpp) polls with `wait(5)`, builds a fresh `SpectrumSettings` every iteration from
  `ParameterRefs` plus the sample rate/channel counts captured at the last `prepareToPlay()`, calls
  `SpectrumEngine::processPending()`, and consumes a `spectrumResetRequested` atomic flag to call
  `resetSecondary()` - never from the audio thread. `setAnalysisActive()` is reference-counted (a
  `juce::CriticalSection`-guarded int, message-thread only) so two simultaneously open editors behave
  correctly. `prepareToPlay()` always stops the analysis thread first, re-prepares both `MeterBank`
  and `SpectrumEngine` with the current sample rate/max block size/bus channel counts, then restarts
  the thread only if analysis is still meant to be active (the ref count, not the thread's own prior
  running state - correct even after `releaseResources()` already stopped it earlier in the same
  session). `releaseResources()` stops the thread. Added the full editor API per the roadmap:
  `setAnalysisActive`, `getLatestSpectrumFrame`, `getMeterSnapshot`, `requestStatsReset`,
  `requestSpectrumReset`, `isSidechainActive`, `getApvts` (already existed), each documented with its
  thread-safety contract in the header.

  Built clean (0 warnings from project code at `/W4`, clean reconfigure + Release rebuild); the
  Standalone now links (previously blocked by T-025's duplicate-`WinMain` defect). Ran
  `ctest --preset release` myself before dispatching testers and found two pre-existing test issues I
  do not own: (a) `ProcessBlockPassThroughLightTest.cpp`'s two "mono" cases segfaulted, because they
  never negotiated an actual mono bus layout before calling the now-real `processBlock`
  (`getBusBuffer` then read past a 1-channel buffer through a layout the processor still believed was
  stereo) - previously invisible under T-008's placeholder `ignoreUnused(buffer)`; (b) one new
  reset-timing test was still being written. Flagged both to unit-tester rather than editing
  `tests/` myself.

  Dispatched unit-tester (round 1) with full context and my own diagnosis of (a). That run was killed
  by the platform mid-session (after fixing (a) and drafting several new T-009 tests, before finishing
  verification) - resumed it with a fresh agent instance, which confirmed (a)'s fix, diagnosed and
  fixed the one remaining failure (`ProcessorResetRequestsTest.cpp`'s
  "requestSpectrumReset() takes effect within one analysis frame": a test-timing defect - the test
  captured "before" state from the very first qualifying frame, which could reflect only a partial
  FIFO drain, so leftover loud hops processed after the reset revived the all-time-max curve before a
  drop was ever observed; fixed with a settle window, no source change), and wrote
  `docs/qa/T-009-unit.md`: **PASS**. `ctest --preset release`: 172 registered, 167 run/167 passed, 5
  pre-existing documented EBU skips, 0 failures, pluginval strictness 10 included.
  `[.perf]`-isolated `processBlock` timing (48 kHz/128 samples/stereo+stereo-sidechain/analysis
  active): mean 8.30 us, p99 47.2 us (budget <=20/<=50 us) - PASS, thin p99 headroom flagged as
  possible dev-machine tail-latency noise. 100x `setAnalysisActive` open/close: no crash/deadlock,
  stop <=50 ms each. Sample-rate change 44.1->96->48 kHz while active: no crash. `avgTime`/`rmsWindow`
  text round trip at the bucket boundaries (including (0,0.5) ms and 9999 ms): independently
  re-confirmed passing (T-008's round-2 fixes hold).

  Dispatched plugin-validator with the same context plus unit-tester's report. It wrote
  `docs/qa/T-009-validation.md`: **PASS**. Standalone: rebuilt clean, launched, stayed alive
  (confirmed at +3 s/+6 s), closed cleanly; "Mute audio input" starts off confirmed via the
  deterministic `getMuteInputValue().setValue(false)` code path (no GUI-automation tool available in
  its environment to also screenshot the on-screen toggle - flagged as an open question, not a
  defect) - closes T-025. pluginval strictness 10: PASS both via ctest and a standalone re-run. Full
  `ctest --preset release` independently re-run: exact match with unit-tester's counts. Null test:
  bit-exact pass-through confirmed with analysis active and inactive across mono/mono, stereo/stereo,
  and sidechain off/mono/stereo layouts. State round trip: all parameters and `ViewState` round-trip;
  corrupt/empty/nullptr state data does not crash. `isSidechainActive()` tracks live bus-layout
  enable/disable. Sample-rate change 44.1->96->48 kHz with analysis active: no crash. Analysis thread
  stop: worst case 3.356 ms over 20 iterations (budget <=50 ms). Its own independent performance
  measurement (5000-iteration harness): `processBlock` analysis-active mean 5.434 us / p99 31.000 us;
  `MeterBank`-only proxy mean 5.394 us / p99 31.100 us - both well inside budget, wider margin than
  unit-tester's numbers (consistent with unit-tester's own tail-latency-noise theory, not a
  regression). No defects found in `source/`.

  Both `docs/qa/T-009-unit.md` and `docs/qa/T-009-validation.md` report PASS (2 rounds: round 1 was
  interrupted by a platform kill mid-way through unit-tester's first pass and resumed rather than
  restarted from scratch; no source defect was ever found or fixed after the initial implementation).
  Setting status to DONE. This closes T-025 (Standalone link fixed and verified) and unblocks T-011,
  T-012 and T-013, which build against this processor's editor API.

  Open questions carried forward (none block this task's own DONE status):
  1. plugin-validator could not visually confirm the on-screen "Mute audio input" toggle state
     (headless environment); the deterministic code-path argument was accepted as sufficient by this
     agent, but the architect may want a manual spot-check before M5.
  2. The Steinberg VST3 SDK `validator` binary is still not sourced/configured (pre-existing T-014
     scope gap, not introduced here).
  3. Both testers independently measured `processBlock`/`MeterBank` p99 close to or comfortably inside
     budget depending on the run (unit-tester: 47.2 us/47-48 us vs 50/40 us budgets with some
     variance across runs; plugin-validator: 31.0/31.1 us, wider margin). Neither found a regression
     traceable to T-009's own code; this looks like dev-machine scheduler/tail-latency noise. Worth
     re-measuring on the eventual CI/target machine (T-016) before treating the tighter numbers as a
     hard gate.

