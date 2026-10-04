# Investigate: Roadmap

Last updated: 2026-10-03 by vst-architect (third session: milestone M3c planned with the user, `docs/plans/m3c-feature-expansion.md`, tasks T-042 to T-059; zoom modifiers moved to Alt).

## Vision

Investigate is a free real-time audio analyser plugin for mixing and mastering engineers. Its features are modelled on Voxengo SPAN. It shows a high-resolution FFT spectrum with SPAN-style controls: block size, window, overlap, slope, averaging, smoothing, a secondary max or average curve, and a hover readout of frequency, note and level. Next to the spectrum it shows level and loudness statistics (peak, RMS, true peak, clip count, and EBU R128 momentary, short-term and integrated loudness plus loudness range), a stereo correlation meter, and Mid/Side and per-channel spectrum modes. It also accepts up to two sidechain inputs (from M3c) and draws each as its own spectrum, so users can compare their mix against a reference track, or several instruments against each other. Investigate passes the signal through bit for bit with zero latency. The only exception is the M3c band solo: while the user holds it, the output plays just the band under the pointer.

Investigate reproduces SPAN's features and workflow, not its identity. It uses its own name, artwork, colours, fonts and text. No Voxengo trademarks, logos, assets or manual text are used.

## Scope

### In scope for v1
- A pass-through analyser effect with mono or stereo main input and output, plus an optional mono or stereo sidechain input.
- A real-time FFT spectrum:
  - block size from 512 to 65536
  - window choice, overlap, time averaging, fractional-octave smoothing, and slope with a 1 kHz pivot
  - a secondary curve (real-time max, all-time max or all-time average)
  - adjustable frequency and dB display ranges
  - a hover readout of frequency, musical note with cents, and level
  - a freeze toggle
- Channel modes: Mid, Side, Left, Right, L+R power mean, L/R dual curve and M/S dual curve.
- A sidechain reference spectrum drawn as its own curve.
- Level meters:
  - per-channel sample peak, RMS with an adjustable window, true peak (BS.1770 oversampling), and clip counters for both sample peaks and true peaks
  - a peak maximum and a true-peak maximum held since the last reset
  - a headroom readout (0 dBTP minus the true-peak maximum), derived in the editor
  - display scales: dBFS, K-20, K-14 and K-12
- Loudness to ITU-R BS.1770-4/-5 and EBU R128: momentary, short-term and integrated (gated) loudness, loudness range (EBU Tech 3342), and the maximum momentary and short-term values.
- A stereo correlation meter, and an L/R balance readout.
- A statistics reset, state save and restore, and a resizable editor.
- Interactions: freeze turns off automatically when `channelMode` changes; clicking the spectrum resets its accumulated curves; right-clicking copies the frequency under the pointer to the clipboard; a toggle collapses the meter panel, and its state is saved in `ViewState`.
- **Spectrum zoom and pan** (milestone M3b; feature plan in `docs/plans/spectrum-zoom-pan.md`). It is a view layer inside the home range set by the Frequency and Level chips, and it never writes parameters:
  - the wheel zooms frequency and Alt+wheel zooms level, both around the pointer (Alt replaced Ctrl on 2026-10-03, T-042);
  - left-drag and middle-drag pan;
  - a double-click returns to the full view (Fit);
  - Alt+drag zooms into a box;
  - an overview strip and a zoom indicator appear while zoomed;
  - zooming a frozen spectrum shows full detail.

  The zoom is not saved.
- **Feature expansion** (milestone M3c; feature plan in `docs/plans/m3c-feature-expansion.md`). The 14 items the user requested on 2026-10-03:
  - band solo: Ctrl+left-hold listens to the band under the pointer, Ctrl+wheel sets its width, and the spectrum outside the band is tinted;
  - SPAN's metering modes (14) and a Density style;
  - integration, release and peak-hold times;
  - each meter section can be hidden;
  - SPAN's primary channel plus underlay model;
  - frequencies down to 1 Hz;
  - Align 0 dB, and an anti-alias switch;
  - a second sidechain input, with labels for each input;
  - curve colours chosen with a colour wheel, which persist globally, with colour presets;
  - a frame-rate cap from 30 to 144 fps with Adaptive pacing;
  - four reference slots saved with the session;
  - CSV export and import;
  - settings presets, which never include colours.
- Windows 11 x64: VST3 and Standalone.
- A signed Windows installer for a free public release.

### Out of scope for v1
- Spectrogram or waterfall display (a SPAN Plus feature, planned for v2).
- macOS, AU, AAX and CLAP builds.
- Surround or more than two main channels.
- SPAN's multi-channel routing groups, routing between plugin instances, and more than two sidechain inputs.
- Match EQ, a full preset browser (tags, search, factory banks, the VST3 program list), A/B comparison and undo. A simple Presets menu and CSV export are in scope from M3c.
- Recolouring the whole theme (M3c recolours the spectrum curves only), and exporting meter statistics.
- Saving or automating the band solo.
- Loudness targets, platform presets (streaming targets) and automatic reset on transport start.
- Capturing system audio (WASAPI loopback) in the Standalone app.
- Saving the spectrum zoom with the session, keyboard shortcuts for zoom and pan, a zoom history, and zoom presets.

## Architecture

### Signal flow

```
Main in (L, R) ───────────────────────────────────────────────────────► Main out (untouched, bit-exact)
   │
   ├── Audio thread, processBlock (always runs)
   │     MeterBank
   │       ├── LevelMeter        sample peak, peak max, RMS (window), clip count
   │       ├── TruePeakDetector  BS.1770 polyphase oversampling, true peak and TP max
   │       ├── LoudnessMeter     K-weighting, 100 ms sub-blocks, M, S, I (histogram gating), LRA
   │       └── CorrelationMeter  normalised cross-correlation (500 ms window), L/R balance (3 s window)
   │     └──► MeterSnapshot published through atomics ──────────────► Editor meter panel (≥ 30 Hz)
   │
   └── Audio thread, only while analysis is active (the editor is open)
         SpectrumEngine producer side: push main L/R and sidechain L/R into a lock-free SPSC SampleFifo
                                  │
Sidechain in (L, R) ──────────────┘   (analysed only, never mixed into the output)

Editor (message thread) ── visible frequency range and freeze state + epoch, through atomics (M3b) ──┐
                                                                                                      ▼
Analysis thread (juce::Thread owned by the processor; runs only while the editor is open; polls every ~5 ms)
   SpectrumEngine consumer side:
     SampleFifo ─► channel mode combine ─► window ─► real FFT ─► power spectrum (bin domain)
       ─► exponential averaging (power domain, τ = avgTime / ln 100) ─► secondary accumulators (RT max, all-time max, all-time avg)
       ─► [while frozen: bin-domain snapshot taken at freeze time] (M3b)
       ─► SpectrumMapper: bins → log-spaced display points over the visible range (interpolate low, peak-preserve high)
          ─► fractional-octave smoothing (padded past the view edges) ─► slope (1 kHz pivot) ─► dB
          + 256-point overview of primary A over the home range (M3b)
       ─► FrameExchange (lock-free triple buffer) ──────────────────► Editor SpectrumView (VBlank, ≤ 60 fps)
     A view change re-maps the latest bin-domain spectra (or the frozen snapshot) and publishes at once, without a new FFT (M3b).
```

Zoom and pan (M3b) work as a view layer. The range parameters define the **home range**. The editor keeps a view window inside it, and tells the analysis thread only the visible frequency range; level zoom is editor-only. The full design is in `docs/plans/spectrum-zoom-pan.md`.

**M3c changes to the signal flow** (full diagram in `docs/plans/m3c-feature-expansion.md`):
- **A third input bus, "Sidechain 2"** (disabled, mono or stereo). It is pushed into its own FIFO and analysed only; it is never mixed into the output.
- **The band solo.** After MeterBank and the FIFO pushes, `BandSoloFilter` filters the main output, but only while the user holds the solo or it is crossfading out (10 ms). The meters and the spectrum always see the unfiltered input. The processor writes the output buffer only in that state, so the output is bit-exact at all other times. An editor heartbeat guards it: if the heartbeat goes stale for 250 ms, the audio thread releases the solo.
- **MeterBank** adds an integrated level (integration and release times) and a density histogram per channel.
- **The analysis thread** computes an optional underlay curve and a Sidechain 2 curve, maps down to 1 Hz with optional Align 0 dB, and serves spectrum captures for reference slots and CSV export.
- **The editor** reads and writes a machine-wide preference file (curve colours, frame-rate cap, Adaptive pacing, anti-alias), and settings and colour preset files, on the message thread only.

### Modules

| Module | Location | Owner | Thread | Purpose |
|---|---|---|---|---|
| `SampleFifo` | `source/dsp/` | dsp-engineer | audio (push), analysis (pop) | Lock-free SPSC multichannel ring buffer. The producer never blocks or allocates; on overflow it drops the incoming samples and increments an atomic counter. |
| `WindowFunctions` | `source/dsp/` | dsp-engineer | analysis | Hann, Blackman, Hi-Res (Nuttall squared) and flat-top windows with coherent-gain correction. |
| `SpectrumAnalyser` | `source/dsp/` | dsp-engineer | analysis | Framing with overlap, channel-mode combination, FFT, power spectrum, averaging and secondary accumulators, all in the bin domain. |
| `SpectrumMapper` | `source/dsp/` | dsp-engineer | analysis | Maps bins to log-spaced display points over the visible range, then applies smoothing (padded past the view edges), slope and dB conversion. It also maps the overview curve (M3b). |
| `SpectrumFrame`, `FrameExchange` | `source/dsp/` | dsp-engineer | analysis → message | The fixed-size frame struct and a lock-free triple buffer. |
| `SpectrumEngine` | `source/dsp/` | dsp-engineer | both | A facade: a real-time-safe producer API plus a consumer `processPending(settings)` call. From M3b it also re-publishes on a view change without a new FFT, and holds the freeze snapshot. |
| `DisplayRange` | `source/dsp/` | dsp-engineer | any | The effective-range formulas, plus (M3b) the minimum view spans and the view clamps shared by the mapper and the editor. |
| `LoudnessMeter` | `source/dsp/` | dsp-engineer | audio | BS.1770-4/-5 and EBU R128: M, S, I and LRA, using fixed-size histograms. |
| `TruePeakDetector` | `source/dsp/` | dsp-engineer | audio | BS.1770 Annex 2 polyphase FIR; the oversampling factor depends on the sample rate. |
| `LevelMeter` | `source/dsp/` | dsp-engineer | audio | Sample peak, RMS window and clip counting. |
| `CorrelationMeter` | `source/dsp/` | dsp-engineer | audio | Phase correlation from −1 to +1. |
| `MeterBank`, `MeterSnapshot` | `source/dsp/` | dsp-engineer | audio → any | Aggregates the meters, publishes snapshots through atomics, and handles reset requests. |
| Parameter layout and IDs | `source/params/` | plugin-engineer | message | The single source of parameter definitions. |
| `InvestigateProcessor` | `source/PluginProcessor.*` | plugin-engineer | all | Buses, state, `processBlock`, the analysis thread (a private class) and the editor-facing API. |
| `InvestigateEditor` and components | `source/PluginEditor.*`, `source/gui/` | gui-engineer | message | LookAndFeel, SpectrumView, MeterPanel, settings controls and resizing. From M3b: zoom and pan (`ZoomMath`), the zoom indicator, the overview strip and box zoom. |
| `BandSoloFilter` (M3c) | `source/dsp/` | dsp-engineer | audio | The band-pass behind the band solo: smoothed centre and width, and a 10 ms crossfade on engage and release. |
| `ReferenceGrid`, `SpectrumCapture` (M3c) | `source/dsp/` | dsp-engineer | analysis → message | A fixed 4096-point log grid from 1 Hz to 48 kHz, and an on-request full-range capture of every curve, for reference slots and CSV. |
| `PresetStore`, `UserDataPaths` (M3c) | `source/presets/` | plugin-engineer | message | Settings presets (`*.invpreset`, parameter values only) and the user data folder layout. |
| `GlobalSettings`, `ColourWheel`, colour presets, `CsvSpectrum` (M3c) | `source/gui/` | gui-engineer | message | Machine-wide display preferences (curve colours, frame-rate cap, Adaptive, anti-alias), the colour editor, colour presets (`*.invcolours`), and the CSV parser and writer. |

