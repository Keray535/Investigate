# T-003: Project scaffold
- Owner: build-engineer
- Milestone: M0
- Status: DONE
- Depends on: none

## Goal
Create the CMake project for Investigate with pinned JUCE 8, Catch2 v3 and pluginval. Deliver a placeholder pass-through plugin that builds as VST3 and Standalone on Windows, passes pluginval, and has a unit test target that runs through CTest. Every later task builds on this scaffold.

## Inputs
- `CLAUDE.md`, sections Technical baseline, Directory layout and ownership, and Code conventions.
- `docs/roadmap.md`, sections Architecture (Identity, Buses and channel layouts), Working rules, and Decisions (entries for the ownership exception, CMake globbing, and CI in M5).
- Development machine: Windows 11 x64, Visual Studio 2022 (MSVC). Git Bash is the shell for the Bash tool.

## Scope
1. **Git.** The folder is not a git repository yet. Run `git init` and add a `.gitignore` covering the build directories, `research/_cache/`, `.vs/`, `out/`, CMake user presets and common IDE files. Do not commit anything, add a remote, or push. The user decides that later.
2. **CMake.** Add a root `CMakeLists.txt` requiring CMake 3.22 or newer, with C++20 and project version 0.1.0. Add `CMakePresets.json` with at least these presets:
   - configure presets: `windows-msvc` (Visual Studio 17 2022, x64)
   - build presets: `debug` and `release` (plus `relwithdebinfo` if convenient)
   - test presets matching the build presets
   Presets must put their binary directories under `build/`.
3. **Dependencies.** Pin JUCE to the latest JUCE 8 release tag, and Catch2 to the latest v3 release tag. Use FetchContent or git submodules in `third_party/`; choose one and justify it in `docs/dependencies.md`. Also pin a pluginval release binary: download it at configure time into the build tree (or into `third_party/`) and verify its checksum. Record every dependency in `docs/dependencies.md` with its version or tag, source URL, license, how it is used (linked, tool only, or test only), and how to update it. Do not add any other dependency.
4. **Plugin target.** Use `juce_add_plugin` with these settings:
   - product name `Investigate`, company name `Investigate Audio`
   - manufacturer code `Ivga`, plugin code `Ivst`
   - bundle ID `com.investigateaudio.investigate`
   - formats `VST3 Standalone`
   - VST3 category `Fx Analyzer`
   - not a synth, no MIDI in or out
   - `COPY_PLUGIN_AFTER_BUILD FALSE`
   Link `juce_audio_utils` and `juce_dsp` plus JUCE's recommended config, LTO and warning flags. Compile project code at `/W4`. Disable `JUCE_WEB_BROWSER`, `JUCE_USE_CURL` and `JUCE_VST3_CAN_REPLACE_VST2`.
5. **Source globbing.** Add sources with `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)`:
   - `source/*.cpp`, `source/dsp/*.cpp`, `source/params/*.cpp` and `source/gui/*.cpp` go into the plugin target.
   - `tests/unit/*.cpp` goes into the unit test target.
   Coders must be able to add files without editing CMake. Document this in a comment in `CMakeLists.txt`.
6. **Assets.** Add a binary data target `InvestigateAssets` built from `assets/ui/**` (svg, ttf, otf, png). Create it only when at least one asset exists, and link it to the plugin target. If no asset exists yet, configuration must still succeed.
7. **Unit test target.** Create `InvestigateUnitTests` using Catch2 v3 with `catch_discover_tests`. It must be able to include and link code from `source/dsp/` and `source/params/` without compiling JUCE twice in conflicting ways. Linking the plugin's shared-code target is acceptable. Explain the chosen approach in `docs/dependencies.md` or in a CMake comment.
8. **pluginval in CTest.** Register a CTest test `pluginval` that runs the pinned pluginval on the built VST3 with `--strictness-level 10` and `--validate-in-process` (or document why not). Label it `validation` so it can be run with `ctest -L validation`.
9. **Placeholder sources (one-time ownership exception, see the roadmap Decisions log).** Create these minimal files:
   - `source/PluginProcessor.h/.cpp`: class `investigate::InvestigateProcessor`. Stereo in and stereo out; mono in and mono out also accepted. `processBlock` leaves the buffer untouched and uses `juce::ScopedNoDenormals`. Zero latency, no parameters yet, and empty state save and restore.
   - `source/PluginEditor.h/.cpp`: class `investigate::InvestigateEditor`, which hosts a `juce::GenericAudioProcessorEditor` as a child at a fixed 600×400 size.
   - `tests/unit/SmokeTest.cpp`: one Catch2 test that instantiates the processor, calls `prepareToPlay(48000, 512)`, processes one block of noise, and checks that the output equals the input.
   After this task is DONE, ownership of these files reverts to plugin-engineer, gui-engineer and unit-tester. Keep the files minimal so there is nothing to undo.
