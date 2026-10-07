# Investigate: UI specification

- Task: T-010. Author: ui-ux-designer. Date: 2026-09-28. Status: final. The user approved `mockup.html` without changes on 2026-09-28. The layout and visual design are frozen for T-011 to T-013. The final pass reconciled the spec with the roadmap Decisions dated 2026-09-28, finalised the fonts (§3.2) and added the About view (§6.2).
- Addendum T-034 (2026-10-03, milestone M3b): spectrum zoom and pan, new §5.9, with updates to §4.1, §5.3, §5.5 to §5.7, §6, §7.2, §10, §11, §12, §13.3 and §15. The updated mockup awaits the user's approval; T-037 does not start before it. All changes are listed in §16.
- Addendum T-045 (2026-10-04, milestone M3c): every M3c change is in the new §17, with marked "M3c" notes in the sections it supersedes, a new §6.3 control inventory, §10.4, §12.2 and §13.4. The M3b behaviour that T-037 implements is unchanged. The updated mockup awaits the user's approval; T-054 to T-058 do not start before it. Changes are listed in §16 and §17.21.
- Addendum T-065 (2026-10-07, milestones M3d and M3c): the user's M3 review and the four spectrum slots are in the new §18. §18.1 (M3d) is self-contained for T-066; §18.2 onwards replaces the superseded parts of §17 (list in §18.14). The combined mockup (T-045 plus T-065) awaits the user's approval before T-054. Changes are listed in §16.
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
- **M3c (T-045):** the region rectangles are unchanged. The header contents change (§17.2), and the meter strip's header grows to two rows, moving its content top from strip y 52 to 86 (§17.10).

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

**M3c:** this table describes the header up to M3b. From M3c (T-055) the header is §17.2: Channel, Underlay, Presets, a Sidechain 1 and 2 pair and Refs are added, with a compact mode below 1174 px.

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

**M3c:** from M3c (T-057) the strip header has two rows and the sections can be hidden one by one. The layout above is replaced by §17.10.1, §17.10.6 and the resolved tables in §17.17.3.

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
- **M3c:** from M3c (T-054, T-055) the curves use colour roles, the dual modes become the underlay, and Sidechain 2 and four reference curves are added. §17.4 replaces the table above (the M3b styles and colours are its defaults).

### 5.2 Legend

- Position: top-left of the data rect at (data.x + 8, data.y + 8). It is 20 high, with fill `bg-plot` at 85 %, radius 4, padding 3 × 8, and 10 px between entries. Each entry is a 16 × 10 style swatch plus a `body-sm` name in `text-secondary`.
- Entries appear in this order, each only when the matching curve is drawn: primary A name; primary B name; the secondary name (with both A and B swatches stacked in dual modes); "Sidechain".
- Names by mode:
  - single modes: "Mid", "Side", "Left", "Right", "L+R";
  - L/R dual: "L", "R";
  - M/S dual: "M", "S".
- Secondary names: "RT max", "All-time max", "All-time avg".
- When the pointer is within 16 px of the legend box, the legend drops to 25 % opacity so it never hides the data under the cursor.
- **M3c:** entries gain the input labels, Sidechain 2, the references and the ALIGN tag, and the legend flows into rows (§17.5).

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
  - At Fit: "Scroll: zoom · Alt+drag: box zoom · Click: reset max/avg · Right-click: copy frequency".
  - While zoomed: "Drag: pan · Double-click: fit · Scroll: zoom · Click: reset max/avg · Right-click: copy frequency".
  - **Fit rule:** the measured width (text plus 12 px padding) must be at most data.w − 16, which is 544 px at the minimum size 920 × 540. Estimated with Inter 500 at 10 px, the two texts are at most about 431 px and 474 px wide (the Fit text was estimated with the longer "Ctrl"; "Alt" is narrower), so both fit at the minimum size. If the measured width is larger (for example with a fallback font), use the short forms "Scroll: zoom · Alt+drag: box · Click: reset · Right-click: copy" and "Drag: pan · Double-click: fit · Scroll: zoom · Click: reset · Right-click: copy".
  - Level zoom (Alt+scroll, or scroll over the dB labels) is not in the hint, because it does not fit at the minimum size. It is in the zoom panel's tooltip (§5.9.6), the accessible description (§10.3) and the M3b user checklist.
  - The reserved Ctrl gestures (§5.9.3 G22, G23) are not in the hint until M3c gives them a function.
  - **M3c:** while the band solo previews or plays, the hint shows the band texts of §17.9.6 instead. The two texts above are unchanged.
- **M3c additions to the card and formats** (T-055, T-056): the band block at the top of the card during the band solo (§17.9.4); rows with input labels, Sidechain 2 and references (§17.5); 2 decimals below 10 Hz (3 when zoomed) and note names below C0 (§17.7). Nothing else in this section changes.
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

This table replaces the T-010 table (T-034; modifiers remapped by T-042). The pointer actions are distinguished by button, modifier and movement, so none of them conflicts with hover. "G1" to "G23" refer to the exact rules in §5.9.3; the numbers are repeated here for convenience, and every G number appears in exactly one row of each table.

| Input | Action | Feedback |
|---|---|---|
| Hover / move in the data rect | Hover readout (§5.5) over the view mapping | Crosshair, markers, card, hint line |
| Wheel, vertical, in the data rect, without Ctrl or Alt (G1, G2) | Zooms frequency around the pointer: one notch scales the visible span by 0.8 (in) or 1.25 (out); Shift gives 0.95 or 1 / 0.95. Limits: 1/6 octave and the home range. Always consumed. | Curves, grid and zoom panel update on the next editor frame |
| Alt+wheel in the data rect (G3) | Zooms level around the pointer, same factors (Alt+Shift for fine steps), limits 6 dB and the home range | as above |
| Wheel over the frequency labels or the dB labels (G4, G5) | Zooms that axis around the pointer, no modifier needed (any modifier, including Ctrl or Alt, is ignored there) | as above |
| Horizontal wheel or sideways two-finger swipe, without Ctrl (G6) | Pans frequency by 10 % of the visible span per notch, only while frequency is zoomed | as above |
| Left drag (> 4 px) without Ctrl or Alt, or middle drag (G7 to G9) | Pans both axes while zoomed; does nothing at Fit | Grabbing-hand cursor; hover readout hidden while dragging |
| Left click, ≤ 4 px of movement, no Ctrl or Alt (G11) | After a **250 ms defer**, `requestSpectrumReset()`, which clears the all-time max and all-time average. A double-click inside the defer cancels it. | Only when `secondary` is All-time max or All-time avg: a toast "Max/avg curves reset" centred at the top of the data rect (y = data.y + 8, or data.y + 36 while the FROZEN badge shows), for 900 ms, shifted left clear of the zoom panel (§5.9.7). With Off or RT max nothing visible changes, so no toast appears. |
| Double-click, without Ctrl or Alt (G12) | Fit: the view returns to the home range. No spectrum reset. | Zoom panel disappears |
| Alt+left drag (G13 to G15) | Box zoom: on release both axes zoom to the box. A box under 4 px in either dimension cancels; so does Esc when the editor has keyboard focus. | Box-zoom cursor and rubber band |
| Alt+left click or Alt+double-click, middle click (G10, G14) | Nothing. An Alt+click on the data rect is not a reset (the Alt+click reset of §7.1 applies to chips only). | |
| Right button down (G16) | Copy the frequency under the pointer to the system clipboard (`juce::SystemClipboard::copyTextToClipboard`). Ignored during a pan or box drag. | Toast "Copied 1250 Hz" at (px + 12, py − 28), clamped inside the data rect and clear of the zoom panel (§5.9.7), for 1200 ms |
| Zoom panel: Fit button, overview strip (G17 to G21) | Fit; pan by dragging the window; press beside the window to centre the view there; the wheel passes through to the data rect | §5.9.6 |
| Ctrl+wheel in the data rect, including a Windows touchpad pinch (G22) | **Reserved for the band solo width (M3c).** Until M3c: nothing; the event is consumed. | None |
| Ctrl+left press, click, double-click or drag in the data rect (G23) | **Reserved for the band solo (M3c).** Until M3c: nothing; never pans, zooms, box-zooms, fits or starts the deferred reset. | None; the crosshair cursor and hover readout continue |
| **M3c** (T-056), Ctrl held without a button in the data rect (G24) | Band preview at the pointer, no audio change | Preview tint, dashed band edges, "BAND" block in the hover card, preview hint, band cursor (§17.9) |
| **M3c** (T-056), Ctrl+wheel in the data rect or over the zoom panel (G22-M3c, replaces G22 from M3c) | Steps `soloWidth` along the width ladder; Ctrl+Shift+wheel in fine steps | The band, tint and readout update (§17.9.5) |
| **M3c** (T-056), Ctrl+left press and hold in the data rect (G23-M3c, replaces G23 from M3c) | Audible band solo; moving sweeps it; ends on release, Ctrl up, leaving the editor or focus loss | Solo tint, solid edges, "SOLO" block, solo cursor (§17.9) |

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
- **Auto-unfreeze:** a `channelMode` change from any source (UI, host automation or state load) turns freeze off before the next frame. **M3c:** a change of the effective channel and underlay pair does (§17.3).
- **Secondary-curve reset is DSP-side:** `SpectrumAnalyser` clears its secondary accumulators whenever `channelMode`, `fftSize`, `window` or the sample rate changes (roadmap Decision 2026-09-28). The editor does **not** call `requestSpectrumReset()` on any of these changes and shows no toast for them. The first frame after the change already carries the cleared curves. The mockup's `resetCurves(false)` call on a mode change only simulates this DSP behaviour. It is not an editor call to port.
- Left-click-to-reset still works while frozen, after the same 250 ms defer (§5.9.3 G11). The engine resets, and the effect becomes visible after unfreezing.

### 5.8 No frames

- **Before the first frame:** only the grid is drawn.
- **Audio stopped or silent:** frames that keep arriving fall according to `avgTime`. If no new frame arrives (the host stopped calling `processBlock`), the last frame stays on screen. This is intended, because the spectrum is a snapshot tool.
- **Sidechain active but silent:** the silhouette falls to the floor.
- **Sidechain bus not active:** no silhouette and no legend entry. The SC button shows its "no input" state (§9). **M3c:** the same per sidechain, on the Sidechain pair's segments (§17.6).

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
| G1 | Wheel, vertical, without Ctrl or Alt | Data rect, and the zoom panel (which passes the wheel through to the data rect at the same position) | Zooms frequency, anchored at the pointer's x | `k = 0.8^n`; limits 1/6 octave and the home range. Always consumed, also when nothing changes at a limit. |
| G2 | Shift+wheel, without Ctrl or Alt | Data rect | Fine frequency zoom | `k = 0.95^n`. If the platform reports Shift+wheel as horizontal (deltaY = 0, deltaX ≠ 0), deltaX is used as the vertical amount. Shift+wheel never pans. |
| G3 | Alt+wheel, Alt+Shift+wheel, without Ctrl | Data rect | Zooms level, anchored at the pointer's y; Shift for fine steps | Same factors; limits 6 dB and the home range. Always consumed. The Shift-as-horizontal rule of G2 applies here too. With Alt held, a vertical-dominant event zooms level; a horizontal-dominant event pans as G6. |
| G4 | Wheel, any modifier | Frequency gutter | Zooms frequency, anchored at the pointer's x | Shift for fine steps; Ctrl and Alt are ignored, because the gutter chooses the axis. A touchpad pinch over the gutter therefore zooms frequency. |
| G5 | Wheel, any modifier | dB gutter | Zooms level, anchored at the pointer's y, with no modifier needed | Shift for fine steps; Ctrl and Alt are ignored. This is the modifier-free way to zoom level. |
| G6 | Horizontal wheel, or a sideways two-finger swipe, without Ctrl | Data rect, frequency gutter (in the gutter, Ctrl is ignored) | Pans frequency | 10 % of the visible span per notch. Consumed only while frequency is zoomed; at frequency Fit it is passed to the parent. An event whose `|deltaX|` exceeds `|deltaY|` pans; otherwise it zooms (G1 or G3), so a diagonal swipe never does both. In the data rect, any wheel event with Ctrl held is G22 instead. |
| G7 | Left press without Ctrl or Alt, then more than 4 px of movement (Euclidean distance from the press point) | Data rect, while zoomed on either axis | Pans both axes like a hand tool | 1:1 (§5.9.2). The pan includes the first 4 px, so the content does not jump at the threshold. An axis at Fit cannot move. Wheel events during the pan are applied, and the pan continues from the new view. |
| G8 | Left drag at Fit (both axes) | Data rect | Nothing: no pan and no reset | The crosshair cursor stays and the hover readout continues (the pre-T-034 behaviour that protects against accidental resets) |
| G9 | Middle press and drag (> 4 px) | Data rect | The same as G7, or G8 at Fit | |
| G10 | Middle click (≤ 4 px) | Data rect | Nothing | |
| G11 | Left click: press and release within 4 px, without Ctrl or Alt at the press | Data rect | After **250 ms**, `requestSpectrumReset()` and the reset toast (§5.6) | Started on the release. Cancelled by a double-click (G12) that arrives before it fires. A release that ends a pan, a box or a no-op drag never starts it. |
| G12 | Double-click: JUCE `mouseDoubleClick` (the second press within the OS double-click time and distance), without Ctrl or Alt at the second press | Data rect | **Fit** both axes, at the second press | Cancels a pending G11 reset. The release that completes the double-click starts no reset. If the second click comes after the 250 ms defer, the first click's reset has already run; this is the same edge case as the chips (§7.1). |
| G13 | Alt+left press (without Ctrl), then drag | Data rect | **Box zoom:** a rubber band from the press point to the pointer, clamped to the data rect. On release both axes zoom to it (§5.9.2). | The mode is fixed at the press: releasing Alt, or pressing Ctrl, during the drag keeps the box. A box narrower or lower than **4 px** at the release cancels (no zoom, no reset). An axis below its minimum span is widened, and the widened result is previewed (§5.9.6, box style). |
| G14 | Alt+left click (no movement), and Alt+double-click | Data rect | Nothing: no reset, no Fit | So a cancelled box never resets curves. This is not the Alt+click reset of §7.1, which applies to chips only. |
| G15 | Esc during a box drag | Editor with keyboard focus | Cancels the box | SpectrumView never takes keyboard focus (§7.2), so Esc works only when the host has given the editor focus and a control in it holds the focus. Otherwise, dragging back to a box under 4 px cancels. |
| G16 | Right press | Data rect | Copies the frequency under the pointer (view mapping, §5.6) | Ignored while a pan or a box drag is active |
| G17 | Fit button, click | Zoom panel | **Fit** both axes, on release inside the button | |
| G18 | Press on the view window, then drag | Overview strip | Pans both axes | The pointer's offset from the window centre is kept; 1:1 in strip coordinates |
| G19 | Press outside the view window | Overview strip | The view centres on that point at once (both axes, spans kept, clamped); continuing to drag pans as in G18 | The press acts on mouse-down, so there is no click or double-click ambiguity in the strip |
| G20 | Double-click, right-click, middle press | Zoom panel | Nothing (a double-click on the strip is two G19 presses) | |
| G21 | Wheel | Zoom panel | Passes through to the data rect at the same position (G1 to G3, G6, G22) | The panel component does not override `mouseWheelMove`, so JUCE forwards the event to SpectrumView |
| G22 | Ctrl+wheel, with any other modifier, vertical or horizontal (T-042) | Data rect, and the zoom panel (pass-through) | **Reserved for the band-solo width (M3c).** Until M3c: nothing | The event is consumed (never passed to the parent), changes no view, and causes no repaint. A Windows precision-touchpad pinch arrives as Ctrl+wheel, so until M3c a pinch does nothing over the data rect; from M3c it changes the solo width (M3c plan Risk 18). Over the gutters a pinch zooms that axis (G4, G5). |
| G23 | Ctrl+left press (with any other modifier), then click, double-click, hold or drag (T-042) | Data rect | **Reserved for the band solo (M3c).** Until M3c: nothing | Ctrl is read at the press, and the mode is fixed there: releasing Ctrl later does not start a pan or box, and the release never starts the G11 reset. A Ctrl+double-click does not fit. Pressing Ctrl during a pan or box that began without Ctrl changes nothing. The cursor stays the crosshair and the hover readout continues, as at G8. Middle (G9, G10) and right (G16) presses keep their behaviour with Ctrl held. |
| G22-M3c | **M3c only (T-056); T-037 implements G22 above.** Ctrl+wheel, with any other modifier, vertical or horizontal | Data rect, and the zoom panel (pass-through) | Steps the band-solo width (`soloWidth`) | Ladder steps per whole accumulated notch; Ctrl+Shift for continuous steps (§17.9.5). Works in Preview and Solo. Consumed; never zooms, pans or repaints the view. |
| G23-M3c | **M3c only (T-056); T-037 implements G23 above.** Ctrl+left press (with any other modifier), then hold or drag | Data rect, not over the zoom panel | **Audible band solo** at the pointer; dragging sweeps the centre | Mode fixed at the press, as G23: never pans, boxes, fits or resets. Ends on button up, Ctrl up, the pointer leaving the editor, focus loss, hide or close (§17.9.1). A Ctrl+double-click starts a new solo at the second press and does not fit. Middle and right presses keep their behaviour; a right press during a solo does nothing. |
| G24 | **M3c only (T-056).** Ctrl held with no button down | Data rect, not over the zoom panel | **Band preview** at the pointer, no audio | Ctrl is polled on every editor update (§17.9.2). No view change, no reset. |

**Modifier precedence in the data rect:** Ctrl is tested first (G22, G23), then Alt (G3, G13, G14), then no modifier. Shift only selects fine steps.

**Coalescing:** the input handlers only record the requested view. The editor applies it once per editor update (§12), so a burst of wheel events or drag moves produces at most one repaint and one `setSpectrumView` per update.

**Alt on Windows:** whether releasing Alt after an Alt gesture activates a host's menu bar is under research in T-044 (M3c plan Risk 17); this section does not change until that result is in.

#### 5.9.4 Cursors

| State | Cursor | Source |
|---|---|---|
| Data rect, no button down, Alt not held, or Ctrl held (zoomed or not) | Crosshair | `juce::MouseCursor::CrosshairCursor` |
| Data rect, Alt held without Ctrl, no button down | Box zoom | `assets/ui/cursor-zoom-box.svg`, hotspot (7.5, 7.5) |
| Ctrl+left press or drag (G23, reserved until M3c) | Crosshair | |
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
| **M3c:** Ctrl held, no button, in the data rect (G24) | Band | `assets/ui/cursor-band.svg`, hotspot (12, 12); replaces the crosshair of the rows above from M3c |
| **M3c:** band solo (G23-M3c) | Band, solo | `assets/ui/cursor-band-solo.svg`, hotspot (12, 12) |

- **Alt and Ctrl state:** take it from the modifiers of the latest mouse event, and from `modifierKeysChanged` when the host delivers it. Some hosts do not deliver key changes to an unfocused plugin window; then the cursor updates on the next mouse move.
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
| **M3c:** band preview (G24) or solo (G23-M3c) | shown | shown | shown, with the band block | normal rule | band text (§17.9.6) |

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
- Rows 1 and 2: "Visible range. Scroll to zoom frequency; Alt+scroll, or scroll over the dB labels, to zoom level. Drag the spectrum to pan."
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
| 5 | Not specified | Middle click, Alt+click (box modifier; T-042 replaced Ctrl) and a right-click during a drag do nothing; a release that ends a pan, box or no-op drag never resets; the release that completes a double-click does not start a reset | Clarifications, so no gesture can reset curves by accident. |
| 6 | Not specified | The hover card and clipboard use 4 significant figures while the visible span is below 1 octave (§5.5, §5.6) | At deep zoom, 3 figures are coarser than the axis labels and the pixel resolution. The unzoomed formats, and the T-012 tests that pin them, are unchanged. |
| 7 | An indicator and an overview strip | Both in one **zoom panel** at the top-right of the data rect | One rectangle to keep clear of the legend, badge, toasts, hint and hover card, at every size. |
| 8 | One view change per editor update | The same, and view-change repaints follow the T-029 frame pacing (§12) | A view change repaints the whole data rect, like a new frame, so it must obey the same pacing to stay inside the CPU budget. |
| 9 | Esc cancels a box when the editor has focus | The same; SpectrumView never takes keyboard focus itself | Taking focus on a click could stop the host's Space (transport) key from working in some hosts. |
| 10 | Not specified | The wheel over the zoom panel passes through to the data rect | The panel covers part of the data rect; the wheel should not stop working there. |
| 11 | Not specified | The box-zoom preview outlines the widened result when an axis is below its minimum span | The user sees in advance what the release will produce. |
| 12 | Level zoom and box zoom on the Control modifier; a pinch zooms level (plan risk Z4) | Level zoom on **Alt+wheel** (G3), box zoom on **Alt+left-drag** (G13). Ctrl+wheel and Ctrl+left press over the data rect are reserved and inert until M3c (G22, G23); a pinch does nothing over the data rect. | User decision of 2026-10-03 (T-042): Ctrl is kept free for the M3c band solo. The architect updates the plan's interaction table to match, after which this row is no longer a deviation. |
| — | Minimum spans 1/6 octave and 6 dB; 4 px drag threshold; 250 ms click defer; 4 px box cancel | **Unchanged** | Checked against the grid rules: the 1/6-octave span at 560 px still has 7 to 13 labels (§5.9.8, E and F), and 6 dB at 404 px has 13 dB lines 33.7 px apart. |

#### 5.9.11 What the mockup simplifies

- **Engine:** the mockup's synthetic analyser keeps 4096 log-spaced points from 10 Hz to Nyquist as its stand-in for the engine's bin-domain spectra, and draws the ones in view (at most 2 vertices per physical column, §5.1). Zooming therefore shows more detail, as the real re-mapping does. The real engine uses linear FFT bins, so below the bin-spacing hint threshold the plugin shows straight segments between bins, while the mockup stays smooth.
- **Wheel units:** the browser reports 100 px (or 3 lines) per notch rather than JUCE's 0.234375. The mockup normalises to notches the same way (§5.9.2).
- **Double-click:** the mockup uses the browser's click count on the second press, which follows the OS double-click time, the same as JUCE's `mouseDoubleClick`.
- **Alt and Ctrl in a browser (T-042):** the mockup cancels the browser's default action for Alt+wheel, Ctrl+wheel (page zoom) and an Alt key release while the pointer is over the plot, so the browser's own zoom and menu bar stay out of the preview. Whether the browser honours this varies; it says nothing about how DAW hosts treat Alt (§5.9.3, T-044).
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
- **M3c:** from M3c (T-055, T-057, T-054) the popover has four tabs and is 560 × 400 (§17.16).

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

### 6.3 M3c control inventory (T-045)

> **Rows superseded by §18 (T-065, spectrum slots):** `channelMode` (header Channel control), `underlay` (dropped), `showSidechain` and `showSidechain2` (now slot 3 and slot 4 Visible), `inputLabel0` to `2` (now `slotName1` to `4`), and the eight role colours (now slot colours plus the reference roles). The other rows stay valid.

Every new or changed parameter, `ViewState` property and global preference of the M3c plan. "Wheel" is one notch; "Fine" means Shift. Chip behaviours (250 ms click defer, double-click and Alt+click reset, external-change dot, error flash) are those of §7.1 unless a row says otherwise. Stores: P = APVTS parameter (session and settings presets), V = `ViewState` (session only), G = `GlobalSettings` (machine-wide).

| Item (store) | Control and location | Size | Label | Display format | Default | Drag | Wheel (Fine) | Text entry |
|---|---|---|---|---|---|---|---|---|
| `channelMode` (P) | Channel segmented, header (§17.2); choice field, Settings > Analysis | Segments 28 to 52 × 24; field 128 × 24 | none; "Channel" | "Mid", "Side", "L", "R", "Stereo" (field: Mid, Side, Left, Right, Stereo); legacy L/R and M/S shown as the pair (§17.3) | Mid | Field: 1 step per 20 px (60) | ±1 segment, clamped | (menu in the field) |
| `underlay` (P) | Inline choice chip, header; field, Settings > Analysis | 124 × 28; 128 × 24 | UNDERLAY; "Underlay" | "Off", "Mid", "Side", "Left", "Right", "Stereo"; `state-warn` when effectively Off | Off | 1 step per 20 px (60) | ±1 | (menu) |
| `freqLow` (P, range now 1 to 1000 Hz) | Frequency chip low half (as §6), Settings field | as §6 | FREQUENCY | §5.5 format; below 10 Hz 2 decimals: "1.00" + "Hz" | 20.0 Hz | 240 px over the log range (skew centre 31.6 Hz) | × 2^(±1/6) (× 2^(±1/24)), 3 significant figures | "1", "1 Hz", "2.5", "0.02k" |
| `align0dB` (P) | On/Off field, Settings > Analysis ("LEVEL AND SOLO"); legend tag while on | 128 × 24 | "Align 0 dB" | "On" / "Off" | Off | none | up = On, down = Off | (none) |
| `soloWidth` (P, not automatable) | Float field, Settings > Analysis; Ctrl+wheel over the plot (§17.9.5) | 128 × 24 | "Solo width" | Ladder labels "1/12 oct" … "4 oct", else 2 decimals "0.42 oct" | 1 oct | 240 px over the normalised range (skew centre 0.58 oct) | one ladder step (× 2^(±1/8)) | "1/3", "1/3 oct", "0.5", "2 oct" |
| `showSidechain` (P, name "Sidechain 1 trace") | Segment "1" of the Sidechain pair, header; field, Settings > Inputs | 28 × 24; 128 × 24 | "1"; "Sidechain 1 trace" | On: accent state; "On" / "Off" | On | none | up = On, down = Off | (none) |
| `showSidechain2` (P) | Segment "2" of the Sidechain pair; field, Settings > Inputs | 28 × 24; 128 × 24 | "2"; "Sidechain 2 trace" | as above | On | none | up = On, down = Off | (none) |
| `meterScale` (P, 14 modes) | Inline choice chip, strip row 1; field, Settings > Meters | 128 × 28; 128 × 24 | METERING; "Metering" | Short names of §17.10.2 ("dBFS", "dBFS 30", "K-20 cal", "LUFS M", "LU S"); menu grouped with separators | dBFS | 1 step per 20 px (60), in menu order | ±1 in menu order | (menu) |
| `meterStyle` (P) | Segmented, strip row 1; field, Settings > Meters | 92 × 28 (segments 36 and 52 × 24); 128 × 24 | none ("Bars", "Density"); "Style" | Selected segment; `state-warn` "Density" in loudness modes | Bars | Field: 1 step per 20 px | ±1 | (menu in the field) |
| `meterIntegration` (P) | Inline float chip, strip row 2; field, Settings > Meters | 160 × 28; 128 × 24 | INTEGRATION; "Integration" | 3 significant figures: "0.25 ms" (2 decimals below 1 ms), "1.00 ms", "35.0 ms", "350 ms", "1.50 s"; `state-warn` in the cal and loudness modes | 1.00 ms | 240 px normalised (skew centre 30 ms), snap 3 significant figures | ±0.02 normalised (±0.004), at least one snap step | "1", "1 ms", "0.5", "2 s" |
| `meterRelease` (P) | Float field, Settings > Meters | 128 × 24 | "Release" | as Integration: "10.0 ms" … "750 ms" … "5.00 s"; `state-warn` in the cal and loudness modes | 750 ms | 240 px normalised (skew centre 500 ms), snap 3 significant figures | ±0.02 (±0.004) | "750", "1.2 s" |
| `meterPeakHold` (P) | Float field, Settings > Meters | 128 × 24 | "Peak hold" | 0: "Off"; else "350 ms", "1.00 s", "10.0 s" | 1.00 s | 240 px normalised (skew centre 1000 ms), snap 10 ms below 1 s and 100 ms above | ±0.02 (±0.004) | "off", "0", "1500", "1.5 s" |
| `rmsWindow` (P, moved) | Float field, Settings > Meters (no longer in the strip) | 128 × 24 | "RMS window" | as §6 | 300 ms | as §6 | as §6 | as §6 |
| `meterSections` (V) | Four icon toggles, strip row 2; four On/Off fields, Settings > Meters | 24 × 24 each; 128 × 24 | icons; "Level bars", "Statistics", "Loudness", "Correlation" | Shown: accent; hidden: slashed icon | all shown | none | none (toggles); fields: up = On | (none). No double-click reset on the strip toggles |
| `inputLabel0`, `1`, `2` (V) | Text fields, Settings > Inputs | 160 × 24 | "Main input", "Sidechain 1", "Sidechain 2" | The text, at most 16 characters | "Main", "SC 1", "SC 2" | none | none | Any printable text; empty restores the default |
| Reference slot k: name, visibility, offset (V, `ReferenceSlots`) | Curves panel, References tab (§17.11) | Name 128 × 24; eye 24 × 24; offset chip 76 × 24 | "NAME", "SHOW", "OFFSET" | Name text; eye icon; offset signed 1 decimal "+1.5 dB", "0.0 dB" | "Ref k", hidden (empty), 0.0 dB | Offset: 240 px over −40 … +40 (fine ×0.1) | Offset ±0.5 dB (±0.1) | Offset: "−3", "+2.5 dB" |
| Role colours ×8 (G) | Colours tab: role list, hue ring and square, hex field (§17.14) | ring 176; field 104 × 24 | role names | "#62D6F0" | §17.4 defaults | ring and square, Shift ×0.25 | hue ±2° (±0.5°), value ±0.02 (±0.005) | "#RRGGBB", "RRGGBB", "#RGB" |
| `frameRate` (G) | Float field, Settings > Display | 128 × 24 | "Frame rate" | "60 fps"; `state-warn` above the display rate | 60 fps | 240 px over 30 … 144, 1 fps snap | ladder 30, 48, 50, 60, 72, 75, 90, 100, 120, 144 (±1 fps) | "75", "75 fps", "75 hz" |
| `adaptivePacing` (G) | On/Off field, Settings > Display | 128 × 24 | "Adaptive pacing" | "On" / "Off" | On | none | up = On | (none) |
| `antiAlias` (G) | On/Off field, Settings > Display | 128 × 24 | "Anti-alias" | "On" / "Off" | On | none | up = On | (none) |
| `metersCollapsed` (V, unchanged) | Header Meters toggle | 28 × 28 | icon | §4.5; restores all sections when all are hidden (§17.10.6) | false | none | none | (none) |

