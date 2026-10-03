# T-041: Processor: request the visible point count, not the frame capacity
- Owner: plugin-engineer
- Milestone: M3b
- Status: TODO
- Depends on: T-039

## Goal
Since T-039, `SpectrumFrame::maxPoints` is the frame capacity (3072, margin included), and the new `SpectrumFrame::maxVisiblePoints` (2048) is the largest visible point count. `source/PluginProcessor.cpp` (about line 105, in the analysis-thread settings) still sets `settings.numPoints = SpectrumFrame::maxPoints`. This is harmless today, because the engine clamps `numPoints` to `maxVisiblePoints`, but it states the wrong intent and depends on that clamp. Change it to `SpectrumFrame::maxVisiblePoints`.

## Inputs
- `docs/tasks/T-039-engine-pan-margin-bin-spacing.md` (log: the frame constants and the engine clamp).
- `source/PluginProcessor.cpp`, `source/dsp/SpectrumFrame.h`.

## Scope
1. Replace the constant in the settings fill, and check the rest of `source/PluginProcessor.*` for any other use of `SpectrumFrame::maxPoints` that means "visible points".
2. No behaviour change: the published frames are identical before and after.

## Acceptance criteria
- [ ] `source/PluginProcessor.*` uses `maxVisiblePoints` wherever it means the visible point count; any remaining `maxPoints` use means the capacity, with a short comment.
- [ ] `ctest --timeout 300` passes in Release, and the T-009, T-036 and T-039 tests pass unchanged.
- [ ] Warning-clean at `/W4`. `docs/qa/T-041-unit.md` reports PASS (unit-tester may keep the report short: a build, the ctest run and a diff review).

## Log
- 2026-10-03 vst-architect: Created from the T-039 open question 1. One-line change in a file that dsp-engineer does not own. It runs before T-037, as a separate C++ task (one at a time). It may be folded into the same session as T-037's start, but not into gui-engineer's edits.
