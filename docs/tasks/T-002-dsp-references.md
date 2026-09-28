# T-002: DSP references, standards and dependency licenses
- Owner: researcher
- Milestone: M0
- Status: DONE
- Depends on: none

## Goal
Collect the algorithms, standards, reference implementations, test signals and license verdicts that the M1 DSP tasks and the M4 and M5 tooling tasks need. The dsp-engineer and build-engineer should be able to implement from the report without further research.

## Inputs
- `docs/roadmap.md`, sections Architecture (signal flow, modules, threading model, budgets), Risks and Decisions.
- `CLAUDE.md`, sections Technical baseline and Real-time safety rules.
- Distribution intent: a free public release of closed-source binaries is the default assumption. The JUCE license tier is still open (see roadmap risk 9). Strong-copyleft code must not enter the product. Copyleft tools that are only run, not linked or shipped, are acceptable if you confirm that.

## Deliverable
Write `docs/research/T-002-dsp-references.md` with these sections. Each section ends with a concrete recommendation.
1. **FFT library.** Compare `juce::dsp::FFT` on Windows without Intel IPP (the fallback implementation) against pffft (for example the marton78/pffft fork), KissFFT and any other permissively licensed option. Give the license, whether it supports real-input FFTs up to 65536 points, integration effort with CMake, and published or estimated timings for real FFTs of 4096 and 65536 points. Reject GPL-only options such as FFTW and KFR's GPL edition.
2. **Spectrum display techniques.**
   - Window coherent-gain correction, so that a full-scale sine reads 0 dBFS. Include the scalloping loss per window.
   - Power-domain versus dB-domain averaging, and exponential averaging with a time constant when the hop size changes.
   - Mapping FFT bins to log-spaced display points: interpolation where points are denser than bins, and peak-preserving or power-mean aggregation where bins are denser than points.
   - Fractional-octave smoothing algorithms, with citations (for example Hatziantoniou and Mourjopoulos 2000).
   - Slope or tilt implementation.
   - Open-source reference implementations with their licenses, for example the JUCE spectrum analyser tutorial and the ffAudio Frequalizer. GPL projects may be named for ideas only.
3. **Loudness (ITU-R BS.1770-4/-5, EBU R128).**
   - K-weighting coefficients at 48 kHz, and how to derive them for 44.1 to 192 kHz (for example from the analog prototype, as libebur128 does). Cite a source that evaluates implementation accuracy.
   - Momentary and short-term windows, the gating algorithm, and a histogram implementation that is real-time safe with fixed memory.
   - LRA according to EBU Tech 3342.
   - The EBU Tech 3341 and 3342 test cases, with expected values and tolerances. For each case, say whether the test signal can be synthesised from its written description or needs the official file. Give the download location and license of the official test files.
   - libebur128: its license and whether it is suitable as a test-only oracle.
4. **True peak.** Give the BS.1770 Annex 2 oversampling filter (tap count, phases, where to get coefficients, and their license), the recommended oversampling factor per sample rate from 44.1 to 192 kHz, and the Tech 3341 true-peak tolerances.
5. **Correlation meter.** Give the formula, common integration times and ballistics, and how silence and mono input are handled.
6. **K-system and RMS.** Cover the K-20, K-14 and K-12 scales, the AES17 sine-referenced RMS offset of +3.01 dB, and sliding-window versus exponential RMS, with memory implications for a 3000 ms window at 192 kHz.
7. **Lock-free exchange.**
   - SPSC ring buffer patterns with `juce::AbstractFifo` or `std::atomic`.
   - A triple buffer for publishing fixed-size frames.
   - Publishing a meter snapshot through atomics, including the memory ordering needed.
8. **JUCE 8 rendering on Windows.** Cover Direct2D renderer performance for filled paths at 60 fps, `juce::VBlankAttachment`, the pros and cons of attaching an OpenGL context in JUCE 8, and caching strategies. Cite JUCE docs and forum threads.
9. **Standalone input.** Explain how the JUCE Standalone wrapper handles "Mute audio input", and how to configure the Standalone so an analyser sees its input without feedback.
10. **License table.** For each item give the version or tag to pin, the license, and a verdict for this project (approved, test-only, tool-only, or rejected) with the reason:
    - JUCE 8: the current license terms for a free closed-source release, meaning the Starter tier conditions (revenue limit, splash screen or not, registration), compared with AGPLv3.
    - Catch2 v3.
    - pluginval.
    - Steinberg VST3 SDK and its `validator` tool.
    - The recommended FFT library.
    - libebur128.
    - The EBU test files.
    - Inno Setup and WiX, as installer options.
    - OpenCppCoverage, as a coverage tool.

## Acceptance criteria
- [ ] The report exists at `docs/research/T-002-dsp-references.md` with all 10 sections.
- [ ] Every section ends with a single concrete recommendation.
- [ ] K-weighting: the 48 kHz coefficients are listed to at least 10 significant digits with a source, and a method for other sample rates is given with a source.
- [ ] A table lists every EBU Tech 3341 and 3342 case with its expected value, its tolerance, and whether it can be synthesised or needs the official file.
- [ ] The license table covers every item listed in section 10, with a version or tag to pin, the license name, a verdict and a source URL.
- [ ] Every factual claim has a source URL.
- [ ] No third-party code is copied into the repository. Filter coefficients from published standards may be quoted with attribution.

## Log
- 2026-09-28 researcher: Delivered docs/research/T-002-dsp-references.md with all 10 sections. FFT recommendation: pffft (marton78 fork, BSD-like), rejecting FFTW (GPLv2) and KFR's free edition (GPLv3); pffft's actual Windows/MSVC timing at 4096/65536 points is an open question for T-004 to benchmark. Notable finding: the Steinberg VST3 SDK relicensed to MIT as of SDK 3.8 (late 2025), replacing the old GPLv3/proprietary dual license — recommend build-engineer confirm the vendored SDK version is >=3.8. JUCE 8 tier choice (Starter revenue-cap vs AGPLv3) remains an open user decision per roadmap risk 9; report states both tiers' terms. K-weighting coefficients quoted to 11 significant digits from BS.1770-4/libebur128. EBU Tech 3341/3342 test-case table included; official EBU test files are license "test-only" (EBU Terms of Use), to be kept in a gitignored fixtures path, not committed. OpenCppCoverage license (GPLv3, tool-only) flagged as not independently re-verified in this pass; build-engineer should re-check its LICENSE file before CI wiring. Status set to DONE (no QA report required for research tasks).
