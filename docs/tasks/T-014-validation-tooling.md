# T-014: Validation tooling: VST3 validator, ASan preset and harness hook
- Owner: build-engineer
- Milestone: M4
- Status: TODO
- Depends on: T-002, T-003

## Goal
Give the testers the tooling the M4 gate needs:
- the Steinberg VST3 validator wired into CTest;
- an AddressSanitizer build preset;
- a CMake hook that builds plugin-validator's render harness from `tools/render-harness/`;
- a coverage option if T-002 approved one.

## Inputs
- `docs/research/T-002-dsp-references.md`: section 10 (the license table: VST3 SDK and validator, OpenCppCoverage).
- `docs/dependencies.md`, `CMakeLists.txt` and `CMakePresets.json` from T-003.
- `docs/roadmap.md`: Milestones (M4) and Working rules.

## Scope
1. **VST3 validator.** Pin the Steinberg VST3 SDK at version 3.8 or newer, which is MIT-licensed according to T-002 section 10. Confirm the license file at the pinned tag. Build only its `validator` tool; a prebuilt validator is acceptable if its license and source are equally clear. Register a CTest test `vst3-validator` with label `validation`, running it on the built `Investigate.vst3`. Record the SDK in `docs/dependencies.md`.
2. **ASan preset.** Add configure, build and test presets named `asan` (MSVC `/fsanitize=address`, Debug information, no incremental linking) for the plugin and the unit tests. Document any known false positives with JUCE and how to suppress them.
3. **Harness hook.** If `tools/render-harness/CMakeLists.txt` exists, call `add_subdirectory` on it. The harness may link the plugin's shared-code target, so it can instantiate the processor in-process. Tests it registers carry the label `integration`. plugin-validator owns the harness contents.
4. **Coverage (optional).** If T-002 approved OpenCppCoverage as a tool-only dependency, add a documented script or CTest wrapper that produces a coverage report for `source/dsp/` from the unit tests. Otherwise skip this item and say so in the log.

## Acceptance criteria
- [ ] `ctest --preset release -L validation` runs both pluginval and the VST3 validator, and both pass on the current build.
- [ ] `cmake --preset asan`, the matching build and `ctest --preset asan` build and run the unit tests with ASan enabled. The run is clean, or every finding is listed as a defect for the owning agent in the log.
- [ ] A dummy `tools/render-harness/CMakeLists.txt` containing one trivial executable, created temporarily for verification and deleted afterwards, is picked up and built.
- [ ] `docs/dependencies.md` is updated with the VST3 SDK tag, its license and its usage type, plus the coverage tool if one was added.
- [ ] Release and Debug builds and all existing tests still pass.

## Log
