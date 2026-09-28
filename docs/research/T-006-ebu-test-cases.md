# T-006: EBU Tech 3341 / Tech 3342 conformance test-case transcription

## Sources fetched (primary, verified 2026-09-28)
- EBU Tech 3341, "Loudness Metering: 'EBU Mode' metering to supplement EBU R 128 loudness
  normalization", Version 4.0, November 2023.
  Direct PDF: https://tech.ebu.ch/files/live/sites/tech/files/shared/tech/tech3341.pdf
  Landing page: https://tech.ebu.ch/publications/tech3341
  Cached locally at `research/_cache/ebu/tech3341.pdf` (gitignored).
- EBU Tech 3342, "Loudness Range: A measure to supplement EBU R 128 loudness normalisation",
  Version 4, November 2023.
  Direct PDF: https://tech.ebu.ch/files/live/sites/tech/files/shared/tech/tech3342.pdf
  Landing page: https://tech.ebu.ch/publications/tech3342
  Cached locally at `research/_cache/ebu/tech3342.pdf` (gitignored).

Both PDFs downloaded and read in full (10 pages / 8 pages respectively); every test-case table
and its surrounding prose was read directly from the document, not from any secondary summary.
Values below are transcribed verbatim from Table 1 of each document. **Do not** cross-reference
`docs/research/T-002-dsp-references.md` — its EBU table is flagged by the roadmap as unreliable
and was not used as a source for this file.

Note on document history: both documents' "Revised" history shows V2 (Aug 2011, relative gate
changed from −8 to −10 LU for Tech 3341 / Integrated gate) and V2 (Aug 2011, LRA relative gate
changed from −8 to −10 LU is *not* what happened for 3342 — see Tech 3342 note below) — the
gate values transcribed here are the current (V4, Nov 2023) values in force.

Important cross-check: Tech 3342's LRA relative gate is **−20 LU**, not −10 LU (that −10 LU value
is the Integrated Loudness relative gate from Tech 3341 §2.3 / ITU-R BS.1770, which is a different
gate for a different measurement). Do not conflate the two gates when implementing `LoudnessMeter`.

---

## Part A — EBU Tech 3341, Table 1: "Minimum requirements test signals" (23 cases)

All cases: "The loudness meter shall be reset before each measurement." Source: Tech 3341 v4,
§2.9, pp. 8-9, Table 1.

Gating/measurement definitions referenced by these cases (Tech 3341 §2.2-2.3, verbatim):
- Momentary (M): sliding rectangular window 0.4 s, ungated.
- Short-term (S): sliding rectangular window 3 s, ungated; live-meter update rate ≥10 Hz.
- Integrated (I): ITU-R BS.1770 gating — absolute gate −70 LUFS, relative gate −10 LU below the
  absolute-gated loudness, applied to 400 ms blocks with 75% overlap; live-meter update rate ≥1 Hz.
- Calibration reference (§2.9, not a numbered test case): 1 kHz stereo sine, in-phase, peak level
  −18 dBFS → meter should read −18.0 LUFS (no explicit tolerance stated in the text for this one;
  it is described as a "basic calibration and alignment check," not a pass/fail test case).

