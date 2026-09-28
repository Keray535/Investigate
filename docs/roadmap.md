# Investigate: Roadmap

Last updated: 2026-09-28 by vst-architect.

## Vision

Investigate is a free real-time audio analyser plugin for mixing and mastering engineers. Its features are modelled on Voxengo SPAN. It shows a high-resolution FFT spectrum with SPAN-style controls: block size, window, overlap, slope, averaging, smoothing, a secondary max or average curve, and a hover readout of frequency, note and level. Next to the spectrum it shows level and loudness statistics (peak, RMS, true peak, clip count, and EBU R128 momentary, short-term and integrated loudness plus loudness range), a stereo correlation meter, and Mid/Side and per-channel spectrum modes. It also accepts a sidechain input and draws it as a second spectrum, so users can compare their mix against a reference track. Investigate never changes the audio: it passes the signal through bit for bit with zero latency.

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
- Windows 11 x64: VST3 and Standalone.
- A signed Windows installer for a free public release.

### Out of scope for v1
- Spectrogram or waterfall display (a SPAN Plus feature, planned for v2).
- macOS, AU, AAX and CLAP builds.
- Surround or more than two main channels.
- SPAN's multi-channel routing groups, and routing between plugin instances.
- Match EQ, exporting a spectrum to a file, a preset browser, A/B comparison and undo.
- Loudness targets, platform presets (streaming targets) and automatic reset on transport start.
- Capturing system audio (WASAPI loopback) in the Standalone app.

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

Analysis thread (juce::Thread owned by the processor; runs only while the editor is open; polls every ~5 ms)
   SpectrumEngine consumer side:
     SampleFifo ─► channel mode combine ─► window ─► real FFT ─► power spectrum (bin domain)
       ─► exponential averaging (power domain, τ = avgTime / ln 100) ─► secondary accumulators (RT max, all-time max, all-time avg)
       ─► SpectrumMapper: bins → log-spaced display points (interpolate low, peak-preserve high)
          ─► fractional-octave smoothing ─► slope (1 kHz pivot) ─► dB
       ─► FrameExchange (lock-free triple buffer) ──────────────────► Editor SpectrumView (VBlank, ≤ 60 fps)
