# T-017: Real-time safety, sanitizer and static analysis audit
- Owner: unit-tester
- Milestone: M4
- Status: TODO
- Depends on: T-009, T-012, T-013, T-014, T-038

## Goal
Run a whole-codebase white-box audit before release:
- a real-time safety review of everything reachable from `processBlock` and the `SpectrumEngine` producer API;
- an allocation-hook test of the full processor;
- the unit suite under AddressSanitizer;
- static analysis;
- coverage of `source/dsp/`, if tooling exists.

## Inputs
- `CLAUDE.md`: Real-time safety rules.
- `docs/roadmap.md`: Architecture (threading model) and Budgets.
- All of `source/`, the existing unit tests, and the QA reports for T-004 to T-013.
- The `asan` preset and any coverage tooling from T-014.

## Scope
- Produce a call graph from `processBlock`, and from `SampleFifo::push`, `SpectrumEngine::pushMain` and `pushSidechain`, with a verdict for every function reached: allocation, locks, system calls, exceptions, unbounded loops, `std::function` construction, logging.
- Write an allocation-hook test: a global `operator new` and `delete` counter armed around `processBlock`. Run 10 s of audio with random parameter changes each block, analysis active, a sidechain present, and a `requestStatsReset` from another thread.
- Review the lock-free structures (`SampleFifo`, `FrameExchange`, the `MeterBank` seqlock snapshot) for memory-ordering correctness, citing the orderings used. For the seqlock, check specifically whether the payload copy is a formal C++ data race (a non-atomic read concurrent with a write). If it is, recommend the fix, for example copying through `std::atomic_ref` or relaxed atomic words, and file it as a defect for dsp-engineer. ThreadSanitizer is not available with MSVC, so this review replaces it.
- Run the full unit suite under the `asan` preset.
- Run MSVC `/analyze` (or clang-tidy, if available) on `source/`, and triage the findings.
- If a coverage tool exists: measure line coverage of `source/dsp/`.
- Check that every timing test added since T-026 carries the hidden `[.perf]` tag (T-026 set up the convention).
- **Zoom and pan** (added 2026-10-03, M3b):
  - include the view and freeze atomics (`setSpectrumView`, `setSpectrumFrozen`, the packed 64-bit view pair and the freeze epoch) in the memory-ordering review;
  - confirm that `processBlock` does not reach any zoom code;
  - run an allocation hook around `SpectrumEngine::processPending` during freeze, unfreeze and continuous view changes; the expected result is 0 allocations without an `fftSize`, window or layout change.
- **Watch item (added 2026-10-03 from T-013):** test #315 in `ProcessorAnalysisLifecycleTest` (`setAnalysisActive` 100 times under concurrent audio) timed out once in Debug while a build ran in parallel; six isolated reruns passed. Run it 20 times in Debug and Release under CPU contention (for example, a parallel build or a busy-loop load on every core). If it times out, measure the analysis-thread stop latency under load against the 50 ms budget, and file a defect for plugin-engineer with the numbers.

## Acceptance criteria
- [ ] The call graph and per-function verdicts are in `docs/qa/T-017-unit.md`. The audio-thread paths have 0 violations, or every violation is filed as a defect with file, line and owning agent.
- [ ] The allocation-hook test reports 0 allocations and 0 deallocations inside `processBlock` over 10 s of audio.
- [ ] Every lock-free structure has a written memory-ordering review with no unresolved finding.
- [ ] The ASan run of the full unit suite has 0 errors.
- [ ] Static analysis finds 0 high-severity issues in `source/`. Every remaining warning is triaged: fixed by its owner, or justified.
- [ ] Line coverage of `source/dsp/` is ≥ 85 %, if measurable. Otherwise the report states why it could not be measured.
- [ ] Every defect found is listed with its owner agent, so the architect can route the fixes.

## Log
- 2026-10-03 vst-architect: Added the zoom-and-pan audit items and a dependency on T-038 (milestone M3b).
