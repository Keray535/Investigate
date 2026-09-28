# T-003 validation report (M0 gate)

- Validator: plugin-validator
- Date: 2026-09-28
- Environment: Windows 11 x64 (build 10.0.26200), Visual Studio 2022 Community 17.14.6 (MSVC 19.44.35211.0, toolset 14.44.35207), CMake 4.3.0-rc2, Windows SDK 10.0.26100.0. Project has no commits yet (`git log` reports "no commits yet"), commit hash: none.
- Build type: Release and Debug, both from a clean `build/` directory (removed and reconfigured from scratch for this run; `third_party/` was reused from cache, which is the normal workflow — JUCE/Catch2 fetch is network-gated only on the very first configure).
- Scope: black-box re-verification of T-003's own acceptance criteria and the M0 milestone gate, driven only by the built artefacts, `docs/tasks/T-003-project-scaffold.md`, `docs/roadmap.md` and `docs/dependencies.md`. `source/` and `CMakeLists.txt` were read only to identify parameter/target names for driving the build, not to judge correctness.

## Commands run (in order, from project root `C:\Users\Keray\Desktop\Plugin_dev`)

```
git status                                    (pre-check, no build)
rm -rf build
cmake --preset windows-msvc                   # 8.3s
cmake --build --preset release                # 4m28.6s (clean)
cmake --build --preset debug                  # 2m34.9s (clean)
ctest --preset release --output-on-failure    # 20.5s
ctest --preset debug --output-on-failure       # 16.5s
ctest --preset release -L validation --output-on-failure   # 21.0s
# manual re-run of the pinned pluginval binary, verbose, for the report:
third_party/pluginval/1.0.4/pluginval.exe --strictness-level 10 --validate-in-process --verbose \
  "build/Investigate_artefacts/Release/VST3/Investigate.vst3/Contents/x86_64-win/Investigate.vst3"
# CONFIGURE_DEPENDS glob probe:
# added source/dsp/QaTempProbe.cpp, then:
cmake --build --preset release --target Investigate_VST3   # 48.4s, auto-reconfigured, compiled the new file
# deleted source/dsp/QaTempProbe.cpp, then:
cmake --build --preset release --target Investigate_VST3   # rebuilt clean, file gone from the link
```

## Results table

| # | Acceptance criterion (T-003) | Result | Evidence |
|---|---|---|---|
| 1 | `git status` works; `.gitignore` excludes `build/` and `research/_cache/`; no commits, remotes or pushes | PASS | `git status` runs cleanly on branch `master`, "No commits yet". `.gitignore` contains `build/` and `research/_cache/` (plus `third_party/`, `.vs/`, `out/`, `CMakeUserPresets.json`, common IDE files). `git status --ignored` shows `build/`, `research/` (only holds `_cache/`), `third_party/` ignored. `git log` — no commits. `git remote -v` — empty. |
| 2 | `cmake --preset windows-msvc` configures without errors | PASS | Clean configure from a deleted `build/`: "Configuring done (7.5s)", "Generating done (0.7s)", exit via `time` shows `real 0m8.305s`, no errors in the log. |
| 3 | `cmake --build --preset release` builds `Investigate.vst3` and `Investigate.exe` (Standalone) with 0 project-code warnings at `/W4` | PASS | Clean build (`real 4m28.609s`). `grep -ic warning` on the full log = 0; `grep -ic error` = 0 (checked with and without excluding `third_party/` — zero warnings anywhere in the log, not just in project code). Artefacts confirmed present: `build/Investigate_artefacts/Release/VST3/Investigate.vst3/Contents/x86_64-win/Investigate.vst3` (6,857,216 bytes) and `build/Investigate_artefacts/Release/Standalone/Investigate.exe` (6,696,448 bytes). Verified `WarningLevel4` (`/W4`) is set on the `Investigate` shared-code vcxproj. |
| 4a | `ctest --preset release` runs and passes the smoke test | PASS | `Test #1: InvestigateProcessor passes audio through unchanged ... Passed 0.03 sec`. |
| 4b | `ctest --preset release -L validation` runs pluginval at strictness 10 and passes | PASS | `Test #2: pluginval ... Passed 20.40 sec` in the combined run; isolated `-L validation` run: `Passed 20.98 sec`, `100% tests passed, 0 tests failed out of 1`. Manual verbose re-run confirms `SUCCESS` at the end of the log, strictness level 10, and `Time taken to run all tests: 17 secs`. |
| 5 | Adding a new `.cpp` under `source/dsp/` is picked up on the next build without editing CMake; then delete it | PASS | Added `source/dsp/QaTempProbe.cpp`. The subsequent `cmake --build --preset release --target Investigate_VST3` auto-reconfigured (CONFIGURE_DEPENDS triggered "Configuring done" / "Generating done" inside the build step) and compiled `QaTempProbe.cpp` into `Investigate_SharedCode.lib`, with no CMakeLists.txt edit. Deleted the file, rebuilt again cleanly (`real` build succeeded, file no longer referenced). Tree left as found; `git status --porcelain source/dsp/` is empty. |
| 6 | Configuration succeeds with an empty `assets/ui/` | PASS (see note) | `assets/ui/` is **not** currently empty — the ui-ux-designer has added 12 SVG files (icons and logo marks) in parallel, as expected per the task brief. The build log shows `InvestigateAssets.vcxproj -> ...\InvestigateAssets.lib` being built and linked into the plugin with 0 warnings/errors, which confirms the non-empty-assets branch of the CMake logic also works correctly end-to-end (a stronger check than the empty case, and the one currently exercised by the real tree). The build-engineer's own log entry from 2026-09-28 records verifying the empty-`assets/ui/` case directly at the time T-003 was implemented, before any assets existed; this reviewer has no way to re-empty `assets/ui/` without deleting the ui-ux-designer's in-progress work, which is out of scope for this task and would modify files this agent does not own. Recorded as PASS on the strength of both pieces of evidence, not independently re-tested with an empty directory in this run. |
| 7 | `docs/dependencies.md` lists JUCE, Catch2 and pluginval with pinned versions/tags, licenses, usage type and update instructions | PASS | Confirmed by reading the file: JUCE `8.0.15` (GPLv3/commercial, linked), Catch2 `v3.16.0` (BSL-1.0, test only), pluginval `v1.0.4` (GPLv3, tool only, SHA-256 pinned) each with source URL, license, usage and an update procedure. Also documents the build/test/validate commands. |
| 8 | The Debug build also compiles and the smoke test passes in Debug | PASS | Clean `cmake --build --preset debug`: `real 2m34.919s`, 0 warnings, 0 errors. `ctest --preset debug`: both tests pass (`InvestigateProcessor ... Passed 0.02 sec`, `pluginval ... Passed 16.40 sec`). |

