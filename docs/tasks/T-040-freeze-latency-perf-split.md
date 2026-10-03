# T-040: Move the 20 ms frozen-frame bound out of the default test run
- Owner: unit-tester
- Milestone: M3b
- Status: DONE
- Depends on: T-036

## Goal
The T-036 test case "Freeze epoch: frames after freeze are old-epoch or the frozen snapshot, never live under a frozen epoch; frozen frame within 20 ms" (`tests/unit/ProcessorViewFreezeTest.cpp`, tags `[processor][freeze][T-036]`) runs in the default `ctest` run and asserts a 20 ms wall-clock bound. It failed once in 23 runs on a loaded machine (32 ms; `docs/qa/T-036-unit.md`). The project rule (roadmap Decisions, 2026-09-29) is that timing assertions live in hidden `[.perf]` cases, so that the default run and CI (T-018) stay deterministic. Split the case so that the functional checks stay in the default run and the 20 ms budget is asserted only in `[.perf]`.

## Inputs
- `tests/unit/ProcessorViewFreezeTest.cpp`, in particular the freeze-epoch case (around line 366) and the existing `[.perf]` latency case (around line 272) as the pattern.
- `docs/qa/T-036-unit.md` (the flake note) and `docs/tasks/T-036-processor-view-freeze-api.md` (the freeze-epoch criterion).
- Roadmap Decisions of 2026-09-28 and 2026-09-29 on `[perf]` and `[.perf]`.

## Scope
1. In the default-run case, keep every functional assertion unchanged:
   - frames after a freeze carry the old epoch or the new epoch with the frozen snapshot;
   - no live frame carries a frozen epoch;
   - the frozen frame arrives without new audio;
   - unfreezing publishes the live state.
   Replace the 20 ms bound with a loose bound that only proves the frame arrives: 500 ms, with a bounded wait (no unbounded loop).
2. Add a hidden `[.perf][T-036]` case that asserts the roadmap number: the frozen frame is published within 20 ms of `setSpectrumFrozen (true)`, over the same 20 freeze and unfreeze rounds. Report p95 and max.
3. Review the other wall-clock assertions in the same file that run by default (for example the 50 ms analysis-thread stop over 100 cycles, and the 512-point smoke latency at 200 ms). Leave them as they are if they are contract checks with a wide margin, and state in the report why. Move any that are not.
4. Do not change production code.

## Acceptance criteria
- [x] The default-run freeze-epoch case contains no bound tighter than 500 ms, and its functional assertions are unchanged.
- [x] A hidden `[.perf][T-036]` case asserts the 20 ms frozen-frame bound, and passes in Release when run on its own (`InvestigateUnitTests.exe "[.perf][T-036]"`); p95 and max are reported.
- [x] `ctest --timeout 300` passes 3 times in a row in Release, with the number of test cases reported.
- [x] `docs/qa/T-040-unit.md` reports PASS and includes the review of the other wall-clock assertions in the file.

## Log
- 2026-10-03 vst-architect: Created from the T-036 open question 1 and the flake note in `docs/qa/T-036-unit.md`. Small test-only change; it runs before T-039 so the C++ tasks stay sequential.
- 2026-10-03 unit-tester: Split done. Shared helper runFreezeRounds; default case bounded at 500 ms, hidden [.perf][T-036] case asserts 20 ms (p95 6.593 ms, max 6.843 ms, Release). ctest --timeout 300 passed 3/3 in Release (378 tests). Other timing assertions reviewed and kept (50 ms stop, worst 3.6 ms; 200 ms smoke). Report: docs/qa/T-040-unit.md.