| Case | Test signal (as specified in the document) | Expected value(s) | Stated tolerance | Synthesisable? |
|---|---|---|---|---|
| 1 | Stereo sine, 1000 Hz, −23.0 dBFS per-channel peak level, in phase both channels, 20 s duration | M, S, I = −23.0 LUFS; M, S, I = 0.0 LU | ±0.1 LUFS / ±0.1 LU | Yes — fully specified |
| 2 | As #1 at −33.0 dBFS | M, S, I = −33.0 LUFS; M, S, I = −10.0 LU | ±0.1 LUFS / ±0.1 LU | Yes |
| 3 | 3 tones like #1: 10 s at −36.0 dBFS; 60 s at −23.0 dBFS; 10 s at −36.0 dBFS | I = −23.0 LUFS; I = 0.0 LU | ±0.1 LUFS / ±0.1 LU | Yes |
| 4 | 5 tones like #1: 10 s @ −72.0 dBFS; 10 s @ −36.0 dBFS; 60 s @ −23.0 dBFS; 10 s @ −36.0 dBFS; 10 s @ −72.0 dBFS | I = −23.0 LUFS; I = 0.0 LU | ±0.1 LUFS / ±0.1 LU | Yes |
| 5 | 3 tones like #1: 20 s @ −26.0 dBFS; 20.1 s @ −20.0 dBFS; 20 s @ −26.0 dBFS | I = −23.0 LUFS; I = 0.0 LU | ±0.1 LUFS / ±0.1 LU | Yes |
| 6 | 5.0-channel sine, 1000 Hz, 20 s duration, per-channel peak levels: L/R −28.0 dBFS, C −24.0 dBFS, Ls/Rs −30.0 dBFS (LFE excluded, per BS.1770 channel weighting) | I = −23.0 LUFS; I = 0.0 LU | ±0.1 LUFS / ±0.1 LU | Yes — but requires 5-channel (not just stereo) support in the harness |
| 7 | "Authentic programme 1", stereo, narrow loudness range (NLR), genre similar to a commercial/promo | I = −23.0 LUFS; I = 0.0 LU | ±0.1 LUFS / ±0.1 LU | **No** — real-world audio file, content not specified in the document; official file required |
| 8 | "Authentic programme 2", stereo, wide loudness range (WLR), genre similar to a movie/drama | I = −23.0 LUFS; I = 0.0 LU | ±0.1 LUFS / ±0.1 LU | **No** — official file required |
| 9 | 2 tones like #1, repeated 5×: 1.34 s @ −20.0 dBFS; 1.66 s @ −30.0 dBFS | S = −23.0 LUFS, constant after 3 s | ±0.1 LUFS | Yes |
| 10 | File-based meters. 20 segments, tones like #1: (i·0.15 s silence; 3 s @ −23.0 dBFS; 1 s silence) for i = 0..19 — intended as 20 separate files | Max S = −23.0 LUFS, for each segment | ±0.1 LUFS | Yes |
| 11 | Live meters. 20 tones like #1 in one continuous file: (i·0.15 s silence; 3 s @ (−38.0+i) dBFS; (3 − i·0.15) s silence) for i = 0..19 | Max S sequence = −38.0, −37.0, −36.0, ..., −19.0 LUFS (successive values, one per i) | ±0.1 LUFS | Yes |
| 12 | 2 tones like #1, repeated 25×: 0.18 s @ −20.0 dBFS; 0.22 s @ −30.0 dBFS | M = −23.0 LUFS, constant after 1 s | ±0.1 LUFS | Yes |
| 13 | File-based meters. 20 segments, tones like #1: (i·20 ms silence; 400 ms @ −23.0 dBFS; 1 s silence) for i = 0..19 — 20 separate files | Max M = −23.0 LUFS, for each segment | ±0.1 LUFS | Yes |
| 14 | Live meters. 20 tones like #1 in one file: (i·20 ms silence; 400 ms @ (−38.0+i) dBFS; (400 − i·20) ms silence) for i = 0..19 | Max M sequence = −38.0, −37.0, ..., −19.0 LUFS (successive values) | ±0.1 LUFS | Yes |
| 15 | Stereo sine, freq = fs/4 (e.g. 12 kHz @ 48 kHz), amplitude 0.50 FFS ("fraction full scale"), phase 0.0°, 10 ms fade-in/out, duration immaterial | Max true-peak = −6.0 dBTP | +0.2 / −0.4 dBTP | Yes — true-peak, not loudness, but included for completeness |
| 16 | As #15 but phase 45.0° | Max true-peak = −6.0 dBTP | +0.2 / −0.4 dBTP | Yes |
| 17 | Stereo sine, freq = fs/6, amplitude 0.50 FFS, phase 60.0° | Max true-peak = −6.0 dBTP | +0.2 / −0.4 dBTP | Yes |
| 18 | Stereo sine, freq = fs/8, amplitude 0.50 FFS, phase 67.5° | Max true-peak = −6.0 dBTP | +0.2 / −0.4 dBTP | Yes |
| 19 | Stereo sine, freq = fs/4, amplitude 1.41 FFS, phase 45.0° | Max true-peak = +3.0 dBTP | +0.2 / −0.4 dBTP | Yes |
| 20 | Stereo sine, freq = fs/6, amplitude 0.50 FFS, containing a single period of a fs/4 sine at amplitude 1.00, phase-continuous at both edges; synthesized at 4·fs then anti-alias-filtered and downsampled to fs with 0-sample offset; short fade-in/out | Max true-peak = 0.0 dBTP | +0.2 / −0.4 dBTP | Yes, but non-trivial to synthesize precisely (requires 4× oversample + specific downsample-phase alignment) |
| 21 | As #20, downsampled with 1-sample offset (at the 4·fs rate) | Max true-peak = 0.0 dBTP | +0.2 / −0.4 dBTP | Yes (same caveat as #20) |
| 22 | As #20, downsampled with 2-sample offset | Max true-peak = 0.0 dBTP | +0.2 / −0.4 dBTP | Yes (same caveat) |
| 23 | As #20, downsampled with 3-sample offset | Max true-peak = 0.0 dBTP | +0.2 / −0.4 dBTP | Yes (same caveat) |

Notes transcribed from the document:
- Cases 10/11 and 13/14 are explicitly stated to be duals of each other (file-based vs. live-meter
  variants of the same underlying stepped-level pattern).
- Cases 15-23 test **true-peak**, not loudness (LoudnessMeter's peak-limiter sibling feature, if
  any); include here since T-006 may cover true-peak too, but flag to the coder that these are a
  distinct sub-measurement from M/S/I loudness.
- The document explicitly says LFE is excluded from BS.1770/EBU Mode measurement (§2.10) — case 6
  is a 5.0 signal with L/R/C/Ls/Rs only (no LFE channel is being fed/weighted).
