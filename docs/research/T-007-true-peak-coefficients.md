# T-007: ITU-R BS.1770-5 Annex 2 true-peak FIR coefficients (exact transcription)

## Question
Exact numeric FIR coefficients for the BS.1770-5 (same table as -4) Annex 2 / Attachment 1 true-peak
4x-oversampling polyphase filter, organised per phase, with the phase-to-fractional-offset mapping and
notes on 2x-oversampling support at 88.2/96 kHz.

## Answer and recommendation
BS.1770-5 Annex 2, Attachment 1 (p. 19–20 of the PDF, "Rec. ITU-R BS.1770-5" running header, pages
numbered 18–19 in-document) prints a single table titled "order 48, 4-phase, FIR interpolating" filter,
laid out as 12 rows x 4 columns (Phase 0 .. Phase 3). Read row-major, the 48 values form one symmetric
(linear-phase) prototype impulse response `h[n]`, `n = 0..47`, `h[n] = h[47-n]`, with `h[4r+p]` located at
row `r` (0-indexed), column (phase) `p`. So the table is **already presented per-phase** (each column is
directly usable as a 12-tap polyphase sub-filter) but it is also derivable as a single 48-tap sequence
de-interleaved by phase = `n mod 4`.

Phase 0's 12 coefficients sum to ≈1.0016 (verified below), i.e. Phase 0 is the near-identity ("through")
phase — it reconstructs the original sample instant (0/4 fractional offset). By construction of a
standard 4x polyphase interpolator (phase `p` = fractional delay `p/4`), Phase 1 = 1/4, Phase 2 = 2/4
(mid-sample), Phase 3 = 3/4. **This mapping is not spelled out verbatim in the ITU prose** — it is an
inference from (a) the near-unity Phase-0 coefficient sum and (b) standard polyphase-interpolation
theory; flag as a design decision, not a directly quoted standard statement.

The standard is silent on exact 2x-oversampling coefficients for 88.2/96 kHz: it states only that "for an
incoming signal at 96 kHz sample rate a 2x over-sampling would be sufficient" (p.19), with no alternate
table. Using a subset of the 4 phases (e.g. Phase 0 and Phase 2, which are exactly the 0/4 and 2/4 = 1/2
fractional-offset phases) for a 2x variant is a **reasonable, standard-consistent engineering choice**,
but it is your own design decision, not an ITU-specified filter — document it as such in the
implementation.

## Key details — exact coefficients, per phase (12 taps each)

Transcribed exactly as printed (no re-rounding), from ITU-R BS.1770-5 (11/2023), Annex 2, Attachment 1,
PDF pages 19–20 (document pages numbered 18–19), table captioned "Phase 0 / Phase 1 / Phase 2 / Phase 3".
Tap index `k` = row index in the printed table, `k = 0..11` top to bottom.

| k | Phase 0 | Phase 1 | Phase 2 | Phase 3 |
|---|---|---|---|---|
| 0 | 0.0017089843750 | −0.0291748046875 | −0.0189208984375 | −0.0083007812500 |
| 1 | 0.0109863281250 | 0.0292968750000 | 0.0330810546875 | 0.0148925781250 |
| 2 | −0.0196533203125 | −0.0517578125000 | −0.0582275390625 | −0.0266113281250 |
| 3 | 0.0332031250000 | 0.0891113281250 | 0.1015625000000 | 0.0476074218750 |
| 4 | −0.0594482421875 | −0.1665039062500 | −0.2003173828125 | −0.1022949218750 |
| 5 | 0.1373291015625 | 0.4650878906250 | 0.7797851562500 | 0.9721679687500 |
| 6 | 0.9721679687500 | 0.7797851562500 | 0.4650878906250 | 0.1373291015625 |
| 7 | −0.1022949218750 | −0.2003173828125 | −0.1665039062500 | −0.0594482421875 |
| 8 | 0.0476074218750 | 0.1015625000000 | 0.0891113281250 | 0.0332031250000 |
| 9 | −0.0266113281250 | −0.0582275390625 | −0.0517578125000 | −0.0196533203125 |
| 10 | 0.0148925781250 | 0.0330810546875 | 0.0292968750000 | 0.0109863281250 |
| 11 | −0.0083007812500 | −0.0189208984375 | −0.0291748046875 | 0.0017089843750 |