All C++ code lives in namespace `investigate`.

### Processor API used by the editor (implemented in T-009)
- `void setAnalysisActive (bool)`: the editor calls this from its constructor and destructor. It starts or stops the analysis thread and FIFO pushing.
- `bool getLatestSpectrumFrame (SpectrumFrame& dest)`: message thread only. It returns `true` if a new frame arrived since the last call.
- `MeterSnapshot getMeterSnapshot() const noexcept`: lock-free and callable from any thread.
- `void requestStatsReset()`: resets peak max, true-peak max, clip counts, integrated loudness, LRA, and the max momentary and short-term values.
- `void requestSpectrumReset()`: clears the all-time max and all-time average curves.
- `bool isSidechainActive() const noexcept`.
- `juce::AudioProcessorValueTreeState& getApvts() noexcept`.
- Freezing the display was editor-only until M3b: the editor stops pulling frames.
- Added in M3b (T-036), message thread, lock-free:
  - `void setSpectrumView (float fLo, float fHi) noexcept`: sets the visible frequency range; `(0, 0)` means the home range.
  - `void setSpectrumFrozen (bool) noexcept`: the engine keeps a bin-domain snapshot while frozen, and every change increments a freeze epoch carried by each frame.
  - `uint32_t getFreezeEpoch() const noexcept`.
  - `setAnalysisActive (false)` resets both, so the editor always opens at Fit and unfrozen.
- Added in M3c (T-052), message thread, lock-free:
  - `void setBandSolo (bool active, float centreHz) noexcept` (one packed 64-bit atomic) and `void bandSoloHeartbeat() noexcept`. The width comes from the `soloWidth` parameter. The audio thread releases the solo if the heartbeat stalls for 250 ms, and `releaseResources` and the last `setAnalysisActive (false)` release it too.
  - `bool isSidechainActive (int index) const noexcept` for Sidechain 1 (0) and Sidechain 2 (1). The no-argument form stays and means index 0.
  - `void requestSpectrumCapture() noexcept` and `bool getSpectrumCapture (SpectrumCapture& dest) noexcept`.

### SpectrumFrame contents (finalised in T-005)
- Up to 2048 display points, log-spaced between `freqLow` and `min(freqHigh, Nyquist)`, with the frequency of each point.
- A primary curve in dB, a second primary curve for the dual modes (R or S), a secondary curve for each primary curve (so two secondary curves in the dual modes), and the sidechain curve.
- Flags: `isDual`, `hasSecondary`, `hasSidechain`. Also the sample rate and a sequence number.
- In dual modes the sidechain curve shows the power mean of the sidechain channels.
- Added in M3b (T-035):
  - the view range the frame was mapped for (`viewFreqLow`, `viewFreqHigh`);
  - a 256-point overview of primary A over the effective home range (`overviewFreqHz`, `overviewDb`);
  - the `freezeEpoch` it was produced under.
- Changed in M3c (T-046), additively:
  - `primaryBDb`, `secondaryBDb` and `isDual` now carry the underlay curve, its secondary curve and "an underlay is present". The legacy dual modes produce the same frames as before.
  - `sidechainDb` and `hasSidechain` are Sidechain 1; `sidechain2Db` and `hasSidechain2` are new.
  - The display points may start at 1 Hz (T-047).

### MeterSnapshot contents (finalised in T-007)
- Per channel: sample peak and true peak, each the maximum over the last 50 ms, so a 30 to 60 Hz reader cannot miss block peaks; peak max and true-peak max since reset; RMS; sample clip count; and true-peak clip count.
- `blockCounter`: incremented on every `process` call. The editor uses it to detect a stopped or bypassed host and let the bars fall.
- Loudness: momentary, short-term, integrated, LRA, max momentary and max short-term.
- Correlation (500 ms), L/R balance in dB over the last 3 s (negative means the left channel is louder), `isMono` and the sample rate.
- All levels are reported in dBFS (RMS is not sine-referenced). The editor applies the +3.01 dB AES17 offset and the K-system reference when `meterScale` is K-20, K-14 or K-12. Crest factor is derived in the editor as peak minus RMS.
- Added in M3c (T-049), additively:
  - `level[ch]`: the integrated level, with the integration and release times applied, as the maximum over the last 50 ms;
  - `density[ch][204]`: a density histogram in 0.5 dB bins from −96 to +6 dBFS, normalised from 0 to 1;
  - per-channel momentary and short-term loudness, only if T-043 shows that SPAN meters loudness per channel.

### Parameter table

No parameter changes the audio, except `soloWidth` while the band solo is held (M3c); `BandSoloFilter` smooths it per sample. No other parameter needs smoothing, and every change takes effect at the next analysis frame or meter block. All parameters are APVTS parameters, so they are saved with the state and in settings presets (M3c). All can be automated except `soloWidth`. Rows marked (M3c) are added or changed by T-051. Existing IDs and choice indices are preserved, so saved sessions load unchanged. They use `juce::ParameterID { id, 1 }`. Values marked * are provisional: T-001 will confirm or correct them against SPAN, and any change will be logged in the Decisions section.

| ID | Name | Type | Range / choices | Default | Unit | Skew | Smoothing |
|---|---|---|---|---|---|---|---|
| `channelMode` | Channel (M3c; was "Mode") | choice | Mid, Side, Left, Right, L+R (power mean, shown as "Stereo" from M3c), L/R dual, M/S dual | Mid* | – | – | none. From M3c the editor offers the first five. L/R dual and M/S dual stay valid aliases for Left plus underlay Right and Mid plus underlay Side. |
| `underlay` (M3c) | Underlay | choice | Off, Mid, Side, Left, Right, Stereo | Off | – | – | none. Ignored while `channelMode` holds a legacy dual value. |
| `fftSize` | Block size | choice | 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536 | 4096* | samples | – | none |
| `overlap` | Overlap | choice | 1x, 2x, 4x, 8x | 4x* | – | – | none |
| `window` | Window | choice | Hann, Blackman, Hi-Res (Nuttall squared), Flat-top | Hann | – | – | none |
| `avgTime` | Avg time | float | 0 to 10000; the time for the spectrum to fall 20 dB | 1000* | ms | centre at 500 ms | none |
| `smoothing` | Smoothing | choice | Off, 1/48, 1/24, 1/12, 1/6, 1/3 octave | 1/12 oct* | – | – | none |
| `slope` | Slope | float | −12 to +12, step 0.5 | 4.5 | dB/oct | linear | none |
| `secondary` | Secondary curve | choice | Off, Real-time max, All-time max, All-time average | Real-time max* | – | – | none |
| `freqLow` | Frequency low | float | 1 to 1000 (M3c; was 10 to 1000) | 20 | Hz | log (centre 31.6 Hz from M3c; was 100 Hz) | none |
| `freqHigh` | Frequency high | float | 1000 to 40000, clamped to Nyquist at display | 20000 | Hz | log (centre 6325 Hz) | none |
| `dbLow` | Range low | float | −150 to −30, step 1 | −90* | dB | linear | none |
| `dbHigh` | Range high | float | −30 to +30, step 1 | +6* | dB | linear | none |
| `showSidechain` | Sidechain trace ("Sidechain 1 trace" from M3c) | bool | off / on | on | – | – | none |
| `showSidechain2` (M3c) | Sidechain 2 trace | bool | off / on | on | – | – | none |
| `align0dB` (M3c) | Align 0 dB | bool | off / on | off | – | – | none |
| `meterScale` | Meter scale ("Metering" from M3c) | choice | dBFS, K-20, K-14, K-12; from M3c, appended: dBFS.30, dBFS.15, dBFS+3, K-20 C, K-14 C, K-12 C, LUFS ML, LUFS SL, LU ML, LU SL | dBFS | – | – | none |
| `meterStyle` (M3c) | Meter style | choice | Bars, Density | Bars | – | – | none |
| `meterIntegration` (M3c) | Integration time | float | 0.1 to 3000; time to rise or fall 20 dB | 1.00 | ms | log (centre 30 ms) | none. Fixed at 600 ms in the K-xx C modes. |
| `meterRelease` (M3c) | Release time | float | 10 to 5000; time to fall 20 dB | 750 | ms | log (centre 500 ms) | none. Fixed at 600 ms in the K-xx C modes. |
| `meterPeakHold` (M3c) | Peak hold time | float | 0 to 10000 | 1000 | ms | centre 1000 ms | none |
| `soloWidth` (M3c) | Solo width | float | 1/12 to 4 | 1.0 | oct | log (centre 0.58 oct) | per-sample smoothing in `BandSoloFilter`; not automatable |
| `rmsWindow` | RMS window | float | 50 to 3000 | 300 | ms | log (centre 300 ms) | none (from M3c it drives the statistics RMS only; the bars use `meterIntegration`) |

Constraints:
- The mapper (T-005) and the editor (T-012) compute the effective display range with exactly these formulas: `fHiEff = min(freqHigh, sampleRate / 2)`, `fLoEff = min(freqLow, fHiEff / 2)`, `dbHiEff = dbHigh`, and `dbLoEff = min(dbLow, dbHigh − 12)`. Parameters are never rewritten; only the effective values are clamped.
- `avgTime` follows SPAN's convention: it is the time for the displayed level to fall by 20 dB. The primary curve's power-domain exponential average uses τ = avgTime / ln(100). The real-time max secondary curve falls linearly in dB at 20 dB per `avgTime`. With `avgTime = 0` there is no averaging, and the real-time max equals the current frame.
- Correlation uses a fixed 500 ms averaging window (SPAN User Guide p. 5–6). The L/R balance readout uses a fixed 3 s window.
- The values still marked * are not documented anywhere in Voxengo's guides (confirmed by T-020). They are final product decisions, open to change after user testing.
- M3c:
  - The M3c ranges and defaults for the metering and ballistics parameters are architect interpretations of SPAN. T-043 may refine them before T-049 and T-051 start, and any change is logged in the Decisions.
  - Auto-unfreeze and the DSP-side secondary reset trigger when the **effective** primary or underlay changes.
  - The meaning of every M3c mode and parameter is specified in `docs/plans/m3c-feature-expansion.md`.

### Buses and channel layouts
- Main input and output: mono to mono, or stereo to stereo (the same layout on both sides).
- Sidechain input ("Sidechain 1"): disabled, mono or stereo. Sidechain samples are never written to the output.
- From M3c, a second sidechain input ("Sidechain 2", input bus 2): disabled by default, mono or stereo, independent of Sidechain 1, and never written to the output. Some hosts offer only one sidechain (Risk 15).
- Latency: 0 samples. Tail: 0 seconds.

### Threading model

| Thread | Work | Communication |
|---|---|---|
| Audio | `processBlock`: `ScopedNoDenormals`, MeterBank process, and FIFO push while analysis is active. The output buffer is never written, except by `BandSoloFilter` while the band solo is engaged or crossfading out (M3c). | Publishes the MeterSnapshot through atomics. Reads reset requests from atomic flags. Pushes samples into the SPSC FIFOs. Reads parameters through raw APVTS atomics. From M3c it also reads the band-solo word and the heartbeat counter once per block, and releases the solo itself when the heartbeat is stale for 250 ms. |
| Analysis | A `juce::Thread` owned by the processor. It starts on `setAnalysisActive(true)`, polls with `wait(5)`, runs `SpectrumEngine::processPending`, and stops within 50 ms. It may reallocate when `fftSize`, the window or the sample rate changes. | Pops samples from the FIFOs. Publishes frames into the triple buffer. Reads parameters through raw APVTS atomics. From M3c it serves capture requests on its next poll, from preallocated buffers, through a lock-free single-slot exchange. |
| Message | Editor painting via `juce::VBlankAttachment`, parameter attachments, and state save and restore. | Reads the latest frame and the MeterSnapshot. Sets reset flags. From M3b it also writes the visible frequency range (two floats packed into one 64-bit atomic) and the freeze flag with its epoch (one atomic), and drops frames whose epoch is stale. From M3c it also: makes the band-solo calls and the heartbeat; reads captures; handles the preference file, the preset files and the CSV files (asynchronous file choosers, debounced preference writes). |

