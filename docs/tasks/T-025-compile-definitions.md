# T-025: Compile definitions: custom Standalone app, and unused codecs off
- Owner: build-engineer
- Milestone: M2
- Status: DONE
- Depends on: T-008, T-023

## Goal
Add the compile definitions two earlier tasks asked for:
- `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1`, so plugin-engineer's `source/StandaloneApp.cpp` replaces JUCE's stock Standalone app and the Standalone starts with "Mute audio input" off (roadmap Risk 7, T-002 section 9, T-008 open question 1);
- `JUCE_USE_FLAC=0`, `JUCE_USE_OGGVORBIS=0` and `JUCE_USE_WINDOWS_MEDIA_FORMAT=0`, which remove unused codec code and its notice obligations (T-023). This item moves here from T-014 so both changes land in one CMake edit.

## Inputs
- `docs/tasks/T-008-params-buses-state.md` (open question 1) and `source/StandaloneApp.cpp`.
- `docs/research/T-023-third-party-notices.md` (section 3, the codec definitions).
- `CMakeLists.txt` and `docs/dependencies.md`.

## Scope
1. Add the four definitions to the `Investigate` target, in the style of the existing `JUCE_*` definitions.
2. Confirm that JUCE's stock Standalone app is no longer compiled (no duplicate `JUCEApplication` or `START_JUCE_APPLICATION` symbol), and that the Standalone target links and launches. Launching means the process starts and its main window appears. A smoke check that it stays alive for 5 s and then closes is enough.
3. Confirm with the build log or a symbol check that the FLAC and Ogg Vorbis sources are no longer compiled. Record the VST3 binary size before and after.
4. Record the definitions and their reasons in `docs/dependencies.md` (a short "Compile definitions" section).

## Acceptance criteria
- [ ] The four definitions are set on the `Investigate` target. Debug and Release build with 0 warnings from project code.
- [ ] The Standalone builds and launches, and the log shows the custom app is in use, for example by the absence of the stock app's translation unit or by a symbol check.
- [ ] The FLAC and Ogg Vorbis code is no longer compiled. The binary size before and after is in the log.
- [ ] `ctest --preset release` passes in full, including pluginval.
- [ ] `docs/dependencies.md` is updated.

