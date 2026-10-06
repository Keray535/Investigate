# T-061: Diagnosis: low-frequency curve, block-size stutter, small-window glitch
- Owner: researcher
- Milestone: M3d
- Status: TODO
- Depends on: none

## Goal
Find the root causes of three bugs from the user's M3 review, and point to proven fixes, so the coders can fix them without guessing:
- **#2 (high priority):** the spectrum curve looks jagged or stepped below about 300 Hz;
- **#15:** the display stutters while the Block size is increased;
- **#16:** the editor glitches when the window is made very small, in FL Studio.

This is a read-only investigation of the code plus outside references. It changes no product code.

## Inputs
- `docs/plans/m3-user-review-findings.md`, findings #2, #15 and #16, and the test environment: FL Studio 25.1.2, i7-12700H, RTX 3060 Laptop, a 1080p display.
- `docs/plans/m3-review-spectrum-slots.md` (Scope, M3d).
- The mapping path: `source/dsp/SpectrumMapper.*`, `SpectrumAnalyser.*`, `SpectrumEngine.*`, `DisplayRange.h`, and the T-005, T-035 and T-039 logs (the roadmap's "interpolate low, peak-preserve high" mapping).
- The drawing path: `source/gui/SpectrumView.*`, `SpectrumMath.h`, and the T-012 and T-029 logs (sprite-batch fills, near-collinear vertex dropping, per-column bars, frame pacing).
- The resize path: `source/PluginEditor.*`, `source/gui/LayoutMath.h`, and the T-011 log (constrainer, minimum 920×540).
- The analysis-thread lifecycle and settings changes: `source/PluginProcessor.*`, and the T-009 and T-036 logs.

## Scope
1. **#2, the jagged low end.**
   - Trace how display points are produced below about 300 Hz at the default settings (4096 points, 1/12-octave smoothing, 48 kHz, 1200×700), and also with Smooth Off and at 65536 points.
   - Determine whether the steps come from:
     - the mapper: linear interpolation between sparse bins, nearest-bin lookup, or interpolation in power versus dB;
     - the smoothing: a window narrower than the bin spacing at low frequencies;
     - the editor: vertex dropping, per-column bars, pixel snapping, or the stroke path.
   - Point to the file and function, and give a numeric illustration (for example the point values and their second differences around 50 to 300 Hz).
   - Survey how established analysers draw a smooth low end from sparse FFT bins: for example cubic, Catmull-Rom, monotone (Fritsch–Carlson) or windowed-sinc interpolation of the bin magnitudes in the log-frequency domain, and SPAN's documented behaviour if any.
   - Recommend one method, with its cost and its effect on peak accuracy (a sine at a bin centre must keep its level within 0.1 dB). Licences: only permissive sources may be copied; otherwise describe the method.
2. **#15, the block-size stutter.**
   - Trace what happens between a `fftSize` change and the next displayed frame: the reallocation in `SpectrumAnalyser::setSettings`, the reset of the averaging and secondary accumulators, whether the engine publishes an empty or partial frame, the time until the first full window at the new size, and any message-thread work (for example re-creating paths or images).
   - Measure or estimate the gap for 4096 → 65536.
   - Recommend a fix that keeps the last frame on screen until the new size produces its first full frame, or explain why a different fix is better.
3. **#16, the small-window glitch.**
   - Reproduce it in the Standalone if possible (resizing to the minimum quickly). Inspect the constrainer, the layout math at the minimum size, and repaint behaviour during a live resize under Direct2D.
   - Research known FL Studio wrapper resize issues with JUCE 8 VST3 editors (JUCE forum, issue tracker).
   - Say whether the cause is the plugin, the host, or unknown, and recommend a fix or a workaround.
4. Write `docs/research/T-061-review-bug-diagnosis.md` with one section per bug: the cause (with file and line references), the evidence, the recommended fix, the expected cost, and the sources.

## Acceptance criteria
- [ ] The report names a concrete cause for #2, with file and function, backed by numbers from the code path. It also says which component (mapper or editor) must change.
- [ ] The report recommends one interpolation method for #2 with at least two cited sources. It states the cost per re-map at 2048 visible points and the expected level accuracy at bin centres.
- [ ] The report gives a timeline of the #15 gap (what is published or drawn between the change and the first new frame) and a concrete fix.
- [ ] For #16 the report reaches a verdict (plugin, host or unknown), with the reproduction steps attempted and the sources.
- [ ] No product code or test is changed.

## Log
- 2026-10-06 vst-architect: Created from the M3 review triage (`docs/plans/m3-review-spectrum-slots.md`). The user requires #2 to be solved **at every Smooth setting, including Off**: raising Smooth is not an acceptable workaround. The architect routes T-062 to dsp-engineer or gui-engineer according to this report's verdict on #2.
