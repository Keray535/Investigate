# Investigate: Spectrum zoom and pan (milestone M3b)

Created: 2026-10-03 by vst-architect. Linked from `docs/roadmap.md` (Scope, Milestones, Decisions).

## Vision

Users can look closely at any part of the spectrum. The mouse wheel zooms in and out around the pointer, a left-drag pans like a hand tool, Ctrl+drag zooms into a box, a double-click returns to the full view, and a small overview strip shows where the visible window sits. Zooming in shows real detail: the analysis maps its display points onto the visible range instead of stretching the full-range picture. This serves mixing and mastering engineers who want to inspect a resonance, a hum harmonic, or the fine level structure of one band without changing their range settings.

## Scope

### In scope (v1, milestone M3b)
- **Frequency and level zoom, anchored on the pointer**:
  - the wheel zooms frequency;
  - Ctrl+wheel zooms level (dB);
  - a wheel over an axis label gutter zooms that axis.
- **Pan:**
  - a left-drag or middle-drag pans both axes (the hand tool);
  - a horizontal wheel, or a horizontal two-finger touchpad swipe, pans frequency.
- **Fit:**
  - a double-click returns to the home range;
  - so does the Fit button in the zoom indicator.
- **Box zoom:** Ctrl+left-drag draws a rectangle and zooms both axes to it.
- **Overview strip** (mini-map), shown only while zoomed. It shows the primary curve over the home range and the visible window as a rectangle. Dragging the rectangle pans; clicking outside it centres the view there.
- **Zoom indicator**, shown only while zoomed. It shows the visible range and a Fit button. When fewer than 16 FFT bins lie inside the visible span, it also shows the bin spacing, so the user knows to raise the block size.
- **Zoom while frozen** keeps full detail. The engine keeps a snapshot of the bin-domain spectra.

### Out of scope
- Saving the zoom with the session or the project (the user chose not to persist it).
- Keyboard shortcuts for zoom and pan. Hosts own most keys, and Space is the host's transport key (ui-spec §7.2: no global shortcuts).
- A zoom history (back and forward), and zoom presets.
- Writing the zoom into the range parameters.
- Overscroll beyond the home range.
- Zoom for the meter panel.

## Architecture

### View model

There are two layers.

1. **Home range.** It is set by the existing APVTS parameters `freqLow`, `freqHigh`, `dbLow` and `dbHigh` through the Frequency and Level chips. Its effective values come from `source/dsp/DisplayRange.h`, as today: at least 1 octave and 12 dB, and limited by Nyquist. It is saved with the state, can be automated, and is what Fit returns to. Nothing in this plan changes the parameters, their ranges or their chips.
2. **View window.** It is a sub-range `(viewFreqLow, viewFreqHigh, viewDbLow, viewDbHigh)` that always lies inside the effective home range. It lives in the editor only. It is never written to a parameter, so it never creates host automation, undo steps, or a "project modified" flag. It is not persisted, so the editor always opens at Fit.

Rules:
- **Minimum view span:** 1/6 octave in frequency and 6 dB in level. The largest view is the home range.
- **Shared definitions:** `DisplayRange.h` defines the constants `minViewSpanOctaves = 1/6` and `minViewSpanDb = 6`, and the functions that clamp a view. The mapper and the editor both call them, the same pattern as the existing effective-range formulas.
- **Clamping:** pan and box zoom are clamped to the home range, with no overscroll.
- **Home range change:** this covers a chip edit, automation, a state load and a sample-rate change. The view is clamped into the new home range and keeps its span where possible. If it no longer fits, it returns to Fit.
- **Level zoom** is editor-only. The mapper does not use the dB range, because the editor clamps the curves.
- **Frequency zoom** goes to the analysis thread, so the 2048 display points are spread over the visible range.

### Interaction table (replaces the ui-spec §5.6 table once T-034 lands)

"Pointer" means a position inside the data rect unless stated otherwise. All the numbers are architect defaults. ui-ux-designer may refine them in T-034 and records any change with a reason.