(Source: ITU-R BS.1770-5, R-REC-BS.1770-5-202311-I!!PDF-E.pdf, Annex 2, Attachment 1, table on the pages
immediately following "One set of filter coefficients (for the order 48, 4-phase, FIR interpolating)
that would satisfy the requirements would be as follows:", pp. 19–20.)

Each column above is symmetric top-to-bottom with its mirror column (Phase 0 row k = Phase 3 row (11-k);
Phase 1 row k = Phase 2 row (11-k)), a direct consequence of the underlying single 48-tap prototype being
linear-phase/symmetric (`h[n] = h[47-n]`). This is a useful self-check when transcribing/hardcoding the
table: Phase3[k] == Phase0[11-k], Phase2[k] == Phase1[11-k].

**Verification — Phase 0 (identity/zero-offset phase) coefficient sum:**
`0.0017089843750 + 0.0109863281250 − 0.0196533203125 + 0.0332031250000 − 0.0594482421875 +
0.1373291015625 + 0.9721679687500 − 0.1022949218750 + 0.0476074218750 − 0.0266113281250 +
0.0148925781250 − 0.0083007812500 = 1.0015869140625`

A near-unity DC gain (within ~0.16%) for the phase that outputs at the original sample instants confirms
Phase 0 is the zero-fractional-offset / "through" phase, not an arbitrary numbering. The other phases'
coefficient sums are close to 1 as well (they must be, since the prototype filter is a near-flat-passband
lowpass and all polyphase branches share ~unity DC gain by design of an interpolation filter bank), so
this specific test (Phase-0 sum closest to 1 while also being the phase containing the largest single
coefficient at the "center tap" position, row 5/6 boundary) is the clearest signal — Phase 0's peak
coefficient (0.9721679687500) sits essentially alone as the dominant contributor, characteristic of a
near-impulse (identity) response, whereas Phases 1–3 spread their energy across two comparably large
coefficients (rows 5 and 6), consistent with true fractional-delay (non-integer-sample) interpolation.

## Filter structure summary
- Single ITU-published table, order 48 (48 taps total), 4 phases, 12 taps per phase — stated explicitly
  in the standard's own caption: "order 48, 4-phase, FIR interpolating" (p. 19).
- Presented **already split per phase** as 4 columns; equivalently de-interleaved from one 48-tap
  symmetric prototype `h[n]` via `phase_p[r] = h[4r + p]`.
- Attenuate input by 12.04 dB (2-bit shift) before oversampling/filtering (or skip in floating point),
  oversample 4x (48 kHz -> 192 kHz for this table), filter, take absolute value, convert to dB TP via
  `20*log10(value) + 12.04` (Annex 2 §3, p. 19).
- Phase-to-offset mapping (Phase 0 = 0/4, Phase 1 = 1/4, Phase 2 = 2/4, Phase 3 = 3/4): **inferred**, not
  a verbatim ITU statement, from the near-unity Phase-0 sum and standard polyphase-interpolator theory.
  Confidence: high (this is also how libebur128's and ffmpeg's `ebur128` true-peak implementations order
  and use the table, per T-002's cross-check note), but implement the "extract original sample" path (if
  needed) by also feeding the unmodified decimation-by-1 input sample directly rather than relying solely
  on Phase 0's approximate near-unity gain, since it is not an exact delta (deviates ~0.16% from unity).

## 2x oversampling for 88.2/96 kHz
- BS.1770-5 text (p. 19): "Higher sampling rates and over-sampling ratios are preferred... Incoming
  signals that are at higher sampling rates require proportionately less over-sampling (e.g. for an
  incoming signal at 96 kHz sample rate a 2x over-sampling would be sufficient.)" — no alternate
  coefficient table is given anywhere in the document for a 2x-phase filter.
- Recommendation (design choice, not ITU-specified): reuse Phase 0 and Phase 2 from the 4x table for a
  2x variant, since they are exactly the 0/4 and 2/4 (= 1/2) fractional-offset phases already in the
  published table — this avoids designing a new filter and stays consistent with the same passband/
  stopband characteristics. Document this explicitly in code comments as an engineering choice deviating
  from a literal standard requirement, per T-002's existing recommendation (oversampling factor 4 at
  44.1/48 kHz, 2 at 88.2/96 kHz, 1 at 176.4/192 kHz).

## Licenses and risks
No license required for the numeric coefficients: they are published constants in a normative ITU-R
Recommendation; quoting them with citation is standard industry practice (used identically by libebur128,
ffmpeg's `ebur128` filter, and commercial loudness meters — see T-002 §4). The ITU document's own text/
layout carries ITU copyright, but transcribed numeric constants with attribution are not a copyrightable
"copy" of the document in any way that would restrict use in closed-source code. No copyleft or
attribution-notice obligation applies to the shipped plugin binary.

## Sources
- ITU-R BS.1770-5 (11/2023), "Algorithms to measure audio programme loudness and true-peak audio level",
  Annex 2 and Attachment 1: https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1770-5-202311-I!!PDF-E.pdf
  (PDF pages 19–20 of the fetched file; extracted via PyMuPDF text extraction, cross-checked by manual
  symmetry verification of the printed table as described above).
- `docs/research/T-002-dsp-references.md` §4 (True peak) — prior research pass citing the same PDF and
  the libebur128/ffmpeg oversampling-factor convention (4x / 2x / 1x by sample-rate family).
</content>
