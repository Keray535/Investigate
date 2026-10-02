# T-030: Remove the in-source build at the repository root and guard against it
- Owner: build-engineer
- Milestone: M3
- Status: DONE
- Depends on: T-029

## Goal
An agent ran CMake with the repository root as the build directory. The root now holds a complete Visual Studio build tree next to the sources. Remove it, and make CMake refuse in-source builds, so the root stays clean and nobody builds or measures from a stale tree by accident.

## Inputs
- The T-029 commit report (2026-10-03). The untracked, non-ignored leftovers at the root are:
  - `CTestTestfile.cmake` and `cmake_install.cmake`;
  - `Investigate_artefacts/`, `juce_binarydata_InvestigateAssets/`, `vst3_helpers/` and `x64/`;
  - in `tests/`: `CTestTestfile.cmake`, `cmake_install.cmake` and `InvestigateUnitTests-*_include.cmake`;
  - in `tools/render-harness/`: `CTestTestfile.cmake` and `cmake_install.cmake`.
- Ignored leftovers of the same in-source configure, also at the root: `CMakeCache.txt`, `CMakeFiles/`, `Investigate.sln` and the root-level `*.vcxproj` and `*.vcxproj.filters` files (`ALL_BUILD`, `ZERO_CHECK`, `INSTALL`, `RUN_TESTS`, `Investigate*`, `InvestigateAssets`, `investigate_pffft`). There may be more in `tests/` and `tools/render-harness/`.
- `.gitignore`, `CMakeLists.txt` and `CMakePresets.json`.

## Scope
1. **Inventory first.** List every in-source build artefact at the root, in `tests/` and in `tools/render-harness/` using `git status --porcelain --ignored`. Write the list into the log before deleting anything.
2. **Delete only those artefacts**, by explicit path.
   - Never run `git clean -x`, `git clean -X` or any broad clean. They would also delete `third_party/`, `build/`, `CLAUDE.md` and `.claude/`, which are ignored but must stay.
   - Do not touch `build/`, `third_party/`, `research/_cache/`, `CLAUDE.md`, `.claude/`, or any tracked file.
   - If a path's origin is unclear, leave it and report it.
3. **In-source guard.** Near the top of the root `CMakeLists.txt`, add a check that stops configuration with a clear `FATAL_ERROR` when `CMAKE_BINARY_DIR` equals `CMAKE_SOURCE_DIR`. The message should tell the user to use `cmake --preset <name>` or `-B build`. Note that CMake writes `CMakeCache.txt` and `CMakeFiles/` before the check runs; mention in the message that the user should delete them.
4. **`.gitignore`.** Add patterns for the non-ignored in-source outputs (`CTestTestfile.cmake`, `cmake_install.cmake`, `*_include.cmake`, `Investigate_artefacts/`, `juce_binarydata_*/`, `vst3_helpers/` and `x64/`), so a future accident cannot be committed.
5. **Commit and push** the `CMakeLists.txt` and `.gitignore` changes, with this task file and the roadmap, to `origin main`. The deletions themselves touch only untracked files, so there is nothing to commit for them.

## Acceptance criteria
- [ ] After cleanup, `git status --porcelain --ignored` shows no in-source build artefacts. The only ignored entries left are the intended ones (`build/`, `third_party/`, `CLAUDE.md`, `.claude/`, and `research/_cache/` if present).
- [ ] `cmake -S . -B .` (or running `cmake .` from the root) fails with the new message. The test leftovers it creates (`CMakeCache.txt`, `CMakeFiles/`) are removed again afterwards.
- [ ] `cmake --preset windows-msvc` followed by `cmake --build --preset release` still succeeds, with 0 warnings from project code, and `ctest --preset release` passes. This must happen in the normal `build/` directory.
- [ ] The commit is pushed. Report the hash in the log.

## Log
- 2026-10-03 vst-architect: Created from the T-029 commit report. Runs before T-013, because both build C++.
- 2026-10-02 build-engineer: Status IN_PROGRESS. Inventory from `git status --porcelain --ignored` (none of it tracked), to delete by explicit path:
  - Root untracked: `CTestTestfile.cmake`, `cmake_install.cmake`, `Investigate_artefacts/`, `juce_binarydata_InvestigateAssets/`, `vst3_helpers/`, `x64/`.
  - Root ignored: `CMakeCache.txt`, `CMakeFiles/`, `Investigate.sln`, `*.vcxproj` and `*.vcxproj.filters` for ALL_BUILD, INSTALL, Investigate, InvestigateAssets, Investigate_All, Investigate_Standalone, Investigate_VST3, Investigate_rc_lib, Investigate_vst3_helper, RUN_TESTS, ZERO_CHECK, investigate_pffft.
  - Root, not listed by git (empty directories, 0 files each): `ALL_BUILD.dir`, `INSTALL.dir`, `Investigate.dir`, `InvestigateAssets.dir`, `Investigate_All.dir`, `Investigate_Standalone.dir`, `Investigate_VST3.dir`, `Investigate_rc_lib.dir`, `Investigate_vst3_helper.dir`, `RUN_TESTS.dir`, `ZERO_CHECK.dir`, `investigate_pffft.dir`.
  - tests/: `CTestTestfile.cmake`, `cmake_install.cmake`, `InvestigateUnitTests-b12d07c_include.cmake`, `CMakeFiles/`, `INSTALL`, `InvestigateUnitTests`, `RUN_TESTS`, `investigate_libebur128` (`.vcxproj`, `.vcxproj.filters`, `.dir`).
  - tools/render-harness/: `CTestTestfile.cmake`, `cmake_install.cmake`, `CMakeFiles/`, `INSTALL`, `InvestigateEditorCheck`, `InvestigateRenderHarness`, `InvestigateSpectrumCheck`, `RUN_TESTS` (`.vcxproj`, `.vcxproj.filters`, `.dir`).
  - Intentionally kept (ignored on purpose): `build/`, `third_party/`, `research/`, `CLAUDE.md`, `.claude/`.
- 2026-10-02 build-engineer: Deleted every inventoried path by explicit `rm -rf` (no git clean). `git status --porcelain --ignored` afterwards shows only `.claude/`, `CLAUDE.md`, `build/`, `research/`, `third_party/` as ignored. Nothing unclear was left.
- Guard added to root `CMakeLists.txt` (before `project()`, compares REALPATHs of source and binary dirs). `cmake -S . -B .` fails with the FATAL_ERROR message; the resulting `CMakeCache.txt` and `CMakeFiles/` were removed. `.gitignore` extended (also `*.dir/`).
- `cmake --preset windows-msvc`, `cmake --build --preset release` succeed with no warnings reported. `ctest --preset release`: 276 of 277 pass (skips are the EBU fixture tests). One failure, unrelated to this task: test 105 "SpectrumView pacing: a paced second frame is deferred..." (tests/unit/SpectrumViewPacingTest.cpp lines 107 and 111: paints - base is 1, expected 2); reproducible in isolation. It comes from T-029 code or its test and is reported to the architect. A plain `ctest` without `--timeout` once left InvestigateUnitTests.exe hanging; a rerun with `--timeout 120 -j4` finished in 21 s.