```

### Modules

| Module | Location | Owner | Thread | Purpose |
|---|---|---|---|---|
| `SampleFifo` | `source/dsp/` | dsp-engineer | audio (push), analysis (pop) | Lock-free SPSC multichannel ring buffer. The producer never blocks or allocates; on overflow it drops the incoming samples and increments an atomic counter. |
| `WindowFunctions` | `source/dsp/` | dsp-engineer | analysis | Hann, Blackman, Hi-Res (Nuttall squared) and flat-top windows with coherent-gain correction. |
| `SpectrumAnalyser` | `source/dsp/` | dsp-engineer | analysis | Framing with overlap, channel-mode combination, FFT, power spectrum, averaging and secondary accumulators, all in the bin domain. |
| `SpectrumMapper` | `source/dsp/` | dsp-engineer | analysis | Maps bins to log-spaced display points, then applies smoothing, slope and dB conversion. |
| `SpectrumFrame`, `FrameExchange` | `source/dsp/` | dsp-engineer | analysis → message | The fixed-size frame struct and a lock-free triple buffer. |
| `SpectrumEngine` | `source/dsp/` | dsp-engineer | both | A facade: a real-time-safe producer API plus a consumer `processPending(settings)` call. |
| `LoudnessMeter` | `source/dsp/` | dsp-engineer | audio | BS.1770-4/-5 and EBU R128: M, S, I and LRA, using fixed-size histograms. |
| `TruePeakDetector` | `source/dsp/` | dsp-engineer | audio | BS.1770 Annex 2 polyphase FIR; the oversampling factor depends on the sample rate. |
| `LevelMeter` | `source/dsp/` | dsp-engineer | audio | Sample peak, RMS window and clip counting. |
| `CorrelationMeter` | `source/dsp/` | dsp-engineer | audio | Phase correlation from −1 to +1. |
| `MeterBank`, `MeterSnapshot` | `source/dsp/` | dsp-engineer | audio → any | Aggregates the meters, publishes snapshots through atomics, and handles reset requests. |
| Parameter layout and IDs | `source/params/` | plugin-engineer | message | The single source of parameter definitions. |
| `InvestigateProcessor` | `source/PluginProcessor.*` | plugin-engineer | all | Buses, state, `processBlock`, the analysis thread (a private class) and the editor-facing API. |
| `InvestigateEditor` and components | `source/PluginEditor.*`, `source/gui/` | gui-engineer | message | LookAndFeel, SpectrumView, MeterPanel, settings controls and resizing. |

All C++ code lives in namespace `investigate`.

### Processor API used by the editor (implemented in T-009)
- `void setAnalysisActive (bool)`: the editor calls this from its constructor and destructor. It starts or stops the analysis thread and FIFO pushing.
- `bool getLatestSpectrumFrame (SpectrumFrame& dest)`: message thread only. It returns `true` if a new frame arrived since the last call.
- `MeterSnapshot getMeterSnapshot() const noexcept`: lock-free and callable from any thread.
- `void requestStatsReset()`: resets peak max, true-peak max, clip counts, integrated loudness, LRA, and the max momentary and short-term values.
- `void requestSpectrumReset()`: clears the all-time max and all-time average curves.
- `bool isSidechainActive() const noexcept`.
- `juce::AudioProcessorValueTreeState& getApvts() noexcept`.
- Freezing the display is editor-only: the editor stops pulling frames.

### SpectrumFrame contents (finalised in T-005)
- Up to 2048 display points, log-spaced between `freqLow` and `min(freqHigh, Nyquist)`, with the frequency of each point.
- A primary curve in dB, a second primary curve for the dual modes (R or S), a secondary curve for each primary curve (so two secondary curves in the dual modes), and the sidechain curve.
- Flags: `isDual`, `hasSecondary`, `hasSidechain`. Also the sample rate and a sequence number.
- In dual modes the sidechain curve shows the power mean of the sidechain channels.

### MeterSnapshot contents (finalised in T-007)
- Per channel: sample peak and true peak, each the maximum over the last 50 ms, so a 30 to 60 Hz reader cannot miss block peaks; peak max and true-peak max since reset; RMS; sample clip count; and true-peak clip count.
- `blockCounter`: incremented on every `process` call. The editor uses it to detect a stopped or bypassed host and let the bars fall.
- Loudness: momentary, short-term, integrated, LRA, max momentary and max short-term.
- Correlation (500 ms), L/R balance in dB over the last 3 s (negative means the left channel is louder), `isMono` and the sample rate.
- All levels are reported in dBFS (RMS is not sine-referenced). The editor applies the +3.01 dB AES17 offset and the K-system reference when `meterScale` is K-20, K-14 or K-12. Crest factor is derived in the editor as peak minus RMS.

### Parameter table

No parameter changes the audio. So no parameter needs smoothing, and every change takes effect at the next analysis frame or meter block. All parameters are APVTS parameters, so they are saved with the state and can be automated. They use `juce::ParameterID { id, 1 }`. Values marked * are provisional: T-001 will confirm or correct them against SPAN, and any change will be logged in the Decisions section.

| ID | Name | Type | Range / choices | Default | Unit | Skew | Smoothing |
|---|---|---|---|---|---|---|---|
| `channelMode` | Mode | choice | Mid, Side, Left, Right, L+R (power mean), L/R dual, M/S dual | Mid* | – | – | none |
| `fftSize` | Block size | choice | 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536 | 4096* | samples | – | none |
| `overlap` | Overlap | choice | 1x, 2x, 4x, 8x | 4x* | – | – | none |
| `window` | Window | choice | Hann, Blackman, Hi-Res (Nuttall squared), Flat-top | Hann | – | – | none |
| `avgTime` | Avg time | float | 0 to 10000; the time for the spectrum to fall 20 dB | 1000* | ms | centre at 500 ms | none |
| `smoothing` | Smoothing | choice | Off, 1/48, 1/24, 1/12, 1/6, 1/3 octave | 1/12 oct* | – | – | none |
| `slope` | Slope | float | −12 to +12, step 0.5 | 4.5 | dB/oct | linear | none |
| `secondary` | Secondary curve | choice | Off, Real-time max, All-time max, All-time average | Real-time max* | – | – | none |
| `freqLow` | Frequency low | float | 10 to 1000 | 20 | Hz | log (centre 100 Hz) | none |
| `freqHigh` | Frequency high | float | 1000 to 40000, clamped to Nyquist at display | 20000 | Hz | log (centre 6325 Hz) | none |
| `dbLow` | Range low | float | −150 to −30, step 1 | −90* | dB | linear | none |
| `dbHigh` | Range high | float | −30 to +30, step 1 | +6* | dB | linear | none |
| `showSidechain` | Sidechain trace | bool | off / on | on | – | – | none |
| `meterScale` | Meter scale | choice | dBFS, K-20, K-14, K-12 | dBFS | – | – | none |
| `rmsWindow` | RMS window | float | 50 to 3000 | 300 | ms | log (centre 300 ms) | none |

Constraints:
- The mapper (T-005) and the editor (T-012) compute the effective display range with exactly these formulas: `fHiEff = min(freqHigh, sampleRate / 2)`, `fLoEff = min(freqLow, fHiEff / 2)`, `dbHiEff = dbHigh`, and `dbLoEff = min(dbLow, dbHigh − 12)`. Parameters are never rewritten; only the effective values are clamped.
- `avgTime` follows SPAN's convention: it is the time for the displayed level to fall by 20 dB. The primary curve's power-domain exponential average uses τ = avgTime / ln(100). The real-time max secondary curve falls linearly in dB at 20 dB per `avgTime`. With `avgTime = 0` there is no averaging, and the real-time max equals the current frame.
- Correlation uses a fixed 500 ms averaging window (SPAN User Guide p. 5–6). The L/R balance readout uses a fixed 3 s window.
- The values still marked * are not documented anywhere in Voxengo's guides (confirmed by T-020). They are final product decisions, open to change after user testing.

### Buses and channel layouts
- Main input and output: mono to mono, or stereo to stereo (the same layout on both sides).
- Sidechain input: disabled, mono or stereo. Sidechain samples are never written to the output.
- Latency: 0 samples. Tail: 0 seconds.

### Threading model

| Thread | Work | Communication |
|---|---|---|
| Audio | `processBlock`: `ScopedNoDenormals`, MeterBank process, and FIFO push while analysis is active. The output buffer is never written. | Publishes the MeterSnapshot through atomics. Reads reset requests from atomic flags. Pushes samples into the SPSC FIFO. Reads parameters through raw APVTS atomics. |
| Analysis | A `juce::Thread` owned by the processor. It starts on `setAnalysisActive(true)`, polls with `wait(5)`, runs `SpectrumEngine::processPending`, and stops within 50 ms. It may reallocate when `fftSize`, the window or the sample rate changes. | Pops samples from the FIFO. Publishes frames into the triple buffer. Reads parameters through raw APVTS atomics. |
| Message | Editor painting via `juce::VBlankAttachment`, parameter attachments, and state save and restore. | Reads the latest frame and the MeterSnapshot. Sets reset flags. |

The audio thread never signals the analysis thread, because an OS event signal is not real-time safe. The analysis thread polls instead.

### State format
- APVTS `ValueTree` of type `InvestigateState`, with property `stateVersion = 1`.
- A child `ViewState` holds editor-only persistent properties: `editorWidth`, `editorHeight` and `metersCollapsed`, plus later UI preferences. The editor reads and writes it on the message thread.
- The tree is serialised with `copyXmlToBinary`. Loading tolerates missing properties (they keep their defaults), unknown properties (ignored) and corrupt data (defaults remain, no crash).

### Identity (placeholders, change before the first public release)
- Product name `Investigate`; company name `Investigate Audio`; manufacturer code `Ivga`; plugin code `Ivst`; bundle ID `com.investigateaudio.investigate`.
- VST3 category `Fx|Analyzer`.
- Changing the codes after a public release breaks saved host sessions, so they must be final before M5.

## Budgets

Measured on the development machine in a Release build, at 48 kHz, 128-sample blocks, stereo main plus stereo sidechain, with analysis active.

| Item | Budget |
|---|---|
| Latency | 0 samples |
| Audio output | bit-exact to the input (null test difference exactly 0) |
| `processBlock` CPU | mean ≤ 20 µs and p99 ≤ 50 µs per block (0.75 % and 1.9 % of the 2.67 ms block) |
| MeterBank alone | mean ≤ 15 µs and p99 ≤ 40 µs per block |
| Analysis thread, default settings | ≤ 2 % of one core |
| Analysis thread, worst case (65536, 8x, dual mode, sidechain, 192 kHz) | ≤ 10 % of one core |
| Editor paint | p95 ≤ 4 ms at 1280×720 (100 % scale); p95 ≤ 8 ms at 2560×1440 |
| Message thread at 60 fps | ≤ 25 % of one core |
| Memory | ≤ 64 MB resident increase per instance at maximum settings |
| Analysis thread stop | ≤ 50 ms after `setAnalysisActive(false)` |

## Milestones

| Milestone | Content | Tasks | Gate |
|---|---|---|---|
| M0 Scaffold | Research, CMake project, pinned JUCE, Catch2 and pluginval, a placeholder pass-through plugin | T-001, T-002, T-003 | The placeholder VST3 builds, pluginval passes at strictness 10, CTest runs, and both research reports are delivered |
| M1 DSP core | DSP dependencies, then the spectrum pipeline and meters in `source/dsp/` with unit tests | T-021, T-004, T-005, T-006, T-007, T-024 | Unit QA reports PASS, including the EBU conformance cases |
| M2 Parameters and state | APVTS, buses, state, and DSP wiring with the analysis thread | T-008, T-025, T-026, T-009 | Unit and validation QA PASS: bit-exact null test, state round trip, and real-time safety |
| M3 UI | Design spec, then the editor shell, SpectrumView and MeterPanel | T-010, T-011, T-012, T-013 | The spec is accepted; QA PASS on the editor; the paint budget is met |
| M4 Validation | Validation tooling, the full black-box suite, performance, and a real-time safety and sanitizer audit | T-014, T-015, T-016, T-017 | All QA reports PASS and all budgets are met |
| M5 Packaging | Windows CI, then a signed installer and release docs | T-018, T-019 | CI is green; a signed installer installs, uninstalls and passes pluginval on the installed binary |

## Working rules for this project
- At most one task that builds C++ may be in flight in the main working tree at a time. This avoids build-directory collisions, and half-written files breaking another agent's build. Documentation tasks (research, design) may run in parallel with it.
- Source directories are picked up by CMake globbing with `CONFIGURE_DEPENDS` (set up in T-003). Coders therefore add files without editing CMake.
- Every coder task ends with a unit QA report. Tasks that touch the plugin binary also end with a validation QA report.

## Handoff (architect session ended 2026-09-29)

Status at handoff: M0, M1 and M2 are closed. Every task in them is DONE with passing QA reports; see the Status table and the Decisions log.

The next session starts M3:
1. **T-011** (gui-engineer): editor shell, LookAndFeel, settings controls, About view, resizing and the meter-collapse toggle. Its task file already includes the later decisions: fonts from `InvestigateAssets`, the Level chip built from numeric values, and credits from T-023.
2. **T-012** (gui-engineer): SpectrumView. It uses the shared `source/dsp/DisplayRange.h`. Paint budget: p95 ≤ 4 ms at 1280×720.
3. **T-013** (gui-engineer): MeterPanel.
4. After T-013 passes, offer the user a hands-on review in a DAW and in the Standalone. Include the manual check that "Mute audio input" starts unchecked.
5. Then M4 (T-014 to T-017) and M5 (T-018 and T-019).

Rules learned in this session. Include them in every dispatch prompt:
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
| 1 | Painting dense filled paths at 60 fps is too slow at high DPI | Laggy UI, high message-thread CPU | Reduce the curve to about one point per pixel column, cache the static grid as an image, use `VBlankAttachment`, and profile in T-012. Fall back to attaching an OpenGL context if the Direct2D budget is missed. |
| 2 | `juce::dsp::FFT` without IPP is slow for 65536 points | Analysis CPU budget missed | Resolved by choice: pffft (BSD-like, SIMD) is the product FFT (T-021, T-004). T-004 benchmarks it against `juce::dsp::FFT` on the development machine. The FFT runs off the audio thread and sits behind an interface. |
| 3 | Loudness or true peak deviates from EBU conformance | Wrong numbers; loss of credibility | Test against the EBU Tech 3341 and 3342 cases, synthesised or with the official files if their license allows. Optionally cross-check against libebur128 (MIT) as a test-only oracle after a license check. |
| 4 | Clone concerns: trade dress or trademark | Legal or takedown risk | Own name and artwork only; no Voxengo text, assets or colour-scheme copies. T-001 lists what is off-limits. |
| 5 | FIFO overflow when the analysis thread is starved | Spectrum glitches | Drop samples and count the overflow; the consumer resynchronises. FIFO capacity ≥ 2^17 frames per channel. The overflow counter is visible in debug builds. |
| 6 | Hosts with odd sidechain handling (bus disabled, or present but silent) | Wrong or missing trace | `isSidechainActive()` reflects the enabled bus layout; the trace is hidden when inactive; T-015 tests the layouts. |
| 7 | The Standalone mutes input by default ("Mute audio input") | The analyser shows silence in the Standalone | T-008 sets the Standalone's mute-input default to off (T-002 section 9). The output is a bit-exact copy of the input, so users should use headphones or a separate output device to avoid acoustic feedback when the input is a microphone. T-015 checks the behaviour. |
| 8 | An unsigned installer triggers SmartScreen warnings | Poor user trust | Choose a signing method (for example Azure Trusted Signing or an OV certificate) before M5. |
| 9 | JUCE license choice for a free closed-source release | Cannot ship legally | The user chooses before M5: JUCE Starter licence (closed source, with a revenue cap) or AGPLv3 (the source must be published). T-002 reports the Starter cap as $20,000 per year of gross revenue, not the $50k assumed earlier. The user must read the current JUCE 8 EULA before deciding. |
| 10 | Parallel agents collide in the same working tree | Broken builds, lost work | Apply the one-C++-task-at-a-time rule; revisit worktree isolation after M1 if throughput matters. |

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
| T-011 | Editor shell, LookAndFeel, settings controls and resizing | gui-engineer | M3 | T-008, T-010 | TODO |
| T-012 | SpectrumView component | gui-engineer | M3 | T-009, T-011 | TODO |
| T-013 | Meter panel | gui-engineer | M3 | T-009, T-011 | TODO |
| T-014 | Validation tooling: VST3 validator, ASan preset, harness hook | build-engineer | M4 | T-002, T-003 | TODO |
| T-015 | Black-box validation suite | plugin-validator | M4 | T-009, T-013, T-014 | TODO |
| T-016 | Performance benchmarks | plugin-validator | M4 | T-012, T-013, T-014 | TODO |
| T-017 | Real-time safety, sanitizer and static analysis audit | unit-tester | M4 | T-009, T-012, T-013, T-014 | TODO |
| T-018 | GitHub Actions Windows CI | build-engineer | M5 | T-014 | TODO |
| T-019 | Windows installer, code signing and release docs | build-engineer | M5 | T-018, user decisions | TODO |
| T-020 | SPAN user guide visual extraction | researcher | M0 | T-001 | DONE |
| T-022 | Font license check (Inter, JetBrains Mono) | researcher | M3 | T-010 | DONE |
| T-023 | Third-party notices inventory for the shipped binary | researcher | M3 | T-003, T-022 | DONE |
