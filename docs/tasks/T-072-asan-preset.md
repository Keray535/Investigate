# T-072: AddressSanitizer build preset
- Owner: build-engineer
- Milestone: M3d
- Status: TODO
- Depends on: T-037 (build order)

## Goal
Add an MSVC AddressSanitizer preset, so unit-tester can catch memory errors (such as defect D-1 in `docs/qa/T-037-unit.md`) as precise reports rather than random crashes. unit-tester asked for it in the T-037 report.

## Inputs
- `CMakePresets.json`, `CMakeLists.txt`, `cmake/`, and the T-003 and T-030 logs (presets, the refusal of in-source builds).
- `docs/qa/T-037-unit.md`, the "Not run" note.

## Scope
1. A configure, build and test preset (for example `asan`) using `/fsanitize=address` for the plugin, the shared code and the unit-test target. Debug information on, incremental linking off, and any flags ASan needs on MSVC (`/Zi`, `/INCREMENTAL:NO`; no `/RTC`).
2. Its own binary directory (for example `build-asan/`, gitignored), so it never disturbs `build/`.
3. Make sure the ASan runtime DLL is found when the tests run through CTest (copy it next to the test executable or set `PATH` in the test preset).
4. Document the preset and how to run it in `docs/dependencies.md` or the README build section.

## Acceptance criteria
- [ ] `cmake --preset asan`, `cmake --build --preset asan` and `ctest --preset asan --timeout 300` work from a clean checkout. The suite passes, except the known hidden D-1 case, which is not run by default.
- [ ] A deliberate heap overflow in a throwaway test (not committed) produces an ASan report. Record the output in the log.
- [ ] The default presets are unchanged: a normal Debug build is bit-identical in flags.
- [ ] `build-asan/` is gitignored.

## Log
- 2026-10-07 vst-architect: Created from the unit-tester request in `docs/qa/T-037-unit.md`. T-071 uses this preset to verify the D-1 fix.
