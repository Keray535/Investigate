# Investigate: UI specification

- Task: T-010. Author: ui-ux-designer. Date: 2026-09-28. Status: final. The user approved `mockup.html` without changes on 2026-09-28. The layout and visual design are frozen for T-011 to T-013. The final pass reconciled the spec with the roadmap Decisions dated 2026-09-28, finalised the fonts (§3.2) and added the About view (§6.2).
- Addendum T-034 (2026-10-03, milestone M3b): spectrum zoom and pan, new §5.9, with updates to §4.1, §5.3, §5.5 to §5.7, §6, §7.2, §10, §11, §12, §13.3 and §15. The updated mockup awaits the user's approval; T-037 does not start before it. All changes are listed in §16.
- Companion files: `docs/design/tokens.json` (the source of truth for every value named `token.*` below), `docs/design/mockup.html` (interactive preview), `assets/ui/*.svg`.
- All sizes are logical pixels at 100 % scale. "x, y, w, h" rectangles are in editor coordinates, with the origin at the top-left of the editor, unless a rectangle is marked as relative to a parent.
- Minus signs in displayed text are U+2212 (−), never ASCII hyphen. Multiplication signs are U+00D7 (×).

## 0. Users and design intent

- **Users:** mixing and mastering engineers, and home producers comparing their mix with a reference. The plugin sits on the master bus or a group, often for a whole session, and is glanced at more than it is operated.
- **Hosts and screens:** Windows 11 DAWs (Reaper, Cubase, Studio One, Ableton Live, FL Studio, Bitwig) and the Standalone app. Screens range from 1366 × 768 laptops at 100 % scale to 4K monitors at 150 % to 200 %.
- **Priorities:**
  - Primary, always visible and large: the spectrum, the channel mode, the level bars, and the loudness figures.
  - Secondary, visible in the control bar: block size, overlap, window, averaging, smoothing, slope, secondary curve, and the display ranges.
  - Set once, visible but small: the meter scale and the RMS window.
  - Everything is also in the Settings panel.
- **Identity:** "Investigate" is a forensic, precise tool. The palette is a blue-black ink with an aqua signal colour and an amber second channel, which is deliberately far from SPAN's olive fill and grey chrome. The logo is a monoline lowercase wordmark whose first "i" dot is a small lens ring. The controls are flat "chips" (label over value) instead of knobs: they are denser, readable at a glance, and cheap to render.

References borrowed as function only:
- From SPAN (T-001 §5): the functional zoning, meaning a dominant spectrum, a narrow meter column, and statistics. Investigate puts every statistic in the right-hand column rather than a bottom band, and puts its settings in a bottom control bar.
- From common DAW meters: the stacked safe, warm and hot zones, and K-system colour zones (Katz: below 0 is safe, 0 to +4 is warm, above +4 is hot).
- From measurement tools: a crosshair with a floating readout card.

No artwork, colour scheme, logo or text is taken from any of them (see §14).

## 1. Rendering approach

**Native JUCE 8** with a custom `LookAndFeel` (`source/gui/InvestigateLookAndFeel`) and `juce::Component` subclasses. The WebView is rejected for four reasons:
- The spectrum must repaint at up to 60 fps from a lock-free frame buffer. A WebView would add a JS bridge and serialisation per frame.
- A WebView adds 30 MB to 80 MB of memory per instance, which breaks the ≤ 64 MB budget (roadmap Budgets).
- WebView2 behaviour differs between host processes.
- Everything in this design (flat fills, 1 px lines, one gradient, text) is cheap in Direct2D.

Rendering rules:
- Direct2D (the JUCE 8 default) renders everything. No OpenGL context unless T-012 misses its paint budget and the architect approves (T-002 §8).
- `juce::VBlankAttachment` on the editor drives both the spectrum and the meter bars (§12).
- SVG assets are loaded once into `juce::Drawable`s from the `InvestigateAssets` binary-data target and recoloured with `Drawable::replaceColour` or by drawing with `currentColor` replaced by a token.

## 2. Canvas and sizing

| Property | Value |
|---|---|
| Default size | **1200 × 700** |
| Minimum size | **920 × 540** |
| Maximum size | **2560 × 1600** |
| Aspect ratio | Free. Width and height are constrained independently. |
| Resizing | `setResizable (true, true)` plus a 12 × 12 corner grip at (W − 12, H − 12) drawn as six 1 px-radius dots in `text-tertiary`. Host resizing is honoured through `juce::ComponentBoundsConstrainer` with the limits above. |
| Persistence | `ViewState.editorWidth` / `editorHeight` are written on every resize (message thread) and restored in the editor constructor, clamped to the limits. `ViewState.metersCollapsed` (bool, default false). |
| Scale steps | No internal zoom menu. Text sizes are fixed in logical pixels and the OS/host DPI scale (100 %, 125 %, 150 %, 175 %, 200 %) is applied by JUCE. Growing the window enlarges the plot and meter bars, not the text. |
| HiDPI | JUCE's per-monitor DPI awareness. The cached grid image (§5.3) is allocated at `logicalSize × scale`, where `scale` comes from `juce::Component::getApproximateScaleFactorForComponent (this)`. It is rebuilt when that factor changes, so it is crisp at 200 %. The 1 px lines in this spec are 1 logical px (2 physical px at 200 %). They are drawn at `round(x) + 0.5` logical, which lands on physical pixel edges at both 100 % and 200 %. |

Why 1200 × 700: it fits a 1366 × 768 laptop with host chrome. It keeps the plot at 840 × 564, enough for dense frequency labels and 6 dB grid steps at the default range. It stays under the 1280 × 720 paint-budget reference. The minimum of 920 × 540 is the smallest size at which the meter strip keeps every readout, and the control bar keeps the Analysis and Display groups on one line (§4.3).

## 3. Design tokens

The full list, with W3C-draft structure, is in `tokens.json`. gui-engineer generates or hand-writes `source/gui/DesignTokens.h` from it (T-011). No colour, size or duration below may be hard-coded outside that header.

### 3.1 Colours

| Token | Hex | Role |
|---|---|---|
| `bg-app` | #0E1219 | Header and control-bar background |
| `bg-plot` | #0A0D13 | Spectrum region, meter tracks, text on accent fills (`text-on-accent`) |
| `surface` | #131924 | Meter strip, Settings popover, menus |
| `surface-2` | #1A2130 | Chip and segmented-control fill; clip lamp off |
| `surface-3` | #232C3E | Hover fill, tooltip fill |
| `card` | #151B27 | Hover readout card |
| `border` | #2A3447 | Region separators, popover and card borders, dividers |
| `border-subtle` | #1A2130 | Plot data-rect frame |
| `grid-minor` | #161D2A | Unlabelled and labelled non-decade frequency lines, dB lines |
| `grid-major` | #243047 | Decade frequency lines (10, 100, 1k, 10k), the 0 dB line |
| `text-primary` | #E8ECF3 | Values and readouts |
| `text-secondary` | #A3AEC2 | Labels, units, legend, channel letters |
| `text-tertiary` | #75829A | Axis labels, section titles, meter scale |
| `text-disabled` | #4A5468 | Inactive only |
| `accent` | #62D6F0 | Selected segment, active toggle text and border, focus ring, drag highlight |
| `accent-tint` | #1B313B | Active toggle fill |
| `state-frozen` | #8FB8FF | Freeze button, FROZEN badge, frozen plot frame |
| `state-frozen-tint` | #283347 | Freeze button fill while active |
| `state-warn` | #EAC75A | Range values that are clamped to an effective value |
| `state-error` | #FF4F64 | Invalid text entry flash (2 px outline, 300 ms), negative headroom text |
| `meter.safe` / `warm` / `hot` / `clip` | #34C6A8 / #EAC75A / #FF8A3D / #FF4F64 | Meter zones and clip lamp |
| `meter.safe-peak` / `warm-peak` / `hot-peak` | #217365 / #857339 / #91522A | Peak segment above RMS (zone colour at 55 % over `bg-plot`, pre-blended so no alpha is drawn) |
| `meter.track-line` | #1A2231 | 1 px line across tracks at each labelled tick |
| `meter.hold-tick` | #E8ECF3 | Peak-hold marker |

Curve colours are in §5.1.

### 3.2 Typography

| Token | Family | Size / line | Weight | Use |
|---|---|---|---|---|
| `caps` | Inter | 10 / 12, uppercase, +0.06 em tracking | 600 | Chip labels, section titles, table header, channel letters |
| `body-sm` | Inter | 11 / 14 | 500 | Legend, table row labels, tooltips, toasts, hint line |
| `body` | Inter | 12 / 16 | 500 | Buttons, segmented control, popover row labels, menu items |
| `choice` | Inter | 13 / 16 | 500 | Choice values in chips (Hann, RT max, ...) |
| `value` | JetBrains Mono | 13 / 16 | 500 | Numeric values in chips |
| `unit` | Inter | 10 / 12 | 500 | Unit suffix after a number, 3 px gap, `text-secondary` |
| `readout` | JetBrains Mono | 12 / 16 | 400 | Meter table, correlation value, balance, hover rows |
| `readout-lg` | JetBrains Mono | 18 / 22 | 500 | Loudness M, S, I |
| `readout-hover` | JetBrains Mono | 15 / 18 | 500 | Hover frequency |
| `axis` | JetBrains Mono | 11 / 12 | 400 | Spectrum axis labels |
| `axis-meter` | JetBrains Mono | 10 / 12 | 400 | Meter scale labels |

Every number is set in the monospaced face so digits do not jitter. JUCE cannot enable OpenType `tnum`, so a monospace font is the reliable way to get tabular figures.

**Fonts (bundled; license approved by T-022, roadmap Decision 2026-09-28):**

