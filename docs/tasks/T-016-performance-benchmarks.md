# T-016: Performance benchmarks
- Owner: plugin-validator
- Milestone: M4
- Status: TODO
- Depends on: T-012, T-013, T-014, T-059

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
- **Editor paint** (method amended 2026-10-03, roadmap Decisions). Show the full editor in a real on-screen window with JUCE's default Direct2D renderer. Feed animated synthetic frames and meter data for at least 300 frames, at 1280×720 and 2560×1440, in the worst displayed case (dual mode, secondary curves, sidechain, hover readout, meters moving). Report the p50 and p95 `paint()` wall time. Offscreen `juce::Image` timings (software renderer) may be added for information only; they do not gate.
- **Message thread.** Measure the message-thread CPU over at least 10 s at 60 fps in the same window and case, for example from the thread's CPU time. Do not estimate it from the paint time, because Direct2D defers part of the work out of `paint()`.
  - *Added 2026-10-03 (T-013 F-2):* also measure through the **real editor path**: frames and snapshots pulled by `InvestigateEditor::pollProcessor` with its 60/s `UpdateRateLimiter`, not fed by the harness on every vblank. Measure at the display's **native refresh rate** (144 Hz on the development machine) as well as with the display forced to 60 Hz. Use the T-029 case matrix at 1280×720, 2560×1440 and 1920×1080 at 110 %, median of 3 runs. This real-path number is the one that gates the ≤ 25 % budget. Also report the effective spectrum frame rate in the paced cases (T-013 F-3).
- **Memory.** Measure the resident memory increase per instance at the default and maximum settings, with the editor open.
- **Real program material** (added 2026-10-03). If `third_party/_fixtures/audio/test_audio.wav` is present (local only; located through `INVESTIGATE_TEST_AUDIO_DIR`, see T-032), repeat the audio-thread measurement at 48 kHz, 128 samples, and the message-thread measurement at both window sizes, with the track driving the processor in place of synthetic signals. The track goes on both main and sidechain, looped for the 60 s and 12 s windows. Report these numbers next to the synthetic ones. Real music changes the meters' repaint pattern (T-013 quantised repaints), so this is the realistic editor load. The same budgets apply. If the file is absent, say so and skip.
- **Zoom and pan** (added 2026-10-03, M3b, `docs/plans/spectrum-zoom-pan.md`, Budgets). Measure:
  - the view-change-to-frame latency at `fftSize` 65536 with 1x overlap;
  - the analysis-thread CPU during a continuous 60 fps pan at the default settings;
  - the whole-editor message-thread CPU and the paint p95 during a continuous pan and zoom, with the overview strip visible, through the real editor path at 1280×720 and 2560×1440, at the native refresh rate and at 60 Hz;
  - the memory, which now includes the freeze snapshot.
- **M3c** (added 2026-10-03, `docs/plans/m3c-feature-expansion.md`, Budgets).
  - Repeat the audio-thread measurement with stereo main plus two stereo sidechains, with the band solo idle and engaged and sweeping.
  - Repeat the analysis-thread worst case with an underlay, both secondary curves and two stereo sidechains.
  - Measure the editor with every M3c layer visible (underlay, two sidechains, four references, solo tint, Density meters) at a 60 fps cap with Adaptive on, and report the 144 fps cap numbers for information.
  - Measure the capture latency.
- Record the machine's CPU, clock, RAM, OS build and power plan in the report.

## Acceptance criteria
- [ ] At 48 kHz, 128 samples, stereo plus sidechain, analysis active: `processBlock` mean ≤ 20 µs and p99 ≤ 50 µs.
- [ ] Analysis thread: ≤ 2 % of one core at the default settings, and ≤ 10 % in the worst case.
- [ ] Editor paint: p95 ≤ 4 ms at 1280×720 and ≤ 8 ms at 2560×1440.
- [ ] Message thread: ≤ 25 % of one core at 60 fps.
- [ ] Memory: ≤ 64 MB resident increase per instance at the maximum settings.
- [ ] Zoom and pan:
  - view change to re-mapped frame p95 ≤ 10 ms and max ≤ 20 ms;
  - analysis thread ≤ 4 % of one core during a continuous pan at the default settings;
  - message thread ≤ 25 % and paint p95 ≤ 4 ms and ≤ 8 ms during a continuous pan with the overview visible.
- [ ] The report `docs/qa/T-016-validation.md` gives every number, the pass or fail against its budget, the machine specification, and the commands to reproduce. Any failure names the likely hotspot, from profiling if available, so the architect can route the fix.

## Log
- 2026-10-03 vst-architect: Added the zoom-and-pan measurements and a dependency on T-038 (milestone M3b).
- 2026-10-03 vst-architect: Milestone M3c inserted before M4. The dependency moves to T-059. Added the M3c measurements; the gating budgets are unchanged and still measured at 60 fps.
