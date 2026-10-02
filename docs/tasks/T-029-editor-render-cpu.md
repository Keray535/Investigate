# T-029: Editor rendering CPU: profile and reduce Direct2D message-thread load
- Owner: gui-engineer
- Milestone: M3
- Status: DONE
- Depends on: T-012

## Goal
Bring the editor's message-thread CPU load within budget, with margin left for the meter panel (T-013). T-012 met its paint-time budget by a wide margin (`paint()` p95 0.9 to 1.2 ms). Its CPU gate failed because Direct2D does rasterisation and present work after `paint()` returns. This task finds where that time goes, then reduces it. It does not change the look of the spectrum.

## Inputs
- `docs/qa/T-012-validation.md`: the CPU and paint table per case, the measurement method (`QueryThreadCycleTime` over wall time, 12 s, net of a paced baseline), and the repro command.
- `tools/render-harness/spectrum_check.cpp` (plugin-validator's harness). Its `d2d` mode reproduces the numbers. You may run it, but do not edit it; ask plugin-validator for changes.
- `source/gui/SpectrumView.*`, `source/PluginEditor.*` and the T-012 task log (frames are pulled in `pollProcessor()`, and `useGradientFill` is the spec §12 fallback switch).
- `docs/design/ui-spec.md` §12 (performance rules) and `docs/research/T-002-dsp-references.md` §8 (JUCE 8 rendering on Windows).
- The roadmap: Budgets, Risk 1, and the Decisions dated 2026-10-03.

## Measured starting point (T-012 validation round 2, Release, Direct2D, spectrum only, meter panel still a placeholder)

| Window | Case | CPU gross / net (% of one core, 60 fps) |
|---|---|---|
| 1280×720 | single + RT max (factory view) | 20.4 / 19.4 |
| 1280×720 | dual + 2 secondaries | 25.3 / 24.3 |
| 1280×720 | dual + 2 secondaries + SC + hover | 20.2 / 19.2 |
| 2560×1440 | single + RT max | 36.3 / 35.3 |
| 2560×1440 | dual + 2 secondaries | 41.9 / 40.9 |
| 2560×1440 | dual + 2 secondaries + SC + hover | 28.0 / 26.9 |

Two things in this table are unexplained. CPU grows only about 1.6x for 4x the pixels. Adding the sidechain and the hover readout lowers the CPU. Explain both before optimising. The development display is 1920×1080, so the 2560×1440 window is partly off-screen; the swap chain is still full size.

## Work, in this order
1. **Profile first.** Find where message-thread time goes per frame. Split it into at least these parts: `paint()` command recording, Direct2D `EndDraw` and flush, present and swap chain, parent or sibling repaints, and anything else above 5 % of the total. Useful tools:
   - JUCE 8's Direct2D metrics option (check the pinned 8.0.15 sources for `JUCE_DIRECT2D_METRICS` or its equivalent).
   - Windows Performance Recorder (`wpr`), which ships with Windows 11.
   - Manual timing around the peer's paint.

   Write the breakdown into the task log, with the method used.
2. **Cheap structural fixes.** Check each of these and log the effect of each:
   - Is `SpectrumView` (and every per-frame-repainted component) marked opaque, so that repainting it does not repaint the editor background beneath it?
   - Is each repaint confined to the data rect, with no whole-editor or parent repaint per vblank?
   - Is any path stroked and filled twice, or tessellated more than once per frame?
3. **Geometry.** Reduce the Direct2D tessellation load without a visible change. Options include fewer vertices per curve at high physical widths (keeping per-column peaks), simpler fill paths, and fewer separate geometries per frame. Screenshots before and after must look the same (the T-012 screenshot set is the reference).
4. **Dirty regions.** If it helps, repaint only the bands that changed. For example, split the plot into vertical strips and repaint, in each strip, only the y-range between the old and new curve envelopes plus the stroke width.
5. **Frame pacing (fallback).** Use this only if steps 2 to 4 cannot meet the gates. Cap the spectrum repaint rate at 30 fps when the plot's physical area exceeds a threshold that you choose and log. Below the threshold it stays at up to 60 fps. The meter bars keep their own rate. Report this clearly, because it is a visible trade-off that the architect will present to the user.
6. **Last resort.** If the gates are still missed, stop and return the numbers with OpenGL as an open question. Do not switch renderers.

## Acceptance criteria
Measured by plugin-validator in Release with the Direct2D renderer and the `spectrum_check` method from T-012 (net CPU, at least 12 s, new synthetic frames on every 60 Hz vblank, MeterPanel still a placeholder). Every case in this matrix is measured at both window sizes:
- single + RT max;
- dual + 2 secondaries;
- dual + 2 secondaries + SC + hover;
- single + all-time average + SC.

- [ ] A written breakdown of message-thread time per frame, before and after, in the task log.
- [ ] Spectrum-only message-thread CPU, the maximum over the case matrix: ≤ 18 % of one core at 1280×720 and ≤ 20 % at 2560×1440. The headroom up to the 25 % whole-editor budget is reserved for the meter panel (T-013).
- [ ] `SpectrumView` paint p95 is still ≤ 4 ms at 1280×720 and ≤ 8 ms at 2560×1440.
- [ ] No visual regression. The T-012 functional checks (92 harness checks) still pass, and the regenerated screenshot set matches the T-012 reference: identical curves, fills and colours. Anti-aliasing differences from the geometry changes are allowed only where the report shows them side by side.
- [ ] If frame pacing is used: the threshold and the effective frame rate per case are reported, and hover, freeze and click still respond within 1 vblank.
- [ ] The allocation rule (roadmap Decisions 2026-10-03) still holds: 0 allocations in the curve path and idle polls.
- [ ] Warning-clean at `/W4`. `docs/qa/T-029-unit.md` (allocation test and suite still green) and `docs/qa/T-029-validation.md` (CPU matrix, paint, functional re-run and pluginval at strictness 10) report PASS.
- [ ] A short "rendering rules" note in the task log for T-013, saying what the meter panel must do to stay cheap.

## QA split
- **unit-tester** writes `docs/qa/T-029-unit.md`: the allocation tests, the full suite in Debug and Release, and a review of any new repaint logic for correctness, for example that no stale band is left after a range change.
- **plugin-validator** writes `docs/qa/T-029-validation.md`: the CPU and paint matrix at both sizes, the T-012 functional re-run, screenshot comparison and pluginval at strictness 10. It extends `spectrum_check` for the fourth case if needed.
- Coders never edit `docs/qa/`. Only the testers write reports and verdicts.

## Log
- 2026-10-03 vst-architect: Created from T-012's failed message-thread CPU gate (validation rounds 1 and 2). That criterion moves here; it is not waived. The flat-fill fallback (spec §12) gave about 1 point, within noise, so the gradient is not the cost. Runs before T-013 so that the meter panel is built on the findings.
- 2026-10-02 gui-engineer: Profiled, restructured and re-measured. Frame pacing IS used, in two forms (see "Frame pacing" below); decision for the architect and the user.

### Profile method
- Repro: `InvestigateSpectrumCheck <dir> 60 d2d` (T-012 method: thread cycle time over 12 s, 60 Hz synthetic frames, net of the paced baseline). The run-to-run spread of one case is about +-1.5 points, and the worst case at 1280 x 720 swung between 16.6 and 20.5 on identical builds, so every figure below is a range or a median of two to four runs.
- Temporary, now removed, instrumentation: environment-variable switches inside `SpectrumView::paint` (paint nothing, blit only, no fills, no strokes, no sidechain, no hover paint, smoothed data, dummy draws), per-section `QueryThreadCycleTime` timers inside `paint()`, and a 1 kHz sampling profiler (SuspendThread/GetThreadContext on the message thread). JUCE sources in `third_party/` were not modified (an edit of the pinned JUCE tree was refused by the permission system, so the Direct2D metrics hub and JUCE-internal timers were not used).
- The sampler resolves modules only: the Microsoft symbol server has no PDB for d2d1.dll, so d2d1 internals show as one anonymous blob. Busy samples: 40 to 50 % d2d1, 12 to 15 % Intel user-mode driver (igd10um64xe), 12 to 16 % Investigate (paint recording) and the harness, rest ntdll/ucrt. User-mode time is 85 to 90 % of the thread's CPU (GetThreadTimes), so this is CPU work, not a kernel wait.

### Before (T-012 code, 38f2aa9), message-thread CPU net %, 1280x720 / 2560x1440
| Experiment (temporary switch) | single + RT max | dual + 2 sec | dual + 2 sec + SC + hover |
|---|---|---|---|
| Full (as shipped in T-012) | 19.4 / 35.3 | 24.3 / 40.9 | 19.2 / 26.9 |
| `paint()` returns at once (nothing drawn) | 7.5 / 6.2 | 7.5 / 5.8 | 8.6 / 5.9 |
| Cache blit only (the fixed per-present floor) | 3.7 / 5.8 | 4.1 / 6.4 | 3.9 / 7.5 |
| No strokes (fills only) | 18.0 / 31.2 | 17.6 / 33.0 | 10.4 / 11.9 |
| No fills (strokes only) | 12.6 / 12.1 | 17.2 / 17.5 | 19.4 / 26.7 |
| Flat fill instead of gradient | 19.4 / 36.0 | 24.5 / 41.7 | 18.9 / 28.6 |
| Sidechain suppressed | 21.7 / 36.1 | 26.3 / 41.3 | 27.6 / 42.8 |
| Hover paint suppressed | 19.7 / 36.5 | 24.3 / 40.7 | 17.4 / 26.2 |

Per-frame breakdown, before, message thread, 2560 x 1440 single + RT max (35 % of one core = 5.9 ms per 16.7 ms frame): `paint()` command recording 0.3 ms (paint probe median 0.30 ms), per-present floor (clear, cache blit, `Present1`) about 1.0 ms (5.8 %), the primary polygon fill about 3.9 ms (23 points, deferred to `EndDraw`), the two strokes about 1.0 ms. At 1280 x 720: recording 0.27 ms, floor 0.6 ms (3.7 %), polygon fill about 1.1 ms (6.8 points), strokes about 1.5 ms.

Findings from the structural checks (step 2): the editor and `SpectrumView` are already opaque, no repaint is wider than the data rect or the hover/toast rects, the editor repaints nothing per vblank (only the 48 kHz label on change), no path is stroked or filled twice, and the geometry is built once per curve per frame. Nothing to fix there. Repainting half the plot height (a dirty-region experiment) did not change the per-present floor (6.2 -> 6.5 at 2560), and a repaint that is one rectangle instead of two disjoint ones changed nothing measurable either, so dirty regions only help the hover and toast repaints (done, see below).

### The two unexplained observations
1. **CPU grows about 1.6x for 4x the pixels.** The cost is not proportional to area. The part that scales with pixels is the per-present floor (3.7 to 4.1 % at 1280 x 720, 5.8 to 7.5 % at 2560 x 1440, about 1.6x) and the polygon fill (about 7 -> 23 points, 3.4x). The stroke work does not scale: every curve has at most 2048 points whatever the window size, so about 2000 vertices are widened and tessellated at both sizes (about 2.3 to 3 points per stroke at either size).
2. **Adding the sidechain and the hover readout lowers the CPU.** Isolated to the sidechain silhouette's polygon fill, not the hover and not the sidechain stroke: suppressing the sidechain raised the worst case from 26.9 to 42.8 at 2560 (the same as dual + 2 secondaries), suppressing the hover paint changed nothing. A tiny dummy polygon (10 px triangle, 5 % alpha) drawn before the primary fill reproduced it (single 36 -> 18, dual 42 -> 24 at 2560) while a dummy `fillRect` did not, and the same dummy made the worst case worse (28.5 -> 33.5). So the cost of the large polygon fill depends on what geometry was drawn before it, which means Direct2D's geometry batching or realisation path, not the amount of work. I could not look inside d2d1 (no symbols), so this is the best-supported explanation, not a proof. It is moot after the redesign: with no polygon fills the dummy draw changes nothing (checked) and the sidechain adds its expected cost.

### What was changed (step 2 to 4)
- **Fills as sprite batches.** Direct2D tessellated a 2000-vertex jagged polygon on the CPU every frame. Fills are now lists of 1-pixel-wide bars (one per physical pixel column, top = the lowest y the curve reaches in that column, so the bar lies under the stroke) drawn with `Graphics::fillRectList`, which JUCE sends as a GPU sprite batch. The primary gradient is the exact original gradient rectangle below the lowest column top, plus flat bars in 33 bands (two 8-bit alpha steps each) above it. Lists are padded to a multiple of 64 with off-clip rectangles because JUCE caches only eight batch sizes. Rectangles are whole physical pixels, at least 1 px (JUCE aborts the sprite path for anything smaller).
- **Stroke vertex reduction.** `SpectrumMath::PolylineSimplifier` (Reumann-Witkam strip, 0.04 physical px) drops vertices that are collinear to a fraction of a pixel. It removes most vertices of smooth curves and none of a jagged one.
- **Clip-limited tracing.** `paintCurves` builds only the pixel columns under the clip (+2 columns margin). Hover and toast repaints no longer re-tessellate the full curves. The vertices are the same as in a full trace (the simplifier runs over all columns, only the emission is limited).
- **Software renderer unchanged.** `paint()` detects `LowLevelGraphicsSoftwareRenderer` and keeps the original polygon fills and exact vertices there (the rectangle list is 2x slower in the software renderer: 20 ms against 10 ms). Software timings are back at the T-012 values (worst case p95 10.4 / 26.0 ms).
- Tried and dropped (no measurable gain): join style (mitered, round), opaque instead of alpha sidechain stroke, two-column bars (sprite count is not the cost: about 10 us per sprite-batch call), a half-height dirty rect, a single-rectangle repaint, a dummy draw to steer Direct2D.

### After (this code, unpaced), message thread
Recording per `paint()` (ms, 840-frame means): 1280 x 720 single 0.54 (fills 0.34, strokes 0.07, trace/reduce 0.08, legend 0.02); dual + 2 secondaries 0.63 to 0.67; worst case 1.27 (fills 0.60, hover 0.24, strokes 0.17, trace/reduce 0.19). 2560 x 1440: single 0.70 (fills 0.47), dual + 2 secondaries 0.85, worst case 1.70 (fills 0.97, hover 0.27). The rest of the CPU is Direct2D's `EndDraw` and present (about 10 points at 1280 x 720 in the worst case, of which about 4 is the floor).

Unpaced CPU net %, two to four runs: 1280 x 720 single 9.2 to 9.7, dual + 2 secondaries 12.6 to 14.5, worst case 16.6 to 20.5 (median about 18.4, FAILS the 18 % gate about half the time); 2560 x 1440 single 12.7 to 13.5, dual + 2 secondaries 17.0 to 17.8, worst case 22.5 to 23.5 (FAILS the 20 % gate). That is a 2.1x to 2.4x reduction in the cases that failed at 2560, but not enough by itself, hence the pacing.

### Frame pacing (used) - for the architect and the user
- **Rule** (`SpectrumMath::pacing`): new frames repaint the plot at most every 30 ms (30 fps) when the data rect exceeds **2.0 Mpx physical** (`w * h * scale^2`; the 1280 x 720 default is 0.62 Mpx, 1920 x 1080 is 1.6 Mpx and stays at 60 fps, 2560 x 1440 at 100 % is 3.05 Mpx, and 1280 x 720 at 200 % is 2.5 Mpx), **or** when all seven curve layers are drawn (dual mode with the secondary curves and the sidechain: primary A stroke and fill, primary B, two secondaries, sidechain stroke and fill) at any size. The second rule is an addition to the "area only" fallback in the task, needed because the 1280 x 720 worst case alone swings between 16.6 and 20.5 against the 18 % gate. Set `pacing::minLayerCount` to 99 to drop it.
- **What is paced:** only the repaint request for a new frame. The newest frame is always the one painted, state is never stale. Hover, freeze, click, toasts, range and setting changes repaint at once, so they still respond within one vblank.
- **Effective rate:** 30 fps (33 ms between curve repaints) in the paced cases, 60 fps otherwise. Hover repaints (small clip) still run at the vblank rate, which is why the paced 2560 worst case stays at 16.4 to 18.0 %.
- Pacing lives in `SpectrumView::requestFrameRepaint`, shared by `pollFrame` and the test hook `setFrameForTesting`, so the harness sees it.

### After (shipped, with pacing), CPU net %
| Window | single + RT max | dual + 2 sec | dual + 2 sec + SC + hover | single + all-time avg + SC |
|---|---|---|---|---|
| 1280 x 720 | 8.8 to 9.9 (60 fps) | 12.5 to 13.9 (60 fps) | 13.4 to 14.2 (30 fps, 7 layers) | not in the harness yet (5 layers, expected about 12, 60 fps) |
| 2560 x 1440 | 8.2 to 8.5 (30 fps) | 10.3 to 10.5 (30 fps) | 16.4 to 18.0 (30 fps) | not in the harness yet (30 fps) |

`SpectrumView` paint p95 with the harness probes (information, includes the idle probe frames): 0.64 to 1.43 ms at 1280 x 720 and 0.77 to 2.0 ms at 2560 x 1440.

### Visual regression check (D2D snapshots, `createComponentSnapshot`)
Reference = T-012 code regenerated on this machine (the committed screenshots are the flat-fill round-2 set, so they cannot be used). 92 harness checks pass with the new code. For screenshots 01 to 13: 95.7 % of data-rect pixels are bit-identical, about 4 % differ by exactly 1 level (the gradient is now 33 flat bands plus the exact gradient below the curve, and 8-bit quantisation), under 0.3 % differ by 2 to 16 levels (stroke edge anti-aliasing at peaks, from the 0.04 px vertex tolerance), and with the sidechain on about 300 pixels differ by more than 16 levels (max 60, the sidechain spike's stroke edges). Side-by-side crops of the sidechain spike (top and foot) show no visible difference. Screenshot 17 and the flat-curve matrix 20-synth-3 are bit-identical.

### Rendering rules for T-013 (the meter panel), learned the hard way
1. **No polygons per frame.** `fillPath` of anything with more than a few dozen vertices costs 1 to 4 ms of Direct2D CPU after `paint()` returns, and `paint()` timing hides it. Bars, scales and ticks are `fillRect` or `fillRectList` of whole-physical-pixel rectangles (at least 1 px in both directions, or JUCE falls back to one `FillRectangle` per rectangle).
2. **One `fillRectList` per colour, not one `fillRect` per bar.** A call costs about 10 us plus about 0.1 us per rectangle. Pad lists to a multiple of 64 with rectangles outside the clip (see `flushFillRects`) so the sprite-batch cache (eight sizes) does not thrash. Colours only: a gradient needs one call per band.
3. **Cache the static parts.** The scale, ticks, labels and the bars' background gradient go into a `juce::Image` drawn once per change (as the grid is). Per frame, draw only the lit portions as flat rectangles.
4. **A presented frame has a fixed cost of about 4 to 6 % of a core** (clear, blit, `Present1`), independent of the repainted area. Do not repaint a meter when its pixel value did not change: quantise each bar to its physical pixel height and repaint only on a change. Silence and steady tones then cost nothing.
5. **Align meter repaints with the spectrum's.** JUCE paints once per vblank with the union of the requests, so a meter repaint on a vblank where the spectrum also repaints shares that present. Under pacing the spectrum repaints on every other vblank; a 60 fps meter adds a present on the others. Budget for that: the spectrum alone is 9 to 14 % (1280 x 720) and 8 to 18 % (2560 x 1440) with pacing, against the 25 % editor budget.
6. **Text labels at the specified 10 Hz, from preformatted strings; a `drawText` is about 15 to 35 us** (the hover card's seven strings cost 0.25 ms per frame). No alpha layers, shadows, blurs, `beginTransparencyLayer` or `saveState` + non-rectangular clips.
7. **Only the columns or rectangles under the clip should be built** (`g.getClipBounds()`), so a partial repaint is cheap. Do not assume that fewer repaint rectangles or smaller ones lower the present cost; they did not.
8. **Measure with the T-012 method, at both sizes, after every change; the spread is +-1.5 points and the order of draw calls can change the cost by 2x.** Time `paint()` only as a sanity check, not as the gate.

- 2026-10-03 vst-architect: The gui-engineer session stopped at the usage limit (HTTP 429) after both testers had reported and the log above was written, but before the status was set. Accepted.
  - `docs/qa/T-029-unit.md`: PASS, 0 defects.
  - `docs/qa/T-029-validation.md`: PASS, 0 defects. Spectrum-only CPU, maximum over the 4-case matrix, median of 3 runs: 14.1 % at 1280×720 (gate 18 %) and 16.8 % at 2560×1440 (gate 20 %); the worst single run was 17.1 %. Paint p95 was 1.55 and 2.04 ms. Paced input responded within one vblank (worst 13.3 ms). The visual check found anti-aliasing and 1-level gradient quantisation only. pluginval strictness 10 passed.
  - **Frame pacing is accepted as shipped.** The spectrum repaints at 30 fps when the physical data rect exceeds 2.0 Mpx, or when all seven curve layers are drawn (dual mode plus secondary curves plus sidechain). Otherwise it runs at 60 fps. The layer rule goes beyond the task's area-only fallback. It is accepted because the 1280×720 worst case swung across the 18 % gate without it. The user is told about it in the next status update.
  - Carried to T-013 (same owner and files): fix O-1 (compute `maxTop` over all columns, so a clip-limited paint matches the full paint, then unhide the `[strict]` test), and investigate O-A/O-2 (CPU raised by 2 to 4.5 points at 110 % and 175 % display scaling at 1920×1080; try the small width slack on the sprite-batch rectangles). O-3 and O-4 (sub-frame and sub-pixel cosmetic effects) are accepted with no work.