**M3c actions** (not stored):

| Action | Control | Size | Behaviour |
|---|---|---|---|
| Load a settings preset | Presets menu item | 24-high menu items | §17.13.2; one host gesture per parameter |
| Save as, Overwrite, Delete, Open presets folder | Presets menu, dialogs | 280 × 112 / 96 | §17.13.3, §17.13.4 |
| Open the curves panel | Header Refs button | 72 × 28 (28 × 28 compact) | §17.11.1 |
| Take, Take 2nd, Clear | Slot row buttons | 48, 62, 46 × 24 | §17.11.1 |
| Load CSV, Save CSV, Export curves | Slot row buttons, panel title button | 44, 44 × 24; 148 × 28 | §17.12 |
| Cancel a pending file dialog | Status-line button | 56 × 20 | §17.12.2 |
| Reset role, Reset all colours | Colours tab text buttons | 104 × 24, 128 × 28 | §17.14.2 |
| Colour presets: load, Save as, Overwrite, Delete, Open colours folder | Colour preset button and menu | 200 × 28 | §17.14.4 |
| Edit curve colours | Settings > Display "Edit…" | 128 × 24 | Opens the curves panel on the Colours tab |
| Band solo, solo width | Ctrl gestures over the plot | — | §17.9 |

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

The Alt+click reset applies to the controls in this table only. On the spectrum's data rect, Alt is the box-zoom modifier and an Alt+click does nothing (§5.9.3 G14).

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

  **M3c focus order:** (1) Channel, Underlay, Presets, Sidechain 1, Sidechain 2, Refs, Freeze, Reset curves, Meters, Settings; (2) the control-bar chips as above; (3) Metering, Style, Reset statistics, Integration, then the four section toggles. Popovers (Settings, curves panel, preset dialogs) trap focus as §6.1. The band solo has no keyboard equivalent: it is a listening aid for pointer users, and every analysis result is also available without it.
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

**M3c:** from M3c (T-057) §17.10 adds the 14 metering modes, the `level` and `density` snapshot fields, the Density style, the parameter-driven ballistics (replacing the fixed 20 dB/s, 1500 ms and 30 dB/s below) and section hiding. §8.1 and §8.4 to §8.8 keep applying, except where §17.10 says otherwise.

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
| **M3c:** effective value differs | Underlay effectively Off, Integration and Release fixed by the mode, Density in a loudness mode, frame rate above the display rate: the set value in `state-warn`, the tooltip starts with the effective value (§17.3, §17.10, §17.15) | | | |
| **M3c:** legacy value | | | Channel and Underlay show the equivalent pair, with the external-change dot after a load (§17.3) | |
| **M3c:** no input on a sidechain | | Sidechain segment: `text-disabled` digit, 4 px hollow ring (§17.6) | | No curve, legend entry or hover row |
| **M3c:** band preview / solo | | | | Tint 45 % / 65 %, dashed / solid edges, BAND / SOLO tag, band cursors (§17.9) |
| **M3c:** disabled action | Slot buttons without data, Take 2nd with no secondary curve, file buttons while a dialog is pending, Overwrite and Delete for Default: `text-disabled`, not clickable, tooltip explains | | | |
| **M3c:** destructive confirm | Slot Clear: "Sure?" in `state-error` for 3000 ms; preset Delete: confirmation popover (§17.11.1, §17.13.4) | | | |
| **M3c:** modified preset | Presets button name followed by an `accent` "*" (§17.13.1) | | | |

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
  - The spectrum view has the description "Spectrum display. Hover for a readout. Scroll to zoom frequency, Alt+scroll to zoom level, drag to pan, double-click to fit, Alt+drag to zoom into a box. Click to reset the max and average curves; right-click to copy the frequency." (updated by T-034, modifiers by T-042). The reserved Ctrl gestures are not described until M3c gives them a function.
  - Zoom panel (T-034): a group named "Zoom", whose accessible value is the visible range, for example "1.00 to 1.12 kHz, −30.0 to −24.0 dB", followed by ", bin spacing 11.7 Hz" while row 3 shows. The Fit button is named "Fit view", with the description "Show the full range set by the Frequency and Level chips", and supports the press action. The overview strip is named "Spectrum overview" and has the same value as the group. None of them is in the Tab order (§7.2), but assistive technology can press Fit through its accessibility handler.
  - Meter readouts are labels with names such as "Left peak max", "Integrated loudness" and "Correlation".

### 10.4 M3c (T-045)

§17.19 holds the M3c contrast pairs, the non-text contrast of the role colours, the hit targets and the accessible names; §17.4 holds the colour-vision table of the default role colours and the style cue of every curve type. Summary of the checks:
- Every default role colour is at least 7.2 : 1 against `bg-plot` (the sidechain edges 4.0 and 6.0, the secondaries 5.1 and 5.2), above the 3 : 1 minimum for graphics.
- In the protanopia, deuteranopia, tritanopia and greyscale simulations, every pair of curve types stays distinguishable by fill, width, dash or position; no decision depends on hue. Same-type curves (the four references) also differ by dash pattern and by name.
- The band-solo state, the hidden sections, the modified preset, the effective-value states and the CSV errors each carry a non-colour cue (text, line style, slash, "*", icon).
- Every new control is at least 24 × 24 and has an accessible name.
- The spectrum description gains the band-solo sentence of §17.9.7.

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
| `cursor-zoom-box.svg` (T-034) | 0 0 24 24 | Alt held (without Ctrl) over the data rect, and during a box drag (T-042) | Hotspot (7.5, 7.5), the open centre of the crosshair |
| `icon-refs.svg` / `-16.svg` (T-045) | 24 / 16 | Header Refs button | A solid curve over a dashed curve |
| `icon-eye-16.svg`, `icon-eye-off-16.svg` (T-045) | 16 | Reference slot visibility | Open eye; eye with a slash |
| `icon-section-bars-16.svg` (T-045) | 16 | Section toggle: level bars | Two vertical bars of different heights |
| `icon-section-table-16.svg` (T-045) | 16 | Section toggle: statistics | A table with three rows |
| `icon-section-loudness-16.svg` (T-045) | 16 | Section toggle: loudness | A speaker with two sound arcs |
| `icon-section-correlation-16.svg` (T-045) | 16 | Section toggle: correlation | A horizontal scale with end and centre ticks and a triangular marker above it |
| `icon-warning-16.svg` (T-045) | 16 | Contrast warning, CSV warnings and errors | A triangle with an exclamation mark |
| `icon-chevron-down-16.svg` (T-045) | 16 | Presets and colour-preset buttons | A downward chevron |
| `cursor-band.svg` (T-045) | 0 0 24 24 | Band preview (G24) | A centre line between two open brackets; hotspot (12, 12). Black and white, never recoloured |
| `cursor-band-solo.svg` (T-045) | 0 0 24 24 | Band solo (G23-M3c) | The same brackets with the band between them filled white (8 × 10 at (8, 7), 1 px black outline) and the centre line drawn black across the fill; hotspot (12, 12). Black and white, never recoloured |
| `icon-spectra-16.svg` (T-065) | 16 | Compact Spectra list button (§18.3.1) | Three list rows: a filled, a half-filled and an open square, each with a line |
| `icon-presets-16.svg` (T-065) | 16 | Compact Presets button (§18.9.2) | Two stacked cards with folded corners, `fill-opacity` 0.35 on the front card |
| `icon-copy-ab-16.svg` (T-065) | 16 | Compact A/B Copy button (§18.13) | A filled square, an arrow, a dashed square |
| `icon-lock-16.svg` (T-065) | 16 | Locked slot 1 fields (§18.3.2, §18.4) | A padlock, `fill-opacity` 0.35 on the body |
| `icon-link-16.svg` (T-065) | 16 | Linked analysis fields in the slot window (§18.4) | Two interlocked rounded links |

- **Cursor exception (T-034; T-045 adds the two band cursors):** the cursor SVGs use a fixed `#FFFFFF` fill with a 1 px `#000000` outline instead of `currentColor`, which is the OS cursor convention and keeps them visible on any background. They are rendered to images (§5.9.4) and never recoloured. They are otherwise clean: a `viewBox`, a `<title>`, no raster images, scripts or external references.
- The Fit button is text-only, so there is no Fit icon.

- **Size variants:**
  - The 24-grid icons use 2-unit strokes on integer coordinates, so they are pixel-exact at 24 px and 48 px.
  - The 16-grid variants use 1-unit strokes on half-integer coordinates, so they are pixel-exact at 16 px and 32 px.
  - Use `-16` at 16 px and 32 px (32 px is 200 % of the 16 px header icons), and the 24 set at 24 px.
- **SVG hygiene:** every file uses `currentColor` only (the sidechain, level-bars and loudness icons add `fill-opacity` 0.35), has a `viewBox` and a `<title>`, and contains no raster images, scripts or external references.
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
  - The reserved Ctrl gestures (§5.9.3 G22, G23) record no view and request no repaint, so a touchpad pinch over the data rect costs nothing until M3c.
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

### 12.2 M3c (T-045)

The M3c rendering rules are in §17.18. In short:
- **Layer count** (drives Adaptive pacing): one layer per curve drawing operation (primary fill, primary stroke, underlay stroke, each secondary, each sidechain fill, each sidechain edge, each visible reference), at most 13. With Adaptive on, the spectrum repaints at `max (30, cap / 2)` fps when the physical data rect exceeds 2.0 Mpx or the count is at least `pacing.layerThreshold` = 7 (the T-029 value, to be recalibrated by T-054 and T-055). With Adaptive off only the cap applies.
- **The cap** is `min (frameRate, display refresh)` for every editor update; the numeric meter text stays at 10 Hz; budgets are gated at 60 fps.
- New drawing: the Sidechain 2 hatch, the reference curves and the anti-alias-off strokes are column-rect lists; the band tint and edges are ≤ 4 rects; Density is ≤ 11 rect lists per meter paint; the colour wheel uses cached images.
- No new gradients, alpha layers, shadows or blurs. The curves panel and the preset dialogs reuse the popover's cached shadow.

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
| Zoom modifiers (T-042) | Alt+wheel level zoom (G3), Alt+left-drag box zoom (G13); Ctrl+wheel and Ctrl+left press over the data rect reserved for the M3c band solo and inert until then (G22, G23) | 5.6, 5.9.3 |

### 13.4 M3c coverage (T-045)

**Parameters** (plan parameter table):

| Param ID | Control and location | Display format | Double-click resets to | Settings row | Mockup |
|---|---|---|---|---|---|
| `channelMode` (5 offered values, 2 legacy aliases) | Channel segmented, header | "Mid" … "Stereo"; legacy as the pair | Mid | Analysis | yes |
| `underlay` | Underlay chip, header | "Off" … "Stereo" | Off | Analysis | yes |
| `freqLow` (1 to 1000 Hz) | Frequency chip low half | "1.00 Hz", "20.0 Hz" | 20.0 Hz | Analysis | yes |
| `align0dB` | Settings field, legend tag | "On" / "Off" | Off | Analysis | yes |
| `showSidechain` ("Sidechain 1 trace") | Sidechain pair segment 1 | toggle | On | Inputs | yes |
| `showSidechain2` | Sidechain pair segment 2 | toggle | On | Inputs | yes |
| `meterScale` (14 modes) | Metering chip, strip | §17.10.2 short names | dBFS | Meters | yes |
| `meterStyle` | Style segmented, strip | "Bars" / "Density" | Bars | Meters | yes |
| `meterIntegration` | Integration chip, strip | "1.00 ms" | 1.00 ms | Meters | yes |
| `meterRelease` | Settings field | "750 ms" | 750 ms | Meters | yes |
| `meterPeakHold` | Settings field | "1.00 s", "Off" | 1.00 s | Meters | yes |
| `soloWidth` | Settings field, Ctrl+wheel | "1 oct", "1/3 oct", "0.42 oct" | 1 oct | Analysis | yes |
| `rmsWindow` (moved from the strip) | Settings field | "300 ms" | 300 ms | Meters | yes |

**`ViewState`:** `meterSections` (strip toggles and Meters tab, §17.10.6); `inputLabel0` to `inputLabel2` (Inputs tab, §17.5); `ReferenceSlots` (curves panel, §17.11); `metersCollapsed` and the editor size (unchanged). All are in the mockup.

**Global preferences (`GlobalSettings`):** the eight role colours (§17.4, §17.14), the current colour-preset name (§17.14.4), `frameRate`, `adaptivePacing`, `antiAlias` (§17.15). All are in the mockup.

**Files:** settings presets `*.invpreset` (§17.13), colour presets `*.invcolours` (§17.14.4), CSV (§17.12).

**`MeterSnapshot` fields:**

| Field | Readout | Section |
|---|---|---|
| `level[ch]` | Level part of the bars; Density's current-level notches; stale as −inf | 17.10.3 to 17.10.5 |
| `density[ch][204]` | Density style | 17.10.5 |
| `momentary`, `shortTerm` | Also the programme bar in the loudness modes | 17.10.3 |
| `integrated`, `maxMomentary`, `maxShortTerm` | Loudness block in LUFS, or in LU (+23) in the LU modes | 17.10.7 |
| (`momentaryCh`, `shortTermCh`) | Not used: the user chose one programme value (T-043 R5) | 17.10.3 |

**`SpectrumFrame` and engine fields:**

| Item | Where | Section |
|---|---|---|
| `primaryBDb`, `secondaryBDb`, `isDual` (now: an underlay is present) | Underlay curve and its secondary | 17.4 |
| `sidechain2Db`, `hasSidechain2` | Sidechain 2 hatched curve | 17.4 |
| `sidechainDb`, `hasSidechain` | Sidechain 1 | 17.4 |
| Point frequencies down to 1 Hz | Grid, hover, notes | 17.7 |
| Capture (`requestSpectrumCapture`, `getSpectrumCapture`, the settings in force) | Take, Take 2nd, Export curves, info line | 17.11, 17.12 |
| `isSidechainActive (0)`, `(1)` | Sidechain pair no-input state | 17.6 |
| `setBandSolo`, `bandSoloHeartbeat` | Band solo states | 17.9 |
| `PresetStore` (`getCurrentPresetName`, `isModified`, save, overwrite, remove, load) | Presets button and menu | 17.13 |

## 14. IP check against T-001 §8

| Rule | How this design complies |
|---|---|
| No "SPAN" or "Voxengo" name | The product text says "Investigate" only. The tooltips and this spec's user-facing copy are original. |
| No Voxengo logos, icons or screenshots | Every SVG was drawn from scratch for this task. The wordmark is custom monoline geometry, and the icons are generic metaphors (snowflake, circular arrow, sliders, panel). |
| No SPAN colour scheme or skin | Blue-black ink with aqua, amber and lavender curves, and teal-to-red meters. The spectrum fill is aqua, not olive or yellow-green, and there is no grey chrome. The controls are flat chips, not SPAN's buttons or knobs. |
| No copied layout proportions | Statistics live in a right-hand column rather than a bottom band. Settings live in a bottom bar and a popover rather than a title bar and a modal editor. The window is 12:7 rather than about 5:2. |
| No manual text | Every tooltip and label is newly written. The window and curve names (Hann, Blackman, RT max, …) are generic terms (T-001 §8). |
| M3c names and formats (T-045) | Mode names are Investigate's own or standard terms ("dBFS 30", "dBFS+3", "K-20 cal", "LUFS momentary", "LU short-term", "Underlay", "Density", "Align 0 dB", "Take", "Take 2nd"); no SPAN preset names; file formats `.invpreset`, `.invcolours` and plain CSV; the M3c tooltips and messages are newly written (T-043 IP note). The colour wheel, the curves panel and the two-row strip header are Investigate's own layout. |

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

**M3c (T-045).** Answered on 2026-10-07 (roadmap Decisions of 2026-10-07, T-065 log): 7, 8, 9 and 10 decided by the architect as recommended; 11, 12 and 13 accepted by the user as recommended. 14 and 15 stay open. The question texts are kept for reference.
7. **Resolved (architect, as recommended).** **Band-solo edge function (dsp-engineer, T-050):** the plan clamps the band edges at 10 Hz … 0.45 fs, T-044 clamps the centre at 20 Hz … 0.45 fs. The editor must draw what is played (§17.9.3). **Recommended answer:** T-050 exposes one header-only function, for example `BandSoloMath::resolve (pointerHz, widthOct, sampleRate) → { fc, fLo, fHi }`, used by both the filter and the editor, and documents its clamps.
8. **Resolved (architect, as recommended).** **Choice texts (plugin-engineer, T-051):** the host-visible choice texts of `meterScale`, `channelMode` and `underlay` should match the UI names (§17.10.2, §17.3): "dBFS 30", "dBFS 15", "dBFS+3", "K-20 cal", "LUFS M", "LUFS S", "LU M", "LU S"; channel "Stereo"; legacy "L/R (legacy)" and "M/S (legacy)". The indices are unchanged.
9. **Resolved (architect, as recommended).** **Preset name validation (plugin-engineer, T-053):** `PresetStore` should return a reason code (empty, too long, invalid character, trailing dot or space, reserved device name, reserved "Default") rather than text, so the UI shows the texts of §17.13.3 and the colour presets (T-054) reuse the same check.
10. **Resolved (architect, as recommended).** **Display refresh rate (gui-engineer, T-054):** JUCE does not report the monitor's refresh rate directly. **Recommended answer:** measure the `VBlankAttachment` interval (median of the last 30 callbacks) and use it for the effective frame rate and the `state-warn` rule (§17.15).
11. **Resolved (user, accepted as recommended): no target mark.** The loudness bar has no zones (one colour). Should it mark a target (for example −14 or −23 LUFS)? The spec has none, because a target is a delivery choice.
12. **Resolved (user, accepted as recommended): they stay in Settings > Meters.** Release, Peak hold and the RMS window move into Settings > Meters to make room for Integration and the section toggles in the strip. Is that acceptable, or should Release and Peak hold also be in the strip (a third header row, 34 px less bar height)?
13. **Resolved (user, accepted as specified).** The default colours of Sidechain 2 (green) and References 1 to 4 (pink, yellow, blue, silver), §17.4.
14. **Open.** **CSV sample (user, from T-043 R18):** a CSV file exported by the user's other analyser would confirm the import rules before T-058.
15. **Open.** **Touchpad pinch (T-037, Risk 18):** whether a pinch arrives with Ctrl decides whether it changes the solo width or zooms (§17.9.5). T-037 records the result; the M3c checklist text follows it. No design change either way.