The audio thread never signals the analysis thread, because an OS event signal is not real-time safe. The analysis thread polls instead.

### State format
- APVTS `ValueTree` of type `InvestigateState`, with property `stateVersion = 1`.
- A child `ViewState` holds editor-only persistent properties: `editorWidth`, `editorHeight` and `metersCollapsed`, plus later UI preferences. The editor reads and writes it on the message thread. The spectrum zoom and the freeze state are not persisted (M3b decision).
- The tree is serialised with `copyXmlToBinary`. Loading tolerates missing properties (they keep their defaults), unknown properties (ignored) and corrupt data (defaults remain, no crash).
- **M3c (T-051):** `stateVersion` becomes 2. `ViewState` gains:
  - `meterSections`;
  - `inputLabel0`, `inputLabel1` and `inputLabel2`;
  - a `ReferenceSlots` child (four slots on the reference grid, at most about 90 KB).

  No parameter value needs migrating, because every existing ID and choice index is kept. Version-1 states load unchanged.
- **Outside the session state (M3c):**
  - a machine-wide preference file (`%APPDATA%\Investigate Audio\Investigate\Investigate.settings`: curve colours, frame-rate cap, Adaptive, anti-alias);
  - settings presets (`Presets\Settings\*.invpreset`: parameter values only);
  - colour presets (`Presets\Colours\*.invcolours`).

  The folder name follows the final company name chosen before M5.

### Identity (placeholders, change before the first public release)
- Product name `Investigate`; company name `Investigate Audio`; manufacturer code `Ivga`; plugin code `Ivst`; bundle ID `com.investigateaudio.investigate`.
- VST3 category `Fx|Analyzer`.
- Changing the codes after a public release breaks saved host sessions, so they must be final before M5.

## Budgets

Measured on the development machine in a Release build, at 48 kHz, 128-sample blocks, stereo main plus stereo sidechain, with analysis active.

| Item | Budget |
|---|---|
| Latency | 0 samples |
| Audio output | bit-exact to the input (null test difference exactly 0) whenever the band solo is not engaged (M3c) |
| `processBlock` CPU | mean ≤ 20 µs and p99 ≤ 50 µs per block (0.75 % and 1.9 % of the 2.67 ms block) |
| MeterBank alone | mean ≤ 15 µs and p99 ≤ 40 µs per block |
| Analysis thread, default settings | ≤ 2 % of one core |
| Analysis thread, worst case (65536, 8x, dual mode, sidechain, 192 kHz) | ≤ 10 % of one core |
| Editor paint | p95 ≤ 4 ms at 1280×720 (100 % scale); p95 ≤ 8 ms at 2560×1440 |
| Message thread at 60 fps | ≤ 25 % of one core |
| Memory | ≤ 64 MB resident increase per instance at maximum settings |
| Analysis thread stop | ≤ 50 ms after `setAnalysisActive(false)` |
| Zoom: view change to re-mapped frame published (M3b) | p95 ≤ 10 ms and max ≤ 20 ms, at any `fftSize` and overlap |
| Zoom: one re-map, every curve plus the overview, worst case (M3b) | ≤ 1.0 ms on the analysis thread |
| Analysis thread during a continuous 60 fps pan, default settings (M3b) | ≤ 4 % of one core |
| Whole editor during a continuous pan or zoom (M3b) | message thread ≤ 25 % of one core; paint p95 ≤ 4 ms (1280×720) and ≤ 8 ms (2560×1440) |
| `processBlock` with the band solo engaged, stereo main plus two stereo sidechains (M3c) | mean ≤ 20 µs and p99 ≤ 50 µs per block (unchanged budget) |
| `BandSoloFilter` alone, stereo, 128 samples (M3c) | mean ≤ 3 µs per block |
| Band solo response (M3c) | follows a press, release or sweep within one editor update plus one audio block; releases itself within 250 ms + one block when the editor heartbeat stops |
| MeterBank with integration, release and density (M3c) | mean ≤ 15 µs and p99 ≤ 40 µs per block (unchanged budget) |
| Analysis thread with both sidechains (M3c) | ≤ 2 % at the defaults; ≤ 10 % in the worst case (65536, 8x, an underlay, both secondary curves, two stereo sidechains, 192 kHz) |
| Spectrum capture (M3c) | request to available ≤ 20 ms; mapping ≤ 2 ms on the analysis thread |
| Whole editor with every M3c layer visible, 60 fps cap, Adaptive on (M3c) | message thread ≤ 25 %; paint p95 ≤ 4 ms and ≤ 8 ms. Caps above 60 fps are measured and reported, not gated. |
| Preset load / CSV import of 65,536 rows / preference writes (M3c) | ≤ 50 ms / ≤ 200 ms on the message thread / ≤ 2 writes per second |
| Session state with four full reference slots (M3c) | ≤ 150 KB |

## Milestones

| Milestone | Content | Tasks | Gate |
|---|---|---|---|
| M0 Scaffold | Research, CMake project, pinned JUCE, Catch2 and pluginval, a placeholder pass-through plugin | T-001, T-002, T-003 | The placeholder VST3 builds, pluginval passes at strictness 10, CTest runs, and both research reports are delivered |
| M1 DSP core | DSP dependencies, then the spectrum pipeline and meters in `source/dsp/` with unit tests | T-021, T-004, T-005, T-006, T-007, T-024 | Unit QA reports PASS, including the EBU conformance cases |
| M2 Parameters and state | APVTS, buses, state, and DSP wiring with the analysis thread | T-008, T-025, T-026, T-009 | Unit and validation QA PASS: bit-exact null test, state round trip, and real-time safety |
| M3 UI | Design spec, then the editor shell, SpectrumView, the rendering CPU pass and MeterPanel | T-010, T-011, T-012, T-029, T-013 | The spec is accepted; QA PASS on the editor; the paint budget and the message-thread budget are met |
| M3b Zoom and pan | A view-layer spectrum zoom and pan (`docs/plans/spectrum-zoom-pan.md`): the spec and mockup, engine re-mapping and the freeze snapshot, the processor API, the pan margin and bin spacing, the editor gestures, the overview strip and box zoom | T-034, T-035, T-036, T-039, T-037, T-038 | The user approves the T-034 mockup before T-037 starts. Every QA report is PASS, and the zoom budgets are met during a continuous pan. The user completes the hands-on checklist in the plan. |
| M3c Feature expansion | The 14 user items of 2026-10-03 (`docs/plans/m3c-feature-expansion.md`):<br>• research and the UI spec addendum (T-042 to T-045);<br>• additive DSP work (T-046 to T-050);<br>• parameters, processor wiring and the preset store (T-051 to T-053);<br>• the editor (T-054 to T-058);<br>• the gate validation (T-059). | T-042 to T-059 | The user approves the T-045 mockup before T-054 starts. Every QA report for T-046 to T-058 says PASS, `docs/qa/T-059-validation.md` says PASS, and the M3c budgets are met. The user completes the hands-on checklist in the plan. |
| M4 Validation | Validation tooling, the loudness start-of-measurement fix, the full black-box suite, performance, and a real-time safety and sanitizer audit | T-014, T-033, T-015, T-016, T-017 | All QA reports PASS and all budgets are met. T-015 to T-017 also cover zoom and pan, and the M3c features; they depend on T-059. |
| M5 Packaging | Windows CI, then a signed installer and release docs | T-018, T-019 | CI is green; a signed installer installs, uninstalls and passes pluginval on the installed binary |

## Working rules for this project
- At most one task that builds C++ may be in flight in the main working tree at a time. This avoids build-directory collisions, and half-written files breaking another agent's build. Documentation tasks (research, design) may run in parallel with it.
- Source directories are picked up by CMake globbing with `CONFIGURE_DEPENDS` (set up in T-003). Coders therefore add files without editing CMake.
- Every coder task ends with a unit QA report. Tasks that touch the plugin binary also end with a validation QA report.

## Handoff (architect session of 2026-10-03, third session of the day: M3c planning)

Status at handoff:
- M0 to M2 are closed. In M3 every task is DONE, including T-013 and T-032.
- M3 stays open only for the user's hands-on review (`docs/plans/m3-user-review.md`), which the user deferred. Triage its findings into tasks when they arrive, including the frame-pacing look as a decision point.
- M3b: T-034 (mockup approved by the user without changes), T-035, T-036, T-040 and T-039 are DONE with passing QA reports. T-041, T-037 and T-038 are TODO.
- **M3c planned in this session** with the user (`docs/plans/m3c-feature-expansion.md`, tasks T-042 to T-059). The documents of this session (the roadmap, both plans, the new task files, and the amended T-015, T-016, T-017, T-037 and T-038) are not committed yet. No agent is running.
- **The zoom modifiers changed by user decision:** Alt+wheel for level zoom, Alt+left-drag for box zoom; Ctrl is reserved for the band solo. T-042 updates the spec before T-037 starts.
- Raw QA log folders (`docs/qa/*-logs/`) are gitignored and not committed.

The next session continues with:
1. **Commit** this session's documents (build-engineer, or the user), with an explicit path list.
2. **In parallel:**
   - **T-041** (plugin-engineer): a one-line constant change in `source/PluginProcessor.cpp`, with a short unit QA report. It builds C++, so it runs alone.
   - **T-042** (ui-ux-designer): Alt modifiers in the spec and the mockup.
   - **T-043** and **T-044** (researcher): the SPAN reference and the host and platform constraints.

   All three are documentation only.
3. **T-037** (gui-engineer), once T-041 and T-042 are DONE: the editor zoom, pan, Fit, zoom panel indicator and deep-zoom grid, with Alt+wheel level zoom. Its task log carries everything it needs from T-034, T-035, T-036 and T-039, including the legend and FROZEN badge measurement at 920×540. It needs unit and validation QA reports, including the continuous-pan CPU gate.
4. **T-038** (gui-engineer): the overview strip and Alt+drag box zoom. After T-038, the user runs the M3b hands-on checklist in `docs/plans/spectrum-zoom-pan.md`.
5. **T-045** (ui-ux-designer), once T-042, T-043 and T-044 are DONE. It runs in parallel with T-037 and T-038. The user then approves the M3c mockup.
6. **The M3c C++ chain, one task at a time:**
   - T-046 to T-050 (dsp-engineer, additive);
   - T-051 to T-053 (plugin-engineer);
   - T-054 to T-058 (gui-engineer, after the mockup approval);
   - then T-059 (plugin-validator), and the user's M3c checklist.
7. Then M4 (T-033, T-014 to T-017) and M5 (T-018 and T-019).

M3c rules for every dispatch:
- The DSP tasks (T-046 to T-050) must stay additive. The current processor and editor must build and pass unchanged after each of them.
- Existing parameter IDs and choice indices are never changed (roadmap Decision of 2026-10-03).
- `source/presets/` is owned by plugin-engineer. gui-engineer only includes `UserDataPaths.h` from it.

Rules learned in this session. Include them in every dispatch prompt:
- Keep timing assertions out of the default test run. Functional tests may use only loose, bounded waits (for example 500 ms); budgets go in `[.perf]` cases (T-040).
- After a coder or tester session returns, a background process of theirs may still be running. Before committing or starting the next C++ task, have build-engineer check for running `cmake`, `MSBuild`, `cl`, `link`, `ctest` and `InvestigateUnitTests` processes and wait for them.
- Commits stage explicit path lists only, never `git add -A`.

