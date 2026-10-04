# Investigate: Feature expansion (milestone M3c)

Created: 2026-10-03 by vst-architect. Linked from `docs/roadmap.md` (Scope, Milestones, Decisions, Status).

## Vision

On 2026-10-03 the user asked for 14 additions that bring Investigate closer to a complete mixing and mastering analyser.

**Listening and navigation:**
- **Band solo:** the user can listen to a single frequency band by holding Ctrl and the left mouse button over the spectrum.
- **Display range:** the spectrum can show frequencies down to 1 Hz.

**Metering (following SPAN):**
- more metering modes and a density style;
- adjustable ballistics (integration, release and peak hold);
- each section of the meter strip can be hidden.

**Inputs and comparison:**
- **Channel model:** SPAN's primary channel plus underlay replaces the fixed dual modes.
- **More inputs:** up to two sidechain inputs, so the main input can be compared with, for example, a kick and a bass track at the same time.
- **References:** spectra can be kept in reference slots, and exported and imported as CSV.

**Personalisation:**
- the user chooses the curve colours with a colour wheel; the choice persists across launches and can be saved as colour presets;
- settings presets store every analysis and metering setting;
- the editor frame rate can be set.

Investigate stays a pass-through analyser. The audio is bit-exact whenever the band solo is not held.

## Scope

### In scope (milestone M3c)

The numbers in brackets are the user's item numbers from the request of 2026-10-03.

- **[1] Band solo.**
  - Ctrl+left-hold over the data rect plays a band-pass around the pointer; moving the pointer sweeps it.
  - Ctrl+wheel sets the band width. While Ctrl is held, the band is previewed without audio.
  - The parts of the spectrum outside the band are tinted.
  - Releasing returns to bit-exact pass-through after a short crossfade.
  - Level zoom moves to Alt+wheel and box zoom to Alt+left-drag (T-042, before T-037).