**M3 review and spectrum slots (T-065).** Answered on 2026-10-07 (roadmap Decisions of 2026-10-07, T-065 log): 16, 20, 21, 22, 24 and 25 accepted by the user as recommended; 18 and 19 decided by the architect as recommended; 17 and 23 answered by the user with a change to the sidechain metrics, now built into §18.10. The combined mockup approval (T-045 plus T-065) is still pending. New: question 26.
16. **Resolved (user, accepted): the hint line stays.** **For the user (#18):** the hint line at the bottom-right of the spectrum (gesture help, no delay) is kept as part of the hover readout, and the gutters gain "Scroll: zoom level" and "Scroll: zoom frequency". **Recommended:** keep it; it is the only place the spectrum gestures are written down. The alternative is to remove it too.
17. **Resolved (user, 2026-10-07): one bar per sidechain is kept**, with the main bars at 21 px (one sidechain) or 13 px (two) at the default size (§18.10.2). The user's change concerns the rows (question 23). **For the user (#12):** each connected sidechain gets one bar (its louder channel) next to the main bars; with two sidechains the main bars narrow from 22 to 13 px at the default size. **Recommended:** accept. The alternative, a strip that grows by 32 px per sidechain, would hide the Frequency, Level and Max crest controls at 1200 × 700 whenever a sidechain connects.
18. **Resolved (architect, as recommended).** **dsp-engineer (T-047):** from M3c the display axis runs to `freqHigh` above Nyquist (§18.8): `DisplayRange.h` should return `fHiEff = freqHigh` (keeping the one-octave rule), while the mapper still stops at Nyquist and the frame says where. **Recommended:** add a `nyquistHz` to the frame next to the per-slot `binHz` (T-046).
19. **Resolved (architect, as recommended).** **dsp-engineer and plugin-engineer (T-049, T-052):** the Max crest readout and the sidechain blocks need per-channel `maxCrest[ch]` in `MeterSnapshot`, for every input. **Recommended:** add it to the struct that all three banks publish.
20. **Resolved (user, accepted).** **For the user (#14):** the minimum size becomes 560 × 400; below 600 px wide the meter strip is 120 px with bars and correlation only, and the header is compact below 900 px (icons, Offset in the Spectra panel). **Recommended:** accept.
21. **Resolved (user, accepted).** **For the user (#8):** the Offset options read "Match {slot name}" (by default "Match Spectrum 2", "Match SC 1", "Match SC 2") instead of "Spectrum 2, 3, 4". **Recommended:** accept; the default "Match SC 1" is the user's "Sidechain" option.
22. **Resolved (user, accepted).** **For the user (#22):** Copy (A→B) acts at once with a toast, without a confirmation. **Recommended:** accept; a two-step confirmation would slow the common "copy, then tweak B" workflow.
23. **Resolved by the user with a change (2026-10-07):** each sidechain shows **every statistic the main input shows**, with the same rows and units (Peak, Peak max, TP, TP max, Headroom, RMS, Crest, Max crest, Clips, TP clips, the loudness block, correlation with balance), at `meter.sc-opacity`; where a short window has no room the fit ladder applies (rows shrink, then give way to a note). Built into §18.10.3 (the sidechain table, three forms) and §18.10.4 (the eight-step ladder, resolved at 1200 × 700, 920 × 540, 600 × 400 and 560 × 400 with zero, one and two sidechains); §18.14 change 2 is revised. The original proposal was: a sidechain's metrics are Peak max, TP max, RMS, Crest max, short-term and integrated loudness, correlation and balance (not the full nine-row table), and they shrink or give way to a note when the window is too short.
24. **Resolved (user, accepted): slot 1 is always shown.** **For the user:** slot 1 (Main) has no visibility toggle, because it can never be off and the parameter table has no slot 1 Visible parameter. **Recommended:** keep it always shown. If the user wants to hide it, plugin-engineer adds `s1Visible` (T-051) and the lock cell becomes an eye toggle.
25. **Resolved (user, accepted).** **For the user:** with the slot names, the default legend reads "Main · Mid", "Spectrum 2 · RT max", "SC 1" instead of today's "Mid", "RT max", "Sidechain". The curves themselves are unchanged. **Recommended:** accept.
26. **Open, for the user at the mockup review (follows from question 23):** at the default 1200 × 700, two sidechains with every section shown do not fit in full. The ladder (§18.10.4) keeps both sidechains' ten statistics rows side by side and lets their loudness and correlation rows give way to a note ("SC loudness and correlation need more height."); everything shows from H = 760. **Recommended:** accept, because the pair form compares SC 1 and SC 2 row by row and the per-channel rows are the larger part. **Alternative:** keep SC 1 complete (form A) and let SC 2's rows give way, which fits at 1200 × 700 with main rows of 16 px. A third option is for the user to hide the Loudness section when comparing two sidechains, which frees the room with no ladder step (§18.10.4 example). At 920 × 540 and below, any sidechain table gives way to the note in both variants.

## 16. Change log

| Date | Task | Change |
|---|---|---|
| 2026-09-28 | T-010 | First version. The user approved `mockup.html` without changes; layout and visual design frozen for T-011 to T-013. |
| 2026-10-03 | T-034 | **Spectrum zoom and pan (M3b).** New §5.9: view model, view arithmetic, gestures G1 to G21 with exact numbers, cursors, hover behaviour during gestures, zoom panel (indicator, Fit button, bin-spacing row, overview strip) with geometry at four sizes, collision rules, deep-zoom grid rules with worked examples, Fit and no-op cases, changes from the plan, mockup simplifications. Updated: §4.1 (gutters as wheel targets, panel position); §5.3 (view mapping, cache rule); §5.5 (view-based frequency formula, 4-figure zoom precision, hover-card candidates around the panel, new hint-line texts and fit rule); §5.6 (interaction table replaced, clipboard precision); §5.7 (freeze plus zoom, freeze epoch); §6 (Fit view action, deferred click reset); §7.2 (no zoom keys, Esc during a box drag); §10 (new §10.2a contrast tables, names, hit targets); §11 (three cursor SVGs); §12.1 (rendering and budgets for zoom and pan); §13.3 (view state not persisted, new frame fields); §15 (questions 5 and 6). `tokens.json` 1.1.0 adds the groups `zoom`, `zoomLayout`, `zoomInteraction`, `deepGrid` and `cursor`. New assets: `cursor-hand-open.svg`, `cursor-hand-grab.svg`, `cursor-zoom-box.svg`. `mockup.html` implements every gesture and no longer loads web fonts, so it opens from the file system with no network access. |
| 2026-10-03 | T-034 (fold-in) | Two stale points that T-011 had overridden in its task file (roadmap Decision 2026-10-01) are folded in: the font files and license texts live in `third_party/fonts/` and the `InvestigateAssets` binary-data target, not in `assets/ui/fonts/` (§3.2, §6.2, §11, and the font file paths in `tokens.json`); and the old §15 open questions 1 to 3 are closed (§15). |
| 2026-10-04 | T-042 | **Zoom modifier remap**, from the user decision of 2026-10-03 (Ctrl is kept free for the M3c band solo). Level zoom moves from Ctrl+wheel to **Alt+wheel** (G3) and box zoom from Ctrl+left-drag to **Alt+left-drag** (G13), with the same factors, Shift fine steps, 4 px cancel rule and Esc cancel. Ctrl+wheel and a Ctrl+left press, click, double-click or drag over the data rect are reserved for the band solo and do nothing until M3c (new rows G22, G23; the other G numbers are unchanged); a Windows touchpad pinch, which arrives as Ctrl+wheel, is therefore inert over the data rect until M3c and changes the solo width from M3c (this replaces the old "pinch zooms level" note, plan risk Z4). An Alt+click on the data rect does nothing; the chip Alt+click reset (§7.1) is unchanged. Updated: §5.5 hint line, §5.6, §5.9.3 (G1 to G7, G11 to G14, G21 to G23, modifier precedence, a pointer to T-044 for Risk 17), §5.9.4 cursors, §5.9.6 tooltip, §5.9.10 rows 5 and 12, §5.9.11, §7.1, §10.3 description, §11, §12.1, §13.3, and the `cursor-zoom-box.svg` title. `tokens.json` has no gesture references and is unchanged. `mockup.html` handlers use Alt, and the Ctrl variants are inert. |
| 2026-10-04 | T-045 | **M3c addendum** (awaits the user's mockup approval; T-054 to T-058 do not start before it). New §17 with every M3c design: header with Channel, Underlay, Presets, Sidechain 1 and 2 and Refs, and a compact mode below 1174 px; curve roles, styles and the colour derivation rule; legend and hover card with input labels; sidechain host support; the 1 Hz axis and note names below C0; Align 0 dB; band solo; the two-row meter-strip header, the 14 metering modes, Density, ballistics and section hiding with re-flow; reference slots; CSV flows; settings presets; the colour editor and colour presets; display preferences; the tabbed Settings popover; resolved layouts; performance and accessibility rules. New §6.3 (control inventory), §10.4, §12.2 and §13.4. Marked M3c additions (the M3b behaviour is unchanged for T-037) in §4.1, §4.2, §4.4, §5.1, §5.2, §5.5, §5.6, §5.9.3 (rows G22-M3c, G23-M3c, G24), §5.9.4, §6.1, §7.2, §8, §9, §11 and §14. New questions 7 to 15 in §15. `tokens.json` 1.2.0 adds the groups `curveRole`, `solo`, `meterModes`, `density`, `headerM3c`, `stripM3c`, `popoverM3c`, `display` and `toastM3c`. New assets: `icon-refs.svg`, `icon-refs-16.svg`, `icon-eye-16.svg`, `icon-eye-off-16.svg`, four `icon-section-*-16.svg`, `icon-warning-16.svg`, `icon-chevron-down-16.svg`, `cursor-band.svg`, `cursor-band-solo.svg`. The mockup was extended in the 2026-10-07 row. |
| 2026-10-07 | T-045 (narrowed) | **Superseded parts marked** after the M3 review triage (`docs/plans/m3-review-spectrum-slots.md`): §17.3 (Channel and underlay) and §17.6 (Sidechain pair) in full; §17.2, §17.4, §17.5, §17.14.2, §17.16 and §6.3 in part, each with a "Superseded by §18 (T-065, spectrum slots)" pointer and the old text kept. **Fixes:** §11 and §17.9.2 now describe `cursor-band-solo.svg` as drawn (a filled band between the brackets, the centre line black across it; the file is unchanged) and §17.9.7 says "an open band or a filled band"; §11 describes the loudness and correlation icons as drawn and lists every icon with `fill-opacity`; §17.17.5 gives the colour-preset dialog rects (the old "368 + 112" sum was wrong); §17.10.5 adds the histogram decay (T-043 R11) and the exact warn look of the Style control; §17.12.4 routes messages to a toast while the Colours tab is shown; §17.20 lists what the mockup simplifies or leaves out. `tokens.json` 1.2.1 marks the M3b-only ballistics tokens and the superseded M3c tokens. **`mockup.html`** now demonstrates the band solo (preview, solo, tint, edges, stubs, band block, hint, cursors, Ctrl+wheel ladder and fine steps), the 14 metering modes with Bars and Density, the ballistics parameters, section hiding with re-flow and the all-hidden rule, the 1 Hz axis and note names below C0, Align 0 dB, anti-alias off, the frame-rate cap and Adaptive pacing, the colour wheel and colour presets on generic roles, the reference slots with Take, Load, Save, Clear and Export, the CSV error toast and status line, the settings presets menu with its dialogs, the tabbed Settings popover and the compact header. Approval: one combined mockup after T-065. |
| 2026-10-07 | T-065 | **M3 review and spectrum slots**, new §18. §18.1 (M3d, for T-066): every tooltip removed (texts become accessible descriptions; Block menu hints and gutter hints replace the only information they carried), the sample-rate label removed, the SC toggle icon-only and teal when on, Settings without duplicates of the main window, effective values in `state-warn` with an underline (#5), the 3 dB level-grid ladder, Smooth to 3 oct and Level −72 … −18 dB. §18.2 to §18.13 (M3c): the four-slot model with default colours, the Spectra header group and panel, the slot window with Link, slot styles and Fill, the legend collision rule (fixes the 23.5 px legend and FROZEN overlap measured by T-037), Offset, the 96 kHz axis with the shaded region above Nyquist, the minimum size 560 × 400 with breakpoints 1060, 900 and 600, the compact layout, sidechain bars and metrics with the fit ladder, the Max crest readout, 520-wide popovers, A/B. §18.14 lists every superseded section and the changes from the plan. §15 gains questions 16 to 25. `tokens.json` 1.3.0. New assets: `icon-spectra-16.svg`, `icon-presets-16.svg`, `icon-copy-ab-16.svg`, `icon-lock-16.svg`, `icon-link-16.svg`. `mockup.html` shows the combined M3c design (§18.15). |
| 2026-10-07 | T-065 (revision) | **User answers to §15** (roadmap Decisions of 2026-10-07). **Sidechain metrics (questions 17 and 23):** one bar per sidechain is kept; its rows now show every statistic the main input shows (ten per-channel rows including Max crest, the six loudness values, correlation and balance) at `meter.sc-opacity`. §18.10.1 to §18.10.4 rewritten: one sidechain table at full strip width below the main sections in three forms (one sidechain with Statistics on or off, two sidechains as a pair table); the bars column ends above it; alarm cells keep full strength; an eight-step fit ladder (SC rows 16, main rows 18 then 16, SC loudness and correlation give way, the table gives way, then the main short forms) resolved at 1200 × 700, 920 × 540, 600 × 400 and 560 × 400 with zero, one and two sidechains. §18.9.5: two compact lines per sidechain (correlation and balance), Hc = 68 + 32 × nSC. §18.11 and §18.14 change 2 updated; §18.15, §18.17 and §18.18 adjusted. §15: questions 7 to 13 and 16 to 25 marked resolved (accepted, decided as recommended, or changed); 14 and 15 stay open; new question 26 (two sidechains at 1200 × 700). `tokens.json` `stripSidechain` revised (still 1.3.0, not yet approved). `mockup.html`: full sidechain statistics simulated, the three table forms, the new ladder and notes, two compact lines per sidechain, a 1200 × 780 size button. |

## 17. M3c additions (T-045)

Source: `docs/plans/m3c-feature-expansion.md` (the plan), `docs/research/T-043-span-m3c-reference.md` (R1 to R20), `docs/research/T-044-host-platform-constraints.md`, and the user decisions of 2026-10-04. Every value named `token.*` here is in `tokens.json` 1.2.1. Parts of this section are superseded by §18 (T-065, spectrum slots); each such subsection starts with a pointer. Everything in this section is **M3c** and is implemented by T-054 to T-058; the M3b behaviour of §5.5 to §5.9 that T-037 implements is unchanged. Where an older section is superseded from M3c, it says so with a pointer here.

### 17.1 Scope map

| Plan item | What | Sections |
|---|---|---|
| [1] Band solo | Preview, audible solo, tint, edges, readout, width, release, cursor | 17.9, 5.9.3 (G22-M3c, G23-M3c, G24), 5.9.4, 6.3 |
| [2] Metering modes and style | 14 modes, scales, ticks, zones, Density | 17.10.2 to 17.10.5 |
| [3] Hide metrics | Section toggles, re-flow, all hidden | 17.10.6, 17.17.3 |
| [4] Channel model | Channel segmented control, Underlay, legacy pairs | 17.2, 17.3 |
| [5] 1 Hz | Grid labels, formats, note names | 17.7 |
| [6] Align 0 dB and anti-alias | Align toggle and legend tag; anti-alias preference | 17.8, 17.15 |
| [7] Three inputs | Sidechain 2 curve, toggles, labels, host support | 17.4, 17.5, 17.6 |
| [8] Curve colours | Roles, derivation, colour editor, colour presets, live update | 17.4, 17.14 |
| [9] Frame rate | Cap, Adaptive, effective rate | 17.15, 17.18 |
| [10] Ballistics | Integration, release, peak hold: controls and display formulas | 17.10.4, 17.10.7, 6.3 |
| [11] Multi-track comparison | Covered by [7] | 17.6 |
| [12] Reference slots | Panel, curve style, legend, persistence | 17.11, 17.4 |
| [13] CSV | Export curves, slot load and save, async dialogs, error messages | 17.12 |
| [14] Settings presets | Header menu, Save as, Overwrite, Delete, folder | 17.13 |
| Coverage, accessibility, performance | | 13.4, 17.19 (and 10.4), 17.18 (and 12.2) |

Rows [4], [7] and [11], and the underlay and Sidechain 2 roles of [8], are superseded by §18 (T-065, spectrum slots), which replaces them with plan items [15] to [22] of `docs/plans/m3-review-spectrum-slots.md`.

### 17.2 Header

> **Partly superseded by §18 (T-065, spectrum slots):** the Channel and Underlay items and the Sidechain pair are replaced by the slot controls of §18; T-065 also merges the compact-header breakpoint below into one set of width breakpoints. The Presets, Refs, Freeze, Reset curves, Meters and Settings rows and the compact rule stay valid until §18 restates them. The text below is kept for history.

The M3b Mode control (two segmented groups) is replaced by a **Channel** segmented control and an **Underlay** chip. A **Presets** button, a **Sidechain** toggle pair and a **Refs** button are added. The header stays 44 high.

**Two modes.** The header is **full** when `W ≥ 1174` and **compact** below. Compact changes only the right group: Refs, Freeze and Reset curves become 28 × 28 icon buttons (the label moves into the tooltip and the accessible name), and the Presets button narrows. 1174 is the smallest width at which the full right group starts at least 16 px after the Presets button.

| Item | Full (W ≥ 1174) | Compact (W < 1174) | Content |
|---|---|---|---|
| Wordmark | (16, 13, 81, 18) | same | unchanged |
| Channel | (116, 8, 192, 28) | same | Track `surface-2`, radius 6, padding 2. Segments 24 high at x + 2: Mid 40, Side 40, L 28, R 28, Stereo 52 |
| Underlay chip | (316, 8, 124, 28) | same | Inline choice chip (§4.4 style): label "UNDERLAY" left at x + 8, value right-aligned at x + w − 8 |
| Presets button | (452, 8, Pw, 28) | same | `Pw = clamp (SRx − 14 − 452, 96, 160)`, where SRx is the sample-rate text x of the active mode. §17.13 |
| Sample-rate text | (W − 546, 8, 60, 28) | (W − 358, 8, 60, 28) | unchanged style (§8.8) |
| Sidechain pair | (W − 470, 8, 84, 28) | (W − 282, 8, 84, 28) | §17.6 |
| Refs | (W − 380, 8, 72, 28): 16 px `icon-refs-16` + "Refs" | (W − 192, 8, 28, 28): icon only | Opens the curves panel (§17.11) |
| Freeze | (W − 302, 8, 84, 28) | (W − 158, 8, 28, 28): icon only | unchanged behaviour (§5.7) |
| Reset curves | (W − 212, 8, 116, 28) | (W − 124, 8, 28, 28): icon only | unchanged behaviour |
| Divider | (W − 84, 14, 1, 16) | same | |
| Meters toggle | (W − 72, 8, 28, 28) | same | §17.10.6 adds the all-sections-hidden case |
| Settings | (W − 40, 8, 28, 28) | same | §17.16 |

Resolved positions are in §17.17.1. Gaps: 16 between the sample-rate text and the sidechain pair, 6 between buttons, 12 before the divider, as in §4.2.

Icon-only buttons keep the 28 × 28 hit target, the hover fill and the states of §9. Their tooltip is the button label followed by the existing tooltip text ("Freeze. Hold the current spectrum. …").

### 17.3 Channel and underlay

> **Superseded by §18 (T-065, spectrum slots).** The Channel plus Underlay header control and the underlay are replaced by the per-slot Channel setting; the legacy L/R and M/S values resolve to Left and Mid for slot 1 (`docs/plans/m3-review-spectrum-slots.md`, Legacy values). Not mocked. The text below is kept for history.

**Channel segmented control** (bound to `channelMode` indices 0 to 4):
- Segment texts: "Mid", "Side", "L", "R", "Stereo". "Stereo" is index 4 (the L+R power mean); the plan keeps the definition (T-043 R12).
- Behaviour as the M3b Mode control (§7.1): click selects, wheel steps ±1 over the five segments (clamped), double-click or Alt+click resets to Mid, arrows step and Delete resets when focused.
- Tooltips: "Mid: left and right summed and halved.", "Side: left minus right, halved.", "Left channel.", "Right channel.", "Stereo: the power average of left and right." Each ends with the line "Scroll to step. Double-click to reset to Mid."

**Underlay chip** (bound to `underlay`): an inline choice chip. Values "Off", "Mid", "Side", "Left", "Right", "Stereo". The menu lists them in that order with "default" after Off. Drag, wheel, click-to-menu and reset as every choice chip (§7.1).

**Effective pair and the warn state** (the §5.4 convention: the set value shows in `state-warn`, and the tooltip starts with the effective value):
- Underlay equal to the channel: the chip value shows in `state-warn`; tooltip "Effective Off: the underlay is the same as the channel."
- Mono main input: both controls stay editable. The Underlay value shows in `state-warn` with "Effective Off: the main input is mono." The Channel segments show normally; the legend names the curve "Mono" (§17.5).

**Legacy values** (`channelMode` 5 = L/R, 6 = M/S, from sessions or automation of earlier builds):
- Display: L/R shows the **L** segment selected and the Underlay chip showing "Right"; M/S shows **Mid** and "Side". The `underlay` parameter value is ignored and not shown.
- Both controls carry the external-change dot (§9) for 600 ms after the load, and the Underlay tooltip starts "Saved by an earlier version as L/R. Changing the channel or the underlay saves it as Left with a Right underlay."
- Nothing is written until the user edits either control. The first edit writes the normalised pair as **one undoable gesture group**: `underlay` first, then `channelMode` (each with begin, set, end), so a host that groups adjacent gestures undoes both at once. Examples: legacy L/R, the user clicks "Side": `underlay` = Right, `channelMode` = Side. Legacy M/S, the user picks Underlay "Off": `underlay` = Off, `channelMode` = Mid.
- Host automation that writes 5 or 6 later shows the pair again, with no write-back.

**Auto-unfreeze** (§5.7) fires when the *effective* pair changes, from any source. Changing the set `underlay` while it is ignored (legacy value) does not unfreeze.

### 17.4 Curve roles, styles and colours

> **Partly superseded by §18 (T-065, spectrum slots):** the colour roles tied to the old Primary B (`underlay`, "Secondary of the underlay", the `Underlay` row) and the `sc2` role tied to the Sidechain 2 trace become slot colours in §18. Still valid: the primary, Sidechain 1 and reference styles, the reference dash patterns, the derivation rule for secondary tones and fills, and the opaque-colour rule. The mockup demonstrates the colour editor on the generic roles Primary, Sidechain and Ref 1 to Ref 4. The text below is kept for history.

This table replaces the §5.1 table from M3c. Draw order 1 is at the back.

| Role (colour role) | When drawn | Stroke | Fill | Order |
|---|---|---|---|---|
| Sidechain 1 (`sc1`) | `hasSidechain && showSidechain && isSidechainActive (0)` | 1 px role colour at 70 % | Flat role colour at 26 % to the data bottom (per-column bars, §12) | 1 |
| Sidechain 2 (`sc2`) | `hasSidechain2 && showSidechain2 && isSidechainActive (1)` | 1 px role colour at 70 % | **Hatched**: a 1 logical px bar at every 3rd logical px column (x where `floor (x − data.x) mod 3 = 0`), from the curve down to the data bottom, role colour at 40 % | 2 |
| Primary (`primary`) | always | 1.5 px role colour, round joins | Vertical gradient of the role colour, 30 % at the data top to 4 % at the bottom (T-029 bands) | fill 3, stroke 10 |
| References 1 to 4 (`ref1` to `ref4`) | slot visible and holding data (§17.11) | **Dashed column spans**, 1.5 px thick (§17.18), role colour opaque. Patterns along x, in logical px: Ref 1 dash 8 on 4 off; Ref 2 dot 2 on 3 off; Ref 3 dash-dot 8, 3, 2, 3; Ref 4 long dash 14 on 4 off | none | 4 |
| Sidechain edges | as the sidechains | (the strokes above) | | 5 |
| Secondary of the primary (derived from `primary`) | `hasSecondary && secondary ≠ Off` | 1 px derived colour, opaque | none | 6 |
| Secondary of the underlay (derived from `underlay`) | the same, with an underlay | 1 px derived colour | none | 7 |
| Underlay (`underlay`) | `isDual` (an effective underlay) | 1.5 px role colour | **none** | 9 |

Without hue, every curve type differs by style:
- Primary: the only line with a gradient fill.
- Underlay: the only bright solid line with no fill.
- Secondaries: thin (1 px) and dark (luminance 0.23 by default).
- Sidechain 1: a solid flat silhouette. Sidechain 2: a hatched (striped) silhouette.
- References: the only dashed lines, each slot with its own dash pattern.

**Default role colours** (the first launch, and Reset to default). The first four are the current tokens, so the first launch is pixel-identical to the M3b build.

| Role | Default | Luminance | Contrast on `bg-plot` | Deuteranopia (Machado 1.0) | Greyscale cue |
|---|---|---|---|---|---|
| `primary` | #62D6F0 | 0.57 | 11.5 : 1 | #AEC0F0 pale blue | gradient fill |
| `underlay` | #FFB454 | 0.55 | 11.0 : 1 | #DFCA56 yellow | solid line, no fill |
| `sc1` | #A58BFF | 0.34 | 7.2 : 1 (edge at 70 %: 4.0 : 1) | #669BFC mid blue | solid silhouette |
| `sc2` | #8FD96B | 0.57 | 11.4 : 1 (edge at 70 %: 6.0 : 1) | #D5C472 khaki | hatched silhouette |
| `ref1` | #F48FB1 | 0.42 | 8.7 : 1 | #B6B4AF warm grey | dash 8/4 |
| `ref2` | #E9DD72 | 0.70 | 13.9 : 1 | #F1DD77 yellow | dot 2/3 |
| `ref3` | #6FA8FF | 0.39 | 8.1 : 1 | #72A1FD blue | dash-dot |
| `ref4` | #C8CED8 | 0.61 | 12.3 : 1 | #C9CDD8 silver | long dash 14/4 |
| derived secondary of `primary` | #3E8FA3 | 0.23 | 5.2 : 1 | dark blue | thin dark line |
| derived secondary of `underlay` | #A87A3C | 0.23 | 5.1 : 1 | dark olive | thin dark line |

Protanopia moves every colour onto the same blue–yellow axis as deuteranopia (within a few levels of the values above); tritanopia moves them onto a cyan–pink axis. Pairs that collapse to similar hues in a simulation (for example `underlay`, `sc2` and `ref2`, all yellow in deuteranopia) are always different curve types: a solid line, a hatched silhouette and a dotted line. The mockup's "Vision check" selector (§10.2) applies the simulation to all ten colours live, and its "Role colour check" dev button lists the simulated hex values and contrasts for the current colours.

**Derivation rules** (one function in `source/gui/CurveColours.h`, used by the editor and the colour editor's preview):
- **Secondary tone** of `primary` and `underlay`: if the role colour equals its default, the secondary is the token (#3E8FA3 or #A87A3C), so defaults stay pixel-identical. Otherwise convert to HSV and set `v' = v × 0.68`, `s' = s`, `h' = h`; if the result is below 3 : 1 against `bg-plot`, raise `v'` in steps of 0.01 until it reaches 3 : 1 or `v' = v`. (0.68 is the value ratio of the two default pairs; the default secondaries lie within 4 levels of the rule.)
- **Fills**: the primary gradient uses the role colour at the token alphas (30 % to 4 %); the T-029 band colours are recomputed from the role colour, once per colour change. The sidechain fills use the role colour at 26 % (`sc1`) and 40 % (`sc2`), the edges at 70 %.
- **Markers, swatches and hover rows** use the role colour; secondary markers use the derived tone.
- **Alpha** is never user-editable. Role colours are stored opaque.

Colours apply only to curves. The background, grid, meters, text and accent stay design tokens (plan, Out of scope).

### 17.5 Legend, hover card and input labels

> **Partly superseded by §18 (T-065, spectrum slots):** the per-input labels (`inputLabel0` to `inputLabel2`) are replaced by the slot names `slotName1` to `slotName4`, and the legend and hover names follow the slots. Still valid: the legend flow into rows, the reference and ALIGN 0 dB entries, and the hover-card order with the band block first. The 16-character text-field rules are reused by the reference-slot names (§17.11.1). The text below is kept for history.

**Input labels** (`ViewState.inputLabel0`, `inputLabel1`, `inputLabel2`; defaults "Main", "SC 1", "SC 2"):
- Edited in Settings > Inputs (§17.16) in three text fields of 160 × 24.
- At most **16 characters**: the field refuses further typing (the character is dropped and the field outline flashes `state-error` for 300 ms). Any printable character is allowed; control characters are removed. Leading and trailing spaces are trimmed on commit. An empty field commits the default.
- Enter or focus loss commits; Esc reverts. A change is a `ViewState` change, not a parameter: no host gesture, no undo.

**Legend entries** (in this order, each only while its curve is drawn):
1. Primary: "{label0} · {channel}", for example "Main · Mid", "Mix · Stereo". Channel names in the legend: Mid, Side, Left, Right, Stereo, or "Mono" with a mono main input.
2. Underlay: "{label0} · {underlay channel}", for example "Main · Side".
3. The secondary: "RT max", "All-time max" or "All-time avg", with both swatches stacked while an underlay is drawn (as §5.2).
4. Sidechain 1: "{label1}". 5. Sidechain 2: "{label2}".
6. References, in slot order: the slot name.
7. "ALIGN 0 dB" as a text-only tag (`caps`, `text-secondary`, no swatch) while `align0dB` is on (§17.8).

Each entry is a 16 × 10 swatch in the curve's style (the hatched swatch for Sidechain 2, the slot's dash pattern for references) and its name in `body-sm` / `text-secondary`.

**Legend flow** (it replaces the single-row legend of §5.2 from M3c; position, fill, padding and the 25 % fade rule are unchanged):
- Entries flow left to right into rows. A row is a pill of 20 high; row k sits at y = data.y + 8 + 22k (2 px between rows).
- Row width limit `Wmax = right − (data.x + 8)`, where `right` is `data.right − 8`, or `panel.x − 8` while the zoom panel shows. Row 0 also stops at `badge.x − 8` while the FROZEN badge shows (the badge occupies row 0's height only).
- An entry that does not fit starts a new row. An entry wider than `Wmax` on its own row truncates its name with "…".
- Toasts are drawn above the legend (§5.9.7 z-order), so a reset toast may briefly cover row 1. This is accepted: toasts are transient.
- Widest cases, resolved: §17.17.4.

**Hover card rows** (they replace the rows of §5.5 from M3c; position, style and formats are unchanged):
- Band block first while the band solo previews or plays (§17.9.4).
- Then the frequency and note lines, a separator, and one row per curve: primary; then the underlay if drawn, otherwise the secondary if shown; Sidechain 1; Sidechain 2; each visible reference with data at f. Row names follow the legend names. A reference without data at f (outside its file's range) has no row.
- Name strings come from the label-image cache (§12.1) because they change rarely; only values are drawn per frame.

### 17.6 Sidechain inputs and host support

> **Superseded by §18 (T-065, spectrum slots):** the Sidechain pair and the Sidechain 2 trace toggle are replaced by the slot list (slot 4 Visible is `showSidechain2`), and a missing input is shown per slot. The host-support facts in this section (Live and Bitwig offer one sidechain; an absent and an unconnected bus look the same) remain inputs to §18. Not mocked. The text below is kept for history.

**Sidechain pair** (header, §17.2): track `surface-2`, radius 6. The 16 px sidechain icon at (x + 6, 14) in `text-secondary`, then two toggle segments of 28 × 24 at (x + 26, 10) and (x + 54, 10), showing "1" and "2" in `body` weight 600.
- Segment 1 is bound to `showSidechain`, segment 2 to `showSidechain2`. Each toggles on click, resets to On on double-click or Alt+click, and steps with the wheel (up = On) as the M3b SC toggle (§7.1).
- On: `accent-tint` fill, 1 px `accent` border at 50 %, `accent` digit. Off: transparent, `text-secondary` digit.
- **No input** (`isSidechainActive (i)` false): `text-disabled` digit plus a 4 px hollow ring (1 px stroke) at the segment's top-right, inset 3. When the trace is On, a 1 px `border` outline remains. The segment stays clickable, and the setting is kept.
- Tooltips:
  - connected: "Sidechain 1 ({label1}): show this input as a solid silhouette." and "Sidechain 2 ({label2}): show this input as a striped silhouette." Second line: "Click to toggle. Double-click to reset (on). Rename in Settings > Inputs."
  - no input on 1: "No input on sidechain 1. Route a track to the plugin's first sidechain input in your host."
  - no input on 2: "No input on sidechain 2. Route a track to the plugin's second sidechain input. Some hosts offer only one sidechain input (for example Ableton Live and Bitwig Studio); use sidechain 1 there."

A host that never offers the second bus and a host whose second bus is offered but not connected look the same: the processor reports the bus inactive, and the UI cannot tell the two apart. Nothing in the UI depends on Sidechain 2 (plan Risk 15).

**Aux curve combination** (plan [4]): with no underlay, each sidechain uses the primary channel combination; with an underlay, the Stereo power mean. The legend and the hover names do not change with it.

**Silence and inactivity:** as §5.8 for both sidechains: an active silent sidechain falls to the floor; an inactive one has no curve, no legend entry and no hover row.

Settings > Inputs shows a read-only status row per sidechain: "Stereo", "Mono" or "No input" (`body-sm`, `text-secondary`; "No input" in `text-tertiary`).

### 17.7 1 Hz axis, formats and note names

`freqLow` reaches 1 Hz (plan [5]). Everything in §5.3 and §5.9.8 applies unchanged; this section adds only what is new below 10 Hz.

- **Grid labels:** the decade sets already include k = 0, giving the labels "1", "2", "3", "5", "7". Resolved at 1 Hz to 20 kHz: the default size (840 px, 195.5 px per decade) uses {1, 2, 5}: 1, 2, 5, 10, 20, 50, 100, 200, 500, 1k, 2k, 5k, 10k, 20k (14 labels, smallest gap 58.8 px); {1, 2, 3, 5, 7} fails (5 to 7: 28.6 px). The minimum size (560 px, 130.3 px per decade) also uses {1, 2, 5} (smallest gap 39.2 px). "1" at x = 48 extends from 44.7 to 51.3, inside the edge rule.
- **Deep zoom below 10 Hz:** the linear-step rule of §5.9.8 is unchanged; its format already gives "1.1", "1.25" and so on.
- **Frequency format below 10 Hz** (hover card, chips, panel rows): 3 significant figures with **2 decimals**, "4.37 Hz", "1.00 Hz". The zoomed 4-figure rule gives **3 decimals** below 10 Hz, "4.372 Hz". Clipboard: "4.37" and "4.372". At 10 Hz and above, §5.5 is unchanged.
- **Note names below C0:** the formula of §5.5 continues with negative octave numbers, written with U+2212: 13.75 Hz gives "A−1 +0c", 8.66 Hz gives "C#−1 +0c". Below C−1 (MIDI 0, that is when `midi < 0`, below 7.94 Hz) line 2 shows "below C−1" in `text-tertiary`.
- **Frequency chip:** the low half shows "1.00" + "Hz"; its width is unchanged (4 characters, as "20.0").
- `freqLow` text entry accepts "1", "1 Hz", "2.5". Its drag and wheel follow the new log range 1 to 1000 Hz (skew centre 31.6 Hz): 240 px for the full range, wheel × 2^(±1/6), fine × 2^(±1/24), rounded to 3 significant figures.

### 17.8 Align 0 dB

- **Control:** an On/Off field "Align 0 dB" in Settings > Analysis, section "LEVEL AND SOLO" (§17.16). The control bar has no room: at 1200 × 700 its groups fill the available 880 px exactly (§4.3), so any new chip would hide group C at the default size.
- **Visible state:** while on, the legend ends with the text tag "ALIGN 0 dB" (§17.5), so a user reading levels sees that the calibration is active.
- **Tooltip:** "Calibrates the display so a steady 0 dBFS sine at 1 kHz reads 0 dB at any block size, window and smoothing. Noise and dense music read higher with it on. Pairs well with slope 0 and the dBFS+3 meter mode."
- Toggling it changes the frames only; the editor issues no reset, toast or unfreeze (frozen data keeps the setting of the freeze moment, §5.7).

### 17.9 Band solo

#### 17.9.1 States

| State | Starts when | Ends when | Audio | Visual |
|---|---|---|---|---|
| Idle | otherwise | | pass-through | §5.5 hover |
| **Preview** | Ctrl held (polled, §17.9.2), pointer in the data rect and not over the zoom panel, no mouse button down, no pan, box or overview gesture active | Ctrl released; the pointer leaves the data rect or enters the zoom panel; a button goes down (a left press with Ctrl enters Solo) | pass-through | band, preview tint and dashed edges at the pointer; band block "BAND" in the hover card; preview hint; band cursor |
| **Solo** | Ctrl+left press in the data rect, not over the zoom panel (G23-M3c) | the left button is released; Ctrl is released; the pointer leaves the **editor** bounds; the editor loses focus, is hidden, or is destroyed; `setAnalysisActive (false)` | band-pass at the band (`setBandSolo (true, fc)` on the press and at most once per editor update while fc changes; `bandSoloHeartbeat()` on every editor update) | solo tint, solid edges, band block "SOLO", solo hint, solo cursor |

- On every end of Solo the editor calls `setBandSolo (false, fc)` in the same update and stops the heartbeat; the 250 ms failsafe in the processor covers a hung editor.
- From Solo, a released Ctrl with the button still held ends the solo; the rest of that press does nothing (no pan, box or reset; G23 mode fixed at the press). If the button is released with Ctrl still held, the state returns to Preview.
- While soloing, the centre follows the pointer x, clamped to the data rect, so the solo continues when the pointer crosses into the gutters or the meter strip. Leaving the editor ends it.
- The solo is never saved, never automated and never undoable. `soloWidth` changes are host gestures (the parameter is not automatable but is part of the state and of settings presets).

#### 17.9.2 Modifier polling and the cursor

- Ctrl is read with `juce::ModifierKeys::getCurrentModifiersRealtime()` on every editor update while the pointer is over the editor (T-044 Q3), and from mouse events. The editor never needs keyboard focus for it.
- Shift with Ctrl is still Preview or Solo (Shift only selects fine width steps). Ctrl with Alt (or AltGr, which JUCE reports as Alt without Ctrl) follows the §5.9.3 precedence: Ctrl first.
- **Cursors:** Preview `assets/ui/cursor-band.svg` (a centre line between two brackets), Solo `assets/ui/cursor-band-solo.svg` (the same brackets with the band between them filled white and the centre line drawn black across the fill), both hotspot (12, 12) on the centre line, rendered as §5.9.4. The cursor changes in the first update after Ctrl goes down, also without a mouse move.

#### 17.9.3 The band on the plot

- **Band edges** are the filter's −3 dB points: `fLo = fc · 2^(−W/2)`, `fHi = fc · 2^(+W/2)`, `W = soloWidth` in octaves, after the DSP's clamps. The editor computes `fc`, `fLo` and `fHi` with the same function the filter uses (§15 question 7), so the drawing always shows what is played. Edges outside the view are drawn at the data-rect edge as a 2 px stub at the top and bottom (8 px long) to show that the band continues.
- **Tint outside the band:** up to two rects, `[data.x, x(fLo))` and `(x(fHi), data.right]`, full data height, in `solo.tint` (`bg-plot`) at **45 %** in Preview and **65 %** in Solo. One `fillRectList` of up to 2 rects. The tint covers the curves, the reference curves and the grid lines inside the data rect; it never covers the gutters, the legend, the zoom panel, the hover card, the hint or the toasts.
- **Edge markers** at `x(fLo)` and `x(fHi)`, full data height:
  - Preview: 1 px dashed, 4 on, 3 off (vertical), `solo.edge` (#E8ECF3) at 60 %, one rect list.
  - Solo: 2 px solid `solo.edge` (#E8ECF3), one rect list.
- **Centre:** no extra line; the hover crosshair (§5.5) already marks it.
- **Z-order** in the plot (§5.9.7 extended): grid, curves, **band tint and edges**, crosshair, markers, legend, FROZEN badge, zoom panel, box band, hover card, hint, toasts.

#### 17.9.4 Readout (band block of the hover card)

The hover card keeps following the pointer (the centre). While the band shows, its first block is:

| Line | Content | Style |
|---|---|---|
| 1 | A tag, then the width right-aligned: `[BAND]  1.00 oct` or `[SOLO]  1.00 oct` | Tag: `caps`, 16 high, radius 4, padding 0 × 6. BAND: `surface-3` fill, `text-secondary` text. SOLO: `text-primary` (#E8ECF3) fill, `text-on-accent` text. Width: `readout`, `text-primary` |
| 2 | The edges: "885 Hz … 1.77 kHz" | `readout`, `text-secondary`, §5.5 frequency format (3 figures) |

A 1 px `border` separator follows, then the usual frequency and note lines (the frequency is the effective centre). When the DSP clamps the centre (the pointer is below the lowest or above the highest allowed centre), the frequency line shows the clamped centre in `state-warn`.

**Width format:** a ladder value shows as a fraction or decimal from the ladder labels below ("1/12 oct", "1/6 oct", "1/4 oct", "1/3 oct", "1/2 oct", "2/3 oct", "1 oct", "1.5 oct", "2 oct", "3 oct", "4 oct"); any other value shows 2 decimals ("0.42 oct", "1.25 oct"). A value within 0.001 octave of a ladder value counts as that value.

#### 17.9.5 Width (Ctrl+wheel, G22-M3c)

- **Ladder** (`solo.widthLadder`): 1/12, 1/6, 1/4, 1/3, 1/2, 2/3, 1, 1.5, 2, 3, 4 octaves.
- **Ctrl+wheel**, in Preview and in Solo, over the data rect or the zoom panel: wheel notches `n` as §5.9.2 (normalised, reversed per `isReversed`, ±3 per event), accumulated; each whole notch moves one ladder step (away from the user = wider). The accumulator resets when Ctrl is released or after 500 ms without a wheel event. A width between ladder values steps to the next ladder value in the wheel direction.
- **Ctrl+Shift+wheel:** continuous fine steps, `W' = clamp (W × 2^(n / 8), 1/12, 4)`, with fractional n applied directly.
- Each wheel event that changes the value is one host gesture on `soloWidth`. The event is always consumed and never zooms or pans; a horizontal Ctrl+wheel also changes the width (by its dominant axis).
- The band, the tint and the readout update in the same editor update; while soloing, the audio follows the parameter.
- **Touchpad pinch** (plan Risk 18): if JUCE reports the pinch with Ctrl, it changes the width like Ctrl+wheel; if not, it is a plain wheel and zooms frequency (G1). T-037 records which, and the M3c checklist text follows it.

#### 17.9.6 Interactions

| With | Rule |
|---|---|
| Zoom and pan | No Ctrl gesture pans, zooms, box-zooms, fits or resets (G22-M3c, G23-M3c). Alt+wheel and Alt+drag still zoom (G3, G13) when Ctrl is not held. A pan or box drag in progress is not interrupted by pressing Ctrl later (§5.9.3 G23). |
| Zoom panel | Preview does not show while the pointer is over the panel, and a Ctrl+press on the panel acts as the panel (Fit, overview), never as a solo. During a Solo the drag is captured by the spectrum, so the band and card stay visible when the pointer passes over the panel. The panel is drawn above the tint. |
| Hover card | Stays visible in Preview and Solo, with the band block (§17.9.4). It follows the §5.5 placement and panel-avoidance rules. |
| Hint line | Replaced while the band shows: Preview "Ctrl+click and hold: listen to the band · Ctrl+scroll: band width"; Solo "Listening to the band · release to stop · Ctrl+scroll: band width". Estimated widths about 330 px and 340 px, within the 544 px limit at the minimum size. |
| Toasts | Ctrl gestures create no toast (no reset, no copy). A right press while Ctrl is held but no solo is active still copies (G16). A right press during a Solo does nothing. Existing toasts keep fading above the tint. |
| Freeze | Preview and Solo work while frozen; the tint covers the frozen curves. The solo never unfreezes, and Freeze never ends a solo. |
| Legend | The 25 % fade rule applies as usual. |
| Meters | Unchanged: they measure the unfiltered input. The Solo width tooltip says so. |
| Settings, curves panel, menus | While a popover or menu is open, Ctrl over the plot does nothing (the popover owns the pointer). A popover never opens during a solo. |

#### 17.9.7 Accessibility of the solo

- The state is shown without colour by: the tag text ("BAND" or "SOLO"), the edge style (dashed or solid, 1 or 2 px), the tint strength (45 % or 65 %), the cursor shape (an open band or a filled band between the brackets), and the hint text.
- On entering Solo the spectrum's accessibility handler posts the announcement "Band solo on, 1.25 kHz, 1 octave" (low priority); on leaving it posts "Band solo off". Width changes during a solo are not announced.
- The spectrum description (§10.3) gains: "Hold Ctrl to preview a band; hold Ctrl and the mouse button to listen to it; Ctrl+scroll sets the band width."

### 17.10 Meter strip

The strip stays 296 wide. Its header grows to **two rows**, which moves the content top from y = 52 to y = **86** (strip-relative).

#### 17.10.1 Header rows (strip-relative)

| Item | Rect | Content |
|---|---|---|
| Metering chip | (12, 12, 128, 28) | Inline choice chip, label "METERING", value the mode's short name (§17.10.2) |
| Style segmented | (146, 12, 92, 28) | Track `surface-2`, padding 2, segments "Bars" 36 × 24 and "Density" 52 × 24 |
| Reset statistics | (256, 12, 28, 28) | Icon button (`icon-reset-16`), tooltip "Reset statistics. …" (the §6 text) |
| Integration chip | (12, 46, 160, 28) | Inline float chip, label "INTEGRATION", value "1.00 ms" |
| Section toggles | (178, 46, 106, 28) | Track `surface-2`, padding 2; four icon toggles of 24 × 24 at x = 180, 206, 232, 258: level bars, statistics, loudness, correlation (§17.10.6) |

- The RMS window chip and the Release and Peak hold controls are in Settings > Meters (§17.16). The Integration chip is in the strip because the default 1.00 ms makes the level part read like a peak meter (user decision of 2026-10-04), so the control must be easy to find.
- Integration chip tooltip: "Integration time of the level part of the bars: how long it takes to rise or fall 20 dB. Below about 50 ms it follows the peaks of low frequencies rather than their average. Release and peak hold are in Settings > Meters." (own wording; T-043 R7.)

#### 17.10.2 Metering modes

The menu shows the modes in five groups with separators; drag and wheel step through them in **menu order** (not index order). Display names are Investigate's own (T-043 IP note).

| Menu order | Index (`meterScale`) | Chip value | Menu text | Bar range (display units) | Ticks: candidates (≥ 20 px) | Level part | Zones (safe / warm / hot) | Integration, release |
|---|---|---|---|---|---|---|---|---|
| 1 | 0 | dBFS | dBFS | −60 … +6 dBFS | 6, 12 | `level` | < −12, −12 … −3, ≥ −3 | parameters |
| 2 | 4 | dBFS 30 | dBFS 30 (−24 … +6) | −24 … +6 dBFS | 1, 2, 3, 6 | `level` | as dBFS | parameters |
| 3 | 5 | dBFS 15 | dBFS 15 (−9 … +6) | −9 … +6 dBFS | 1, 2, 3 | `level` | as dBFS | parameters |
| 4 | 6 | dBFS+3 | dBFS+3 | −60 … +6, dBFS labels (T-043 R2) | 6, 12 | `level` + 3.0103 | as dBFS, on the biased value | parameters |
| — | | | | | | | | |
| 5 to 7 | 1, 2, 3 | K-20, K-14, K-12 | K-20, K-14, K-12 | −60 … +6 dBFS, labelled in K units | 4, 8 | `level` + 3.0103, K-referenced | < 0 K, 0 … +4 K, > +4 K | parameters |
| — | | | | | | | | |
| 8 to 10 | 7, 8, 9 | K-20 cal, K-14 cal, K-12 cal | K-20 calibration (600 ms), … | as K | 4, 8 | as K | as K | **fixed 600 ms** (T-043 R3) |
| — | | | | | | | | |
| 11, 12 | 10, 11 | LUFS M, LUFS S | LUFS momentary, LUFS short-term | −60 … +6 LUFS | 6, 12 | **one programme bar**: `momentary` or `shortTerm` | none (single colour) | fixed windows (400 ms, 3 s) |
| — | | | | | | | | |
| 13, 14 | 12, 13 | LU M, LU S | LU momentary, LU short-term | −37 … +29 LU (0 LU = −23 LUFS, T-043 R6); ticks at multiples of the step from 0 | 6, 12 | one programme bar, LUFS + 23 | none | fixed windows |

- **Tick step:** the smallest candidate whose spacing on the track is at least 20 px. Labels are signed integers ("+6", "0", "−24"), right-aligned at x = 24 of the bars column, with a 1 px `meter.track-line` across the tracks, as §8.2. Resolved per size in §17.17.3.
- The menu's "default" mark is on dBFS. Menu items 8 to 10 carry the hint "(600 ms)" in `text-tertiary`; the C-mode name is Investigate's "cal" wording.
- **Fixed ballistics are shown, not hidden:** in the cal modes the Integration chip (and the Release field in Settings) show their set values in `state-warn`, with the tooltip starting "Effective 600 ms: fixed in K-20 calibration mode." In the loudness modes they show "Effective: the momentary loudness window (400 ms)." or "(3 s)". The fields stay editable (the parameters keep their values for other modes).
- Meters measure the main input only (plan decision; T-043 R14). There are no Mid/Side meters.

#### 17.10.3 Bars style (per mode)

The M3b two-part bar (§8.2) remains for the level modes, with `level` replacing `rms` as the lower part:
- **Level part** (solid, zone colours): from the bar bottom to `level[ch]` plus the mode's bias (above). `level` is the integrated level with release, from the snapshot.
- **Peak part** (pre-blended peak colours): from the level top to the displayed peak (§17.10.4).
- **Hold tick:** 2 px `meter.hold-tick` (§17.10.4). Not drawn when `meterPeakHold` = 0.
- Clip lamps and channel letters as §8.2. Mono: one bar (§8.6).
- **Loudness modes:** the two tracks merge into **one programme bar** from x = 30 to the column width (30 … 80, or 30 … 272 when the bars are alone), labelled "M" or "S" at the bottom (`caps`) instead of L and R. It has a solid fill in `meter.safe` (no zones, no peak part) and a 2 px hold tick that holds the highest loudness for `meterPeakHold` and then drops to the current value. The two clip lamps stay above, side by side as before, because clips are per channel. The single wide bar, the absent L/R letters and the absent peak part make it read as one value, not as a pair.

#### 17.10.4 Ballistics display formulas (editor)

Let `dt` be the time since the previous editor update in seconds, `R = meterRelease / 1000` (0.6 in the cal modes) and `Hs = meterPeakHold / 1000`.
- **Peak part:** `p = max (pNew, pPrev − 20 × dt / R)`, where `pNew` is the snapshot `samplePeak[ch]` (the 50 ms maximum). So the peak falls 20 dB per release time.
- **Hold tick:** `h` takes `pNew` and restarts its timer when `pNew ≥ h`. After `Hs` without a new maximum it falls at `20 / R` dB per second and never below `p`. With `Hs = 0` there is no tick, and the Peak row of the table shows `p`.
- **Level part:** no editor ballistics; it shows `level[ch]` (the DSP applies integration and release, plan [2]).
- **TP row** of the table: held like the hold tick, with `Hs` and `R` (it was 1500 ms and 30 dB/s).
- These replace the fixed 20 dB/s, 1500 ms and 30 dB/s of §8.2 and §8.3 from M3c. A parameter change applies on the next update.
- **Stale snapshot** (§8.7): `level` and `density` are treated like the live levels: `level` as −inf (so the level part falls with the DSP's last state at the release rate in the editor), the density histogram as all zero (the density fades out over one release time, linearly).

#### 17.10.5 Density style

Density shows how often the level stays at each value, as brightness along the track (`density[ch][204]`, 0.5 dB bins from −96 to +6 dBFS, normalised 0 to 1).
- **Mapping:** each bin covers the display interval `[bin + bias, bin + bias + 0.5]` (bias as the mode's level part). For each physical pixel row of the track, take the largest density of the bins that overlap it. Rows outside the bar range are skipped.
- **Quantisation:** `q = floor (7 × sqrt (d) + 0.5)` (the square root makes rare levels visible). q = 0 is not drawn. q = 1 to 7 use `density.level-1` … `density.level-7`, pre-blended `meter.safe` over `bg-plot` at q/7 (#102728, #16423E, #1C5C53, #227768, #28917D, #2EAC93, #34C6A8).
- **Rects:** consecutive rows with the same q merge into one rect of the track width. One `fillRectList` per level for all channels together: at most 7 lists.
- **Zone rail:** a 3 px strip along the inner left edge of each track in the zone colours (safe, warm, hot) of the mode, part of the cached static image. It keeps the zones readable, because the density uses one hue.
- **Current level:** two 4 × 2 px notches in `text-primary`, at the left and right inner edges of the track, at `level + bias` (one list).
- **Hold tick:** the 2 px hold tick of §17.10.4 (one list).
- Clip lamps and letters as Bars.
- **Histogram decay:** `MeterBank` decays the histogram with a time constant of `meterPeakHold` (T-043 R11; 50 ms when the hold is 0); the editor draws the normalised snapshot as it arrives and adds no smoothing of its own.
- **Not in loudness modes:** the density is a level histogram, so in LUFS and LU modes the bar shows the loudness bar of §17.10.3; the Style control keeps "Density" selected, shown in `state-warn` (the selected segment is drawn with a `surface-3` fill and `state-warn` text instead of the `accent` fill, 8.6 : 1), with the tooltip "Effective Bars: density is not available in loudness modes."
- Paint: at most 7 density lists + 1 notch list + 1 hold list + the lamps (2 lists, or the cached image) = **≤ 11 `fillRectList` calls** per meter paint, never a path.

#### 17.10.6 Section hiding

- Four sections: **level bars**, **statistics** (the table), **loudness**, **correlation**. State: `ViewState.meterSections`, bit 0 bars, bit 1 statistics, bit 2 loudness, bit 3 correlation; default 0b1111.
- **Control:** the four icon toggles in header row 2 (§17.10.1): `icon-section-bars-16`, `icon-section-table-16`, `icon-section-loudness-16`, `icon-section-correlation-16`. Shown: `accent-tint` fill, `accent` icon. Hidden: transparent, `text-tertiary` icon with a 1 px diagonal slash from the top-left to the bottom-right of the 16 px icon box. Click toggles at once; there is no double-click reset (a double-click toggles twice). The same four are On/Off fields in Settings > Meters. Tooltips: "Show the level bars", "Show the statistics table", "Show the loudness readouts", "Show correlation and balance", each ending "Click to show or hide."
- **Re-flow** (strip-relative; content top Y0 = 86, content height HRC = SH − 98):
  - **Columns:** with the bars shown, the bars column is (12, 86, 80, HRC) and the right column (108, 86, 176, HRC). With the bars hidden, the right column is (12, 86, 272, HRC). With the right column empty, the bars column is (12, 86, 272, HRC).
  - **Bars inside a column of width CW:** bar width `bw = floor ((CW − 36) / 2)`; L at x 30 … 30 + bw, R at CW − bw … CW; mono and the loudness bar as §17.10.3 (mono: one bar of width bw centred at `30 + (CW − 30 − bw) / 2`). CW = 80 gives the M3b 22 px bars; CW = 272 gives 118 px bars.
  - **Right column:** the visible sections, in the order statistics, loudness, correlation, are stacked from the top. Heights: statistics `16 + 9 × rowH`, loudness 98, correlation 68. With n visible sections and `others` the summed height of the visible loudness and correlation blocks: `rowH = clamp (floor ((HRC − 16 − others − 16 × (n − 1)) / 9), 20, 28)`; `gap = max (12, (HRC − Σ heights) / (n − 1))` for n ≥ 2. So with two or three sections the last one ends at the bottom (SH − 12), as the correlation block does in M3b. A 1 px `border` divider is centred in each gap. With all three, this equals the M3b formula except that rowH may go down to 20 (it was 22), which the shorter content area needs at 920 × 540.
  - **Width-dependent parts:** the table's label column is `CWr − 96` (80 or 176 px); the loudness block's three columns share CWr; the correlation bar is CWr wide with its ticks at ±0.5 of it.
- **All four hidden:** the strip behaves as collapsed (MS = 0) and the header Meters toggle shows its collapsed state. Clicking the Meters toggle then sets `meterSections` to all four and shows the strip; its tooltip in that state is "All meter sections are hidden. Click to show them."
- **The Meters toggle** (`metersCollapsed`) hides the whole strip whatever the sections, and showing it again restores the sections as they were.

#### 17.10.7 Statistics and loudness text per mode

- **RMS row unit** follows the mode: "dBFS" in dBFS, dBFS 30 and dBFS 15; "dBFS+3" in dBFS+3 (value + 3.0103); "K-20", "K-14", "K-12" in the K and cal modes (value as §8.3); "dBFS" in the loudness modes (the RMS stays a level statistic; loudness is in the loudness block). The row still uses `rmsWindow`.
- **Loudness block in LU modes:** the title unit reads "LU" and M, S, I, M max and S max show `LUFS + 23`, signed with 1 decimal ("+2.4", "−5.0", "0.0"). LRA is unchanged. In every other mode the block is as §8.4.
- **Peak row** shows the hold-tick value (§17.10.4), or the peak part when `meterPeakHold` = 0.

### 17.11 Reference slots

#### 17.11.1 The curves panel

The header Refs button opens the **curves panel**, a popover with two tabs: **References** (this section) and **Colours** (§17.14). It is anchored like Settings at (W − 572, 48), **560 × 320**, with the popover fill, border, radius and cached shadow. Settings and the curves panel are mutually exclusive: opening one closes the other. Settings > Display "Curve colours: Edit…" opens this panel on the Colours tab.

Panel-relative layout:

| Item | Rect | Content |
|---|---|---|
| Tab "References" | (12, 4, 96, 28) | `body`; selected: `text-primary` with a 2 px `accent` line at the bottom of the tab; unselected: `text-secondary` |
| Tab "Colours" | (112, 4, 72, 28) | same |
| "Export curves…" | (372, 4, 148, 28) | Text button, References tab only (§17.12) |
| Close | (528, 4, 28, 28) | as Settings |
| Title divider | (0, 36, 560, 1) | `border` |
| Column header | y 44, 16 high | `caps`, `text-tertiary`: "SHOW" at x 12, "NAME" at 66, "OFFSET" at 202, "CAPTURE" at 284, "FILE" at 406 |
| Slot k (k = 0 … 3) | row y = 64 + 52k | controls below, 24 high, at y |
| Status line | (12, 284, 536, 20) | §17.12.4 |

Slot row controls (x, width; all 24 high):

| Control | x, w | Behaviour |
|---|---|---|
| Visibility | 12, 24 | Icon toggle `icon-eye-16` / `icon-eye-off-16`; On shows the curve. Disabled (`text-disabled`) while the slot is empty. |
| Colour swatch | 42, 16 (10 high, at y + 7; hit area 38 … 62 × 24) | The slot's dash pattern in its role colour. Click opens the Colours tab with this role selected. |
| Name | 66, 128 | Text field, at most 16 characters, same rules as the input labels (§17.5); empty commits "Ref k+1" |
| Offset | 202, 76 | Inline float chip, "+0.0 dB" (§6.3) |
| Take | 284, 48 | Text button "Take": store the primary curve |
| Take 2nd | 336, 62 | Text button "Take 2nd": store the secondary curve of the primary. Disabled while `secondary` = Off, tooltip "Turn on a secondary curve first." |
| Load | 406, 44 | "Load": CSV into this slot (§17.12) |
| Save | 454, 44 | "Save": this slot to CSV. Disabled while empty. |
| Clear | 502, 46 | "Clear": two-step (below). Disabled while empty. |
| Info line | 66, y + 28, 482 × 14 | `body-sm`, `text-tertiary` |

- **Info line text:** "Taken 14:32 · 48 kHz · 4096 · Hann · 4× · 1/12 oct · Align off · Mid" (from the capture's settings; "· frozen" is appended when taken while frozen; the time is local hh:mm), "From mix-ref.csv · 812 rows · 20.0 Hz … 20.0 kHz", or "Empty. Take the current curve or load a CSV file."
- **Take** requests a capture (plan [12]) and stores the chosen curve when it arrives (≤ 20 ms). While frozen it stores the frozen data. The slot becomes visible. If the slot held data, it is replaced without asking (Take is the slot's main action; Clear is the destructive one). The status line confirms: "Ref 2: took the primary curve."
- **Clear, two-step:** the first click turns the button into "Sure?" with a 1 px `state-error` border and `state-error` text for **3000 ms**; a second click inside that time clears the slot (data, visibility off, the name kept). Otherwise it reverts.
- **Button style:** text buttons are 24 high, radius 4, `body` in `text-secondary`, transparent; hover `surface-3` and `text-primary`; pressed `surface-3` with a 1 px `accent` border; disabled `text-disabled`.
- **Persistence:** the slots are part of the session (`ViewState` child `ReferenceSlots`), not of settings presets.

#### 17.11.2 Reference curves on the plot

- Style per §17.4 (dashed column spans, role colour, no fill), drawn through the current view mapping, plus the current slope and the slot offset (the stored data has no slope). Grid points marked "no data" are not drawn (a gap).
- They are drawn above the fills and below every stroke (order 4 in §17.4), so the live lines stay on top.
- The legend and the hover card show visible slots by name (§17.5).
- References do not follow Freeze, the channel or any analysis setting: they are fixed data.

### 17.12 CSV import and export, file dialogs, messages

#### 17.12.1 Actions

| Action | Where | Result |
|---|---|---|
| Export curves | Curves panel, References tab title bar | Requests a capture at the click (so the file holds what was on screen at the click), then opens a save dialog. Default file name "Investigate curves 2026-10-04 1432.csv". Columns: `frequency_hz` plus one per visible live curve, named after the legend (for example `main_mid_db`, `main_side_db`, `rt_max_db`, `kick_db`). |
| Save (slot) | Slot row | Save dialog, default name "{slot name}.csv". Columns `frequency_hz,level_db`. |
| Load (slot) | Slot row | Open dialog, filters "*.csv;*.txt". On success the slot's data is replaced and the slot shown. If the slot name is still the default ("Ref k"), it becomes the file name without extension, truncated to 16 characters. |

Header lines, separator and decimal point as the plan ([13]).

#### 17.12.2 Asynchronous dialog flow (T-044 Q5)

- Dialogs use `juce::FileChooser::launchAsync` with the native box, the chooser held as an editor member. Only **one** dialog can be pending.
- **While pending:** every Load, Save and Export button is disabled, and the status line shows "Waiting for the file dialog… It may be behind the host window." with a "Cancel" text button at its right end (480, 284, 56, 20). Cancel destroys the chooser, which closes the dialog, and the status line clears.
- The curves panel may be closed meanwhile; the result then shows as a toast (§17.12.4). Reopening the panel shows the pending state.
- Closing the editor cancels the chooser (no result is applied).
- The slot changes only when a load completes successfully, never on cancel or error. If the user took or cleared the slot while the dialog was open, a successful load still replaces it (the last action wins).
- No modal loop, no blocking wait. Writing a file happens on the message thread after the dialog returns (65,536 rows ≤ 200 ms, plan budget).

#### 17.12.3 Import rules and messages

Parsing follows the plan ([13]) and T-043 R18: comma, semicolon or tab; a decimal comma with semicolons; `#` lines and leading non-numeric lines skipped; rows with a frequency not above 0 Hz skipped and counted; unsorted rows sorted; at most 65,536 rows read.

| Case | Kind | Message (the file name is shortened to 24 characters with "…" in the middle) |
|---|---|---|
| Loaded | success | "Ref 2: loaded mix-ref.csv (812 rows)." Appends " 3 rows at 0 Hz or below skipped." when any were skipped. |
| More than 65,536 rows | warning | "Ref 2: loaded the first 65,536 rows of big.csv. The rest was ignored." |
| Cannot open or read | error | "Ref 2 unchanged. mix.csv could not be read." |
| Empty file, or only comment and header lines | error | "Ref 2 unchanged. mix.csv contains no data rows." |
| A non-numeric value after the data started | error | "Ref 2 unchanged. mix.csv, line 37: "abc" is not a number." |
| A row with fewer than 2 columns | error | "Ref 2 unchanged. mix.csv, line 37: expected a frequency and a level." |
| Fewer than 2 usable rows | error | "Ref 2 unchanged. mix.csv has fewer than 2 usable rows." |
| No overlap with 1 Hz … 48 kHz | error | "Ref 2 unchanged. mix.csv has no data between 1 Hz and 48 kHz." |
| Saved | success | "Saved Ref 1 to ref1.csv." / "Saved 4 curves to Investigate curves 2026-10-04 1432.csv." |
| Write failed | error | "Could not write ref1.csv. Check that the folder is writable." |

Line numbers are 1-based physical lines of the file, counting comment lines.

#### 17.12.4 Where messages appear

- **Curves panel open on the References tab:** the status line (12, 284, 536, 20): a 16 px icon (none for success, `icon-warning-16` in `state-warn` for a warning, `icon-warning-16` in `state-error` for an error) and the text in `body-sm`, `text-primary`. It stays until the next action or **6000 ms** for success, and until the next action for errors and warnings.
- **Curves panel closed, or open on the Colours tab:** a toast centred at the top of the data rect (the reset-toast position and panel rule of §5.9.7). Style: the §5.6 toast, with a 1 px `accent` border (success), `state-warn` border and icon (warning) or `state-error` border and icon (error). Duration: 2500 ms for success, **5000 ms** for warnings and errors, each fading over the last 200 ms. Width at most data.w − 16; longer text wraps to a second line.
- Contrast: `text-primary` on `surface-3` 11.8 : 1; the icons are 3 : 1 or more on `surface-3` (`state-warn` 8.6 : 1, `state-error` 4.4 : 1).

### 17.13 Settings presets

#### 17.13.1 Presets button

- Rect per §17.2; fill `surface-2`, radius 6. Text: the current preset name in `body`, `text-primary`, at x + 10, followed directly by a **"*"** in `accent` when `isModified()` (the asterisk is a shape cue, not only colour). A `icon-chevron-down-16` in `text-secondary` sits at the right end (x + w − 20). The name is truncated with "…" to fit `w − 10 − 26`.
- States: hover `surface-3`; open: `accent-tint` fill, 1 px `accent` border at 50 %.
- Tooltip: "Settings preset: {name}{, modified}. A preset stores every analysis and meter setting. It never changes colours, input labels, references or the window size."
- Accessible name "Settings preset", value "{name}" or "{name}, modified".

#### 17.13.2 Menu (a `juce::PopupMenu` in the §7.1 style, opened below the button)

1. "Default" (the current-item dot when current).
2. A separator, then the user presets sorted by name (case-insensitive), each with the current-item dot when current. With none: one inert item "No saved presets" in `text-tertiary`.
3. A separator, then "Save as…", "Overwrite "{name}"", "Delete "{name}"…". Overwrite and Delete are disabled while the current preset is Default.
4. A separator, then "Open presets folder" (opens `…\Presets\Settings\` in the file manager, creating it first).

Choosing a preset loads it at once (no confirmation; a load is a set of host gestures that hosts with undo can revert). Loading never changes colours, `ViewState`, the reference slots or the zoom (plan [14]).

#### 17.13.3 Save as (inline name entry)

A small popover below the Presets button, at (452, 40), **280 × 112**, with the popover style:

| Item | Rect (popover-relative) | Content |
|---|---|---|
| Title | (12, 8, 256, 16) | "Save preset as", `body` weight 600 |
| Name field | (12, 30, 256, 24) | Text field (§17.5 style), pre-filled with the current name ("" for Default), fully selected, focused |
| Message | (12, 58, 256, 14) | `body-sm`; validation message (below) |
| Cancel | (132, 80, 64, 24) | Text button |
| Save / Replace | (204, 80, 64, 24) | Primary button: `accent` fill, `text-on-accent`, weight 600. Disabled (`surface-2`, `text-disabled`) while the name is invalid |

Validation runs on every keystroke (the texts map PresetStore's reasons, §15 question 9):

| Name | Message (colour) | Button |
|---|---|---|
| empty (after trimming) | "Enter a name." (`text-tertiary`) | disabled |
| longer than 64 characters | "Use at most 64 characters." (`state-error`) | disabled |
| contains \ / : * ? " < > \| or a control character | "Remove these characters: \ / : * ? " < > \|" (`state-error`) | disabled |
| ends with a dot or a space | "A name cannot end with a dot or a space." (`state-error`) | disabled |
| CON, PRN, AUX, NUL, COM1 to COM9 or LPT1 to LPT9 (any case) | "This name is reserved by Windows." (`state-error`) | disabled |
| "Default" (any case) | "Default is built in. Choose another name." (`state-error`) | disabled |
| an existing preset (case-insensitive) | "A preset with this name exists. Replace it?" (`state-warn`) | "Replace" |
| valid | (empty) | "Save" |

Enter acts as the button when enabled; Esc or a click outside cancels. On success the popover closes, the preset becomes current (not modified), and a toast "Saved preset "{name}"." appears for 2500 ms. A write error keeps the popover open with the message "Could not save. Check that the presets folder is writable." in `state-error`.

#### 17.13.4 Overwrite and Delete confirmations

The same popover frame, **280 × 96**: a message in `body` (12, 10, 256, 40, wrapping), and Cancel (132, 64, 64, 24) and the action button (204, 64, 64, 24).
- Overwrite: "Replace "{name}" with the current settings?" with the primary button "Replace".
- Delete: "Delete "{name}"? This cannot be undone." with a destructive button "Delete": `state-error` fill, `text-on-accent` text (6.1 : 1).
- After a delete the current preset becomes "Default" with the modified mark if the parameters differ from the defaults.

### 17.14 Colour editor and colour presets

#### 17.14.1 Choice of the colour wheel

**Own component** `source/gui/ColourWheel` (gui-engineer), not `juce::ColourSelector` and not reFX/ColourSelector:
- The design wants a circular hue ring with a saturation and value square inside, a hex field and presets. `juce::ColourSelector` has a square HSV field and RGB sliders, no ring, hex field or presets (T-044 Q7), so it would need the hex field and presets added anyway and still look unlike the rest of the UI.
- reFX/ColourSelector is MIT-licensed, but whether it has a round wheel is undocumented, and it would add a third-party dependency, a licence entry and a build-engineer task for one small control.
- An own component is about 200 lines of plain JUCE, draws with the project's tokens and the T-029 rules (cached images), and needs no licence check.

#### 17.14.2 Colours tab layout (curves panel, panel-relative)

> **Role list partly superseded by §18 (T-065, spectrum slots):** the Underlay and Sidechain 2 rows (roles tied to the old Primary B and to the Sidechain 2 trace) become slot colours. The layout, the wheel, the hex field, the contrast warning and the colour presets stay valid; the mockup shows six generic rows (Primary, Sidechain, Ref 1 to Ref 4).

| Item | Rect | Content |
|---|---|---|
| Role list | (12, 44, 224, 224) | 8 rows of 28: Primary, Underlay, Sidechain 1 ({label1}), Sidechain 2 ({label2}), then the four slot names. Each row: the role's legend swatch (24 × 12) at x + 8, the name (`body`, `text-secondary`) at x + 40, the hex value (`readout` 11 px, `text-tertiary`) right-aligned at x + 216. Selected row: `surface-3` fill, a 2 px `accent` bar at its left edge, name in `text-primary`. |
| Hue ring and square | (252, 44, 176, 176) | Ring: outer radius 88, inner radius 72, centred at (340, 132). Square: (292, 84, 96, 96). |
| Preview | (444, 44, 104, 32) | `bg-plot` fill, radius 4, with a sample of the role's curve style in the edited colour (a short line, fill, hatch or dash) |
| Hex field | (444, 84, 104, 24) | Text field, `value` font, "#62D6F0" |
| Contrast | (444, 114, 104, 32) | Two lines, `body-sm`: "11.5 : 1" (`text-primary`) and "on the plot" (`text-tertiary`). Below 3 : 1: `icon-warning-16` + "2.4 : 1" in `state-warn`, second line "Hard to see" |
| Secondary tone | (444, 152, 104, 16) | Primary and Underlay only: a 24 × 2 px line in the derived secondary colour and "2nd curve" (`body-sm`, `text-tertiary`) |
| Reset role | (444, 176, 104, 24) | Text button "Reset role": the selected role to its default |
| Preset label | (12, 280, 104, 28) | "COLOUR PRESET" (`caps`, `text-tertiary`), vertically centred |
| Preset button | (120, 280, 200, 28) | As the Presets button (§17.13.1): the current colour preset name, "*" when the colours differ from it, chevron; opens the colour-preset menu |
| Reset all | (420, 280, 128, 28) | Text button "Reset all colours": every role to its default; the preset becomes "Default colours" |

#### 17.14.3 Wheel behaviour

- **Ring:** hue angle, 0° (red) at the top, increasing clockwise. Press and drag anywhere in the ring (radius 72 to 88, plus 4 px tolerance each side) sets the hue from the pointer angle. The square's hue follows.
- **Square:** x is saturation (0 left, 1 right), y is value (1 top, 0 bottom). Press and drag sets both; the pointer is clamped to the square.
- **Shift while dragging:** the change from the press point is scaled by 0.25 (fine).
- **Wheel:** over the ring, hue ±2° per notch (Shift ±0.5°); over the square, value ±0.02 (Shift ±0.005).
- **Keyboard** (the wheel component is focusable, in the panel's Tab order): Left/Right saturation ±0.01, Up/Down value ±0.01, PageUp/PageDown hue ±5°, Shift for a quarter of each step.
- **Markers:** ring: a circle of radius 6 on the ring's centre line, 2 px `#FFFFFF` stroke with a 1 px `#000000` outer stroke; square: a circle of radius 5, same strokes. Black and white keep the markers visible on every hue.
- **Hex field:** accepts "#RRGGBB", "RRGGBB" and "#RGB", case-insensitive; Enter or focus loss applies; invalid text reverts with the 300 ms `state-error` flash. Alpha is not accepted.
- **Contrast warning:** the WCAG ratio of the role colour against `bg-plot` (#0A0D13), updated live. Below 3.0 : 1 the warning shows; the colour is still applied.
- **Live update:** every change applies at once to the role in this editor and, through `GlobalSettings`, to every open editor in the process in the same editor update. Writes to the preference file are debounced to at most 2 per second, with a final write 500 ms after the last change.
- **Rendering:** the ring is a cached image per size and display scale (built once with `Colour::fromHSV` per pixel); the square is a cached image rebuilt when the hue changes, at most once per editor update during a ring drag. The markers are drawn per repaint. Only the wheel bounds repaint during a drag.
- **Accessible names:** "Hue", "Saturation and value", "Hex colour", with values "191 degrees", "saturation 59 %, value 94 %", "#62D6F0".

#### 17.14.4 Colour presets

- Files `*.invcolours` in `…\Presets\Colours\` (plan [8]); a name and one ARGB value per role.
- The menu mirrors §17.13.2: "Default colours" (built in), the user colour presets, "Save as…", "Overwrite "{name}"", "Delete "{name}"…", "Open colours folder". Save as, Overwrite and Delete use the dialogs of §17.13.3 and §17.13.4, anchored below the preset button, with the title "Save colour preset as" and the same validation (with "Default colours" as the reserved name).
- Loading a colour preset sets all eight roles at once (live update as above). The current colour preset name and its modified state are stored in `GlobalSettings` with the colours.
- Colours never enter the session state or settings presets.

### 17.15 Display preferences

Global preferences in `GlobalSettings` (every instance and launch on the machine), edited in Settings > Display (§17.16):

| Preference | Control | Format | Default | Notes |
|---|---|---|---|---|
| `frameRate` | Float field 128 × 24 | "60 fps", integers 30 to 144 | 60 | Drag 240 px over 30 … 144 (1 fps snap). Wheel steps along the ladder 30, 48, 50, 60, 72, 75, 90, 100, 120, 144; Shift ±1 fps. Text entry "75", "75 fps", "75 hz". The editor's update limiter caps all updates at `min (frameRate, display refresh)`. When the measured vblank rate is lower than the set value by more than 2, the value shows in `state-warn` with "Effective 60 fps: limited by the display (60 Hz)." |
| `adaptivePacing` | On/Off field | "On" / "Off" | On | Tooltip: "Halves the spectrum's frame rate (never below 30 fps) for very large windows or many curves, to save CPU." (§17.18) |
| `antiAlias` | On/Off field | "On" / "Off" | On | Tooltip: "Smooth curve edges. Off draws crisper, pixel-aligned curves." Drawing only (T-043 R16). |
| Curve colours | Text button "Edit…" (128 × 24) | | | Opens the curves panel on the Colours tab |

A note under the rows (`body-sm`, `text-tertiary`, wrapping in 256 px): "These settings apply to every Investigate window on this computer. Frame rates above 60 fps use more CPU."

These preferences are not parameters: no host gesture, no undo, no external-change dot. Double-click and Alt+click still reset them to their defaults (the chip conventions of §7.1).

### 17.16 Settings popover

> **Partly superseded by §18 (T-065, spectrum slots):** the Underlay row of the Analysis tab and the whole Inputs tab (sidechain traces, Sidechain 2 trace, input labels) are replaced by the slot settings window of §18. The Analysis (other rows), Meters and Display tabs stay valid. The text below is kept for history.

From M3c the popover (§6.1) has **tabs**, at the same anchor (W − 572, 48), **560 × 400** for every tab (no resize when switching tabs, so the cached shadow is rendered once per open). The About view stays 560 × 440 (§6.2).

| Item | Rect (popover-relative) | Content |
|---|---|---|
| Title bar | (0, 0, 560, 36) | "Settings", About button and Close as §6.1 |
| Tabs | y 37, 28 high: "Analysis" (16, 37, 80, 28), "Inputs" (100, 37, 64, 28), "Meters" (168, 37, 68, 28), "Display" (240, 37, 72, 28) | Tab style of §17.11.1 |
| Tab divider | (0, 68, 560, 1) | `border` |
| Column 1 | x 16, from y 80, 256 wide | Section titles 20 high, rows 28 high (label left, 128 × 24 field right), as §6.1 |
| Column 2 | x 288, from y 80, 256 wide | same |

| Tab | Column 1 | Column 2 |
|---|---|---|
| **Analysis** | "SPECTRUM": Channel, Underlay, Block size, Overlap, Window, Average time, Smoothing, Slope, Secondary curve (9 rows, ends y 352) | "DISPLAY RANGE": Frequency low, Frequency high, Range low, Range high; "LEVEL AND SOLO": Align 0 dB, Solo width (ends y 288) |
| **Inputs** | "SIDECHAINS": Sidechain 1 trace, Sidechain 1 input (status text), Sidechain 2 trace, Sidechain 2 input (status text) | "LABELS": Main input, Sidechain 1, Sidechain 2 (text fields 160 × 24, so their labels have 96 px), then the note "At most 16 characters. An empty label restores the default." (`body-sm`, `text-tertiary`) |
| **Meters** | "METERING": Metering, Style, Integration, Release, Peak hold, RMS window (6 rows), then the note "Integration below about 50 ms follows the peaks of low frequencies. The cal modes fix integration and release at 600 ms." | "SECTIONS": Level bars, Statistics, Loudness, Correlation (On/Off fields) |
| **Display** | "ALL INSTANCES": Frame rate, Adaptive pacing, Anti-alias, Curve colours, then the note of §17.15 | (empty) |

- In the Analysis tab, Channel is a choice field over its five values (Mid, Side, Left, Right, Stereo) showing the effective primary; Underlay as §17.3.
- The Settings dot rule (§4.3) is unchanged: hidden control-bar groups are all in the Analysis tab.
- The popover opens on the tab that was last shown in this editor session (Analysis at first). Tab switching: click, or Ctrl+Tab / Ctrl+Shift+Tab while focus is inside the popover.
- **Focus:** Tab cycles the About button, the tabs, the current tab's fields in column order, then Close, trapped as §6.1.
- Height check: at 920 × 540 the popover ends at y = 448, inside the editor.

### 17.17 Resolved layouts

#### 17.17.1 Header

| Item | 1200 × 700 (full) | 920 × 540 (compact) |
|---|---|---|
| Wordmark | 16 … 97 | 16 … 97 |
| Channel | 116 … 308 (Mid 118, Side 158, L 198, R 226, Stereo 254 … 306) | same |
| Underlay | 316 … 440 | same |
| Presets | 452 … 612 (Pw 160) | 452 … 548 (Pw 96) |
| Sample rate | 654 … 714 | 562 … 622 |
| Sidechain pair | 730 … 814 (segments 756, 784) | 638 … 722 (segments 664, 692) |
| Refs | 820 … 892 | 728 … 756 |
| Freeze | 898 … 982 | 762 … 790 |
| Reset curves | 988 … 1104 | 796 … 824 |
| Divider | 1116 | 836 |
| Meters | 1128 … 1156 | 848 … 876 |
| Settings | 1160 … 1188 | 880 … 908 |

The header does not depend on the meters: the same with meters collapsed. Smallest clear gaps: 42 px (Presets to sample rate) at 1200, 14 px at 920. Nothing overlaps.

#### 17.17.2 Control bar and plot

Unchanged from §4.3 and §4.1 at both sizes, meters shown or collapsed. The data rect is (48, 56, 840, 564) at 1200 × 700, (48, 56, 560, 404) at 920 × 540, (48, 56, 1136, 564) at 1200 × 700 with meters collapsed.

#### 17.17.3 Meter strip (editor coordinates; strip x = W − 296, so 904 at 1200 and 624 at 920)

Header rows: Metering (916, 56, 128, 28), Style (1050, 56, 92, 28), Reset (1160, 56, 28, 28), Integration (916, 90, 160, 28), Sections (1082, 90, 106, 28) at 1200; at 920 subtract 280 from each x. Content: y 130 … 688 at 1200 (HRC 558), y 130 … 528 at 920 (HRC 398).

Bars column with the bars shown: tracks y 144 … 672 (528 px) at 1200 and 144 … 512 (368 px) at 920. Columns (x, editor): bars 916 … 996 and right column 1012 … 1188 at 1200; 636 … 716 and 732 … 908 at 920. Bars hidden: right column 916 … 1188 / 636 … 908. Right column empty: bars 916 … 1188 / 636 … 908 with 118 px bars.

Right column, for each set of visible sections (editor y; "d" = divider y):

| Visible | 1200 × 700 | 920 × 540 |
|---|---|---|
| Statistics, loudness, correlation | rowH 28; table 130 … 398; d 429; loudness 460 … 558; d 589; correlation 620 … 688 | rowH 20; table 130 … 326; d 335; loudness 344 … 442; d 451; correlation 460 … 528 |
| Statistics, loudness | rowH 28; table 130 … 398; d 494; loudness 590 … 688 | rowH 28; table 130 … 398; d 414; loudness 430 … 528 |
| Statistics, correlation | rowH 28; table 130 … 398; d 509; correlation 620 … 688 | rowH 28; table 130 … 398; d 429; correlation 460 … 528 |
| Loudness, correlation | loudness 130 … 228; d 424; correlation 620 … 688 | loudness 130 … 228; d 344; correlation 460 … 528 |
| Statistics | rowH 28; table 130 … 398 | the same |
| Loudness | 130 … 228 | the same |
| Correlation | 130 … 198 | the same |
| none | right column empty: wide bars (above) | the same |

With the bars hidden as well, each case keeps its y values and uses x 916 … 1188 (636 … 908). With every section hidden the strip is collapsed (§17.10.6). All 16 combinations at both sizes are therefore: 8 right-column cases × bars shown or hidden; no section overlaps another or the header rows (the smallest gap is 12 px, the header ends at y 118 and the content starts at y 130).

Tick steps on the resolved tracks: dBFS, dBFS+3, LUFS and LU 6 dB (528 px: 8.0 px/dB; 368 px: 5.6 px/dB); dBFS 30 2 dB (17.6 and 12.3 px/dB); dBFS 15 1 dB (35.2 and 24.5 px/dB); K and cal modes 4 K.

#### 17.17.4 Legend widths

Estimates with Inter 500 11 px (about 6.0 px per character), each entry 21 px plus its name, 10 px between entries, 16 px padding:

| Case | Row widths | 1200 × 700 limit | 920 × 540 limit |
|---|---|---|---|
| Default (primary, RT max, Sidechain 1 "SC 1") | 1 row, about 211 px | 824 (606 with the zoom panel, 374 with the FROZEN badge in row 0) | 536 (336 with the panel, 234 with the badge) |
| Widest live: "Main · Stereo", "Main · Right", "All-time max", two 16-character labels | about 563 px: 1 row at 1200 without the panel; 2 rows with the panel or the badge; at 920: 2 rows (3 with the badge) | | |
| Everything: the widest live set, four 16-character references and the ALIGN tag | 2 rows at 1200, up to 5 rows at 920 with the badge (row 0 alone limited to 234 px) | | |

The deepest legend (5 rows) ends at y = 64 + 4 × 22 + 20 = 172 at 920 × 540, clear of the hint line (y 436). It stays left of the zoom panel and of the badge by the width rule.

#### 17.17.5 Popovers

| Popover | 1200 × 700 | 920 × 540 |
|---|---|---|
| Settings (560 × 400) | (628, 48) … (1188, 448) | (348, 48) … (908, 448) |
| About (560 × 440) | (628, 48) … (1188, 488) | (348, 48) … (908, 488) |
| Curves panel (560 × 320) | (628, 48) … (1188, 368) | (348, 48) … (908, 368) |
| Save as (280 × 112) | (452, 40) … (732, 152) | (452, 40) … (732, 152) |
| Confirmations (280 × 96) | (452, 40) … (732, 136) | same |
| Colour-preset dialogs (280 × 112 or 96), below the colour preset button | panel x + 120, panel y + 312: (748, 360) … (1028, 472) | (468, 360) … (748, 472) (fits: 48 + 312 + 112 = 472 ≤ 540) |

### 17.18 Performance (M3c)

Everything here follows the T-029 rules (§12, §12.1): no polygons per frame beyond the existing strokes, one `fillRectList` per colour, cached static parts, only what is under the clip.

- **Curve layers.** A layer is one curve drawing operation in a frame: the primary fill, the primary stroke, the underlay stroke, each secondary stroke, each sidechain fill (solid or hatched), each sidechain edge, and each visible reference. The maximum is 13 (2 + 1 + 2 + 2 + 2 + 4). The band tint and edges are not layers (≤ 2 + 2 rects).
- **Adaptive pacing** (on): the spectrum repaints at `max (30, cap / 2)` fps when the physical data rect exceeds 2.0 Mpx or when the layer count is at least `pacing.layerThreshold` (**7**, the T-029 value, which equals the old seven-layer worst case). T-054 and T-055 measure the M3c worst cases and may raise the threshold; the measured value goes into `tokens.json` and this paragraph. **Off:** no halving; the cap alone applies. Hover, gestures, the band and toasts always respond within one editor update.
- **Frame-rate cap:** the editor's update limiter runs at `min (frameRate, measured display refresh)`. The numeric meter text stays at 10 Hz at every cap. Budgets are gated at 60 fps; higher caps are measured and reported (plan).
- **Sidechain 2 hatch:** one `fillRectList` of one-logical-pixel bars at every 3rd column, built only under the clip; cheaper than the solid fill.
- **References:** each visible slot is one `fillRectList` of column spans (§17.4 thickness, its dash pattern applied by skipping columns), built only under the clip from the slot's grid data mapped through the view. No path, no cache needed.
- **Anti-alias off:** every stroke (primary, underlay, secondaries, sidechain edges) is drawn as pixel-snapped column spans: per physical column, one rect from `min (y, yPrev)` to `max (y, yPrev)` plus the stroke thickness `max (1, round (width × scale))` physical px, one `fillRectList` per colour. Fills are unchanged.
- **Band solo:** the tint (≤ 2 alpha rects, one list) and the edges (≤ 2 rects solid, or dash rects, one list) repaint with the plot. When only the band moves (the pointer moves, the frame does not), repaint the union of the old and new band edges and tint boundary columns plus the hover card, not the whole data rect.
- **Hover card:** up to 18 strings (band block 3, frequency 2, up to 9 rows of name and value). Names and the band tag come from the label-image cache; values are drawn with `drawText` (≤ 12 per frame), within the §12.1 per-frame text rule.
- **Meters:** Density ≤ 11 rect lists per paint (§17.10.5); the zone rail, scale, ticks and lamp backgrounds are in the cached static image, rebuilt on mode, size, section or scale-factor change. The meter repaint follows the T-029 rule of repainting only when a bar's physical pixel value changes.
- **Colour wheel:** cached ring and square images (§17.14.3); a colour change re-derives the curve colours once and repaints the plot once per editor update.

### 17.19 Accessibility (M3c)

See also §10.4.
- **Text contrast:** every new text uses existing token pairs from §10.1 and §10.2a. New pairs: the SOLO tag `text-on-accent` on #E8ECF3 16.4 : 1; the BAND tag `text-secondary` on `surface-3` 6.3 : 1; the primary button `text-on-accent` on `accent` 11.5 : 1; the Delete button `text-on-accent` on `state-error` 6.1 : 1; the status line `text-primary` on `surface` 14.9 : 1; the info line and notes `text-tertiary` on `surface` 4.5 : 1; the ALIGN legend tag `text-secondary` on the legend fill (`bg-plot` at 85 %) at least 7.7 : 1; the warn-state values `state-warn` on `surface-2` 9.8 : 1.
- **Non-text contrast** (≥ 3 : 1 on `bg-plot`): every default role colour (§17.4 table, 7.2 to 13.9); the sidechain edges 4.0 and 6.0; the derived secondaries 5.2 and 5.1 (and ≥ 3.0 by the derivation rule for user colours); the band edges 16.4 (solo) and 6.3 (preview, 60 %); the current-level notches 16.4. The density levels carry information by relative brightness and are read against the scale labels, like a heat map; the low levels are deliberately dim, so no contrast minimum applies to them. Section-toggle icons: `accent` on `accent-tint` 8.0 : 1 (shown), `text-tertiary` on `surface-2` 4.1 : 1 (hidden, plus the slash).
- **Colour-vision:** §17.4. Every curve type differs by style; the solo state by text, line style and tint strength; section toggles by the slash; the modified preset by "*"; the warn and error messages by their icon and text.
- **Hit targets** (≥ 24 × 24): Channel segments 28 to 52 × 24; Sidechain segments 28 × 24; Style segments 36 and 52 × 24; section toggles 24 × 24; Reset statistics 28 × 28; icon-only header buttons 28 × 28; Presets 96 to 160 × 28; panel buttons ≥ 44 × 24; the visibility toggle 24 × 24; the colour swatch's hit area 24 × 24; tabs ≥ 64 × 28; the hue ring 16 px thick plus 8 px tolerance; the square 96 × 96.
- **Accessible names** (`setTitle`) for the new controls: "Channel", "Underlay", "Settings preset", "Sidechain 1 trace", "Sidechain 2 trace", "Reference spectra and curve colours" (Refs), "Freeze display", "Reset spectrum curves" (also when icon-only), "Metering", "Meter style", "Integration time", "Release time", "Peak hold time", "RMS window", "Show level bars", "Show statistics", "Show loudness", "Show correlation", "Reset statistics", "Align 0 dB", "Solo width", "Frame rate", "Adaptive pacing", "Anti-alias", "Edit curve colours", "Main input label", "Sidechain 1 label", "Sidechain 2 label", the tabs by their text; per slot k: "Reference k visible", "Reference k colour", "Reference k name", "Reference k offset", "Take primary curve into reference k", "Take secondary curve into reference k", "Load CSV into reference k", "Save reference k as CSV", "Clear reference k"; "Export curves", "Cancel file dialog"; the colour editor names of §17.14.3, "Colour role", "Reset role colour", "Colour preset", "Reset all colours". Values are the formatted display strings.

### 17.20 What the mockup simplifies (M3c)

- **Not mocked (superseded by §18, T-065):** the Channel plus Underlay header control, the underlay curve, the per-input labels, the Sidechain 2 trace and its striped style, and the role colours tied to the underlay. The header keeps the approved M3b Mode control and the M3b SC toggle in their place (the SC toggle sits in the Sidechain-pair slot, 84 wide), and the Settings Inputs tab shows a pointer to §18. A dev-bar line says so.
- **Audio:** the mockup has no audio. The band solo shows its visual states only; "Solo" means the plugin would play the band. The band edges use a stand-in for the shared edge function (centre 20 Hz … 0.45 fs, edges 10 Hz … 0.45 fs; §15 question 7).
- **Ctrl in a browser:** the mockup reads Ctrl from key and pointer events (a browser cannot poll keys) and cancels the page zoom of Ctrl+wheel over the plot. A dev-bar checkbox "Hold Ctrl (for touch or laptops)" simulates a held Ctrl. The editor-bounds rule is checked on pointer moves; window blur stands in for focus loss.
- **Inputs:** one simulated sidechain (the approved M3b trace). A dev-bar checkbox connects or disconnects it.
- **Meters:** the snapshot is simulated. `level` is a stand-in for `MeterBank` (short integration follows the peaks, long integration approaches the RMS; rise with τ = T / ln 100, fall 20 dB per release time); the density histogram, its decay and the loudness values are approximations driven by the same fake music. The ballistics display formulas of §17.10.4 are implemented as written.
- **Align 0 dB:** the toggle and the legend tag work; the simulated spectrum is already sine-calibrated, so the curve does not move.
- **Files:** "Load" uses the browser's file picker and the real import rules of §17.12.3; "Save" and "Export curves" download a file after a 700 ms stand-in for the native save dialog, during which the pending state and Cancel are shown. The browser picker cannot fall behind a window. Dev buttons load a valid 812-row sample into Ref 1, a sample with an error on line 37 into Ref 2, and take the current curve into Ref 3. Messages go to the status line while the References tab is open and to a toast otherwise (§17.12.4).
- **Colour roles:** six generic roles (Primary, Sidechain, Ref 1 to Ref 4) instead of eight; T-065 maps them to slot colours. The wheel, hex field, contrast warning, derived secondary tone, live update and colour presets work as specified.
- **Presets and colour presets** live in memory for the page session; "Open presets folder" and "Open colours folder" show a toast.
- **Frame rate:** the mockup measures the display rate from `requestAnimationFrame` (median of 30 intervals, §15 question 10), caps the spectrum at `min (frameRate, display rate)`, applies Adaptive pacing (2.0 Mpx or 7 layers), and repaints at once for hover, gestures, the band and toasts. A dev-bar readout shows the effective rate, the cap, the display rate and the layer count. Anti-alias off is drawn with column spans as specified.
- **Minimum size:** the mockup keeps 920 × 540; T-065 sets the smaller minimum and the compact layout.
- **Tooltips:** the mockup still shows tooltips; their removal (M3d review item #18) is specified by T-065.
- **Rendering:** Canvas 2D and DOM, as §5.9.11.

### 17.21 Changes from the plan

| # | Plan | This spec | Reason |
|---|---|---|---|
| 1 | Metering mode names dBFS.30, dBFS.15, K-20 C, LUFS ML, LUFS SL, LU ML, LU SL | Chip values "dBFS 30", "dBFS 15", "K-20 cal", "LUFS M", "LUFS S", "LU M", "LU S"; menu texts spelled out ("LUFS momentary", "K-20 calibration (600 ms)") | Investigate's own wording (T-043 IP note prefers the standard words); short enough for a 128 px chip. §15 question 8 asks T-051 to use the same choice texts. |
| 2 | LUFS and LU bars show the programme loudness | One merged programme bar, single colour, no peak part and no zones, "M" or "S" under it | User decision (one value) and the request that it must not look like an L/R pair; loudness zones would imply a delivery target that the user did not choose. |
| 3 | The RMS row unit follows the mode | It does for the level modes; in the loudness modes it stays "dBFS" | The RMS row is a level statistic; loudness has its own block. The LU modes show the loudness block in LU instead (T-043 R4, partly adopted). |
| 4 | Density in every mode | Not in loudness modes; the Style control shows the warn state | The histogram is of the level, not of loudness, so it would show a different quantity than the scale. |
| 5 | Density "as brightness" | 7 quantised brightness levels of one hue, a zone rail, level notches | Fits the T-029 rule (≤ 11 rect lists) and keeps the zones visible. |
| 6 | Release applies to the peak part and the hold tick | Also to the TP row's hold; peak hold 0 means no tick | One consistent ballistics model; "0 ms hold" has no visible meaning otherwise. |
| 7 | Controls for the metering parameters (location open) | Metering, Style and Integration in the strip; Release, Peak hold and the RMS window in Settings > Meters | The strip is 296 px wide; Integration is the one the user must find (user decision). The RMS window leaves the strip, which changes the approved T-010 strip. |
| 8 | Section-hiding control (open) | Four icon toggles in the strip's second header row, plus Settings fields | Visible, one click, no context menu (§5.6). |
| 9 | Every section hidden: behaves as collapsed | And the Meters toggle then restores all four | Otherwise the user could not get the sections back from the header. |
| 10 | Header Mode becomes Channel plus Underlay | Plus a compact header mode below 1174 px with icon-only Refs, Freeze and Reset curves | The new items do not fit at 920 px otherwise. |
| 11 | Colour editor entry (open) | In the curves panel (Refs button) next to the references, and from Settings > Display | One header entry for both curve features; the header has no room for two more buttons at 920 px. |
| 12 | Secondary curves "a darker tone" | HSV value × 0.68, with the default tokens kept exactly when the role is at its default, and a 3 : 1 floor | Keeps the first launch pixel-identical (T-054 acceptance) and the secondaries visible for user colours. |
| 13 | Reference curves "line style with no fill" | Dashed column spans with one dash pattern per slot | The only line style that differs from every live curve without hue, and drawable without polygons. T-043 R17's global fill switch is not adopted. |
| 14 | Align 0 dB control (open) | Settings > Analysis plus an "ALIGN 0 dB" legend tag | The control bar is full at the default size (§4.3). |
| 15 | `soloWidth` step (open) | An 11-value ladder, Shift for continuous steps | Musically useful widths in few notches. |
| 16 | Band edges clamped at 10 Hz … 0.45 fs (plan) and centre clamped at 20 Hz … 0.45 fs (T-044) | The editor draws whatever the shared DSP function returns | The two sources differ; the drawing must match the audio (§15 question 7). |
| 17 | Legacy pair written "on the next user edit" | Written as one gesture group: `underlay`, then `channelMode` | Undo reverts the whole edit in one step (T-055 acceptance). |
| 18 | Labels in the legend (format open) | "Main · Mid" for the main input's curves; the plain label for sidechains | The main label needs a place, and the channel name stays visible. |
| 19 | Note names below C0 (format open) | Negative octaves down to C−1, "below C−1" under 7.94 Hz | MIDI note 0 is a common lower bound; lower names have no musical use. |
| 20 | Reference slot panel (layout open) | A 560 × 320 popover shared with the colour editor; Clear needs a second click | Same popover family as Settings; Clear is the only destructive slot action. |

## 18. M3 review and spectrum slots (T-065)

Source: `docs/plans/m3-review-spectrum-slots.md` (the source of truth for behaviour; "the slot plan" below), `docs/plans/m3-user-review-findings.md` (findings #1 to #22, quoted as "#n"), `docs/research/T-061-review-bug-diagnosis.md` §3 (#16), `docs/research/T-043-span-m3c-reference.md`, and the user decisions of 2026-10-06. Values named `token.*` are in `tokens.json` 1.3.0.

- **§18.1 is milestone M3d** and is implemented by T-066 on the current editor (the M3b layout of §4 to §9). It is self-contained: T-066 needs nothing else from §18.
- **§18.2 onwards is milestone M3c** (T-054 to T-058, T-069, T-070). Where §18 and §17 disagree, §18 wins. Every §17 subsection marked "Superseded by §18" is replaced here; §18.14 lists each replacement.

### 18.1 M3d visual fixes (T-066)

Applies to the current editor. Reference sizes: **1200 × 700** (default) and **920 × 540** (the minimum until T-070; the M3c minimum of §18.9 is also resolved where a rule depends on size). Nothing here changes a parameter definition; the M3d parameter changes (`dbLow`, `dbHigh`, `smoothing`) are T-064's.

#### 18.1.1 Tooltips removed (#18)

**Rule.** The editor shows no tooltips. No `juce::TooltipWindow` is created and no component calls `setTooltip`. Nothing appears after the pointer rests on a control.

**Kept:** the spectrum hover readout of §5.5: the crosshair, the markers, the hover card and the hint line. The hint line is part of the readout: it has no delay, belongs to the plot rather than to a control, and is the only place the spectrum gestures are written down (§15 question 16 asks the user to confirm this).

**Accessible descriptions.** Every removed text that explains a control becomes that control's accessible description (`setDescription`), so a screen-reader user still hears it. It is never drawn.

**Removed tooltips, and what replaces information that only a tooltip carried:**

| Where | Removed (spec section) | Information only the tooltip carried | Replacement |
|---|---|---|---|
| Control-bar chips Block, Overlap, Window, Avg time, Smooth, Slope, 2nd curve | Description and the how-to line (§6 "Tooltips") | Block: the unit (samples), the block duration and the bin spacing at the current sample rate | The Block menu shows them per item: "4096" followed by the hint "85.3 ms · 11.7 Hz" in `text-tertiary` (§7.1 menu hint style, 6 px after the value), computed at the current sample rate with the §5.5 frequency format and ms with 1 decimal. The how-to lines need no replacement: every chip behaves the same (§7.1). |
| Frequency and Level chip halves, and their Settings fields | Description and the "Effective …" lines (§5.4) | The effective value and its reason | §18.1.5: the half shows the effective value in `state-warn` with an underline; the reason goes into the accessible value. |
| Header Mode segments | Channel definitions (§17.3 texts, M3b `MODE_TIPS`) | none (standard terms) | none |
| SC toggle | Description; no-input text "No sidechain input is connected. Route a track …" (§9) | The no-input state | The no-input look of §18.1.3 (dim icon and hollow ring) |
| Freeze, Reset curves, Meters toggle | Descriptions | Meters toggle: "all sections hidden" (M3c, §17.10.6) | The toggle's collapsed look already shows the state. |
| Settings button | "All settings" / "Some controls … are only here" | What the accent dot means | The dot keeps one meaning: the popover holds at least one setting (§18.1.4). |
| Meter strip: Scale chip, RMS chip, Reset statistics | Descriptions | none | none |
| Correlation block | Window lengths (500 ms, 3 s); mono text "Correlation and balance need two channels." (§8.5, §8.6) | The mono reason | Mono already shows "—" and draws the block at 40 % (§8.6). |
| Zoom panel: Fit, range rows, bin-spacing row, overview strip | Gesture help (§5.9.6 "Tooltips") | Alt+scroll zooms level: it was only in this tooltip (§5.5 hint line) | Gutter hints (below). |
| Settings popover fields | Descriptions | none | none |
| Every M3c control of §17 | All §17 tooltip texts | Each is listed with its replacement in §18.14 | — |

**Gutter hints** (new, replace the zoom-panel gesture help): while the pointer is over the dB gutter (§5.9.3), the hint line shows "Scroll: zoom level"; over the frequency gutter it shows "Scroll: zoom frequency". Same style and position as the §5.5 hint line (bottom-right of the data rect). It hides when the pointer leaves the gutter. At Fit and while zoomed the gutter texts are the same.

**Tokens and sections made void:** `motion.tooltipDelayMs` is deprecated (tokens 1.3.0); `surface-3` loses its "tooltip fill" role and `body-sm` its "tooltips" use. The tooltip texts in §5.4, §5.9.6, §6, §7.1 (hover column), §8.5, §8.6, §9 and §17 no longer describe drawn text; where they say "the tooltip starts with the effective value", §18.1.5 applies.

#### 18.1.2 Sample-rate label removed (#19)

- The header text "48 kHz" (§4.2 row "Sample rate text", §8.8) is removed. Nothing moves into its place in M3d; §18.1.3 moves the SC toggle right by 32 px.
- **The sample rate is not shown as a live label anywhere.** It still drives the Nyquist limit (§5.4, §18.1.5), the Block menu hints (§18.1.1) and the bin-spacing row of the zoom panel (§5.9.6), and it is part of a stored reference's info line from M3c ("Taken 14:32 · 48 kHz · …", §17.11.1), which describes a capture, not the session. The About view does not show it.
- `MeterSnapshot.sampleRate` keeps every other use (§13.2 row "sampleRate": "Header text" is void).

#### 18.1.3 Sidechain button (#10, #11)

- **Rect:** (W − 336, 8, 28, 28), an icon-only toggle: `icon-sidechain-16` centred, at (W − 330, 14). It sits 6 px before Freeze (W − 302). The "SC" text and the 60 px width are gone. Resolved x: 864 at 1200 × 700, 584 at 920 × 540.
- **Colour:** when on, the button uses the level-meter turquoise (`meter.safe`, #34C6A8) instead of `accent`. Tokens: `toggle-teal` #34C6A8 and `toggle-teal-tint` #142F30 (`toggle-teal` at 16 % over `bg-app`, pre-blended).

| State | Fill | Border | Icon |
|---|---|---|---|
| Off | transparent | none | `text-secondary` |
| Off, hover | `surface-3` | none | `text-primary` |
| On | `toggle-teal-tint` | 1 px `toggle-teal` at 50 % | `toggle-teal` |
| On, hover | `surface-3` | 1 px `toggle-teal` at 50 % | `toggle-teal` |
| Pressed (either) | `surface-3` | as the state | as the state |
| No input, Off | transparent | none | `text-disabled`, plus a 6 px hollow ring (1 px `text-disabled` stroke) at (x + 19, y + 3, 6, 6) |
| No input, On | transparent | 1 px `border` | `text-disabled`, plus the ring |
| Keyboard focus | as the state | plus the 2 px `accent` focus ring (§7.2) | |

- **Behaviour** is unchanged (§7.1): click toggles, double-click or Alt+click resets to On, wheel up = On, down = Off, Space or Enter toggles when focused. The no-input states stay clickable and keep the setting.
- **Accessible name** "Sidechain trace" (the button has no text), value "On" or "Off", description "No sidechain input is connected." while the bus is inactive.
- **Contrast:** `toggle-teal` on `toggle-teal-tint` 6.6 : 1, on `surface-3` 6.5 : 1, on `bg-app` 8.8 : 1 (all ≥ 3 : 1 for icons). Colour is not the only cue: the On state also has the border and the fill.
- **From M3c** the SC toggle is replaced by the slot list, and the slot visibility toggles follow the same teal rule (§18.3.2).

#### 18.1.4 All Settings without duplicates (#20)

**Rule:** a setting appears in the Settings popover only while its control on the main window is not on screen. Nothing appears in both places, and every setting stays reachable at every size.

| Setting | Control on the main window | In Settings when |
|---|---|---|
| Channel mode | Header Mode control | never |
| Sidechain trace | Header SC toggle | never |
| Block size, Overlap, Window, Average time, Smoothing | Control-bar group A | never in M3d (group A always fits: at 920 × 540, avail = 600 ≥ 376) |
| Slope, Secondary curve | Group B | group B is hidden (avail < 592; never in M3d at the sizes allowed) |
| Frequency low, Frequency high, Range low, Range high | Group C | group C is hidden (avail < 880) |
| Meter scale, RMS window | Meter strip | the meters are collapsed |

`avail = (W − MS) − 24` as in §4.3.

**Layout** (replaces the body of §6.1; frame, title bar, About button and Close are unchanged):
- Sections, in this order: "SPECTRUM", "DISPLAY RANGE", "METERS". A section is shown only when it has at least one row. Rows keep the §6.1 labels and 128 × 24 fields.
- The first non-empty section goes in column 1 (x 16); the others stack in column 2 (x 288), 8 px apart. A section is 20 (title) + 28 per row.
- **Height** = 36 (title bar) + 12 + the taller column + 16. With no rows, the body is one line, "Every setting is on the main window at this size.", in `body-sm` / `text-tertiary` at (16, 48, 528, 20), and the height is **84**.
- The popover is re-laid out at once when the window is resized or the meters are toggled while it is open (the cached shadow is rendered once per change).

| Size | Rows shown | Popover rect |
|---|---|---|
| 1200 × 700, meters shown | none (message) | (628, 48, 560, 84) |
| 1200 × 700, meters collapsed | METERS: Meter scale, RMS window | (628, 48, 560, 140) |
| 920 × 540, meters shown | DISPLAY RANGE: Frequency low, Frequency high, Range low, Range high | (348, 48, 560, 196) |
| 920 × 540, meters collapsed | METERS: Meter scale, RMS window (group C fits again: avail 896) | (348, 48, 560, 140) |

- **Settings dot** (§4.2): shown exactly while the popover holds at least one setting, so it now also shows while the meters are collapsed.
- **Focus order:** About, the fields in column order, Close (§6.1). The About view (§6.2) is unchanged.
- From M3c, §18.12 gives the list for the tabbed popover.

#### 18.1.5 Effective values without tooltips (#5; replaces the tooltip rule of §5.4)

When a set value differs from its effective value (§5.4, `DisplayRange.h`), the control shows the **effective** value, so what the plot shows and what the chip shows always agree.
- **Look:** the value text in `state-warn` and a 1 px `state-warn` underline, 2 px below the text baseline, as wide as the number (not the unit). The underline is the non-colour cue.
- **Cases in M3d:**
  - `freqHigh` above Nyquist: effective `sampleRate / 2`;
  - `freqLow` above `fHiEff / 2` (the one-octave rule);
  - `dbLow` at or above `dbHigh − 1` (#5): effective `dbHigh − 1`. The top half is never changed; `dbHigh` always applies.
- **Editing a clamped half:** a drag, wheel step or arrow key starts from the effective value, so the first step moves the value visibly (no dead zone). Text entry opens pre-filled with the **set** value, so the user sees what is stored. Double-click resets to the default as usual.
- **Accessible value:** the effective value, then the set value and the reason, for example "−19 dB, effective. Set to −10 dB; the bottom stays at least 1 dB below the top." The other reasons: "limited by Nyquist (24.0 kHz)" and "the range must span at least one octave".
- **Example (#5):** Level set to −10 (bottom) and −18 (top): the chip reads "−19 … −18 dB", with "−19" in `state-warn` and underlined; the grid spans −19 to −18 dB.
- **Width check:** the widest Level text, "−150 … −149 dB" (both halves 4 characters, possible now that both ranges are −150 to +30 dB), needs about 123 px in the 120 px chip. When both halves have 4 characters, the high half drops its "dB" unit (the accessible value keeps it). Every other combination fits.
- The Settings fields of the four range rows follow the same rule.

#### 18.1.6 Spectrum level grid (#4; replaces the dB step rules of §5.3 and §5.9.8)

The level-meter grid is unchanged. The frequency grid is unchanged.

- **Line step** `s`: the smallest value in **{0.1, 0.2, 0.5, 1, 3, 6, 12, 24} dB** with `s ≥ span / 24` and `s × pxPerDb ≥ 12`, where `span` is the visible dB span (the home span at Fit) and `pxPerDb = data.h / span`. 2 dB is no longer a step, so the default range uses 3 dB, as the user asked.
- **Lines** at every multiple of `s` (k × s, rounded to 6 decimals). The 0 dB line uses `grid-major`, all others `grid-minor`.
- **Labels:** on every line when `s × pxPerDb ≥ 20`; otherwise only on lines at multiples of `2s` (these are then at least 24 px apart). Format as §5.9.8: signed integers for `s ≥ 1` ("−21", "0", "+6"), 1 decimal below 1 dB ("−12.4"). The 6 px edge-skip rule of §5.9.8 is unchanged.
- Tokens: `deepGrid.dbSteps` [0.1, 0.2, 0.5, 1, 3, 6, 12, 24], `deepGrid.dbMaxLines` 24, `deepGrid.dbLineMinPx` 12, `deepGrid.dbLabelMinPx` 20 (replaces `dbStepMinPx`).

Resolved (data heights 564 at 1200 × 700, 404 at 920 × 540, 264 at the M3c minimum 560 × 400 of §18.9):

| Span (example) | 1200 × 700 | 920 × 540 | 560 × 400 |
|---|---|---|---|
| 54 dB, the new default −72 … −18 | 3 dB, 31.3 px, all labelled (19 lines) | 3 dB, 22.4 px, all labelled | 3 dB, 14.7 px; labels every 6 dB (29.3 px) |
| 96 dB, −90 … +6 (the old default) | 6 dB, 35.3 px, all labelled | 6 dB, 25.3 px, all labelled | 6 dB, 16.5 px; labels every 12 dB |
| 180 dB, −150 … +30 (widest) | 12 dB, 37.6 px, all labelled | 12 dB, 26.9 px, all labelled | 12 dB, 17.6 px; labels every 24 dB |
| 24 dB, −42 … −18 | 1 dB, 23.5 px, all labelled | 1 dB, 16.8 px; labels every 2 dB | 3 dB, 33.0 px, all labelled (1 dB is 11 px) |
| 12 dB | 0.5 dB, 23.5 px, all labelled | 0.5 dB, 16.8 px; labels every 1 dB | 1 dB, 22.0 px, all labelled |
| 6 dB (the minimum level-zoom span) | 0.5 dB, 47.0 px | 0.5 dB, 33.7 px | 0.5 dB, 22.0 px |
| 1 dB (the smallest home span, #5) | 0.1 dB, 56.4 px | 0.1 dB, 40.4 px | 0.1 dB, 26.4 px |

Guarantees: at most 25 lines; lines at least 12 px apart; labels at least 20 px apart. The 12 px floor holds at every span from 1 to 180 dB and every height from 264 px, because 24 dB lines at 180 dB are at least 35 px apart there. T-066 updates the `ZoomMath` grid test to sweep spans 1 to 180 dB at heights 264, 404 and 564 and to assert these three guarantees.

#### 18.1.7 Smoothing and level range texts (T-064 parameters)

- **Smooth** chip and menu: "Off", "1/48", "1/24", "1/12", "1/6", "1/3", "1/2", "1", "2", "3", each followed by the unit "oct" except Off (#6). The widest value is still "1/48 oct", so the 68 px chip is unchanged. The menu's "default" mark stays on 1/12.
- **Level** chip: default "−72 … −18 dB" (#4). Both halves range over −150 to +30 dB: 240 px drag for the full range, Shift × 0.1, wheel ±1 dB, text entry as §6. Double-click resets the low half to −72 and the high half to −18.

#### 18.1.8 Summary for T-066

1. Remove every tooltip; move the texts into accessible descriptions; add the gutter hints (§18.1.1).
2. Remove the header sample-rate text (§18.1.2).
3. SC toggle: icon only at (W − 336, 8, 28, 28), teal when on, states as §18.1.3.
4. Settings popover: only settings not on the main window, sections and height as §18.1.4; the Settings dot follows it.
5. Effective values in `state-warn` with an underline, editing from the effective value (§18.1.5).
6. The level grid rule of §18.1.6 (and the `ZoomMath` test sweep).
7. Smooth and Level texts (§18.1.7).
8. Tokens: `toggle-teal`, `toggle-teal-tint`, the `deepGrid` changes and the deprecated `motion.tooltipDelayMs` (tokens 1.3.0).

The mockup shows every item in its default state (§18.15).

### 18.2 Spectrum slots: model, defaults and colours (M3c)

Four identical slots (the slot plan, "Spectrum slot model"). Slot 1 is the main spectrum. The bottom control bar keeps controlling slot 1's analysis. Every slot, including slot 1, is edited in the same slot window (§18.4).

| Slot | Parameters (the slot plan's table) | Default name (`slotNameN`) | Default colour | Default source, channel, type | Visible |
|---|---|---|---|---|---|
| 1 | `channelMode`, `fftSize`, `overlap`, `window`, `avgTime`, `smoothing`, `slope`, `s1Fill` | "Main" | #62D6F0 (today's primary) | Main (locked), Mid, Real-time (locked) | always (slot 1 has no visibility parameter) |
| 2 | `s2Source`, `s2Channel`, `secondary` (Type), `s2Link`, `s2FftSize` … `s2Slope`, `s2Fill`, `s2Visible` | "Spectrum 2" | #3E8FA3 (today's secondary tone) | Main, Mid, RT max, linked | on |
| 3 | `s3Source`, `s3Channel`, `s3Type`, `s3Link`, `s3…`, `s3Fill`, `showSidechain` (Visible) | "SC 1" | #A58BFF (today's sidechain) | Sidechain 1, Stereo, Real-time, linked | on |
| 4 | `s4Source`, `s4Channel`, `s4Type`, `s4Link`, `s4…`, `s4Fill`, `showSidechain2` (Visible) | "SC 2" | #8FD96B (the §17.4 Sidechain 2 colour) | Sidechain 2, Stereo, Real-time, linked | on |

- **Colours** are global preferences (`GlobalSettings`), edited in the curves panel's Colours tab (§17.14), whose role list now has eight rows: Spectrum 1 to 4 (labelled with the slot names) and Ref 1 to 4. They replace the roles `primary`, `underlay`, `sc1` and `sc2` of §17.4. The derived secondary tone of §17.4 is no longer used for live curves (a held type is drawn in the slot colour, §18.5); the colour editor's "Secondary tone" line is removed.
- **Defaults look like today:** slot 1 is the aqua line with its gradient, slot 2 is the thin dark RT max line, slot 3 is the lavender sidechain silhouette, and slot 4 appears only when a second sidechain is connected (§18.5).
- **Names** (`ViewState`, at most 16 characters) are edited in the Spectra list (§18.3), with the text-field rules of §17.5: further typing refused with a 300 ms `state-error` flash, control characters removed, spaces trimmed, an empty field restores the default, no host gesture and no undo.
- **Input state per slot**, used everywhere below:
  - **active**: Type is not Off and the slot's source is connected (`isSidechainActive` for sidechain sources; Main is always connected);
  - **off**: Type is Off (slots 2 to 4 only);
  - **no input**: Type is not Off and the source is a sidechain that is not connected. A host that never offers the bus and an unconnected bus look the same (§17.6).
- **Drawn** means active, Visible on, and holding data. Only drawn slots have a curve, a legend entry and a hover row.

### 18.3 The Spectra list

#### 18.3.1 Where it lives

The Spectra list has two parts:
- **A header group** with the list button and a visibility toggle per slot 2 to 4 (§18.9.2 gives its rect per width). It replaces the M3d SC toggle and is the one-click way to show or hide a slot.
- **The Spectra panel**, a popover opened by the list button. It holds one row per slot with every per-slot control except the analysis settings, which are in the slot window (§18.4).

**Header group (full form, W ≥ 900):** track `surface-2`, radius 6, padding 2, rect (116, 8, 164, 28).
- List button (118, 10, 76, 24): "Spectra" in `body`, `text-secondary`, at x + 8, and `icon-chevron-down-16` at x + 56. Hover `surface-3` with `text-primary`. While the panel is open: `accent-tint` fill, `accent` text.
- Visibility toggles for slots 2, 3 and 4 at (196, 10), (224, 10), (252, 10), each 28 × 24. Each shows the slot number in `body` weight 600, centred 2 px above the middle, and a 12 × 2 bar in the slot colour at (x + 8, y + 19).

**Compact form (W < 900):** rect (52, 8, 104, 28). The list button is a 28 × 24 icon segment (54, 10) with `icon-spectra-16`; the toggles are 24 × 24 at x 82, 106 and 130, with the number and a 10 × 2 colour bar at (x + 7, y + 19).

**Toggle states** (the teal rule of §18.1.3, extended):

| State | Fill | Border | Number | Colour bar |
|---|---|---|---|---|
| Visible, active | `toggle-teal-tint` | 1 px `toggle-teal` at 50 % | `toggle-teal` | slot colour |
| Hidden, active | transparent | none | `text-secondary` | slot colour |
| Hover (either) | `surface-3` | as the state | as the state, or `text-primary` when hidden | slot colour |
| Off (Type Off) | transparent | none | `text-disabled` | none |
| No input, visible | transparent | 1 px `border` | `text-disabled` | none; a 4 px hollow ring (1 px `text-disabled`) at the top-right, inset 3 |
| No input, hidden | transparent | none | `text-disabled` | none; the ring |

Click toggles Visible (`s2Visible`, `showSidechain`, `showSidechain2`); double-click or Alt+click resets to On; wheel up = On, down = Off; Space or Enter toggles when focused. Off and no-input toggles stay clickable and keep the setting. Accessible names "Show {slot name}" with the value "On" or "Off" and the description "Off", "No input" or nothing.

#### 18.3.2 The Spectra panel

A popover in the §6.1 style (fill `surface`, 1 px `border`, radius 8, cached shadow), **480 wide**, at x = clamp (group x, 12, W − 12 − 480), y = 48. It is one of the mutually exclusive popovers (§18.12): opening it closes Settings, the curves panel and the slot window.

Panel-relative layout:

| Item | Rect | Content |
|---|---|---|
| Title | (16, 0, 200, 36) | "Spectra", `body` weight 600 |
| Close | (448, 4, 28, 28) | as Settings |
| Title divider | (0, 36, 480, 1) | `border` |
| Column header | y 44, 16 high | `caps`, `text-tertiary`: "SHOW" at x 12, "NAME" at 70, "SOURCE" at 214, "TYPE" at 322 |
| Slot row k (k = 1 … 4) | y = 64 + 36 (k − 1), controls 24 high | below |
| Offset row (only while the header does not show the Offset chip, W < 900) | divider (12, 208, 456, 1); label "Offset" (`body`, `text-secondary`) at (12, 220, 96, 24); Offset field (214, 220, 112, 24) | §18.7 |

Height: 208 (rows end at 196, plus 12), or 256 with the Offset row. Resolved: (116, 48, 480, 208) at 1200 × 700; (52, 48, 480, 256) at 560 × 400.

Row controls (x, width; 24 high):

| Control | x, w | Slots 2 to 4 | Slot 1 |
|---|---|---|---|
| Visibility | 12, 28 | Icon toggle `icon-eye-16` / `icon-eye-off-16` with the toggle states above (teal when visible) | `icon-lock-16` in `text-tertiary`, not interactive; accessible text "Main is always shown" |
| Swatch | 44, 20 (drawn 20 × 10 at y + 7; hit area 42 … 66 × 24) | The slot's curve style (§18.5) in the slot colour. Click opens the curves panel on the Colours tab with this slot selected. | same |
| Name | 70, 136 | Text field (§17.5 rules), placeholder-free; the default name when empty | same |
| Source | 214, 100 | Choice field: "Main", "Sidechain 1", "Sidechain 2". No input: the value in `state-warn` with the §18.1.5 underline (the set value is shown, because a choice has no other effective value) | "Main" as plain `text-secondary` text with `icon-lock-16` at the right, not interactive |
| Type | 322, 112 | Choice field: "Off", "RT max", "All-time max", "All-time avg", "Real-time" (parameter order), "default" mark on the slot's default | "Real-time" with the lock, not interactive |
| Settings | 440, 28 | Icon button `icon-settings-16`: opens the slot window on this slot (§18.4) | same |

Choice fields are the §6.1 inline fields (fill `surface-2`, radius 4, `body`), with the chip conventions of §7.1 (drag, wheel, click opens the menu after the 250 ms defer, double-click resets). Accessible names: "{slot name} visible", "{slot name} colour", "Spectrum k name", "{slot name} source", "{slot name} type", "{slot name} settings".

### 18.4 The slot window

**One component for all four slots**, an in-editor popover, never an OS window. Reasons: plugin-owned top-level windows behave differently per host (focus, z-order, FL Studio's wrapper, T-061 §3), they can fall behind the host window like file dialogs (T-044 Q5), and every other panel of the editor is already a popover. The editor's minimum size holds the window (below).

- **Opening:** the gear of a Spectra-list row (that slot), or the **slot-1 gear** at the left end of the control bar (§18.9.3), which opens it on slot 1. Opening it closes the other popovers.
- **Frame:** popover style, **520 × 312**, at (W − 532, 48). Resolved: (668, 48) … (1188, 360) at 1200 × 700; (28, 48) … (548, 360) at 560 × 400.
- **Closing:** Esc, a click outside, Close. The back button returns to the Spectra panel.

Popover-relative layout:

| Item | Rect | Content |
|---|---|---|
| Back | (4, 4, 28, 28) | The §6.2 back chevron; opens the Spectra panel |
| Title | (36, 0, 300, 36) | "Spectrum settings", `body` weight 600 |
| Close | (488, 4, 28, 28) | as Settings |
| Slot tabs | y 37, 28 high, at x 16, 140, 264, 388, each 116 wide | A 10 × 10 swatch in the slot colour at x + 4, then "k  {name}" in `body`, truncated with "…". Selected: `text-primary` with the 2 px `accent` bottom line (§17.11.1 tab style); others `text-secondary`. A slot that is off or has no input shows its text in `text-tertiary`. |
| Tab divider | (0, 68, 520, 1) | `border` |
| Column 1 | x 16, 236 wide, from y 80 | "INPUT" (section title 20 high), then rows of 28: label (`body`, `text-secondary`) left, 128 × 24 field at x + 108 |
| Column 2 | x 268, 236 wide, from y 80 | "ANALYSIS", then rows |
| Note | (16, 248, 236, 48) | `body-sm`, `text-tertiary`, wrapping; text below |

| Row | Column 1 (INPUT) | Column 2 (ANALYSIS) |
|---|---|---|
| 1 | Source | Link ("Linked to Main"): On/Off field. Slot 1: the text "Reference for linked spectra" in `text-tertiary` instead of a field |
| 2 | Channel: "Mid", "Side", "Left", "Right", "Stereo" | Block size |
| 3 | Type | Overlap |
| 4 | Show (Visible). Slot 1: "Always" with the lock | Window |
| 5 | Fill: "On" / "Off" | Average time |
| 6 | | Smoothing (up to "3 oct", §18.1.7) |
| 7 | | Slope |

Column 2 ends at y 296; the height is 312. Formats, drag, wheel and text entry of every analysis field are those of the slot-1 chips (§6, §18.1.7); slot 2 to 4 fields bind `sNFftSize` … `sNSlope`.

**Locked fields (slot 1):** Source "Main" and Type "Real-time" are drawn as plain `text-secondary` text with `icon-lock-16` (`text-tertiary`) at the right of the field rect, no fill, not focusable. Their accessible names say "locked".

**Link** (slots 2 to 4):
- **While on**, the six analysis fields show **slot 1's current values**, live. Linked look: no fill, a 1 px `border` outline, `icon-link-16` in `text-tertiary` at the field's left (x + 4), the value in `text-secondary`. The note reads "Linked to Main: these values follow the main spectrum. Editing one unlinks this spectrum."
- **Editing a linked field** (a drag past 4 px, a wheel step, an arrow key, a menu choice or a committed text entry) turns Link off first. In one gesture group the editor writes, in this order: `sNLink` = Off; the slot's six analysis parameters set to slot 1's current values; then the edited parameter. So nothing on screen jumps except the edited value, and the fields take the normal look. A click that only opens a menu, and a cancelled text entry, change nothing.
- **Turning Link on** shows slot 1's values again. The slot's own values are kept for when Link is turned off.
- **While off**, the fields look like every other field, and the note reads "Not linked: this spectrum uses its own analysis settings."
- Slot 1's note: "The control bar sets these values too."

**Effective values** (§18.1.5): the Channel field of a slot whose source is mono shows "Mono" in `state-warn` with the underline (accessible: "Mono, effective. Set to Stereo; the source has one channel."). The Source field of a no-input slot shows its set value in `state-warn` with the underline.

**Keyboard:** Tab cycles back, the slot tabs, column 1, column 2, Close; Ctrl+Tab and Ctrl+Shift+Tab switch slots. Focus is trapped as §6.1.

### 18.5 Curve styles, Fill and the no-input state

Line style follows the **type**, fill follows the **source** and **Fill**, and the hatch follows the **slot** (slot 4 only), so every pair of drawn slots differs by at least one non-hue cue, or by name when two slots have the same type and source (as the four references do, §17.4).

| Type and source | Line | Fill off | Fill on |
|---|---|---|---|
| Slot 1 (Real-time, Main) | 1.5 px slot colour, round joins | Vertical gradient of the slot colour, 30 % at the data top to 4 % at the bottom (today's look) | Flat slot colour at **40 %** to the data bottom |
| Real-time, Main source (slots 2 to 4) | 1.5 px slot colour | none | Flat slot colour at 40 % |
| Real-time, sidechain source | 1 px slot colour at 70 % (the edge) | Flat slot colour at 26 % to the data bottom (the silhouette, today's sidechain) | Flat slot colour at 40 % |
| RT max, All-time max, All-time avg (any source) | 1 px slot colour, opaque | none | Flat slot colour at 40 % |

- **Slot 4's fills are hatched** instead of flat: a 1 logical px bar at every 3rd logical column (`floor (x − data.x) mod 3 = 0`), at 40 % for the silhouette and 70 % with Fill on (§17.4 hatch). This keeps the default SC 1 and SC 2 silhouettes apart without hue.
- **Draw order** (back to front): the fills of slots 4, 3, 2, 1; the reference curves (§17.11.2); the lines of slots 4, 3, 2; the slot 1 line. The band tint and the overlays follow §17.9.3.
- **Defaults** reproduce today: slot 1 aqua line and gradient; slot 2 1 px #3E8FA3 RT max line; slot 3 lavender silhouette; slot 4 a green hatched silhouette when SC 2 is connected.
- **Swatches** (legend, hover card, Spectra panel, colour list) draw the same style in 16 × 10 (20 × 10 in the panel): a 2 px top line plus the fill below it.
- **No input, Off, hidden:** nothing is drawn, and there is no legend entry or hover row. The header toggle and the Spectra row show the state (§18.3.1). The DSP decides when a slot is active; the editor draws from the frame's per-slot flags.
- **Colour vision:** the default slot colours keep the §17.4 values (contrast on `bg-plot` 11.5, 5.2, 7.2 and 11.4 : 1). The cues without hue: slot 1 is the only line with a gradient; real-time Main lines are 1.5 px, held types 1 px; sidechain silhouettes are fills with a dim edge; slot 4 is hatched; references are dashed. Two slots with the same type and source differ by name in the legend and hover card.
- **T-043 R17** (one fill switch for every reference): per-slot Fill covers the user's request (#7) for every live curve; references stay line-only (§17.4), so they remain the only dashed, unfilled curves. Per-slot Fill therefore covers R17's purpose, and it is not an open question.
- **Performance:** at most 4 fills (gradient bands for slot 1, one `fillRectList` per other fill; hatches as column lists), 4 lines, 4 references: 12 layers, within the §17.18 count (pacing threshold 7 unchanged until T-054 and T-055 measure).

### 18.6 Legend, hover card and the legend collision rule

**Legend entries** (in order; each only while drawn; flow, pill style and the 25 % fade of §17.5 unchanged):
1. Slot 1: "{name} · {channel}", for example "Main · Mid"; "Mono" as the channel with a mono main input.
2. Slots 2 to 4: "{name}", plus " · {type}" when the type is not Real-time, for example "Spectrum 2 · RT max", "SC 1".
3. References, by slot name (§17.11).
4. Tags (text only, `caps`, `text-secondary`): "ALIGN 0 dB" (§17.8), then "OFFSET: NORMALIZE", "OFFSET: CENTER" or "OFFSET: {SLOT NAME}" while `offsetMode` is not Off (§18.7).

Default legend: "Main · Mid", "Spectrum 2 · RT max", "SC 1" (about 297 px, one row at 1200 × 700).

**Collision rule** (replaces the row rule of §17.5; it holds at every size from 560 × 400 up):
- **Obstacles:** the FROZEN badge while frozen, and the zoom panel while zoomed, each inflated by 8 px.
- **Rows:** row k is the band y = data.y + 8 + 22k to y + 20. Its width limit is from `data.x + 8` to the left edge of the leftmost obstacle that overlaps the band vertically, or to `data.right − 8` if none does.
- **Placement:** entries flow in order. An entry that does not fit in the rest of the current row starts the next row. **If an entry does not fit even in an empty row, that row is left empty and the entry tries the next row**, so the legend steps down past the badge or the panel instead of running under it. An entry wider than the full data width is truncated with "…".
- **Limit:** no row may end below `data.bottom − 40` (clear of the hint line). Entries that would need such a row are dropped, and the last visible row ends with "+n" (`caps`), where n is the number of dropped entries.
- **Badge position:** centred on the data rect as §5.7, except while the zoom panel shows and the badge would intersect it inflated by 8 px: then the badge ends 8 px left of the panel (badge.right = panel.x − 8).

Resolved (badge 63 px wide; zoom panel per §5.9.6):

| Size and state | Badge x | Panel x | Legend rows |
|---|---|---|---|
| 1200 × 700, frozen | 436.5 … 499.5 | — | Row 0 limit 428.5 − 56 = 372.5 px; the default legend (297 px) fits in row 0. No overlap. |
| 1200 × 700, frozen and zoomed | 436.5 … 499.5 | 670 … 880 | as above |
| 920 × 540, frozen (the T-037 case: 23.5 px overlap today) | 296.5 … 359.5 | — | Row 0 limit 288.5 − 56 = 232.5 px: "Main · Mid" (97 px); adding "Spectrum 2 · RT max" would need 242 px, so it starts row 1 with "SC 1" (206 px). No overlap. |
| 920 × 540, frozen and zoomed | 296.5 … 359.5 | 400 … 600 | as above (the panel is right of the badge) |
| 560 × 400 (compact, data 376 px), frozen and zoomed | pushed left: 145 … 208 | 216 … 416 | Row 0 limit 137 − 56 = 81 px: "Main · Mid" (97 px) does not fit, so row 0 stays empty. Rows 1 to 3 are limited by the panel (208 − 56 = 152 px): row 1 "Main · Mid", row 2 "Spectrum 2 · RT max" (151 px), row 3 "SC 1". |
| 600 × 400 (full layout, data 240 px), zoomed | — | 80 … 280 | Rows 0 to 4 overlap the inflated panel (y 56 … 164) and have a 16 px limit, so they stay empty. Row 5 (y 174): "Main · Mid"; row 6: "Spectrum 2 · RT max", "SC 1". The last row ends at y 238, above the limit of 280. |

T-055 adds a unit test that places the default and the widest legend at 560 × 400, 600 × 400, 920 × 540 and 1200 × 700 with every badge and panel combination, and asserts no intersection.

**Hover card** (rows replace §17.5's; position, style and formats unchanged):
- The band block first while the band solo shows (§17.9.4).
- The frequency and note lines. Above the Nyquist frequency (§18.8) the note line reads "Above Nyquist (24.0 kHz)" in `text-tertiary`.
- One row per drawn slot in slot order, named as the legend, with the level at f. Slot 1's value includes the applied offset (it is what is drawn).
- Then each visible reference with data at f.
- **Offset line**, while `offsetMode` is not Off: a 1 px separator, then "Offset" (`body-sm`, `text-secondary`) and the applied offset from the frame, right-aligned in `readout` / `text-primary`, signed with 1 decimal ("−6.2 dB", "0.0 dB"). While the target is missing (§18.7), the value is followed by " · inactive" in `text-tertiary`.

### 18.7 Offset

- **Control:** an inline choice chip "OFFSET" (`ichip` style, label left, value right), bound to `offsetMode`. In the header at (288, 8, 140, 28) while W ≥ 900; in the Spectra panel's Offset row (112 × 24 field) below 900 (§18.3.2). Never in Settings.
- **Values and texts:**

| Index | Chip value | Menu item |
|---|---|---|
| 0 | "Off" | "Off" (default) |
| 1 | "Normalize" | "Normalize (peak at 0 dB)" |
| 2 | "Center" | "Center (mean at the middle)" |
| 3 to 5 | "Match {slot name}", for example "Match SC 1" | "Match {slot name}" |

  Match items name the slot, not "Spectrum N", so the default "Match SC 1" reads as the user's "Sidechain" option (#8). The chip truncates long names with "…" (the value area is 74 px).
- **Disabled options:** a Match item whose slot is off or has no input is drawn in `text-disabled` with the hint "(off)" or "(no input)" in `text-tertiary`, and cannot be chosen. Drag, wheel and arrow steps skip disabled options. Normalize and Center are always available.
- **Inactive selection:** if the selected target becomes off or loses its input, the selection is kept (the slot plan) and the chip shows the set value in `state-warn` with the §18.1.5 underline. The applied offset eases to 0 dB (DSP), and the hover card marks it "inactive". Accessible value: "Match SC 1, inactive: SC 1 has no input."
- **How the applied offset is shown:** the legend tag (§18.6) while the mode is not Off, and the Offset line of the hover card with the applied value. The grid and axis labels never move: only slot 1 is shifted.
- Changing the mode is a host gesture on `offsetMode`; double-click resets to Off. No toast, no reset, no unfreeze (frozen data keeps its offset, the slot plan).

### 18.8 The 96 kHz axis

- `freqHigh` ranges up to 96 kHz (T-051). From M3c the **display axis runs to `freqHigh` even above the Nyquist frequency**: `fHiEff = freqHigh`, and the one-octave rule `fLoEff = min (freqLow, fHiEff / 2)` stays. This needs the `DisplayRange.h` change of T-047 (§15 question 18). The Nyquist warn state of §18.1.5 therefore ends in M3c.
- **Grid labels:** the §5.3 decade sets continue above 10 kHz: 20k, 30k, 50k, 70k (set 1) or 20k, 50k (set 2). Resolved at 20 Hz to 96 kHz, 1200 × 700 (840 px, 228.2 px per decade): set 1 fails (5 to 7 is 33.3 px < 36), so set 2 is used: 20, 50, 100, 200, 500, 1k, 2k, 5k, 10k, 20k, 50k (11 labels, smallest gap 68.7 px). At 560 × 400 (376 px, 102.1 px per decade) set 2 also fails (1 to 2 is 30.7 px), so set 3 (decades) is used: 100, 1k, 10k. Linear labels in deep zoom use the §5.9.8 format ("91.0k").
- **Above Nyquist:** the region from x (sampleRate / 2) to the data rect's right edge is filled with `plot-nyquist` (#0E1219, the `bg-app` value) in the cached grid image, over the grid lines, with a 1 px `grid-major` line at the Nyquist frequency. Frequency labels in the gutter continue. Live slots draw nothing there (the mapper stops at Nyquist). Reference curves are fixed data and are drawn over it where they have data, so a 192 kHz capture can be compared at 48 kHz.
- **Hover** in that region: the card shows the frequency, the note line "Above Nyquist (24.0 kHz)" and only reference rows.
- **Text:** the CSV message of §17.12.3 becomes "Ref 2 unchanged. mix.csv has no data between 1 Hz and 96 kHz." and the reference grid spans 1 Hz to 96 kHz (roadmap Decision of 2026-10-06).

### 18.9 Canvas, breakpoints and the compact layout (#14, #16)

#### 18.9.1 Canvas

| Property | Value (replaces §2 where different) |
|---|---|
| Default | 1200 × 700 (unchanged) |
| **Minimum** | **560 × 400** |
| Maximum | 2560 × 1600 (unchanged) |
| Width breakpoints (one set, `breakpoints` tokens) | **1060**: below it the header's Refs, Freeze and Reset curves become icon buttons. **900**: below it the header is compact (§18.9.2). **600**: below it the compact layout applies (§18.9.5). |
| Height | No height breakpoint (#14). Lower heights shrink the plot and the meter bars; the meter content follows the fit ladder (§18.10.4). |

Why 560 × 400: both are the plan's targets. At 560 × 400 the compact layout keeps every region usable: the compact header, a 376 × 264 data rect (3 dB lines at the default range, §18.1.6), six control-bar items, and a 120 px meter strip with bars and correlation. The T-045 compact-header breakpoint (1174) is replaced by 1060 and 900: the Channel and Underlay controls are gone, so the full header fits down to 1060.

`ViewState.editorWidth` and `editorHeight` are clamped to the new limits on restore.

#### 18.9.2 Header (replaces §17.2 and §17.17.1)

All items y 8, height 28 unless stated. Right-anchored items use W.

| Item | Full, W ≥ 1060 | Medium, 900 ≤ W < 1060 | Compact, W < 900 |
|---|---|---|---|
| Logo | Wordmark (16, 13, 81, 18) | same | `logo-mark.svg` (16, 10, 24, 24) in `accent` |
| Spectra group | (116, 8, 164, 28), §18.3.1 | same | (52, 8, 104, 28) |
| Offset chip | (288, 8, 140, 28), §18.7 | same | not in the header (Spectra panel) |
| Presets | (440, 8, Pw, 28), Pw = clamp (W − 956, 96, 160) | (440, 8, Pw, 28), Pw = clamp (W − 768, 96, 160) | (164, 8, 36, 28): `icon-presets-16` at x + 10; a `body` 600 "*" in `accent` at x + 27 when modified |
| A/B | (448 + Pw, 8, 60, 28) | same | (208, 8, 60, 28) |
| Copy | (512 + Pw, 8, 48, 28): "A→B" | same | (272, 8, 28, 28): `icon-copy-ab-16` |
| Refs | (W − 380, 8, 72, 28) | (W − 192, 8, 28, 28), icon | as medium |
| Freeze | (W − 302, 8, 84, 28) | (W − 158, 8, 28, 28), icon | as medium |
| Reset curves | (W − 212, 8, 116, 28) | (W − 124, 8, 28, 28), icon | as medium |
| Divider | (W − 84, 14, 1, 16) | same | same |
| Meters | (W − 72, 8, 28, 28) | same | same |
| Settings | (W − 40, 8, 28, 28) | same | same |

Resolved:

| Item | 1200 × 700 (full) | 920 × 540 (medium) | 560 × 400 (compact) |
|---|---|---|---|
| Logo | 16 … 97 | 16 … 97 | 16 … 40 |
| Spectra group | 116 … 280 (toggles 196, 224, 252) | same | 52 … 156 (toggles 82, 106, 130) |
| Offset | 288 … 428 | 288 … 428 | — |
| Presets | 440 … 600 | 440 … 592 | 164 … 200 |
| A/B | 608 … 668 | 600 … 660 | 208 … 268 |
| Copy | 672 … 720 | 664 … 712 | 272 … 300 |
| Refs | 820 … 892 | 728 … 756 | 368 … 396 |
| Freeze | 898 … 982 | 762 … 790 | 402 … 430 |
| Reset curves | 988 … 1104 | 796 … 824 | 436 … 464 |
| Divider | 1116 | 836 | 476 |
| Meters | 1128 … 1156 | 848 … 876 | 488 … 516 |
| Settings | 1160 … 1188 | 880 … 908 | 520 … 548 |

Smallest gaps: 16 px at W = 1060 (Copy ends 664, Refs starts 680) and at W = 900 (Copy ends 692, Refs 708); 68 px at 560. Nothing overlaps at any width from 560 up. The header does not depend on the meters.

- Icon-only buttons keep their states (§9) and 28 × 28 targets; their names are accessible names only (no tooltips, §18.1.1).
- **Focus order:** Spectra list button, the three slot toggles, Offset (when in the header), Presets, A/B, Copy, Refs, Freeze, Reset curves, Meters, Settings; then the control bar; then the meter strip.

#### 18.9.3 Control bar (replaces §4.3 from M3c)

Chips as §4.3 (40 high at y = H − 48). Two groups:

| Group | Items, in order, with widths | Group width |
|---|---|---|
| A, Spectrum 1 | **Slot-1 gear 28**, Block **56**, Overlap 68, Window 76, Avg time 72, Smooth 68, Slope 92 | 508 |
| C, Range | **Max crest 76** (§18.11), Frequency 140, Level 120 | 352 |

- **Removed:** the "2nd curve" chip (slot 2's Type is in the Spectra list and the slot window). Slope joins group A because it is a slot-1 setting. Block narrows to 56 ("65536" is 39 px wide plus 16 padding).
- **Slot-1 gear:** a 28 × 40 button, `surface-2` fill, radius 6, `icon-settings-16` centred, and a 12 × 2 bar in slot 1's colour at (x + 8, y + 33) that ties the bar to slot 1. Hover `surface-3`; pressed `surface-3` with a 1 px `accent` border. It opens the slot window on slot 1. Accessible name "Main spectrum settings".
- **Placement** with `avail = (W − MS) − 24`: group A starts at x = 12 with 8 px gaps; **an item is shown only if it and every item before it fit** (end ≤ 12 + avail). Group C is shown only if all of group A is shown and 508 + 20 + 352 ≤ avail (880). It is right-aligned to end at 12 + avail, with a 1 × 24 `border` divider centred in the gap. Hidden items move to Settings > Analysis (§18.12).

| Size | Shown | x positions |
|---|---|---|
| 1200 × 700 (with 0, 1 or 2 sidechains) | A, C | gear 12, Block 48, Overlap 112, Window 188, Avg 272, Smooth 352, Slope 428 (ends 520); divider 530; Max crest 540, Frequency 624, Level 772 (ends 892) |
| 1200 × 700, meters collapsed | A, C | A as above; Max crest 836, Frequency 920, Level 1068 (ends 1188) |
| 920 × 540 | A | as above; C in Settings, Settings dot on |
| 920 × 540, meters collapsed | A, C | Max crest 556, Frequency 640, Level 788 (ends 908) |
| 600 × 400 (full layout, MS 296) | gear, Block, Overlap, Window | Avg time, Smooth, Slope and C in Settings |
| 560 × 400 (compact, MS 120) | gear … Smooth (ends 420) | Slope and C in Settings |

The meter strip stays 296 wide when sidechains connect (§18.10.2), so connecting a sidechain never hides a control-bar item.

#### 18.9.4 Plot region

Unchanged formulas (§4.1): plot (0, 44, W − MS, H − 100), data rect (48, 56, W − MS − 64, H − 136). Resolved: (48, 56, 840, 564) at 1200 × 700; (48, 56, 560, 404) at 920 × 540; (48, 56, 376, 264) at 560 × 400; (48, 56, 240, 264) at 600 × 400.
- **Hint line:** when even the short form (§5.5) is wider than data.w − 16, the hint line is not shown (at data widths below about 330 px).
- **Zoom panel:** unchanged (`Wz` ≥ 200). At data widths below 216 px the panel would not fit; this cannot happen at the minimum size (smallest data width 240 px).

#### 18.9.5 Compact layout (W < 600)

**Trigger:** editor width below 600 px (half the default). Height never triggers it. Above 600 the full layout applies at any height.

**Hidden** (#14): the statistics table (Peak, Peak max, TP, TP max, Headroom, RMS, Crest, Clips, TP clips), the loudness block, the Max crest readout (group C cannot fit), and the sidechain statistics and loudness rows. **Kept:** the spectrum, the level bars (main and sidechain), the correlation (adapted), the clip lamps and Reset statistics.

**Meter strip:** MS = **120**: (W − 120, 44, 120, H − 44), `surface`, 1 px `border` left line.
- Header row (strip-relative): Metering chip (12, 12, 64, 28), value only (accessible name "Metering"); Reset statistics (80, 12, 28, 28). Style, Integration and the section toggles are in Settings > Meters while compact.
- Content from y 52 to SH − 12. Correlation block (compact form, below) at the bottom, height Hc = 68 + 32 × nSC (revised 2026-10-07; was 18 × nSC); the bars column (12, 52, 96, SH − 76 − Hc) above it with a 12 px gap. Correlation hidden: the bars reach the bottom. Bars hidden: the correlation starts at y 52. Both hidden: the strip collapses (MS = 0, §17.10.6).
- **Compact correlation** (96 wide): title row 16 ("CORR" in `caps` / `text-tertiary`, the value right-aligned in `readout`); bar (0, 20, 96, 12) as §8.5 with ticks at ±0.5; scale row (0, 34, 96, 12); balance row (0, 52, 96, 16): "BAL" (`caps`) and the value right-aligned ("L 1.2 dB" is 58 px). Then, per connected sidechain, the same two readouts as the main input (§18.10.3: the compact layout hides statistics and loudness for every input alike), two 16 px lines: "SC 1" (`caps`, `meter.sc-text-label`) with its correlation right-aligned ("+0.82", `readout`, `meter.sc-text-value`), then "BAL" (`caps`, `meter.sc-text-label`) with its balance right-aligned in the main format ("L 1.2 dB", 58 px). Mono sidechain: "—" on both lines.
- Bars: §18.10.2 with a 96 px column.

Resolved at 560 × 400 (strip x 440, SH 356; editor y): bars 96 … 308 and correlation 320 … 388 with no sidechain; bars 96 … 276 and correlation 288 … 388 (Hc 100) with one; bars 96 … 244 and correlation 256 … 388 (Hc 132) with two. The tracks are then 118 px tall (dBFS ticks every 12 dB).

#### 18.9.6 Sizes below the minimum and host-imposed sizes (T-061 §3)

- The constrainer limits are 560 × 400 … 2560 × 1600.
- **Every region has a valid layout at every size from the minimum up:** the header tiers cover every W ≥ 560 (§18.9.2); the control bar always shows at least the gear and Block (avail ≥ 280 at 600 and 416 at 560); the data rect is at least 240 × 264; the meter strip is compact below 600 and its content follows the fit ladder (§18.10.4), which fits at the smallest content height (258 px at H = 400). Every popover fits inside 560 × 400 (§18.12).
- **If a host imposes a smaller size anyway** (the T-061 mechanism: the host window shrinks without `checkSizeConstraint`): `resized()` lays out for `max (getWidth(), 560)` × `max (getHeight(), 400)`, so the editor keeps the minimum layout and the host window shows its top-left part. Every extent is computed with `std::max (0, …)`; no component ever gets negative bounds; `SpectrumView` paints nothing when its data rect is empty. Nothing is stored: `writeEditorSize` keeps clamping.
- T-070 verifies this in FL Studio (both resize handles, at 100 % and a scaled display), as T-061 recommends.

### 18.10 Sidechain meters (#12)

#### 18.10.1 What appears

- For each **connected** sidechain input (`isSidechainActive (i)`): one level bar next to the main bars (§18.10.2) and **every statistic the main input shows**, with the same rows and units, in the **sidechain table** below the main sections (§18.10.3; user decision of 2026-10-07, §15 question 23). A disconnected sidechain shows nothing, and the layout re-flows as if it did not exist. With no sidechain connected the strip is exactly §17.10.
- The meters belong to the inputs, not to the slots (the slot plan). They are labelled "SC 1" and "SC 2" ("SIDECHAIN 1" in the one-sidechain table title).
- **Sidechain colour:** the colour of the lowest-numbered slot whose source is that input; if no slot uses it, the default colour of slot 3 (SC 1) or slot 4 (SC 2).
- Metering mode, style, integration, release, peak hold and the RMS window are shared (the slot plan). Reset statistics clears every bank. The stale rule (§8.7) applies per bank.

#### 18.10.2 Bars

The strip stays 296 wide. Each sidechain gets **one** bar showing its louder channel (the per-field maximum of L and R; its only channel when mono), so up to four bars fit in the column.
- **Column width** `CWb`: 80 with no sidechain (§17.10.6, unchanged); **90** with one or two, the right column staying at x 108 (6 px gap); 272 when the right column is empty; 96 in the compact layout.
- With nSC ≥ 1: scale labels right-aligned at x 24; bar area from x 28 to CWb; SC bar width `bs = 10` (CWb ≤ 120) or 24; main bar width `bm = floor ((CWb − 28 − 4 − nSC × (6 + bs)) / 2)`. Main L at 28, R at 28 + bm + 4; SC k at 28 + 2 bm + 4 + 6 + (k − 1)(bs + 6).

| Column | 1 sidechain | 2 sidechains |
|---|---|---|
| 90 (default) | L 28 … 49, R 53 … 74 (21 px), SC 1 80 … 90 | L 28 … 41, R 45 … 58 (13 px), SC 1 64 … 74, SC 2 80 … 90 |
| 272 (bars alone) | L 28 … 133, R 137 … 242 (105 px), SC 1 248 … 272 | L 28 … 118, R 122 … 212 (90 px), SC 1 218 … 242, SC 2 248 … 272 |
| 96 (compact) | L 28 … 52, R 56 … 80 (24 px), SC 1 86 … 96 | L 28 … 44, R 48 … 64 (16 px), SC 1 70 … 80, SC 2 86 … 96 |

Column-relative x; the column starts at strip x 12 (editor x 916 at 1200 × 700).

- **Column height:** with a sidechain table (or its note) shown, the bars column spans the upper region only, from the content top to the bottom of the last main section (§18.10.4); the table sits below it at full width. Track geometry inside the column is unchanged (lamps at the top, letters at the bottom, tracks from y + 14 to bottom − 16).

- **SC bar look:** level part solid in the sidechain colour; peak part in the sidechain colour pre-blended at 55 % over `bg-plot` (computed once per colour change, no alpha drawing); hold tick 2 px `meter.hold-tick`; a clip lamp bs × 10 above it; the digit "1" or "2" (`caps`) under it instead of L/R. No zones: the zones stay readable on the main bars and the scale. In the loudness modes the SC bar shows the sidechain's programme loudness. The Density style applies to the main bars only; SC bars stay Bars.
- One `fillRectList` per colour, as the main bars.

#### 18.10.3 Sidechain table (full statistics; revised 2026-10-07)

**User decision (2026-10-07, §15 question 23):** each connected sidechain keeps one bar, but its rows show **every statistic the main input shows**, with the same rows and units, at `meter.sc-opacity`. The reduced block of the first T-065 delivery (Peak max, TP max, RMS, Crest max, S · I, Corr · balance; full 124, short 52) is withdrawn.

**Statistics per sidechain** (the same `MeterSnapshot` fields as the main bank, §15 question 19):

| Group (section toggle) | Rows, label and unit | Values |
|---|---|---|
| Statistics | Peak dBFS, Peak max dBFS, TP dBTP, TP max dBTP, Headroom dB, RMS {unit of the mode}, Crest dB, **Max crest dB**, Clips, TP clips | per channel, L and R |
| Loudness | M, S, I, M max, S max (LUFS, or LU in the LU modes), LRA (LU) | per input |
| Correlation | Correlation, Balance (dB) | per input |

- **Max crest** is a table row for a sidechain because the main input's value lives in the control bar (§18.11), which shows the main input only. The main table keeps its nine rows.
- **Formats** as §8.3 to §8.5 and §17.10.7, with the same rounding, "−inf" and "—" rules. Max crest as §18.11 (1 decimal, no sign, "—"). Balance drops the unit from the value because the unit is in the label: "L 1.2", "C 0.0", "R >60". In the LU modes M, S, I, M max and S max show LUFS + 23, signed. Mono sidechain: "MONO" over its right value column, values in that column, the left column blank; correlation and balance "—".
- **Peak and TP rows** use per-channel display ballistics (§17.10.4), computed by the editor for each sidechain channel exactly as for the main channels, although the bar shows only the louder channel.
- **Alarm cells keep full strength:** a Clips or TP clips cell with a count above 0 becomes the §8.3 lamp (`meter.clip` fill, `text-on-accent`), and a negative Headroom is drawn in `state-error`, not pre-blended, so a clip on a sidechain is as visible as on the main input.

**Placement.** The sidechain table is **one item** below the last main section, at full content width: strip x 12, width **272** (editor x 916 … 1188 at 1200 wide, 636 … 908 at 920), whether the bars are shown or not. It is separated from the main sections like they are from each other (gap ≥ 12 with a centred 1 px `border` divider across the full width). The bars column ends above it (§18.10.2). Rows are `r` high, r = 18, or 16 from ladder step 1 (§18.10.4); the title row is 16. Grid: label column **84**, value columns **47**.

**Form A, one sidechain, Statistics on (side form).** Height 16 + 10 × r.

| Part | Table-relative x | Content |
|---|---|---|
| Title row | 0 … 272 | 10 × 10 swatch in the sidechain colour at (0, 3), "SIDECHAIN 1" (`caps`) at x 14; heads "L" and "R" (`caps`) right-aligned at x 131 and 178 (mono: "MONO" over R); "LUFS" (`caps`; "LU" in the LU modes) right-aligned at x 272 |
| Statistics rows (10) | label 0 … 84, L 84 … 131, R 131 … 178 | Peak, Peak max, TP, TP max, Headroom, RMS, Crest, Max crest, Clips, TP clips; labels as §8.3 (`body-sm`) with the unit 4 px after (`unit`) |
| Side rows (up to 8, rows 1 to 8 of the table) | 190 … 272 | One per row: label at x 190 (`caps`, unit after it in `unit`), value right-aligned at x 272 (`readout`): M, S, I, M MAX, S MAX, LRA (unit "LU"); CORR, BAL (unit "dB"). Loudness off removes the first six, Correlation off the last two; the rest move up. |

Widest side row: "BAL dB" (≈ 37 px) and "L 12.3" (43 px) = 80 ≤ 82.

**Form B, one sidechain, Statistics off (grid form).** Height 16 + r × (rows): three cells per row at x 0, 92 and 184, each 88 wide, label left (`caps`, unit after it) and value right-aligned (`readout`). Row 1 M, S, I and row 2 M MAX, S MAX, LRA (Loudness on); row 3 CORR, BAL (Correlation on). Title as form A without the L and R heads.

**Form C, two sidechains (pair form).** Height 16 + r × (10 + 4 + 1) with every section on.

| Part | Table-relative x | Content |
|---|---|---|
| Title row | label 0 … 84; SC 1 pair 84 … 178; SC 2 pair 178 … 272 | "SIDECHAINS" (`caps`) at x 0. In each pair: an 8 × 8 swatch in that sidechain's colour at (pair x + 4, 4) and its digit "1" or "2" (`caps`) at pair x + 16; heads "L" and "R" right-aligned at pair x + 47 and pair x + 94 (mono: "MONO" over R); a 2 px bar in the sidechain colour across the pair at y 14 … 16 |
| Statistics rows (10) | label 0 … 84; values in the four columns | as form A, two values per sidechain |
| Loudness rows (4) | as above | "M · max" LUFS: momentary in L, M max in R; "S · max" LUFS: short-term in L, S max in R; "I" LUFS and "LRA" LU: the value in the L column, R blank ("LU" replaces "LUFS" in the LU modes) |
| Correlation row (1) | as above | "Corr · bal" dB: correlation in L ("+0.82"), balance in R ("L 1.2") |
| Note row (ladder step 4 only) | 0 … 272 | "SC loudness and correlation need more height." (`body-sm`, `text-tertiary`, ≈ 245 px) |

Widest labels at 84: "Peak max dBFS" and "M · max LUFS" (≈ 79 and 71 px). The pair form keeps the sidechains side by side, so the same statistic of SC 1 and SC 2 reads along one row.

- **Lower opacity**, pre-blended (no alpha drawing): values `meter.sc-text-value` #ACB1B9 (= `text-primary` at **72 %** over `surface`, **8.2 : 1**), labels, heads and units `meter.sc-text-label` #7B8496 (= `text-secondary` at 72 %, **4.7 : 1**). The opacity token is `meter.sc-opacity` = 0.72. The main values (14.9 : 1) and labels (7.9 : 1) stay clearly brighter. The note uses `text-tertiary` (4.5 : 1).
- **Section toggles** apply to main and sidechain parts together: Bars hides the main and SC bars; Statistics the main table and the ten SC statistics rows; Loudness the main block and the SC loudness rows; Correlation the main block and the SC correlation rows. A table whose rows are all hidden is not shown.
- **Accessible names** per cell: "{Sidechain 1} {statistic} {left | right}", for example "Sidechain 1 peak max left", value "−6.2 dBFS"; per-input values without the channel ("Sidechain 2 integrated loudness", "−16.1 LUFS"). The table is one focus stop (a `juce::AccessibilityRole::table`), like the main table.
- **Text refresh:** 10 Hz, only labels whose string changed (§8.1). That is at most 28 more value labels with one sidechain and 56 with two.

#### 18.10.4 Fit ladder (replaces the rowH formula of §17.10.6; revised 2026-10-07)

Items, top to bottom: main table T, main loudness L, main correlation C, the sidechain table S (§18.10.3), only the visible ones. Heights: T = 16 + 9 × rowH, short 16 + 4 × 20 = 96 (Peak max, TP max, RMS, Crest); L 98, short 56 (title and the M, S, I row); C 68, short 48 (no balance row); S by its form and `r`, or 16 when it has given way (the note). Content height HRC = SH − 98 (strip header of §17.10.1). The minimum main row height `rowMin` is 20.

A configuration **fits** when the sum of its heights at rowH = `rowMin` plus 12 × (items − 1) ≤ HRC. Starting from everything full with r = 18, apply these steps in order, each keeping the previous ones, until it fits. Steps 1 to 5 exist only while a sidechain table is shown; with no sidechain the ladder is steps 6 to 8, as before.

| Step | Change | Why this order |
|---|---|---|
| 1 | SC rows r = 16 | Rows shrink before anything is lost. |
| 2 | `rowMin` = 18 | Main rows shrink next; still above the 16 px line height of `readout`. |
| 3 | `rowMin` = 16 | The densest legible table: rows equal the `readout` line height. |
| 4 | Form C only: the SC loudness and correlation rows give way to the note row of §18.10.3 (S = 16 + 10 r + 16). If Statistics is off, nothing would remain, so step 5 applies instead. Forms A and B: no change (form A's side rows cost no height). | The per-channel statistics are the larger part, and the pair form compares them row by row. |
| 5 | The sidechain table gives way: S becomes one note item (16 high, full width) "Sidechain readouts need a taller window." (`body-sm`, `text-tertiary`) | Main metrics stay complete first. |
| 6 | L short | As before (main data is lost only after every SC row). |
| 7 | C short | As before. |
| 8 | T short | Always fits from H = 400: 96 + 56 + 48 + 16 + 3 × 12 = 252 ≤ 258. |

Then, for a full T: `rowH = clamp (floor ((HRC − others − 16 − 12 × (items − 1)) / 9), rowMin, 28)`, where `others` is the summed height of the other items; the remaining space is shared equally between the gaps, and the dividers are centred in them. The bars column runs from the content top to the bottom of the last main section (to the content bottom when there is no S).

**Heights at which nothing gives way:** one sidechain from H = 698 (steps 1 and 2: 178 + 98 + 68 + 176 + 36 = 556 = HRC), and with r = 18 and rowH ≥ 20 from H = 736; two sidechains from H = 760 (steps 1 to 3: 160 + 98 + 68 + 256 + 36 = 618) and with r = 18 from H = 826.

**Resolved** (editor y; all sections on; bars shown; strip x 904 at 1200, 624 at 920 and 304 at 600; width 1200 and 920 use the full layout, 600 × 400 too because W ≥ 600):

| Size (content y) | No sidechain | One sidechain (form A) | Two sidechains (form C) |
|---|---|---|---|
| 1200 × 700 (130 … 688, HRC 558) | no step, rowH 28: T 130 … 398, L 460 … 558, C 620 … 688; bars 130 … 688 | steps 1, 2; rowH 18, r 16, gaps 12.7: T 130 … 308, L 321 … 419, C 431 … 499, **S 512 … 688** (title 512 … 528, statistics 528 … 688, side rows 528 … 656); bars 130 … 499 | steps 1 to 4; rowH 16, r 16, gaps 13.3: T 130 … 290, L 303 … 401, C 415 … 483, **S 496 … 688** (title 496 … 512, statistics 512 … 672, note row 672 … 688); bars 130 … 483 |
| 920 × 540 (130 … 528, HRC 398) | no step, rowH 21: T 130 … 335, L 349 … 447, C 460 … 528 | steps 1 to 5; rowH 18, gaps 12.7: T 130 … 308, L 321 … 419, C 431 … 499, note 512 … 528; bars 130 … 499 | as one sidechain (step 4 does not fit either: 160 + 98 + 68 + 192 + 36 = 554 > 398) |
| 600 × 400 (130 … 388, HRC 258) | steps 6 to 8: T 130 … 226, L 255 … 311, C 340 … 388 | steps 1 to 8: T 130 … 226, L 240 … 296, C 310 … 358, note 372 … 388 (gaps 14); bars 130 … 358 | as one sidechain |
| 560 × 400 | compact layout (§18.9.5): bars 96 … 308, correlation 320 … 388 | compact: bars 96 … 276, correlation 288 … 388 | compact: bars 96 … 244, correlation 256 … 388 |

- **Nothing overlaps:** every item and the note sit inside the content rect, the gaps are ≥ 12 px, the table and the note span x 916 … 1188 (636 … 908 at 920, 316 … 588 at 600) below the bars column, and the header rows end at y 118. The control bar does not depend on the strip content: the strip stays 296 wide (120 compact), so connecting one or two sidechains changes no control-bar item at any size (§18.9.3).
- At 920 × 540 with no sidechain this gives rowH 21 instead of the 20 of §17.17.3, because the formula reserves the real 12 px gaps.
- With other section combinations the same steps apply. Example: 1200 × 700, two sidechains, Loudness hidden: T, C and S (form C with 11 rows) fit with no step (196 + 68 + (16 + 11 × 18 = 214) + 24 = 502 ≤ 558), and rowH = floor ((558 − 68 − 214 − 16 − 24) / 9) = 26.

### 18.11 Max crest factor readout (#13)

- **Place:** the first item of control-bar group C, left of the Frequency chip: (540, H − 48, 76, 40) at 1200 × 700 (§18.9.3).
- **Look:** a read-only readout, not a chip: no fill, no border, no hover state, no cursor change, not focusable. Label "MAX CREST" (`caps`, `text-secondary`) at (x + 4, y + 5); value at (x + 4, y + 21) in `value` (JetBrains Mono 13) / `text-primary` with the unit "dB" (`unit` style): "14.2 dB".
- **Value:** the higher of the main input's per-channel maximum crest factors since the last statistics reset (`MeterBank`, T-049: 50 ms windows, sample peak minus RMS, SPAN's definition per the roadmap Decision of 2026-10-06). 1 decimal, no sign. "—" before the first complete window or when non-finite. Mono: its single channel. Text refresh 10 Hz (§8.1); stale snapshots hold it (§8.7); Reset statistics clears it.
- **Per channel and sidechains:** the per-channel values of the sidechains are rows of the sidechain table ("Max crest", §18.10.3). The main input's per-channel values are not shown separately; the existing Crest row (editor-derived peak minus RMS) stays as it is.
- **K-weighting (T-043 R4, handed over by T-045):** not adopted. The readout is unweighted, as SPAN's; a K-weighted crest factor would be a different quantity with no user request behind it.
- **When group C is hidden** (§18.9.3) it moves to Settings > Analysis as a read-only row "Max crest" (value text in `readout` / `text-primary`, no field fill). In the compact layout it is hidden with the other statistics.
- Accessible: a label named "Main input max crest factor", value "14.2 dB". Contrast `text-primary` on `bg-app` 15.7 : 1.

### 18.12 Popovers and All Settings (M3c; replaces §17.16 and §17.17.5)

**Popover family.** Every right-anchored popover becomes **520 wide**, at (W − 532, 48), so it fits at 560 wide:

| Popover | Size | 1200 × 700 | 560 × 400 |
|---|---|---|---|
| Settings (tabs) | 520 × 340 | (668, 48) … (1188, 388) | (28, 48) … (548, 388) |
| About | 520 × 340 (notices viewport (16, 140, 488, 184)) | same | same |
| Curves panel | 520 × 320 | (668, 48) … (1188, 368) | (28, 48) … (548, 368) |
| Slot window | 520 × 312 (§18.4) | (668, 48) … (1188, 360) | (28, 48) … (548, 360) |
| Spectra panel | 480 × 208, or 256 with the Offset row (§18.3.2) | (116, 48) … (596, 256) | (52, 48) … (532, 304) |
| Save as and confirmations | 280 × 112 / 96, below the Presets button: x = clamp (button x, 12, W − 292), y 40 | (440, 40) … | (164, 40) … (444, 152) |

Settings, About, the curves panel, the Spectra panel and the slot window are mutually exclusive: opening one closes the others.

**Settings tabs:** "Analysis" (16, 37, 80, 28), "Meters" (100, 37, 68, 28), "Display" (172, 37, 72, 28). The Inputs tab is removed (its contents are in the Spectra panel and the slot window). Columns at x 16 and 268, each 236 wide (label 100, field 128 at x + 108). Height fixed at 340 for every tab; a tab whose content were taller would scroll (6 px scroll bar of §6.2), which no tab needs.

**Contents (#20: only what the main window does not show):**

| Tab | Section | Rows | Shown when |
|---|---|---|---|
| Analysis | SPECTRUM 1 | Block size, Overlap, Window, Average time, Smoothing, Slope | each row while its control-bar chip is hidden (§18.9.3) |
| Analysis | RANGE | Frequency low, Frequency high, Range low, Range high, Max crest (read-only) | while group C is hidden |
| Analysis | LEVEL AND SOLO | Align 0 dB, Solo width; note "Align 0 dB: a steady 0 dBFS sine reads 0 dB. Solo width: the band you hear with Ctrl+click and hold." | always |
| Meters | METERING | Metering, Style, Integration | while the strip does not show them (meters collapsed, all sections hidden, or compact: Style and Integration only) |
| Meters | METERING | Release, Peak hold, RMS window; the §17.16 note | always |
| Meters | SECTIONS | Level bars, Statistics, Loudness, Correlation | while the strip does not show its toggles (collapsed, all hidden, or compact) |
| Display | ALL INSTANCES | Frame rate, Adaptive pacing, Anti-alias, Curve colours "Edit…"; the §17.15 note | always |

Non-empty sections are placed in order: the first in column 1, the rest stacked in column 2 (8 px apart); in the Meters tab the two METERING parts form one section. **Settings dot:** shown while any conditional row of the table is present. Resolved: at 1200 × 700 the Analysis tab shows only LEVEL AND SOLO (dot off); at 920 × 540 it shows RANGE in column 1 and LEVEL AND SOLO in column 2 (dot on); at 560 × 400 SPECTRUM 1 (Slope) in column 1, RANGE and LEVEL AND SOLO in column 2 (ends at y 324 ≤ 340).

**Curves panel at 520** (replaces the §17.11.1 rects):
- Title bar: tabs "References" (12, 4, 96, 28) and "Colours" (112, 4, 72, 28); "Export curves…" (344, 4, 136, 28); Close (484, 4, 28, 28).
- Column header: SHOW 12, NAME 66, OFFSET 202, CAPTURE 284, FILE 356.
- Slot row: eye 12 (24), swatch 42 (16), name 66 (128), offset 202 (76), **Take ▾ 284 (64)**, Load 356 (44), Save 404 (44), Clear 452 (46); info line (66, y + 28, 442, 14).
- **Take ▾** replaces Take and Take 2nd: a menu with one item per slot, "{slot name}", enabled while that slot is drawn, otherwise in `text-disabled` with "(not shown)". Choosing one stores that slot's current curve. The info line names the slot: "Taken 14:32 · SC 1 · 48 kHz · 4096 · Hann · 4× · 1/12 oct · Align off · Stereo".
- **Export curves** writes one column per drawn slot, named after its legend entry (`main_mid_db`, `spectrum_2_rt_max_db`, `sc_1_db`).
- **Colours tab:** role list (12, 44, 200, 224) with 8 rows: the four slot names, then Ref 1 to Ref 4; wheel (224, 44, 176, 176); right column x 412, 96 wide: preview (412, 44, 96, 32), hex (412, 84, 96, 24), contrast (412, 114, 96, 32), Reset role (412, 176, 96, 24); "COLOUR PRESET" label (12, 280, 104, 28), preset button (120, 280, 200, 28), Reset all (380, 280, 128, 28). The "Secondary tone" line is removed (§18.2).

**Presets menu:** gains an inert last line "Presets store analysis and meter settings only." in `text-tertiary` (it replaces the information of the removed tooltip).

### 18.13 A/B comparison (#22)

- **Controls** (rects in §18.9.2): an **A/B segmented control** right of the Presets button (8 px gap) and a **Copy** button 4 px after it.
- **A/B segmented:** track `surface-2`, radius 6, padding 2; segments "A" and "B" of 28 × 24 in `body` weight 600. The **active letter** is the selected segment: `accent` fill, `text-on-accent` (11.5 : 1). The other: transparent, `text-secondary`, hover `surface-3` with `text-primary`.
- **Switching:** a click on the inactive letter stores the current parameters into the active state and applies the other one (one host gesture per changed parameter, ≤ 50 ms, the slot plan). A click on the active letter does nothing. No wheel action and no double-click reset, so a stray scroll never switches states. Keyboard: one Tab stop; Left/Right or A/B keys switch. Accessible radiogroup "A/B comparison" with radios "State A" and "State B".
- **Copy:** text "A→B" while A is active and "B→A" while B is active (`body`, `text-secondary`, transparent; hover `surface-3`; pressed `surface-3` with a 1 px `accent` border). A click copies the active state onto the other at once and shows the toast "Copied A to B." (or "B to A") for 1200 ms at the reset-toast position (§5.9.7). Compact: a 28 × 28 icon button with `icon-copy-ab-16`. Accessible name "Copy A to B" or "Copy B to A".
- **With presets:** loading a preset changes the active state only. The Presets button keeps the current preset name; its "*" is recomputed against the active state after a switch.
- **Freeze:** a switch unfreezes only when slot 1's channel changes (§5.7 rule).
- A/B is not a parameter, is not automatable and is not in presets (the slot plan); colours, layout and display preferences are not part of A or B.

### 18.14 Superseded sections, M3c tooltip replacements, changes from the plan

**Superseded sections** (each keeps its text for history):

| Old section | Replaced by |
|---|---|
| §2 minimum size 920 × 540 | §18.9.1 (560 × 400) |
| §4.2 header, §17.2, §17.17.1 | §18.9.2 (M3d: §18.1.2, §18.1.3) |
| §4.3 control bar, §17.17.2 | §18.9.3 |
| §5.1, §17.4 roles `primary`, `underlay`, `sc1`, `sc2` and their styles | §18.2 (slot colours), §18.5 (styles) |
| §5.2, §17.5 (labels, legend rows), §17.17.4 | §18.6 |
| §5.3 and §5.9.8 dB step rules | §18.1.6 |
| §5.4 tooltip paragraph | §18.1.5 |
| §6 tooltips, §5.9.6 tooltips, every §17 tooltip text | §18.1.1 and the table below |
| §6.1 body (M3d), §17.16, §17.17.5 | §18.1.4 (M3d), §18.12 (M3c) |
| §6.3 rows `channelMode` (header), `underlay`, `showSidechain`, `showSidechain2`, `inputLabel0` to `2`, role colours | §18.16 |
| §8.8 sample-rate text | §18.1.2 |
| §17.3 Channel and underlay | §18.4 (Channel per slot); the underlay is dropped |
| §17.6 Sidechain pair | §18.3 (Spectra list), §18.2 (no-input state) |
| §17.10.6 rowH formula, §17.17.3 right-column table | §18.10.4 (the column and bar rules stay, extended by §18.10.2) |
| §17.11.1 Take and Take 2nd, panel rects | §18.12 (Take ▾, 520 wide) |
| §17.12.3 "1 Hz and 48 kHz" | §18.8 |
| §17.14.2 Colours tab rects and role list | §18.12 |

**M3c tooltip replacements** (#18 applied to §17; every text becomes the accessible description):

| §17 tooltip | Replacement |
|---|---|
| Compact header icon buttons (the label was in the tooltip) | Accessible names only; the icons are the established ones (snowflake, circular arrow, curves) |
| Effective values: Integration and Release in the cal and loudness modes, frame rate above the display rate | §18.1.5 look: the effective value ("600 ms", "60 fps") in `state-warn`, underlined |
| Style "Density" in a loudness mode | The set segment keeps the warn look of §17.10.5, plus the underline under its text |
| Integration (the 50 ms hint), Release, Peak hold | The existing Settings > Meters note |
| Section toggles | Accessible names ("Show the level bars", …) |
| Presets ("a preset stores …") | The inert last line of the Presets menu (§18.12) |
| Align 0 dB, Solo width | The LEVEL AND SOLO note (§18.12) |
| Take 2nd disabled ("Turn on a secondary curve first") | Take ▾ items marked "(not shown)" (§18.12) |
| Sidechain pair no-input texts | The no-input look of the slot toggles and the Source field (§18.3) |
| Frame rate, Adaptive pacing, Anti-alias | The existing Display note |

**Changes from the slot plan:**

| # | Plan | This spec | Reason |
|---|---|---|---|
| 1 | Each sidechain's bars next to the main bars | One bar per sidechain (its louder channel), 10 px wide; the main bars narrow to 21 px (one sidechain) or 13 px (two) | The strip must stay 296 px wide: wider, it would hide control-bar group C at 1200 × 700 whenever a sidechain connects. §15 question 17. |
| 2 | Sidechain statistics below the main rows | **Revised 2026-10-07 (user decision, §15 question 23):** every statistic of the main input for each sidechain (ten per-channel rows including Max crest, the six loudness values, correlation and balance), in one sidechain table at full strip width below the main sections, at `meter.sc-opacity`; one sidechain in the side form, two in the pair form (§18.10.3). An eight-step fit ladder (§18.10.4) shrinks rows first, then lets SC loudness and correlation, then the whole table, give way to a note. | The full sets do not fit beside the bars at the default size: everything shows from H = 698 with one sidechain and from H = 760 with two; at 1200 × 700 two sidechains show their statistics but not their loudness and correlation (§15 question 26); at 920 × 540 and below the sidechain table gives way to the note. The main metrics stay complete first. The first delivery's reduced row set is withdrawn. |
| 3 | Visible per slot | Slots 2 to 4 only; slot 1 is always shown | The parameter table has no slot 1 Visible parameter, and slot 1 can never be off. §15 question 24. |
| 4 | Offset options "Spectrum 2, 3, 4" | Shown as "Match {slot name}" | The user's own word was "Sidechain"; the slot name says which curve. The parameter's choice texts are unchanged. |
| 5 | The 2nd curve chip (today) | Removed from the control bar; slot 2's Type is in the Spectra list and slot window | Room for the slot-1 gear and the Max crest readout at 1200 × 700. |
| 6 | Take and Take 2nd (§17.11) | One "Take ▾" menu over the four slots | Any slot can now be captured; one button is narrower. |
| 7 | Compact header below 1174 px (§17.2) | Breakpoints 1060, 900 and 600 | One set for the whole editor; the slot header is shorter than the Channel plus Underlay header. |
| 8 | Popovers 560 wide | 520 wide | They must fit inside the 560 px minimum. |

### 18.15 What the mockup shows (T-065)

`mockup.html` shows the combined M3c design (T-045 plus T-065). Everything in this section can be clicked through:
- **Default state:** four slots at their defaults, one sidechain connected: the curves look as today (aqua line and gradient, dark RT max line, lavender silhouette).
- **M3d items:** no tooltips; no sample-rate text; the teal rule on the slot toggles; the dynamic Settings content and dot; the effective-value look (set Level bottom above top); the 3 dB grid; Smooth up to 3 oct; Level −72 … −18 dB.
- **Slots:** the header group, the Spectra panel (names, source, type, gear), the slot window (tabs, Link with unlink-on-edit, locks on slot 1, Fill, Channel), per-type styles and Fill, slot colours in the Colours tab, the no-input and off states.
- **Offset** with Normalize, Center and Match (300 ms easing), disabled and inactive states, the legend tag and the hover line.
- **96 kHz:** set Frequency high above Nyquist to see the shaded region.
- **Sidechain meters** for zero, one or two sidechains (dev-bar checkboxes), with the full-statistics sidechain table in its three forms and the eight-step fit ladder (§18.10.3, §18.10.4); the dev-bar size buttons include 1200 × 780, where two sidechains show every row.
- **Max crest** readout.
- **Compact layout** and the 560 × 400 minimum: a dev-bar width slider and size buttons.
- **A/B** switching and Copy.
- A dev-bar button "All four slots and both sidechains" sets slot 2 to Real-time Side, connects both sidechains and enables Fill on slot 3.

Simplifications: as §17.20, plus:
- the sidechain signals and meters are simulated;
- an unlinked slot differs only in its averaging and slope, because the simulation analyses one spectrum per source with slot 1's block, window and smoothing;
- the M3d SC toggle (§18.1.3) is not shown separately, because M3c replaces it; its teal rule is visible on the slot toggles;
- the M3d Settings popover (§18.1.4) is shown in its M3c form (§18.12), which follows the same rule;
- tooltip texts are kept as `aria-description` attributes only, so nothing appears on hover.

### 18.16 Control inventory additions (M3c; stores as §6.3)

| Item (store) | Control and location | Size | Label | Display format | Default | Drag | Wheel (Fine) | Text entry |
|---|---|---|---|---|---|---|---|---|
| `channelMode` (P), slot 1 Channel | Slot window, slot 1 | 128 × 24 | "Channel" | "Mid", "Side", "Left", "Right", "Stereo"; legacy L/R and M/S show "Left" and "Mid" | Mid | 1 step per 20 px | ±1 | (menu) |
| `s2Channel` … `s4Channel` (P) | Slot window | 128 × 24 | "Channel" | as above; mono source: "Mono" in the effective look | Mid, Stereo, Stereo | as above | ±1 | (menu) |
| `s2Source` … `s4Source` (P) | Spectra panel; slot window | 100 × 24; 128 × 24 | "Source" | "Main", "Sidechain 1", "Sidechain 2"; no input: warn look | Main, Sidechain 1, Sidechain 2 | 1 step per 20 px | ±1 | (menu) |
| `secondary`, `s3Type`, `s4Type` (P) | Spectra panel; slot window | 112 × 24; 128 × 24 | "Type" | "Off", "RT max", "All-time max", "All-time avg", "Real-time" | RT max, Real-time, Real-time | as above | ±1 | (menu) |
| `s2Link` … `s4Link` (P) | Slot window | 128 × 24 | "Linked to Main" | "On" / "Off" | On | none | up = On | (none) |
| `sNFftSize` … `sNSlope` (P, N = 2 … 4) | Slot window | 128 × 24 | as the slot 1 chips | as slot 1 (§6, §18.1.7) | as slot 1 | as slot 1 | as slot 1 | as slot 1 |
| `s1Fill` … `s4Fill` (P) | Slot window | 128 × 24 | "Fill" | "On" / "Off" | Off | none | up = On | (none) |
| `s2Visible`, `showSidechain`, `showSidechain2` (P) | Header toggles; Spectra panel eye; slot window "Show" | 28 × 24 (24 × 24 compact); 28 × 24; 128 × 24 | number; eye; "Show" | teal rule | On | none | up = On | (none) |
| `offsetMode` (P) | Header Offset chip; Spectra panel (W < 900) | 140 × 28; 112 × 24 | "OFFSET" | "Off", "Normalize", "Center", "Match {name}" | Off | 1 step per 20 px, skipping disabled options | ±1, skipping disabled | (menu) |
| `freqHigh` (P, to 96 kHz) | Frequency chip high half | as §6 | | "96.0 kHz" | 20.0 kHz | 240 px over 1 kHz … 96 kHz (log, centre 9.80 kHz) | × 2^(±1/6) | "96k" |
| `slotName1` … `4` (V) | Spectra panel | 136 × 24 | "Name" | text, ≤ 16 characters | "Main", "Spectrum 2", "SC 1", "SC 2" | none | none | any printable text |
| Slot colours ×4 (G) | Colours tab (8 roles) | as §17.14 | slot names | "#62D6F0" | §18.2 | as §17.14 | as §17.14 | as §17.14 |
| A/B (session state, not P) | Header segmented and Copy | 60 × 28; 48 × 28 (28 compact) | "A", "B"; "A→B" | selected letter | A | none | none | (none) |
| Max crest (read-only) | Control bar group C | 76 × 40 | "MAX CREST" | "14.2 dB", "—" | — | none | none | none |
| Slot-1 gear, Spectra list button, slot gears (actions) | Control bar; header; Spectra rows | 28 × 40; 76 × 24 (28 × 24); 28 × 24 | icon; "Spectra"; icon | — | — | — | — | — |

### 18.17 Tokens and assets added (tokens 1.3.0)

**Tokens** (`tokens.json` 1.3.0):
- `color.toggle-teal` #34C6A8, `color.toggle-teal-tint` #142F30 (§18.1.3); `color.plot-nyquist` #0E1219 (§18.8).
- `meter.sc-opacity` 0.72, `meter.sc-text-value` #ACB1B9, `meter.sc-text-label` #7B8496, `meter.sc-peak-alpha` 0.55 (§18.10).
- `deepGrid.dbSteps` [0.1, 0.2, 0.5, 1, 3, 6, 12, 24], `deepGrid.dbMaxLines` 24, `deepGrid.dbLineMinPx` 12, `deepGrid.dbLabelMinPx` 20 (`dbStepMinPx` removed) (§18.1.6).
- `motion.tooltipDelayMs` deprecated (§18.1.1).
- `layout.editorMin` [560, 400]; new group `breakpoints` (1060, 900, 600).
- New groups: `slot` (default colours, names, style values, draw order), `headerSlots` (tier rects), `controlBarM3c`, `spectraPanel`, `slotWindow`, `stripSidechain` (bar rules, the sidechain table forms and rows, the fit ladder and its notes, compact strip; revised on 2026-10-07 for the full statistics set, still version 1.3.0 because 1.3.0 is not yet approved), `offset`, `ab`, `popoverM3d` (§18.1.4) and `popoverM3c520`.
- Marked superseded: `headerM3c`, `curveRole.defaults.underlay`, `curveRole.defaults.sc2`, `curveRole.secondaryDefaults`, `popoverM3c.settings.tabs.Inputs`, and the 560-wide popover rects.

**Assets** (`assets/ui/`, clean SVG: `viewBox`, `<title>`, `currentColor`, no raster, scripts or external references; 16 grid, 1-unit strokes on half-integer coordinates):

| File | Use | Drawing |
|---|---|---|
| `icon-spectra-16.svg` | Compact Spectra list button (§18.3.1) | Three list rows, each a small filled square and a line |
| `icon-presets-16.svg` | Compact Presets button (§18.9.2) | Two stacked cards with a folded corner |
| `icon-copy-ab-16.svg` | Compact A/B Copy button (§18.13) | Two squares with an arrow from the left one to the right one |
| `icon-lock-16.svg` | Locked slot 1 fields and the slot 1 visibility cell (§18.3.2, §18.4) | A padlock |
| `icon-link-16.svg` | Linked analysis fields (§18.4) | Two interlocked links |

Existing assets reused: `icon-eye-16` and `icon-eye-off-16` (slot visibility in the Spectra panel), `icon-settings-16` (slot gears), `icon-chevron-down-16` (Spectra list button), `logo-mark.svg` (compact header). `icon-sidechain-16` is used by the M3d SC toggle only.

### 18.18 Accessibility and performance (M3c additions)

- **Text contrast:** `meter.sc-text-value` on `surface` 8.2 : 1, `meter.sc-text-label` 4.7 : 1; `toggle-teal` on `toggle-teal-tint` 6.6 : 1; `text-primary` on `bg-app` 15.7 : 1 (Max crest); `state-warn` underlined values 9.8 : 1 on `surface-2`; the note line `text-tertiary` on `surface` 4.5 : 1.
- **Non-colour cues:** slot toggles (teal fill and border vs. none, the ring for no input, no colour bar when off); effective values (underline); Offset inactive (underline, "· inactive" in the hover card); A/B (the selected segment's fill and weight); slot styles (§18.5); sidechain bars (the digit under each bar).
- **Hit targets:** slot toggles 28 × 24 (24 × 24 compact); A/B segments 28 × 24; Copy 48 × 28 (28 × 28); slot-1 gear 28 × 40; Spectra panel controls ≥ 24 × 24; slot tabs 116 × 28.
- **Accessible names:** listed with each control in §18.3, §18.4, §18.7, §18.11 and §18.13.
- **Performance:** curve layers ≤ 12 (§18.5); sidechain bars add one `fillRectList` per colour; sidechain text follows the 10 Hz rule (at most 56 more value labels with two sidechains, each repainted only when its string changes; the table's static labels are drawn once into the strip's cached background); the Nyquist shade is part of the cached grid image; Offset changes the slot 1 curve only (no grid rebuild); legend placement runs only when its inputs change (entries, badge, panel, size).