| Input | Action |
|---|---|
| Wheel, vertical | Zooms frequency around the pointer. One notch scales the span by 0.8 (in) or 1.25 (out). The frequency under the pointer stays fixed. Shift gives fine steps (0.95 and 1.05). The event is consumed. |
| Ctrl+wheel | Zooms level around the pointer, with the same factors. The level under the pointer stays fixed. |
| Wheel over the frequency label gutter | Zooms frequency, anchored at the pointer's x. |
| Wheel over the dB label gutter | Zooms level, with no modifier needed, anchored at the pointer's y. |
| Horizontal wheel or two-finger swipe | Pans frequency. |
| Left-drag (> 4 px of movement) | Pans both axes. The cursor is a grabbing hand while dragging. It does nothing at Fit, because there is nothing to pan. |
| Middle-drag | The same as a left-drag. |
| Left click (≤ 4 px) with no second click within 250 ms | `requestSpectrumReset()`, as today but after the 250 ms double-click window (the same defer rule as the chips, ui-spec §7.1). |
| Double-click | Fit (the home range). No spectrum reset. |
| Ctrl+left-drag | Box zoom: a rubber-band rectangle; on release both axes zoom to it. An axis narrower than its minimum span is widened to the minimum, centred on the box. A box under 4 px in either dimension cancels. Esc also cancels when the host gives the editor keyboard focus. |
| Right-click | Copies the frequency under the pointer, unchanged. It follows the zoomed mapping. |
| Overview strip: drag the window rectangle | Pans, keeping the pointer's offset inside the rectangle. |
| Overview strip: click outside the rectangle | Centres the view on that point, keeping the span. |
| Indicator: the Fit button | Fit. |

The hover readout, crosshair and markers use the view mapping, so they show the correct frequency and level at every zoom. How the hover card behaves during a pan or box drag (for example, hidden while dragging) is specified by T-034.

### Data flow

```
Editor (message thread)
  ViewWindow (fLo, fHi, dbLo, dbHi) ⊆ effective home range (DisplayContext + DisplayRange.h)
    ├─► SpectrumView mapping: curves, grid, hover, right-click copy (dB zoom stops here)
    ├─► processor.setSpectrumView (fLo, fHi)   at most once per editor update, on frequency changes only
    └─► processor.setSpectrumFrozen (bool)     on every freeze change, including auto-unfreeze
          │
          ▼  one std::atomic<uint64_t> holding both view floats (never torn);
          │  one atomic holding the freeze flag and a freeze epoch
Analysis thread (polls every 5 ms)
  SpectrumSettings gets viewFreqLow, viewFreqHigh, frozen, freezeEpoch
  SpectrumEngine::processPending
    live:   new FFT hop ─► map onto the view ─► publish
            no new hop, but the view or home range changed ─► re-map the latest bin-domain spectra ─► publish
    frozen: on the first frozen poll, copy the bin-domain spectra into a preallocated snapshot;
            analysis keeps running internally (so click-to-reset still works), but live frames are not published;
            a view or home-range change ─► re-map the snapshot ─► publish
    unfreeze: publish the live state at once (no wait for the next hop)
  Each frame: the curves over the view, the view range used, a 256-point overview of primary A
  over the home range, and the freeze epoch
          │
          ▼  FrameExchange (lock-free triple buffer)
Editor: drops frames whose epoch is not the current one; until the re-mapped frame arrives,
        it draws the current frame through the new mapping (the points carry freqHz)
```

`processBlock` and the audio thread do not change. The output stays bit-exact.

### Changes per module

| Module | Owner | Change |
|---|---|---|
| `source/dsp/DisplayRange.h` | dsp-engineer | `minViewSpanOctaves`, `minViewSpanDb`, `clampViewFrequency (lo, hi, homeLo, homeHi)` and `clampViewDb (lo, hi, homeLo, homeHi)`. All are pure, shared by the mapper and the editor. |
| `SpectrumSettings` | dsp-engineer | `viewFreqLow` and `viewFreqHigh` (0 means the home range); `frozen`; `freezeEpoch`; `overviewPoints` (256). |
| `SpectrumMapper` | dsp-engineer | Maps onto the clamped view. Smoothing is computed over a range padded by half a smoothing window on each side, clipped to the effective home range (so the curves at Fit stay bit-identical to today), and only the visible points are published. The scratch is preallocated for the worst case: the minimum span with 1/3-octave smoothing, about 3 × `maxPoints`. Overview mapping. |
| `SpectrumEngine` | dsp-engineer | Re-publishes on a view or home-range change without a new FFT. A re-publish must leave the analyser state untouched. Freeze snapshot, preallocated in `prepare` for the largest bin count (65536 / 2 + 1 bins × 5 curves, about 0.66 MB), together with `binHz`, `numBins`, `isDual`, `hasSecondary`, `hasSidechain`, smoothing and slope at freeze time. |
| `SpectrumFrame` | dsp-engineer | `viewFreqLow` and `viewFreqHigh` (the range this frame was mapped for); `overviewFreqHz[256]` and `overviewDb[256]`; `freezeEpoch`. |
| `InvestigateProcessor` | plugin-engineer | `setSpectrumView (float fLo, float fHi) noexcept` and `setSpectrumFrozen (bool) noexcept` (message thread); `getFreezeEpoch() const noexcept`; the AnalysisThread fills the new settings fields. `setAnalysisActive (false)` resets the view to the home range and the freeze to off. |
| `source/gui/ZoomMath.h` (new) | gui-engineer | Pure arithmetic: anchored zoom, pan, clamping (through `DisplayRange.h`), box-to-view conversion, the home-change rule, and deep-zoom tick selection. |
| `SpectrumView` | gui-engineer | View-based mapping; wheel, drag, click-defer and double-click handlers; box zoom; the indicator; the overview strip; the grid rules for deep zoom; no full cache rebuild per frame during an interaction. |
| `docs/design/ui-spec.md`, `mockup.html`, `tokens.json` | ui-ux-designer | A new §5.9 plus updates to §5.5 to §5.7, §12 and §13.3. A working mockup of every gesture. |

