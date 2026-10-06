# M3 hands-on review: user findings

Findings from the M3 hands-on review, sorted into the sections of the M3 review checklist (`m3-user-review.md`). Findings are numbered #1 to #22 in the order they appear below.

**Type:** Bug = something broken · Change = existing behaviour to change · Feature = something new to add · Not tested = could not be checked

**Priority:** only #2 is marked High (the user called it "very important"). Everything else is Normal.

**Scope:** every change in this file applies to both the VST3 plugin **and** the Standalone app, unless a finding says otherwise.

## Summary

| # | Section | Type | Finding | Triage (2026-10-06) |
|---|---------|------|---------|---------------------|
| 1 | Standalone app | Change | You hear your own microphone as soon as the app starts | M3d: T-064 |
| 2 | Spectrum | Bug · **High** | Curve looks jagged below about 300 Hz | M3d: T-061 (diagnosis), T-062 (fix) |
| 3 | Spectrum | Change | Frequency range from 1 Hz to 96 kHz | M3c: 1 Hz in T-047 (already planned); 96 kHz in T-047, T-051, T-055 |
| 4 | Spectrum | Change | 3 dB grid steps; default level range −18 to −72 dB, adjustable | M3d: defaults in T-064, grid in T-065 (spec) and T-066 |
| 5 | Spectrum | Change | Keep the level-range minimum at least 1 dB below the maximum | M3d: T-063 (rule), T-064 (ranges), T-066 (editor) |
| 6 | Spectrum | Change | Smoothing up to 3 octaves | M3d: T-062 (DSP), T-064 (parameter) |
| 7 | Spectrum | Feature | Filled display option | M3c: per-slot Fill (T-051, T-055, T-065) |
| 8 | Spectrum | Feature | Offset control: Normalize / Center / Sidechain | M3c: T-067; alignment target is any slot (Spectrum 2 to 4) |
| 9 | Spectrum | Feature | 2nd curve gets its own settings window | M3c: spectrum slot model (T-046, T-051, T-065, T-069) |
| 10 | Spectrum (sidechain) | Change | Take the "SC" label off the sidechain button | M3d: T-065 (spec), T-066 |
| 11 | Spectrum (sidechain) | Change | Sidechain button turns turquoise when active | M3d: T-065 (spec), T-066 |
| 12 | Spectrum (sidechain) | Feature | Sidechain gets its own dB scale, loudness, correlation and a settings window | M3c: meters in T-049, T-052, T-057; settings through the slot model (T-069) |
| 13 | Meters | Feature | Max crest factor fields left of the frequency range controls | M3c: read-only readout; T-049 (DSP), T-057 (editor) |
| 14 | Window and smoothness | Change | Compact layout at small sizes; allow a lower minimum height | M3c: T-065 (spec), T-070 |
| 15 | Window and smoothness | Bug | Stutter when the Block size is increased | M3d: T-061 (diagnosis), T-063 (fix) |
| 16 | Window and smoothness | Bug | Glitches when the window is made very small | M3d diagnosis in T-061; fix in T-066 if cheap, otherwise T-070 |
| 17 | Window and smoothness | Not tested | Monitors above 1080p | Risk 25; T-016 tests on real hardware if available |
| 18 | Look | Change | Remove all tooltips except the spectrum hover readout | M3d: T-065 (spec), T-066 |
| 19 | Look | Change | Remove the "48 kHz" label | M3d: T-065 (spec), T-066 |
| 20 | Look | Change | Take the settings already on the main UI out of "All Settings" | M3d: T-065 (spec), T-066 |
| 21 | Look | Feature | Presets and panel visibility toggles | **Cancelled** by the user (the M3c settings presets stay) |
| 22 | Look | Feature | A/B comparison | M3c, moved into v1: T-068 (store), T-058 (editor) |

**Triage.** The architect triaged these findings with the user on 2026-10-06. The decisions and the design are in `docs/plans/m3-review-spectrum-slots.md`. That plan covers:
- the four spectrum slots, which answer #9 and #12;
- the sidechain meters;
- Offset, A/B, and the cancellation of #21.

Frame pacing is accepted as it is. With this triage, M3 is closed.

---

## 1. Get the build

No findings reported.

## 2. Standalone app

All findings under section 3 (In a DAW) also apply to the Standalone app. The item below is specific to the Standalone app.

**#1 — You hear your own voice as soon as the app starts** · Change · Standalone only
- Seen: when `Investigate.exe` opens, the microphone input plays straight through to the output.
- Expected: no input monitoring when the app starts.
- Architect note: the app currently does this on purpose. "Mute audio input" starts unchecked (carried over from M2), and the input is passed straight through. So this asks for a different default (for example, start muted), not a fix for a malfunction.

## 3. In a DAW

### Audio

No findings reported.

### Spectrum

