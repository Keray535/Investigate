# T-043: SPAN reference for M3c

Researcher, 2026-10-04. Sources: SPAN User Guide v3.24 ("SPAN p.N") and Voxengo Primary User Guide ("Primary p.N"), cached in `research/_cache/span/`, plus T-001 and the M3c plan. Marking: **confirmed** (source, page), **inferred**, **not documented**. Nothing in this report is SPAN text or artwork; statements are paraphrases for internal use.

## Question
How exactly does SPAN handle the M3c features (metering modes, density, ballistics, Align 0 dB, anti-alias, channel groups and Underlay, static spectra and CSV, narrow-band solo, colours), so that T-045 to T-051 rest on sources?

## Answer and recommendation
- The guides document behaviour but give **no numeric ranges, defaults or bar scales** for the meter settings. The user's 1.00 / 750 / 1000 ms defaults can be neither confirmed nor corrected from the guides. Ranges and defaults in the plan stay architect decisions.
- Confirmed corrections to the plan: (a) K-xx C modes fix **both integration and release to 600 ms** (the plan agrees); plain K-xx do not. (b) SPAN's LUFS/LU modes also feed the "Integr" statistic with integrated loudness. (c) SPAN's Underlay chooses **another channel group**, not a fixed channel list; the Off/Stereo/Mid/Side/Left/Right list is Investigate's own mapping and is acceptable. (d) The CSV is a plain linear-frequency, dB-valued text file; its header, separator and decimal point are **not documented**. (e) The band solo needs a **drag** (Ctrl, then left-drag); the wheel sets the bandwidth. This matches the plan.
- Align 0 dB: SPAN documents only the outcome (a stationary 0 dBFS 1 kHz sine reads 0 dB irrespective of block size). Recommended method for Investigate: a **calibration offset computed from an analytic model of the 1 kHz sine run through the same mapper** (section "Align 0 dB method").
- Static spectra: the guide states no slot count. Four slots is an Investigate choice, not a SPAN fact.

## Answers to questions 1 to 9

### 1. Metering modes
| Item | Answer | Status |
|---|---|---|
| dBFS, dBFS.30, dBFS.15 | No bias; the three differ only in meter range. | confirmed (SPAN p.5) |
| Exact top and bottom of each | dBFS shows +6 at the top and -60 at the bottom in the only screenshot (SPAN p.1 as described in T-001 §5). dBFS.30 and .15 ranges are **not documented**. | dBFS: inferred from screenshot; .30/.15: not documented |
| dBFS+3 | Adds 3 dB to the level indicator and to the RMS/Integr values; **peak indication unchanged**. Intended with Align 0 dB and slope 0 so spectral power matches the meter. Whether the scale labels shift is not stated. | confirmed (SPAN p.5) |
| K-20, K-14, K-12 | Bob Katz K-system biasing, unweighted; no fixed ballistics. | confirmed (SPAN p.5) |
| K-20 C, K-14 C, K-12 C | Calibration variants; RMS integration and release fixed to 600 ms, not affected by Settings. | confirmed (SPAN p.5) |
| LUFS ML / LUFS SL / LU ML / LU SL | BS.1770-4 and Tech 3341-2016 K-weighting; ML = momentary 0.4 s, SL = short-term 3 s; integration fixed. EBU +18 scale and a summary loudness line are not offered. Integrated loudness goes to the "Integr" panel, updated twice a second over the last 5 minutes only. | confirmed (SPAN p.5) |
| LU reference | The guide follows Tech 3341 but never states -23 LUFS. -23 LUFS is the EBU R128 target, so it is the natural reference. | inferred |
| LUFS/LU bar: programme or per channel | **Not documented.** Hints both ways: the Primary guide says level meters have one bar per channel of the group, with a "minimal" variant averaging channels (Primary p.11); SPAN p.5 says Max Crest Factor in LUFS modes uses K-weighted RMS "integrated loudness" per statistic. Nothing says the bar is per channel. | not documented |
| RMS/"Integr" statistic | "Integrated signal power estimation" since reset in level modes; in LUFS/LU modes it shows integrated loudness. dBFS+3 adds 3 dB to it. | confirmed (SPAN p.4, p.5) |
| Max Crest Factor | Difference between RMS and 50 ms peak RMS; in LUFS modes it uses K-weighted RMS, not the EBU loudness. | confirmed (SPAN p.4-5) |