### Freeze semantics (amends ui-spec §5.7)
- Freeze still stops live updates. While frozen, zoom and pan re-map the frozen snapshot at full detail.
- Smoothing, slope, the secondary mode and the other analysis settings stay as they were at freeze time. A home-range change redraws the frozen data in the new range, as §5.7 already says.
- Auto-unfreeze on a `channelMode` change and click-to-reset while frozen work as before.
- The freeze epoch guarantees that the editor never shows a live frame that was published just before the freeze took effect.

## Budgets

Measured as in the roadmap: Release build on the development machine, 48 kHz.

| Item | Budget |
|---|---|
| View change to the re-mapped frame being published | p95 ≤ 10 ms, max ≤ 20 ms, at any `fftSize` and overlap (including 65536 with 1x) |
| One re-map: every curve plus the overview, worst case (dual mode, both secondary curves, sidechain, 1/3-octave smoothing, minimum span) | ≤ 1.0 ms on the analysis thread |
| Analysis thread during a continuous 60 fps pan, at the default settings | ≤ 4 % of one core |
| Whole-editor message thread during a continuous pan or zoom (one view change per editor update) | ≤ 25 % of one core at 1280×720 and 2560×1440 (T-012/T-029 harness, median of 3; the real editor path at the native refresh rate is reported) |
| Editor paint during a pan | p95 ≤ 4 ms at 1280×720 and ≤ 8 ms at 2560×1440 |
| Per-frame allocation in the pan and zoom paths | none in Investigate code; text follows the ≤ 20 per frame rule (roadmap decision of 2026-10-03) |
| Audio thread | unchanged: bit-exact output, 0 allocations, `processBlock` budgets as in the roadmap |
| Memory | the freeze snapshot adds about 0.7 MB, which is inside the 64 MB per-instance budget |

## Milestone M3b: tasks

| Task | Title | Owner | Depends on |
|---|---|---|---|
| T-034 | Zoom and pan: UI spec addendum and mockup | ui-ux-designer | none |
| T-035 | Spectrum engine: view-range mapping, re-publish and freeze snapshot | dsp-engineer | T-005, T-009 |
| T-036 | Processor: view and freeze API, analysis-thread wiring | plugin-engineer | T-035 |
| T-039 | Spectrum engine: pan margin and bin spacing in the frame (from T-034 open questions 5 and 6) | dsp-engineer | T-035, T-036 (build order) |
| T-041 | Processor: request the visible point count, not the frame capacity | plugin-engineer | T-039 |
| T-037 | SpectrumView: zoom, pan, Fit, indicator and deep-zoom grid | gui-engineer | T-034 (mockup approved by the user), T-036, T-039, T-041 |
| T-038 | SpectrumView: overview strip and box zoom | gui-engineer | T-037 |

Order:
- T-034 is documentation only. It may run in parallel with any C++ task.
- T-035, T-036, T-040, T-039, T-041, T-037 and T-038 build C++, so they run one at a time, in that order (the one-C++-task rule).
- T-035 starts after T-032 has finished its configure run.
- The M4 tasks T-015, T-016 and T-017 depend on T-038, so the validation suite covers the final UI.

**M3b gate:**
- The user approves the T-034 mockup before T-037 starts.
- Every unit and validation QA report for T-035 to T-038 says PASS, and the budgets above are met.
- The user completes the hands-on checklist below.

## Risks

