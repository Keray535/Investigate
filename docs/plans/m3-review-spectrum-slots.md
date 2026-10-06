# Investigate: M3 review fixes (M3d) and spectrum slots (M3c amendment)

Created: 2026-10-06 by vst-architect. Linked from `docs/roadmap.md` (Scope, Milestones, Decisions, Status) and from `docs/plans/m3c-feature-expansion.md` (Amendment of 2026-10-06).

This plan triages the 22 findings of the user's M3 hands-on review (`docs/plans/m3-user-review-findings.md`) and records the user's decisions of 2026-10-06. Where this plan and `docs/plans/m3c-feature-expansion.md` disagree, **this plan wins**.

## Vision

The M3 review showed two kinds of work. The first is a short list of fixes to the current product: a jagged low-frequency curve, a stutter when the block size changes, a few defaults, and some visual clean-up. The second is a set of larger additions that reshape M3c.

The central change is the **spectrum slot model**. Every curve on the spectrum is the same kind of object: the main spectrum, a second spectrum of the main input, and the spectra of both sidechains. So Investigate gets four identical spectrum slots, each with its own source, channel, analysis settings and look, and one settings window shared by all of them. This replaces three separate M3c features: the underlay, today's secondary curve and the fixed sidechain traces.

The user also asked for:
- level meters and statistics for both sidechains, next to the main ones;
- an Offset control that normalises or aligns the curves;
- A/B comparison in v1;
- a compact layout for small windows.

Investigate stays a pass-through analyser. The audio is bit-exact whenever the band solo is not held.

## User decisions of 2026-10-06

| Finding | Decision |
|---|---|
| #9 | Yes. A second spectrum gets every setting the main spectrum has. |
| #12 | Yes, for both sidechains. Each connected sidechain gets its own level meter next to the main meter. Its metrics (statistics, loudness, correlation) sit below the main metrics, drawn at a different transparency so they cannot be confused with the main values. |
| #9 and #12 together | Spectrum 2, SC1 and SC2 are all spectra, so they share one model: **four identical spectrum slots**. Slot 1 is the main spectrum; slots 2 to 4 take a free source. Every slot uses the same settings window, including slot 1. |
| #8 | Yes, as the architect proposed. Off, Normalize (the curve peak goes to 0 dB), Center (the mean level goes to the middle of the level range), or alignment to another slot (Spectrum 2, 3 or 4). An option cannot be selected while its slot is off or its source is missing. |
| #13 | A read-only readout. |
| #21 | Cancelled: no factory presets, no "User" label, and panel visibility does not go into presets. The M3c user settings presets (T-053, T-058) stay. |
| #22 | A/B comparison moves into v1. A and B each hold every parameter; colours, layout and display preferences are excluded. Both are saved with the session, with an A→B copy. |
| Slot count | 4 slots. |
| Frame pacing | Acceptable as it is (30 fps on large windows). Checked on a 1080p display only. |

## Scope

### Milestone M3d: review fixes (before the M3c C++ chain)
- **#1** The Standalone no longer plays the input through at launch. A Standalone-only output monitoring switch defaults to off. The analyser and the meters still receive the input, so JUCE's "Mute audio input", which feeds the processor silence, stays off. The VST3 plugin is unaffected. This amends Risk 7.
- **#2 (High)** The curve is smooth at every frequency and every Smooth setting, including Off. Raising Smooth is not an acceptable workaround.
- **#4** The spectrum level grid uses 3 dB steps, growing automatically on wide ranges and shrinking on very narrow ones. The default level range becomes −18 dB (top) to −72 dB (bottom). The level-meter grid is unchanged.
- **#5** The effective level range keeps the bottom at least 1 dB below the top. The `dbLow` and `dbHigh` parameter ranges both become −150 to +30 dB, so either edge can be set freely.
- **#6** Smooth gains 1/2, 1, 2 and 3 octaves.
- **#10, #11** The sidechain button loses its "SC" text and uses the level-meter turquoise when active. From M3c the sidechain button is replaced by the slot list, and the same visual rule applies to the slot visibility toggles.
- **#15** A block-size change does not stutter the display.
- **#16** The small-window glitch is diagnosed. A fix lands in M3d if it is cheap, otherwise in T-070 with the compact layout.
- **#18** Every tooltip is removed except the spectrum hover readout.
- **#19** The "48 kHz" label is removed.
- **#20** "All Settings" no longer repeats settings that are on the main interface.

