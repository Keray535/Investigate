# T-061: Diagnosis of review bugs #2, #15 and #16

## Question
Why does the spectrum curve look stepped below about 300 Hz (#2), why does the display stutter when Block size goes up (#15), and why does the editor glitch at very small window sizes in FL Studio (#16)? What are proven fixes?

## Answer and recommendation
- **#2: the cause is in the mapper (`source/dsp/SpectrumMapper.cpp`), not the editor. Route T-062 to dsp-engineer.** Below the interpolation/aggregation crossover the mapper draws straight power-domain lines between FFT bins. Each bin becomes a visible corner. The smoothing window is narrower than a bin there, so it cannot hide the corners at any setting. Recommended fix: monotone cubic (Fritsch-Carlson / PCHIP) interpolation of the bin levels in the dB domain, evaluated at the display points. This keeps bin-centre levels exact (0.000 dB error) and cannot overshoot.
- **#15: the "keep the last frame" behaviour already exists.** Nothing empty is published and no message-thread work happens. The stutter is the analysis latency itself. After a size change `reset()` waits for a whole new window (1.37 s at 65536 and 48 kHz). Afterwards frames arrive only every `fftSize/overlap` samples (341 ms at 65536, about 2.9 fps). Recommended fix: keep a `maxFftSize` sample history so a size change yields a frame at once, and cap the hop (for example 2048 samples). Both changes are in the dsp layer.
- **#16: verdict is host-triggered and plugin-visible. The reproduction was not attempted, for the reasons given in section 3.** FL Studio can resize the host window below the 920x540 minimum without the constrainer being applied. JUCE's VST3 wrapper then clamps the editor back to the minimum (`setBoundsConstrained`), so the editor and the host window disagree and the content is clipped or stale. Recommended: lower the minimum size (the user already wants this in #6/#13), keep the layout valid at every size down to the new minimum, and verify in FL.

## 1. Bug #2: jagged curve below about 300 Hz

### Cause
`SpectrumMapper::mapOnGrid`, step 1 (`source/dsp/SpectrumMapper.cpp:284-322`).
- Lines 291-304: where a point's span is no wider than a bin (`hiEdgeHz - loEdgeHz <= binHz`), the value is `binPower[b0] + (binPower[b1] - binPower[b0]) * frac`. This is **linear interpolation in the power domain** between the two bracketing bins.
- On a dB axis this is a polyline with a vertex (corner) at every bin centre, and a concave arc between vertices.
- The points are far denser than the bins, so every bin corner is rendered.
- Lines 331-379: the "smoothing" is a boxcar of `lround(octaves/2/step)` points (`smoothingHalfWidthPoints`, lines 81-93). It is constant in octaves, so at low frequencies it is narrower than the bin spacing:

| Bin spacing | 4096-point FFT, 48 kHz (11.7 Hz bins) | 65536-point FFT |
|---|---|---|
| at 50 Hz | 0.304 oct | 0.021 oct |
| at 100 Hz | 0.160 oct | 0.011 oct |
| at 200 Hz | 0.082 oct | 0.005 oct |
| at 300 Hz | 0.055 oct | 0.004 oct |

- 1/12-octave smoothing (0.083 oct) is wider than the bin spacing only above about 197 Hz at 4096 points. Below that, the corners survive. This matches the user's "below about 300 Hz".
- With Smooth Off (`halfWidth == 0`) nothing at all is applied to the interpolated line. At 1/3 octave the window is wider than the bin spacing down to about 50 Hz, which is why raising Smooth "helps" but is not acceptable.
- Interpolation is used wherever point spacing is at most the bin spacing. For 2048 points over 20 Hz to 20 kHz and 4096-point FFT, that is below about 3.47 kHz (2048 points from 20 Hz to 20 kHz is 0.00488 oct per point). At 65536 points the interpolation region ends at about 217 Hz, so the corners are far less visible there (bin spacing 0.005 oct at 200 Hz).
- Only the 4096-point default and similarly small Block sizes show the defect strongly.

### The editor is not the cause
The drawing path reproduces the data faithfully:
- `SpectrumMath::reduceToColumns` (`source/gui/SpectrumMath.h:297-328`) keeps the extremes per physical pixel column.
- `SpectrumView::tracePolyline` (`source/gui/SpectrumView.cpp:710-827`) feeds them to `PolylineSimplifier`. Its tolerance is `simplifyTolerancePx = 0.04` px (line 43). That removes only vertices within 0.04 px of a straight line and cannot create or hide corners.
- There is no pixel snapping on the curve (`snapToPixel` is used for grid lines only).
- With a 1200 px wide plot, about 2 points fall on each pixel column at the low end, so the stroke shows the polyline exactly.

### Evidence (throwaway script, scratchpad `m.py`, pure Python)
This re-implements the mapping math. The input is synthetic: a 1/f power envelope times chi-square(2n) ripple, averaged over 16 frames. 4096-point FFT, 48 kHz, 20 Hz to 20 kHz grid, region 50 to 300 Hz.

| Points | Points in 50-300 Hz | Linear (current): max abs second difference | PCHIP in dB: max abs second difference |
|---|---|---|---|
| 2048 | 531 | 0.408 dB | 0.088 dB |
| 4096 | 1062 | 0.309 dB | 0.024 dB |

- With the current method, 68 % (2048 points) and 86 % (4096 points) of the points have an almost-zero second difference (below 0.001 dB), meaning they sit on straight segments. The curvature is concentrated into one-point spikes at the 21 bin corners between 50 and 300 Hz.
- A Hann-windowed sine at a bin centre, shown at bin offsets in dB (0 is the bin centre):

| Offset | Linear power | PCHIP in dB |
|---|---|---|
| 0 | 0.00 | 0.00 |
| -0.25 | -0.90 | -0.40 |
| -0.5 | -2.04 | -1.58 |
| -1 | -6.02 | -6.02 |

  The current method gives a sharp triangular apex in dB. PCHIP gives a rounded peak, with the same value at every bin centre.
- The numbers are illustrative, not measured on real audio.

### Interpolation methods compared

| Method | Smooth (C1) | Level at bin centre | Overshoot | Verdict |
|---|---|---|---|---|
| Current: linear power between bins | no (corners) | exact | none | cause of the bug |
| Linear in dB | no | exact | none | still corners |
| Catmull-Rom cubic | yes | exact | yes, ripples around peaks and notches in dB | can exceed the +0.1 dB bound |
| Natural cubic spline | C2 | exact | yes, global ringing | no |
| Windowed-sinc / zero-padding (4x FFT) | yes | near exact | small Gibbs ripple | 4x FFT cost, changes the analyser |
| **Monotone cubic Hermite (Fritsch-Carlson, PCHIP) in dB** | yes (C1) | exact (0.000 dB) | none: monotone in every bin interval | **recommended** |

### Recommended fix (for T-062, dsp-engineer)
Interpolate in the dB domain with PCHIP, then convert back to power for the existing power-domain boxcar:
1. For the bin range that the interpolation branch uses, compute `d[k] = 10 log10(max(binPower[k], eps))`.
2. Compute the secant slopes between bin centres and the Fritsch-Carlson tangents: the harmonic mean of the neighbouring secants where they have the same sign, and 0 at local extrema. Use the three-point formula at the two ends.
3. For each display point, `t = bin - floor(bin)`, evaluate the cubic Hermite polynomial, and convert with `10^(y/10)`.

Notes for the implementation:
- Because bin-centre values are interpolated exactly and the cubic is monotone inside each interval, the result is never above `max(bin0, bin1)`. The task's acceptance criterion "interpolated low-frequency points never exceed max(neighbour bins) + 0.1 dB" and "a sine at a bin centre keeps its level within 0.1 dB" both hold with 0.0 dB error.
- The sampling of the display in the aggregation branch (peak-preserving max) stays as it is.
- Optional: at the branch crossover (about 3.47 kHz at 4096), blend or share the value so the interpolated and aggregated regions join without a visible step. This is secondary.
- The floor `kFloorPower` in `SpectrumAnalyser` should bound `d[k]` from below, so deep gaps do not produce huge slopes.
- Slope is added afterwards in dB (lines 381-393), unchanged.

### Cost (estimate, not measured)
- Per curve: about 300 `log10` calls for the bins below the crossover (about 6 microseconds). The same happens at 65536 points, where the crossover is at about 217 Hz and the bin count is also about 300.
- Per curve: 2048 points times (about 15 flops for the Hermite plus one `exp`/`pow`), roughly 50 to 80 microseconds.
- With up to 7 curves per frame and at most about 47 frames per second, that is below 0.5 ms per frame and under 0.5 % of one core, on the analysis thread. This is negligible against the current mapper cost.
- Memory: one scratch array of the bin slopes, sized in `prepare()` (65537 floats), no allocation at run time.

### Sources
- Fritsch, F. N. and Carlson, R. E. (1980), "Monotone Piecewise Cubic Interpolation", SIAM Journal on Numerical Analysis 17(2), 238-246. Summary: https://en.wikipedia.org/wiki/Monotone_cubic_interpolation (algorithm steps, C1, no overshoot).
- SciPy `PchipInterpolator`, a widely used reference implementation of the same method (BSD-3-Clause): https://docs.scipy.org/doc/scipy/reference/generated/scipy.interpolate.PchipInterpolator.html
- SPAN's documented interpolation behaviour: not found; the manual does not describe it.
- Code reviewed: `source/dsp/SpectrumMapper.cpp` lines 81-93, 284-394; `source/gui/SpectrumView.cpp` 710-827, 1039; `source/gui/SpectrumMath.h` 297-328.

## 2. Bug #15: stutter when Block size goes up

### What happens, step by step (`source/dsp/SpectrumAnalyser.cpp`)
1. The user changes `fftSize`. The analysis thread reads it every pass (`source/PluginProcessor.cpp:75-126`, loop with `wait (5)`) and calls `SpectrumEngine::processPending`, which calls `analyser.setSettings` (`SpectrumEngine.cpp:73`).
2. `setSettings` (lines 128-187): the FFT plan, window table and `binScale` are rebuilt. `needsFullReset` is true, so `reset()` (lines 219-234) zeroes the rings, sets `samplesIngested = 0`, `countdownToNextFrame = fftSize` and `framesProduced = 0`, and re-primes the averaging. `resetSecondary()` clears the max and average curves.
3. In `processPending` (`SpectrumEngine.cpp:153-157`) there is no new hop and `getFramesProduced() <= 0`, so it returns without publishing. The editor keeps drawing the last published frame. It is not an empty or partial frame, and the message thread does no extra work (no path or image re-creation is triggered by the change).
4. `pushBlock` (line 304-309) produces the first frame only when `samplesIngested >= fftSize`. That first frame is a single raw periodogram, because `primeOrBlend` primes the average with it (no averaging yet, no smoothing in time).

### Timeline (48 kHz; hop is `fftSize/overlap`, default overlap 4)

| Block size | Time to first new frame after the change | Update interval afterwards |
|---|---|---|
| 4096 | 85 ms | 21 ms (47 fps) |
| 8192 | 171 ms | 43 ms |
| 16384 | 341 ms | 85 ms |
| 32768 | 683 ms | 171 ms |
| 65536 | **1365 ms** (0.68 s at 96 kHz, 1.49 s at 44.1 kHz) | **341 ms (2.9 fps)** |

- Going 4096 to 65536 by stepping the selector through each value gives a frozen display for 171 + 341 + 683 + 1365 ms in total, one gap per step.
- Averaging constants: with the default 1000 ms average, alpha per hop is 1 - exp(-0.341 / 0.217) = 0.79 at 65536, so the averaging barely smooths and the curve jumps visibly each update.
- The new first frame is also un-averaged and noisy, so after the frozen period the curve jumps from a smooth averaged curve to a rough one. This is part of what the user sees.
- The FFT plan, window and `reset()` cost (rebuilding 65536-point tables and clearing the rings) is a few milliseconds on the analysis thread and is not significant. The audio thread is not involved.

### Diagnosis
The fix the task asked for, "keep the last frame on screen until the first full frame at the new size", is already what the code does, so it cannot reduce the stutter. The stutter is the physical latency of an N-sample window plus the low frame rate at large N. The fixes must make frames available sooner.

### Recommended fix (dsp-engineer, in `SpectrumAnalyser`)
1. **History ring sized for `maxFftSize`.** Write the combined channel(s) into rings of 65536 samples and mask with `maxFftSize - 1` instead of `fftSize - 1`. Read a window as the most recent `fftSize` samples, starting at `writePos - fftSize`. On an `fftSize` change (and on `window` and `sampleRate`-independent changes), do not zero the rings, keep `samplesIngested` (capped), and set `countdownToNextFrame` so the next sample produces a frame. The first frame at the new size then comes within one 5 ms analysis pass, if at least `fftSize` samples of history exist. Only a `channelMode` or channel-count change needs a ring clear, because the rings hold the combined signal. The new bin count still needs `reprimeAveraging` and `resetSecondary` (the stale bins have a different meaning).
2. **Cap the hop.** Use `hopSize = min(fftSize / overlap, 2048)`, or equivalent, so large blocks still update at 23 fps or more (at 48 kHz). The alpha and the real-time-max decay are derived from `hopSeconds` (lines 191-217), so they stay correct. Estimated extra cost: a 65536-point real FFT per 2048 samples, about 23 per second per channel (my estimate: under 3 % of one core on the analysis thread; measure it). This changes what the Overlap control means for large sizes, so the architect should decide it.
3. Optional: when the averaging is re-primed after a size change, apply the average from the history frames instead of one raw frame, to avoid the noisy first frame. With 1, this needs several frames; with the cap in 2 they come quickly.

Expected cost: about a day for the dsp-engineer plus unit tests (the T-004/T-005 tests assume `reset()` zeroes the ring).

### Sources
- Code: `source/dsp/SpectrumAnalyser.cpp` 128-234, 289-312, 350-403; `source/dsp/SpectrumEngine.cpp` 71-161; `source/PluginProcessor.cpp` 75-126; `source/params/ParameterLayout.cpp` 38-55 (Block size 512 to 65536, overlap 1 to 8).
- Numbers are computed from `fftSize / sampleRate`; nothing measured.

## 3. Bug #16: glitch at very small window size in FL Studio

### Reproduction
**Not attempted**, for two reasons:
- The Standalone window enforces the constrainer through the OS (minimum track size), so it cannot reach sizes below 920x540 and cannot reproduce a host that bypasses `checkSizeConstraint`.
- No FL Studio instance is available to this session; I did not launch anything.

A repro in FL Studio is still needed to confirm the diagnosis below. Steps: open the plugin in FL Studio 25.1.2, drag its window to the smallest size quickly, and also try the FL title-bar resize handle in addition to the JUCE corner grip.

### What the code does at small sizes
- `InvestigateEditor` sets `setResizeLimits (920, 540, 2560, 1600)` and `setResizable (true, false)` (`source/PluginEditor.cpp:66-67`), with the values in `source/gui/DesignTokens.h:162-163`.
- `resized()` (lines 240-264) lays out with the actual `getWidth()`/`getHeight()` and has no guard for sizes below the minimum. `LayoutMath::computeRegions` (`source/gui/LayoutMath.h:46-56`) yields negative heights below the minimum. `SpectrumView::paintCurves` is guarded (`clip.isEmpty() || numCols <= 0`, line 1010), so the spectrum itself does not crash.
- `ViewStateStore::writeEditorSize` clamps what it stores, so a tiny size is never persisted.

### JUCE 8.0.15 VST3 wrapper (`third_party/juce-src/modules/juce_audio_plugin_client/juce_audio_plugin_client_VST3.cpp`)
- `checkSizeConstraint` (lines 2102-2133 and following) clamps the host rectangle to the constrainer's min and max. That only runs if the host calls it, as the VST3 `IPlugView` contract expects during a live resize.
- `onSize` (lines 2041-2074 and `ContentWrapperComponent::onSize` at 2349-2358) applies any size the host gives with `setBoundsConstrained`, which clamps the editor to the minimum, and then sets the peer bounds to the host's size. `resizingChild` suppresses the call back to the host.
- If the host window is made smaller than 920x540 without a prior `checkSizeConstraint`, the editor stays at 920x540 inside a smaller window and the host is not told: clipped or mismatched content, which matches a "glitch at very small sizes".

### Known issues from outside (all with JUCE VST3 in FL Studio or similar hosts)
- JUCE forum, "VST3 plugin editor resizing glitch/issues" (FL Studio 20.8, Live 10, Bitwig): a black piece of the editor remained after resizing from the FL corner, with no `resized()` or `paint()` calls. Cause in JUCE 6.0.7 high-DPI aspect-ratio logic in the VST3 wrapper; fixed on `develop` in February 2021: https://forum.juce.com/t/vst3-plugin-editor-resizing-glitch-issues/44170 . It also records two resize handles in FL (JUCE's and FL's own). A remaining issue was unwanted window movement when resizing from non-bottom-right corners while hitting size limits.
- JUCE forum, "VST3 issue in FL studio": an endless `getSize -> canResize -> checkSizeConstraint -> onSize -> getSize` loop in FL Studio (Mac) after opening the plugin's menu; Image Line fixed it in FL 20.5; making the plugin non-resizable also avoided it: https://forum.juce.com/t/vst3-issue-in-fl-studio/33414
- JUCE forum, "ComponentBoundsConstrainer + VST3 Resizing Issues": custom constrainers misbehave in several hosts: https://forum.juce.com/t/componentboundsconstrainer-vst3-resizing-issues/24152

### Verdict
**Host-triggered, and partly plugin-visible: not confirmed.** The most likely mechanism is the host resizing below the declared minimum while JUCE keeps the editor at the minimum. I could not run FL Studio. Direct2D repaint during a live resize is a second suspect (the plot cache is dropped in `SpectrumView::resized`, `cacheKey = {}`), but nothing in the code points to it specifically.

### Recommended fix or workaround
1. **Make the layout valid down to the smallest size the host can request.** The user wants a lower minimum anyway (findings #6 and #13), so lower `minWidth` and `minHeight` and make `computeRegions`, `ControlBar`, `MeterPanel` and `SpectrumView` degrade without negative rectangles. Guard `resized()` with `std::max` on every extent.
2. **Do not rely on the constrainer alone**: the plugin cannot stop FL from shrinking the window, but with a valid layout at the new minimum, a clamp to that minimum leaves nothing broken.
3. **Verify in FL Studio** after T-0xx: drag the window to the minimum quickly, via both resize handles, at 100 % and at a scaled display setting.
4. As a last resort, `setResizable (false, false)` in FL only (host detection through `juce::PluginHostType`) is the documented workaround for FL resize problems (forum thread above), but it removes resizing there; I do not recommend it unless the repro shows the glitch persists.

## Licenses and risks
- Fritsch-Carlson PCHIP is a published algorithm (1980) with no patent or licence issue; no code was copied. The SciPy reference implementation is BSD-3-Clause (permissive), but the method is simple enough to write from the paper.
- JUCE source excerpts above are read-only references to the pinned JUCE 8.0.15 tree (AGPLv3/commercial dual licence; no code was copied).
- Risks: #2 numbers come from a synthetic spectrum, not from real audio; the #15 timeline is computed from the sample counts, not measured; the #16 diagnosis is unverified in FL Studio.

## Sources
- https://en.wikipedia.org/wiki/Monotone_cubic_interpolation
- https://docs.scipy.org/doc/scipy/reference/generated/scipy.interpolate.PchipInterpolator.html
- https://forum.juce.com/t/vst3-plugin-editor-resizing-glitch-issues/44170
- https://forum.juce.com/t/vst3-issue-in-fl-studio/33414
- https://forum.juce.com/t/componentboundsconstrainer-vst3-resizing-issues/24152
- Local: `source/dsp/SpectrumMapper.cpp`, `SpectrumAnalyser.cpp`, `SpectrumEngine.cpp`; `source/gui/SpectrumView.cpp`, `SpectrumMath.h`, `LayoutMath.h`; `source/PluginEditor.cpp`, `source/PluginProcessor.cpp`; the pinned JUCE VST3 wrapper.