| # | Risk | Impact | Mitigation |
|---|---|---|---|
| Z1 | Rebuilding the grid and labels on every frame during a pan exceeds the message-thread budget | Laggy pan; a sluggish host UI | Grid lines as one rectangle list per colour during an interaction, label images reused while their text is unchanged, and the T-029 rules. T-037 gates CPU during a continuous pan at both sizes. |
| Z2 | A live frame published just before a freeze, or a torn view pair, shows the wrong data | Flicker, or a wrong curve after a freeze or zoom | A freeze epoch in every frame (the editor drops stale epochs), and the view pair packed into one 64-bit atomic. T-036 stress-tests both. |
| Z3 | Zooming past the FFT resolution shows straight interpolated segments, which may look like a bug | User confusion | The indicator shows the bin spacing below 16 bins in view. The checklist explains it. |
| Z4 | Windows precision touchpads deliver pinch as Ctrl+wheel, so pinch zooms level, not frequency | Unexpected on laptops | Accepted v1 limitation, documented. A horizontal swipe still pans frequency. Revisit after user feedback. |
| Z5 | The 250 ms click defer makes the curve reset feel slower | Minor | It is the same rule as the chips, and the toast confirms the reset. |
| Z6 | Smoothing at the view edges differs from the unzoomed curve | Curves "bend" at the edges while panning | Padded smoothing in the mapper. T-035 tests the edges against a wider view within 0.05 dB. |

## Decisions

| Date | Decision | Reason |
|---|---|---|
| 2026-10-03 | Zoom and pan go into v1 as milestone M3b, between M3 and M4. | User request. M4 must validate the final UI. |
| 2026-10-03 | Zoom is a view layer inside the home range set by the range parameters. It never writes parameters. | User choice. It keeps automation and undo clean, and the parameter bounds (`freqLow` ≤ 1 kHz, `dbLow` ≤ −30 dB) would block close zooms. |
| 2026-10-03 | Gestures: the wheel zooms frequency; Ctrl+wheel zooms level; left-drag and middle-drag pan; a double-click fits; Ctrl+drag box-zooms; an overview strip appears while zoomed. The click reset is deferred by 250 ms. There is no Space shortcut. | User choices. Space is the host's transport key. |
| 2026-10-03 | The zoom is not persisted. The editor opens at Fit, and `ViewState` gains no properties. | User choice (persistence was offered and not selected). |
| 2026-10-03 | The engine re-maps the latest bin-domain spectra on a view change without a new FFT, and holds a bin-domain snapshot while frozen. Frames carry a freeze epoch. | At 65536 points with 1x overlap a new frame arrives only every 1.37 s. Zooming a frozen spectrum must show detail. This partly supersedes "Freezing the display is editor-only". |
| 2026-10-03 | The minimum view span is 1/6 octave and 6 dB, defined in `DisplayRange.h`. The home range keeps its 1-octave and 12 dB rules. | Architect default. ui-ux-designer may refine it in T-034. |

## Status

| Task | Owner | Status |
|---|---|---|
| T-034 | ui-ux-designer | DONE (mockup approved by the user without changes) |
| T-035 | dsp-engineer | DONE (`docs/qa/T-035-unit.md` PASS) |
| T-036 | plugin-engineer | DONE (both QA reports PASS) |
| T-040 | unit-tester | DONE (test-only follow-up of T-036; `docs/qa/T-040-unit.md` PASS) |
| T-039 | dsp-engineer | DONE (`docs/qa/T-039-unit.md` PASS) |
| T-041 | plugin-engineer | TODO (one-line constant change; runs before T-037) |
| T-037 | gui-engineer | TODO |
| T-038 | gui-engineer | TODO |

## User hands-on checklist (at the M3b gate)

Use the Release build, in a DAW and in the Standalone, with music playing.

- [ ] The wheel over the spectrum zooms frequency, and the point under the mouse stays put. Zooming out stops at the chips' range.
- [ ] Ctrl+wheel zooms the level axis. A wheel over the dB labels does the same without Ctrl.
- [ ] Left-drag (and middle-drag) pans. It cannot go past the chips' range.
- [ ] A double-click returns to the full view, with no curve reset. A single click still resets the max/avg curves, shortly after the click.
- [ ] Ctrl+drag draws a box and zooms into it. A tiny box does nothing.
- [ ] While zoomed, the overview strip shows where you are. Dragging its window pans, and clicking beside it jumps there.
- [ ] Zooming into a narrow band shows more detail, not a blurry stretch. With a small block size, the indicator shows "bin … Hz".
- [ ] Freeze, then zoom and pan: the frozen curve shows detail and does not move with the music.
- [ ] Changing the Frequency or Level chips while zoomed keeps the view sensible, or returns to Fit.
- [ ] Close and reopen the editor: it opens at the full view.
- [ ] The DAW stays responsive while you pan continuously in a large window.
- [ ] Note: on a laptop touchpad, pinch zooms the level axis (a known limitation). A two-finger sideways swipe pans frequency.