### Additions to M3c
- **[15] Spectrum slots** (#9, #12): four slots, defined below. They replace M3c item [4] (primary plus underlay), the sidechain-curve parts of item [7], and today's secondary curve.
- **[16] Sidechain meters** (#12): a level meter, statistics, loudness and correlation for each connected sidechain input.
- **[17] Offset** (#8).
- **[18] Fill** (#7): a filled-area display option per slot.
- **[19] Frequency range up to 96 kHz** (#3). Above the Nyquist frequency the plot is drawn empty.
- **[20] Max crest factor readout** (#13), in the control bar to the left of the frequency range controls.
- **[21] Compact layout and a smaller minimum size** (#14, #16).
- **[22] A/B comparison** (#22).

### Out of scope
- Factory presets, a "User" preset label, and panel visibility inside presets (#21, cancelled).
- More than four spectrum slots, and more than two sidechain inputs.
- Testing on displays above 1080p is not possible on the development machine (#17). It stays a risk, and T-016 tests on 1440p or 4K hardware if it becomes available.

## Architecture

### Spectrum slot model

All four slots share one template:

| Setting | Values | Notes |
|---|---|---|
| Source | Main, Sidechain 1, Sidechain 2 | Slot 1 is locked to Main. |
| Channel | Mid, Side, Left, Right, Stereo | Stereo is the power mean of L and R. A mono source shows its single channel whatever the choice. |
| Type | Off, RT max, All-time max, All-time avg, Real-time | Slot 1 is locked to Real-time and can never be off. Off stops the slot's analysis. |
| Link | on / off | While on, the analysis settings below follow slot 1. Slot 1 has no Link. |
| Block size, Overlap, Window, Avg time, Smooth, Slope | as slot 1 today | These are the slot's own settings when Link is off. |
| Fill | on / off | The filled-area style (#7). |
| Visible | on / off | Hides the curve but keeps the analysis running, so max and average curves keep accumulating. |
| Name | text, up to 16 characters | Session state (`ViewState`), not a parameter. |
| Colour | colour | Global preference (M3c item [8]); the colour roles become slot colours. |

The frequency range, the level range and the Offset are shared by every slot, because they define the display axes.

**Default slots** reproduce today's look exactly:

| Slot | Source | Channel | Type | Link | Visible |
|---|---|---|---|---|---|
| 1 | Main (locked) | Mid | Real-time (locked) | – | on |
| 2 | Main | Mid | RT max | on | on |
| 3 | Sidechain 1 | Stereo | Real-time | on | on |
| 4 | Sidechain 2 | Stereo | Real-time | on | on |

Slot 2 at its defaults is today's secondary curve, and slot 3 is today's sidechain trace. Today the sidechain curve uses the main analysis settings, which is what Link does.

**Link** keeps the old behaviour as the default and makes "edit everything per slot" a single switch. The settings window shows slot 1's values while Link is on. Editing any analysis field turns Link off for that slot; the exact interaction is set by T-065.

**Engine sharing.** Slots that use the same source, channel and effective analysis settings share one analyser, and differ only in their secondary accumulators. At the defaults, slots 1 and 2 therefore cost what the primary and secondary curves cost today. Each slot only pays for its own FFT when it really differs.

**Legacy values.** `channelMode` keeps its seven indices. The legacy values L/R and M/S now resolve to Left and Mid for slot 1, and the editor offers the first five values only. The product has not been released, so no public session depends on the dual modes. The dual-curve display is removed together with the underlay, which was never built.

### Signal flow (changes from the M3c plan)

```
Main in (L, R) ──────────────────────────────────────┬──────────────────────► Main out (bit-exact unless band solo held)
   ├── Audio thread: MeterBank[Main]                 │
   │                 MeterBank[SC1], MeterBank[SC2]  │  (new: one bank per connected sidechain input)
   │     └──► MeterSnapshot per input ───────────────┼──► Editor meter panel (main bars + SC bars; SC metrics below main)
   └── FIFO push while analysis is active: Main, SC1, SC2 (each into its own SPSC FIFO)
Sidechain 1 in ──┘  analysed and metered only, never mixed into the output
Sidechain 2 in ──┘

Analysis thread:
  per input FIFO: drain once per poll ─► feed every active slot that uses that input
  SpectrumSlot[1..4]: channel combine ─► window ─► FFT ─► averaging ─► type (real-time / max / avg)
     (slots with equal source, channel and analysis settings share one analyser)
  SpectrumMapper per slot: 1 Hz … min(freqHigh, Nyquist), smooth interpolation (M3d), smoothing up to 3 oct, slope, Align
  Offset: computed from slot 1 and its target, time-smoothed, applied to slot 1 (Normalize, Center: to slot 1 only)
  ─► SpectrumFrame (four slot curves, per-slot binHz and flags) ─► FrameExchange ─► SpectrumView
  capture on request: any slot ─► CaptureExchange ─► reference slots, CSV
```

### Offset (#8)

`offsetMode` has the values Off, Normalize, Center, Spectrum 2, Spectrum 3 and Spectrum 4. The default is Off.
- **Normalize** shifts slot 1 so that its highest point in the visible range reads 0 dB.
- **Center** shifts slot 1 so that its power-mean level over the visible range sits at the middle of the effective level range.
- **Spectrum N** shifts slot 1 so that its power-mean level over the visible range matches slot N's.
- **Scope of the shift.** It applies to slot 1 only. The other slots and the reference slots are not shifted, so the alignment stays readable.
- **Smoothing over time.** The offset is time-smoothed, with a one-pole time constant of 300 ms by default (T-067 may refine it with a reason), so the curve does not jump frame to frame.
- **Freeze.** The offset holds while the display is frozen.
- **Missing target.** If the target slot is off, or its source is not connected, the offset eases to 0 dB. The selection is kept, and the UI marks it inactive.
- **Readout.** The applied offset is published in the frame, and the hover card shows it.
- **Relation to Align 0 dB.** The two are separate: Align is a calibration, Offset is a display shift. Align is applied first.

### Sidechain meters (#12)
- One `MeterBank` per input: Main, Sidechain 1 and Sidechain 2. A sidechain bank runs only while that bus is enabled.
- **Shared settings.** The metering mode, the style and the ballistics (`meterScale`, `meterStyle`, `meterIntegration`, `meterRelease`, `meterPeakHold`, `rmsWindow`) are shared by every bank.
- **Snapshots.** The processor exposes one snapshot per input: `getMeterSnapshot (int input)`, where 0 is Main, 1 is SC1 and 2 is SC2. The no-argument form stays and means Main.
- **Statistics reset.** One reset request clears every bank.
- **Editor layout.**
  - Each sidechain's bars sit next to the main bars, in its slot colour.
  - Its statistics, loudness and correlation rows sit below the main rows, at a lower opacity set by T-065.
  - The meters of a disconnected sidechain are hidden.
  - The meters belong to the inputs, not to the slots.

### Max crest factor (#13)
- **Definition.** The maximum, since the last statistics reset, of the crest factor measured over 50 ms windows: the window's sample peak minus its RMS, in dB, per channel. It is computed in `MeterBank` on the audio thread.
- **Earlier decision reversed.** This reverses the 2026-09-28 decision not to replicate SPAN's Max Crest Factor. The user asked for it.
- **The existing Crest statistic stays.** It is still the editor-derived peak minus RMS.
- **Placement.** The readout sits in the control bar, to the left of the frequency range controls, per T-065. It shows the main input; the sidechain values appear in their statistics rows.

### A/B comparison (#22)
- **Storage.** Two complete parameter states, A and B. Each holds every APVTS parameter value, the same content as a settings preset. Neither includes `ViewState`, colours or display preferences.
- **Switching.** Switching from A to B stores the current parameters into A, then applies B. B is applied as one host gesture per changed parameter, on the message thread, using the `PresetStore` apply code (T-053).
- **Copy.** "Copy A→B" copies the active state onto the other one.
- **Interaction with presets.** Loading a settings preset changes the active state only.
- **Session.** Both states and the active letter are saved in the session state, in a child `ABStates` under `stateVersion` 2. A version-1 state starts with A equal to the loaded parameters and B equal to A.
- **No parameters of its own.** The A/B choice is not an APVTS parameter, so it is not automatable and is not stored inside presets.
- **Owners.** plugin-engineer writes `ABStateStore` in `source/presets/`. gui-engineer adds the A, B and Copy controls (T-058).

### Compact layout (#14, #16)
- **Trigger.** The compact layout switches on below 600 px of editor width, half of the 1200 px default. Height does not trigger it.
- **What hides and what stays.** In compact layout these readouts are hidden: Peak, Peak Max, TP, TP Max, Headroom, RMS, Crest, Clips, TP Clips and Loudness. The spectrum and the level meter stay. The correlation meter adapts to the layout.
- **Minimum size.** The minimum editor size drops from 920×540. T-065 sets the exact values, with a target of at most 560 px wide and at most 400 px high.
- **Compact header.** T-065 merges the T-045 compact header (below 1174 px) into one set of width breakpoints.

### Parameter changes (all `juce::ParameterID { id, 1 }`, state version 2)

The rows below amend the table in `docs/plans/m3c-feature-expansion.md`. Every existing ID and choice index is kept.

| ID | Change | Range / choices | Default | Task |
|---|---|---|---|---|
| `smoothing` | 4 choices appended | Off, 1/48, 1/24, 1/12, 1/6, 1/3, then 1/2, 1, 2, 3 oct | 1/12 oct | T-064 (M3d) |
| `dbLow` | range and default | −150 to +30 dB, step 1 | −72 dB | T-064 (M3d) |
| `dbHigh` | range and default | −150 to +30 dB, step 1 | −18 dB | T-064 (M3d) |
| `freqHigh` | range | 1000 to 96000 Hz, log (centre 9798 Hz) | 20000 Hz | T-051 |
| `channelMode` | slot 1 channel; L/R and M/S resolve to Left and Mid | unchanged indices | Mid | T-051 |
| `secondary` | slot 2 Type; "Real-time" appended | Off, RT max, All-time max, All-time avg, Real-time | RT max | T-051 |
| `showSidechain` | slot 3 Visible | bool | on | T-051 |
| `showSidechain2` | slot 4 Visible (was planned as "Sidechain 2 trace") | bool | on | T-051 |
| `underlay` | **dropped** before it was ever built | – | – | – |
| `s1Fill` | new, slot 1 Fill | bool | off | T-051 |
| `s2Visible` | new, slot 2 Visible | bool | on | T-051 |
| `s3Type`, `s4Type` | new, Type of slots 3 and 4 | same choices as `secondary` | Real-time | T-051 |
| `s2Source`, `s3Source`, `s4Source` | new | Main, Sidechain 1, Sidechain 2 | Main, Sidechain 1, Sidechain 2 | T-051 |
| `s2Channel`, `s3Channel`, `s4Channel` | new | Mid, Side, Left, Right, Stereo | Mid, Stereo, Stereo | T-051 |
| `s2Link`, `s3Link`, `s4Link` | new | bool | on | T-051 |
| `sNFftSize`, `sNOverlap`, `sNWindow`, `sNAvgTime`, `sNSmoothing`, `sNSlope` (N = 2, 3, 4) | new; generated from slot 1's definitions | as the slot 1 parameter of the same name | as slot 1 | T-051 |
| `sNFill` (N = 2, 3, 4) | new | bool | off | T-051 |
| `offsetMode` | new | Off, Normalize, Center, Spectrum 2, Spectrum 3, Spectrum 4 | Off | T-051 |

The slot parameters are generated from one table in `source/params/`, so a slot's definition exists in one place. That makes 32 new parameters in total, all automatable. A changed range renormalises host automation for `freqHigh`, `dbLow` and `dbHigh`; this is accepted before the first public release, as was done for `freqLow`.

`ViewState` additions (T-051): `slotName1` to `slotName4` (default "Main", "Spectrum 2", "SC 1" and "SC 2"). They replace the M3c `inputLabel0` to `inputLabel2`.

### Processor API additions
- `MeterSnapshot getMeterSnapshot (int input) const noexcept` for inputs 0 to 2.
- `bool isSidechainActive (int index) const noexcept`, as in the M3c plan.
- `ABStateStore& getABStates() noexcept` (message thread).
- **Frame contents.** The `SpectrumFrame` changes are defined by T-046. Slot curves are indexed 1 to 4. The frame carries per-slot flags (active, type, source present), per-slot `binHz`, slot 2's secondary curve, and the applied offset. The frame capacity stays 3072 points per curve.

### Threading
No new thread. The analysis thread now runs up to four slot analysers. Each FIFO is still drained once per poll. Parameter reads stay raw APVTS atomics, read once per poll. The sidechain meter banks run on the audio thread under the same real-time rules.

## Budgets

Measured as in the roadmap: Release build, 48 kHz, 128-sample blocks.

| Item | Budget |
|---|---|
| Audio output with the band solo not held | bit-exact in every layout (unchanged) |
| `processBlock`, stereo main plus two stereo sidechains, three meter banks, analysis active, solo engaged | mean ≤ 40 µs, p99 ≤ 90 µs per block (was 20 and 50 µs with one bank) |
| `MeterBank` per instance, with integration, release, density and max crest | mean ≤ 15 µs, p99 ≤ 40 µs (unchanged) |
| Analysis thread, defaults with all four slots active and both sidechains connected | ≤ 4 % of one core |
| Analysis thread, worst case (four unlinked slots at 65536 points, 8x overlap, 192 kHz) | ≤ 20 % of one core |
| One re-map of every slot, worst case (four slots, 3-octave smoothing, minimum span) | ≤ 2.0 ms on the analysis thread |
| Zoom: view change to re-mapped frame published | p95 ≤ 10 ms, max ≤ 20 ms (unchanged) |
| Block-size change (#15) | no blank or frozen display: the last frame stays on screen until the new size produces its first frame, and the first new frame arrives within one hop of the new size plus 50 ms |
| Low-frequency smoothness (#2) | defined in T-062 |
| Editor with every layer visible (four slots, four references, solo tint, three meter sets), 60 fps cap, Adaptive on | message thread ≤ 25 % at 1280×720 and 2560×1440; paint p95 ≤ 4 ms and ≤ 8 ms (unchanged) |
| A/B switch (every parameter) | ≤ 50 ms on the message thread |

## Tasks

### M3d: review fixes

| Task | Title | Owner | Depends on |
|---|---|---|---|
| T-061 | Diagnosis: low-frequency curve, block-size stutter, small-window glitch | researcher | none |
| T-062 | Spectrum mapper: smooth low-frequency curve and wider smoothing | dsp-engineer | T-061; T-037 (build order) |
| T-063 | Engine continuity on block-size change, and the 1 dB level-range rule | dsp-engineer | T-061, T-062 |
| T-064 | M3d parameters and the Standalone mute default | plugin-engineer | T-062, T-063 |
| T-066 | Editor review fixes | gui-engineer | T-064, T-065 (spec sections), T-038 (build order) |

### M3c additions

| Task | Title | Owner | Depends on |
|---|---|---|---|
| T-065 | UI spec §18 and mockup: review items and spectrum slots | ui-ux-designer | T-045 |
| T-067 | Spectrum engine: offset modes | dsp-engineer | T-047 |
| T-068 | A/B state store | plugin-engineer | T-053 |
| T-069 | Slot settings window and Spectra list | gui-engineer | T-055 |
| T-070 | Compact layout and smaller minimum size | gui-engineer | T-057 |

Amended M3c tasks: T-045, T-046, T-047, T-048, T-049, T-051, T-052, T-055, T-057, T-058 and T-059. Each has a dated architect entry in its log.

### Order
- **Documentation tasks** run in parallel with C++. T-045, then T-065 (ui-ux-designer, same files, one after the other), and T-061 (researcher) start now.
- **C++ tasks** run one at a time, in this order:
  1. T-037;
  2. M3d: T-062, T-063, T-064;
  3. T-038;
  4. T-066;
  5. M3c: T-046, T-047, T-067, T-048, T-049, T-050, T-051, T-052, T-053, T-068, T-054, T-055, T-069, T-056, T-057, T-070, T-058;
  6. T-059.
- **Approval.** The user approves one combined mockup (T-045 plus T-065) before T-054 starts. T-066 needs only T-065's delivered spec, not the approval.
- **Additive DSP tasks.** T-046 to T-050 and T-067 stay additive: the current processor and editor build and behave the same after each of them.

**M3d gate:**
- the QA reports of T-062, T-063, T-064 and T-066 say PASS;
- the null test is bit-exact and pluginval passes at strictness 10;
- the user re-checks #1, #2, #4, #5, #6, #10, #11, #15 and #18 to #20 in FL Studio.

**M3c gate:** as in the M3c plan, plus:
- every slot works with each of the three sources;
- version-1 states load with the same look (slot 2 as the old secondary curve, slot 3 as the old sidechain trace);
- A/B round-trips through the session;
- the sidechain meters match the main meter on identical input;
- the revised budgets above are met.

## Risks

These continue the roadmap's numbering.

| # | Risk | Impact | Mitigation |
|---|---|---|---|
| 24 | Four slot analysers, three meter banks and up to four smoothing passes exceed the CPU budgets | A sluggish host, or missed frames | Engine sharing for equal settings. Unlinked slots only cost when used. Revised budgets gated in T-046, T-052 and T-059, with `[.perf]` cases. |
| 25 | The editor is not checked on displays above 1080p (#17) | Pacing or layout issues on 1440p or 4K | The large-window CPU harness already runs at 2560×1440 offscreen. T-016 tests on real hardware if it becomes available. The README notes the tested displays. |
| 26 | The slot model makes the editor harder to understand | Users get lost among four slots and a settings window | Defaults reproduce today's look. Link is on by default. The Spectra list shows source, type and visibility at a glance. Reviewed at the combined mockup approval. |
| 27 | M3c grows from 18 to 23 tasks, plus 5 in M3d | A later v1 release | One C++ task at a time, in a fixed order. The user can still drop or defer items at the mockup review. |
| 28 | The 1 dB level-range rule and the 3 dB grid interact badly with level zoom (minimum view span 6 dB) | A confusing grid, or a view narrower than its home range | T-063 makes the minimum level view span `min (6 dB, home span)`. T-065 specifies the grid steps for narrow ranges. |

## Decisions

| Date | Decision | Reason |
|---|---|---|
| 2026-10-06 | M3 closes: T-013 passed QA earlier, and the user's findings are now triaged (`docs/plans/m3-user-review-findings.md`). | The M3 gate rule of 2026-10-03. |
| 2026-10-06 | A new milestone, M3d (T-061 to T-064 and T-066), runs before the M3c C++ chain. #2 has high priority. | Fixes to the current product should not wait for M3c. |
| 2026-10-06 | The four-slot model (user decision) replaces the underlay, today's secondary curve and the fixed sidechain traces. Link is on by default; slots with equal analysis settings share an analyser; the legacy dual modes resolve to single channels. | A single code path, a single settings window and fewer special cases. The defaults keep today's look. The product is unreleased, so dropping the dual display breaks no public session. |
| 2026-10-06 | Sidechain meters: one `MeterBank` per input, shared metering settings, metrics below the main metrics at a lower opacity (user decision). The `processBlock` budget rises to a mean of 40 µs and a p99 of 90 µs. | The user's layout. One bank costs about 5 µs mean and 29 µs p99 today, so three banks do not fit the old budget. |
| 2026-10-06 | Offset: Off, Normalize, Center and Spectrum 2 to 4, applied to slot 1 only and time-smoothed. | User decision. Shifting only slot 1 keeps the comparison readable. |
| 2026-10-06 | Max crest factor is SPAN's definition (50 ms windows, maximum since reset), computed in `MeterBank`. | User request. It reverses the 2026-09-28 divergence. |
| 2026-10-06 | A/B is in v1: two full parameter states saved in the session, owned by plugin-engineer in `source/presets/`. #21 (factory presets and panel visibility in presets) is cancelled. | User decisions. |
| 2026-10-06 | The Standalone starts with output monitoring off (#1), and the analyser still receives the input. The plugin is unaffected. | User request. It avoids hearing the microphone at launch without blinding the analyser. Risk 7 is amended. |
| 2026-10-06 | The M3c reference grid (`ReferenceGrid.h`, capture, CSV) spans 1 Hz to 96 kHz, instead of 48 kHz, with 4096 points. | It follows the 96 kHz axis (#3), so captures at 192 kHz keep their top octave. |
| 2026-10-06 | `dbLow` and `dbHigh` both range from −150 to +30 dB, with defaults of −72 and −18 dB. The effective rule becomes "bottom ≤ top − 1 dB" (was 12 dB). `freqHigh` reaches 96 kHz. | User requests #3 to #5. Renormalised automation is accepted before the first public release. |
| 2026-10-06 | Tooltips are removed everywhere except the spectrum hover readout (#18). | User request. It supersedes the tooltip sections of ui-spec. |