Rules from the first session of 2026-10-03:
- CMake refuses in-source builds (T-030). Always build in `build/` through the presets.
- Run `ctest` with `--timeout 300` until T-014 adds a default timeout.
- New on-screen GUI tests use `tests/unit/GuiTestEnvironment.h`: gate on the environment, SKIP with a reason, and give every wait a deadline (T-031).
- Editor rendering: follow the rules at the end of the T-029 log. Gate CPU with the T-012/T-029 harness method, median of 3 runs. `paint()` timing alone hides Direct2D cost.
- When a coder session stops at the usage limit, the architect can dispatch the testers directly, rather than starting a cold coder session.
- Run only one architect session at a time. A parallel session wrote T-032 at the same time as this one changed the same documents.

Rules from the session that ended 2026-09-29:
- Only one task that builds C++ runs at a time in the main tree. Documentation and research tasks may run in parallel with it.
- Coders never edit `docs/qa/`. Only the testers write their reports and verdicts.
- Timing tests use the hidden Catch2 tag `[.perf]` and assert roadmap budgets. They are run explicitly with `InvestigateUnitTests.exe "[perf]"`.
- `third_party/` is gitignored as a whole. Committed build scripts go in `cmake/`.
- ui-ux-designer has no shell. Binary downloads go to build-engineer.
- If a dispatch fails with "auto mode classifier gave no verdict", do some read-only work, then retry. If agents stop with HTTP 429 (usage limit), resume from the task files once the limit resets; task logs record progress.

Open user decisions:
- **Before M5 (T-019):** JUCE licence tier (Starter or AGPLv3), code-signing method, final company name and plugin codes, installer license text, and the first version number.
- None for T-018: the repository exists (T-027) at https://github.com/Keray535/Investigate. It is private, and the default branch is `main`. `CLAUDE.md` and `.claude/` are gitignored and never committed.
- Commit hygiene: every task that changes files should end with a commit and push by build-engineer, or by the user, so the remote stays current.

## Risks

| # | Risk | Impact | Mitigation |
|---|---|---|---|
| 1 | Painting dense filled paths at 60 fps is too slow at high DPI | Laggy UI, high message-thread CPU | Reduce the curve to about one point per pixel column, cache the static grid as an image, use `VBlankAttachment`, and profile in T-012. **Materialised in T-012** (2026-10-03): paint is fast, but Direct2D work outside `paint()` costs up to 42 % of the message thread at 2560×1440. **Mitigated in T-029:** sprite-batch fills, vertex reduction and frame pacing bring it to ≤ 16.8 %. The residual risk is the meter panel's added repaints (T-013 gates the whole editor at ≤ 25 %), and the 2560×1440 numbers have not been confirmed on a native 1440p display. |
| 2 | `juce::dsp::FFT` without IPP is slow for 65536 points | Analysis CPU budget missed | Resolved by choice: pffft (BSD-like, SIMD) is the product FFT (T-021, T-004). T-004 benchmarks it against `juce::dsp::FFT` on the development machine. The FFT runs off the audio thread and sits behind an interface. |
| 3 | Loudness or true peak deviates from EBU conformance | Wrong numbers; loss of credibility | Test against the EBU Tech 3341 and 3342 cases, synthesised or with the official files if their license allows. Optionally cross-check against libebur128 (MIT) as a test-only oracle after a license check. |
| 4 | Clone concerns: trade dress or trademark | Legal or takedown risk | Own name and artwork only; no Voxengo text, assets or colour-scheme copies. T-001 lists what is off-limits. |
| 5 | FIFO overflow when the analysis thread is starved | Spectrum glitches | Drop samples and count the overflow; the consumer resynchronises. FIFO capacity ≥ 2^17 frames per channel. The overflow counter is visible in debug builds. |
| 6 | Hosts with odd sidechain handling (bus disabled, or present but silent) | Wrong or missing trace | `isSidechainActive()` reflects the enabled bus layout; the trace is hidden when inactive; T-015 tests the layouts. |
| 7 | The Standalone mutes input by default ("Mute audio input") | The analyser shows silence in the Standalone | T-008 sets the Standalone's mute-input default to off (T-002 section 9). The output is a bit-exact copy of the input, so users should use headphones or a separate output device to avoid acoustic feedback when the input is a microphone. T-015 checks the behaviour. |
| 8 | An unsigned installer triggers SmartScreen warnings | Poor user trust | Choose a signing method (for example Azure Trusted Signing or an OV certificate) before M5. |
| 9 | JUCE license choice for a free closed-source release | Cannot ship legally | The user chooses before M5: JUCE Starter licence (closed source, with a revenue cap) or AGPLv3 (the source must be published). T-002 reports the Starter cap as $20,000 per year of gross revenue, not the $50k assumed earlier. The user must read the current JUCE 8 EULA before deciding. |
| 10 | Parallel agents collide in the same working tree | Broken builds, lost work | Apply the one-C++-task-at-a-time rule; revisit worktree isolation after M1 if throughput matters. |
| 11 | Rebuilding the grid and labels on every frame during a pan or zoom exceeds the message-thread budget (M3b) | Laggy pan; a sluggish host UI | Grid lines as one rectangle list per colour during an interaction, label images reused while their text is unchanged, and the T-029 rules. T-037 and T-038 gate CPU during a continuous pan at both sizes. |
| 12 | A live frame published just before a freeze, or a torn view pair, shows the wrong data (M3b) | Flicker, or a wrong curve after a freeze or zoom | A freeze epoch in every frame (the editor drops stale epochs), and the view pair packed into one 64-bit atomic. T-036 stress-tests both. |
| 13 | Zooming past the FFT resolution shows straight interpolated segments, which may look like a bug (M3b) | User confusion | The zoom indicator shows the bin spacing when fewer than 16 bins are in view. |
| 14 | Windows precision touchpads deliver pinch as Ctrl+wheel, so pinch zooms level, not frequency (M3b) | Unexpected on laptops | **Superseded by Risk 18** (2026-10-03): level zoom moved to Alt+wheel. |
| 15 | Some hosts offer only one sidechain input for VST3 (Ableton Live, for example), or route a second one awkwardly (M3c) | Sidechain 2 unusable there | T-044 surveys the hosts, FL Studio first. An inactive bus shows "no input". No feature depends on Sidechain 2. The README documents host support. |
| 16 | The band solo changes the audio: clicks, a stuck solo, or an accidental solo during a mix or a render (M3c) | Audible damage; loss of trust in a pass-through tool | The solo only works while held. 10 ms crossfades. A 250 ms heartbeat failsafe on the audio thread. Release on Ctrl or button up, focus loss, editor close and `releaseResources`. Null tests with the solo idle. Never saved, never automatable. |
| 17 | On Windows, releasing Alt after Alt+wheel or Alt+drag may activate the host's menu bar (M3b/M3c) | Zoom gestures feel broken in some hosts | T-044 researches it. T-037, T-038 and T-059 record the behaviour in FL Studio, Reaper and the Standalone. If it is confirmed in a major host, the architect brings a fallback modifier to the user. |
| 18 | A touchpad pinch arrives as Ctrl+wheel, which is now reserved for the band solo: until M3c it does nothing, and from M3c it changes the solo width | Unexpected on laptops | Accepted, and documented in the checklists. A horizontal swipe still pans frequency. |
| 19 | Several instances or processes share the preference file (bridged or sandboxed hosts) (M3c) | Lost or torn updates | `InterProcessLock`, debounced writes, reload on editor open, defaults on a corrupt file. Never touched by the audio thread. |
| 20 | User-chosen colours break the colour-blind-safe distinctions or the contrast (M3c) | Curves hard to tell apart | Curve styles still differ by fill, width and dash. A contrast warning in the colour editor. Reset to default. |
| 21 | 14 items in 18 tasks delay the v1 release (M3c) | Later release | The M3c gate. The user can drop or defer items at the T-045 mockup review. M4 and M5 are otherwise unchanged. |
| 22 | Align 0 dB behaves differently from SPAN or from user expectations (M3c) | Confusing levels | T-043 researches it. A numeric criterion with sines in T-047. Off by default, with today's frames bit-identical. |
| 23 | Sessions or automation saved with earlier builds load differently after M3c | Lost settings or a changed view | Existing parameter IDs and choice indices are preserved: the legacy dual modes become aliases, and the new metering modes are appended. T-051 and T-059 test stored version-1 states. |

## Decisions

