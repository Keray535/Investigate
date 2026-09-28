# T-010: UI spec, design tokens, SVG assets and HTML mockup
- Owner: ui-ux-designer
- Milestone: M3
- Status: DONE
- Depends on: T-001, T-020

## Goal
Design Investigate's editor so that gui-engineer can implement it without guessing. The direction is dark and SPAN-inspired, but the identity is our own. Deliver:
- a written spec covering layout, every control and readout, interactions, meter ballistics and resizing;
- design tokens;
- SVG assets;
- an interactive HTML mockup for the user to approve.

## Inputs
- `docs/roadmap.md`: Vision, Scope, the Architecture sections (parameter table and constraints, processor API, SpectrumFrame and MeterSnapshot contents, state format) and Decisions (visual direction).
- `docs/research/T-001-span-feature-parity.md`: sections 1, 3, 4, 5, 8 and 9. T-020 updated this report from the full SPAN guide text. Section 5 describes SPAN's functional zoning, sourced from a screenshot: a dominant spectrum, a narrow side level-meter column, and a bottom statistics band. Treat it as one reference, not a template. Section 8 lists what must not be copied, including SPAN's olive/yellow-green spectrum fill and grey chrome.
- `docs/research/T-002-dsp-references.md`: section 8 (rendering constraints that affect the design, such as fills, gradients and blur cost).

## Design requirements
- **Regions.**
  - The spectrum display is the dominant area.
  - A meter and statistics strip holds: L/R level bars with peak and RMS; clip indicators with sample and true-peak clip counts; numeric peak max, TP max, headroom (0 dBTP minus TP max), RMS and crest; LUFS M, S and I, LRA, M max and S max; the correlation bar; and the L/R balance readout.
  - A toggle collapses the meter strip to give the spectrum more room. Its state persists in `ViewState`.
  - A control bar or settings panel holds every spectrum and meter parameter.
  - The spec must also cover the freeze toggle and two reset actions: statistics, and spectrum all-time curves.
- **Coverage.** Every parameter in the roadmap table maps to a control, with its type, the format of its displayed value, and a double-click action that resets it to the default. Every MeterSnapshot field maps to a readout, with its number format, units and "−inf" handling.
- **Curves.** Primary, dual (second curve), secondary and sidechain curves must stay distinguishable without relying on hue alone: use lightness, line style or fill as well, so the design is colour-blind safe. Specify the line widths and fill opacity.
- **Grid and axes.** Frequency labels at the standard decade and third steps. dB lines and labels that adapt to the range. Behaviour at the minimum size.
- **Hover readout.** Show frequency (with the format per range), note name with cents (A4 = 440 Hz), and the level of the primary curve (and of the sidechain when it is shown). Specify where the readout sits and how it avoids the cursor. SPAN has no such readout, so this is an Investigate improvement; design it freely.
- **Spectrum interactions** (roadmap Scope):
  - Left-click on the spectrum resets the accumulated secondary curves (all-time max and all-time average); an explicit reset control also exists.
  - Right-click copies the frequency under the pointer to the clipboard, with visual confirmation.
  - Freeze turns off automatically when `channelMode` changes.
  Specify each so it does not conflict with hover.
- **Control conventions:** double-click resets a control to its default; the mouse wheel nudges the value; a fine-adjust modifier while dragging (for example Shift); and click-to-type on numeric values. Define the step sizes.
- **Meter ballistics.** Define peak-hold duration, bar fall rate, RMS bar behaviour, clip indicator latch and reset, and correlation bar smoothing. All ballistics are display-only; the numbers come from `MeterSnapshot`.
- **Sizing.** Give the default editor size, the minimum and maximum sizes, the aspect-ratio policy, and the layout rules as the editor grows. Everything must be readable at 100 % scaling and crisp at 200 % scaling.
- **Tokens.** Define colours, type scale, spacing and radii in `docs/design/tokens.json`, and describe them in the spec.
- **Fonts.** Fonts must be licensed under SIL OFL 1.1 or be equally permissive. List every font and its source in an open question, so the architect can route a license confirmation. Do not add font files until that confirmation arrives; the mockup may load them from the web.
- **SVG assets.** Put them in `assets/ui/`: icons for freeze, reset, settings and sidechain, plus a logo wordmark "Investigate". All SVGs are original work with no Voxengo-derived artwork.