- Test signals for cases 1-14 (i.e., all sine-based loudness cases) are downloadable as pre-made
  WAV files from the EBU ("tech.ebu.ch/loudness"), but every parameter needed to regenerate them
  synthetically is also given in the document text, so they are still classified "synthesisable"
  here — you do not need the official files to write a valid unit test, though downloading them
  as a cross-check would add confidence.

---

## Part B — EBU Tech 3342, Table 1: "Minimum requirements test signals" for Loudness Range (6 cases)

Source: Tech 3342 v4, §4, p. 6, Table 1. All cases: "the expected response is unchanged if the
test signal is repeated one or more times in its full length. The loudness meter shall be reset
before each measurement."

LRA algorithm definition used by these cases (Tech 3342 §3.1, verbatim, distinct from Tech 3341's
Integrated-loudness gate):
- Input: short-term loudness computed with a sliding 3 s window, **ungated**, sampled at ≥10 Hz
  (≥2.9 s block overlap).
- Absolute gate: −70 LUFS.
- Relative gate: **−20 LU** below the absolute-gated integrated loudness of the already
  absolute-gated values (note: this is a different relative-gate value than the −10 LU used for
  Integrated Loudness in Tech 3341/BS.1770 — do not reuse the same constant for both).
- LRA = (90th-percentile-range) = difference between the 95th and 10th percentiles of the
  relative-gated short-term loudness distribution.
- Percentile indexing given explicitly in the reference MATLAB (rank-based, not interpolated):
  `stl_sorted_vec(round((n-1)*P/100 + 1))` for P = 10 and P = 95, 1-based MATLAB indexing i.e.
  0-based index = `round((n-1)*P/100)`.
- For file-based measurement, the document recommends appending ≥1.5 s of trailing silence before
  computing the final LRA (to flush the 3 s analysis-window latency).

| Case | Test signal | Expected LRA | Stated tolerance | Synthesisable? |
|---|---|---|---|---|
| 1 | Stereo sine, 1000 Hz, −20.0 dBFS per-channel peak, in phase, 20 s, immediately followed by same signal at −30.0 dBFS (tones 10 dB apart) | LRA = 10 LU | ±1 LU | Yes — fully specified |
| 2 | As #1, tones at −20.0 dBFS and −15.0 dBFS | LRA = 5 LU | ±1 LU | Yes |
| 3 | As #1, tones at −40.0 dBFS and −20.0 dBFS | LRA = 20 LU | ±1 LU | Yes |
| 4 | As #1 but 5 tone-segments, 20 s each: −50.0, −35.0, −20.0, −35.0, −50.0 dBFS | LRA = 15 LU | ±1 LU | Yes |
| 5 | "Authentic programme 1", stereo, narrow LRA (NLR), genre similar to a commercial/promo | LRA = 5 LU | ±1 LU | **No** — official file required, content unspecified |
| 6 | "Authentic programme 2", stereo, wide LRA (WLR), genre similar to a movie/drama | LRA = 15 LU | ±1 LU | **No** — official file required |

Additional figure in the document (§3.1, Fig. 1, illustrative, not a numbered compliance test
case): a worked example using "The Matrix" (DVD version) shows absolute-gated loudness
−21.6 LUFS, relative gate at −41.6 LUFS (i.e. −20 LU below −21.6), and LRA = 25 LU for the full
film. This is **not** one of the 6 official test cases and depends on a specific commercial film
file not available for synthesis or redistribution — do not use it as a unit test; it is included
here only for context/sanity-checking the gating-threshold arithmetic (−21.6 − 20 = −41.6, checks
out algebraically).

---

## Summary: synthesisable vs. official-file-required

- Tech 3341: 23 cases total. 21 synthesisable (cases 1-6, 9-23), 2 require official files
  (cases 7, 8 — the "authentic programme" NLR/WLR segments).
- Tech 3342: 6 cases total. 4 synthesisable (cases 1-4), 2 require official files
  (cases 5, 6 — same "authentic programme" NLR/WLR pattern as Tech 3341 cases 7/8; likely the
  same two underlying source files, reused across both documents, though the documents do not
  state this explicitly).

Combined: 25 synthesisable cases, 4 official-file-required cases, out of 29 total conformance
cases across the two documents.

## Open verification points
- The official NLR/WLR programme WAV files (Tech 3341 #7/#8, Tech 3342 #5/#6) are referenced as
  downloadable from "tech.ebu.ch/loudness" (EBU reference [7] in 3341, [5] in 3342) but the exact
  current download URL was not located during this pass — the old `http://tech.ebu.ch/loudness`
  link in the document is likely stale. If these 4 cases are needed for CI, a follow-up research
  task should locate the current EBU loudness test-signal download page.
- Cases 15-23 (true-peak) are out of scope for a pure `LoudnessMeter` M/S/I implementation unless
  T-006 also covers true-peak metering; confirm with the task file whether true-peak is in scope.