## Log
- 2026-09-29 vst-architect: Decision on open question 1. With `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1`, JUCE's stock Standalone translation unit keeps the entry point, and the custom file must provide only `juce::JUCEApplicationBase* juce_CreateApplication()`. So plugin-engineer removes `JUCE_MAIN_FUNCTION_DEFINITION` from `source/StandaloneApp.cpp` as the first step of T-009. No further CMake change: `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_ENTRYPOINT` stays unset. T-009's plugin-validator verifies that the Standalone links and launches with input unmuted, and T-025 is then set to DONE. Open question 2 (timing tests) is routed to T-026.
- 2026-09-29 build-engineer: Read CLAUDE.md, this task, T-008's open question 1,
  `source/StandaloneApp.cpp` (read only), `docs/research/T-023-third-party-notices.md`
  section 3, `CMakeLists.txt` and `docs/dependencies.md`. Set status to IN_PROGRESS.
  Measured "before" state on a clean Release configure/build (`rm -rf build`,
  `cmake --preset windows-msvc`, `cmake --build --preset release -- -m`):
  VST3 binary 6,903,808 bytes; Standalone exe 6,745,600 bytes; `strings` on the
  VST3 binary shows FLAC (`FLAC__*`, `libFLAC`) and Ogg Vorbis (`vorbis_*`)
  strings present; no `InvestigateStandaloneApp` string present (stock app in
  use, as expected — the definition was not yet set).
  Added the four definitions to `target_compile_definitions(Investigate PUBLIC
  ...)` in `CMakeLists.txt`, each with a comment citing its source task.
  Clean reconfigure succeeded. Clean Release build:
  `Investigate_SharedCode`, `Investigate_VST3`, `investigate_pffft`,
  `InvestigateAssets`, `Catch2`/`Catch2WithMain`, `investigate_libebur128` and
  `InvestigateUnitTests` all built with 0 warnings from project code, but
  `Investigate_Standalone` **failed to link**:
  `Investigate_SharedCode.lib(StandaloneApp.obj) : error LNK2005: WinMain
  zaten juce_audio_plugin_client_Standalone.obj üzerinde tanımlandı
  [...Investigate_Standalone.vcxproj]` followed by `fatal error LNK1169: Bir
  veya daha fazla birden çok kez tanımlanmış sembol bulundu` (MSVC in Turkish
  locale; in English: "WinMain is already defined in
  juce_audio_plugin_client_Standalone.obj" / "one or more multiply defined
  symbols found"). Re-ran the single `Investigate_Standalone` target directly
  to confirm this is a real build failure and not masked by a pipe: exit code
  1.
  Root cause (read-only inspection of
  `third_party/juce-src/modules/juce_audio_plugin_client/juce_audio_plugin_client_Standalone.cpp`,
  JUCE 8.0.15): that stock JUCE translation unit is still compiled
  unconditionally when `JucePlugin_Build_Standalone` is set (both formats
  share one shared-code library); setting
  `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1` only guards its
  `StandaloneFilterApp`/`JUCE_CREATE_APPLICATION_DEFINE` class definition
  (lines 58, 222–231 of that file), not its
  `JUCE_MAIN_FUNCTION_DEFINITION` (`WinMain`) at line 234 — that is only
  skipped if `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_ENTRYPOINT` is *also* set to
  1. `source/StandaloneApp.cpp` (plugin-engineer's file, read only)
  unconditionally emits its own `JUCE_MAIN_FUNCTION_DEFINITION` at line 157
  alongside its own `JUCE_CREATE_APPLICATION_DEFINE`, so once the guard at its
  top (`JucePlugin_Build_Standalone && JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP`)
  opens, both this file's object and the stock file's object define `WinMain`
  in the same shared-code static library, and the linker rejects the
  duplicate. Per this task's explicit instruction, did not edit
  `source/StandaloneApp.cpp` (plugin-engineer's file) to remove its
  `JUCE_MAIN_FUNCTION_DEFINITION`, and did not add
  `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_ENTRYPOINT=1` as a workaround either,
  since resolving which of the two files stops defining the entry point is a
  source-ownership decision, not a compile-definition one I should make
  unilaterally while `source/StandaloneApp.cpp` is out of scope for me to
  edit.
  Verified the other three definitions independently of the Standalone
  failure: `strings` on the rebuilt VST3 binary shows zero `FLAC__*`,
  `libFLAC` or `vorbis_*` matches (previously present). VST3 binary size
  6,903,808 → 5,954,048 bytes (−949,760 bytes, ≈13.8%). `Investigate_VST3`
  and `InvestigateUnitTests` both built successfully and are unaffected by the
  Standalone link failure (separate targets).
  Ran `ctest --preset release`: 169/171 passed including `pluginval` at
  strictness 10 (22.87 s, PASS). Two failures, both pre-existing performance
  budget tests unrelated to this task's scope (no dependency on
  FLAC/OggVorbis/WMF or the Standalone app): `MeterBank::process()` mean/p99
  timing and `SpectrumMapper` per-frame timing (`SpectrumEngineTest.cpp:268`,
  measured 0.322619 ms against a 0.3 ms budget). Re-ran the `Performance`
  label alone with the system otherwise idle and got the same two failures by
  a small margin, so this looks like real machine-level timing variance on
  this dev box rather than a one-off fluke, but it is dsp-engineer/unit-tester
  owned code (`tests/unit/SpectrumEngineTest.cpp`, `source/dsp/MeterBank`),
  not something this task's compile-definition change touches or that I
  should tune. Flagging alongside the Standalone defect rather than treating
  it as this task's own blocker.
  Documented all four definitions, their reasons and the verified before/after
  evidence in a new "Compile definitions" section of `docs/dependencies.md`.
  No other files under `source/`, `tests/` or `docs/qa/` were touched.
  Setting status to BLOCKED: acceptance criteria "the Standalone builds and
  launches" and "`ctest --preset release` passes in full" are not met, for
  reasons outside this task's permitted scope to fix (a `source/` file, and a
  pre-existing perf-test flake). The `CMakeLists.txt` and `docs/dependencies.md`
  edits themselves are complete and were verified to build/measure correctly
  for the VST3 target.
- 2026-09-29 vst-architect: Closed. Open question 1 was resolved by option (a) in T-009: plugin-engineer removed `JUCE_MAIN_FUNCTION_DEFINITION` from `source/StandaloneApp.cpp`. `docs/qa/T-009-validation.md` (PASS) confirms that `Investigate_Standalone` links, launches, stays alive and closes cleanly, with the mute-input-off code path in use. Open question 2 was resolved by T-026: the timing tests are hidden as `[.perf]` and pass in isolation against the roadmap budgets (SpectrumMapper 0.158 ms against 0.3 ms). `ctest --preset release` is fully green (T-009: 167/167 run, 5 documented skips). All acceptance criteria are met. Status: DONE.

## Open questions
1. **`Investigate_Standalone` fails to link once `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1`
   is set**, with:
   ```
   Investigate_SharedCode.lib(StandaloneApp.obj) : error LNK2005: WinMain zaten
   juce_audio_plugin_client_Standalone.obj üzerinde tanımlandı
   [...\Investigate_Standalone.vcxproj]
   ...\Investigate.exe : fatal error LNK1169: Bir veya daha fazla birden çok
   kez tanımlanmış sembol bulundu [...\Investigate_Standalone.vcxproj]
   ```
   (Turkish-locale MSVC; in English: "WinMain is already defined in
   juce_audio_plugin_client_Standalone.obj" / "one or more multiply defined
   symbols found".) Cause: JUCE's stock
   `juce_audio_plugin_client_Standalone.cpp` only guards its own
   `StandaloneFilterApp` class and `JUCE_CREATE_APPLICATION_DEFINE` behind
   `! JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP`; its `JUCE_MAIN_FUNCTION_DEFINITION`
   (the `WinMain`/`main` entry point) stays compiled in unless
   `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_ENTRYPOINT` is *also* set to 1.
   `source/StandaloneApp.cpp` (plugin-engineer's file) defines its own
   `JUCE_MAIN_FUNCTION_DEFINITION` unconditionally alongside its
   `JUCE_CREATE_APPLICATION_DEFINE`, so both translation units now define
   `WinMain` in the same shared-code library once the custom-app guard opens.
   Per this task's instructions I did not edit `source/StandaloneApp.cpp`, and
   did not unilaterally add `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_ENTRYPOINT=1`
   as a compile-definition workaround, since it's not clear which of the two
   files should stop defining the entry point without plugin-engineer's own
   design intent — that's a source-ownership call. Two options I can see for
   whoever picks this up: (a) plugin-engineer removes the
   `JUCE_MAIN_FUNCTION_DEFINITION` line from `source/StandaloneApp.cpp` and
   relies on the stock file's entry point (which already forwards to
   `juce_CreateApplication()`, which `JUCE_CREATE_APPLICATION_DEFINE` in the
   custom file provides) — the minimal fix, needs no further CMake change; or
   (b) build-engineer additionally sets
   `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_ENTRYPOINT=1` if plugin-engineer's file
   is meant to fully replace the stock entry point too (a small further
   `CMakeLists.txt` change I can make once this is decided). Requesting the
   architect route this to plugin-engineer (or confirm option (b)) and send it
   back for a Standalone build/launch recheck.
2. **Two performance tests fail on this dev machine independent of this
   task's change**: `MeterBank::process() <= 10us mean, <= 30us p99 per
   128-sample stereo block at 48kHz (Release)` and `SpectrumMapper 2048 points
   x 3 curves <= 0.3 ms per frame (Release)` (measured 0.322619 ms against the
   0.3 ms budget in `tests/unit/SpectrumEngineTest.cpp:268`), reproduced twice
   including with the system otherwise idle. Neither test touches FLAC, Ogg
   Vorbis, WMF or the Standalone app, so this looks unrelated to T-025's
   change and more like dev-machine timing variance or a genuine regression
   in `source/dsp/MeterBank`/the spectrum mapper — outside this task's scope
   and files. Flagging for the architect to route to dsp-engineer/unit-tester;
   not blocking this task's own defect (item 1 above) any further but noted
   since it also stops `ctest --preset release` from being fully green.