### 2. Density mode
- Purpose: shows the levels the signal stays at often; the spread indicates effective dynamic range. **confirmed** (Primary p.28).
- Level estimation in this mode follows the configured integration and release times; the peak-hold time sets how long a level stays registered. **confirmed** (Primary p.28).
- Drawing method (brightness, colour, histogram bin size, where it is drawn, normalisation): **not documented**. The plan's 0.5 dB bins and brightness along the track are Investigate choices (inferred as compatible).

### 3. Ballistics
- Integration time: milliseconds, "time to fall 20 dB, or to rise from one steady level to another"; it does not affect the registered peak and directly sets the visible peak-to-RMS gap. Below about 50 ms the meter measures peaks, not RMS, at low frequencies; 2 ms is quoted as a "low" example. **confirmed** (Primary p.28-29). This implies the same time constant for rise and fall of the level part (inferred).
- Release time: milliseconds, time to fall 20 dB, also called ballistics; 1000 ms gives 20 dB/s. **confirmed** (Primary p.29). Whether release applies to the level part, the peak part or both: **not documented**. The density text ("integration and release affect level estimation") confirms it applies at least to the level estimate (Primary p.28). Peak: inferred only.
- Peak hold: milliseconds a registered 1-sample-wide peak stays unchanged on the meter; also affects density display. **confirmed** (Primary p.29). What the marker does afterwards (fall at release rate, or jump): **not documented**.
- Ranges and defaults (the user's 1.00 / 750 / 1000 ms): **not documented**. The guides, the SPAN Plus guide search results and the forum snippets found give no numbers. They remain the user's stated values; mark them "user-stated, unverified".
- Settings are per plug-in type and shared by all instances (Primary p.27, p.11).

### 4. Align 0 dB
- SPAN: the switch aligns a stationary 1 kHz sine of 0 dBFS peak to a 0 dB visual level **irrespective of Block Size**; suggested use is slope 0 and adjusted range. **confirmed** (Primary p.17).
- SPAN without Align: the normalisation is **not documented**. Because Align exists "irrespective of block size", the unaligned level most likely depends on block size (inferred; consistent with a power-per-bin or per-bandwidth scale). Smoothing drops stationary sines by 6 dB per octave of frequency as an inherent effect (Primary p.17); how Align interacts with that (whether the 1 kHz reading includes the smoothing loss) is **not documented**. Window interaction: not documented.
- dBFS+3 plus Align 0 dB plus slope 0 are meant to be used together to match spectral power to the meter (SPAN p.5). That suggests Align gives the spectrum a power scale consistent with a sine's RMS (inferred): a 0 dBFS peak sine reads 0 dB on the spectrum and -3.01 dB RMS plus 3 dB bias on the meter.
- Investigate today: a bin-centred full-scale sine reads 0 dB before smoothing (coherent-gain normalisation), and smoothing plus peak-preserving mapping then lower the displayed peak by an amount that depends on block size, smoothing and the sine's position between bins.
- Recommended method: see the section "Align 0 dB method".

### 5. Anti-Alias
The switch only "enables anti-aliased spectrum curve drawing" (Primary p.17). It is a drawing option; no analysis effect is described. **confirmed** (drawing only, by omission; Primary p.17).

### 6. Channel groups and Underlay
- Routing: input routing, mid-side pairs, group assignments, key sources, output routing; presets are shared by all Voxengo plug-ins (Primary p.23-25). Presets named in the guides: "Dual Mono" (SPAN p.4, Primary p.17), "Mid-Side Stereo" (Primary p.39 FAQ), "Stereo Side-Chain" (Primary p.40 FAQ). A "Stereo" group name appears in the group selector example (Primary p.26). **confirmed**. Complete preset list: not documented.
- Group selector: lists only groups actually assigned; default names are indices, user-renamable (Primary p.25-26). **confirmed**. Default group at first open: **not documented**.
- Spectrum of a two-channel group (power mean, sum or maximum): **not documented**. The guides only say the spectrum shows "the current channel group" (SPAN p.4) and that channels in a group are linked for loudness estimation by a mode such as max or average (Primary p.24, dynamics context). Treat as unknown; power mean (Investigate's current L+R) stays a defensible choice.
- Mid-side: a mid-side pair encodes two internal channels before processing; the group list shows the mid part as "m" and side part as "s" (Primary p.23, p.26). With the "MS1" pair assigned to L and R and each in its own group, mid and side are separate groups (Primary p.39). **confirmed**
- Underlay: adds "an additional underlying spectrum" from **another channel group of the same instance** (SPAN p.4). The choices are therefore the group names; the guide's example is "Dual Mono" plus Underlay "Right". **confirmed**. Whether "Off" exists: implied, not stated. Its spectrum mode (block size and so on) comes from that group's own settings and must be copied with the group selector's "Copy" (Primary p.30 FAQ). **confirmed**. Investigate has one set of analysis settings, so this does not apply.
- Hold (freeze) resumes automatically when the group changes (SPAN p.4). **confirmed**
- Meters: bars follow the **currently selected group**; "Show All Channel Meters" shows all channels and statistics of all groups, intended for dual mono or mid-side (Primary p.11, p.23). Statistics are "for each channel of the currently selected group" (SPAN p.4). **confirmed**. So in a Mid-plus-Side setup the meters show M and S only if that option is on and the mid-side pair is assigned (inferred for the exact bar labels).

### 7. Static spectra
- Slot count: **not documented** (the editor is described as "the spectrum slot", with a slot selection implied). Four is an Investigate choice.
- Per slot: display name, colour, dB shift (a pure display shift that does not change the shape), visibility checkbox. **confirmed** (Primary p.18). The shift range is **not documented**; the plan's -40 to +40 dB is an Investigate choice.
- "Take" snapshots the primary spectrum and "Take 2nd" the secondary; taken spectra use the spectrum parameters of the Spectrum Mode Editor; the taken spectrum's parameters are shown; clicking the spectrum resets accumulation before another take; Predef loads built-ins; "x" clears a slot; "Filled Display" is **one switch for all static spectra**. **confirmed** (Primary p.18).
- Files: native format ".csf"; a ".csv" extension given explicitly writes Excel-style text. In the CSV, **frequencies are raw linear-scale values** (not the displayed log scale) and power values are in dB. WAV, AIFF, FLAC can also be loaded as a spectrum source. **confirmed** (Primary p.18).
- Header line, separator, decimal point, column count and names, row count: **not documented**. Treat SPAN as two columns (frequency in Hz, level in dB) from the description only (inferred). The plan's tolerant importer (comma, semicolon or tab; skip `#` and non-numeric lines) is the right way to cope.

### 8. Narrow-band sweep and solo
- Gesture: hold Ctrl (Command on macOS), then drag with the left mouse button on the control surface; the band-pass curve of the filter that passes the selected range is drawn; the **mouse wheel adjusts bandwidth**. Useful for finding resonances. **confirmed** (Primary p.13, SPAN p.4). In SPAN the plug-in does not touch the sound when this mode is not engaged (SPAN p.4).
- Click needed: Ctrl alone does not engage it; a left-drag does (Primary p.13). A Ctrl-only preview is an Investigate addition. **confirmed**
- Filter type, order, slope, width range: **not documented**. Visual feedback beyond the drawn band-pass curve: not documented.

### 9. Colours
- Plug-in-type-wide colour scheme (all instances, applied immediately in open instances and in later ones), with an Edit button opening a detailed editor "to fine-tune colours of different visual elements", plus Load and Save to a `.ccf` file (Primary p.27). **confirmed**. This matches the plan's global-colours model.
- Which individual elements are editable: **not documented**. From SPAN p.4: the primary spectrum has "a specified colour", the Underlay has its own "specified colour", and the secondary spectrum is a darker shade. Static spectrum slots have their own colours (Primary p.18). **confirmed** for these four roles (primary, secondary as a shade, Underlay, static slots). Background, grid and meter colours are probably editable too (inferred); the plan keeps them as tokens, which is a deliberate difference.

## Align 0 dB method
Goal: a stationary 0 dBFS 1 kHz sine reads 0 ± 0.5 dB at slope 0 for every block size, window, overlap and smoothing setting, with Align off bit-identical to today.

Facts about today's pipeline (from `SpectrumMapper.h`): bin power (coherent-gain normalised) → bin-to-point mapping (linear interpolation or peak-preserving aggregation) → power boxcar smoothing → dB → slope. Before dB conversion every step is linear in power, so the reading of a stationary sine is a deterministic function of block size N, window, sample rate and smoothing. Averaging modes (RT Avg, Avg) do not change a stationary sine's power; Max and RT Max also do not (stationary). Overlap does not change the stationary reading.

Method (a calibration, computed on the analysis thread only when its inputs change: block size, window, sample rate, smoothing, view range, point count):
1. Let `binHz = fs / N` and `x = 1000 / binHz` (fractional bin position of 1 kHz). Build a model bin-power curve for a 0 dBFS sine at exactly 1 kHz: for bins `k` within ±8 of `x`, `P[k] = |A · W(k - x)|^2`, where `W(ν) = Σ_{n=0}^{N-1} w[n] e^{-j 2π ν n / N}` is evaluated directly for that handful of bins from the actual window `w` (cost about 17·N operations, N up to 65536, once per settings change), `A = 0.5 / Σ w[n] · 2` scaled so a bin-centred sine gives exactly 0 dB in today's normalisation (that is, the same coherent gain division the analyser uses; the sine amplitude 1.0 gives peak bin amplitude `Σw/2`). All other bins `P = 0` (leakage beyond ±8 bins is below -60 dB for Hann and below -100 dB for Hi-Res and Blackman, so ignore it; Dome, Hi-Res: check at test time).
2. Run this model curve through the same `SpectrumMapper` path (interpolation or aggregation, smoothing) with slope 0, over the points around 1 kHz only, and take the maximum dB value `M` found within ±(half smoothing width + 1 bin) of 1 kHz. If 1 kHz is outside the visible range, compute on a local grid of the same spacing.
3. Set `alignOffsetDb = -M`, applied after smoothing and before slope (add before the dB floor). Align off adds 0.
4. The offset applies equally to primary, secondary, underlay, aux curves and captured/CSV curves ("with the current smoothing and Align", as the plan says for capture).

Why this meets the criterion: the offset is computed from the exact window transform at the actual fractional bin position, so scalloping (up to 1.4 dB for Hann at half a bin) is part of the calibration and is removed for 1 kHz; it uses the same mapper, so smoothing and aggregation losses cancel. Other frequencies keep the SPAN-documented 6 dB per octave smoothing slope-down for sines (Primary p.17). Block-size independence is automatic because the model uses N.

Alternative (simpler, less exact): subtract the mapper's measured loss for a bin-centred sine, plus a fixed scalloping allowance. Not recommended: it fails the ±0.5 dB limit for off-bin positions at low block sizes.

Risks: (a) For `1 kHz` close to a bin boundary the peak-preserving aggregation is robust; a plain interpolation path can differ, hence step 2 uses the real mapper. (b) With the lowest block sizes at high sample rates, 1 kHz is spread over few bins; the model handles that. (c) Align on makes noise read higher (as in SPAN; Primary p.17 suggests slope 0).

### Test signal set for T-047
All 0 dBFS peak sines, stereo main input with identical channels and Mid, primary, slope 0, smoothing off then 1/3 octave, averaging to steady state, tolerance 0 ± 0.5 dB at 1 kHz with Align on:
1. 1000 Hz, block sizes 1024, 2048, 4096, 8192, 16384, 65536, at 44.1, 48 and 96 kHz.
2. Windows Hann, Hi-Res (or Investigate's equivalents), Blackman, Dome, Hamming, flat-top if present.
3. Smoothing off, 1/6, 1/3, 1/2 and 1 octave.
4. Overlap 2x, 4x, 8x.
5. Fractional bin offsets: 1 kHz as given (varies per fs and N), plus a sweep of sine frequencies at exact bin centre, +0.25 and +0.5 bin around 1 kHz (checks the calibration does not depend on lucky alignment; expected spread with Align on: 0 dB at 1 kHz itself, others only informative).
6. Align off: compare frames to the pre-change reference bit for bit (existing golden frames).
7. Level linearity: -6, -20 and -60 dBFS 1 kHz sines read -6, -20 and -60 ± 0.5 dB.
8. Additional informative cases (no pass limit): 100 Hz and 10 kHz sines (smoothing loss visible, about 6 dB per octave per Primary p.17), white noise (reads higher with Align, expected), a two-sine 1 kHz + 2 kHz signal.
9. Cases with an underlay and with the secondary curve (RT Max, Avg): offset applies equally.
10. View range with 1 kHz outside the visible window: the offset must equal the one with it inside.

## Recommendations for M3c
| # | Plan item | Recommendation | Affected tasks |
|---|---|---|---|
| R1 | dBFS.30 / dBFS.15 bar ends | Not documented. Propose a 30 dB and a 15 dB span ending at +6 dBFS (so overs stay visible): dBFS.30 = -24 … +6, dBFS.15 = -9 … +6. Record as an architect decision, not a SPAN fact; the name implies the span. | T-049, T-051, T-045 |
| R2 | dBFS+3 | Confirmed as in the plan: level and statistics RMS +3.01 dB, peaks unchanged. Use 3.0103 dB (SPAN says "3 dB"; either is within display resolution). Tick labels of the scale: unspecified, keep the dBFS scale. | T-049 |
| R3 | K-xx C | Plan correct. Add: only the "C" modes fix **integration and release** to 600 ms; plain K-xx follow the parameters. | T-049, T-051 |
| R4 | LUFS/LU "Integr" | In LUFS/LU modes the statistics "Integr" must show integrated loudness (gated per BS.1770-4/Tech 3341, last 5 minutes only, updated 2 times a second). SPAN does not display a summary line or EBU +18. Max Crest Factor in these modes uses K-weighted RMS. The plan's RMS row stays on `rmsWindow` in level modes only; the unit label follows the mode. | T-049, T-045, T-057 |
| R5 | LUFS/LU bar display | Not documented. Keep the plan default (programme loudness, one value). Do not build per-channel K-windows (no `momentaryCh`/`shortTermCh`) unless the user supplies a reference. Decide before T-049: **programme loudness**. | T-049, T-045 |
| R6 | LU reference | Use -23 LUFS (EBU R128 target), labelled inferred. | T-049, T-045 |
| R7 | Ballistics ranges and defaults | The guides give none. Keep the plan values as user-stated: 1.00 / 750 / 1000 ms; the plan ranges (0.1-3000, 10-5000, 0-10000) are acceptable. Add to the spec that integration below about 50 ms measures low-frequency peaks, not RMS (Primary p.29): the UI hint should say so in its own words. | T-051, T-045 |
| R8 | Integration definition | Plan's "one-pole on power, τ = T/ln 100" matches the documented "time to fall 20 dB" and rise convention. Keep. Integration does not affect the **peak** indication (Primary p.28); the plan's separate peak part is consistent. | T-049 |
| R9 | Release scope | SPAN is silent on whether release applies to the peak part. The plan applies it to the level part (in `MeterBank`) and to the peak part and hold tick (editor). Keep as an Investigate design choice; document as inferred. | T-049, T-057 |
| R10 | Peak hold after expiry | Not documented. Keep: hold tick falls at the release rate after the hold time. | T-057 |
| R11 | Density | Confirmed intent only (levels where the signal dwells; uses integration, release and hold). Histogram size, bin width and decay with the hold time are Investigate choices. Make the histogram decay time exactly `meterPeakHold` as the plan states. | T-049, T-045 |
| R12 | Stereo definition | SPAN does not define a two-channel group's spectrum. Keep L+R power mean as "Stereo"; no change. | T-046 |
| R13 | Underlay choices | SPAN's Underlay chooses a channel group. Investigate's list Off/Mid/Side/Left/Right/Stereo is an acceptable mapping; "Off" must exist. Hold auto-releases on a group change: matches the plan's auto-unfreeze. | T-046, T-045 |
| R14 | Meters and groups | SPAN meters follow the group, with "Show All Channel Meters". Investigate meters the main input only. Keep (Decision 2026-10-03), and state in the spec that this deliberately differs: no M/S meters. | T-045, T-049 |
| R15 | Align 0 dB | Use the calibration method above; the test set is the T-047 set. | T-047, T-051 |
| R16 | Anti-alias | Drawing only: confirmed. | T-045, T-054 |
| R17 | Reference slots | Slot count: SPAN does not specify; four stays an Investigate choice. SPAN has **one "Filled Display" switch for all slots**; the plan's no-fill line style is a deliberate difference and may add one global fill switch. dB shift range -40 to +40 is an Investigate choice. | T-045, T-048, T-051 |
| R18 | CSV format | SPAN: linear frequency in Hz and dB; other layout not documented. Plan's export (`#` comment lines, header `frequency_hz,level_db`, comma, dot) and tolerant importer are fine. Note that SPAN's own CSV (linear spacing, probably dense) is resampled by log interpolation; with linear data of few points the low end is coarse, so the importer should accept any number of rows and treat 0 Hz rows (frequency not above 0) as skipped. Verify with a real SPAN-exported file from the user; none was available. | T-058, T-048, T-045 |
| R19 | Band solo | Gesture confirmed (Ctrl then left-drag, wheel = bandwidth, band-pass curve drawn). The preview with Ctrl alone is an Investigate addition. Filter type not documented, so the T-044 references decide. | T-045, T-050, T-056 |
| R20 | Colours | Confirmed roles: primary, Underlay, secondary (darker shade), static slots, global per plug-in type, immediate update in open instances, file Load and Save. Matches the plan. | T-045, T-054 |

Nothing found changes the slot count, the Stereo definition or the Underlay model in a way that blocks T-046 or T-047.

## Licences and IP note
- Only descriptions of behaviour were taken. Parameter ranges, ballistic definitions and metering behaviour are not protected; the SPAN and Primary guide wording is copyrighted by Voxengo and must not enter the product, tooltips, help text or `docs/design/`.
- Names that are generic or standards-based and safe to use in the UI: **dBFS**, **dBFS+3** (descriptive; avoid "dBFS.30/.15" only if the user prefers, they are plain descriptions of range), **K-20, K-14, K-12** (Bob Katz's K-system, a public term), **K-20 C** style suffix (descriptive; may be labelled "K-20 (cal.)" to avoid copying), **LUFS, LU, ML/SL** (EBU/ITU terms; "momentary" and "short-term" are the standard words and preferable in labels), **Underlay** (an ordinary English word, a feature name; acceptable, with Investigate's own wording), **Align 0 dB**, **Anti-alias**, **Density**, **Take**, **Hold**, **Integration, Release, Peak hold**. Hann, Blackman: textbook names.
- Not to use: the names "SPAN" or "Voxengo", logos, icons, screenshots, the layout artwork and colour scheme names, the `.csf`, `.ccf` formats and preset names such as "Dual Mono", "Mid-Side Stereo", "Stereo Side-Chain". Plain CSV is a convention and free to use. Do not claim "SPAN compatible" in marketing; "imports linear-frequency CSV" is a neutral statement.
- Product formats stay Investigate's own: `.invcolours`, `.invpreset`.
- No third-party code or library was identified or recommended, so no licence entries apply.

## Open questions
1. Can the user supply a SPAN-exported `.csv` (or a screenshot of dBFS.30 and dBFS.15 scales)? It would settle R1 and R18.
2. Confirm the ballistics defaults 1.00 / 750 / 1000 ms; they could not be verified, and 1.00 ms makes the level part behave like a peak meter at low frequencies (SPAN states this for about 2 ms).
3. Confirm "programme loudness, one value" for the LUFS/LU bars (R5).

## Sources
- SPAN User Guide v3.24: https://www.voxengo.com/files/userguides/VoxengoSPAN_en.pdf/getbyname/Voxengo%20SPAN%20User%20Guide%20en.pdf (cached `research/_cache/span/span_guide.pdf`; pages 3-6 used)
- Voxengo Primary User Guide: https://www.voxengo.com/files/userguides/VoxengoPrimaryUserGuide_en.pdf/getbyname/Voxengo%20Primary%20User%20Guide%20en.pdf (cached `research/_cache/span/primary_guide.pdf`; pages 11, 13, 17, 18, 23-30, 39-40 used)
- SPAN product page: https://www.voxengo.com/product/span/ (not needed beyond the guides)
- `docs/research/T-001-span-feature-parity.md` (§3, §4, §5, §8, §9), `docs/plans/m3c-feature-expansion.md`, `source/dsp/SpectrumMapper.h` (pipeline description)
- A web search for numeric defaults and scales found only the same guide text; no forum or video source with numbers was found.
