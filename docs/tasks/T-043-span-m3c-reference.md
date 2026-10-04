# T-043: SPAN reference for M3c (metering, ballistics, Align 0 dB, channel groups, static spectra)
- Owner: researcher
- Milestone: M3c
- Status: DONE
- Depends on: none

## Goal
The user chose Voxengo SPAN as the reference for the M3c metering modes and stereo modes, because their own reference images were not available. Several M3c items copy SPAN behaviour (Align 0 dB, anti-alias, meter ballistics, static spectra and CSV). Establish exactly how SPAN behaves, so the parameter ranges, the DSP and the UI spec rest on sources, not guesses.

## Inputs
- `docs/plans/m3c-feature-expansion.md`, the whole document. Its architect interpretations are what this report confirms or corrects.
- `docs/research/T-001-span-feature-parity.md`, §3, §4, §8 (IP boundaries) and §9.
- The local cache `research/_cache/span/`: `extracted_text.txt` (SPAN User Guide v3.24) and `primary_text.txt` (Voxengo Primary User Guide). Relevant pages:
  - Primary User Guide p.17: Anti-Alias and Align 0 dB;
  - p.18: Static Spectrums Editor and CSV;
  - p.23 to 27: Channel Routing, groups and colour scheme;
  - p.28 and 29: Density mode, Integration, Release and Peak Level Hold.
- Public sources: voxengo.com product and FAQ pages, the SPAN Plus guide, KVR and Gearspace threads, and video descriptions. Describe what they show; do not copy text or artwork.

## Questions to answer
1. **Metering modes.**
   - For dBFS, dBFS.30, dBFS.15 and dBFS+3: the level-meter scale, top and bottom.
   - For LUFS ML/SL and LU ML/SL: the scale, the LU reference (−23 LUFS?), and whether the bars show programme loudness (one value) or per-channel loudness.
   - What the RMS/"Integr" statistic shows in each mode.
2. **Density mode.** How it is drawn, and how the integration, release and peak-hold times affect it.
3. **Ballistics.**
   - The ranges and defaults of Meter Integration Time, Release Time and Peak Level Hold Time. The user states the defaults as 1.00, 750 and 1000 ms; confirm or correct them.
   - What the peak-hold marker does after the hold time ends (falls at the release rate, or jumps).
   - Whether release applies to the peak indication, the level indication, or both.
4. **Align 0 dB.**
   - What SPAN's level normalisation is without Align, and what changes with it.
   - How it stays independent of block size, and how it interacts with smoothing and window.
   - Compare with Investigate's current normalisation: a full-scale, bin-centred sine reads 0 dB before smoothing (coherent-gain normalisation in `source/dsp/SpectrumAnalyser.cpp`); smoothing and peak-preserving mapping then lower a sine's displayed peak.
   - Recommend an algorithm that meets the plan's criterion: a 1 kHz 0 dBFS sine reads 0 ± 0.5 dB at slope 0 for every block size, window and smoothing setting.
5. **Anti-Alias.** Confirm that it affects curve drawing only.
6. **Channel groups and Underlay.**
   - SPAN's routing presets (for example Stereo, Mono, Dual Mono, Mid-Side), and the group selector's choices.
   - What the spectrum of a two-channel group shows: a power mean, a sum or a maximum.
   - The Underlay selector's choices.
   - The default group.
   - Whether the meters follow the group (for example M and S meters in a mid-side group, "Show All Channel Meters").
7. **Static spectra.**
   - The number of slots; Take and Take 2nd; the dB shift range; the name; visibility; filled display.
   - The exact CSV layout SPAN writes and reads: header, separator, decimal point, columns, and frequency scale.
8. **Narrow-band sweep/solo** (Ctrl): any documented filter type, band width, or interaction details (a click needed or not, the visual feedback).
9. **Colours:** which spectrum elements SPAN's colour scheme lets the user change (the spectrum colour, the Underlay colour, others).

## Acceptance criteria
- [ ] `docs/research/T-043-span-m3c-reference.md` answers questions 1 to 9. Each answer is marked "confirmed (source, page)", "inferred" or "not documented".
- [ ] A **"Recommendations for M3c"** section lists every place where the plan's architect interpretation should change: a range, a default, a mode scale, the Stereo definition, the LUFS bar display, or the slot count. Each item names the affected tasks (T-045, T-046, T-047, T-049, T-051).
- [ ] An Align 0 dB recommendation gives a concrete method with its maths or a citation, and a test signal set for T-047.
- [ ] IP note: no SPAN text, artwork or file formats beyond plain CSV conventions enter the product. The report confirms which names are generic (for example "dBFS+3", "K-20 C", "Underlay") and safe to use in the UI.

## Log
- 2026-10-03 vst-architect: Created for M3c. Documentation only; may run in parallel with any C++ task. T-045, T-046, T-047, T-049 and T-051 wait for it.
- 2026-10-04 researcher: Report written to docs/research/T-043-span-m3c-reference.md. Guides give no numeric ranges, defaults or dBFS.30/.15 scales; ballistics defaults unverified. Align 0 dB: calibration from an analytic 1 kHz window model through the real mapper, test set included. LUFS bar display, Stereo definition, slot count and CSV header not documented; recommendations R1 to R20 name affected tasks. Open: user-supplied SPAN CSV/scale evidence.
