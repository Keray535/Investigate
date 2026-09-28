# T-001: SPAN feature parity and UX survey
- Owner: researcher
- Milestone: M0
- Status: DONE
- Depends on: none

## Goal
Build a precise, sourced inventory of Voxengo SPAN's features, controls, default values, meter definitions and interactions. Investigate's parameter table, meter set and UI spec will then match SPAN's behaviour wherever the roadmap scope includes that feature. The report also lists what Investigate must not copy, for trademark and trade-dress reasons.

## Inputs
- `docs/roadmap.md`, sections Vision, Scope, Architecture (parameter table, MeterSnapshot contents, constraints) and Decisions. Values marked * in the parameter table are provisional and waiting for this report.
- Public sources: the Voxengo SPAN product page and user guide (PDF), the Voxengo primary user guide (shared controls such as routing, presets and UI scaling), the Voxengo forum, KVR and Gearspace threads, and published reviews or videos with screenshots.
- For comparison: the public documentation of 2 or 3 other analysers, for example FabFilter Pro-Q's analyser, iZotope Insight, MeldaProduction MAnalyzer and Blue Cat FreqAnalyst.

## Deliverable
Write `docs/research/T-001-span-feature-parity.md` with these sections:
1. **Feature inventory.** Cover the current free SPAN version. For every control give its name, range or choices, default value, and behaviour. Mark each feature as Free or Plus-only, and as In scope or Out of scope for Investigate v1 according to the roadmap Scope section.
2. **Spectrum settings.** Cover these settings and their defaults: block size range, overlap, window type(s), averaging time and how averaging is applied (power or dB domain, exponential or linear), smoothing, slope and its pivot frequency, display dB range, frequency range, the secondary spectrum modes and how real-time max releases, the channel modes and routing, and how stereo is combined in SPAN's default mode.
3. **Statistics and meters.** Cover every value SPAN displays, with its exact definition and ballistics: peak, RMS (integration time and sine reference), K-system modes, crest factor, true peak (oversampling factor if documented), clipping count (what counts as a clip), EBU R128 values (M, S, I, LRA and max values), the correlation meter (integration time and scale), and what resets each value.
4. **Interactions.** Cover the hover readout content and format (frequency, note name with cents, level), click, drag, wheel and double-click behaviour on the spectrum and meters, freeze, zoom, reset, editor resizing and UI scaling, and keyboard shortcuts.
5. **Layout.** Describe SPAN's regions, their relative proportions and the information hierarchy in words. Describe only; do not reproduce the artwork.
6. **Comparison.** List the table-stakes analyser features found in the other products, and state whether each is in or out of Investigate v1 scope.
7. **Gap list and recommendations.** Make a table of every roadmap parameter marked *, plus the fixed constants (real-time max release and correlation integration time). Give SPAN's value, your recommended Investigate value, and the reason. Also list SPAN features that are missing from the roadmap's in-scope list but cheap to add, as suggestions only.
8. **IP boundaries.** List what Investigate must not reuse: names, logos, artwork, colour schemes that are distinctive trade dress, manual text, preset names and files, and anything else you find relevant.

## Acceptance criteria
- [ ] The report exists at `docs/research/T-001-span-feature-parity.md` with all 8 sections.
- [ ] Every parameter marked * and both fixed constants have a recommended value that cites a source, or an explicit statement "not documented, recommend X because Y".
- [ ] Every SPAN feature listed is labelled Free or Plus-only, and In scope or Out of scope for v1.
- [ ] Every meter definition in section 3 cites a source, or is labelled "inferred" with the reasoning.
- [ ] Every factual claim has a source URL. Anything inferred from screenshots or videos is labelled "inferred".
- [ ] No third-party code, images or manual text are copied into the repository. Short quotes in the report are acceptable only with attribution.

## Log
- 2026-09-28 researcher: Delivered docs/research/T-001-span-feature-parity.md with all 8 sections. Primary SPAN PDF user guides are rasterised/image-only (no extractable text; pdftoppm unavailable), so most numeric SPAN defaults are marked 'not documented' with sourced recommendations instead of confirmed parity values. Confirmed from sources: slope default 4.5 dB/oct (matches roadmap, no change), K-system calibrated RMS/release 600 ms, correlation meter's 3-second balance window, ~50 ms peak-RMS window. Recommended keeping all other provisional roadmap defaults as-is (fftSize 4096, overlap 4x, window Hann, avgTime 1000 ms, smoothing 1/12 oct, secondary real-time max, dbLow/dbHigh -90/+6, rmsWindow 300 ms, real-time-max release 20 dB/s, correlation integration ~300 ms) since no SPAN source contradicts them; flagged each as unconfirmed-but-reasonable rather than SPAN-verified. Suggested two cheap additions: a 'freeze peaks' hold control and a headroom/margin-to-clipping readout. IP boundaries section lists names, logos, trade dress, and manual text that must not be reused.
