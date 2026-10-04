# T-041 unit QA: processor requests visible point count

Verdict: PASS

## 1. Release build, /W4
- Command: `touch source/PluginProcessor.cpp; cmake --build --preset release` (preset `windows-msvc`, binaryDir `build/`, no in-source build).
- PluginProcessor.cpp was recompiled; the full log contains 0 occurrences of "warning" and 0 errors. Project sources compile at /W4 (CMakeLists.txt line 196).

## 2. ctest
- Command: `cd build && ctest -C Release --timeout 300`
- Result: 100 % passed, 0 failed of 395 (includes pluginval, 16.5 s). 8 Tech 3341/3342 cases report "not run (Skipped)"; they are existing skips unrelated to this change.
- The T-009, T-036 and T-039 tests (SpectrumEngine, SpectrumMapper, frame and chunking tests, e.g. #43, #198-#205) are in the passing set. `git status` shows no changes under `tests/`, so they ran unedited.
- No timing assertions were added to the default run.

## 3. Diff review
- `git diff source/`: one line, PluginProcessor.cpp:105, `SpectrumFrame::maxPoints` -> `SpectrumFrame::maxVisiblePoints`.
- `grep maxPoints|maxVisiblePoints source/PluginProcessor.*`: no other use of either constant.
- SpectrumEngine.cpp:248 does `numVisible = jlimit(1, SpectrumFrame::maxVisiblePoints, settings.numPoints)`. The old value (3072) was clamped to 2048, so the engine received the same effective count and published frames are identical. No behaviour change.

## Processes
No cmake, MSBuild, cl, link, ctest or InvestigateUnitTests process of mine remains.