**#2 — Curve looks jagged at low frequencies** · Bug · **Priority: High**
- Seen: the spectrum curve looks broken or stepped, especially below about 300 Hz, where the FFT bins are close together on the log axis.
- Expected: a fully smooth curve across the whole range.
- This has to work **whatever the Smooth setting is**. Raising Smooth is not an acceptable workaround; the curve should be drawn smoothly in every case.

**#3 — Frequency range** · Change
- Expected: frequency range from **1 Hz (min)** to **96 kHz (max)**.

**#4 — Level grid and default level range** · Change
- Spectrum dB grid lines in **3 dB steps** (−3, −6, −9, −12, −15, …).
- Default level range: **−18 dB (top) to −72 dB (bottom)**. The range stays adjustable.
- The grid steps on the level meter are fine as they are; this change is only for the spectrum.

**#5 — Keep the level-range minimum below the maximum** · Change
- If the user sets the level-range minimum equal to or above the maximum, set it automatically to **1 dB below the maximum**.

**#6 — Smoothing range** · Change
- Smooth should go up to **3 octaves**.

**#7 — Filled display** · Feature
- Add an option to show the spectrum as a filled area.

**#8 — Offset control** · Feature
- Add an **Offset** control with three options: **Normalize**, **Center**, **Sidechain**.
- When no sidechain signal is present, the **Sidechain** option cannot be selected.

**#9 — Settings for the 2nd curve** · Feature
- Setting **2nd curve** to "On" opens its own settings window.
- Every setting available for the main spectrum is also available for the 2nd curve.

**Sidechain**

**#10 — Sidechain button label** · Change
- Remove the "SC" text from the sidechain button.

**#11 — Sidechain button active colour** · Change
- When active, the sidechain button uses the same turquoise as the dB meter.

**#12 — Separate sidechain settings and meters** · Feature
- When the sidechain is on, it gets its own **dB scale**, **loudness** and **correlation** readouts.
- All of the sidechain's spectrum settings go in a separate window (similar to #9).

### Meters

**#13 — Max crest factor** · Feature
- Add **Max crest factor** fields to the **left of the frequency range controls** (where the frequency values are edited).

### Window and smoothness

**#14 — Compact layout at small window sizes** · Change
- **Trigger:** the compact layout switches on when the window **width** is reduced to **50% of the default width**. Only the horizontal size counts; changing the height does not trigger it.
- In compact layout, hide these readouts: **Peak, Peak Max, TP, TP Max, Headroom, RMS, Crest, Clips, TP Clips, Loudness**.
- The **spectrum (frequency)** and the **level** meter stay visible.
- The **correlation meter** adjusts to the compact layout.
- The plugin can be made **shorter** than it can now (lower minimum height than the current 920×540).

**#15 — Stutter when the Block size goes up** · Bug
- Seen: the display stutters while the Block size (FFT size) is being increased.
- Expected: keep this stutter as small as possible.

**#16 — Glitches at very small window sizes** · Bug
- Seen: the plugin window glitches when it is resized to a very small size.
- This may come from FL Studio's plugin wrapper rather than the plugin itself, but it still needs checking.

**#17 — Monitors larger than 1080p** · Not tested
- The user has only a 1080p monitor, so the plugin has not been tried on 1440p or 4K screens. The large-window CPU numbers are still measured on 1080p only.

**Frame pacing (decision point):** Acceptable. The user saw no choppiness, so the 30 fps redraw can stay. Because of #17, this was checked on a 1080p display only.

### Look

**#18 — Tooltips** · Change
- Remove the detailed tooltips from **all** buttons and settings.
- **Keep** only the frequency readout that appears when hovering over the spectrum.

**#19 — "48 kHz" label** · Change
- Remove the "48 kHz" text from the UI.

**#20 — Duplicate settings in "All Settings"** · Change
- Remove from the **All Settings** view every setting that is already on the main interface, so nothing appears in both places.

**#21 — Presets and panel visibility** · Feature
- Add presets, for example: **Default, Master, High Resolution, Average, Average + Max**.
- When the user changes any parameter, the preset name shows **"User"**.
- Presets (and the user) can turn these on and off: **dB scale**, **spectrum analyser**, **bottom settings bar**, **correlation panel**, **loudness panel**.

**#22 — A/B comparison** · Feature
- Add an **A/B** section with two independent states: a change in A does not affect B, and the other way round.

*The checklist has no section for new features, so the plugin-wide features (#21, #22) are listed under Look, next to the Settings and About items.*

## 4. Reporting: test environment

| | |
|---|---|
| **DAW** | FL Studio 25.1.2, All Plugins Edition |
| **Projects** | Small and large. Large = 70 plugins, 31 channels, 23 mixer tracks |
| **OS** | Windows 11 |
| **CPU / GPU / RAM** | Intel i7-12700H / NVIDIA RTX 3060 Laptop GPU / 16 GB |
| **Audio drivers** | FL Studio ASIO, RØDE AI-1 ASIO |
| **Display** | 1080p only (see #17) |