| Date | Decision | Reason |
|---|---|---|
| 2026-09-28 | The product is named "Investigate": a feature clone of Voxengo SPAN with its own identity. | User request. Features are not protected; name and trade dress are. |
| 2026-09-28 | v1 scope: spectrum, level and loudness meters, correlation and M/S modes, and a sidechain reference trace. The spectrogram moves to v2. | User choice. The spectrogram adds significant UI and GPU work. |
| 2026-09-28 | Windows only: VST3 and Standalone. No macOS, AU or CI Mac runner. This overrides the macOS/AU line in the CLAUDE.md baseline. | User choice. |
| 2026-09-28 | Visual direction: dark and SPAN-inspired, with our own artwork, colours and fonts. | User choice. |
| 2026-09-28 | Distribution: free public release with a signed Windows installer. The JUCE license tier and signing method are open until M5. Copyleft dependencies are rejected unless the user decides otherwise. | User choice. The rest follows from the CLAUDE.md rules. |
| 2026-09-28 | Audio passes through bit-exact with zero latency. Meters run on the audio thread (so integrated loudness never misses samples). The spectrum runs on a separate analysis thread only while the editor is open. | Correctness of loudness; low audio-thread cost; no wasted CPU when the UI is closed. |
| 2026-09-28 | All analysis and display settings are APVTS parameters without smoothing. | They are saved with state and are host-visible, and none of them is audible. |
| 2026-09-28 | Placeholder identity codes: `Ivga` / `Ivst`, company "Investigate Audio". | These must exist from M0. The user may change them before M5. |
| 2026-09-28 | One-time ownership exception in T-003: build-engineer creates placeholder `source/PluginProcessor.*`, `source/PluginEditor.*` and `tests/unit/SmokeTest.cpp`. Ownership reverts to plugin-engineer, gui-engineer and unit-tester when T-003 is DONE. | M0 requires a buildable plugin and a running test target before those owners start. |
| 2026-09-28 | CMake globs source and test directories with `CONFIGURE_DEPENDS`. | Coders add files without concurrent CMake edits. |
| 2026-09-28 | GitHub Actions CI moves to M5 (T-018), because no remote repository exists yet. | Nothing to run CI against until the user creates a remote. |
| 2026-09-28 | Baseline dependencies (JUCE 8, Catch2 v3, pluginval) are pre-approved by CLAUDE.md, so T-003 may start before T-002. T-002 still records their licenses. Any other dependency waits for a researcher license check. | Unblocks M0 without breaking the license rule. |
| 2026-09-28 | New directory `packaging/` (installer scripts) is owned by build-engineer. | Not covered by the CLAUDE.md layout; needed for T-019. |
| 2026-09-28 | The mockup from T-010 needs user approval before editor implementation (T-011) starts. | The visual direction is the user's call; approving it early avoids rework. |
| 2026-09-28 | T-001 confirmed the `slope` default of 4.5 dB/oct. Every other * default stays provisional, because the SPAN guide PDF is a scanned image. T-020 retries with page-image extraction, and T-010 waits for T-020. | Parity on defaults and interactions matters for a SPAN-style tool; the retry is cheap. |
| 2026-09-28 | Added to scope: a headroom readout (0 dBTP minus TP max), derived in the editor with no new DSP. Not added: a separate "hold peaks" control, because the All-time max secondary curve already covers it. | T-001 suggestion. SPAN advertises headroom estimation; it costs nothing to derive. |
| 2026-09-28 | The sidechain reference trace is an Investigate feature. SPAN compares tracks through routing groups between instances, which is out of scope. | T-001 found no dedicated sidechain trace in SPAN. |
| 2026-09-28 | FFT library: pffft (marton78 fork, BSD-like), with `juce::dsp::FFT` kept as a test reference. FFTW and KFR are rejected (GPL). | T-002 section 1. pffft is SIMD-accelerated; the JUCE fallback is not tuned for speed without IPP. |
| 2026-09-28 | libebur128 (MIT) is a test-only loudness oracle, linked into unit tests only. The official EBU test files stay local and gitignored, and are never committed (EBU terms of use). | T-002 sections 3 and 10. |
| 2026-09-28 | K-weighting at every sample rate is derived from the analog prototype with the bilinear transform. T-002's alternative of resampling to 48 kHz is rejected. | Resampling adds audio-thread CPU and complexity for no accuracy gain. The prototype method is the established approach (libebur128). |
| 2026-09-28 | Exact EBU Tech 3341 and 3342 case values are transcribed from the official documents during T-006 (into `docs/research/T-006-ebu-test-cases.md`), not taken from the T-002 summary table. | The T-002 table is incomplete and partly inaccurate. |
| 2026-09-28 | True-peak oversampling: 4x at 44.1 and 48 kHz, 2x at 88.2 and 96 kHz, 1x at 176.4 and 192 kHz. The correlation meter uses exponential sums with a 300 ms time constant (superseded below: 500 ms window). The meter snapshot is double-buffered with acquire/release. RMS uses a true sliding window. | T-002 sections 4 to 7. |
| 2026-09-28 | The Steinberg VST3 SDK is MIT-licensed from version 3.8. T-014 must use SDK 3.8 or newer for the validator. | T-002 section 10. |
| 2026-09-28 | Correction: the JUCE Starter revenue cap is reported as $20k per year, not the $50k stated in the initial questions to the user. | T-002 section 10. The user must verify it against the EULA before M5. |
| 2026-09-28 | T-020 read the full SPAN guide text: the PDF has a text layer, and T-001's "scanned" conclusion was a tooling artefact. Resulting changes follow in the next rows. The * defaults stay: Voxengo does not document them. | T-001 section 9 (addendum). |
| 2026-09-28 | Window list changed to Hann (default), Blackman, Hi-Res (Nuttall squared) and Flat-top. SPAN's "Dome" is dropped because it has no published definition. Hamming and Blackman-Harris are dropped. Flat-top is kept for level accuracy. | Parity with SPAN's Hann, Blackman and Hi-Res (Primary User Guide p. 16), plus one Investigate addition. |
| 2026-09-28 | `avgTime` means "time to fall 20 dB", as in SPAN. The primary average uses τ = avgTime / ln(100), and the real-time max falls at 20 dB per `avgTime`. The fixed 20 dB/s constant is removed. | SPAN parity (Primary User Guide p. 17): SPAN's RT Max fall-off follows Avg Time. It also removes a hidden constant. |
| 2026-09-28 | Correlation window changed from about 300 ms to 500 ms. Added an L/R balance readout over 3 s, and a true-peak clip counter per channel next to the sample clip counter. | SPAN User Guide p. 5–6. All three are cheap on the audio thread. |
| 2026-09-28 | Crest factor stays peak minus RMS; SPAN's "Max Crest Factor" with its 50 ms peak-RMS window is not replicated. RMS stays a sliding window (`rmsWindow`), not SPAN's integrate-since-reset. | The standard definitions are clearer; integrated loudness already covers long-term level. This is a documented divergence. |
| 2026-09-28 | Pinned JUCE 8.0.15, Catch2 v3.16.0 and pluginval v1.0.4 (FetchContent into the gitignored `third_party/`). The project stays on JUCE 8 even though a JUCE 9 line exists. | The CLAUDE.md baseline requires JUCE 8. Moving to JUCE 9 would be a separate decision for the user, including its license terms. |
| 2026-09-28 | `cmake/` (CMake helper scripts) is owned by build-engineer. Build-engineer-owned scripts that must be committed go there, not in `third_party/`, which is gitignored as a whole. | T-003 layout. |
| 2026-09-28 | M0 gate: plugin-validator independently re-runs the T-003 build, the unit tests and pluginval, and writes `docs/qa/T-003-validation.md`. | A milestone closes only on QA reports, not on the implementer's own verification. |
| 2026-09-28 | Following the T-010 questions: meter snapshot peaks are the maximum over the last 50 ms; a `blockCounter` is added; the effective-range formulas are shared by mapper and editor; there are two secondary curves in the dual modes. | These close gaps the designer found between the DSP contract and what the editor needs. |
| 2026-09-28 | `SpectrumAnalyser` resets its secondary accumulators when `channelMode`, `fftSize`, `window` or the sample rate changes. The editor does not need to request this. | Accumulated curves from a different mode or resolution are meaningless. Resetting at the DSP level keeps the editor simple. |
| 2026-09-28 | M0 closed: `docs/qa/T-003-validation.md` PASS (clean rebuild with 0 warnings, CTest green, pluginval strictness 10, 0 latency, mono and stereo layouts). Both research reports (T-001 with T-020, and T-002) were reviewed by the architect. | Gate criteria met. |
| 2026-09-28 | Font TTFs and license texts are downloaded with checksum verification by build-engineer (T-021) into `third_party/fonts/`, and compiled into `InvestigateAssets`. They are not added to `assets/ui/`. | ui-ux-designer has no shell to fetch binary files, and pinned third-party files belong to build-engineer. |
| 2026-09-28 | T-023 added: an inventory of every third-party component compiled into the binary (the VST3 SDK and the libraries bundled in JUCE modules), with the notices each requires, and the JUCE codec definitions to switch off. | The About view and `THIRD-PARTY-NOTICES.txt` must be complete. Disabling unused codecs removes notice obligations and code size. |
| 2026-09-28 | T-023 results: every component in the shipped binary is permissively licensed except JUCE (AGPLv3 or the JUCE 8 EULA; the tier is decided at M5). The bundled VST3 SDK is 3.8.0 (MIT). A Steinberg trademark line goes in the About view and the notices. Unused codecs (FLAC, Ogg Vorbis, Windows Media) are disabled in T-014. The final notice text is assembled in T-019. | Keeps obligations minimal and complete. |
| 2026-09-28 | T-021 pinned pffft v1.1.0 (single precision, SSE) and libebur128 v1.2.6 (test-only, with a configure guard against linking it into the plugin). Fonts and license texts are in `InvestigateAssets`. There is no EBU fetch script: the EBU terms are accept-on-download and limited to non-commercial internal R&D, so a person must accept them. The user decides whether to download the set manually into `third_party/_fixtures/ebu/`. | EBU terms of use. |
| 2026-09-28 | The user chose not to use the official EBU test files. Loudness and true-peak conformance rests on the synthesisable Tech 3341 and 3342 cases plus libebur128 cross-checks (loudness in T-006, true peak in T-007). The `INVESTIGATE_EBU_TEST_SET_DIR` hook stays, unused. | User decision: avoids the EBU terms question and keeps the tests fully automatic. |
| 2026-09-28 | T-004 accepted. pffft measured 18.6x (4096 points) and 27 to 29x (65536 points) faster than `juce::dsp::FFT` on the development machine, which closes Risk 2. The analysis cost is 0.53 ms per second of audio at the defaults and 5.3 ms in the worst case, against budgets of 15 ms and 60 ms. The secondary curves are also cleared when the number of main or sidechain channels changes (a bus-layout change), in addition to the four roadmap triggers. | Measured in T-004. A layout change invalidates the accumulated curves just as a mode change does. |
| 2026-09-28 | T-005 accepted: mapping takes 0.147 ms per frame against a 0.3 ms budget. The sidechain curve is always computed when a sidechain is active; the editor gates its visibility with `showSidechain`. `SpectrumEngine::prepare` fixes the FIFO channel counts, so T-009 re-prepares it, with the analysis thread stopped, on any bus-layout change. | The cost of the sidechain curve is negligible; keeping the analyser independent of display toggles is simpler. |
| 2026-09-28 | T-006 accepted. Every synthesisable EBU case passes: Tech 3341 cases 1–5 and 9–14, and Tech 3342 cases 1–4. Case 6 (5.0 surround) is out of scope; cases 7 and 8 and Tech 3342 cases 5 and 6 need the official files and were not run (user decision). `process()` takes about 1.5 µs per 128-sample stereo block. M and S are recomputed on a 10 ms grid; integrated loudness and LRA gate on the BS.1770 100 ms hop. | Case 13 and 14 accuracy needed the finer M/S grid. |
| 2026-09-28 | Performance assertions inside the unit suite are noisy when the full suite runs (one ~5 µs outlier against the 1.5 µs typical). unit-tester tags them `[perf]` (T-017), and CI excludes that tag from the pass/fail gate (T-018). Budgets are enforced by T-016's dedicated benchmarks. | Keeps CI deterministic without losing budget enforcement. |
| 2026-09-28 | T-007 accepted with one criterion moved out. EBU true-peak cases 15–19 pass, and the libebur128 cross-check passes at 44.1, 48 and 96 kHz. The 88.2/96 kHz tier uses a 49-tap, 2x Hann-windowed sinc (the same parametrisation as libebur128); 44.1/48 kHz uses the literal BS.1770-5 table. `MeterBank` publishes with a seqlock: the writer never waits, and the reader on the message thread retries. Cases 20–23 and the thin MeterBank p99 margin (38.8 µs against a 40 µs budget) move to T-024, which gates M1. | The acceptance criterion is kept, not waived. The p99 margin is fixed at the source (zero-padded taps) rather than by raising the budget. |
| 2026-09-28 | ThreadSanitizer does not exist for MSVC on Windows. Lock-free correctness rests on the written memory-ordering review in T-017, which must specifically cover the seqlock payload copy and whether it is formally a data race, plus the multi-threaded stress tests already in the suite. | Tooling limitation on the Windows-only toolchain. |
| 2026-09-28 | M1 closed. T-004, T-005, T-006, T-007 and T-024 each have a unit QA report with PASS, and the full suite (119 cases, about 152k assertions, 5 documented skips) is green in Debug and Release with pluginval. EBU coverage: Tech 3341 cases 1–5 and 9–23 pass, and Tech 3342 cases 1–4 pass. Case 6 (surround) and the cases that need the official files were not run. `MeterBank` measures about 5 µs mean and 29 µs p99 per block, isolated. The p99 comes from the deterministic 10 ms exact recomputation. | Gate criteria met. The p99 is re-measured inside the full processor at T-009 and T-016. |
| 2026-09-29 | T-008 accepted. Both agents running on 2026-09-28 were stopped by the account usage limit (HTTP 429). T-008 had finished before the cut-off. In round 2 the plugin-engineer updated the testers' QA verdicts itself, which breaks QA ownership. From now on dispatch prompts forbid coders from editing `docs/qa/`, and T-009's testers re-confirm T-008's round-2 fixes in their own reports. | The fixes are small and were re-run, but independent sign-off is the rule. |
| 2026-09-29 | T-025 added: `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1` (activates `source/StandaloneApp.cpp`, so the Standalone starts with input unmuted), plus the codec-off definitions moved over from T-014. The Level chip text is built from the numeric `dbLow` and `dbHigh`, not from `getText()` (T-011). | Resolves T-008's open questions. One CMake edit instead of two. |
| 2026-09-29 | T-025 results: the codecs are off (VST3 went from 6.90 MB to 5.95 MB, −13.8 %). The Standalone fails to link because WinMain is defined twice. Fix: `source/StandaloneApp.cpp` provides only `juce_CreateApplication()` (plugin-engineer, first step of T-009); `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_ENTRYPOINT` stays unset. | This is the documented JUCE custom-app pattern, and it keeps JUCE's own entry point. |
| 2026-09-29 | Timing tests use Catch2's hidden tag `[.perf]`, so neither `ctest` nor CI runs them by default. They assert the roadmap budgets, not stretch targets. They are run on purpose with `InvestigateUnitTests.exe "[perf]"`, and by T-016. This is T-026, done before T-009. | Makes the default test run deterministic without a CMake change. |
| 2026-09-29 | M2 closed. T-008, T-025, T-026 and T-009 are DONE with passing QA reports. Evidence: bit-exact null test in every layout with analysis active and inactive; state round trip and corrupt-data safety; 0 allocations in `processBlock` over 10 s with parameter changes; analysis thread stops within 50 ms over 100 open/close cycles; clean sample-rate changes; the Standalone links, launches and starts with input unmuted; pluginval strictness 10; `ctest` 167/167 run (5 documented skips). `processBlock` timing, isolated, at 48 kHz, 128 samples, stereo plus sidechain: mean 5.4–8.3 µs, p99 31–47 µs against the 20/50 µs budget. The p99 spread between the two testers' machines is noted for T-016. | Gate criteria met. |
| 2026-09-29 | Items carried to later milestones: (1) a manual check that the Standalone's "Mute audio input" toggle shows unchecked, during the user's M3 review and in T-015; (2) the Steinberg VST3 validator is still to be added (T-014); (3) re-measure the `processBlock` p99 under realistic load (T-016). | Found at the M2 gate; none blocks M3. |
| 2026-09-29 | The user asked for a GitHub repository and a commit of all produced files except the Claude files (T-027). Architect defaults: the repository is private and named `Investigate`, the branch is `main`, and `CLAUDE.md` and `.claude/` are added to `.gitignore` so they are never committed. | Private is reversible; publishing is not, and the licence decision (Starter or AGPLv3) is still open. Ignoring the Claude files keeps them out of every future commit too. |
| 2026-09-29 | T-027 done. The private repository https://github.com/Keray535/Investigate was created. The root commit is `71d0313` (155 files); two documentation follow-ups were pushed after it. The secret scan was clean; the largest file is 90 KB. Commits use the user's existing global git identity, which was left unchanged. | Checkpoint of M0–M2, and the T-018 precondition is met. |
| 2026-09-29 | The root `README.md` is owned by build-engineer (T-028). It must not name SPAN or Voxengo, and it gives the licence as "all rights reserved; public licence to be decided". | The file is not in the CLAUDE.md layout. Build-engineer already owns the build instructions. IP boundaries from T-001 section 8. |
| 2026-09-28 | The user approved the T-010 mockup (`docs/design/mockup.html`) without changes. The visual direction and layout are frozen for T-011 to T-013. | User decision. |
| 2026-09-28 | Editor sizes: default 1200×700, minimum 920×540, maximum 2560×1600, free aspect ratio (T-010 spec). | Taken from the design spec. |
| 2026-09-28 | Fonts approved: Inter v4.1 (Medium, SemiBold) and JetBrains Mono v2.304 (Regular, Medium), both SIL OFL 1.1 with no Reserved Font Name. Embedding and subsetting are allowed. Obligations: ship the OFL text and copyright notices with the installer (T-019) and in the editor's About or credits (T-011), and never sell the font files on their own. The ui-ux-designer adds the TTFs to `assets/ui/` in the mockup revision pass, not while a C++ build task is running. | T-022. |
| 2026-09-28 | Added the interactions listed in Scope: auto-unfreeze on mode change, click to reset curves, right-click to copy frequency, and a meter-collapse toggle. The rich hover readout (frequency, note with cents, level) is an Investigate improvement; SPAN has no such readout. | SPAN User Guide p. 4 and Primary User Guide p. 13 and 18. All are low cost. |
| 2026-10-01 | M3 started with T-011. Its task file now overrides two stale points in `docs/design/ui-spec.md`: the fonts and license texts live in `third_party/fonts/` and `InvestigateAssets` (not `assets/ui/fonts/`), and spec §15 open questions 1 to 3 are closed. The spec itself is not edited mid-milestone; ui-ux-designer may fold the corrections in at the M3 gate. T-014 stays sequential after M3, because it edits CMake and builds C++. | Keeps the gui-engineer from chasing missing files, and keeps the one-C++-task rule. |
| 2026-10-02 | T-011 accepted. Unit QA failed in round 1 on one defect (`ViewState` size and collapse state were lost through the host's XML state round trip, because XML turns properties into strings), which was fixed in round 2; 236/236 tests pass in Debug and Release. Validation passed: 0 px region-edge deviation, 50 open and close cycles with no leak, pluginval strictness 10. The documented spec deviations (focus ring drawn inside the bounds, a custom corner grip, tooltips clipped to the editor, and minor About-view text spacing) are accepted. | Gate evidence in `docs/qa/T-011-*.md`. Each deviation is either a JUCE constraint or cosmetic. |
| 2026-10-02 | Accepted v1 limitation: the editor reads `ViewState` only in its constructor, so a state load while the editor is open does not resize it until it is reopened. | Most hosts recreate the editor on project load. Listening for live changes risks editor resizes from a non-message thread in `setStateInformation`. |
| 2026-10-02 | The render harness (`tools/render-harness/`, with `InvestigateEditorCheck` from T-011) is built by a validation preset in T-014 and run in CI by T-018. The typographic minus (U+2212) issue on the Level chip goes to T-012. | Routes T-011's open questions to their owners. |
| 2026-10-03 | Editor paint budget method: p95 is measured as `paint()` wall time in a real on-screen window with JUCE's default Direct2D renderer, in the worst displayed case. Message-thread CPU at 60 fps ≤ 25 % is gated alongside it. Offscreen `juce::Image` timings are for information only. This applies to T-012, T-013 and T-016. | In JUCE 8 on Windows an offscreen `juce::Image` is Direct2D-backed, and its readback (5 to 17 ms) swamps the paint cost. The software image is not the production renderer. |
| 2026-10-03 | Editor allocation rule: Investigate's own code allocates nothing per frame in curve, meter and idle paths. Text readouts may rebuild strings only when their content can change, with a bound of ≤ 20 allocations per frame. Allocations inside JUCE's text layout are excluded. This applies to T-012 and T-013. | JUCE's `drawText` allocates internally, so "zero allocation" cannot be met with text. Message-thread allocations affect smoothness, not real-time safety. |
| 2026-10-03 | The T-012 coder session stopped at the usage limit after its round 1 fix. The architect dispatches round 2 QA directly, unit-tester then plugin-validator, sequentially. | Avoids a cold coder session only to dispatch testers. One C++ build at a time. |
| 2026-10-03 | T-012 accepted with the message-thread CPU criterion moved to the new T-029, which runs before T-013. Measured with the meter panel still a placeholder: `paint()` p95 is 0.9 to 1.2 ms, but message-thread CPU is up to 25.3 % at 1280×720 and up to 41.9 % at 2560×1440, against 25 %. The flat-fill fallback changed it by about 1 point. T-029 profiles first, then applies structural fixes, geometry reduction and dirty regions. Frame pacing (30 fps above a plot-area threshold) is the fallback. OpenGL stays an open question. T-029 gates the spectrum alone at ≤ 18 % (1280×720) and ≤ 20 % (2560×1440). T-013 gates the whole editor at ≤ 25 % at both sizes. | The message thread is shared with the host's UI, so 40 % at 1440p would make hosts sluggish. Gating the 1280×720 size alone was rejected, because it is borderline before the meters exist. Optimising before T-013 lets the meter panel follow the findings. |
| 2026-10-03 | T-029 accepted. Profiling showed that the cost was Direct2D's CPU tessellation of the large jagged fill polygons, not `paint()`. The fills are now GPU sprite batches of one-pixel bars, the strokes drop near-collinear vertices, and partial repaints build only the columns under the clip. The spectrum alone now costs 14.1 % (1280×720) and 16.8 % (2560×1440), down from 25 % and 42 %. **Frame pacing is part of the product:** the spectrum repaints at 30 fps when the physical plot area exceeds 2.0 Mpx (roughly any window larger than 1920×1080 at 100 %) or when all seven curve layers are visible (dual mode with secondary curves and sidechain); otherwise at 60 fps. Hover, freeze and clicks still respond within one vblank. The software-renderer path keeps the original polygons. Small follow-ups (a clip-edge seam, and CPU at 110 % and 175 % display scaling) go to T-013. | Pacing was the planned fallback. Without it, the busiest 1280×720 case swung across its gate. The user was told in advance that it might be needed, and can veto it at the M3 review. |
| 2026-10-03 | T-030 added: an agent configured CMake in-source at the repository root (a full Visual Studio tree next to the sources). build-engineer removes it by explicit path, never with a broad `git clean`, adds a `FATAL_ERROR` guard against in-source builds, and extends `.gitignore`. Every build uses `build/` through the presets. | Stale trees risk building or measuring the wrong binaries. A guard stops it from happening again. |
| 2026-10-03 | T-030 accepted (commit `da48a81`). Its verification run exposed a reproducible failure of the T-029 pacing test (#105) and one hung `ctest` run. T-031 (unit-tester) finds the root cause and makes every on-screen test SKIP rather than fail when the desktop delivers no paints or vblanks, with every wait bounded; any production defect goes to gui-engineer through T-013. T-014 adds a default ctest timeout. Until then, testers run `ctest` with `--timeout 300`. | The suite must be deterministic before T-013 adds more GUI tests. The test passed earlier the same day, so the environment (display asleep or session locked) is the main suspect. |
| 2026-10-03 | T-031 accepted. Test #105 failed because of the environment combined with a fragile test, not a code defect. Vblanks and small repaints still arrived, but the repaint of the large data rect in a window larger than the display was lost, and other desktop activity added unrequested paints. The pacing test now gates on the environment, bounds every wait to 10 s, retries up to 3 times from a quiet window, and SKIPs rather than fails when the desktop misbehaves. A mutation check confirms it still catches a broken pacing deferral. A 60 s watchdog ends the process if a native call blocks. Full suite: 3 of 3 green in Debug and in Release. The hang was not reproduced. | The suite is deterministic again before T-013. The watchdog exit is accepted, and the default ctest timeout in T-014 remains. |
| 2026-10-03 | The user's M3 hands-on review follows the checklist in `docs/plans/m3-user-review.md`, which covers the Standalone mute-input check, DAW checks, meters, resizing, and the frame-pacing look as an explicit decision point. M3 closes after T-013 QA passes and the user's findings are triaged. | Gives the user a concrete, bounded review and turns feedback into tasks. |
| 2026-10-03 | `test_audio.wav` (the user's own produced track, added for the test agents) is **local-only and never committed**, by user decision. It was made in FL Studio: 48 kHz, 32-bit float, stereo, 200.8 s, 77 MB, sample peak −0.22 dBFS, very bass-heavy (`docs/research/test-audio-inspection.md`). T-032 (build-engineer) moves it to `third_party/_fixtures/audio/` (gitignored, like the EBU fixture hook), adds root-level audio patterns to `.gitignore`, and exposes the folder to tests as `INVESTIGATE_TEST_AUDIO_DIR`. A folder rather than a file, so a separate master or another track can be added. T-015 adds a real-material null test and a cross-check of loudness, LRA and true peak against libebur128. T-016 adds real-music CPU measurements. Tests SKIP when the file is absent, as in CI. A parallel architect session wrote T-032 at the same time as the main session planned `tests/fixtures/audio/`; T-032's location was kept. | User decision. It keeps the user's unreleased work out of a repository that may later become public under AGPLv3, and at 77 MB it is too large for git history. A real track partly fills the gap left by the unused EBU authentic-programme files. |
| 2026-10-03 | T-013 validation round 1 was FAIL with 3 small defects, and everything else passed (whole-editor CPU worst median 20.0 %, at 110 % scaling, against 25 %). Round 2 fixes:<br>• the lag of the reset text: a reset forces an immediate text refresh, as an exception to the 10 Hz text rule;<br>• the truncated "MONO" label;<br>• a cap of 60 Hz on editor updates for displays faster than 60 Hz (spec §8.1; the development display runs at 144 Hz);<br>• "M max" and "S max" below −99.9 LUFS display in the silent form.<br>The meter paint limit at 2560×1440 is amended from 1 ms to 1.5 ms. Cold-start loudness (I is −23.1/−23.2 and LRA about 1 LU after 3 s of a steady sine) becomes T-033 (dsp-engineer, M4), checked against libebur128. | The fixes are small and user-visible. The 1 ms meter share was the architect's own allocation, and the governing budgets pass with a wide margin. The cold-start bias is a DSP correctness question, not a GUI one. |
| 2026-10-03 | T-013 accepted after round 2. Reset clears within one update interval, "MONO" shows in full, editor updates are capped at 60/s on faster displays, and M max and S max read "−inf" in silence. Whole-editor CPU worst medians (60 Hz): 9.9 % (1280×720), 12.9 % (2560×1440) and 20.1 % (1920×1080 at 110 %). Meter paint p95: 0.99 and 1.27 ms. "One UI frame" now means one editor update interval (≤ 21 ms on a 144 Hz display under the 60/s cap). T-016 gates message-thread CPU through the real editor path at the native refresh rate, because the T-013 harness bypassed the limiter. The paced spectrum runs at about 24 fps at 144 Hz with 2560×1440, which is part of the user's pacing review. The flaky lifecycle test #315 is a T-017 watch item. | Every functional and budget criterion passes at the reference rate. The remaining items are measurement-method or polish questions, routed to the tasks that own them. |
| 2026-10-03 | Editor rendering rules for every editor component: no polygons drawn per frame; one `fillRectList` per colour; cache the static parts as images; repaint only when a physical-pixel value changes; build only what lies under the clip. CPU is gated with the T-012/T-029 harness method, as the median of 3 runs. | Learned in T-029 (rules in its log); Direct2D costs are invisible to `paint()` timing. |
| 2026-10-03 | Spectrum zoom and pan go into v1 as milestone M3b, between M3 and M4. The plan is `docs/plans/spectrum-zoom-pan.md`, and the tasks are T-034 to T-038. T-015, T-016 and T-017 now depend on T-038. | User request. M4 must validate the final UI. |
| 2026-10-03 | Zoom is a view layer inside the home range set by `freqLow`, `freqHigh`, `dbLow` and `dbHigh`. It never writes parameters, and the parameters, their ranges and the chips do not change. | User choice. It keeps host automation and undo clean, and the parameter bounds (`freqLow` ≤ 1 kHz, `dbLow` ≤ −30 dB) would block close zooms. |
| 2026-10-03 | Gestures:<br>• the wheel zooms frequency and Ctrl+wheel zooms level, both around the pointer;<br>• a wheel over an axis gutter zooms that axis;<br>• a horizontal wheel pans frequency;<br>• left-drag and middle-drag pan;<br>• a double-click fits, so the click reset now waits 250 ms (the chip rule);<br>• Ctrl+drag zooms into a box;<br>• an overview strip and a zoom indicator appear while zoomed.<br>There is no Space shortcut. This supersedes ui-spec §5.6 "Left drag: nothing" and "Mouse wheel: nothing", once T-034 lands. | User choices. Space is the host's transport key. |
| 2026-10-03 | The zoom is not persisted. The editor opens at Fit, and `ViewState` gains no properties. | User choice: persistence was offered and not selected. |
| 2026-10-03 | The engine re-maps the latest bin-domain spectra on a view change without a new FFT, and holds a preallocated bin-domain snapshot (about 0.7 MB) while frozen. Frames carry a freeze epoch. This partly supersedes "Freezing the display is editor-only" (Processor API). | At 65536 points with 1x overlap a new frame arrives only every 1.37 s. Zooming a frozen spectrum must show real detail. |
| 2026-10-03 | The minimum view span is 1/6 octave and 6 dB, defined in `DisplayRange.h` and shared by the mapper and the editor. The home range keeps its 1-octave and 12 dB rules. One wheel notch scales the span by 0.8 or 1.25. | Architect defaults. ui-ux-designer may refine them in T-034 with a reason. |
| 2026-10-03 | Accepted limitation: on Windows precision touchpads, pinch arrives as Ctrl+wheel and zooms level. | The platform translates pinch into Ctrl+wheel for ordinary windows. A horizontal swipe still pans frequency. |
| 2026-10-03 | T-032 done (commit `5e54716`). `test_audio.wav` moved unchanged to `third_party/_fixtures/audio/` (SHA-256 `178af299…0d434b`, 77,103,712 bytes). Root audio files are gitignored. `INVESTIGATE_TEST_AUDIO_DIR` is a compile definition on `InvestigateUnitTests` and the four render-harness executables; each test checks for the file and skips when it is absent. The previous session's planning documents were committed separately (`7130542`). The "leave out the WAV" commit rule is lifted, because git now ignores it. | Closes the T-032 handoff item. |
| 2026-10-03 | The user gave the go-ahead for M3b. T-034 (documentation) and T-035 (the only C++ task) run in parallel. The user's M3 hands-on review is deferred; M3 stays open until it is done and triaged, and M3b proceeds in the meantime. | User decision. |
| 2026-10-03 | T-035 accepted (unit QA PASS in round 1, 0 defects; ctest 367/367 in Debug and Release). The worst-case re-map takes a median of 0.31 ms against 1.0 ms. With no view and no freeze the output is bit-identical to before. Follow-up decisions: the freeze epoch increments only on a real state change; a resize or a smoothing or slope change alone does not re-publish (it shows on the next hop, as before M3b; accepted for v1); when Freeze is pressed before any frame exists, the editor keeps what it showed. The T-036 freeze-epoch criterion was amended, because a poll that started before the call can legitimately publish one more live frame with the old epoch, which the editor drops. | Routes the tester's open points to T-036 and T-037. |
| 2026-10-03 | T-034 delivered (spec §5.9, tokens 1.1.0, three cursor SVGs, the working mockup). It keeps every plan number and refines a few with reasons (spec §5.9.10): the zoom factors apply per notch-equivalent, so touchpads zoom at a sane speed; a horizontal pan moves 10 % of the span per notch; a chip change affects only its own axis; the indicator and the overview strip are combined into one zoom panel at the top right of the data rect; the hover card shows 4 significant figures below a 1-octave span. Its open questions 5 (a curve margin for panning) and 6 (`binHz` in the frame) are accepted as recommended and become T-039 (dsp-engineer), which runs after T-036 and before T-037. A possible overlap between the legend and the FROZEN badge at 920×540 in the busiest legend state goes to T-037 as a measurement; a fix, if needed, becomes a small ui-ux-designer task. | Accepting the margin removes an empty strip at the leading edge during a fast pan or zoom-out, at about 1.5× the re-map cost, which stays well inside the budget. A per-frame `binHz` is the only correct source while frozen. |
| 2026-10-03 | T-039 accepted (unit QA PASS in round 1; ctest 395/395 in Debug and Release; the T-009, T-036 and editor tests and pluginval pass). Zoomed frames carry a 25 % margin per side on the same grid. At Fit, and for the visible points of every zoomed view, the output is bit-identical to T-035. The margin values match a shifted-view reference within 0.0003 dB. The frame capacity is 3072 points (`maxVisiblePoints` 2048 plus 2 × 512); `sizeof (SpectrumFrame)` grew from about 51 KB to 75.8 KB. The worst-case re-map takes a median of 0.33 ms against 1.0 ms. The processor still requests `maxPoints`, which the engine clamps; T-041 corrects the constant. No tie-breaking rule is added for the 18 reference-tie combinations the margin test skips. | The skipped ties are an artefact of a float-rounded reference view, not a margin defect. A one-line ownership-respecting fix is cheaper than relying on a clamp. |
| 2026-10-03 | Raw tool logs that testers save under `docs/qa/` (for example `docs/qa/T-036-logs/`, pluginval output) are not committed. The QA report summarises them. build-engineer adds the ignore pattern `docs/qa/*-logs/`. | They are bulky, machine-specific and reproducible. The verdict and the numbers live in the reports. |
| 2026-10-03 | T-036 accepted (unit and validation QA PASS in round 1; ctest 378/378; pluginval strictness 10; null tests bit-exact). View change to published frame at 65536 points with 1x overlap: p95 5.7 to 6.1 ms and max 7.2 to 7.4 ms, against 10 and 20 ms. 0 torn pairs in 10^6; worst analysis-thread stop 3.5 ms. The last `setAnalysisActive (false)` resets view and freeze, and an active freeze then counts as a real change (the epoch increments). The editor detects frozen frames by epoch only; `SpectrumFrame` has no frozen flag. One frozen-frame latency of 32 ms in 23 runs under load (budget 20 ms) came from a timing assertion in the default test run: T-040 moves that bound to `[.perf]`, per the 2026-09-29 rule. Debug-build, Steinberg-validator and sanitizer coverage stays with M4 (T-014, T-017). | Keeps the default run and CI deterministic without weakening the budget, which `[.perf]` still enforces. |
| 2026-10-03 | The user approved the T-034 mockup without changes. Spec §5.9 and the mockup are frozen for T-037 and T-038. | User decision. It removes the last precondition of T-037 other than T-036 and T-039. |
| 2026-10-03 | Zoom smoothing padding is clipped to the effective home range. At Fit there is no padding, so the curves stay bit-identical to today; a zoomed curve matches the Fit curve at the same frequencies, and only the home range edges keep the truncated window. The T-035 edge-smoothing criterion was amended. | The current mapper truncates the smoothing window at the range ends. Padding past the home range would change the Fit curves and break the no-regression criterion. |
| 2026-10-03 | **Milestone M3c** is added between M3b and M4, with the 14 items the user requested (`docs/plans/m3c-feature-expansion.md`, tasks T-042 to T-059). M4 (T-015, T-016, T-017) now depends on T-059. | User request and user choice of timing. M4 must validate the final UI. |
| 2026-10-03 | The **band solo is audible**:<br>• Ctrl+left-hold over the spectrum plays a band-pass around the pointer; Ctrl+wheel sets the width (`soloWidth`); while Ctrl is held, the band is previewed without audio.<br>• The output is bit-exact whenever the solo is not engaged, which amends the Vision's "never changes the audio".<br>• Level zoom moves to Alt+wheel and box zoom to Alt+left-drag, which amends the approved M3b spec through T-042 before T-037 starts. | User choices. Ctrl follows SPAN's narrow-band sweep. The zoom gestures were not yet implemented, so the change costs only a spec update. |
| 2026-10-03 | Items 2 and 4 follow **SPAN as the reference**: its 14 metering modes, the Bars and Density styles, and its primary-channel-plus-underlay model. T-043 confirms the details. | The user's reference images did not reach the session and were not in the project folder. The user chose SPAN. |
| 2026-10-03 | Existing **parameter IDs and choice indices are preserved**:<br>• `channelMode` keeps its seven values; L/R and M/S become aliases of Left plus Right and Mid plus Side; `underlay` is new;<br>• `meterScale` keeps its four values, and the ten new modes are appended.<br>The DSP tasks (T-046 to T-050) are additive and run before the parameter task (T-051). | No migration code. Sessions and automation from earlier builds load unchanged, and every intermediate build keeps the current editor working. |
| 2026-10-03 | **Meter sections** (bars, statistics, loudness, correlation) can be hidden one by one, stored in `ViewState.meterSections`. The existing header toggle still hides everything. | User choice. |
| 2026-10-03 | **Three inputs:** main plus two sidechains. The meters measure the main input only. Sharing between plugin instances stays out of scope. Each input gets a user-editable label, stored in `ViewState`. | User choice. Comparison through buses needs no inter-instance infrastructure. |
| 2026-10-03 | **Curve colours** are chosen with a colour wheel for the curve roles only (Primary A, Primary B, Sidechain 1, Sidechain 2, References 1 to 4).<br>• They are global: every instance, persisted across launches, in a machine-wide preference file. The first launch uses the token defaults.<br>• Colour presets are separate files.<br>• Colours are never stored in the session or in settings presets. | User choices. This matches SPAN's global colour schemes. |
| 2026-10-03 | **Frame rate:** a cap from 30 to 144 fps (default 60), with Adaptive pacing (default on) that keeps the T-029 rule. Both are global preferences, like anti-alias. Budgets stay gated at 60 fps. | User choice. Display rate is a property of the machine, not of the project. |
| 2026-10-03 | **Ballistics** use SPAN's definitions, in milliseconds: integration 1.00 ms, release 750 ms, peak hold 1000 ms, all as parameters (saved in the session and in settings presets). `rmsWindow` stays for the statistics RMS. The editor's fixed 20 dB/s, 1500 ms and 30 dB/s ballistics are replaced. | User choice of values and units. |
| 2026-10-03 | **Reference slots and CSV.** Four reference slots are saved with the session. CSV export and import use a fixed 4096-point log grid from 1 Hz to 48 kHz. The import accepts SPAN's linear-frequency CSV. | User choice of slots plus CSV. Grid size and slot count are architect defaults that T-045 may refine. |
| 2026-10-03 | **Settings presets** hold the parameter values only. New directory `source/presets/`, owned by plugin-engineer (with `UserDataPaths.h`, which gui-engineer includes read-only). The VST3 program list stays unused. | User choice. The preset store is host-facing state, which plugin-engineer already owns. The directory is not in the CLAUDE.md layout, so it is recorded here. |
| 2026-10-03 | **M3c architect defaults**, open to change at the T-045 review:<br>• `channelMode` Mid, `underlay` Off;<br>• `soloWidth` 1 octave (1/12 to 4);<br>• LUFS and LU bars show the programme loudness unless T-043 shows SPAN meters loudness per channel;<br>• anti-alias on, Align 0 dB off. | Keeps today's behaviour as the default wherever possible. |
## Status

| Task | Title | Owner | Milestone | Depends on | Status |
|---|---|---|---|---|---|
| T-001 | SPAN feature parity and UX survey | researcher | M0 | none | DONE (completed by T-020) |
| T-002 | DSP references, standards and dependency licenses | researcher | M0 | none | DONE |
| T-003 | Project scaffold | build-engineer | M0 | none | DONE (`docs/qa/T-003-validation.md` PASS) |
| T-021 | DSP dependencies: pffft, libebur128 (test-only), EBU test-set fetch, bundled fonts | build-engineer | M1 | T-002, T-003 | DONE (EBU fetch script not written: the terms need a human to accept them; decision with the user) |
| T-004 | SampleFifo, windows and SpectrumAnalyser | dsp-engineer | M1 | T-002, T-003, T-021 | DONE (`docs/qa/T-004-unit.md` PASS) |
| T-005 | SpectrumMapper, frame exchange and SpectrumEngine | dsp-engineer | M1 | T-001, T-004 | DONE (`docs/qa/T-005-unit.md` PASS) |
| T-006 | LoudnessMeter (BS.1770 / EBU R128) | dsp-engineer | M1 | T-002, T-003, T-021 | DONE (`docs/qa/T-006-unit.md` PASS) |
| T-007 | Level, true-peak and correlation meters, MeterBank | dsp-engineer | M1 | T-006 | DONE (`docs/qa/T-007-unit.md` PASS; the EBU cases 20–23 criterion was moved to T-024) |
| T-024 | True-peak follow-up: per-tier tap count, EBU cases 20–23 | dsp-engineer | M1 | T-007 | DONE (`docs/qa/T-024-unit.md` PASS) |
| T-008 | Parameters, buses and state | plugin-engineer | M2 | T-001, T-003 | DONE (both QA reports PASS in round 2; testers re-confirm during T-009) |
| T-025 | Compile definitions: custom Standalone app, unused codecs off | build-engineer | M2 | T-008, T-023 | DONE (codecs off, VST3 −14 %; Standalone link fixed and verified in T-009) |
| T-026 | Hide timing tests from the default test run (`[.perf]`) | unit-tester | M2 | T-024 | DONE (`docs/qa/T-026-unit.md` PASS) |
| T-027 | GitHub repository and initial commit | build-engineer | M2 | T-009 | DONE (https://github.com/Keray535/Investigate, private, `main`) |
| T-028 | Project README | build-engineer | M2 | T-027 | DONE (commit `4dd1453`) |
| T-009 | Processor wiring, analysis thread and editor API | plugin-engineer | M2 | T-005, T-007, T-008 | DONE (`docs/qa/T-009-unit.md` and `docs/qa/T-009-validation.md` PASS) |
| T-010 | UI spec, design tokens, SVG assets and HTML mockup | ui-ux-designer | M3 | T-001, T-020 | DONE (mockup approved by the user; font files moved to T-021) |
| T-011 | Editor shell, LookAndFeel, settings controls and resizing | gui-engineer | M3 | T-008, T-010 | DONE (`docs/qa/T-011-unit.md` PASS in round 2, `docs/qa/T-011-validation.md` PASS) |
| T-012 | SpectrumView component | gui-engineer | M3 | T-009, T-011 | DONE (`docs/qa/T-012-unit.md` PASS; `docs/qa/T-012-validation.md` passes all but the message-thread CPU criterion, which moved to T-029) |
| T-029 | Editor rendering CPU: profile and reduce Direct2D message-thread load | gui-engineer | M3 | T-012 | DONE (`docs/qa/T-029-unit.md` and `docs/qa/T-029-validation.md` PASS; frame pacing used) |
| T-030 | Remove the in-source build at the root; in-source guard | build-engineer | M3 | T-029 | DONE (commit `da48a81`; the unrelated test failure moves to T-031) |
| T-031 | Make the on-screen GUI tests deterministic (pacing test failure, test hang) | unit-tester | M3 | T-030 | DONE (`docs/qa/T-031-unit.md` PASS; no production defect) |
| T-032 | Local-only test audio: keep it out of git, `INVESTIGATE_TEST_AUDIO_DIR` hook | build-engineer | M3 | none | DONE (commit `5e54716`; the WAV is in `third_party/_fixtures/audio/`, never committed) |
| T-013 | Meter panel | gui-engineer | M3 | T-009, T-011, T-029, T-030, T-031 | DONE (`docs/qa/T-013-unit.md` PASS; `docs/qa/T-013-validation.md` round 2 PASS at the 60 Hz reference; the 144 Hz findings were decided in the log) |
| T-034 | Zoom and pan: UI spec addendum and mockup | ui-ux-designer | M3b | none | DONE (mockup approved by the user without changes, 2026-10-03) |
| T-035 | Spectrum engine: view-range mapping, re-publish and freeze snapshot | dsp-engineer | M3b | T-005, T-009 | DONE (`docs/qa/T-035-unit.md` PASS in round 1; re-map 0.31 ms against 1.0 ms) |
| T-036 | Processor: view and freeze API, analysis-thread wiring | plugin-engineer | M3b | T-035 | DONE (`docs/qa/T-036-unit.md` and `docs/qa/T-036-validation.md` PASS in round 1) |
| T-040 | Move the 20 ms frozen-frame bound out of the default test run | unit-tester | M3b | T-036 | DONE (`docs/qa/T-040-unit.md` PASS; `[.perf]` p95 6.6 ms, max 6.8 ms; ctest 3 of 3 green) |
| T-039 | Spectrum engine: pan margin and bin spacing in the frame | dsp-engineer | M3b | T-035, T-036 (build order) | DONE (`docs/qa/T-039-unit.md` PASS in round 1; re-map 0.33 ms with the margin) |
| T-041 | Processor: request the visible point count, not the frame capacity | plugin-engineer | M3b | T-039 | TODO (next C++ task; one-line change) |
| T-042 | Zoom modifiers: Alt+wheel level zoom, Alt+drag box zoom, Ctrl reserved for band solo (spec and mockup) | ui-ux-designer | M3b | none | TODO (documentation; runs now, before T-037) |
| T-037 | SpectrumView: zoom, pan, Fit, indicator and deep-zoom grid | gui-engineer | M3b | T-034 (mockup approved), T-036, T-039, T-041, T-042 | TODO |
| T-038 | SpectrumView: overview strip and box zoom | gui-engineer | M3b | T-037, T-042 | TODO |
| T-043 | SPAN reference for M3c | researcher | M3c | none | TODO (documentation; runs now) |
| T-044 | Host and platform constraints, band-solo filter references | researcher | M3c | none | TODO (documentation; runs now) |
| T-045 | M3c UI spec addendum, tokens, assets and mockup | ui-ux-designer | M3c | T-042, T-043, T-044 | TODO (the user approves the mockup) |
| T-046 | Spectrum engine: underlay model and a second sidechain input (additive) | dsp-engineer | M3c | T-043, T-038 (build order) | TODO |
| T-047 | Spectrum mapper: 1 Hz range and Align 0 dB | dsp-engineer | M3c | T-043, T-046 | TODO |
| T-048 | Spectrum engine: reference grid and capture | dsp-engineer | M3c | T-047 | TODO |
| T-049 | MeterBank: integration, release, metering modes and density (additive) | dsp-engineer | M3c | T-043, T-048 (build order) | TODO |
| T-050 | BandSoloFilter | dsp-engineer | M3c | T-044, T-049 (build order) | TODO |
| T-051 | Parameters and state version 2 | plugin-engineer | M3c | T-043, T-047, T-050 (build order) | TODO |
| T-052 | Processor wiring for M3c | plugin-engineer | M3c | T-046 to T-051 | TODO |
| T-053 | Settings preset store | plugin-engineer | M3c | T-052 | TODO |
| T-054 | Global settings, curve colours, colour wheel and colour presets, frame rate, anti-alias | gui-engineer | M3c | T-045 (mockup approved), T-053 | TODO |
| T-055 | SpectrumView M3c: channel and underlay controls, sidechain curves and labels, 1 Hz axis, Align | gui-engineer | M3c | T-054 | TODO |
| T-056 | Band solo interaction and outside-band tint | gui-engineer | M3c | T-055 | TODO |
| T-057 | Meter panel M3c: metering modes, styles, ballistics, section hiding | gui-engineer | M3c | T-056 | TODO |
| T-058 | Presets menu, reference slots, CSV import and export | gui-engineer | M3c | T-057 | TODO |
| T-059 | M3c gate validation | plugin-validator | M3c | T-058 | TODO |
| T-033 | Loudness meter at the start of a measurement (partly filled gating blocks) | dsp-engineer | M4 | T-013 | TODO |
| T-014 | Validation tooling: VST3 validator, ASan preset, harness hook | build-engineer | M4 | T-002, T-003 | TODO |
| T-015 | Black-box validation suite | plugin-validator | M4 | T-009, T-013, T-014, T-059 | TODO |
| T-016 | Performance benchmarks | plugin-validator | M4 | T-012, T-013, T-014, T-059 | TODO |
| T-017 | Real-time safety, sanitizer and static analysis audit | unit-tester | M4 | T-009, T-012, T-013, T-014, T-059 | TODO |
| T-018 | GitHub Actions Windows CI | build-engineer | M5 | T-014 | TODO |
| T-019 | Windows installer, code signing and release docs | build-engineer | M5 | T-018, user decisions | TODO |
| T-020 | SPAN user guide visual extraction | researcher | M0 | T-001 | DONE |
| T-022 | Font license check (Inter, JetBrains Mono) | researcher | M3 | T-010 | DONE |
| T-023 | Third-party notices inventory for the shipped binary | researcher | M3 | T-003, T-022 | DONE |