## Deliverables
- `docs/design/ui-spec.md`
- `docs/design/tokens.json`
- `docs/design/mockup.html`: self-contained, with an animated fake spectrum and fake meters, working hover readout, and working controls where practical.
- `assets/ui/*.svg`

## Acceptance criteria
- [x] Every roadmap parameter and every MeterSnapshot field appears in the spec with its control or readout, format and interaction. The spec contains a checklist table proving full coverage.
- [x] The spec defines default, minimum and maximum editor sizes, and the layout at the default and minimum sizes.
- [x] The curve styles pass a colour-blind check: the spec states the method, for example a deuteranopia and protanopia simulation of the mockup.
- [x] Text meets a WCAG contrast ratio of at least 4.5:1 for readouts and 3:1 for grid labels against their backgrounds, with values listed in the spec.
- [x] `mockup.html` opens offline in a current Chromium browser (web fonts may fall back) and demonstrates the hover readout, the freeze toggle and at least the Mode, Block size, Slope and Range controls.
- [x] The SVG assets are valid, use `currentColor` or token colours, and render crisply at 16, 24 and 32 px.
- [x] Nothing reproduces SPAN's artwork, logo or distinctive trade dress, as checked against T-001 section 8.
- [x] The user approves the mockup. The architect records the approval in the task log before T-011 starts.

## Log
- 2026-09-28 vst-architect: Answered the designer's open questions (roadmap Decisions dated 2026-09-28): 50 ms display peaks, `blockCounter`, shared effective-range formulas, two secondary curves in the dual modes, secondary curves reset at the DSP level when the mode or resolution changes (the editor need not request it), and a 500 ms correlation window. The fonts were approved by T-022. In the revision pass after user review, add `Inter-Medium.ttf`, `Inter-SemiBold.ttf` (Inter v4.1, `extras/ttf/`), `JetBrainsMono-Regular.ttf` and `JetBrainsMono-Medium.ttf` (v2.304, `fonts/ttf/`) to `assets/ui/fonts/`, together with the OFL text as `OFL-Inter.txt` and `OFL-JetBrainsMono.txt`. Also add a spec section for the About or credits view.
- 2026-09-28 vst-architect: The user approved `docs/design/mockup.html` without changes. What remains is the final pass: add the font files and OFL texts, write the About or credits spec section, and reconcile the spec with the architect decisions above. After that pass, T-010 is DONE.
- 2026-09-28 vst-architect: Re-scoped. The designer has no shell, so it cannot fetch binary files. Fonts and license texts are pinned third-party files, so T-021 (build-engineer) downloads them with checksum verification into `third_party/fonts/` and compiles them into the `InvestigateAssets` binary-data target. They will not be added to `assets/ui/fonts/`. gui-engineer loads them through `BinaryData` as before; spec sections 3.2 and 11 should be read with that source location. The remaining open questions (JUCE notice wording, the pffft license file, VST3 SDK and trademark notices) are routed to T-021 and T-023. All spec acceptance criteria are met, so the status is DONE.
- 2026-09-28 ui-ux-designer: Delivered `docs/design/ui-spec.md`, `docs/design/tokens.json`, `docs/design/mockup.html`, and 12 SVGs in `assets/ui/` (wordmark, mark, and five icons at 24 and 16 px). Decisions:
  - Native JUCE with Direct2D; no WebView.
  - Default size 1200x700, minimum 920x540, maximum 2560x1600, free aspect ratio.
  - Layout: header with the Mode segmented control and actions; the spectrum plus a bottom control bar of "chips" (label over value); a 296 px right meter strip holding bars, a statistics table, loudness and correlation. The Range group moves into the Settings popover below 880 px of available bar width, and the Settings popover always holds every parameter.
  - Curves are distinguished by style as well as hue: primary A is a line with a gradient fill; primary B is a line only; secondaries are thin and dark; the sidechain is a flat silhouette with a dim edge. Checked with Machado 2009 protanopia, deuteranopia and tritanopia simulation plus greyscale; the mockup includes the simulation selector.
  - Contrast values are tabulated in spec section 10.1: all readouts at least 4.5:1, grid labels 5.0:1.
  - Hover readout: 3 significant figures for frequency, note name with cents (A4 = 440 Hz), and curve levels, in a card that flips away from the plot edges.
  - Left-click resets the curves (4 px movement slop); right-click copies the frequency, with a toast.
  - Freeze turns off and the curves reset on a channelMode change.
  - Meter ballistics: peak falls at 20 dB/s; hold lasts 1.5 s, then falls at 30 dB/s; clip lamps latch until Reset statistics; the correlation display filter is 50 ms; numeric readouts refresh at 10 Hz.
  - Fonts proposed: Inter and JetBrains Mono (both OFL 1.1). No font files added yet; the mockup loads them from Google Fonts and falls back to Segoe UI and Consolas offline.
  - Open questions are in spec section 15: font license; snapshot peak semantics; snapshot freshness counter; effective-range rule shared with T-005; dual-mode secondaries; reset on mode change; T-012 note-name example; correlation constant.
  - Awaiting user approval of the mockup before T-011.