| Font | Pinned release | Files in `third_party/fonts/` (not `assets/ui/`) | Path inside the release zip | License file | Copyright notice |
|---|---|---|---|---|---|
| Inter | `v4.1`, https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip | `Inter-Medium.ttf` (500), `Inter-SemiBold.ttf` (600). Weight 400 is not used for Inter. | `extras/ttf/`. Do not use `InterDisplay-*` or `InterVariable*`. | `OFL-Inter.txt` (the repository's `LICENSE.txt` at `v4.1`, unmodified) | "Copyright (c) 2016 The Inter Project Authors (https://github.com/rsms/inter)" |
| JetBrains Mono | `v2.304`, https://github.com/JetBrains/JetBrainsMono/releases/download/v2.304/JetBrainsMono-2.304.zip | `JetBrainsMono-Regular.ttf` (400), `JetBrainsMono-Medium.ttf` (500) | `fonts/ttf/`. Do not use `JetBrainsMonoNL-*`. | `OFL-JetBrainsMono.txt` (the repository's `OFL.txt` at `v2.304`, unmodified) | "Copyright 2020 The JetBrains Mono Project Authors (https://github.com/JetBrains/JetBrainsMono)" |

- **License:** SIL Open Font License 1.1 for both, with no Reserved Font Name. Embedding in the plugin binary is allowed.
- **License obligations** (T-022):
  - the OFL text and copyright notices ship with the installer (T-019) and appear in the About view (§6.2);
  - the font files are never sold or offered as a download on their own.
- **Files:** the TTFs are unmodified and not subset. If a later task subsets them for binary size, it must keep the `name` table and the family names (T-022 action item 4).
- **Loading (T-011):**
  - T-021 downloads the four TTFs and the two OFL texts, with SHA-256 checks, into `third_party/fonts/` and compiles them into the `InvestigateAssets` binary-data target. The symbols are `BinaryData::InterMedium_ttf`, `InterSemiBold_ttf`, `JetBrainsMonoRegular_ttf`, `JetBrainsMonoMedium_ttf`, `OFLInter_txt` and `OFLJetBrainsMono_txt`. The files are not in `assets/ui/` (roadmap Decision 2026-09-28; T-011 correction, roadmap Decision 2026-10-01).
  - The editor creates each typeface once with `juce::Typeface::createSystemTypefaceFor (data, size)`.
  - The LookAndFeel maps each token in the table above to its typeface by family and weight: Inter 500, Inter 600, JetBrains Mono 400, JetBrains Mono 500. No synthetic bold is used.
- **Fallbacks:** Segoe UI stands in for Inter and Consolas for JetBrains Mono. Both ship with Windows and are referenced by name only, never embedded. A fallback is used only if a bundled typeface fails to load (a missing resource or `createSystemTypefaceFor` returning null). The layout does not change, because every size in this spec is in logical pixels. Since T-021 the resources are always compiled in, so the fallback is a defensive rule only.

### 3.3 Spacing, radii, strokes, shadow, motion

- **Spacing scale:** 2, 4, 6, 8, 12, 16, 20, 24, 32. Region padding is 12. Chip gap is 8. Control-bar group gap is 20.
- **Radii:** meter bars and lamps 2; small pills (badge, toast, legend, entry, tooltip, clip cells, segments) 4; chips, buttons, segmented track and card 6; popover 8.
- **Strokes:**
  - hairline 1 (grid, borders, dividers, crosshair, secondary curves, sidechain edge);
  - primary curves 1.5;
  - hover markers 1.5;
  - focus ring 2 (drawn 1 px outside the control bounds);
  - peak-hold tick 2.
- **Shadow:** exactly one, on the Settings popover: offset (0, 8), blur 24, black at 50 %. Render it once into a cached image when the popover opens (`juce::DropShadow::drawForRectangle` into an `Image`). No other shadows, glows or blurs anywhere.
- **Motion:**
  - click-defer 250 ms (§7.1);
  - tooltip delay 700 ms;
  - copy toast 1200 ms and reset toast 900 ms, each fading out over its last 200 ms;
  - external-change dot 600 ms;
  - error flash 300 ms.
  
  Nothing else animates. The popover, menus, tooltips and hover card appear and disappear instantly.

## 4. Layout

### 4.1 Regions

```
+----------------------------------------------------------------------------------+
| HEADER 44: wordmark | Mode segmented | ...  SR | SC | Freeze | Reset curves | Mtr | Set |
+----------------------------------------------------------+-----------------------+
|                                                          | METER STRIP 296       |
|  SPECTRUM PLOT (flex)                                    |  Scale | RMS | Reset  |
|   dB labels in 48 px left gutter, freq labels in 24 px   |  bars | table          |
|   bottom gutter                                          |       | loudness       |
+----------------------------------------------------------+       | correlation    |
| CONTROL BAR 56: Analysis | Display | Range               |                       |
+----------------------------------------------------------+-----------------------+
```

- **Header:** (0, 0, W, 44), `bg-app`, 1 px `border` bottom line.
- **Spectrum panel:** (0, 44, W − MS, H − 44), where MS = 296, or 0 when the meters are collapsed. It contains:
  - the **plot region** (0, 44, W − MS, H − 100), filled with `bg-plot`;
  - the **control bar** (0, H − 56, W − MS, 56), filled with `bg-app`, with a 1 px `border` top line.
- **Meter strip:** (W − 296, 44, 296, H − 44), `surface`, 1 px `border` left line. It is absent when collapsed.
- **Data rect** (the plotted area inside the plot region): x = 48, y = 44 + 12, w = (W − MS) − 64, h = (H − 100) − 36. The gutters are left 48 (dB labels), right 16, top 12, bottom 24 (frequency labels). The data rect has a 1 px `border-subtle` frame.
- **Zoom and pan (T-034, §5.9):** the left gutter (dB labels) and the bottom gutter (frequency labels) are also wheel targets for zooming their axis. While the view is zoomed, the zoom panel (indicator and overview strip) sits at the data rect's top-right corner (§5.9.6). Neither changes the region rectangles above.

### 4.2 Header contents (right-anchored items use W)

| Item | Rect | Notes |
|---|---|---|
| Wordmark | (16, 13, 81, 18) | `logo-wordmark.svg` in `text-primary`, height 18 |
| Mode, single group | (116, 8, 200, 28) | Track `surface-2`, radius 6, padding 2. Segments 24 high, at x offsets +2: Mid 44, Side 44, L 32, R 32, L+R 44 |
| Mode, dual group | (324, 8, 92, 28) | Segments L/R 44, M/S 44 |
| Sample rate text | (W − 444, 8, 60, 28) | Right-aligned `axis` font in `text-tertiary`, for example "48 kHz" (§8.8) |
| SC toggle | (W − 368, 8, 60, 28) | 16 px sidechain icon + "SC" |
| Freeze toggle | (W − 302, 8, 84, 28) | 16 px freeze icon + "Freeze" |
| Reset curves | (W − 212, 8, 116, 28) | 16 px reset icon + "Reset curves" |
| Divider | (W − 84, 14, 1, 16) | `border` |
| Meters toggle | (W − 72, 8, 28, 28) | 16 px meters icon, drawn mirrored horizontally when collapsed |
| Settings | (W − 40, 8, 28, 28) | 16 px settings icon. A 6 px `accent` dot at its top-right (3, 3 inset) when any control-bar group is hidden (§4.3) |

At the minimum width (920) the sample-rate text starts at x = 476, clear of the Mode control, which ends at 416.

### 4.3 Control bar (inside the spectrum panel)

Chips are 40 high at y = H − 48, with radius 6, fill `surface-2`, horizontal padding 8 and top padding 5. The label (`caps`, `text-secondary`) sits above the value (baseline about 4 px below the label).

| Group | Chips, in order, with widths | Group width |
|---|---|---|
| A, Analysis | Block 60, Overlap 68, Window 76, Avg time 72, Smooth 68 | 376 |
| B, Display | Slope 92, 2nd curve 96 | 196 |
| C, Range | Frequency 140 (two halves: low … high), Level 120 (two halves: low … high dB) | 268 |

Placement rules, with `avail = (W − MS) − 24`:
1. Group A starts at x = 12, with 8 between chips.
2. Group B starts 20 after group A ends, with a 1 × 24 `border` divider centred in the gap (y = H − 40).
3. Group B is shown only if 376 + 20 + 196 ≤ avail, which is 592.
4. Group C is shown only if 592 + 20 + 268 ≤ avail, which is 880. It is **right-aligned** so it ends at x = 12 + avail, with a divider centred between the end of group B and the start of group C.
5. Hidden groups stay reachable in the Settings panel, and the Settings dot appears.

Resolved positions:

| Size | Visible groups | Chip x positions |
|---|---|---|
| 1200 × 700 (default) | A, B, C | Block 12, Overlap 80, Window 156, Avg 240, Smooth 320, divider 398, Slope 408, 2nd 508, divider 614, Frequency 624, Level 772 (ends 892) |
| 920 × 540 (minimum) | A, B | As above up to 2nd (ends 604). C hidden, Settings dot on. |
| 1200 × 700, meters collapsed | A, B, C | A and B as default. Frequency 920, Level 1068 (ends 1188). |

### 4.4 Meter strip (strip-relative coordinates; strip origin = (W − 296, 44), strip height SH = H − 44)

| Item | Rect (strip-relative) | Notes |
|---|---|---|
| Scale chip (inline) | (12, 12, 96, 28) | Label "SCALE" left, value right |
| RMS chip (inline) | (114, 12, 96, 28) | Label "RMS" left, value right |
| Reset statistics button | (216, 12, 68, 28) | Reset icon + "Reset" |
| Bars column | (12, 52, 80, SH − 64) | See below |
| Right column | (108, 52, 176, SH − 64) | Table, loudness, correlation |

**Bars column** (relative to its own origin, height BH = SH − 64):
- clip lamps: y 0 to 10;
- tracks: y 14 to BH − 16;
- channel letters: `caps`, centred, baseline BH − 2;
- scale labels: right-aligned at x = 24;
- bar L: x 30 to 52 (22 wide); bar R: x 58 to 80;
- mono: one bar at x 44 to 66, letter "MONO".

**Right column** (height HRC = SH − 64):
- Table: a header row of 16, then 9 rows of height `rowH = clamp (floor ((HRC − 214) / 9), 22, 28)`. Columns: label 80, L value 48, R value 48, with values right-aligned.
- Loudness block: 98 high.
- Correlation block: 68 high, anchored to the bottom (its bottom = SH − 12).
- `gap = max (12, (HRC − tableH − 98 − 68) / 2)`, where `tableH = 16 + 9 × rowH`. The loudness block starts at `52 + tableH + gap`. A 1 px `border` divider is centred in each gap.

| Size | Bars track (editor y) | rowH | Table (editor y) | Loudness (editor y) | Correlation (editor y) |
|---|---|---|---|---|---|
| 1200 × 700 | 110 to 672 (562 px) | 28 | 96 to 364 | 443 to 541 | 620 to 688 |
| 920 × 540 | 110 to 512 (402 px) | 24 | 96 to 328 | 345 to 443 | 460 to 528 |

### 4.5 Growth rules

- The header (44), the control bar (56) and the meter strip width (296) are fixed. All extra width and height goes to the plot. Extra height also lengthens the meter bars and, up to rowH = 28, the table rows. Beyond that it goes into the two meter-strip gaps.
- Grid density adapts (§5.3). The text never scales with the window.
- **Meter collapse:** the header Meters toggle sets MS = 0. The strip is removed (not painted, zero bounds) and the spectrum panel and control bar take the full width. The state is stored in `ViewState.metersCollapsed`, and the editor size does not change.

## 5. Spectrum view

### 5.1 Curve styles (colour-blind safe by line/fill style and lightness, not hue alone)

| Curve | When shown | Stroke | Fill | Draw order (1 = back) |
|---|---|---|---|---|
| Sidechain | `hasSidechain && showSidechain && isSidechainActive()` | 1 px #A58BFF at 70 % | Flat #A58BFF at 26 % to the bottom of the data rect | 1 |
| Primary A (single-mode curve, or L or M in dual modes) | always | 1.5 px #62D6F0, round joins | Vertical linear gradient from #62D6F0 at 30 % (data top) to 4 % (data bottom), closed to the data bottom | Fill 2, stroke 6 |
| Secondary A | `hasSecondary && secondary != Off` | 1 px #3E8FA3, opaque | none | 3 |
| Secondary B | the same, in dual modes (the frame carries one secondary curve per primary, roadmap SpectrumFrame) | 1 px #A87A3C, opaque | none | 4 |
| Primary B (R or S) | `isDual` | 1.5 px #FFB454 | **none** | 5 |

The four styles differ without relying on hue:
- Primary A is the only bright line with a gradient fill.
- Primary B is the only bright line with no fill.
- Secondaries are thin and dark (luminance 0.23 against 0.55 to 0.57 for the primaries).
- The sidechain is the only flat silhouette with a thin dim edge, drawn behind everything.

The legend and the hover card repeat each swatch next to its name.

- In dual modes the sidechain curve is the power mean of the sidechain channels (roadmap). In single modes it uses the same channel combination as the main curve.
- Points below the effective `dbLow` or above `dbHigh` are clamped to the data-rect edge. Frequencies outside the range are clipped by the data rect.
- **Curve geometry:** reduce each curve to at most 2 vertices per physical pixel column, keeping the column's max and min in dB. Build `juce::Path` objects that are reused across frames (`Path::clear` preserves storage).

### 5.2 Legend

- Position: top-left of the data rect at (data.x + 8, data.y + 8). It is 20 high, with fill `bg-plot` at 85 %, radius 4, padding 3 × 8, and 10 px between entries. Each entry is a 16 × 10 style swatch plus a `body-sm` name in `text-secondary`.
- Entries appear in this order, each only when the matching curve is drawn: primary A name; primary B name; the secondary name (with both A and B swatches stacked in dual modes); "Sidechain".
- Names by mode:
  - single modes: "Mid", "Side", "Left", "Right", "L+R";
  - L/R dual: "L", "R";
  - M/S dual: "M", "S".
- Secondary names: "RT max", "All-time max", "All-time avg".
- When the pointer is within 16 px of the legend box, the legend drops to 25 % opacity so it never hides the data under the cursor.

### 5.3 Grid and axes

The grid and axis labels are painted into a cached `juce::Image` (§2 HiDPI). The cache is rebuilt only on resize, scale-factor change, effective-range change, sample-rate change, or view change (§5.9). During a continuous pan or zoom the image is not rebuilt per frame; §12.1 says what is drawn instead.

**View mapping (T-034).** Every mapping in §5.1 to §5.6 (curves, grid, labels, hover, markers, curve clamping, right-click copy) runs over the **view window** (`viewFreqLow` … `viewFreqHigh`, `viewDbLow` … `viewDbHigh`, §5.9.1). At Fit the view window equals the effective home range below (`fLoEff` … `fHiEff`, `dbLoEff` … `dbHi`), so everything in this section is unchanged at Fit. For views narrower than the rules below can label, §5.9.8 extends the frequency label sets and the dB steps.

**Frequency axis** (log), from `fLoEff` to `fHiEff` (§5.4) at Fit, from `viewFreqLow` to `viewFreqHigh` when zoomed:
- **Lines:** every integer multiple m × 10^k (m = 1 to 9) inside the range. m = 1 lines use `grid-major`, and all others use `grid-minor`. Unlabelled minor lines are skipped when the gap between the 9 × 10^k and 10^(k+1) lines is under 4 px.
- **Labels:** choose the densest set whose smallest gap between adjacent label centres is ≥ 36 px:
  1. {1, 2, 3, 5, 7} × 10^k;
  2. {1, 2, 5} × 10^k, the standard 1-2-5 steps, roughly thirds of a decade;
  3. {1} × 10^k, decades only.
- **Label text:** integers below 1000 ("20", "50", "300"); thousands with "k" ("1k", "2k", "20k").
- **Label placement:** `axis` font in `text-tertiary`, centred on the line with its top at data.bottom + 6. A label is skipped if it would extend past x = 2 or x = plotWidth − 2.
- Resolved: the default size (840 px wide for 20 Hz to 20 kHz, 280 px per decade) uses set 1. The minimum size (560 px wide, 187 px per decade) uses set 2.

**dB axis** (linear), from `dbLoEff` to `dbHi`:
- **Step:** the smallest of {1, 2, 3, 6, 12, 24} dB whose pixel spacing is ≥ 20 px. Lines sit on multiples of the step. The 0 dB line uses `grid-major` and all others `grid-minor`.
- **Labels:** at every line, right-aligned at x = data.x − 8 and vertically centred, formatted as signed integers ("+6", "0", "−12", "−90").
- Resolved: the default range of −90 to +6 dB uses a 6 dB step at both the default size (564 px, 35 px per step) and the minimum size (404 px, 25 px per step). The widest range, −150 to +30 dB at the minimum height, uses a 12 dB step (27 px per step).

There are no axis titles. Units appear in the hover card and in the Level and Frequency chips.

### 5.4 Effective ranges (constraint display)

The editor and SpectrumMapper clamp identically, because both call the shared functions in **`source/dsp/DisplayRange.h`** (owned by dsp-engineer; roadmap Parameter table constraints). The editor must not re-implement these formulas; it calls the header. For reference, the formulas are:

- `fHiEff = min (freqHigh, sampleRate / 2)`
- `fLoEff = min (freqLow, fHiEff / 2)`, which guarantees at least 1 octave
- `dbHiEff = dbHigh`
- `dbLoEff = min (dbLow, dbHigh − 12)`, which guarantees at least 12 dB

The editor uses `DisplayRange.h` for the grid, axis labels, hover mapping, curve clamping and the clamped-value warnings below.

Parameters are never rewritten. When `x_eff ≠ x`, the chip half (and its Settings-panel field) shows the **set** value in `state-warn`. Its tooltip starts with the effective value and the reason:
- "Effective 24.0 kHz: limited by Nyquist (24.0 kHz)."
- "Effective 500 Hz: the range must span at least one octave."
- "Effective −42 dB: the range must span at least 12 dB."

### 5.5 Hover readout (an Investigate improvement; SPAN has none)

Active while the pointer is inside the data rect. The cursor is a crosshair there and the default arrow elsewhere.

- **Crosshair:** a vertical 1 px line in `text-primary` at 35 %, spanning the data rect at `round(x) + 0.5`. There is no horizontal line.
- **Markers:** a circle of radius 3.5 with a `bg-plot` fill and a 1.5 px stroke in the curve colour, at (x, y of the curve) for every curve listed in the card. Primary A is drawn last so it is on top.
- **Frequency under the pointer:** `f = viewFreqLow × (viewFreqHigh / viewFreqLow)^((x − data.x) / data.w)`. At Fit this is `fLoEff × (fHiEff / fLoEff)^((x − data.x) / data.w)`. The markers' y uses the view's dB range the same way.
- **During gestures** (pan, box zoom, pointer over the zoom panel) the crosshair, markers, card and hint line follow §5.9.5.
- **Level at f:** linear interpolation in dB between the two neighbouring display points, in log-frequency. If either neighbour is −inf, the level is −inf.
- **Card:**
  - Fill `card`, 1 px `border`, radius 6, padding 8 × 10, minimum width 132. The width fits the content.
  - Line 1: the frequency in `readout-hover` / `text-primary`, for example "1.25 kHz".
  - Line 2: the note in `readout` / `text-secondary`, for example "D#6 +12c".
  - A 1 px `border` separator with 6 px vertical margins.
  - Then one row per curve: a 16 × 10 swatch, the name in `body-sm` / `text-secondary`, and the value right-aligned in `readout` / `text-primary`, at least 12 px after the name.
  - Rows, single modes: primary; the secondary (if shown); Sidechain (if shown).
  - Rows, dual modes: primary A; primary B; Sidechain (if shown). The secondaries are omitted to keep the card compact.
- **Position:**
  1. Start at `(px + 18, py − 18 − cardH)`, above and to the right of the pointer.
  2. If the right edge would pass data.right − 4, place the card at `px − 18 − cardW` instead.
  3. If the top would pass data.top + 4, place it at `py + 18` instead.
  4. Finally clamp it inside the data rect with a 4 px inset.
  
  The card never sits under the pointer. It follows the pointer every frame without easing, and hides immediately when the pointer leaves the data rect.
  
  **While the zoom panel shows (T-034)** the card also avoids the panel. Try four candidates in this order and take the first that lies fully inside the data rect (4 px inset) and does not intersect the panel bounds inflated by 4 px (`zoomLayout.hoverCardExclusion`): (a) `(px + 18, py − 18 − cardH)`; (b) `(px − 18 − cardW, py − 18 − cardH)`; (c) `(px + 18, py + 18)`; (d) `(px − 18 − cardW, py + 18)`. If none qualifies, use steps 1 to 4 above and draw the card over the panel (the card is above the panel in the z-order, §5.9.7). Without the panel, candidates (a) to (d) give the same result as steps 1 to 4, so one implementation serves both.
- **Hint line:** while the pointer is in the data rect, no pan or box drag is active, and the pointer is not over the zoom panel. It uses `body-sm` at 10 px in `text-tertiary` on `bg-plot` at 85 %, radius 4, padding 2 × 6, anchored with its bottom-right at (data.right − 8, data.bottom − 6). Its text depends on the zoom state (T-034) and switches when the state changes:
  - At Fit: "Scroll: zoom · Ctrl+drag: box zoom · Click: reset max/avg · Right-click: copy frequency".
  - While zoomed: "Drag: pan · Double-click: fit · Scroll: zoom · Click: reset max/avg · Right-click: copy frequency".
  - **Fit rule:** the measured width (text plus 12 px padding) must be at most data.w − 16, which is 544 px at the minimum size 920 × 540. Estimated with Inter 500 at 10 px, the two texts are about 431 px and 474 px wide, so both fit at the minimum size. If the measured width is larger (for example with a fallback font), use the short forms "Scroll: zoom · Ctrl+drag: box · Click: reset · Right-click: copy" and "Drag: pan · Double-click: fit · Scroll: zoom · Click: reset · Right-click: copy".
  - Level zoom (Ctrl+scroll, or scroll over the dB labels) is not in the hint, because it does not fit at the minimum size. It is in the zoom panel's tooltip (§5.9.6), the accessible description (§10.3) and the M3b user checklist.
- **Formats:**
  - **Frequency:** round to 3 significant figures first, then:
    - below 100 Hz: 1 decimal, "43.7 Hz";
    - 100 to 999 Hz: 0 decimals, "437 Hz";
    - 1 to 9.99 kHz: 2 decimals, "4.37 kHz";
    - 10 kHz and above: 1 decimal, "14.3 kHz".
    
    The rounding happens first, so 999.6 Hz shows "1.00 kHz".
    
    **Zoom precision (T-034):** while the visible frequency span is below 1 octave (`viewFreqHigh / viewFreqLow < 2`), round to **4** significant figures instead, then: below 100 Hz 2 decimals ("43.72 Hz"); 100 to 999.9 Hz 1 decimal ("437.2 Hz"); 1 to 9.999 kHz 3 decimals ("4.372 kHz"); 10 kHz and above 2 decimals ("14.37 kHz"). Rounding happens first, so 999.96 Hz shows "1.000 kHz". At spans of 1 octave or more the 3-figure rule applies unchanged. Reason: at a 1/6-octave view one pixel is about 0.2 Hz at 1 kHz, and 3 figures (10 Hz steps there) would be coarser than the axis labels (§5.9.8).
  - **Note** (A4 = 440 Hz, sharps only, scientific octave numbering where C4 = 261.63 Hz):
    - `s = 12 × log2 (f / 440)`
    - `n = floor (s + 0.5)`
    - `cents = round ((s − n) × 100)`, rounding half away from zero
    - `midi = 69 + n`
    - Name = the pitch class of `midi` + `floor (midi / 12) − 1`, then a space, the sign ("+" for cents ≥ 0, including "+0c"; "−" otherwise), |cents| and "c".
    - Examples: 440 gives "A4 +0c"; 261.626 gives "C4 +0c"; 27.5 gives "A0 +0c"; 452.6 gives "A4 +49c" (s = 0.48880); 452.9 gives "A#4 −50c" (s = 0.50027, so n = 1 and cents = round (−49.97) = −50). The last two are the T-012 acceptance examples.
  - **Level:** 1 decimal with "dB" (not "dBFS", because the curve includes the slope tilt), for example "−23.4 dB". The rules:
    - The value is rounded to 1 decimal, half away from zero.
    - A value that rounds to zero shows as "0.0".
    - Positive values carry no "+" sign.
    - Non-finite values, or values ≤ −200, show as "−inf dB".
    - Values outside the display range still show their true value.

### 5.6 Spectrum interactions

This table replaces the T-010 table (T-034). The pointer actions are distinguished by button, modifier and movement, so none of them conflicts with hover. "G1" to "G21" refer to the exact rules in §5.9.3; the numbers are repeated here for convenience.

| Input | Action | Feedback |
|---|---|---|
| Hover / move in the data rect | Hover readout (§5.5) over the view mapping | Crosshair, markers, card, hint line |
| Wheel, vertical, in the data rect (G1, G2) | Zooms frequency around the pointer: one notch scales the visible span by 0.8 (in) or 1.25 (out); Shift gives 0.95 or 1 / 0.95. Limits: 1/6 octave and the home range. Always consumed. | Curves, grid and zoom panel update on the next editor frame |
| Ctrl+wheel in the data rect (G3) | Zooms level around the pointer, same factors, limits 6 dB and the home range. A touchpad pinch arrives as Ctrl+wheel and zooms level. | as above |
| Wheel over the frequency labels or the dB labels (G4, G5) | Zooms that axis around the pointer, no modifier needed | as above |
| Horizontal wheel or sideways two-finger swipe (G6) | Pans frequency by 10 % of the visible span per notch, only while frequency is zoomed | as above |
| Left drag (> 4 px) or middle drag (G7 to G9) | Pans both axes while zoomed; does nothing at Fit | Grabbing-hand cursor; hover readout hidden while dragging |
| Left click, ≤ 4 px of movement, no Ctrl (G11) | After a **250 ms defer**, `requestSpectrumReset()`, which clears the all-time max and all-time average. A double-click inside the defer cancels it. | Only when `secondary` is All-time max or All-time avg: a toast "Max/avg curves reset" centred at the top of the data rect (y = data.y + 8, or data.y + 36 while the FROZEN badge shows), for 900 ms, shifted left clear of the zoom panel (§5.9.7). With Off or RT max nothing visible changes, so no toast appears. |
| Double-click (G12) | Fit: the view returns to the home range. No spectrum reset. | Zoom panel disappears |
| Ctrl+left drag (G13 to G15) | Box zoom: on release both axes zoom to the box. A box under 4 px in either dimension cancels; so does Esc when the editor has keyboard focus. | Box-zoom cursor and rubber band |
| Ctrl+left click, middle click (G10, G14) | Nothing | |
| Right button down (G16) | Copy the frequency under the pointer to the system clipboard (`juce::SystemClipboard::copyTextToClipboard`). Ignored during a pan or box drag. | Toast "Copied 1250 Hz" at (px + 12, py − 28), clamped inside the data rect and clear of the zoom panel (§5.9.7), for 1200 ms |
| Zoom panel: Fit button, overview strip (G17 to G21) | Fit; pan by dragging the window; press beside the window to centre the view there; the wheel passes through to the data rect | §5.9.6 |

- **Clipboard text:** the hover frequency in Hz, as a plain number with no unit, so it pastes into any EQ field. It always matches the card value:
  - 3 significant figures (spans of 1 octave or more): 1 decimal below 100 Hz ("43.7") and none above ("1250", "14300");
  - 4 significant figures (spans below 1 octave, §5.5 "Zoom precision"): 2 decimals below 100 Hz ("43.72"), 1 decimal from 100 to 999.9 Hz ("437.2"), none above ("1253", "14370").
- No context menu appears anywhere in the editor.
- **Toast style:** `surface-3` fill, 1 px `accent` border, radius 4, padding 4 × 8, `body-sm` in `text-primary`. It fades to 0 over its last 200 ms, and repaints only its own bounds.

### 5.7 Freeze

- The Freeze toggle in the header is a view flag, not a parameter, and is not persisted.
- **While frozen:**
  - Live updates stop. Since T-034 the editor calls `setSpectrumFrozen (true)` on every freeze change (including auto-unfreeze) and keeps reading frames, but accepts only frames that carry the current freeze epoch. While frozen the engine publishes such frames only as re-maps of its bin-domain freeze snapshot, after a view or home-range change. Until a re-mapped frame arrives, the editor draws the frame it has through the new mapping. The hover readout, the legend and the interactions keep working.
  - Range changes redraw the frozen data in the new range. Slope, smoothing, the secondary mode and the other analysis settings do not alter it until unfreeze: the snapshot keeps the settings of the freeze moment.
- **Freeze plus zoom (T-034, plan "Freeze semantics"):**
  - Zoom and pan keep working while frozen and show **full detail**: each view change re-maps the frozen bin-domain snapshot onto the new view, so zooming into a frozen spectrum reveals real structure rather than a stretched picture.
  - Freeze and unfreeze never change the view. On unfreeze the engine publishes the live state at once, mapped onto the current view.
  - The bin-spacing hint (§5.9.6) uses the bin spacing of the frozen data, not the current Block setting.
  - The overview strip shows the frozen frame's overview, so it stays still with the curves.
  - A home-range change while frozen follows the view rule of §5.9.1 and redraws the frozen data.
  - The editor drops any frame whose freeze epoch is not the current one, so a live frame published just before the freeze took effect never appears.
- **Visuals while frozen:**
  - the button uses the `state-frozen` text, a border at 60 % and the `state-frozen-tint` fill;
  - a "FROZEN" badge (20 high, `state-frozen` fill, `caps` with +0.08 em tracking in `text-on-accent`, radius 4, padding 0 × 8) is centred horizontally at y = data.y + 8;
  - the data-rect frame becomes 1 px `state-frozen`.
- **Auto-unfreeze:** a `channelMode` change from any source (UI, host automation or state load) turns freeze off before the next frame.
- **Secondary-curve reset is DSP-side:** `SpectrumAnalyser` clears its secondary accumulators whenever `channelMode`, `fftSize`, `window` or the sample rate changes (roadmap Decision 2026-09-28). The editor does **not** call `requestSpectrumReset()` on any of these changes and shows no toast for them. The first frame after the change already carries the cleared curves. The mockup's `resetCurves(false)` call on a mode change only simulates this DSP behaviour. It is not an editor call to port.
- Left-click-to-reset still works while frozen, after the same 250 ms defer (§5.9.3 G11). The engine resets, and the effect becomes visible after unfreezing.

### 5.8 No frames

- **Before the first frame:** only the grid is drawn.
- **Audio stopped or silent:** frames that keep arriving fall according to `avgTime`. If no new frame arrives (the host stopped calling `processBlock`), the last frame stays on screen. This is intended, because the spectrum is a snapshot tool.
- **Sidechain active but silent:** the silhouette falls to the floor.
- **Sidechain bus not active:** no silhouette and no legend entry. The SC button shows its "no input" state (§9).

### 5.9 Zoom and pan (T-034, milestone M3b)

Source: `docs/plans/spectrum-zoom-pan.md` (View model, Interaction table, Freeze semantics). Every deviation from its defaults is listed in §5.9.10. Token values are in `tokens.json` groups `zoom`, `zoomLayout`, `zoomInteraction`, `deepGrid` and `cursor`.

#### 5.9.1 View model (user terms)

- **Home range:** what the Frequency and Level chips set, as effective values through `source/dsp/DisplayRange.h` (§5.4). It is saved, automatable, and nothing in this section changes it. **Fit** means "the view equals the home range".
- **View window:** the part of the home range that the data rect shows: `viewFreqLow`, `viewFreqHigh`, `viewDbLow`, `viewDbHigh`.
  - It always lies inside the home range. There is no overscroll.
  - Its smallest span is **1/6 octave** (`minViewSpanOctaves`) and **6 dB** (`minViewSpanDb`); its largest span is the home range.
  - It lives in the editor only. It is never written to a parameter, so it creates no automation, no undo step and no "project modified" flag. It is not saved: the editor always opens at Fit, and `setAnalysisActive (false)` also returns it to Fit.
- **Zoomed:** the view differs from the home range on at least one axis. Frequency is zoomed when `|ln (viewFreqLow / fLoEff)| > 1e−6` or `|ln (viewFreqHigh / fHiEff)| > 1e−6`; level is zoomed when `|viewDbLow − dbLoEff| > 1e−4` or `|viewDbHigh − dbHiEff| > 1e−4`. Each axis is zoomed or at Fit on its own.
- **Where each axis goes:** a frequency view change goes to the analysis thread (`setSpectrumView (fLo, fHi)`), which spreads its display points over the visible range, so zooming shows real detail. Level zoom stays in the editor.
- **Home-range change** (a chip edit, automation, a state load, or a sample-rate change), decided per axis:
  1. An axis that was at Fit (compared with the old home range) follows the new home range, so it stays at Fit.
  2. A zoomed axis keeps its span and its low edge where possible: it is shifted inside the new home range. If its span is larger than the new home span, that axis returns to Fit.
  
  The other axis is not touched.
- **Other events:** a resize, a meter collapse, a `channelMode` change and freeze or unfreeze keep the view (in frequency and dB) unchanged.

#### 5.9.2 View arithmetic (`source/gui/ZoomMath.h`, clamps from `DisplayRange.h`)

Frequency is handled in natural-log units (`u = ln f`), level in dB. The data rect is (x0, y0, w, h). `L = ln (viewFreqHigh / viewFreqLow)`, `S = viewDbHigh − viewDbLow`, and `Lhome`, `Shome` are the home spans. `Lmin = ln 2 / 6`, `Smin = 6`.

- **Pointer fractions:** `t = clamp ((px − x0) / w, 0, 1)` and `s = clamp ((py − y0) / h, 0, 1)` (0 at the top).
- **Anchored frequency zoom by factor k:** `uA = ln f(px)`; `L' = clamp (L × k, Lmin, Lhome)`; `uLo' = uA − t × L'`; then `clampViewFrequency` shifts `[uLo', uLo' + L']` inside the home range. If `L' ≥ Lhome − 1e−6`, the axis is set exactly to the home range.
- **Anchored level zoom by factor k:** `dA = dB(py)`; `S' = clamp (S × k, Smin, Shome)`; `viewDbHigh' = dA + s × S'`; `viewDbLow' = viewDbHigh' − S'`; then `clampViewDb`. If `S' ≥ Shome − 1e−4`, the axis is set exactly to the home range.
- The span is clamped before the anchor is applied, so the value under the pointer stays where it is on every step, including a step that hits the span limit. Only the home-edge clamp can move it.
- **Wheel notches:** `n = wheel.deltaY / 0.234375`, negated when `wheel.isReversed` (as `juce::Slider` does), clamped to ±3 per event. 0.234375 is the delta that JUCE reports on Windows for one 120-unit notch (60 / 256). Positive n (wheel turned away from the user) zooms in. `k = 0.8^n`; with Shift `k = 0.95^n`. So one notch scales the span by 0.8 in and by 1.25 out; touchpads and high-resolution wheels send fractions of a notch and zoom in proportion.
- **Horizontal pan:** `uLo += nX × 0.10 × L`, with `nX = −wheel.deltaX / 0.234375` (JUCE reports scrolling right as a negative deltaX) and the same ±3 clamp. One notch moves the view by 10 % of its span; scrolling right shows higher frequencies.
- **Drag pan by (dx, dy) pixels since the press**, computed from the view at the press (`v0`): `uLo = ln v0.fLo − dx / w × L0` and `viewDbHigh = v0.dbHigh + dy / h × S0`, spans unchanged, then clamped. The point grabbed at the press stays under the pointer until a home edge is reached; there the content stops and the pointer slides over it.
- **Box to view:** `fLo = f (min x)`, `fHi = f (max x)`, `dbHigh = dB (min y)`, `dbLow = dB (max y)`, with the box corners clamped to the data rect. An axis narrower than its minimum span is widened to the minimum around its centre (the geometric centre `√(fLo × fHi)` for frequency, the arithmetic centre for dB). Both axes are then clamped into the home range by shifting.
- **Overview to view:** the strip maps the home range (log frequency on x, dB on y with `dbHi` at the top). A press at strip point q has home coordinates `(uq, dq)`. Centring on it gives `uLo = uq − L / 2` and `viewDbHigh = dq + S / 2`, spans unchanged, then clamped.

#### 5.9.3 Gestures

"Data rect" means the plotted area of §4.1. The **frequency gutter** is (data.x, data.bottom, data.w, 24), under the data rect; the **dB gutter** is (plot.x, data.y, 48, data.h), left of it. The bottom-left corner (48 × 24) is neither.

| # | Input | Where | Action | Exact rule |
|---|---|---|---|---|
| G1 | Wheel, vertical | Data rect, and the zoom panel (which passes the wheel through to the data rect at the same position) | Zooms frequency, anchored at the pointer's x | `k = 0.8^n`; limits 1/6 octave and the home range. Always consumed, also when nothing changes at a limit. |
| G2 | Shift+wheel | Data rect | Fine frequency zoom | `k = 0.95^n`. If the platform reports Shift+wheel as horizontal (deltaY = 0, deltaX ≠ 0), deltaX is used as the vertical amount. Shift+wheel never pans. |
| G3 | Ctrl+wheel, Ctrl+Shift+wheel | Data rect | Zooms level, anchored at the pointer's y; Shift for fine steps | Same factors; limits 6 dB and the home range. A Windows precision-touchpad pinch arrives as Ctrl+wheel and therefore zooms level (accepted v1 limitation, plan risk Z4). |
| G4 | Wheel, any modifier | Frequency gutter | Zooms frequency, anchored at the pointer's x | Shift for fine steps; Ctrl is ignored, because the gutter chooses the axis |
| G5 | Wheel, any modifier | dB gutter | Zooms level, anchored at the pointer's y, with no modifier needed | Shift for fine steps |
| G6 | Horizontal wheel, or a sideways two-finger swipe | Data rect, frequency gutter | Pans frequency | 10 % of the visible span per notch. Consumed only while frequency is zoomed; at frequency Fit it is passed to the parent. An event whose `|deltaX|` exceeds `|deltaY|` pans; otherwise it zooms (G1), so a diagonal swipe never does both. |
| G7 | Left press, then more than 4 px of movement (Euclidean distance from the press point) | Data rect, while zoomed on either axis | Pans both axes like a hand tool | 1:1 (§5.9.2). The pan includes the first 4 px, so the content does not jump at the threshold. An axis at Fit cannot move. Wheel events during the pan are applied, and the pan continues from the new view. |
| G8 | Left drag at Fit (both axes) | Data rect | Nothing: no pan and no reset | The crosshair cursor stays and the hover readout continues (the pre-T-034 behaviour that protects against accidental resets) |
| G9 | Middle press and drag (> 4 px) | Data rect | The same as G7, or G8 at Fit | |
| G10 | Middle click (≤ 4 px) | Data rect | Nothing | |
| G11 | Left click: press and release within 4 px, without Ctrl | Data rect | After **250 ms**, `requestSpectrumReset()` and the reset toast (§5.6) | Started on the release. Cancelled by a double-click (G12) that arrives before it fires. A release that ends a pan, a box or a no-op drag never starts it. |
| G12 | Double-click: JUCE `mouseDoubleClick` (the second press within the OS double-click time and distance) | Data rect | **Fit** both axes, at the second press | Cancels a pending G11 reset. The release that completes the double-click starts no reset. If the second click comes after the 250 ms defer, the first click's reset has already run; this is the same edge case as the chips (§7.1). |
| G13 | Ctrl+left press, then drag | Data rect | **Box zoom:** a rubber band from the press point to the pointer, clamped to the data rect. On release both axes zoom to it (§5.9.2). | The mode is fixed at the press: releasing Ctrl during the drag keeps the box. A box narrower or lower than **4 px** at the release cancels (no zoom, no reset). An axis below its minimum span is widened, and the widened result is previewed (§5.9.6, box style). |
| G14 | Ctrl+left click (no movement) | Data rect | Nothing | So a cancelled box never resets curves |
| G15 | Esc during a box drag | Editor with keyboard focus | Cancels the box | SpectrumView never takes keyboard focus (§7.2), so Esc works only when the host has given the editor focus and a control in it holds the focus. Otherwise, dragging back to a box under 4 px cancels. |
| G16 | Right press | Data rect | Copies the frequency under the pointer (view mapping, §5.6) | Ignored while a pan or a box drag is active |
| G17 | Fit button, click | Zoom panel | **Fit** both axes, on release inside the button | |
| G18 | Press on the view window, then drag | Overview strip | Pans both axes | The pointer's offset from the window centre is kept; 1:1 in strip coordinates |
| G19 | Press outside the view window | Overview strip | The view centres on that point at once (both axes, spans kept, clamped); continuing to drag pans as in G18 | The press acts on mouse-down, so there is no click or double-click ambiguity in the strip |
| G20 | Double-click, right-click, middle press | Zoom panel | Nothing (a double-click on the strip is two G19 presses) | |
| G21 | Wheel | Zoom panel | Passes through to the data rect at the same position (G1 to G3) | The panel component does not override `mouseWheelMove`, so JUCE forwards the event to SpectrumView |

**Coalescing:** the input handlers only record the requested view. The editor applies it once per editor update (§12), so a burst of wheel events or drag moves produces at most one repaint and one `setSpectrumView` per update.

#### 5.9.4 Cursors

| State | Cursor | Source |
|---|---|---|
| Data rect, no button down, Ctrl not held (zoomed or not) | Crosshair | `juce::MouseCursor::CrosshairCursor` |
| Data rect, Ctrl held, no button down | Box zoom | `assets/ui/cursor-zoom-box.svg`, hotspot (7.5, 7.5) |
| Box drag (G13) | Box zoom | as above |
| Press, before the 4 px threshold | Unchanged | |
| Pan (G7, G9) | Grabbing hand | `assets/ui/cursor-hand-grab.svg`, hotspot (12, 12) |
| Drag at Fit (G8) | Crosshair | |
| Frequency and dB gutters | Normal arrow | |
| Zoom panel background and text rows | Normal arrow | |
| Fit button | The cursor the header buttons use (T-011) | |
| Overview strip, over the window's hit area | Open hand | `assets/ui/cursor-hand-open.svg`, hotspot (12, 12) |
| Overview strip, outside the window | `juce::MouseCursor::PointingHandCursor` | |
| Overview drag (G18, G19) | Grabbing hand | |

- **Ctrl state:** take it from the modifiers of the latest mouse event, and from `modifierKeysChanged` when the host delivers it. Some hosts do not deliver key changes to an unfocused plugin window; then the cursor updates on the next mouse move.
- **Custom cursors:** render each SVG once per display scale into an ARGB `juce::Image` of 24 × scale pixels, and create `juce::MouseCursor (juce::ScaledImage (image, scale), hotspot)`. T-037 checks that the hotspot lands on the logical point given above at 100 % and 200 %. The cursors are fixed black and white and are never recoloured (§11).

#### 5.9.5 Hover readout, crosshair, markers and hint during gestures

| State | Crosshair | Markers | Hover card | Legend fade (§5.2) | Hint line |
|---|---|---|---|---|---|
| Hover, no button | shown | shown | shown | normal rule | shown |
| Press, before the 4 px threshold | shown | shown | shown | normal rule | shown |
| Drag at Fit (G8) | shown | shown | shown | normal rule | shown |
| Pan (G7, G9) | hidden | hidden | hidden | 100 % | hidden |
| Box drag (G13) | hidden | hidden | hidden | 100 % | hidden |
| Wheel zoom (G1 to G6) | shown, recomputed in the same frame | shown | shown, values recomputed (the anchored frequency stays fixed) | normal rule | shown (text follows the zoom state) |
| Pointer over the zoom panel | hidden | hidden | hidden | 100 % | hidden |
| Overview drag (G18, G19) | hidden | hidden | hidden | 100 % | hidden |

- After a pan or a box ends with the pointer inside the data rect, the readout reappears at the pointer on the next frame, with no delay.
- Why hidden: during a pan every value in the card changes under a moving spectrum, the card covers the data the user is dragging, and hiding it reduces the paint cost (§12).
- The readout uses the view mapping, so the frequency, note and levels are correct at every zoom. Levels outside the visible dB range still show their true values (§5.5).

#### 5.9.6 Zoom panel: indicator and overview strip

The indicator and the overview strip form one **zoom panel**: a child component of SpectrumView at the top-right corner of the data rect. It shows only while the view is zoomed, and it appears and disappears instantly (no animation, §3.3).

**Geometry** (`zoomLayout`):
- Width `Wz = clamp (round (data.w / 4), 200, 280)`.
- Position x = data.right − 8 − Wz, y = data.y + 8.
- Height 92, or 108 while the bin-spacing row shows (the panel grows downwards and the strip moves with it).
- Fill `zoom.panel-fill` (opaque, so text contrast does not depend on the curves beneath), 1 px `zoom.panel-border` inside the bounds, radius 6.

Panel-relative layout:

| Item | Rect (panel-relative) | Content |
|---|---|---|
| Row 1, frequency range | (8, 6, Wz − 60, 16) | Text, left-aligned, vertically centred |
| Fit button | (Wz − 44, 2, 40, 24) | "Fit" |
| Row 2, level range | (8, 26, Wz − 16, 16) | Text |
| Row 3, bin spacing (only when fewer than 16 bins are in view) | (8, 42, Wz − 16, 16) | Text |
| Overview strip | (6, 46, Wz − 12, 40), or (6, 62, Wz − 12, 40) with row 3 | Mini-map |

Resolved in editor coordinates:

| Size | Data rect | Wz | Panel | Fit button | Row 1 | Row 2 | Row 3 | Strip |
|---|---|---|---|---|---|---|---|---|
| 1200 × 700 (default) | (48, 56, 840, 564) | 210 | (670, 64, 210, 92 or 108) | (836, 66, 40, 24) | (678, 70, 150, 16) | (678, 90, 194, 16) | (678, 106, 194, 16) | (676, 110, 198, 40), or (676, 126, 198, 40) |
| 920 × 540 (minimum) | (48, 56, 560, 404) | 200 | (400, 64, 200, 92 or 108) | (556, 66, 40, 24) | (408, 70, 140, 16) | (408, 90, 184, 16) | (408, 106, 184, 16) | (406, 110, 188, 40), or (406, 126, 188, 40) |
| 1200 × 700, meters collapsed | (48, 56, 1136, 564) | 280 | (896, 64, 280, 92 or 108) | (1132, 66, 40, 24) | (904, 70, 220, 16) | (904, 90, 264, 16) | (904, 106, 264, 16) | (902, 110, 268, 40), or (902, 126, 268, 40) |
| 2560 × 1600 (maximum) | (48, 56, 2200, 1464) | 280 | (1960, 64, 280, 92 or 108) | (2196, 66, 40, 24) | (1968, 70, 220, 16) | (1968, 90, 264, 16) | (1968, 106, 264, 16) | (1966, 110, 268, 40), or (1966, 126, 268, 40) |

**Indicator text** (font `axis`: JetBrains Mono 400, 11 px, drawn in a 16 px row):
- **Row 1, frequency:** each end in the 3-figure format of §5.5. When both ends have the same unit, the unit appears once: "1.00 … 1.12 kHz", "500 … 900 Hz"; otherwise both carry their unit: "437 Hz … 1.25 kHz". The separator is " … " (U+2026 with a space on each side), as in the Level chip. The longest possible text, for example "43.7 Hz … 1.25 kHz", is 18 characters = 119 px, which fits the 140 px row at the minimum size.
- **Row 2, level:** each end with 1 decimal, U+2212 for negative values, "+" for positive values and "0.0" for zero, then " dB": "−30.0 … −24.0 dB". The longest, "−150.0 … −144.0 dB", is 18 characters = 119 px.
- **Colour:** `zoom.range-text-zoomed` (text-primary) for an axis that is zoomed, `zoom.range-text-full` (text-secondary) for an axis at Fit. The brighter row tells the user which axis is zoomed.
- **Row 3, bin spacing:** shown when `binsInView = (viewFreqHigh − viewFreqLow) / binHz < 16`, where `binHz` is the bin spacing of the displayed data (sample rate / block size; the freeze-time value while frozen; §15 question 6). Text "Bin spacing " plus `binHz` in the 3-figure format of §5.5: "Bin spacing 11.7 Hz" (48 kHz, 4096), "Bin spacing 93.8 Hz" (48 kHz, 512), "Bin spacing 0.7 Hz" (48 kHz, 65536). Colour `zoom.bin-hint-text` (state-warn). At most 19 characters = 125 px.
- **Text cost:** the strings change only when the view changes. They are drawn from the label-image cache (§12).

**Fit button** (40 × 24, radius 4, label "Fit" in `body`, Inter 500 12 px, centred):

| State | Fill | Border | Label |
|---|---|---|---|
| Normal | none (panel fill shows) | 1 px `zoom.panel-border` | `zoom.range-text-full` |
| Hover | `zoom.fit-hover-fill` | 1 px `zoom.panel-border` | `zoom.range-text-zoomed` |
| Pressed | `zoom.fit-hover-fill` | 1 px `zoom.fit-pressed` | `zoom.fit-pressed` |

It acts on release inside the button, like the other action buttons (§7.1). It has no icon.

**Overview strip:**
- **Frame and background:** a 1 px `zoom.panel-border` frame drawn inside the strip rect. The inner rect I (the strip inset by 1 px: 196 × 38 at the default size, 186 × 38 at the minimum) is filled with `zoom.overview-bg`.
- **Axes:** x is log frequency over the whole home range (`fLoEff` to `fHiEff`); y is dB over the home range (`dbHi` at the top). There is no grid and there are no labels.
- **Curve:** primary A only (in dual modes, L or M), from the frame's 256-point overview (`overviewFreqHz`, `overviewDb`, plan "Data flow"). For each physical pixel column of I, take the highest dB of the points that fall in the column. If none falls in it, interpolate linearly in log frequency between the neighbours. Clamp y to I; a non-finite value goes to the bottom.
  - **Body:** one rect per column, from y down to the bottom of I, in `zoom.overview-fill`.
  - **Top line:** one rect per column, from `min (y, yPrevious)` to `max (y, yPrevious) + 1 physical px`, in `zoom.overview-line`. This gives a continuous 1 px line without paths.
- **View window:** the view mapped into I: x1 = x (viewFreqLow), x2 = x (viewFreqHigh), y1 = y (viewDbHigh), y2 = y (viewDbLow).
  - Each axis is widened to at least **6 px** around its centre (`windowMinPx`) and shifted to stay inside I. At the 1/6-octave minimum the window would otherwise be about 3 px wide.
  - **Outside the window:** `zoom.overview-shade` (`#0A0D13` at 60 %), up to 4 rects.
  - **Outline:** 1 px `zoom.window-stroke` along the inside of the window rect. When an axis is at Fit, that axis's outline runs along the edges of I.
  - **Hit area:** the window rect widened to at least **16 × 16 px** around its centre (`windowHitMinPx`). It may extend past I.

Strip states:

| State | Window fill | Other | Cursor |
|---|---|---|---|
| Normal | none | | arrow outside the strip |
| Pointer over the window's hit area | `zoom.window-fill-hover` (accent at 12 %) | | open hand |
| Pointer in the strip, outside the hit area | none | 1 px `zoom.window-ghost` outline of the window that a press there would produce (centred on the pointer, clamped) | pointing hand |
| Dragging (G18, G19) | `zoom.window-fill-drag` (accent at 20 %) | no ghost | grabbing hand |
| Frozen | as above | The curve comes from the frozen frames, so it stands still | |
| No frame yet | as above | Background, frame, shade and window only | |

**Box-zoom rubber band** (G13), drawn over the curves and the zoom panel (z-order in §5.9.7):
- Fill `zoom.box-fill` (`#E8ECF3` at 8 %), one rect.
- Outline 1 px `zoom.box-stroke`, 4 rects, on physical pixel edges.
- If either axis of the current box is below its minimum span, the widened and clamped result (§5.9.2) is also outlined in 1 px `zoom.box-target`, clipped to the data rect. This shows the user what the release will produce.
- The box outline is neutral white, so it can never be mistaken for a curve.

**Tooltips** (700 ms delay, standard tooltip style):
- Fit button: "Show the full range set by the Frequency and Level chips. Double-clicking the spectrum does the same."
- Rows 1 and 2: "Visible range. Scroll to zoom frequency; Ctrl+scroll, or scroll over the dB labels, to zoom level. Drag the spectrum to pan."
- Row 3: "Fewer than 16 FFT bins are in view, so the curve is drawn straight between bins. Raise Block for finer detail."
- Overview strip: "Overview of the full range. Drag the window to pan, or click beside it to jump there."

#### 5.9.7 Collision rules

**Z-order inside the plot region**, back to front:
1. the grid (cached image or rect lists, §12);
2. the curves;
3. the crosshair;
4. the markers;
5. the legend;
6. the FROZEN badge;
7. the zoom panel;
8. the box-zoom rubber band;
9. the hover card;
10. the hint line;
11. the toasts.

Items 8 to 11 are painted in `paintOverChildren` (or by components above the panel), so they are never hidden by the panel. The frozen data-rect frame stays as it is today.

**Positions at the two reference sizes** (editor coordinates; the legend, badge and toast widths are estimates for Inter at the token sizes):

| Element | 1200 × 700 | 920 × 540 | Rule |
|---|---|---|---|
| Legend | x 56 to at most 317 (the widest case: dual mode, "All-time max" and "Sidechain", about 261 px), y 64 to 84 | the same | Top-left, unchanged. It never reaches the panel: 353 px clearance at the default size, 83 px at the minimum. |
| FROZEN badge | about 60 px wide, centred at x 468: 438 to 498, y 64 to 84 | centred at x 328: 298 to 358 | Unchanged. Clearance to the panel: 172 px and 42 px. |
| Reset toast | centred at x 468, y 64 (y 92 while frozen), about 134 px wide: 401 to 535 | centred at x 328: 261 to 395 | While the panel shows, if the toast's right edge would pass panel.x − 8, shift it left so it ends at panel.x − 8 (`toastClearance`). At the minimum size this moves it 3 px, to 258 to 392. |
| Zoom panel | x 670 to 880, y 64 to 156 (172 with row 3) | x 400 to 600, y 64 to 156 (172) | Top-right, §5.9.6 |
| Hint line | right edge 880, y 596 to 614, at most 474 px wide, so x ≥ 406 | right edge 600, y 436 to 454, so x ≥ 126 | Bottom-right, unchanged. Vertical clearance to the panel: 424 px and 264 px. |
| Copy toast | at (px + 12, py − 28), clamped inside the data rect | the same | If it would intersect the panel bounds inflated by 4 px, place it at px − 12 − its width instead (still clamped). It is always drawn above the panel. |
| Hover card | follows the pointer | the same | Avoids the panel with the candidate rule of §5.5; it is above the panel when no candidate fits |

Consequences:
- The panel never covers the legend, the FROZEN badge or a toast: the first two are at different positions at every size from 920 × 540 up (the panel's left edge only moves right as the data rect grows), and the toasts are drawn above it.
- The legend's 25 % fade rule (§5.2) is unchanged. The panel does not fade, because it is an interactive control.
- The pointer over the panel shows no crosshair, card or hint (§5.9.5).

#### 5.9.8 Deep-zoom grid rules

These rules extend §5.3. At Fit, and at every view the §5.3 rules already label well, they give exactly the §5.3 result.

**Frequency labels.** Inputs: the view `[fL, fH]`, the data width w, the plot-region width PW, and the label advance a = 6.6 px per character (JetBrains Mono 11 px is 0.6 em per character, and every label character is one monospaced cell).
- **Gap rule:** a candidate label set qualifies when
  - the smallest distance between adjacent label centres, over all its values inside the view (before edge skipping), is at least **G = max (36, 6.6 × the longest label's character count + 16)**; and
  - at least 2 of its labels survive the §5.3 edge rule (a label is skipped if it would extend past x = 2 or x = PW − 2).
  
  G is 36 px for the 3-character labels of §5.3, so wide views are unchanged; longer labels such as "1.25k" (5 characters) need 49 px, which keeps at least 16 px of space between labels.
- **Candidates:**
  1. **Decade sets** (§5.3): {1, 2, 3, 5, 7} × 10^k, then {1, 2, 5} × 10^k, then {1} × 10^k. Take the densest one that qualifies. Its labels use the §5.3 format ("20", "300", "1k", "20k").
  2. **Linear step:** the smallest step s in {1, 2, 5} × 10^n Hz (n any integer) that qualifies. Its labels are all multiples of s inside the view. Search upwards from n = floor (log10 (fH − fL)) − 3, and skip any step with more than 400 multiples in view. On a log axis the smallest gap is always between the two highest multiples, so only that gap needs checking.
  
  **Choose** the candidate with more surviving labels; on a tie, the decade set. At 20 Hz to 20 kHz the decade set has 16 labels against the linear step's 2, so wide views keep the §5.3 look, and narrow views get linear steps.
- **Fallback** (a guard; it is not reached at any allowed span and size): the smallest linear step whose top gap is at least 36 px, ignoring label width.
- **Linear label format:** values are computed as k × s and rounded to 6 decimals, so a 0.2 Hz step never prints "20.200000001".
  - Below 1000 Hz: plain Hz, with d = max (0, −floor (log10 s)) decimals. s = 1 or 2 gives "102", "104"; s = 0.2 or 0.5 gives "20.2", "20.5"; s = 0.05 gives "10.25".
  - From 1000 Hz up: kHz with "k", with d = max (0, −floor (log10 (s / 1000))) decimals and trailing zeros kept, so one set has one label width. s = 100 gives "1.0k", "1.1k", "1.2k"; s = 10, 20 or 50 gives "1.00k", "1.25k", "3.05k"; s ≥ 1000 gives "11k", "12k".
- **Lines:**
  - Decade mode: unchanged (§5.3).
  - Linear mode: a line at every multiple of s in view (`grid-major` when the value is a power of ten, otherwise `grid-minor`), plus an unlabelled `grid-minor` line at every odd multiple of s / 2 when the gap between the two highest half-step lines is at least 16 px (`deepGrid.halfStepMinPx`).

**dB labels.**
- **Step:** the smallest of **{0.1, 0.2, 0.5, 1, 2, 3, 6, 12, 24} dB** whose pixel spacing is at least 20 px. This is the §5.3 rule with three sub-dB steps added below 1 dB. Lines sit on multiples of the step (k × step, rounded to 6 decimals); the 0 dB line uses `grid-major`.
- **Format:** for steps of 1 dB or more, signed integers as in §5.3 ("+6", "0", "−12"). For steps below 1 dB, signed with 1 decimal: "−12.5", "0.0", "+0.5". The longest label, "−149.5", is 6 characters = 39.6 px; right-aligned at data.x − 8 = 40, it starts at x = 0.4, inside the 48 px gutter.
- Labels whose centre would be within 6 px of the plot region's top or bottom edge are skipped, as today.

**Guarantees and how they are verified:**
- **dB:** consecutive steps differ by at most 2.5 ×, so the chosen spacing is below 50 px, unless even 24 dB is under 20 px, which cannot happen: the widest home span is 180 dB and the smallest data height is 404 px, so 12 dB is at least 26.9 px. With h ≥ 404 px there are therefore at least 8 dB lines, all at least 20 px apart.
- **Frequency:** the worked table below shows 7 to 16 labels at the extremes (the minimum span at the minimum width, and Fit). T-037 adds a `ZoomMath` unit test that sweeps the view span from 1/6 octave to the widest home range (10 Hz to 40 kHz), with centres from 10 Hz to 24 kHz, at w = 560 and w = 840. It asserts at least 2 labels and adjacent label centres at least 36 px apart for every case.

**Worked examples** (plot region widths PW = 904 at 1200 × 700 and PW = 624 at 920 × 540; "gap" is the smallest distance between adjacent label centres):

| # | Size | View | Frequency labels | Smallest gap / G | dB step, labels | dB spacing |
|---|---|---|---|---|---|---|
| A | 1200 × 700 | Fit: 20 Hz to 20 kHz, −90 to +6 dB | Decade {1, 2, 3, 5, 7}: 20, 30, 50, 70, 100, 200, 300, 500, 700, 1k, 2k, 3k, 5k, 7k, 10k, 20k (16). The linear candidate (s = 10 kHz) has 2. | 40.9 px (50 to 70) / 36 | 6 dB: −90 … +6 (17) | 35.3 px |
| B | 920 × 540 | Fit: 20 Hz to 20 kHz, −90 to +6 dB | Decade {1, 2, 5}: 20, 50, 100, 200, 500, 1k, 2k, 5k, 10k, 20k (10). {1, 2, 3, 5, 7} fails (27.3 px). | 56.2 px / 36 | 6 dB: −90 … +6 (17) | 25.3 px |
| C | 1200 × 700 | 500 Hz to 2 kHz, −48 to −12 dB | Linear s = 200 Hz: 600, 800, 1.0k, 1.2k, 1.4k, 1.6k, 1.8k, 2.0k (8), against 4 for the decade set (500, 700, 1k, 2k). s = 100 Hz fails (31.1 px). | 63.8 px (1.8k to 2.0k) / 42.4 | 2 dB: −48 … −12 (19) | 31.3 px |
| D | 1200 × 700 | 3.00 to 3.37 kHz (1/6 octave), −20 to −14 dB | Linear s = 50 Hz: 3.00k, 3.05k, 3.10k, 3.15k, 3.20k, 3.25k, 3.30k, 3.35k (8). s = 20 Hz fails (43.4 px). | 109.3 px / 49.0 | 0.5 dB: −20.0 … −14.0 (13) | 47.0 px |
| E | 920 × 540 | **minimum span at the minimum size:** 1.00 to 1.12 kHz (1/6 octave), −30 to −24 dB (6 dB) | Linear s = 20 Hz: 1.00k, 1.02k, 1.04k, 1.06k, 1.08k, 1.10k, 1.12k (7). s = 10 Hz fails (43.5 px against 49). | 87.3 px / 49.0 | 0.5 dB: −30.0 … −24.0 (13) | 33.7 px |
| F | 920 × 540 | **minimum span at the minimum size, at the bottom of the default range:** 20.0 to 22.4 Hz (1/6 octave), −90 to −84 dB | Linear s = 0.2 Hz: 20.0, 20.2, …, 22.4 (13). s = 0.1 Hz fails (21.7 px). | 43.5 px / 42.4 | 0.5 dB: −90.0 … −84.0 (13) | 33.7 px |

Edge checks: in A, "20k" at x = 888 ends at 897.9 ≤ 902; in E, "1.12k" at x = 597.4 ends at 613.9 ≤ 622; in F, "22.4" ends at 610.6 ≤ 622.

#### 5.9.9 Fit and no-op cases

- **At Fit (both axes):** a left or middle drag does nothing (G8): no pan, no reset, the crosshair stays and the readout continues. A horizontal wheel is passed to the parent. A vertical wheel that would zoom out is consumed and changes nothing. The zoom panel is hidden.
- **One axis at Fit:** a drag pans only the zoomed axis; the other axis is clamped and does not move. The overview window then spans the strip's full height (or width) on that axis.
- **At a limit:** zooming in at the minimum span, zooming out at the home range, and panning against a home edge change nothing. There is no flash, sound, toast or animation.
- **Returning to Fit** by zooming out step by step: when both spans reach the home spans, the view is set exactly to the home range (§5.9.2), the panel disappears and the hint switches to its Fit text.
- **Fit** (double-click or the Fit button) never resets curves and never changes a parameter.

#### 5.9.10 Changes from the plan

| # | Plan default | This spec | Reason |
|---|---|---|---|
| 1 | One notch scales the span by 0.8 or 1.25; Shift by 0.95 or 1.05 | The factors apply **per notch-equivalent**: `k = 0.8^n` and `0.95^n`, where n is the wheel delta divided by one notch (0.234375 in JUCE on Windows), clamped to ±3 per event. Zooming in by one notch is 0.8 and out is 1.25, as planned. Fine zooming out is 1 / 0.95 = 1.0526 instead of 1.05. | Touchpads and high-resolution wheels send many small deltas; counting each event as a notch would make touchpad zoom far too fast. With exact inverse factors, zooming in and back out returns to the same view. |
| 2 | Horizontal wheel pans frequency (no amount) | 10 % of the visible log span per notch-equivalent; an event pans only when `|deltaX| > |deltaY|`, otherwise it zooms | The plan gave no number. The dominant-axis rule keeps a slightly diagonal touchpad swipe from zooming and panning at once. |
| 3 | Home-range change: clamp, keep span, else Fit | Decided **per axis**, and an axis at Fit follows the new home range | A Level chip change should not reset the frequency zoom, and a chip change while unzoomed must not produce a "zoomed" state. |
| 4 | Overview: click outside the rectangle centres the view | A **press** outside the window centres at once, and continuing the drag pans | One gesture to jump and fine-tune. It also removes any click or double-click ambiguity in the strip. |
| 5 | Not specified | Middle click, Ctrl+click and a right-click during a drag do nothing; a release that ends a pan, box or no-op drag never resets; the release that completes a double-click does not start a reset | Clarifications, so no gesture can reset curves by accident. |
| 6 | Not specified | The hover card and clipboard use 4 significant figures while the visible span is below 1 octave (§5.5, §5.6) | At deep zoom, 3 figures are coarser than the axis labels and the pixel resolution. The unzoomed formats, and the T-012 tests that pin them, are unchanged. |
| 7 | An indicator and an overview strip | Both in one **zoom panel** at the top-right of the data rect | One rectangle to keep clear of the legend, badge, toasts, hint and hover card, at every size. |
| 8 | One view change per editor update | The same, and view-change repaints follow the T-029 frame pacing (§12) | A view change repaints the whole data rect, like a new frame, so it must obey the same pacing to stay inside the CPU budget. |
| 9 | Esc cancels a box when the editor has focus | The same; SpectrumView never takes keyboard focus itself | Taking focus on a click could stop the host's Space (transport) key from working in some hosts. |
| 10 | Not specified | The wheel over the zoom panel passes through to the data rect | The panel covers part of the data rect; the wheel should not stop working there. |
| 11 | Not specified | The box-zoom preview outlines the widened result when an axis is below its minimum span | The user sees in advance what the release will produce. |
| — | Minimum spans 1/6 octave and 6 dB; 4 px drag threshold; 250 ms click defer; 4 px box cancel | **Unchanged** | Checked against the grid rules: the 1/6-octave span at 560 px still has 7 to 13 labels (§5.9.8, E and F), and 6 dB at 404 px has 13 dB lines 33.7 px apart. |

#### 5.9.11 What the mockup simplifies

- **Engine:** the mockup's synthetic analyser keeps 4096 log-spaced points from 10 Hz to Nyquist as its stand-in for the engine's bin-domain spectra, and draws the ones in view (at most 2 vertices per physical column, §5.1). Zooming therefore shows more detail, as the real re-mapping does. The real engine uses linear FFT bins, so below the bin-spacing hint threshold the plugin shows straight segments between bins, while the mockup stays smooth.
- **Wheel units:** the browser reports 100 px (or 3 lines) per notch rather than JUCE's 0.234375. The mockup normalises to notches the same way (§5.9.2).
- **Double-click:** the mockup uses the browser's click count on the second press, which follows the OS double-click time, the same as JUCE's `mouseDoubleClick`.
- **Rendering:** the mockup draws with Canvas 2D and DOM elements. It shows the look and behaviour, not the T-029 drawing method of §12.

## 6. Control inventory

Every roadmap parameter maps to one control in the control bar or header, plus a row in the Settings panel. "Wheel" is one notch; "Fine" means Shift is held.

| Param ID | Control, location | Size | Label | Display format | Default | Drag | Wheel (Fine) | Text entry accepts |
|---|---|---|---|---|---|---|---|---|
| `channelMode` | Segmented control, header; choice field in Settings | Segments 32 to 44 × 24 | (none) "Mid", "Side", "L", "R", "L+R", "L/R", "M/S" | Selected segment; Settings shows "Mid", "Side", "Left", "Right", "L+R", "L/R", "M/S" | Mid | Settings field: 1 step per 20 px (60) | ±1 option, clamped | (menu) |
| `fftSize` | Choice chip, bar A | 60 × 40 | BLOCK | "4096" (samples; the unit is in the tooltip and the Settings label) | 4096 | 1 step per 20 px (60) | ±1 | (menu) |
| `overlap` | Choice chip, bar A | 68 × 40 | OVERLAP | "4×" | 4× | same | ±1 | (menu) |
| `window` | Choice chip, bar A | 76 × 40 | WINDOW | "Hann", "Blackman", "Hi-Res", "Flat-top" | Hann | same | ±1 | (menu) |
| `avgTime` | Float chip, bar A | 72 × 40 | AVG TIME | 0 shows "Off"; 1 to 999 shows "350 ms"; 1000 to 9999 shows "1.00 s"; 10000 shows "10.0 s" | 1.00 s | 240 px over the normalised range (skew centre 500 ms). Snap: 10 ms below 1 s, 100 ms at or above. | ±0.02 normalised (±0.004), always at least one snap step | "250", "250 ms", "1.5 s", "off" |
| `smoothing` | Choice chip, bar A | 68 × 40 | SMOOTH | "Off", "1/48", "1/24", "1/12", "1/6", "1/3", each followed by the unit "oct" (except Off) | 1/12 oct | 1 step per 20 px (60) | ±1 | (menu) |
| `slope` | Float chip, bar B | 92 × 40 | SLOPE | "+4.5" + "dB/oct"; always signed, 1 decimal; "0.0" at zero | +4.5 | 240 px for the full −12 to +12 (5 px per 0.5 step); 2400 px fine | ±0.5 (±0.5) | "3", "−1.5", "2 dB/oct" |
| `secondary` | Choice chip, bar B | 96 × 40 | 2ND CURVE | "Off", "RT max", "All-time max", "All-time avg" | RT max | 1 step per 20 px (60) | ±1 | (menu) |
| `freqLow` | Low half of the Frequency chip, bar C | about 54 × 18 half | FREQUENCY | Per the §5.5 frequency format, for example "20.0" + "Hz" | 20.0 Hz | 240 px full log range | × 2^(±1/6) (× 2^(±1/24)), rounded to 3 significant figures | "20", "20 Hz", "0.5k" |
| `freqHigh` | High half of the Frequency chip | about 56 × 18 half | (shared) | "20.0" + "kHz" | 20.0 kHz | same | same | "18k", "18 kHz", "18000" |
| `dbLow` | Low half of the Level chip, bar C | about 40 × 18 half | LEVEL | Signed integer, no unit on this half: "−90" | −90 | 240 px full range (2 px per dB); fine 10 px per dB | ±1 dB | "−96", "−96 dB" |
| `dbHigh` | High half of the Level chip | about 46 × 18 half | (shared) | Signed integer + "dB": "+6" + "dB"; zero shows "0" | +6 | same | ±1 dB | "0", "+12 dB" |
| `showSidechain` | Toggle button "SC", header; On/Off field in Settings | 60 × 28 | SC | On: accent state. Off: normal. Settings shows "On" / "Off". | On | none | up = On, down = Off | (none) |
| `meterScale` | Inline choice chip, meter strip | 96 × 28 | SCALE | "dBFS", "K-20", "K-14", "K-12" | dBFS | 1 step per 20 px (60) | ±1 | (menu) |
| `rmsWindow` | Inline float chip, meter strip | 96 × 28 | RMS | "300 ms"; "1.50 s" at or above 1000 | 300 ms | 240 px normalised (skew centre 300 ms). Snap: 10 ms below 1 s, 100 ms at or above. | ±0.02 normalised (±0.004) | "300", "300 ms", "1.2 s" |

Further rules:
- In the Level chip, the separator between the halves is "…" in `text-tertiary` (12 px). The Frequency chip reads "20.0 Hz … 20.0 kHz" and the Level chip reads "−90 … +6 dB".
- Values rounded to 3 significant figures by drag or wheel are what the parameter stores. Typed values are stored as typed, clamped to the range and rounded to 0.1 (or to the step).
- The Settings-panel fields are 128 × 24 inline chips with the same behaviour. They are labelled with the full names: Channel mode, Block size, Overlap, Window, Average time, Smoothing, Slope, Secondary curve, Sidechain trace, Frequency low, Frequency high, Range low, Range high, Meter scale, RMS window.

**Tooltips.** Tooltips are original text, not Voxengo's. The mockup's `TIPS` table is the canonical copy. Each tooltip ends with a second line in `text-secondary` at 10 px:
- float controls: "Drag or scroll to change, Shift for fine. Click to type. Double-click to reset.";
- choice controls: "Click to choose. Drag or scroll to step. Double-click to reset.".

The Block tooltip is dynamic: "FFT block size: 4096 samples, 85.3 ms, 11.7 Hz bin spacing at 48 kHz. …".

**Actions** (not parameters):

| Action | Control | Size | Behaviour |
|---|---|---|---|
| Freeze | Header toggle "Freeze" | 84 × 28 | §5.7 |
| Reset spectrum curves | Header button "Reset curves", and a left-click on the spectrum (after the 250 ms defer, §5.9.3 G11) | 116 × 28 | `requestSpectrumReset()`; toast as in §5.6 |
| Fit view (T-034) | "Fit" button in the zoom panel, and a double-click on the spectrum | 40 × 24 | Returns the view window to the home range (§5.9). Not a parameter, not undoable, not saved. |
| Reset statistics | Meter-strip button "Reset" | 68 × 28 | `requestStatsReset()`. The next snapshot shows the cleared values. The editor also clears its own held values (peak-hold ticks, TP hold, crest window) immediately. |
| Collapse meters | Header icon toggle | 28 × 28 | §4.5; persisted in `ViewState.metersCollapsed` |
| Settings panel | Header icon button | 28 × 28 | §6.1 |

### 6.1 Settings panel

- **Frame:** a popover anchored at (W − 572, 48), size 560 × 336, `surface` fill, 1 px `border`, radius 8, with the popover shadow (§3.3).
- **Title bar:** 36 high, with these items:
  - the title "Settings" (`body`, weight 600);
  - an "About" text button at popover-relative (460, 4, 60, 28): `body` in `text-secondary`, transparent, radius 6, hover `surface-3` with `text-primary`. It opens the About view (§6.2);
  - a 28 × 28 close button at (528, 4), an × drawn with two 1 px lines.
  
  A 1 px `border` line sits under the title bar.
- **Body:** two 256 px columns with a 16 px gap and 12 / 16 / 16 padding (top, sides, bottom).
  - Column 1, section "SPECTRUM" (`caps`, `text-tertiary`, 20 high): 9 rows of 28 for Channel mode, Block size, Overlap, Window, Average time, Smoothing, Slope, Secondary curve, Sidechain trace.
  - Column 2, section "DISPLAY RANGE": Frequency low, Frequency high, Range low, Range high. Then section "METERS": Meter scale, RMS window.
- **Rows:** the label in `body` / `text-secondary` on the left, and a 128 × 24 field on the right.
- **Behaviour:**
  - Opens and closes with the gear.
  - Closes on Esc, a click outside it, or the close button.
  - Focus moves into the popover on open and is trapped there. Tab cycles the About button, the fields in column order, then Close. On close, focus returns to the gear.
  - It does not block the spectrum or meters, which keep updating underneath.

### 6.2 About view (credits and notices)

**Purpose:** show the product identity and version, and meet the notice obligations: OFL clause 2 for the fonts (T-022 action items 1 and 2), and the BSD notice for pffft and the JUCE notice in the binary.

**How to reach it:**
- Click **About** in the Settings title bar (§6.1). The About view replaces the Settings content inside the same popover. It is a view switch, not a second window.
- There is no other entry point: no header button, no menu, no shortcut. The approved header layout stays unchanged.

**Frame:** the same popover as Settings, anchored at (W − 572, 48), but **560 × 440**. At the minimum editor height of 540 it ends at y = 488. It uses the same fill, border, radius and cached shadow (§3.3). Switching between Settings and About resizes the popover instantly, with no animation, and re-renders the cached shadow once.

**Layout** (popover-relative coordinates):

| Item | Rect | Style |
|---|---|---|
| Back button | (4, 4, 28, 28) | A "<" chevron from two 1 px `text-secondary` lines, 5 × 10, centred. Hover: `surface-3` fill, radius 6. Returns to the Settings view. |
| Title | (36, 0, 200, 36) | "About", `body`, weight 600, `text-primary`, vertically centred |
| Close button | (528, 4, 28, 28) | The same as in Settings. Closes the whole popover. |
| Title divider | (0, 36, 560, 1) | `border` |
| Logo mark | (16, 52, 32, 32) | `logo-mark.svg` in `accent` |
| Wordmark | (60, 52, 81, 18) | `logo-wordmark.svg` in `text-primary` |
| Version line | (60, 72, 480, 14) | `body-sm`, `text-secondary`. Format: "Version {version} · {format} · 64-bit", for example "Version 1.0.0 · VST3 · 64-bit". `{version}` is `JucePlugin_VersionString`. `{format}` is "VST3" or "Standalone", from `AudioProcessor::wrapperType`. |
| Company line | (60, 88, 480, 14) | "Investigate Audio", `body-sm`, `text-tertiary`. It follows the roadmap Identity section, which may change before M5. |
| Divider | (16, 112, 528, 1) | `border` |
| Section title | (16, 120, 528, 20) | "CREDITS AND LICENSES", `caps`, `text-tertiary` |
| Notices viewport | (16, 140, 528, 284) | Scrollable, detailed below. Bottom padding 16, so the popover height is 440. |

**Notices viewport:**
- **Widget:** a read-only, multi-line `juce::TextEditor` with word wrap on, no caret, a transparent background and no border. Text selection and Ctrl+C work, so a user can copy a notice.
- **Scroll bar:** vertical only, 6 px wide, inside the viewport's right edge. The track is transparent. The thumb is `border`, radius 3, and turns `text-tertiary` on hover or drag. It is visible only when the content overflows, which it always does.
- **Mouse wheel:** scrolls 3 lines per notch.
- **Text style:**
  - Headings: `body`, weight 600, `text-primary`, with 12 px space above (none above the first heading).
  - Notice lines: `body-sm` in `text-secondary`, with 4 px after each paragraph.
  - Full license texts: `body-sm` in `text-secondary`, shown exactly as stored, with line breaks preserved. Word wrap applies only when a line is wider than 522 px.
- **Content, in this order.** The text is fixed. Build it once when the view first opens, then cache it.
  1. Heading "Fonts".
     - "Inter 4.1. Copyright (c) 2016 The Inter Project Authors (https://github.com/rsms/inter). Licensed under the SIL Open Font License, Version 1.1."
     - "JetBrains Mono 2.304. Copyright 2020 The JetBrains Mono Project Authors (https://github.com/JetBrains/JetBrainsMono). Licensed under the SIL Open Font License, Version 1.1."
     - "The font software is not sold separately. The full license texts are below and in the installed THIRD-PARTY-NOTICES.txt."
  2. Heading "Software".
     - "JUCE 8.0.15. Copyright (c) Raw Material Software Limited. {JUCE license line}". The license line depends on the JUCE license tier, which the user chooses before M5 (roadmap Risk 9). It stays a placeholder until M5 (T-019); JUCE 8's open-source option is AGPLv3 (§15, closed question 1).
     - "pffft (marton78 fork). A BSD-style license. The copyright notices, conditions and disclaimer follow in full." The copyright holders are listed as they appear in pffft's license file at the pinned version (they are expected to be Julien Pommier and, for the FFTPACK-derived parts, UCAR). They are copied from that file, not typed by hand.
  3. Heading "SIL Open Font License 1.1: Inter": the contents of `BinaryData::OFLInter_txt` (`third_party/fonts/OFL-Inter.txt`), verbatim.
  4. Heading "SIL Open Font License 1.1: JetBrains Mono": the contents of `BinaryData::OFLJetBrainsMono_txt` (`third_party/fonts/OFL-JetBrainsMono.txt`), verbatim.
  5. Heading "pffft license": the full text of pffft's license file at the pinned version, verbatim. The BSD terms require the conditions and disclaimer in binary distributions.
  6. Heading "JUCE license": only if the license tier chosen at M5 requires more text than the line in item 2.
- **Sources of the texts:** each full text comes from a binary-data resource, so nothing is retyped in C++.
  - The two OFL texts and the pffft license are in `third_party/fonts/` and compiled into `InvestigateAssets` by T-021: `BinaryData::OFLInter_txt`, `OFLJetBrainsMono_txt` and `LICENSEpffft_txt` (§15, closed questions 2 and 3).
  - If a resource is missing, the viewport shows "License text missing: {file name}" in `state-error` in place of that text. A test can then catch the omission.

**Behaviour:**
- Esc, a click outside the popover, or the close button closes the whole popover. The next time the gear opens it, it shows the Settings view.
- The Back button (or Backspace while the notices viewport does not have focus) returns to Settings.
- **Focus:** opening About moves focus to the Back button. Tab cycles Back, the notices viewport, then Close, and stays inside the popover. Returning to Settings puts focus on the About button.
- **Repaint:** static content. It repaints only on scroll, hover or selection.
- **Accessible names:**
  - "About Investigate" (the view);
  - "Back to settings";
  - "Close";
  - "Credits and licenses" (the viewport, with its full text as the accessible value).
- **Mockup:** `mockup.html` is the approved design and was deliberately left unchanged in the final pass. The About view is specified here only. It reuses existing tokens and components, so it needs no new visual approval.

## 7. Interaction conventions

### 7.1 Pointer

| Gesture | Chips (choice and float) and chip halves | Segmented Mode | Toggle buttons (SC) | Action buttons |
|---|---|---|---|---|
| Hover | `surface-3` fill; tooltip after 700 ms | The segment turns `surface-3` with `text-primary` text | `surface-3` fill | `surface-3` fill |
| Press and vertical drag (> 4 px) | Change the value (up increases). Float: 240 px for the full normalised range. Choice: 1 step per 20 px. Shift: ×0.1 for float, 60 px per step for choice. The cursor is `UpDownResizeCursor` over chips. | none | none | none |
| Click (≤ 4 px movement) | After a **250 ms defer**, a choice opens its menu and a float opens text entry. A second click inside the 250 ms cancels this. | Select the segment immediately | Toggle immediately | Act on release |
| Double-click | **Reset to the default** (`param->getDefaultValue()`), as one undoable gesture (`beginChangeGesture` / `setValueNotifyingHost` / `endChangeGesture`) | Reset to Mid | Reset to On | None (acts twice) |
| Alt + click | Reset immediately, with no defer. This is the accessibility alternative to a double-click. | Reset | Reset | none |
| Wheel | Step (§6). Consumes the event. | ±1 mode | Up = On, down = Off | none |
| Right-click | None (no context menu) | none | none | none |

Why the 250 ms defer: a double-click must reset without a menu or text field flashing up first. 250 ms is short enough to feel immediate, and double-clicks slower than that fall back to click, click.

Other pointer rules:
- **Menus:** a `juce::PopupMenu` styled by the LookAndFeel.
  - Fill `surface`, 1 px `border`, radius 6, padding 4, shadow as for the popover.
  - Items are 24 high in `body` / `text-primary`, with a hover fill of `surface-3`.
  - The current item has a 6 px `accent` dot at x = 8. The default item carries a trailing "default" in `text-tertiary` at 10 px.
  - The menu opens below the chip, 4 px gap, or above it if there is no room.
- **Text entry:**
  - A `juce::TextEditor` over the value, with a 4 px inset. Fill `bg-plot`, 1 px `accent` border, radius 4, `value` font. It opens pre-filled with the formatted value and unit, fully selected.
  - Enter or focus loss commits; Esc cancels.
  - Parsing:
    - Accepts ASCII "-" or U+2212, "," as the decimal separator, optional spaces, and case-insensitive units (Hz, k, kHz, ms, s, dB, dB/oct).
    - "off" or 0 means Off for `avgTime`.
    - Out-of-range values are clamped.
    - Unparseable text reverts to the old value and flashes the chip outline in `state-error` for 300 ms.
- **External-change indicator:** see §9, "automation-driven".

### 7.2 Keyboard

- **Focus order** (Tab, with Shift+Tab reversing):
  1. Mode, then SC, Freeze, Reset curves, Meters, Settings;
  2. the control-bar chips left to right (with the Frequency low, Frequency high, Level low and Level high halves in order);
  3. Scale, RMS, Reset statistics.
  
  Hidden controls are skipped. The spectrum view is not focusable.
- **Focus ring:** a 2 px `accent` outline, 1 px outside the bounds, following the corner radius. It is drawn only for keyboard focus (`juce::Component::hasKeyboardFocus` after a Tab), not after a mouse click.
- **Keys on a focused chip or half:**
  - Up/Right: +1 wheel step. Down/Left: −1. Shift: fine.
  - PageUp/PageDown: 5 wheel steps.
  - Enter/Space: the primary action (menu or text entry).
  - Delete/Backspace: reset to the default.
- **Keys on Mode:** arrows step and Delete resets.
- **Keys on buttons:** Space/Enter activate.
- The editor defines no global shortcuts, because hosts own most keys.
- **Zoom and pan (T-034):** there are no keys for zoom or pan, and Space is never used (it is the host's transport key). SpectrumView and the zoom panel are not focusable and never take keyboard focus on a click. The only key is **Esc during a box drag** (§5.9.3 G15): if the host has given the editor keyboard focus, the editor's key handling forwards Esc to SpectrumView, which cancels the box.

## 8. Meters and statistics

All values come from `MeterSnapshot`. Ballistics are display-only and never change the snapshot.

### 8.1 Update rates

- **Bars, lamps and the correlation bar:** read the snapshot on every vblank (≤ 60 Hz, at least 30 Hz) and repaint only the bars column and the correlation-bar bounds.
- **Numeric text:** every value in the table, loudness block, correlation value and balance refreshes at **10 Hz** (every 100 ms, from the same vblank callback with a timestamp check). This keeps the numbers readable and repaint cost low. A label is repainted only when its string changes.

### 8.2 Level bars

- **Geometry:** a linear dB scale from **−60 to +6 dBFS** for every meter scale, so the bar geometry never jumps. The track is `bg-plot` with radius 2.
- **Peak bar:** the sample peak (`samplePeak[ch]`). In the snapshot, this value and `truePeak[ch]` are each the **maximum over the last 50 ms** (roadmap MeterSnapshot). A reader at 30 to 60 Hz therefore sees every block peak. The editor applies no extra peak detection; it only applies the display ballistics below.
- **RMS bar:** `rms[ch]` plus 3.0103 dB in K modes (the AES17 sine reference). In dBFS mode it is shown unreferenced, as the roadmap specifies.
- **Drawing:**
  - The RMS part is drawn solid, from −60 up to the RMS value.
  - The part from RMS up to the peak is drawn in the pre-blended peak colours.
  - Each part is stacked by zone: the portion inside each zone takes that zone's colour, like a segmented hardware meter.
  - A 1 px `bg-plot` separator line sits at the RMS top when the peak is higher.
  - If the RMS is at or above the peak (possible in K modes for square-like signals), only the RMS part is drawn.
- **Zones** (in display units):
  - dBFS: safe below −12, warm from −12 to below −3, hot at −3 and above.
  - K-20, K-14, K-12: safe below 0 K, warm from 0 to +4 K, hot above +4 K. That is below −N dBFS, from −N to −N + 4, and above −N + 4.
- **Ticks:**
  - dBFS: every 6 dB from +6 to −60, or every 12 dB when 6 dB is under 20 px.
  - K modes: every 4 K units (8 when under 20 px), labelled in K units (for example K-20 ticks run "+24", "+20", …, "0", …, "−40").
  - Labels use `axis-meter` in `text-tertiary`, right-aligned at x = 24, with a 1 px `meter.track-line` across both tracks at each tick.
- **Ballistics** (editor side):
  - **Peak bar:** instant rise, then a linear fall of **20 dB/s**. The display value is `max (new, previous − 20 × dt)`.
  - **Peak-hold tick:** 2 px `meter.hold-tick`. It holds the highest peak for **1500 ms**, then falls at **30 dB/s**, never below the bar.
  - **RMS bar:** no extra smoothing; it shows the snapshot value, whose window is `rmsWindow`.
- **Clip lamps:** 22 × 10 above each track, radius 2.
  - Lit in `meter.clip` when `sampleClipCount + truePeakClipCount > 0` for that channel. The lamp therefore latches until **Reset statistics** clears the counts.
  - Off state: `surface-2`.
  - Lamps are not clickable, so a misclick cannot clear an integrated-loudness measurement.

### 8.3 Statistics table (per channel; headers "L" and "R")

| Row (label + unit) | Source | Value | Format |
|---|---|---|---|
| Peak dBFS | `samplePeak` (50 ms max) | The peak-hold tick value (§8.2), so it is readable | Signed, 1 decimal: "−6.0", "+0.3". "0.0" when it rounds to zero. "−inf" when non-finite or ≤ −120 dBFS. |
| Peak max dBFS | `peakMax` | Snapshot value | same |
| TP dBTP | `truePeak` (50 ms max) | Held like the peak tick: 1500 ms hold, then 30 dB/s | same |
| TP max dBTP | `truePeakMax` | Snapshot value | same |
| Headroom dB | derived: `0 − truePeakMax` | Per channel | 1 decimal with no "+" ("1.2"); negative in `state-error` ("−0.4"); "—" while TP max is −inf |
| RMS, unit shows the scale ("dBFS", "K-20", …) | `rms` | dBFS mode: raw. K-N: `rms + 3.0103 + N` in K units | Signed, 1 decimal; "−inf" as above |
| Crest dB | derived | `max (samplePeak over the last rmsWindow ms) − rms (raw dBFS)`. The editor keeps a fixed 256-entry ring of (time, samplePeak) from its snapshot reads, so no allocation is needed. | Unsigned, 1 decimal ("12.4"); "—" when either input is −inf. A sine reads 3.0. |
| Clips | `sampleClipCount` | Integer | "0" to "9999", then "9999+". The cell becomes a lamp (`meter.clip` fill, `text-on-accent`, radius 4, padding 0 × 4) when > 0. |
| TP clips | `truePeakClipCount` | Integer | same |

- Row labels are `body-sm` / `text-secondary`, with the unit in `caps`-sized `text-tertiary` 4 px after the label. Values are `readout` / `text-primary`, right-aligned.
- Rounding: round half away from zero on value × 10, so −0.04 shows "0.0" and +0.05 shows "+0.1".
- Nothing ever shows NaN. Any non-finite input shows "−inf" (levels) or "—" (derived values).

### 8.4 Loudness block (98 high; values in LUFS)

- **Title row (16):** "LOUDNESS" (`caps`, `text-tertiary`) on the left and "LUFS" (`unit`, `text-tertiary`) on the right.
- **Row 1**, three equal columns: labels "M", "S", "I" (`caps`), with values in `readout-lg`.
- **Row 2**, 8 px below: labels "M MAX", "S MAX", "LRA", with values in `value` (13 px mono). LRA carries the unit "LU".

| Field | Source | Format |
|---|---|---|
| M | `momentary` | 1 decimal, no "+": "−14.2". "−inf" when non-finite or below −99.9. |
| S | `shortTerm` | same |
| I | `integrated` | 1 decimal; "—" until the first gated block (non-finite) |
| M max | `maxMomentary` | 1 decimal; "—" when non-finite |
| S max | `maxShortTerm` | same |
| LRA | `loudnessRange` | 1 decimal + "LU"; "—" while I is "—" |

### 8.5 Correlation and balance (68 high)

- **Title row:** "CORRELATION" on the left; the value on the right in `readout`, 2 decimals, signed: "+0.82", "−0.15", "0.00".
- **Bar:** 176 × 12 at 4 px below the title. The track is `bg-plot` with radius 2, with a 1 px `border` centre line at 0 and 1 px `meter.track-line` ticks at ±0.5. The fill runs from the centre to the value at 8 px high (inset 2): `meter.corr-positive` (#34C6A8) for values ≥ 0 and `meter.corr-negative` (#FF8A3D) for values below 0. A 2 px `text-primary` indicator marks the value. Side and colour together make the sign readable without colour.
- **Scale row:** "−1", "0", "+1" in `axis-meter` / `text-tertiary`, spread across the bar.
- **Balance row:** "BALANCE" (`caps`) on the left; the value on the right in `readout`. The format uses the side that is louder:
  - "L 1.2 dB" (the snapshot balance is negative, so the left channel is louder);
  - "R 0.8 dB" (positive);
  - "C 0.0 dB" when |b| < 0.05;
  - "L >60 dB" or "R >60 dB" beyond 60.
- **Smoothing:** the DSP already averages over a fixed 500 ms window for correlation (roadmap Parameter table constraints) and a fixed 3 s window for balance. The correlation block tooltip is the mockup's canonical text, which already states the 500 ms and 3 s windows. When shipped, it uses U+2212 for the minus sign. The editor adds only a one-pole display filter with **τ = 50 ms** on the correlation bar to hide 30 Hz stepping. The text shows the filtered value.

### 8.6 Mono input (`isMono`)

- One bar, centred (x 44 to 66), labelled "MONO". Its lamp sits above it.
- The table header shows "MONO" over the right value column, and the left column is blank. All rows use channel 0.
- The correlation block renders at 40 % opacity. Its value and balance show "—", the bar has no fill or indicator, and its tooltip reads "Correlation and balance need two channels."

### 8.7 Meter strip when audio is stopped

- **Host still calling `processBlock` with silence:** the bars fall at their ballistics to the floor, and the text shows "−inf". The max values, clip counts, I and LRA hold.
- **Host not calling `processBlock`** (transport stopped in hosts that stop processing, host bypass, or an offline render pause): the snapshot does not change. The editor detects this with `MeterSnapshot.blockCounter`, which the DSP increments on every `process` call (roadmap MeterSnapshot).
  - **Stale:** on each vblank the editor compares `blockCounter` with the last value it saw. If the counter has not changed for **250 ms** (measured with `juce::Time::getMillisecondCounterHiRes`), the snapshot is stale.
  - **While stale:** the editor treats `samplePeak`, `truePeak`, `rms`, `momentary` and `shortTerm` as −inf. The bars and hold ticks fall at their normal ballistics, and those readouts show "−inf". Peak max, TP max, the clip counts, headroom, I, LRA, M max and S max hold their last values. Correlation and balance hold their last values.
  - **Recovery:** the first counter change ends the stale state immediately.
  - **Wrap-around:** compare with `!=` only, never `<`, so the uint32 wrap is harmless.

### 8.8 Sample rate

`MeterSnapshot.sampleRate` (or the frame's sample rate) appears in the header as "44.1 kHz", "48 kHz", "88.2 kHz", "96 kHz", "176.4 kHz" or "192 kHz": kHz with 1 decimal, and a trailing ".0" dropped. The editor also uses it for the Nyquist clamp (§5.4) and the Block tooltip.

## 9. States

| State | Chips / halves / inline chips | Buttons and toggles | Segmented | Plot / meters |
|---|---|---|---|---|
| Normal | `surface-2` fill; value in `text-primary`; label in `text-secondary` | Transparent; `text-secondary` | Track `surface-2`; unselected text `text-secondary` | as specified |
| Hover | `surface-3` fill | `surface-3` fill, `text-primary` | Segment `surface-3`, `text-primary` | Crosshair and card |
| Active / dragging | `surface-3` fill, 1 px `accent` border, value text `accent` | Pressed: `surface-3` | Selected: `accent` fill, `text-on-accent`, weight 600 | n/a |
| On (toggle) | n/a | `accent-tint` fill, 1 px `accent` border at 50 %, `accent` icon and text | n/a | n/a |
| Frozen | n/a | Freeze: `state-frozen-tint` fill, `state-frozen` border at 60 %, `state-frozen` text | n/a | Badge and frame, §5.7 |
| Disabled / inactive | Not used; every parameter is always editable | SC with the sidechain bus inactive: `text-disabled` icon and text, plus a 6 px hollow ring after the label. When `showSidechain` is on, a 1 px `border` outline. Still clickable. Tooltip: "No sidechain input is connected. Route a track to the sidechain bus in your host." | n/a | Mono correlation block at 40 % (§8.6) |
| Clamped | Set value in `state-warn`; tooltip explains (§5.4) | n/a | n/a | Grid uses effective values |
| Invalid entry | 2 px `state-error` outline for 300 ms, then revert | n/a | n/a | n/a |
| Automation-driven | When a parameter changes without a local gesture (host automation, state load, or another control such as the Settings copy), the chip shows a 4 × 4 `accent` dot at its top-right (4 px inset) for 600 ms after the last change. The value updates within 1 frame (T-011). | SC: the state updates, no dot | The selection updates | Auto-unfreeze on a mode change |
| Bypassed | Investigate has no bypass parameter, and host bypass is not reported to the editor. With host bypass the snapshot stops changing, so the stale rule of §8.7 applies. | | | |

## 10. Accessibility

### 10.1 Contrast (WCAG 2.2; relative luminance from sRGB)

| Foreground | Background | Ratio | Requirement | Use |
|---|---|---|---|---|
| `text-primary` #E8ECF3 | `bg-plot` #0A0D13 | 16.4 : 1 | 4.5 | Hover card values (on `card` #151B27: 14.6) |
| `text-primary` | `surface-2` #1A2130 | 13.6 : 1 | 4.5 | Chip values |
| `text-primary` | `surface` #131924 | 14.9 : 1 | 4.5 | Meter readouts |
| `text-secondary` #A3AEC2 | `surface-2` | 7.2 : 1 | 4.5 | Chip labels, units |
| `text-secondary` | `surface` | 7.9 : 1 | 4.5 | Table labels, loudness labels |
| `text-secondary` | `bg-plot` / `card` | 8.7 : 1 / 7.7 : 1 | 4.5 | Legend, hover labels |
| `text-tertiary` #75829A | `bg-plot` | 5.0 : 1 | 3.0 (grid labels) | Frequency and dB axis labels, hint line |
| `text-tertiary` | `surface` | 4.5 : 1 | 4.5 | Section titles, meter scale, units in the table |
| `text-on-accent` #0A0D13 | `accent` #62D6F0 | 11.5 : 1 | 4.5 | Selected Mode segment |
| `text-on-accent` | `state-frozen` #8FB8FF | 9.7 : 1 | 4.5 | FROZEN badge |
| `text-on-accent` | `meter.clip` #FF4F64 | 6.1 : 1 | 4.5 | Lit clip-count cells |
| `state-warn` #EAC75A | `surface-2` | 9.8 : 1 | 4.5 | Clamped values |
| `state-error` #FF4F64 | `surface` | 5.5 : 1 | 4.5 | Negative headroom |
| `accent` | `bg-app` #0E1219 | 11.0 : 1 | 3.0 | Toggle text and icons, focus ring |

Non-text graphics against `bg-plot` (WCAG 1.4.11, ≥ 3:1):

| Graphic | Ratio |
|---|---|
| Primary A line | 11.5 |
| Primary B line | 11.0 |
| Secondary A | 5.2 |
| Secondary B | 5.1 |
| Sidechain edge (70 %) | 4.0 |
| meter.safe | 9.1 |
| meter.safe-peak | 3.4 |
| meter.warm | 11.9 |
| meter.hot | 8.3 |
| meter.clip | 6.1 |

### 10.2 Colour-vision check

- **Method:** simulation with the Machado, Oliveira and Fernandes (2009) matrices at severity 1.0 for protanopia, deuteranopia and tritanopia, plus a luminance-only greyscale, applied in linear RGB. `mockup.html` has a "Vision check" selector that applies these as SVG `feColorMatrix` filters (`color-interpolation-filters="linearRGB"`) to the whole live editor. The table values below were computed with the same matrices.
- **Results:**

| Colour | Normal | Protanopia | Deuteranopia | Tritanopia | Greyscale luminance |
|---|---|---|---|---|---|
| Primary A | #62D6F0 | #C3D0F1 (pale blue) | #AEC0F0 (pale blue) | #00E0DE (cyan) | 0.57 |
| Primary B | #FFB454 | #CEB849 (yellow) | #DFCA56 (yellow) | #FFA29F (pink) | 0.55 |
| Sidechain | #A58BFF | #639FFF (mid blue) | #669BFC (mid blue) | (violet-grey) | 0.34, at 26 % / 70 % |
| Secondary A / B | #3E8FA3 / #A87A3C | dark blue / dark olive | dark blue / dark olive | | 0.23 / 0.23 |

- **Pairs and their cues:**
  - A against B: blue against yellow is preserved in protanopia and deuteranopia, cyan against pink in tritanopia. In greyscale the two primaries are equal in lightness, so the cue is **fill against no fill**, which is independent of colour.
  - Primary against secondary: lightness (luminance 0.57 against 0.23, ratio 2.2 : 1) and width (1.5 px against 1 px).
  - Sidechain against everything else: the flat silhouette fill behind the curves, with a thin dim edge.
  - A secondary against the sidechain edge in protanopia or deuteranopia (both dim blue lines): only the sidechain has a fill beneath it.
  - Every curve's name and swatch is in the legend and the hover card, so no decision depends on colour alone.
- **Meters:** teal, yellow, orange and red become blue-grey, yellow, dark yellow and olive-brown under deuteranopia. The zones are also positional (fixed thresholds on the scale). The clip state is also shown as a number, and the correlation sign is shown by the side of the centre line.

### 10.2a Zoom and pan elements (T-034)

Text (WCAG 1.4.3, at least 4.5 : 1). Relative luminances: `card` 0.0109, `surface-3` 0.0251, `bg-plot` 0.0040.

| Foreground | Background | Ratio | Use |
|---|---|---|---|
| `zoom.range-text-zoomed` #E8ECF3 | `zoom.panel-fill` #151B27 | 14.6 : 1 | Range text of a zoomed axis |
| `zoom.range-text-full` #A3AEC2 | `zoom.panel-fill` | 7.7 : 1 | Range text at Fit, Fit label |
| `zoom.bin-hint-text` #EAC75A | `zoom.panel-fill` | 10.5 : 1 | Bin-spacing hint |
| `text-primary` #E8ECF3 | `zoom.fit-hover-fill` #232C3E | 11.8 : 1 | Fit label on hover |
| `zoom.fit-pressed` #62D6F0 | `zoom.fit-hover-fill` | 8.3 : 1 | Fit label while pressed |
| `text-tertiary` #75829A | `bg-plot` #0A0D13 | 5.0 : 1 | Deep-zoom axis labels (same colour as §5.3) |

`text-tertiary` on `card` would be only 4.4 : 1, which is why no panel text uses it.

Non-text graphics (WCAG 1.4.11, at least 3 : 1):

| Graphic | Against | Ratio |
|---|---|---|
| Overview curve line `zoom.overview-line` #3E8FA3 | `zoom.overview-bg` #0A0D13 | 5.2 : 1 |
| View-window outline `zoom.window-stroke` #62D6F0 | `zoom.overview-bg` | 11.5 : 1 |
| View-window outline | the shaded strip (60 % `bg-plot` over the curve body) | 10.1 : 1 |
| View-window outline | the curve body inside the window, `zoom.overview-fill` #1C353F | 7.6 : 1 |
| Ghost outline `zoom.window-ghost` #75829A | `zoom.overview-bg` | 5.0 : 1 |
| Box outline `zoom.box-stroke` #E8ECF3 | `bg-plot` | 16.4 : 1 |
| Box outline | the densest primary-A fill (30 % #62D6F0 over `bg-plot`, #244955) | 8.2 : 1 |
| Box target outline `zoom.box-target` #75829A | `bg-plot` | 5.0 : 1 |

- Decorative, with no requirement: the overview body `zoom.overview-fill` (1.5 : 1, always paired with the 5.2 : 1 line), the panel border and the strip frame (the panel is identified by its text and strip, like the hover card), and the box fill.
- Cursors use the OS convention of a white fill with a 1 px black outline, so they show on any background.
- **Colour vision:** the window, ghost and box differ by position, line style and fill, not by hue. The box outline is neutral white, so it never reads as a curve. In the strip, the window outline (luminance 0.57) is much lighter than the overview line (0.23) under every simulation of §10.2. The bin-spacing hint is text, not colour alone, and which axis is zoomed is also readable from the range text itself.

### 10.3 Hit targets and names

- **Zoom targets (T-034):** the Fit button is 40 × 24. The overview window's hit area is at least 16 × 16; this is below 24 × 24 but covered by the WCAG 2.5.8 equivalent-control exception, because dragging in the data rect pans the same way. The whole strip (at least 186 × 38) accepts presses.
- **Minimum target:** 24 × 24 (WCAG 2.2 target-size minimum). Mode segments are 32 to 44 × 24; buttons and inline chips are 28 high; chips are 40 high; each chip half's hit area is its side of the chip, split at the "…" separator, at the full chip height of 40, so every half is at least 50 × 40 (the mockup approximates this with text-sized hit areas); the resize grip is 12 × 12, and host-edge resizing is the alternative.
- **Accessible names** (`setTitle`) and values (`AccessibilityHandler` value interface, returning the formatted display string):
  - Channel mode, Block size, Overlap, Window, Average time, Smoothing, Slope, Secondary curve, Frequency low, Frequency high, Range low, Range high, Sidechain trace, Meter scale, RMS window.
  - Freeze display, Reset spectrum curves, Reset statistics, Show meters, Settings, About.
  - The About view names are listed in §6.2.
  - The spectrum view has the description "Spectrum display. Hover for a readout. Scroll to zoom frequency, Ctrl+scroll to zoom level, drag to pan, double-click to fit, Ctrl+drag to zoom into a box. Click to reset the max and average curves; right-click to copy the frequency." (updated by T-034).
  - Zoom panel (T-034): a group named "Zoom", whose accessible value is the visible range, for example "1.00 to 1.12 kHz, −30.0 to −24.0 dB", followed by ", bin spacing 11.7 Hz" while row 3 shows. The Fit button is named "Fit view", with the description "Show the full range set by the Frequency and Level chips", and supports the press action. The overview strip is named "Spectrum overview" and has the same value as the group. None of them is in the Tab order (§7.2), but assistive technology can press Fit through its accessibility handler.
  - Meter readouts are labels with names such as "Left peak max", "Integrated loudness" and "Correlation".

## 11. Assets (`assets/ui/`)

| File | viewBox | Use | Notes |
|---|---|---|---|
| `logo-wordmark.svg` | −1 0 152 34 | Header wordmark (drawn 81 × 18) | Monoline lowercase "investigate", 2.4 stroke, `currentColor`; the first i-dot is a lens ring |
| `logo-mark.svg` | 0 0 24 24 | Standalone app icon source, about box, installer | Lens with a small spectrum trace |
| `icon-freeze.svg` / `-16.svg` | 24 / 16 | Freeze | Six-arm snowflake |
| `icon-reset.svg` / `-16.svg` | 24 / 16 | Reset curves, Reset statistics | Counter-clockwise arrow |
| `icon-settings.svg` / `-16.svg` | 24 / 16 | Settings | Three sliders |
| `icon-sidechain.svg` / `-16.svg` | 24 / 16 | SC toggle | A silhouette plus a line, which mirrors the in-plot sidechain style |
| `icon-meters.svg` / `-16.svg` | 24 / 16 | Meters toggle | A panel with a right column and a chevron; drawn mirrored when collapsed |
| `cursor-hand-open.svg` (T-034) | 0 0 24 24 | Open hand over the overview window (§5.9.4) | Hotspot (12, 12) |
| `cursor-hand-grab.svg` (T-034) | 0 0 24 24 | Grabbing hand while panning | Hotspot (12, 12) |
| `cursor-zoom-box.svg` (T-034) | 0 0 24 24 | Ctrl held over the data rect, and during a box drag | Hotspot (7.5, 7.5), the open centre of the crosshair |

- **Cursor exception (T-034):** the three cursor SVGs use a fixed `#FFFFFF` fill with a 1 px `#000000` outline instead of `currentColor`, which is the OS cursor convention and keeps them visible on any background. They are rendered to images (§5.9.4) and never recoloured. They are otherwise clean: a `viewBox`, a `<title>`, no raster images, scripts or external references.
- The Fit button is text-only, so there is no Fit icon.

- **Size variants:**
  - The 24-grid icons use 2-unit strokes on integer coordinates, so they are pixel-exact at 24 px and 48 px.
  - The 16-grid variants use 1-unit strokes on half-integer coordinates, so they are pixel-exact at 16 px and 32 px.
  - Use `-16` at 16 px and 32 px (32 px is 200 % of the 16 px header icons), and the 24 set at 24 px.
- **SVG hygiene:** every file uses `currentColor` only (the sidechain icon adds `fill-opacity`), has a `viewBox` and a `<title>`, and contains no raster images, scripts or external references.
- **Colour at runtime:** the colour is applied by replacing `currentColor` with the state's token colour at load time (one `Drawable` per state colour, cached).
- **Resize grip:** drawn in code (§2).

**Fonts and license texts** are not in `assets/ui/`. T-021 downloads them, unmodified and with SHA-256 checks, from the pinned release archives (§3.2) into **`third_party/fonts/`**, and compiles them into the **`InvestigateAssets`** binary-data target (roadmap Decision 2026-09-28; T-011 correction, roadmap Decision 2026-10-01):

| File in `third_party/fonts/` | Source | `BinaryData` symbol |
|---|---|---|
| `Inter-Medium.ttf`, `Inter-SemiBold.ttf` | `Inter-4.1.zip`, `extras/ttf/` | `InterMedium_ttf`, `InterSemiBold_ttf` |
| `JetBrainsMono-Regular.ttf`, `JetBrainsMono-Medium.ttf` | `JetBrainsMono-2.304.zip`, `fonts/ttf/` | `JetBrainsMonoRegular_ttf`, `JetBrainsMonoMedium_ttf` |
| `OFL-Inter.txt` | Inter `LICENSE.txt` at tag `v4.1` | `OFLInter_txt` |
| `OFL-JetBrainsMono.txt` | JetBrains Mono `OFL.txt` at tag `v2.304` | `OFLJetBrainsMono_txt` |
| `LICENSE-pffft.txt` | pffft's license file at the pinned version | `LICENSEpffft_txt` |

The `InvestigateAssets` target also globs `assets/ui/`, so the SVGs above (including the T-034 cursors) are embedded automatically. The OFL texts are shown in the About view (§6.2) and also go into the installer's `THIRD-PARTY-NOTICES.txt` (T-019).

## 12. Performance rules (for T-011 to T-013)

- **Spectrum repaint:** repaint only when a new frame arrived, the hover position changed, a toast is fading, or a setting changed. Repaint only the plot region, never the whole editor.
- **Spectrum paint cost per frame:**
  - one cached-image blit (the grid and labels);
  - at most one gradient fill and one flat fill;
  - up to five strokes (≤ 2 vertices per physical column each);
  - the crosshair, up to four markers, and the card (a small rect plus text).
  
  There are no per-frame gradients other than the primary fill (the `ColourGradient` object is cached per size), no alpha layers (`beginTransparencyLayer`), no blurs and no shadows.
- **Meter repaint:**
  - The bars column and the correlation bar repaint every vblank.
  - Text labels repaint at no more than 10 Hz, and only when their string changes.
  - The header, control bar and strip header repaint only on interaction or parameter change.
- **Allocation:** no allocation in paint or in the vblank callback after the first frame. Reuse the paths, point buffers, crest ring buffer and preformatted strings; format into a fixed `char` buffer or reuse a `juce::String` with sufficient capacity.
- **Fallback:** if the T-012 paint budget is missed, the first thing to drop is the primary gradient. Replace it with a flat #62D6F0 fill at 14 %, which is visually close. Only after that consider OpenGL, with the architect's approval.

### 12.1 Zoom and pan (T-034, for T-037 and T-038)

Every element of §5.9 is drawn under the T-029 editor rendering rules (roadmap Decision 2026-10-03): no polygons drawn per frame; one `fillRectList` per colour; static parts cached as images; repaint only when a physical-pixel value changes; build only what lies under the clip.

- **Coalescing.** Wheel, drag and overview handlers write a pending view only. The editor update callback (vblank, at most 60 per second) applies it at most once per update.
  - The update is dropped when it would move none of the four view edges by 0.5 physical px or more in the current mapping. This is the "repaint only on a physical-pixel change" rule for views; it absorbs tiny touchpad deltas and wheel steps at a limit.
  - `setSpectrumView (fLo, fHi)` is called at most once per update, and only when the frequency view changed. Level-only changes never reach the processor.
  - A view change repaints the whole data rect, like a new frame, so it follows the T-029 frame pacing: when the spectrum is paced (physical plot area above 2.0 Mpx, or 7 curve layers), view changes are applied at most 30 times per second. Hover, clicks and the Fit button still respond within one editor update.
- **Repaint regions.** A frequency-only change repaints the data rect and the frequency gutter; a level-only change repaints the data rect and the dB gutter; a change on both axes repaints both gutters. The header, control bar and meter strip never repaint because of zoom or pan.
- **Curves until the re-mapped frame arrives.** The editor draws the frame it has through the new mapping (its points carry `freqHz`), using the existing T-029 curve path: one bar per physical column for fills, simplified strokes, columns built only under the clip. Where that frame has no points (a margin exposed by a pan or a zoom-out), nothing is drawn for that frame (§15 question 5).
- **Grid during an interaction.** The cached grid image is not rebuilt per view change. While the view differs from the view the cached image was built for:
  - grid lines are drawn as **two `fillRectList` calls**, one in `grid-minor` and one in `grid-major`, each line a rect 1 physical px wide (or high) snapped to physical pixels, built only for lines under the clip;
  - axis labels come from the label-image cache (next item);
  - the data-rect frame is one `fillRectList` of 4 rects in its current colour (`border-subtle`, or `state-frozen` while frozen).
  
  After the view has been unchanged for **200 ms** (`gridIdleRebuildMs`), the cached grid image is rebuilt once for the new view and the single blit returns. The steady-state cost is therefore the same as before T-034.
- **Label-image cache.** Key: the label text, formatted into a fixed `char` buffer, plus its colour. Value: a `juce::Image` at physical scale. 128 entries (`labelCacheEntries`), least recently used evicted, storage preallocated. At most **16 new entries per frame** (`maxNewLabelsPerFrame`); any further missing label that frame is drawn directly with `drawText` (allocations inside JUCE's text layout are excluded by the allocation rule) and cached on a later frame. Axis labels, the zoom-panel range texts and the bin-spacing text all use this cache. A pan mostly moves existing labels and adds one or two at the edges; a zoom changes the label set only when the step changes.
- **Zoom panel.**
  - The chrome (fill, border, radius, the Fit button in its current state, the strip frame and background) is one cached image per panel size, display scale and Fit-button state.
  - The overview curve is two `fillRectList` calls: the body in `zoom.overview-fill` and the top line in `zoom.overview-line`, one rect each per physical column of the strip (at most 268 × 2 = 536 columns at 200 %).
  - The shade is one `fillRectList` of up to 4 rects in `zoom.overview-shade`; the window outline is one of 4 rects; the hover or drag fill is one rect; the ghost outline is 4 rects.
  - There are no paths: rounded corners exist only in the cached chrome.
  - The panel repaints with the data rect. When only its own state changes (Fit hover or press, strip hover, the ghost), only the panel bounds repaint.
- **Box zoom.** One alpha rect plus a 4-rect outline (plus 4 rects for the widened-result outline), painted in `paintOverChildren`. When only the box changes, repaint the union of the old and new box bounds, inflated by 2 px (and the old and new target outlines).
- **Hover elements** are hidden during a pan or a box drag (§5.9.5), so those frames draw no crosshair, markers or card.
- **Budgets** (plan "Budgets"; measured in Release at 48 kHz with the T-012/T-029 harness method, median of 3):
  - whole-editor message thread during a continuous pan or zoom: ≤ 25 % of one core at 1280 × 720 and at 2560 × 1440;
  - editor paint during a pan: p95 ≤ 4 ms at 1280 × 720 and ≤ 8 ms at 2560 × 1440;
  - no per-frame allocation in Investigate's pan and zoom code; text follows the ≤ 20 per frame rule;
  - from a view change to the re-mapped frame being published: p95 ≤ 10 ms, max ≤ 20 ms (analysis side, T-035).

## 13. Coverage checklist

### 13.1 Parameters (roadmap table)

| # | Param ID | Control and location | Display format | Double-click resets to | Settings row | Mockup |
|---|---|---|---|---|---|---|
| 1 | `channelMode` | Segmented, header | Mid, Side, L, R, L+R, L/R, M/S | Mid | yes | yes |
| 2 | `fftSize` | Choice chip BLOCK, bar A | "4096" | 4096 | yes | yes |
| 3 | `overlap` | Choice chip OVERLAP, bar A | "4×" | 4× | yes | yes |
| 4 | `window` | Choice chip WINDOW, bar A | "Hann" … | Hann | yes | yes |
| 5 | `avgTime` | Float chip AVG TIME, bar A | "Off" / "350 ms" / "1.00 s" / "10.0 s" | 1.00 s | yes | yes |
| 6 | `smoothing` | Choice chip SMOOTH, bar A | "Off" / "1/12 oct" | 1/12 oct | yes | yes |
| 7 | `slope` | Float chip SLOPE, bar B | "+4.5 dB/oct" | +4.5 | yes | yes |
| 8 | `secondary` | Choice chip 2ND CURVE, bar B | "Off" / "RT max" / "All-time max" / "All-time avg" | RT max | yes | yes |
| 9 | `freqLow` | Frequency chip, low half, bar C | "20.0 Hz" (3 significant figures) | 20.0 Hz | yes | yes |
| 10 | `freqHigh` | Frequency chip, high half, bar C | "20.0 kHz" | 20.0 kHz | yes | yes |
| 11 | `dbLow` | Level chip, low half, bar C | "−90" | −90 | yes | yes |
| 12 | `dbHigh` | Level chip, high half, bar C | "+6 dB" | +6 | yes | yes |
| 13 | `showSidechain` | Toggle SC, header | on/off state; "On" / "Off" in Settings | On | yes | yes |
| 14 | `meterScale` | Inline choice chip SCALE, meter strip | "dBFS" / "K-20" / "K-14" / "K-12" | dBFS | yes | yes |
| 15 | `rmsWindow` | Inline float chip RMS, meter strip | "300 ms" / "1.50 s" | 300 ms | yes | yes |

### 13.2 MeterSnapshot fields

| Field | Readout | Format / −inf handling | Section |
|---|---|---|---|
| samplePeak[L,R] (max over the last 50 ms) | Bar (peak segment), hold tick, "Peak" row, crest input | Signed 1 decimal; "−inf" ≤ −120 or non-finite | 8.2, 8.3 |
| peakMax[L,R] | "Peak max" row | same | 8.3 |
| rms[L,R] | RMS bar, "RMS" row (scale-referenced), crest input | same; K units in K modes | 8.2, 8.3 |
| truePeak[L,R] (max over the last 50 ms) | "TP" row (held) | same | 8.3 |
| truePeakMax[L,R] | "TP max" row, headroom input | same | 8.3 |
| sampleClipCount[L,R] | "Clips" row cell lamp, bar lamp | Integer, "9999+" cap | 8.2, 8.3 |
| truePeakClipCount[L,R] | "TP clips" row cell lamp, bar lamp | same | 8.2, 8.3 |
| momentary | M | 1 decimal; "−inf" non-finite or < −99.9 | 8.4 |
| shortTerm | S | same | 8.4 |
| integrated | I | 1 decimal; "—" non-finite | 8.4 |
| loudnessRange | LRA | 1 decimal "LU"; "—" while I is "—" | 8.4 |
| maxMomentary | M max | 1 decimal; "—" non-finite | 8.4 |
| maxShortTerm | S max | same | 8.4 |
| correlation (500 ms window) | Bar and value | Signed 2 decimals; "—" in mono | 8.5 |
| balance (dB, negative means L louder) | Balance row | "L 1.2 dB" / "R 0.8 dB" / "C 0.0 dB" / ">60"; "—" in mono | 8.5 |
| isMono | Single bar, "MONO" column, dimmed correlation | layout rule | 8.6 |
| sampleRate | Header text; Nyquist clamp (through `DisplayRange.h`); Block tooltip | "48 kHz" / "44.1 kHz" | 8.8, 5.4 |
| blockCounter | No readout. Staleness detection: unchanged for 250 ms means the live levels fall to −inf and the statistics hold. | n/a (compared with `!=`, wrap-safe) | 8.7 |
| derived: headroom | "Headroom" row | Unsigned 1 decimal, negative in red, "—" | 8.3 |
| derived: crest | "Crest" row | Unsigned 1 decimal, "—" | 8.3 |

### 13.3 SpectrumFrame and view state

| Item | Where | Section |
|---|---|---|
| Primary curve | Primary A style | 5.1 |
| Second primary curve (dual) and `isDual` | Primary B style, legend names | 5.1, 5.2 |
| Secondary curves (one per primary; two in dual modes) and `hasSecondary` | Secondary A / B styles | 5.1 |
| Sidechain curve and `hasSidechain` | Silhouette style | 5.1 |
| Point frequencies | x mapping, hover | 5.3, 5.5 |
| sampleRate | Nyquist clamp through `source/dsp/DisplayRange.h` | 5.4 |
| Secondary reset on a `channelMode`, `fftSize`, `window` or sample-rate change | DSP-side; the editor issues no reset and shows no toast | 5.7 |
| Freeze (view flag) | Header toggle, badge, auto-unfreeze on a `channelMode` change | 5.7 |
| Reset statistics | Meter-strip button | 6 |
| Reset spectrum curves | Header button and left-click | 5.6, 6 |
| Right-click copy | Toast, clipboard | 5.6 |
| `ViewState.editorWidth/Height` | Resizing | 2 |
| `ViewState.metersCollapsed` | Meters toggle | 4.5 |
| About view (version, font notices, OFL texts, JUCE and pffft notices) | Settings title bar, "About" | 6.2 |
| View window (`viewFreqLow`, `viewFreqHigh`, `viewDbLow`, `viewDbHigh`), T-034 | Editor only. Not a parameter, **not in `ViewState`, not persisted**: the editor always opens at Fit, and `setAnalysisActive (false)` returns it to Fit. Follows the per-axis home-range rule. | 5.9.1 |
| `setSpectrumView (fLo, fHi)` | At most once per editor update, on frequency changes only | 5.9.3, 12.1 |
| `setSpectrumFrozen (bool)` and the freeze epoch | Called on every freeze change, including auto-unfreeze; frames with a stale epoch are dropped | 5.7 |
| Frame view range (`viewFreqLow`, `viewFreqHigh` of the frame) | Mapping the current frame until the re-mapped one arrives | 12.1 |
| Frame overview (`overviewFreqHz[256]`, `overviewDb[256]`, primary A over the home range) | Overview strip curve | 5.9.6 |
| Bin spacing of the displayed data | Bin-spacing row of the zoom panel | 5.9.6, 15 |
| Zoom panel, box zoom, cursors | Indicator, Fit button, overview strip, rubber band, custom cursors | 5.9.4 to 5.9.7 |

## 14. IP check against T-001 §8

| Rule | How this design complies |
|---|---|
| No "SPAN" or "Voxengo" name | The product text says "Investigate" only. The tooltips and this spec's user-facing copy are original. |
| No Voxengo logos, icons or screenshots | Every SVG was drawn from scratch for this task. The wordmark is custom monoline geometry, and the icons are generic metaphors (snowflake, circular arrow, sliders, panel). |
| No SPAN colour scheme or skin | Blue-black ink with aqua, amber and lavender curves, and teal-to-red meters. The spectrum fill is aqua, not olive or yellow-green, and there is no grey chrome. The controls are flat chips, not SPAN's buttons or knobs. |
| No copied layout proportions | Statistics live in a right-hand column rather than a bottom band. Settings live in a bottom bar and a popover rather than a title bar and a modal editor. The window is 12:7 rather than about 5:2. |
| No manual text | Every tooltip and label is newly written. The window and curve names (Hann, Blackman, RT max, …) are generic terms (T-001 §8). |

## 15. Open questions

**Resolved in the first review round** (roadmap Decisions 2026-09-28), now built into the spec:
- Font licenses: approved by T-022 (§3.2).
- Snapshot peaks are 50 ms maxima (§8.2).
- `blockCounter` is part of the snapshot (§8.7).
- The effective-range formulas are shared through `source/dsp/DisplayRange.h` (§5.4).
- There are two secondary curves in the dual modes (§5.1).
- The secondary-curve reset on a mode or resolution change happens at the DSP level; the editor only auto-unfreezes (§5.7).
- The note-name examples are 452.6 Hz for "A4 +49c" and 452.9 Hz for "A#4 −50c" (§5.5).
- The correlation window is 500 ms (§8.5).

**Closed on 2026-10-01** (T-011 task-file correction, roadmap Decision 2026-10-01; folded into this spec by T-034). The numbers are kept so that older references stay valid:
1. **JUCE notice line:** JUCE 8's open-source option is AGPLv3; T-021 corrected `docs/dependencies.md`. The notice line itself stays a placeholder until M5 (T-019).
2. **pffft license text in binary data:** done by T-021, as `BinaryData::LICENSEpffft_txt` from `third_party/fonts/LICENSE-pffft.txt`.
3. **Font files:** done by T-021, in `third_party/fonts/` and compiled into `InvestigateAssets` (§3.2, §11), not in `assets/ui/fonts/`. The Segoe UI and Consolas fallback and the "License text missing" placeholder are defensive only.

**Open:**
4. **Other notices (researcher):** the VST3 build links the Steinberg VST3 SDK (MIT from 3.8, roadmap Decisions). Its MIT notice, and possibly a Steinberg "VST" trademark line, may also need to appear in the About view and in `THIRD-PARTY-NOTICES.txt`. If so, they are added as further entries under "Software" in §6.2, in the same style.
5. **Curve margin while panning (dsp-engineer, T-035; T-034):** a frame carries curves only over the view it was mapped for. During a continuous pan, or right after a zoom-out step, the editor has no points for the newly exposed margin until the re-mapped frame arrives (up to one editor frame; p95 ≤ 10 ms). In a fast pan this shows as an empty strip flickering at the leading edge. **Recommended answer:** the mapper also maps a margin of 25 % of the visible log-frequency span on each side (clamped to the home range) at the same point density, so at most 1.5 × 2048 = 3072 points per curve, and the frame's view range records the mapped range. 25 % covers one zoom-out notch (12.5 % per side) and a pan of a quarter of the view per frame. If this is declined, §12.1 applies as written and the margin stays empty for that frame.
6. **Bin spacing in the frame (dsp-engineer, T-035; T-034):** the bin-spacing row (§5.9.6) needs the bin spacing of the data on screen. That differs from `sampleRate / fftSize` while frozen, and for one frame after a Block change. **Recommended answer:** add `float binHz` to `SpectrumFrame`; the freeze snapshot already stores it (plan, "Changes per module"). Until it exists, the editor uses `sampleRate / fftSize`, captured at the moment of freezing while frozen.

## 16. Change log

| Date | Task | Change |
|---|---|---|
| 2026-09-28 | T-010 | First version. The user approved `mockup.html` without changes; layout and visual design frozen for T-011 to T-013. |
| 2026-10-03 | T-034 | **Spectrum zoom and pan (M3b).** New §5.9: view model, view arithmetic, gestures G1 to G21 with exact numbers, cursors, hover behaviour during gestures, zoom panel (indicator, Fit button, bin-spacing row, overview strip) with geometry at four sizes, collision rules, deep-zoom grid rules with worked examples, Fit and no-op cases, changes from the plan, mockup simplifications. Updated: §4.1 (gutters as wheel targets, panel position); §5.3 (view mapping, cache rule); §5.5 (view-based frequency formula, 4-figure zoom precision, hover-card candidates around the panel, new hint-line texts and fit rule); §5.6 (interaction table replaced, clipboard precision); §5.7 (freeze plus zoom, freeze epoch); §6 (Fit view action, deferred click reset); §7.2 (no zoom keys, Esc during a box drag); §10 (new §10.2a contrast tables, names, hit targets); §11 (three cursor SVGs); §12.1 (rendering and budgets for zoom and pan); §13.3 (view state not persisted, new frame fields); §15 (questions 5 and 6). `tokens.json` 1.1.0 adds the groups `zoom`, `zoomLayout`, `zoomInteraction`, `deepGrid` and `cursor`. New assets: `cursor-hand-open.svg`, `cursor-hand-grab.svg`, `cursor-zoom-box.svg`. `mockup.html` implements every gesture and no longer loads web fonts, so it opens from the file system with no network access. |
| 2026-10-03 | T-034 (fold-in) | Two stale points that T-011 had overridden in its task file (roadmap Decision 2026-10-01) are folded in: the font files and license texts live in `third_party/fonts/` and the `InvestigateAssets` binary-data target, not in `assets/ui/fonts/` (§3.2, §6.2, §11, and the font file paths in `tokens.json`); and the old §15 open questions 1 to 3 are closed (§15). |
