# T-071: Analysis thread: safe stop, and settings changes on an unprepared processor
- Owner: plugin-engineer
- Milestone: M3d
- Status: TODO
- Depends on: T-037 (build order), T-072 (ASan preset, to verify the fix)

## Goal
Fix defect D-1 from `docs/qa/T-037-unit.md`. On a processor that was never prepared, a `channelMode` or `fftSize` change makes the analysis thread take more than 50 ms to stop. `setAnalysisActive (false)` then force-kills the thread with `stopThread (50)`, and the destructor corrupts the heap. A pass-through analyser must never crash its host, whatever the call order.

## Inputs
- `docs/qa/T-037-unit.md`, defect D-1: the reproduction, the timings (51 to 56 ms on an unprepared processor, 2 to 5 ms on a prepared one) and the crash stack (`SpectrumAnalyser::~SpectrumAnalyser` from `~SpectrumEngine` from `~InvestigateProcessor`).
- The hidden test `KNOWN DEFECT*` in `tests/unit/SpectrumViewZoomIntegrationTest.cpp`, which reproduces it.
- `source/PluginProcessor.*` (`AnalysisThread::run`, `setAnalysisActive`, the editor open and close path), `source/dsp/SpectrumEngine.*` and `SpectrumAnalyser.*`, and the T-009 and T-036 logs.
- Working rules from the roadmap handoff: one C++ task at a time; build in `build/` through the presets; `ctest --timeout 300`; timing assertions only in `[.perf]`; coders never edit `docs/qa/`; check for leftover build processes before handing over.

## Scope
1. **Root cause.** Find why a settings change with sample rate 0 or zero channels makes one analysis step take about 50 ms or more (for example a reconfiguration or a wait inside `processPending`, or a sleep or wait in `AnalysisThread::run` that does not wake on `signalThreadShouldExit`). Record the cause in the log, with file and line.
2. **No force-kill.** The analysis thread is never terminated. `setAnalysisActive (false)`, editor close, `releaseResources` and the destructor signal the thread and join it without a timeout that kills it. The thread's wait loop wakes promptly on the exit signal (for example `wait` on a `WaitableEvent`, or `notify` paired with `threadShouldExit()` checks), so the join is short in practice.
3. **Unprepared processor.** Settings changes before `prepareToPlay` (sample rate 0, no channels) are either ignored by the engine or deferred until `prepare`. They never start an analyser reconfiguration with invalid sizes.
4. **Ownership.** If the fix needs a change in `source/dsp/` (owned by dsp-engineer), keep it minimal and additive, record it in the log, and say so in the return, so the architect can route a review. Do not reshape the analyser history or the reset logic: T-063 changes them next.

## Acceptance criteria
- [ ] The `KNOWN DEFECT*` test is turned into a normal (not hidden) regression test by unit-tester, and it passes 20 of 20 runs in Debug and Release: on an unprepared processor, changing `channelMode`, `fftSize`, the window and the overlap while the analysis is active, then deactivating and destroying it, never crashes.
- [ ] Under the ASan preset from T-072, the same test and the whole `[gui]` and processor test sets run with no ASan report.
- [ ] Stopping the analysis thread takes ≤ 20 ms in the worst measured case, prepared or unprepared (`[.perf]`), and no code path calls a thread-killing stop.
- [ ] Every existing test passes. pluginval at strictness 10 passes (it opens editors in varied orders).
- [ ] Warning-clean at `/W4`. `docs/qa/T-071-unit.md` and `docs/qa/T-071-validation.md` report PASS.

## Log
- 2026-10-07 vst-architect: Created from defect D-1 in `docs/qa/T-037-unit.md` (found incidentally by unit-tester; it is outside T-037). The severity is raised because the force-kill corrupts the heap, and some hosts and scanners open an editor before `prepareToPlay`. It runs right after T-037 and T-072, before T-062.
