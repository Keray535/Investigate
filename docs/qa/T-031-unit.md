# T-031 unit QA: GUI test robustness (pacing test failure, test hang)

Verdict: PASS (0 production defects)

## Root cause of test #105
Classification: environment (desktop state), not a code defect. Honest limit: the failing state ended on its own during the investigation and could not be brought back, so the exact OS-level trigger is not proven.

Evidence:
- Test #105 failed 3 of 3 times (Release, isolated, `paints - base` = 1, expected 2) at the start of the session, and one extra time with a debug-instrumented scratch test. A scratch test then showed that in that state the pacing poll issued no visible paint, while a direct `view.repaint()` of a 100x100 area was painted and vblank callbacks arrived at 144 Hz (289 in 2 s) and a plain probe window painted 10 of 10 repaints. So "no vblanks / no paints" (the display-asleep hypothesis) is rejected for the failing period: the desktop was delivering both. What was lost was the repaint of the large (2536x1464) data rect issued by the poll.
- After a rebuild (source files unchanged: `git status source/` clean throughout) the same test passed: 3/3 Release, 3/3 Debug (the unchanged Debug exe from 11:13), 14 of 15 in a loop (one failure), 60/60 in-process iterations, 25/25 fresh-process iterations. With temporary `fprintf` logging in `requestFrameRepaint`/`pollFrame` (reverted) the log in passing runs is exactly as designed: `paced=1`, second request deferred (`dt=0.1 ms`), `POLL pending=1`, then repaint at `dt=91 ms`, `POLL pending=0`. This shows the pacing logic itself is correct and deterministic: the repaint is requested every time, and whether it is painted depends on the desktop.
- Display state recorded: two 1920x1080 displays (Intel Iris Xe 144 Hz, NVIDIA RTX 3060 60 Hz, hybrid laptop), scale 100 %, session unlocked, display on, Chrome and Settings open. The test window is 2600x1500, larger than either display and spanning both.
- Reproduced a second flavour deliberately: minimising all windows around each run (`Shell.Application.MinimizeAll`/`UndoMinimizeAll`) produced one failure in 18 runs: section 2 got `paints - afterFrames` = 4 and 14 instead of 1. These are OS-driven paints (window exposed or re-shown) that the old exact-count test counted as view paints. This proves the old test's exact paint counts depend on the desktop staying still.
- Mutation check: replacing the pacing deferral in `requestFrameRepaint` with `false && ...` (reverted) makes the new test FAIL (`{1,1,1,1}` vs `{1,1,2,2}`), so the rewritten test still detects a real pacing defect.

Conclusion: test defect (fragile exact paint counts with fixed 25/40 ms pumps and no environment gate) triggered by desktop state. No defect for gui-engineer.

## The hang
Only one unit test creates a desktop window (`SpectrumViewPacingTest.cpp`; grep over `tests/unit` for `addToDesktop`, `PeekMessage`, `VBlankAttachment`, `MessageManager`). Its waits were fixed-length pumps, so it could not loop forever by itself. Candidates examined:
- Window test: a blocking native call (swap-chain present, window creation or `~ScopedJuceInitialiser_GUI` against a sleeping display or locked session) cannot be bounded by a deadline in the test code. Not reproducible here. Mitigation: a `Watchdog` (GuiTestEnvironment.h) that ends the process with a message after 60 s.
- Thread tests (`MeterBankTest`, `SampleFifoTest`, `FrameExchangeTest`, `ProcessorAnalysisLifecycleTest`, `ProcessorResetRequestsTest`): all waits are bounded by a deadline or a producer-done flag; the unbounded `while (!resetSent)` in MeterBankTest.cpp:78 waits on a thread that sets the flag immediately. No unbounded wait found, no hang seen in 6 full runs plus the isolated loops. Left unchanged.
- `ctest` runs each Catch2 test case as its own process, so a hang is confined to one test case. The cause of the one-time hang in T-030 is therefore not proven; the likeliest candidate is the window test. T-014 should add the default `--timeout`.

## Changes
- New `tests/unit/GuiTestEnvironment.h`: `pump`, `pumpUntil` (every wait capped at 10 s), `VBlankCounter`, `Watchdog`.
- `tests/unit/SpectrumViewPacingTest.cpp` rewritten:
  - environment gate (all bounded to 2 s): native peer exists, window paints, at least 10 vblank callbacks, a direct repaint of the whole view is painted; otherwise `SKIP` with the reason;
  - paint waits use `pumpUntil` (2 s) instead of fixed pumps;
  - each scenario is measured up to 3 times from a quiet window; it passes if one matches, SKIPs if the desktop produced unsolicited paints or stopped painting, and FAILs only if the window was quiet and the counts never matched (a real, deterministic defect).
- No other test needed a change.
- Pure-logic pacing coverage without a window already exists (`SpectrumMathTest`/`SpectrumTraceMathTest`: `countLayers`, `isPaced`, constants). The deferral and pending flag in `SpectrumView` are private; see open question 1.

## Results
Environment: Windows 11, MSVC, display on, session unlocked, two displays. Commands:
`cmake --preset windows-msvc`, `cmake --build --preset release`, `cmake --build --preset debug`, then from `build/`: `ctest -C <Release|Debug> -E pluginval --timeout 300`.

| Run | Result | Time |
|---|---|---|
| Release 1 / 2 / 3 | 276 of 276 passed (5 EBU fixture tests report skipped) | 24.0 / 25.9 / 25.5 s |
| Debug 1 / 2 / 3 | 276 of 276 passed (same 5 skipped) | 22.7 / 22.5 / 22.0 s |
| Pacing test alone, display on, Release | 3/3 pass | 1.6 s each |
| Pacing test, all windows minimised around each run, 15 runs (new test) | 15/15 pass | |
| Pacing test with vblank threshold forced to 100000 (temporary edit, reverted) | SKIP with reason after 2.4 s, no hang | |
| Mutation (deferral disabled, reverted) | FAIL `{1,1,1,1}` vs `{1,1,2,2}` | |

Not done: a run with the display actually asleep or the session locked (would disturb the user's session). The SKIP path was exercised by forcing the gate to fail instead.

## Real-time safety, sanitizers, coverage
Not applicable (tests only; no change in `source/`).

## Open questions
1. A test hook such as `bool isFrameRepaintPendingForTesting() const` (and a repaint counter) in `SpectrumView` would let the pacing logic be checked without a window. For gui-engineer through T-013, optional.
2. The 60 s watchdog ends the process (exit code 3) on a blocked native call; confirm this is acceptable. T-014 should still add the default ctest `timeout`.