## M0 gate (roadmap: "The placeholder VST3 builds, pluginval passes at strictness 10, CTest runs, and both research reports are delivered")

| Gate condition | Result |
|---|---|
| Placeholder VST3 builds | PASS — `Investigate.vst3` builds clean in Release and Debug, 0 warnings/errors. |
| pluginval passes at strictness 10 | PASS — `SUCCESS` at strictness level 10, both via CTest and a standalone verbose run. Reported latency `0`, reported tail length `0`, bus layout lists `Mono` and `Stereo` named layouts with a stereo (2-in/2-out) default main bus — matches the roadmap's stereo pass-through / mono-or-stereo layout requirement (roadmap "Buses and channel layouts": mono-to-mono or stereo-to-stereo; latency 0 samples, tail 0 seconds). |
| CTest runs | PASS — `ctest --preset release`, `ctest --preset debug` and `ctest --preset release -L validation` all run and report 100% pass. |
| Both research reports delivered | Not independently re-verified in depth by this agent (out of scope for a build/validation gate check), but `docs/roadmap.md` Status table lists T-001 and T-002 as DONE, and `docs/dependencies.md` (T-002-adjacent) exists. Flagged as an open item for the architect to confirm against T-001/T-002's own deliverables list, not a build/validator concern. |

**M0 gate verdict: PASS**, with one non-blocking open item (research report delivery is the architect's/researcher's domain, not re-verified here) and one caveat (assets/ui/ empty-case not independently re-tested this run — see criterion 6).

## Performance / numbers

| Step | Duration |
|---|---|
| `cmake --preset windows-msvc` (clean, third_party cached) | 8.3 s |
| `cmake --build --preset release` (clean `build/`) | 4 min 28.6 s |
| `cmake --build --preset debug` (clean `build/`) | 2 min 34.9 s |
| `ctest --preset release` (both tests) | 20.5 s (pluginval 20.40 s, smoke test 0.03 s) |
| `ctest --preset debug` (both tests) | 16.5 s (pluginval 16.40 s, smoke test 0.02 s) |
| `ctest --preset release -L validation` | 21.0 s |
| Incremental rebuild after adding/removing one `.cpp` | 48.4 s (includes MSVC recompiling `Investigate_SharedCode`) |

pluginval summary (verbose, Release build):
```
Testing plugin: VST3-Investigate-bb377da8-7d3509bd
Investigate Audio: Investigate v0.1.0
Time taken to open plugin (cold): 156 ms
Time taken to open plugin (warm): 2 ms
SupportsDoublePrecision: no
Reported latency: 0
Reported taillength: 0
Inputs:  Named layouts: Mono, Stereo   Discrete layouts: Discrete #1
Outputs: Named layouts: Mono, Stereo  Discrete layouts: Discrete #1
Main bus num input channels: 2
Main bus num output channels: 2
Time taken to run all tests: 17 secs
SUCCESS
```

## Defects

None found.

## Open questions

1. T-001 and T-002 research report delivery (the fourth M0 gate condition) was not independently re-verified against their own deliverable lists in this run — this is a documentation/process check outside black-box binary validation. Recommend the architect confirm this directly rather than treating it as covered by this report.
2. The empty-`assets/ui/` configure path (acceptance criterion 6) could not be independently re-tested in this run because `assets/ui/` currently holds the ui-ux-designer's in-progress SVG assets (owned by ui-ux-designer, out of scope to delete). The non-empty path was verified instead and passes. If independent re-verification of the empty case is required, it should be scheduled before ui-ux-designer's assets land, or exercised in an isolated worktree.

## Files touched by this validation run

- Created: `docs/qa/T-003-validation.md` (this report).
- Temporary, since removed: `source/dsp/QaTempProbe.cpp` (created and deleted during criterion 5's glob-pickup check; confirmed absent via `git status --porcelain source/dsp/`).
- No other files under `source/`, `CMakeLists.txt`, `CMakePresets.json`, `docs/tasks/`, or any other owned path were modified.
- Build artefacts (`build/`) were deleted and freshly regenerated by CMake/MSVC as part of this run; nothing under `build/` is committed or tracked (gitignored).