10. **Build instructions.** Document the local build and test commands in `docs/dependencies.md`, or in a short build section of `README.md` if you prefer: configure, build, run the unit tests, and run pluginval.

## Acceptance criteria
- [ ] `git status` works in the project root; `.gitignore` excludes `build/` and `research/_cache/`; no commits, remotes or pushes.
- [ ] `cmake --preset windows-msvc` configures without errors on the development machine.
- [ ] `cmake --build --preset release` builds `Investigate.vst3` and `Investigate.exe` (Standalone) with 0 warnings from project code at `/W4`. JUCE's own warnings may be suppressed only for JUCE sources.
- [ ] `ctest --preset release` runs and passes the smoke test. `ctest --preset release -L validation` runs pluginval at strictness 10 and passes.
- [ ] Adding a new `.cpp` file under `source/dsp/` or `tests/unit/` is picked up on the next build without editing CMake. Verify this with a temporary file, then delete it.
- [ ] Configuration succeeds with an empty `assets/ui/`.
- [ ] `docs/dependencies.md` lists JUCE, Catch2 and pluginval with pinned versions or tags, licenses, usage type, and update instructions.
- [ ] The Debug build also compiles and the smoke test passes in Debug.

## Log
- 2026-09-28 build-engineer: Toolchain check (first build task): CMake 4.3.0-rc2, Git 2.49.0.windows.1, gh 2.92.0, Visual Studio 2022 Community 17.14.6 (MSVC 19.44.35211.0 / toolset 14.44.35207), Windows SDK 10.0.26100.0. Ninja not installed and not needed (CMakePresets uses the Visual Studio 17 2022 generator only). Nothing missing for this task.
- 2026-09-28 build-engineer: Ran `git init` in the project root (no commits, no remote). Wrote `.gitignore` (build/, third_party/, research/_cache/, .vs/, out/, CMakeUserPresets.json, common IDE files).
- 2026-09-28 build-engineer: Pinned dependencies (checked GitHub releases directly): JUCE `8.0.15` (latest JUCE 8 tag; JUCE 9.x exists but is out of the roadmap's scope), Catch2 `v3.16.0`, pluginval `v1.0.4` (Windows asset `pluginval_Windows.zip`, SHA-256 `c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab`, computed locally since pluginval publishes no official checksum). All three are pre-approved baseline dependencies per CLAUDE.md/roadmap; no other dependency was added. Recorded full details, licenses, usage type and update steps in `docs/dependencies.md`.
- 2026-09-28 build-engineer: Added root `CMakeLists.txt` (CMake ≥3.22, C++20, `project(Investigate VERSION 0.1.0)`, languages CXX and C — JUCE's own CMake requires a C compiler), `CMakePresets.json` (configure preset `windows-msvc`, build/test presets `debug`/`release`/`relwithdebinfo`, binaries under `build/`), and `cmake/DownloadPluginval.cmake`. JUCE and Catch2 are fetched via `FetchContent` with `GIT_TAG` + `GIT_SHALLOW TRUE` into `third_party/` (via `FETCHCONTENT_BASE_DIR`), not vendored/committed; chose FetchContent over a submodule so no extra checkout step is needed and the tag alone gives full reproducibility. Options `PLUGIN_BUILD_TESTS` (ON) and `PLUGIN_BUILD_HARNESS` (ON, but the `tools/render-harness` subdirectory is only added if its CMakeLists.txt exists — reserved for T-014).
- 2026-09-28 build-engineer: `juce_add_plugin(Investigate ...)` with product name Investigate, company Investigate Audio, manufacturer code Ivga, plugin code Ivst, bundle ID com.investigateaudio.investigate, formats VST3+Standalone, VST3 category Fx|Analyzer, not a synth, no MIDI, `COPY_PLUGIN_AFTER_BUILD FALSE`. Linked `juce_audio_utils`, `juce_dsp` and JUCE's recommended config/LTO/warning flags as PUBLIC (needed so `InvestigateUnitTests`, which links this shared-code target, gets the JUCE include paths — documented in `docs/dependencies.md`). Disabled `JUCE_WEB_BROWSER`, `JUCE_USE_CURL`, `JUCE_VST3_CAN_REPLACE_VST2`. Project code compiles at `/W4` via an explicit `target_compile_options` on both the plugin and test targets.
- 2026-09-28 build-engineer: Source globbing via four `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` calls exactly as scoped (source/*.cpp, source/dsp/*.cpp, source/params/*.cpp, source/gui/*.cpp, tests/unit/*.cpp). Verified experimentally that GLOB_RECURSE with a `source/*.cpp`-style pattern already recurses into subdirectories, so the four result lists are merged and `list(REMOVE_DUPLICATES ...)` is applied before `target_sources` to avoid duplicate-symbol link errors; documented this in a CMakeLists.txt comment.
- 2026-09-28 build-engineer: Asset binary-data target `InvestigateAssets` is created only when `assets/ui/**` (svg/ttf/otf/png) is non-empty; verified configuration succeeds with the current empty `assets/ui/`.
- 2026-09-28 build-engineer: `tests/CMakeLists.txt` defines `InvestigateUnitTests` (Catch2 v3, `catch_discover_tests`) linking the `Investigate` shared-code target, and registers CTest test `pluginval` (label `validation`) running the pinned pluginval binary at `--strictness-level 10 --validate-in-process` against `$<TARGET_FILE:Investigate_VST3>`.
- 2026-09-28 build-engineer: One-time ownership exception used to create minimal placeholders: `source/PluginProcessor.h/.cpp` (`investigate::InvestigateProcessor`, stereo-or-mono pass-through, `ScopedNoDenormals`, 0 latency, empty state save/restore), `source/PluginEditor.h/.cpp` (`investigate::InvestigateEditor` hosting a fixed 600x400 `juce::GenericAudioProcessorEditor`), `tests/unit/SmokeTest.cpp` (prepares at 48 kHz/512 samples, processes one block of random noise, asserts bit-exact output). No other product code touched.
- 2026-09-28 build-engineer: Documented build/test/validate commands in `docs/dependencies.md`.
- 2026-09-28 build-engineer: Verified every acceptance criterion by running the commands from a fresh `build/` directory: `cmake --preset windows-msvc` (clean configure, no errors, empty `assets/ui/`), `cmake --build --preset release` (builds `Investigate.vst3` and `Investigate.exe` Standalone, 0 warnings anywhere in the log, including JUCE and Catch2 sources), `ctest --preset release` (2/2 pass: the smoke test and pluginval at strictness 10) and `ctest --preset release -L validation` (pluginval only, passes). `cmake --build --preset debug` also builds warning-clean and `ctest --preset debug` passes both tests. Added a temporary `source/dsp/TempProbe.cpp`, confirmed the next `cmake --build --preset release` reconfigured automatically (CONFIGURE_DEPENDS) and compiled it with no CMakeLists.txt edit, then deleted it and rebuilt cleanly. Confirmed via `git status --ignored` that `build/` and `third_party/` are excluded and nothing has been committed, no remote added.
- 2026-09-28 build-engineer: Open question for the architect — none blocking. Note only: JUCE now has a 9.x line; this task pins JUCE 8.0.15 per the roadmap's explicit JUCE-8 baseline, so no action needed unless the architect later decides to move to JUCE 9.