- **[2] Metering modes and styles, following SPAN.**
  - The modes: dBFS, dBFS.30, dBFS.15, dBFS+3, K-20, K-14, K-12, K-20 C, K-14 C, K-12 C, LUFS ML, LUFS SL, LU ML and LU SL.
  - The styles: Bars (today's look) and Density.
- **[3] Hide metrics.**
  - Each meter-strip section (level bars, statistics table, loudness, correlation) can be hidden on its own.
  - The existing header toggle still hides the whole strip.
- **[4] Stereo modes, following SPAN.** A primary channel (Stereo, Mid, Side, Left, Right) plus an optional underlay (Off, Stereo, Mid, Side, Left, Right). Today's L/R dual and M/S dual become Left plus Right and Mid plus Side.
- **[5]** The spectrum frequency range reaches down to 1 Hz.
- **[6] Align 0 dB and anti-alias.**
  - Align 0 dB: a stationary 0 dBFS sine reads 0 dB regardless of block size, window and smoothing.
  - Anti-alias: a display preference to turn curve anti-aliasing off.
- **[7] Three inputs:** main plus Sidechain 1 and Sidechain 2. Each input has its own curve, visibility toggle and editable label.
- **[8] Curve colours.**
  - A colour wheel recolours the spectrum curves.
  - The default colours apply on first launch.
  - A change persists for every later instance and launch.
  - Colour presets are saved separately.
- **[9]** An editor frame-rate cap from 30 to 144 fps (default 60), with an "Adaptive" switch that keeps the T-029 automatic pacing.
- **[10] Meter ballistics:** integration time (default 1.00 ms), release time (default 750 ms) and peak-hold time (default 1000 ms).
- **[11] Multi-track comparison.** It already exists for one sidechain (T-005, T-009, T-012); item 7 extends it to two.
- **[12] Reference slots ("static" spectra).**
  - Four slots that take a snapshot of the primary or secondary curve and draw it next to the live curves.
  - Each slot has a name, visibility and a dB offset.
  - The slots are saved with the session.
  - Freeze, which already exists, is unchanged.
- **[13] CSV.**
  - Export the current curves, or one slot, as CSV.
  - Import a CSV file into a slot. The format is compatible with SPAN's linear-frequency CSV.
- **[14] Settings presets.**
  - A preset stores every parameter value, including the metering settings.
  - It never stores colours, editor layout or display preferences.
  - Colour presets are a separate preset type.

### Out of scope
- Sharing spectra between plugin instances (SPAN Plus-style routing). The comparison works through sidechain buses only.
- More than two sidechain inputs, and surround.
- A full preset browser with tags, search or factory banks, and the VST3 program list. A simple Presets menu is in scope.
- A/B comparison and undo, Match EQ, and the spectrogram (v2).
- Recolouring the whole theme. Only the spectrum curves change colour; the background, grid and meter colours stay the design tokens.
- Exporting meter statistics to CSV.
- Saving the band-solo state, or automating it. The solo is a held gesture, not a parameter.
- Per-channel loudness meters beyond what SPAN shows (T-043 confirms how SPAN displays loudness on its bars).

## Architecture

### Signal flow (M3c changes marked)

```
Main in (L, R) ─────────────────────────────────────┬──────────────────────────► Main out
   │                                                │ bit-exact, unless band solo is held (M3c):
   ├── Audio thread, processBlock                   │ BandSoloFilter on the output, after the meters
   │     MeterBank (input, never filtered)          │ and the FIFO push; 10 ms crossfade in and out;
   │       LevelMeter: + integrated level with      │ the processor writes the buffer only while the
   │         meterIntegration, + density histogram  │ solo is engaged or crossfading
   │         (M3c)                                  │
   │       TruePeak, Loudness, Correlation          │
   │     └──► MeterSnapshot (+ level, + density) ──► Editor meter panel
   │
   └── FIFO push while analysis is active: main L/R + Sidechain 1 L/R + Sidechain 2 L/R (6 channels, M3c)
Sidechain 1 in (L, R) ──┘   analysed only, never mixed into the output
Sidechain 2 in (L, R) ──┘   (new input bus, M3c)

Analysis thread:
  SpectrumEngine: primary combination (+ underlay combination) of the main input,
  aux 1 and aux 2 curves, secondary accumulators for primary and underlay,
  SpectrumMapper (range down to 1 Hz, optional Align 0 dB),
  frames ─► FrameExchange ─► SpectrumView
  capture on request ─► CaptureExchange ─► editor (reference slots, CSV export) (M3c)

Editor (message thread):
  band solo: setBandSolo (active, centreHz) + heartbeat ─► atomics ─► audio thread (M3c)
  GlobalSettings (colours, fps cap, Adaptive, anti-alias) ◄─► preference file (M3c)
  Presets menu ─► PresetStore (source/presets) ─► APVTS parameters (M3c)
```

### Item-by-item design

**[1] Band solo**
- **DSP (`source/dsp/BandSoloFilter`, dsp-engineer).**
  - Band edges: `fLo = fc · 2^(−w/2)` and `fHi = fc · 2^(+w/2)`, where `w = soloWidth` in octaves. They are clamped to 10 Hz … 0.45 · fs.
  - The filter is a band-pass from Zavalishin TPT state-variable filters. Its order and topology are chosen by dsp-engineer from the T-044 references, aiming for a flat passband for widths of 1/3 octave or more and slopes of at least 24 dB per octave.
  - The centre and width are smoothed per sample, so a fast sweep has no zipper noise. The TPT structure stays stable under modulation.
  - Engage and release use a 10 ms linear crossfade between the dry and filtered signals. The filter state resets before each engage.
  - Each output channel is filtered independently. The filter itself is the same for a mono or stereo main input.
- **Processor (plugin-engineer).**
  - `setBandSolo (bool active, float centreHz) noexcept` is called from the message thread. Both values travel in one 64-bit atomic, and the width comes from the `soloWidth` parameter.
  - `bandSoloHeartbeat() noexcept` is called on every editor update. If the audio thread sees no heartbeat change for 250 ms while the solo is engaged, it releases the solo itself. A closed, hidden or hung editor can therefore never leave the audio filtered.
  - `releaseResources` and the last `setAnalysisActive (false)` also release the solo.
  - `MeterBank` and the FIFO push see the unfiltered input. The output buffer is written only while the solo is engaged or crossfading, so the null test stays exactly 0 at all other times.
- **Editor (gui-engineer, spec by ui-ux-designer).**
  - **Ctrl held, no button:** a visual preview of the band at the pointer, with no audio. Ctrl is polled on each editor update, because the editor has no keyboard focus.
  - **Ctrl + left button held:** audible solo. Moving the pointer sweeps the centre.
  - **Ctrl+wheel:** changes `soloWidth` in steps set by the spec. It works during the preview and during the solo.
  - **Release:** on button up, Ctrl up, focus loss, or the pointer leaving the editor.
  - **Visuals:** outside the band, the spectrum is tinted. The band edges are marked, and a small readout shows the centre and the width.
  - **Interactions:** Ctrl gestures never pan, zoom or reset the spectrum.
  - **Touchpad pinch:** Windows delivers it as a wheel event with a synthetic Ctrl flag. T-044 infers that JUCE may not report that flag as Ctrl, because JUCE reads the physical key state. So a pinch over the data rect either changes the solo width or acts as a plain wheel (frequency zoom). T-037 tests it on a laptop and records the result (Risk 18).
- **Zoom modifier change (T-042):** level zoom moves from Ctrl+wheel to Alt+wheel, and box zoom from Ctrl+left-drag to Alt+left-drag. Until M3c lands, Ctrl+wheel and Ctrl+left-drag over the data rect do nothing.

**[2] Metering modes and style**
- **`meterScale` (shown as "Metering") gains SPAN's other ten modes.** They are appended after its four existing choices, so saved sessions and automation keep their index. The menu may still present them in SPAN's order. The architect's interpretation of SPAN (SPAN User Guide p.5) follows. T-043 confirms the bar ranges and the loudness display.

| Mode | Bar scale | Level part of the bar | Bias | Integration and release |
|---|---|---|---|---|
| dBFS | −60 … +6 dBFS (as today) | integrated level | none | `meterIntegration`, `meterRelease` |
| dBFS.30, dBFS.15 | −24 … +6 dBFS and −9 … +6 dBFS (user decision of 2026-10-04; SPAN does not document the ends, T-043 R1) | integrated level | none | parameters |
| dBFS+3 | as dBFS | integrated level + 3.01 dB; the statistics RMS also + 3.01 dB; peaks unchanged | +3.01 dB | parameters |
| K-20, K-14, K-12 | K scale (as today) | integrated level, K-referenced | as today | parameters |
| K-20 C, K-14 C, K-12 C | K scale | integrated level, K-referenced | as today | fixed at 600 ms |
| LUFS ML, LUFS SL | LUFS scale | momentary (ML) or short-term (SL) loudness | none | fixed (0.4 s or 3 s windows) |
| LU ML, LU SL | LU relative to −23 LUFS | as LUFS | none | fixed |

- **LUFS and LU bars.** They show the programme loudness, which is one value per BS.1770 (user decision of 2026-10-04). SPAN does not document a per-channel display (T-043 R5), so no per-channel K-weighted windows are added.
- **`meterStyle`** is Bars or Density.
  - Bars is today's two-part bar: the integrated level below, the peak above.
  - Density, following SPAN, shows how often the signal stays at each level, as brightness along the track.
  - The density histogram is computed on the audio thread in `MeterBank`:
    - per channel, in 0.5 dB bins from −96 to +6 dBFS;
    - fed with the integrated level about once per millisecond (not per sample, to stay inside the MeterBank budget);
    - decaying with the peak-hold time;
    - published in the snapshot.
- **Integration ([10]).** The integrated level is a one-pole on power with τ = T / ln 100, the same "time to rise or fall 20 dB" convention as `avgTime`. It is computed per channel on the audio thread and published as the maximum over the last 50 ms, like the peaks. In the K-xx C modes the processor passes 600 ms whatever the parameter says.
- **Release and hold ([10]).**
  - `meterRelease` is the time to fall 20 dB. `meterPeakHold` is how long the peak-hold tick stays before it falls.
  - Both are applied in the editor to the peak part and the hold tick, replacing today's fixed 20 dB/s fall, 1500 ms hold and 30 dB/s fall.
  - The level part falls at the same release rate, in `MeterBank`, so the density style sees the same ballistics.
  - The K-xx C modes force 600 ms.
- **The statistics table** keeps its RMS row on `rmsWindow`. Its unit label follows the metering mode.

**[3] Hide metrics**
- Four sections can be hidden one by one: level bars, statistics table, loudness block and correlation block.
- The visible sections re-flow per the spec. If every section is hidden, the strip behaves as collapsed.
- The visibility is stored in `ViewState` as `meterSections`, a bitmask with every section visible by default.
- The existing header toggle (`metersCollapsed`) still hides the whole strip in one click, whatever the section settings.
- The control that toggles sections is specified by T-045. The editor has no context menus (ui-spec §5.6), so it must be a visible control.

**[4] Channel model (SPAN groups and Underlay)**
- **The parameters.** `channelMode` keeps its ID and its seven choice indices: Mid, Side, Left, Right, L+R, L/R, M/S.
  - The editor offers its first five choices as the primary channel. L+R is shown as "Stereo"; it is today's power mean.
  - A new parameter, `underlay` (Off, Mid, Side, Left, Right, Stereo; default Off), adds the second curve.
  - An underlay equal to the primary choice counts as Off.
  - With a mono main input, every choice shows the single channel and the underlay is off.
- **Legacy dual values.** The old values L/R and M/S stay valid aliases, so sessions and automation from earlier builds load unchanged:

| `channelMode` | Effective primary | Effective underlay |
|---|---|---|
| Mid, Side, Left, Right | the same | `underlay` |
| L+R | Stereo | `underlay` |
| L/R (legacy) | Left | Right (`underlay` ignored) |
| M/S (legacy) | Mid | Side (`underlay` ignored) |

  - The engine resolves the alias.
  - The editor shows a legacy value as the equivalent pair, and the next user edit writes the normalised pair. Nothing is rewritten automatically.
- **The frame:** the underlay takes today's dual-mode "primary B" place.
  - The frame keeps its field names: `primaryBDb` and `secondaryBDb` carry the underlay, and `isDual` means that an underlay is present.
  - The underlay is drawn with the primary B style and has its own secondary curve, exactly as the dual modes today.
- **Aux curves:** they use the primary combination when the underlay is off, and the Stereo power mean when it is on. This is today's sidechain rule, extended to both sidechains.
- **Resets:** auto-unfreeze (ui-spec §5.7) and the DSP-side secondary reset both trigger when the effective primary or underlay changes.
- **Header control:** the header Mode control becomes a Channel segmented control plus an Underlay choice (T-045).
- T-043 checks SPAN's routing presets and what a two-channel group's spectrum shows (power mean or sum). Any change to the Stereo definition is logged in the Decisions before T-046.

**[5] 1 Hz**
- **The parameter:** `freqLow` becomes 1 to 1000 Hz, with a log skew centred at 31.6 Hz (the geometric centre). The default stays 20 Hz. The home-range rule (at least 1 octave) is unchanged.
- **The mapper:** it defines the points below the first FFT bin, with no NaN or infinity and a monotone interpolation. dsp-engineer chooses whether bin 0 (DC) takes part and logs the reason.
- **The editor:** it adds 1, 2 and 5 Hz grid labels, and note names below C0 (the format is set by T-045).

**[6] Align 0 dB and anti-alias**
- **`align0dB`** (APVTS, default Off).
  - Today a full-scale, bin-centred sine reads 0 dB before smoothing. Smoothing and peak-preserving mapping then lower a sine's displayed peak by an amount that depends on block size and smoothing.
  - With Align on, the mapper compensates, so a stationary 1 kHz 0 dBFS sine reads 0 ± 0.5 dB at slope 0 for every block size, window, overlap and smoothing setting. The method is chosen by dsp-engineer, informed by T-043.
  - Noise-like signals read higher with Align on, as in SPAN, which also suggests setting slope 0 and adjusting the range.
  - With Align off, the frames are bit-identical to today.
- **Anti-alias** is a global display preference, default on. When off, the curve strokes are drawn as pixel-snapped column spans in one `fillRectList` per colour, with no anti-aliasing. The fills are already per-column bars.

**[7] Three inputs**
- **Buses:** input 0 is the main input (mono or stereo, unchanged). Input 1 is "Sidechain 1" and input 2 is "Sidechain 2". Each sidechain can be disabled, mono or stereo, and both are disabled by default.
- **Processor:** `isSidechainActive (int index)` for 0 and 1. The engine is re-prepared on any layout change, as T-009 does today.
- **Engine:**
  - The analysis input carries 6 channels: a new FIFO for Sidechain 2 and `pushSidechain2`, next to the existing two.
  - `SpectrumFrame` keeps `sidechainDb` and `hasSidechain` for Sidechain 1, and adds `sidechain2Db` and `hasSidechain2`.
  - The freeze snapshot gains one bin-domain curve, about 0.13 MB at 65536 points.
  - Sidechain curves have no secondary curve.
- **Parameters:** `showSidechain` keeps its ID and is labelled "Sidechain 1 trace". `showSidechain2` is new (default on).
- **Labels:** the user can edit a label for each input (for example "Mix", "Kick" or "Bass"), shown in the legend and the hover card. They are stored in `ViewState` (`inputLabel0`, `inputLabel1`, `inputLabel2`), with the defaults "Main", "SC 1" and "SC 2" and at most 16 characters.
- **Meters:** they still measure the main input only.
- **Host support differs.** T-044 surveys it, FL Studio first. An inactive sidechain bus shows the existing "no input" state.

**[8] Curve colours and colour presets**
- **Colour roles:** Primary A, Primary B (underlay), Sidechain 1, Sidechain 2, and References 1 to 4.
  - The secondary curves are a darker tone of their primary.
  - The gradient fill and the sidechain silhouette use the role colour at the token alphas.
  - T-045 defines the derivation rules, and the defaults are the current tokens.
- **`GlobalSettings`** (in `source/gui/`, gui-engineer):
  - one `juce::PropertiesFile`, in `%APPDATA%\Investigate Audio\Investigate\Investigate.settings`, with an `InterProcessLock`;
  - it holds the current role colours, the fps cap, Adaptive and anti-alias;
  - all editors in the process share one instance (`juce::SharedResourcePointer`);
  - a change repaints every open editor in the process at once, and other processes pick it up when their editor next opens;
  - writes are debounced to at most 2 per second;
  - a missing or corrupt file gives the defaults and never fails.

  The folder name follows the final company name chosen before M5.
- **The colour editor:**
  - a colour wheel (a custom component: a hue ring plus a saturation and value area) with hex entry;
  - a contrast warning when a curve colour falls below 3:1 against `bg-plot`;
  - "Reset to default".
- **Colour presets:** `*.invcolours` XML files in `…\Presets\Colours\`, holding a name and one ARGB value per role. The user can save as, load, overwrite and delete. Loading a colour preset changes the current global colours.
- Colours are never written to the session state or to settings presets.

**[9] Frame rate**
- **The preferences:** `frameRate` (30 to 144, default 60) and `adaptivePacing` (default on), both in `GlobalSettings`.
- **The cap:** the editor's update limiter caps all updates (spectrum and meters) at `min (frameRate, display refresh)`.
- **Adaptive:** the T-029 rule halves the spectrum repaint rate, but not below 30 fps, when the plot area exceeds 2.0 Mpx or when the heaviest curve-layer case is visible. T-054 and T-055 recalibrate that layer threshold for the M3c layers.
- **The numeric meter text** stays at 10 Hz.
- **CPU budgets** are gated at 60 fps. Higher rates are measured and reported, not gated.

**[10] Ballistics**
- The parameters `meterIntegration`, `meterRelease` and `meterPeakHold` are in the parameter table. They are part of the session and of settings presets. The model is described under [2].

**[12] Reference slots**
- **The fixed reference grid:**
  - 4096 points, log-spaced from 1 Hz to 48 kHz;
  - defined once in `source/dsp/` (dsp-engineer) and shared by the engine, the editor and the CSV code;
  - points with no data (for example above the Nyquist frequency) carry a sentinel and are not drawn.
- **Capture:**
  - `requestSpectrumCapture()` sets an atomic counter.
  - The analysis thread maps every current curve (primary, underlay, both secondary curves, Sidechain 1 and 2) onto the reference grid, with the current smoothing and Align but without slope.
  - It publishes the result through a lock-free single-slot exchange, together with the settings in force: sample rate, block size, window, overlap, smoothing, slope, Align and the channel choices.
  - While frozen, the capture comes from the freeze snapshot.
  - `getSpectrumCapture (SpectrumCapture&)` reads it on the message thread.
- **The slots:**
  - There are four slots, each with a name (default "Ref 1" to "Ref 4"), visibility, a dB offset (−40 to +40 dB) and its colour role.
  - "Take" copies the primary curve, and "Take 2nd" copies the secondary curve.
  - The editor draws a slot through the current view mapping, adding the current slope and the slot's offset. It uses a line style with no fill that stays distinct from every live curve without relying on hue (T-045).
- **Persistence:** the slots are saved with the session in a `ViewState` child `ReferenceSlots`, as base64 float arrays on the fixed grid (at most about 90 KB for four full slots). They are not part of settings presets.

**[13] CSV**
- **Code:** a pure `CsvSpectrum` module in `source/gui/` (gui-engineer), unit-tested on its own.
- **Export:**
  - "Export curves" writes one column per visible live curve, from a capture, against `frequency_hz` on the reference grid.
  - "Export slot" writes `frequency_hz,level_db`.
  - The first lines start with `#` and record the product version, sample rate, block size, window, overlap, smoothing, slope, Align and channel choices.
  - The decimal point is always "." and the separator ",".
- **Import into a slot:**
  - Separators: comma, semicolon or tab. A "," decimal comma is accepted when the separator is a semicolon.
  - Lines starting with `#`, and non-numeric header lines, are skipped.
  - The first column is the frequency in Hz (above 0, sorted if needed), and the second is the level in dB.
  - At most 65,536 rows are read.
  - The data is resampled onto the reference grid by log-frequency interpolation, with no extrapolation beyond the file's frequency range.
  - SPAN's CSV export (linear frequencies, levels in dB) loads unchanged.
  - Errors show a toast that names the line, and leave the slot unchanged.
- **File dialogs:** files are chosen with `juce::FileChooser::launchAsync` on the message thread, never with a blocking modal loop.

**[14] Settings presets**
- **Code:** `source/presets/` is a new directory owned by plugin-engineer.
  - `UserDataPaths.h` defines the user data folder, the preset subfolders and the preference file name. `GlobalSettings` and the colour presets include it read-only.
  - `PresetStore` lists, saves, overwrites, deletes and loads `*.invpreset` XML files in `…\Presets\Settings\`.
- **The file format:**
  - the root `InvestigatePreset`, with `name`, `productVersion` and `stateVersion`;
  - one `PARAM` element (`id`, `value`) per APVTS parameter, with plain values;
  - no `ViewState`, no colours and no display preferences.
- **Loading:**
  - unknown IDs are ignored, missing IDs take their default, and values are clamped;
  - every parameter is set as one host gesture (`beginChangeGesture`, `setValueNotifyingHost`, `endChangeGesture`), on the message thread;
  - a corrupt file leaves every parameter unchanged and reports an error.
- **"Default"** is built in: every parameter at its default. It is not a file.
- **The Presets menu** sits in the header (T-045 defines it): Default, the user presets, Save as, Overwrite, Delete, and Open presets folder. The current preset name is shown, marked when the parameters differ from it.
- `getNumPrograms()` stays at 1. The VST3 program list is not used.

### Storage summary

| Store | Scope | Contents |
|---|---|---|
| APVTS parameters | session, settings presets | every parameter in the table below |
| `ViewState` (in the session state) | session only | editor size, `metersCollapsed`, `meterSections`, input labels, `ReferenceSlots` |
| `GlobalSettings` file | every instance and launch on the machine | curve role colours, `frameRate`, `adaptivePacing`, `antiAlias` |
| Colour presets (`*.invcolours`) | files | a name and the role colours |
| Settings presets (`*.invpreset`) | files | a name and the parameter values |
| Not stored anywhere | – | band solo state, zoom, freeze |

### Parameter table changes (all use `juce::ParameterID { id, 1 }`; `stateVersion` becomes 2)

| ID | Name | Type | Range / choices | Default | Unit | Skew | Change |
|---|---|---|---|---|---|---|---|
| `channelMode` | Channel | choice | Mid, Side, Left, Right, Stereo (L+R power mean), L/R (legacy), M/S (legacy) | Mid | – | – | ID and indices kept. The editor offers the first five. The last two stay valid aliases (see [4]). Display texts may change. |
| `underlay` | Underlay | choice | Off, Mid, Side, Left, Right, Stereo | Off | – | – | new; ignored while `channelMode` holds a legacy dual value |
| `freqLow` | Frequency low | float | 1 to 1000 | 20 | Hz | log (centre 31.6 Hz) | range was 10 to 1000 |
| `align0dB` | Align 0 dB | bool | off / on | off | – | – | new |
| `showSidechain` | Sidechain 1 trace | bool | off / on | on | – | – | display name only |
| `showSidechain2` | Sidechain 2 trace | bool | off / on | on | – | – | new |
| `meterScale` | Metering | choice | dBFS, K-20, K-14, K-12, then appended: dBFS.30, dBFS.15, dBFS+3, K-20 C, K-14 C, K-12 C, LUFS ML, LUFS SL, LU ML, LU SL | dBFS | – | – | ID and the first four indices kept; ten modes appended |
| `meterStyle` | Meter style | choice | Bars, Density | Bars | – | – | new |
| `meterIntegration` | Integration time | float | 0.1 to 3000 | 1.00 | ms | log (centre 30 ms) | new |
| `meterRelease` | Release time | float | 10 to 5000 | 750 | ms | log (centre 500 ms) | new |
| `meterPeakHold` | Peak hold time | float | 0 to 10000 | 1000 | ms | centre 1000 ms | new |
| `soloWidth` | Solo width | float | 1/12 to 4 | 1.0 | oct | log (centre 0.58 oct) | new; not automatable |

The remaining parameters are unchanged, including `rmsWindow`, which still drives the statistics RMS.

Conditions:
- T-043 may refine the ranges of the metering and ballistics parameters before T-049 and T-051 start. Any change is logged in the roadmap Decisions.
- `soloWidth` is created with `withAutomatable (false)`, because it has an audible effect only during the held gesture.

**State version 2** (plugin-engineer):
- `stateVersion` becomes 2. It marks the new `ViewState` content (`meterSections`, the input labels and `ReferenceSlots`).
- No parameter value needs migrating. Every existing ID and choice index is preserved, and the APVTS state stores plain values, so a `freqLow` of 20 Hz stays 20 Hz.
- Missing `ViewState` properties take their defaults.
- A version-1 state therefore loads with the same channel mode, meter scale and ranges it was saved with.
- Host automation of `freqLow` is normalised through the new skew. This is accepted before the first public release.

### Processor API additions (plugin-engineer)
- `void setBandSolo (bool active, float centreHz) noexcept` and `void bandSoloHeartbeat() noexcept`. Message thread, lock-free.
- `bool isSidechainActive (int index) const noexcept`, for index 0 and 1. The existing no-argument form maps to index 0 until the editor moves over.
- `void requestSpectrumCapture() noexcept` and `bool getSpectrumCapture (SpectrumCapture& dest) noexcept`. Message thread.
- The meter parameters reach `MeterBank` through raw APVTS atomics, read once per block. The effective integration and release (600 ms in the K-xx C modes) are resolved before `MeterBank` sees them.

### MeterSnapshot additions (dsp-engineer)
- `level[ch]`: the integrated level with release applied, in dBFS, as the maximum over the last 50 ms.
- `density[ch][204]`: the density histogram (0.5 dB bins, −96 to +6 dBFS), normalised from 0 to 1.
- If T-043 calls for per-channel loudness bars: `momentaryCh[ch]` and `shortTermCh[ch]`.

### Threading
- **Audio thread:**
  - reads the band-solo word, the heartbeat counter and the meter parameters once per block;
  - writes the output buffer only while the solo is engaged or crossfading;
  - does nothing new that could block or allocate.
- **Analysis thread:** serves capture requests on its next poll, from preallocated buffers.
- **Message thread:**
  - band-solo calls and the heartbeat;
  - `GlobalSettings` file input and output, debounced;
  - preset and CSV files, through asynchronous file choosers;
  - capture reads.

## Budgets

Measured as in the roadmap: Release build on the development machine, 48 kHz, 128-sample blocks.

| Item | Budget |
|---|---|
| Audio output with the band solo not held | bit-exact (null test difference exactly 0) in every layout, including both sidechains |
| `processBlock` with the solo engaged, stereo main plus two stereo sidechains, analysis active | mean ≤ 20 µs and p99 ≤ 50 µs per block (the roadmap budget, unchanged) |
| `BandSoloFilter` alone, stereo, 128 samples | mean ≤ 3 µs per block |
| Solo response | the audio follows a press, release or sweep within one editor update plus one audio block |
| `MeterBank` with integration, release and density | mean ≤ 15 µs and p99 ≤ 40 µs per block (unchanged) |
| Analysis thread | ≤ 2 % of one core at the defaults with both sidechains active; ≤ 10 % in the worst case (65536, 8x, an underlay, both secondary curves, two stereo sidechains, 192 kHz) |
| One re-map, worst case (underlay, both secondary curves, two sidechains, 1/3-octave smoothing, minimum span) | ≤ 1.0 ms on the analysis thread |
| Capture: request to available | ≤ 20 ms; mapping ≤ 2 ms on the analysis thread |
| Editor at a 60 fps cap with every M3c layer (primary, underlay, two secondary curves, two sidechains, four references, solo tint), Adaptive on | whole-editor message thread ≤ 25 % at 1280×720 and 2560×1440 (T-012/T-029 harness, median of 3); paint p95 ≤ 4 ms and ≤ 8 ms |
| Editor at a 144 fps cap | measured and reported, not gated |
| Preset load (every parameter) | ≤ 50 ms on the message thread |
| CSV import of 65,536 rows | ≤ 200 ms on the message thread |
| `GlobalSettings` writes | ≤ 2 per second during a colour drag |
| Session state with four full reference slots | ≤ 150 KB |
| Memory | ≤ 64 MB per instance (the frame grows to about 100 KB, the capture to about 120 KB, and the freeze snapshot to about 0.8 MB) |

## Milestone M3c: tasks

| Task | Title | Owner | Depends on |
|---|---|---|---|
| T-042 | Zoom modifiers: Alt+wheel level zoom, Alt+drag box zoom, Ctrl reserved for band solo (spec and mockup) | ui-ux-designer | none (M3b; blocks T-037) |
| T-043 | SPAN reference for M3c | researcher | none |
| T-044 | Host and platform constraints, band-solo filter references | researcher | none |
| T-045 | M3c UI spec addendum, tokens and mockup | ui-ux-designer | T-042, T-043, T-044 |
| T-046 | Spectrum engine: underlay model and a second sidechain input (additive) | dsp-engineer | T-043, T-038 (build order) |
| T-047 | Spectrum mapper: 1 Hz range and Align 0 dB | dsp-engineer | T-043, T-046 |
| T-048 | Spectrum engine: reference grid and capture | dsp-engineer | T-047 |
| T-049 | MeterBank: integration, release, metering modes and density (additive) | dsp-engineer | T-043, T-048 (build order) |
| T-050 | BandSoloFilter | dsp-engineer | T-044, T-049 (build order) |
| T-051 | Parameters and state version 2 | plugin-engineer | T-043, T-047, T-050 (build order) |
| T-052 | Processor wiring for M3c | plugin-engineer | T-046 to T-051 |
| T-053 | Settings preset store | plugin-engineer | T-052 |
| T-054 | Global settings, curve colours, colour wheel and colour presets, frame rate, anti-alias | gui-engineer | T-045 (mockup approved), T-053 |
| T-055 | SpectrumView: channel and underlay controls, sidechain curves and labels, 1 Hz axis, Align | gui-engineer | T-054 |
| T-056 | Band solo interaction | gui-engineer | T-055 |
| T-057 | Meter panel: metering modes, styles, ballistics, section hiding | gui-engineer | T-056 |
| T-058 | Presets menu, reference slots, CSV import and export | gui-engineer | T-057 |
| T-059 | M3c gate validation | plugin-validator | T-058 |

Order:
- **Documentation tasks run in parallel with C++.** T-042, T-043 and T-044 start now, next to T-041. T-045 runs after them, in parallel with T-037 and T-038.
- **C++ tasks run one at a time, in number order:** T-041, T-037 (after T-042), T-038, T-046 to T-058, then T-059.
- **The DSP tasks (T-046 to T-050) are additive.** Existing APIs, frame fields and settings keep working with their current meaning, so the processor and the editor build and pass unchanged after each of them.
- **T-051 (parameters) comes after the DSP tasks**, so that `freqLow` can reach 1 Hz only once the mapper handles it. It only adds parameters, extends choice lists at the end, and changes the `freqLow` range, so the current editor keeps working.
- **T-052 switches the processor to the new parameters and APIs**, and the GUI tasks then move the controls.
- **M4** (T-015, T-016, T-017) now depends on T-059.

**M3c gate:**
- The user approves the T-045 mockup before T-054 starts.
- Every unit and validation QA report for T-046 to T-058, and the T-059 validation report, says PASS. The budgets above are met.
- The user completes the hands-on checklist below.

## Risks

These continue the roadmap's numbering.

| # | Risk | Impact | Mitigation |
|---|---|---|---|
| 15 | Some hosts offer only one sidechain input for VST3 (Ableton Live, for example), or route a second one awkwardly | Sidechain 2 unusable there | T-044 surveys the hosts, FL Studio first. An inactive bus shows "no input". No feature depends on Sidechain 2. The README documents host support. |
| 16 | The band solo changes the audio: clicks, a stuck solo, or an accidental solo during a mix or a render | Audible damage; loss of trust in a "pass-through" tool | The solo only works while held. 10 ms crossfades. A 250 ms heartbeat failsafe. Release on Ctrl or button up, focus loss, editor close and `releaseResources`. Null tests with the solo idle. Never saved, never automatable. |
| 17 | On Windows, releasing Alt after Alt+wheel or Alt+drag may activate the host's menu bar | Zoom gestures feel broken in some hosts | T-044 verdict: plausible but unproven. JUCE re-posts an unconsumed `WM_SYSKEYUP` to the host window, and the JUCE Standalone suppresses `SC_KEYMENU`. User decision of 2026-10-04: keep Alt. T-037 adds an Alt key-up guard, which consumes the key-up after an Alt gesture while the editor has keyboard focus. The user tests in FL Studio: hold Alt, scroll 5 notches, release. Only if the menu bar activates does the user choose a fallback from T-044 (A: Shift; B: Shift plus right-drag; C: Ctrl+Shift; D: Alt behind a setting). |
| 18 | A touchpad pinch may arrive as Ctrl+wheel, so it would change the solo width instead of zooming | Unexpected on laptops | Accepted, and documented in the checklist. It replaces roadmap Risk 14. T-044 infers that JUCE may not see the pinch's Ctrl flag at all; T-037 tests it on a laptop and the checklist text follows the finding. |
| 19 | Several instances or processes share the `GlobalSettings` file (bridged or sandboxed hosts) | Lost or torn updates | `InterProcessLock`, debounced writes, reload on editor open, defaults on a corrupt file. Never touched by the audio thread. |
| 20 | User-chosen colours break the colour-blind-safe distinctions or the contrast | Curves hard to tell apart | Curve styles still differ by fill, width and dash. A contrast warning in the picker. Reset to default. |
| 21 | 14 items in 18 tasks delay the v1 release | Later release | The M3c gate. The user can drop or defer items at the T-045 mockup review. M4 and M5 are otherwise unchanged. |
| 22 | Align 0 dB behaves differently from SPAN or from user expectations | Confusing levels | T-043 researches it. A numeric criterion with sines in T-047. Off by default, with today's frames bit-identical. |
| 23 | Sessions or automation saved with earlier builds load differently after M3c | Lost settings, or a changed view | Existing parameter IDs and choice indices are preserved: the legacy L/R and M/S values become aliases, and the new metering modes are appended. T-051 and T-059 load stored version-1 states and compare the result. |

## Decisions

| Date | Decision | Reason |
|---|---|---|
| 2026-10-03 | M3c is added between M3b and M4, with the 14 items of this plan. M4 depends on T-059. | User request and user choice of timing. M4 must validate the final UI. |
| 2026-10-03 | The band solo is audible:<br>• Ctrl+left-hold solos the band at the pointer; Ctrl+wheel sets its width.<br>• Level zoom moves to Alt+wheel and box zoom to Alt+left-drag, which amends the approved M3b spec through T-042 before T-037 starts.<br>• The bit-exact rule now holds whenever the solo is not held. | User choices. It follows SPAN's Ctrl gesture. The zoom spec is still unimplemented, so changing it costs only a spec update. |
| 2026-10-03 | Items 2 and 4 follow SPAN: 14 metering modes, the Bars and Density styles, and SPAN's primary-plus-underlay channel model. | The user's reference images did not arrive. The user chose SPAN as the reference. |
| 2026-10-03 | Existing parameter IDs and choice indices are preserved:<br>• `channelMode` keeps its seven values; the dual values become aliases of the new primary-plus-underlay pairs, and `underlay` is a new parameter;<br>• `meterScale` keeps its four values, and the ten new modes are appended.<br>The DSP tasks are additive and run before the parameter task. | No state migration is needed, sessions and automation from earlier builds load unchanged, and every intermediate build keeps the current editor working. |
| 2026-10-03 | Meter sections can be hidden one by one; the existing toggle still hides everything. | User choice. |
| 2026-10-03 | Three inputs: main plus two sidechains. The meters measure the main input only. Sharing between instances stays out of scope. | User choice. Bus-based comparison needs no inter-instance infrastructure. |
| 2026-10-03 | The colour wheel recolours the spectrum curve roles only. Colours are global (every instance, persisted across launches), with colour presets as files. They are never part of sessions or settings presets. | User choice. It matches SPAN's global colour schemes. |
| 2026-10-03 | The frame-rate cap is 30 to 144 fps (default 60), with Adaptive pacing (default on), as global preferences. Budgets are gated at 60 fps. | User choice. Display rate is a property of the machine, not of the project. |
| 2026-10-03 | Ballistics use SPAN's definitions, in milliseconds: integration 1.00 ms, release 750 ms, peak hold 1000 ms. `rmsWindow` stays for the statistics RMS. | User choice. |
| 2026-10-03 | Four reference slots are saved with the session; CSV export and import use a fixed 4096-point log grid from 1 Hz to 48 kHz. | User choice of slots plus CSV. The grid sizes, slot count and folder layout are architect defaults that T-045 may refine. |
| 2026-10-03 | Settings presets hold the parameter values only. The new directory `source/presets/` is owned by plugin-engineer. The VST3 program list stays unused. | User choice. The preset store is host-facing state, which plugin-engineer already owns. |
| 2026-10-03 | Architect defaults, open to change at the T-045 review: `channelMode` Mid, `underlay` Off, `soloWidth` 1 octave (1/12 to 4), LUFS and LU bars showing the programme loudness, anti-alias on, Align off. | Keeps today's behaviour as the default wherever possible. |
| 2026-10-04 | User decisions after T-043 and T-044:<br>• keep Alt for the zoom gestures, with an Alt key-up guard in T-037 and a hands-on test; a fallback is chosen only if the test fails;<br>• dBFS.30 spans −24 to +6 dBFS and dBFS.15 spans −9 to +6 dBFS;<br>• the integration default stays 1.00 ms, so the level part reacts like a peak meter by default (release 750 ms, peak hold 1000 ms);<br>• the LUFS and LU bars show one programme loudness value. | SPAN documents none of these values. The user confirmed the stated ballistics after being told that 1.00 ms reads like a peak meter. |
| 2026-10-04 | Architect decisions after T-043 and T-044:<br>• Align 0 dB uses T-043's calibration method: a 0 dBFS 1 kHz sine is modelled through the real mapper on the analysis thread, and the offset is recomputed when its inputs change. Its test signal set goes to T-047.<br>• The band solo uses two cascaded TPT/Simper state-variable band-passes, with a unity-peak output, edges at fc·2^(±W/2), Q = 0.6436 / (2^(W/2) − 2^(−W/2)), log-domain smoothing, coefficient updates every 16 to 32 samples, and 10 ms crossfades. A Linkwitz-Riley pair is rejected: it loses 11 dB at 1/12 octave.<br>• The shared preference file uses `PropertiesFile`, an `InterProcessLock` and `SharedResourcePointer`, with a lock, reload, change, save, unlock write cycle, and a reload when the editor opens.<br>• File dialogs use `launchAsync` only, with the chooser held as a member and cancelled when the editor closes.<br>• The colour wheel choice (an own wheel component, `juce::ColourSelector` plus a hex field, or the MIT-licensed reFX/ColourSelector) is made in T-045. Any third-party code goes through build-engineer.<br>• The sidechain bus declaration defect (a disabled default layout) is fixed now, in T-060, before T-037. | Research findings with sources (`docs/research/T-043-*.md`, `docs/research/T-044-*.md`). |

## Status

| Task | Owner | Status |
|---|---|---|
| T-042 | ui-ux-designer | DONE |
| T-043 | researcher | DONE |
| T-044 | researcher | DONE |
| T-045 | ui-ux-designer | IN_PROGRESS |
| T-046 | dsp-engineer | TODO |
| T-047 | dsp-engineer | TODO |
| T-048 | dsp-engineer | TODO |
| T-049 | dsp-engineer | TODO |
| T-050 | dsp-engineer | TODO |
| T-051 | plugin-engineer | TODO |
| T-052 | plugin-engineer | TODO |
| T-053 | plugin-engineer | TODO |
| T-054 | gui-engineer | TODO |
| T-055 | gui-engineer | TODO |
| T-056 | gui-engineer | TODO |
| T-057 | gui-engineer | TODO |
| T-058 | gui-engineer | TODO |
| T-059 | plugin-validator | TODO |

## User hands-on checklist (at the M3c gate)

Use the Release build, in a DAW (FL Studio, plus Reaper if available) and in the Standalone, with music playing.

- [ ] **Band solo:**
  - Hold Ctrl: a band preview follows the mouse, and the audio does not change.
  - Hold Ctrl and the left button: only the band plays, and moving the mouse sweeps it without clicks.
  - Ctrl+wheel changes the band width.
  - Releasing returns the full signal without a click.
  - Closing the editor while soloing returns the full signal.
- [ ] Alt+wheel zooms the level axis and Alt+drag box-zooms. Releasing Alt does not open the DAW's menus.
- [ ] Every metering mode shows a sensible scale. Density style shows where the level dwells. The integration, release and hold settings visibly change the bars.
- [ ] Each meter section can be hidden and shown again. The header button still hides all of them.
- [ ] **Channel and underlay:**
  - The Channel and Underlay controls show the expected curves.
  - Left plus Right looks like the old L/R mode, and Mid plus Side like the old M/S mode.
  - An old project opens with the same mode it had.
- [ ] The frequency range can start at 1 Hz. With a large block size, a sub-20 Hz tone is visible.
- [ ] With Align 0 dB on and slope 0, a 0 dBFS 1 kHz sine reads 0 dB at any block size. Turning anti-alias off makes the curve visibly crisper.
- [ ] **Two sidechains:** route two tracks (for example a kick and a bass) into Sidechain 1 and Sidechain 2. Both curves appear with their own colours and the labels you typed.
- [ ] **Colours:**
  - Change a curve colour with the wheel, then close and reopen the plugin and the DAW: the colour stays.
  - Save it as a colour preset, reset to default, then load the preset again.
- [ ] Set the frame rate to 30, 60 and 144 fps: the motion changes, and the DAW stays responsive at 60.
- [ ] **Reference slots:** take a reference, then change the mix; the reference stays. Rename it, offset it and hide it. Save the project, reopen it, and the reference is still there.
- [ ] **CSV:** export the curves and open the file in a spreadsheet. Import a CSV file into a slot. A broken file shows an error and changes nothing.
- [ ] **Settings presets:** save a preset, change settings, and load it back: the analysis and meter settings return, and the colours do not change.
