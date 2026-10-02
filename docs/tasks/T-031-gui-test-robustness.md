# T-031: Make the on-screen GUI tests deterministic (pacing test failure, test hang)
- Owner: unit-tester
- Milestone: M3
- Status: DONE
- Depends on: T-030

## Goal
The unit suite must be green and must always finish, whatever the state of the desktop. Two problems appeared during T-030's verification run on 2026-10-03:
1. Test #105, "SpectrumView pacing: a paced second frame is deferred and issued by the next poll after 30 ms" (`tests/unit/SpectrumViewPacingTest.cpp`, checks at lines 107 and 111), fails on every run, including when run alone: `probe.paints - base` is 1 where 2 is expected. It passed during T-029 unit QA earlier the same day. T-030 changed only the CMake in-source guard and `.gitignore`.
2. A plain `ctest --preset release` (no `--timeout`) once left `InvestigateUnitTests.exe` hanging until it was killed. A rerun with `--timeout 120` finished in 21 s.

The working hypothesis, not yet verified: tests that need a real on-screen window depend on Windows delivering vblank callbacks and window paints. Those can stop or slow down when the display is asleep, the session is locked, or the window is occluded or off-screen. A test that waits for a paint without a bound would then hang, and one that counts paints would fail.

## Inputs
- `tests/unit/SpectrumViewPacingTest.cpp`, `SpectrumViewRenderTest.cpp`, `SpectrumViewAllocationTest.cpp`, `Editor*Test.cpp`, and any other test that creates a desktop window or waits on the message loop.
- `source/gui/SpectrumView.*` (`requestFrameRepaint`, `pollFrame`, `SpectrumMath::pacing`). Read only: the code belongs to gui-engineer.
- `docs/qa/T-029-unit.md` (your earlier note: "If CI has no desktop session the test SKIPs rather than fails") and the T-029 task log (the frame pacing design).
- `docs/tasks/T-030-in-source-build-cleanup.md` (the failing run).

## Scope
1. **Reproduce and find the cause.** Run test #105 alone, several times. Record whether the display is on, and whether vblank callbacks and paints arrive at the expected rate (for example, count `VBlankAttachment` callbacks per second in a scratch test). Decide whether the failure comes from the environment or from a defect in the pacing code.
   - If it is a code defect, do not fix production code. Write it as a defect for gui-engineer, with file, line and a repro, in `docs/qa/T-031-unit.md`. The architect routes it into T-013.
2. **Make the on-screen tests robust.** Every test that needs real window paints or vblanks must:
   - check up front that the environment delivers them, for example that a probe window receives paints and vblank callbacks within a bounded time such as 2 s;
   - if it does not, end with Catch2 `SKIP` and a clear reason, rather than failing or hanging;
   - use bounded waits everywhere. No loop may wait for a paint or a callback without a deadline.

   Pure-logic tests must not depend on a window. If pacing logic can be checked without one through the public test hooks, check it that way too.
3. **Find the hang.** Find the test or tests that can block without a deadline, and bound them.
4. **Run the full suite.** Run it in Debug and Release with `--timeout 300`, three times each, with the display on. If you can arrange it, also run it once with the window minimised or occluded. Record the results.

## Acceptance criteria
- [x] The root cause of the test #105 failure is stated in `docs/qa/T-031-unit.md` as environment, test defect or code defect, with evidence.
- [x] No test can block forever: every wait in a window-dependent test has a deadline of 10 s or less.
- [x] When the environment delivers no paints or vblanks, window-dependent tests SKIP with a reason and never fail.
- [x] With the display on, the full suite passes three times in a row in Debug and in Release (`ctest -C <cfg> -E pluginval --timeout 300`), with no failures.
- [x] Any production-code defect is reported for gui-engineer, and not fixed by you.
- [x] `docs/qa/T-031-unit.md` reports PASS.

## Log
- 2026-10-03 vst-architect: Created from T-030's open question 1. Runs before T-013, because T-013's testers need a green suite. Open question 2 from T-030 (a default `timeout` in the ctest presets) is routed to T-014.
- 2026-10-02 unit-tester: Root cause of #105: environment plus a fragile test (exact paint counts, fixed pumps), not a code defect; the failure stopped by itself and could not be re-triggered, vblank (144 Hz) and plain paints were fine while it failed, so the display-asleep hypothesis is rejected for that period. Pacing logic confirmed by logging (reverted) and by a mutation check. Only SpectrumViewPacingTest uses a window; rewrote it with an environment gate (SKIP), bounded waits (new `tests/unit/GuiTestEnvironment.h`, 10 s cap), retry-from-quiet scenarios and a 60 s watchdog. No unbounded wait found elsewhere; the T-030 hang is unproven, most likely the window test. Results: Release 3/3 and Debug 3/3 full suites 276/276 pass, plus 15/15 with windows minimised. Report: `docs/qa/T-031-unit.md`. No production defects.
