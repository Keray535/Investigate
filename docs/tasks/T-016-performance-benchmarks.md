# T-016: Performance benchmarks
- Owner: plugin-validator
- Milestone: M4
- Status: TODO
- Depends on: T-012, T-013, T-014

## Goal
Measure Investigate against every budget in the roadmap. The measurements cover the audio thread, the analysis thread, editor painting, the message thread and memory. Report the numbers so the M4 gate can be decided on data.

## Inputs
- `docs/roadmap.md`: the Budgets table, and Architecture (threading model).
- The render harness and tooling from T-014 and T-015.
- The processor API from T-009, and the editor from T-011 to T-013.

## Scope
- Build the benchmarks in `tools/render-harness/` or `tests/integration/` using the Release build. Label the CTest entries `benchmark`, so they are excluded from normal test runs.
- **Audio thread.** Time `processBlock` per block (mean, p99 and max) over at least 60 s of audio. Use stereo main plus stereo sidechain, with analysis active and inactive, at 44.1, 48, 96 and 192 kHz, with block sizes 32, 128 and 512.
- **Analysis thread.** Measure CPU time per second of audio at the default settings and at the worst case: fftSize 65536, 8x overlap, a dual mode, sidechain on, 192 kHz.
- **Editor paint.** Render the full editor into a `juce::Image` with animated synthetic frames and meter data for 300 frames, at 1280×720 and 2560×1440, and report p50 and p95. State clearly that this measures the software renderer, as an upper-bound proxy for Direct2D.
- **Message thread.** Estimate the load at 60 fps from the paint time plus the update path time.
- **Memory.** Measure the resident memory increase per instance at the default and maximum settings, with the editor open.
- Record the machine's CPU, clock, RAM, OS build and power plan in the report.

## Acceptance criteria
- [ ] At 48 kHz, 128 samples, stereo plus sidechain, analysis active: `processBlock` mean ≤ 20 µs and p99 ≤ 50 µs.
- [ ] Analysis thread: ≤ 2 % of one core at the default settings, and ≤ 10 % in the worst case.
- [ ] Editor paint: p95 ≤ 4 ms at 1280×720 and ≤ 8 ms at 2560×1440.
- [ ] Message thread: ≤ 25 % of one core at 60 fps.
- [ ] Memory: ≤ 64 MB resident increase per instance at the maximum settings.
- [ ] The report `docs/qa/T-016-validation.md` gives every number, the pass or fail against its budget, the machine specification, and the commands to reproduce. Any failure names the likely hotspot, from profiling if available, so the architect can route the fix.

## Log