- 2026-09-28 ui-ux-designer: Final pass. The approved visual design and layout are unchanged, and `mockup.html` was not edited.
  - **Spec reconciled with the architect decisions:**
    - snapshot `samplePeak` and `truePeak` are 50 ms maxima (§8.2, §8.3, §13.2);
    - `blockCounter` staleness rule: 250 ms, compared with `!=` (§8.7, §13.2);
    - the effective-range formulas come from the shared `source/dsp/DisplayRange.h`, including `dbHiEff`, and the editor must not re-implement them (§5.4);
    - two secondary curves in the dual modes (§5.1, §13.3);
    - the secondary reset on a channelMode, fftSize, window or sample-rate change is DSP-side, so the editor no longer calls `requestSpectrumReset()`; the auto-unfreeze stays (§5.7);
    - the correlation window is 500 ms (§8.5);
    - noteName examples: 452.6 Hz gives "A4 +49c" and 452.9 Hz gives "A#4 −50c" (§5.5).
  - **Fonts:** §3.2 now lists the bundled fonts, with the pinned release zips, in-zip paths, license files and copyright lines. Segoe UI and Consolas are the load-failure fallbacks. `tokens.json` font entries updated.
  - **About view:** new §6.2, reached from an "About" button in the Settings title bar. A 560 × 440 popover view holds the logo, version line, company, and a scrollable read-only notices viewport. It contains the font copyright and OFL notices, the JUCE and pffft notices, and the verbatim OFL and pffft texts from binary-data resources. Accessibility names and focus order are in §6.2 and §10.3.
  - **Open questions:** §15 now lists four: the JUCE notice line and the AGPL/GPL wording; the pffft license file in binary data; the font download; and the VST3 SDK and Steinberg notices.
  - **BLOCKED on one item only:** the four TTFs and two OFL texts were not added to `assets/ui/fonts/`. This session had no shell or binary-download tool (only file read/write and a markdown-converting web fetch), so a byte-exact, unmodified copy was not possible. Needs routing to an agent with a shell (for example build-engineer): download `Inter-4.1.zip` and `JetBrainsMono-2.304.zip` into `research/_cache/`, and extract the files listed in spec §11. Everything else in this task is complete. Set DONE once the files are in place.
