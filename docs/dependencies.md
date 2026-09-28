# Dependencies

Owner: build-engineer. This file is the single source of truth for pinned
versions, sources, licenses and update procedures for Investigate's
dependencies. Full license analysis (compatibility with a free closed-source
Windows release) is researched independently in `docs/research/` (T-002);
this file tracks the build-facing facts.

All three dependencies below (JUCE 8, Catch2 v3, pluginval) are pre-approved
baseline dependencies per `CLAUDE.md` and the roadmap Decisions log, so T-003
did not wait for the T-002 license report before integrating them. No other
dependency was added.

## JUCE

| | |
|---|---|
| Version | `8.0.15` (latest JUCE 8 release tag at the time of writing; JUCE 9.x exists but is out of scope — the roadmap pins JUCE 8) |
| Source | https://github.com/juce-framework/JUCE.git, `GIT_TAG 8.0.15`, `GIT_SHALLOW TRUE` |
| License | **Correction (T-021):** dual-licensed as **AGPLv3** (open source; the source of the whole application must be published) or the proprietary **JUCE 8 EULA** (Starter tier: closed source, capped at $20,000/year gross revenue; higher tiers remove the cap). Not GPLv3, as an earlier version of this row said. The roadmap (Risk 9) leaves the final tier choice to the user before M5; until then, this project builds under the assumption that a paid/Starter tier (or AGPLv3 publication) will be chosen before any public release, since AGPLv3 is incompatible with the closed-source distribution the roadmap targets. See https://juce.com/legal/juce-8-licence/ and `docs/research/T-002-dsp-references.md` section 10. |
| Usage | Linked. `juce_audio_utils`, `juce_dsp`, plus `juce::juce_recommended_config_flags`, `juce::juce_recommended_lto_flags`, `juce::juce_recommended_warning_flags`. |
| Integration | `FetchContent`, fetched into `third_party/` (via `FETCHCONTENT_BASE_DIR`), not vendored/committed. Chosen over a git submodule because it needs no extra `git submodule update` step for every agent/CI checkout, and `GIT_TAG` + `GIT_SHALLOW TRUE` already pin an exact, reproducible commit. |
| How to update | Bump `INVESTIGATE_JUCE_TAG` in the root `CMakeLists.txt` to a new JUCE 8.x release tag, delete `third_party/juce-*` (or run a clean configure), and rebuild. Re-run the full test suite, including pluginval, before merging. |

## Catch2

| | |
|---|---|
| Version | `v3.16.0` (latest Catch2 v3 release tag) |
| Source | https://github.com/catchorg/Catch2.git, `GIT_TAG v3.16.0`, `GIT_SHALLOW TRUE` |
| License | Boost Software License 1.0 (BSL-1.0), permissive, no conflict with a closed-source release. |
| Usage | Test only. Linked into `InvestigateUnitTests` via `Catch2::Catch2WithMain`. Never linked into the plugin targets. |
| Integration | `FetchContent`, guarded by `PLUGIN_BUILD_TESTS`, fetched into `third_party/`. |
| How to update | Bump `INVESTIGATE_CATCH2_TAG` in the root `CMakeLists.txt`, reconfigure, rerun `ctest`. |

## pluginval

| | |
|---|---|
| Version | `v1.0.4` |
| Source | https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Windows.zip |
| SHA-256 | `c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab` (computed by build-engineer from the exact release asset on 2026-09-28; pluginval does not publish its own checksums) |
| License | GPLv3 (Tracktion pluginval). Tool only — it validates the built plugin out of process and is never linked, so it does not affect the license of Investigate itself. |
| Usage | Tool only, test-time. Downloaded and checksum-verified at configure time by `cmake/DownloadPluginval.cmake` into `third_party/pluginval/1.0.4/pluginval.exe`. Run via the CTest `pluginval` test (label `validation`) against the built VST3 with `--strictness-level 10 --validate-in-process`. |
| Integration | `file(DOWNLOAD ... EXPECTED_HASH SHA256=...)` + `file(ARCHIVE_EXTRACT ...)`, guarded by `PLUGIN_BUILD_TESTS AND WIN32`. |
| How to update | Bump `PLUGINVAL_VERSION` and `PLUGINVAL_URL` in `cmake/DownloadPluginval.cmake`, download the new asset once, compute its SHA-256 (e.g. `sha256sum pluginval_Windows.zip`), replace `PLUGINVAL_SHA256`, delete `third_party/pluginval/`, and reconfigure. |

## pffft (marton78 fork)

| | |
|---|---|
| Version | `v1.1.0` (latest tag; commit `3cbff1ce5d2612d7b7d41dda8181eee7a6cfbe50`) |
| Source | https://github.com/marton78/pffft.git, `GIT_TAG v1.1.0`, `GIT_SHALLOW TRUE` |
| License | BSD-like (2-clause, derived from Julien Pommier's original BSD license and the FFTPACKv5 license it is based on); see `third_party/fonts/LICENSE-pffft.txt` (compiled into the binary as `BinaryData::LICENSEpffft_txt`) and `docs/research/T-002-dsp-references.md` section 10. |
| Usage | **Product.** The FFT engine behind `SpectrumAnalyser` (T-004). Linked into `Investigate` (the plugin's shared-code target) and, transitively, `InvestigateUnitTests`. |
| Integration | `FetchContent`, fetched into `third_party/pffft-src`. `SOURCE_SUBDIR` is pointed at a directory that does not exist in the pffft tree so `FetchContent_MakeAvailable` populates the source without running pffft's own `CMakeLists.txt` (which builds double-precision code, tests and benchmarks). A hand-written static library target `investigate_pffft` compiles only `src/pffft.c` and `src/pffft_common.c` — the single-precision real/complex FFT implementation. `src/fftpack.c` is **not** compiled: `pffft_priv_impl.h` (included by `pffft.c`) vendors its own fftpack-derived twiddle-factor generation (`rffti1_ps`/`cffti1_ps`); upstream's `fftpack.c` is only used by pffft's own benchmarks/tests, which this project does not build. `pffft_double.c` and the `pf_carrier`/`pf_cic`/`pf_mixer`/`pffastconv` extras are also excluded — out of scope for a single-precision real-FFT spectrum analyser. |
| SIMD | SSE is enabled automatically: `simd/pf_sse1_float.h` sets `SIMD_SZ=4` whenever `_M_X64` is defined, which MSVC always defines for an x64 target, so no extra compiler flags are needed. This fork has **no AVX implementation for single precision** (only for double, via `pf_avx_double.h`, which `investigate_pffft` does not compile), so there is no further AVX opt-in available — the roadmap's "SSE, and AVX if the fork supports it" instruction resolves to SSE-only for this fork. |
| Compile definitions | `PFFFT_STATIC_DEFINE` (`PUBLIC`) — `pffft.h`'s `PFFFT_EXPORT` macro defaults to `__declspec(dllimport)` on Windows unless told the library is static, which fails to compile `investigate_pffft`'s own `.c` files. |
| Warnings | `investigate_pffft` builds with `/w` (all MSVC warnings off) — third-party code only; `Investigate` and `InvestigateUnitTests` are never touched, so the project stays warning-clean at `/W4`. |
| How to update | Bump `INVESTIGATE_PFFFT_TAG` in the root `CMakeLists.txt`, delete `third_party/pffft-src`/`pffft-build`/`pffft-subbuild` (or run a clean configure), rebuild, and re-run the FFT correctness/benchmark tests (T-004). |

## libebur128

| | |
|---|---|
| Version | `v1.2.6` (latest release tag) |
| Source | https://github.com/jiixyj/libebur128.git, `GIT_TAG v1.2.6`, `GIT_SHALLOW TRUE` |
| License | MIT. Fully permissive — no risk even if linked into the product — but the intended use here is narrower (see Usage). |
| Usage | **Test-only.** A BS.1770/EBU R128 loudness oracle used to cross-check `LoudnessMeter` (T-006) in `InvestigateUnitTests`. **Never linked into the plugin.** The root `CMakeLists.txt` has a configure-time check, after `tests/` is configured, that inspects `Investigate`'s `LINK_LIBRARIES` and `INTERFACE_LINK_LIBRARIES` properties for `investigate_libebur128` and fails the configure step (`message(FATAL_ERROR ...)`) if it is ever found there. This check was verified by temporarily linking `investigate_libebur128` into `Investigate` and confirming the configure step failed with the expected error, then reverting (T-021 log). |
| Integration | `FetchContent`, declared in `tests/CMakeLists.txt` (guarded by `PLUGIN_BUILD_TESTS`, which already wraps the whole `tests/` subdirectory) using the same `SOURCE_SUBDIR` trick as pffft, so upstream's own `CMakeLists.txt` (shared-library-by-default, install rules, `queue.h` `try_compile` probe) is never configured. A hand-written static library target `investigate_libebur128` compiles only `ebur128/ebur128.c`. It is linked `PRIVATE` into `InvestigateUnitTests` only. |
| Windows-specific include | `ebur128.c` unconditionally includes `<sys/queue.h>`, which MSVC's CRT does not provide. `investigate_libebur128` adds `ebur128/queue` (libebur128's own BSD-derived compat header) as a `PRIVATE` include directory, matching upstream's `ENABLE_INTERNAL_QUEUE_H` path for non-BSD platforms. |
| Compile definitions | `_USE_MATH_DEFINES` (`PRIVATE`) — matches upstream's own MSVC handling in `ebur128/CMakeLists.txt`. |
| Warnings | `investigate_libebur128` builds with `/w`, same rationale as pffft above. |
| How to update | Bump `INVESTIGATE_LIBEBUR128_TAG` in `tests/CMakeLists.txt`, delete `third_party/libebur128-src`/`libebur128-build`/`libebur128-subbuild` (or run a clean configure), rebuild, and re-run the loudness conformance tests (T-006). |

## EBU loudness test set (official conformance files) — no automated fetch script

`docs/research/T-002-dsp-references.md` section 3 recommends downloading the EBU's official Tech 3341/3342 test-set files (v5.0, 87.4 MB, at
https://tech.ebu.ch/files/live/sites/tech/files/shared/testmaterial/ebu-loudness-test-setv05.zip) for the true-peak and LRA unit-test cases that need bit-exact reference audio, and using them only as local, gitignored fixtures.

**Terms of use, read at https://tech.ebu.ch/files/live/sites/tech/files/shared/testmaterial/use%20of%20EBU%20AUDIO%20test%20sequences.pdf (EBU Technology and Innovation Department, v1.0, July 2019):**
- Scope: "EBU Test Sequences are made available for the sole purposes to assess the performance of audio equipment and systems within the frame of internal Research and Development operations."
- "By downloading and using the EBU Test Sequences, you hereby accept that" [the terms below apply] — an accept-by-downloading clause.
- No business, commercial or for-profit use. No copying, modifying, merging, publishing, distributing, sublicensing or selling of the files. All rights stay with the EBU.
- Provided "as is", no warranty; the EBU may revoke the authorization at any time without notice.
- Permitted: quoting that the files were used in test reports/articles, with a "© EBU" credit.

**Decision: no `cmake/FetchEbuTestSet.cmake` script was written.** The "by downloading... you hereby accept" wording is a clickwrap-style agreement tied to a human's affirmative act, not a machine's; an unattended `cmake -P` script (or a CI job) run repeatedly on every clean checkout cannot meaningfully accept those terms on a person's behalf the way a developer reading and clicking through them can. This matches the T-021 task instruction: "If the terms require the user's explicit acceptance... do not write the script." The download itself is a plain HTTPS URL with no login/paywall gate, so this is a judgement call about the acceptance clause specifically, not a technical restriction — **open question for the user**, below.

What was still done, since it costs nothing and needs no acceptance of the EBU terms:
- `INVESTIGATE_EBU_TEST_SET_DIR` — a CMake cache `PATH` variable in the root `CMakeLists.txt`, defaulting to `third_party/_fixtures/ebu` (already covered by the blanket `third_party/` entry in `.gitignore`) — is passed to `InvestigateUnitTests` as the compile definition `INVESTIGATE_EBU_TEST_SET_DIR`, so future true-peak/LRA tests (T-006) can check whether the folder exists and skip cleanly if it does not.
- A developer who reads and accepts the terms of use above may manually download and unpack the zip into that folder (or set `-DINVESTIGATE_EBU_TEST_SET_DIR=<path>` to point elsewhere).

**Open question for the user:** does downloading the EBU test set as part of this project's own (non-commercial, internal) DSP conformance testing during development count as "internal Research and Development operations" under the EBU's terms, and are you comfortable with a human (not a script) doing that download once, locally, per the terms above? If yes, a developer can populate `INVESTIGATE_EBU_TEST_SET_DIR` manually; the true-peak/LRA test cases in T-006 should otherwise rely on the synthesizable Tech 3341 cases only (see `docs/research/T-002-dsp-references.md` section 3's table).

## Fonts and their license texts

Approved by `docs/research/T-022-font-licenses.md` and the roadmap decision dated 2026-09-28 (both under **SIL Open Font License 1.1**, no Reserved Font Name declared by either project, embedding and subsetting in a closed-source, freely distributed binary explicitly permitted). Downloaded with SHA-256 checksum verification by `cmake/DownloadFonts.cmake` (same pattern as `cmake/DownloadPluginval.cmake`) into `third_party/fonts/`, and compiled into the existing `InvestigateAssets` binary-data target alongside the SVGs in `assets/ui/` — one target, because a second `juce_add_binary_data` target would default to the same `BinaryData` namespace and clash. Fonts are **not** added to `assets/ui/` (owned by ui-ux-designer); this directory is build-engineer's, per the roadmap decision.

| Asset | Version | Copyright | License | Source zip | SHA-256 (zip) | BinaryData symbol |
|---|---|---|---|---|---|---|
| Inter-Medium.ttf | v4.1 | Copyright (c) 2016 The Inter Project Authors (https://github.com/rsms/inter) | SIL OFL 1.1 | https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip | `9883fdd4a49d4fb66bd8177ba6625ef9a64aa45899767dde3d36aa425756b11e` | `BinaryData::InterMedium_ttf` |
| Inter-SemiBold.ttf | v4.1 | Copyright (c) 2016 The Inter Project Authors (https://github.com/rsms/inter) | SIL OFL 1.1 | (same zip as above) | (same zip hash as above) | `BinaryData::InterSemiBold_ttf` |
| OFL-Inter.txt (Inter's `LICENSE.txt`) | v4.1 | (as above) | SIL OFL 1.1 | (same zip as above) | (same zip hash as above) | `BinaryData::OFLInter_txt` |
| JetBrainsMono-Regular.ttf | v2.304 | Copyright 2020 The JetBrains Mono Project Authors (https://github.com/JetBrains/JetBrainsMono) | SIL OFL 1.1 | https://github.com/JetBrains/JetBrainsMono/releases/download/v2.304/JetBrainsMono-2.304.zip | `6f6376c6ed2960ea8a963cd7387ec9d76e3f629125bc33d1fdcd7eb7012f7bbf` | `BinaryData::JetBrainsMonoRegular_ttf` |
| JetBrainsMono-Medium.ttf | v2.304 | (as above) | SIL OFL 1.1 | (same zip as above) | (same zip hash as above) | `BinaryData::JetBrainsMonoMedium_ttf` |
| OFL-JetBrainsMono.txt (JetBrains Mono's `OFL.txt`) | v2.304 | (as above) | SIL OFL 1.1 | (same zip as above) | (same zip hash as above) | `BinaryData::OFLJetBrainsMono_txt` |
| LICENSE-pffft.txt | pinned to `INVESTIGATE_PFFFT_TAG` (`v1.1.0`) | See the pffft section above | BSD-like | Copied from the already `GIT_TAG`-pinned `third_party/pffft-src/LICENSE.txt` at configure time (no separate download/checksum: git already pins the exact commit) | n/a (copied, not downloaded) | `BinaryData::LICENSEpffft_txt` |

The four TTF files extracted by `cmake/DownloadFonts.cmake` are byte-identical to the files inside the release zips above (verified: `sha256sum` of each extracted file matched a manual extraction of the same zip during T-021 — see the task log for the individual per-file hashes).

**Distribution obligations (T-019, installer, and any About/credits screen):** ship the OFL 1.1 text and the copyright notices above for both fonts (satisfied by embedding `OFL-Inter.txt`/`OFL-JetBrainsMono.txt` as binary data and surfacing them in an About/credits view); never sell the font files standalone; do not claim the fonts are unmodified originals if further subsetting changes the `name` table. Ship pffft's `LICENSE-pffft.txt` alongside the other third-party notices (T-023's third-party-notices inventory should include it).

## Unit test target and JUCE double-compilation

`InvestigateUnitTests` links the `Investigate` target that `juce_add_plugin`
creates — this is JUCE's shared-code target, compiled once and reused by both
`Investigate_VST3` and `Investigate_Standalone`. Linking it into the test
executable reuses that same compiled object code and JUCE configuration
instead of recompiling JUCE modules a second time with different settings,
which is the failure mode the task warns about ("without compiling JUCE twice
in conflicting ways"). `source/` is exposed as a `PUBLIC` include directory on
`Investigate` for the same reason: so `#include "PluginProcessor.h"` (and
future `source/dsp/*.h`, `source/params/*.h` includes) resolve for test code
without a duplicated include path definition.

## Compile definitions

In addition to the ones listed for JUCE's target defaults in `CLAUDE.md`
(`JUCE_VST3_CAN_REPLACE_VST2=0`, `JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`), the
`Investigate` target sets, in `target_compile_definitions(Investigate PUBLIC ...)`:

| Definition | Value | Reason |
|---|---|---|
| `JUCE_STRICT_REFCOUNTEDPOINTER` | `1` | Catches unsafe implicit conversions of `ReferenceCountedObjectPtr`; set from T-003. |
| `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP` | `1` | Activates plugin-engineer's `source/StandaloneApp.cpp` in place of JUCE's stock Standalone app, so the Standalone starts with "Mute audio input" off (roadmap risk #7, T-002 section 9). Requested by T-008 open question 1, added in T-025. **As of T-025, this definition alone does not make the Standalone link** — see the T-025 task log/open questions: JUCE's stock `juce_audio_plugin_client_Standalone.cpp` still defines the `WinMain`/`main` entry point (`JUCE_MAIN_FUNCTION_DEFINITION`) unless `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_ENTRYPOINT` is *also* set to 1, and `source/StandaloneApp.cpp` unconditionally defines its own `JUCE_MAIN_FUNCTION_DEFINITION` too, so both translation units currently define `WinMain` and the Standalone target fails to link with `LNK2005`/`LNK1169`. `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_ENTRYPOINT` is not yet set — resolving this is plugin-engineer's and/or the architect's call (source file ownership), not build-engineer's alone; see the T-025 log. |
| `JUCE_USE_FLAC` | `0` | Investigate never reads or writes audio files; excludes the bundled FLAC codec's source from compilation. Removes its BSD notice obligation and reduces binary size. Recommended by `docs/research/T-023-third-party-notices.md` section 3, added in T-025 (moved from T-014). |
| `JUCE_USE_OGGVORBIS` | `0` | Same rationale as `JUCE_USE_FLAC`, for the bundled Ogg Vorbis codec. |
| `JUCE_USE_WINDOWS_MEDIA_FORMAT` | `0` | Excludes JUCE's Windows Media Format reader/writer wrapper (calls OS WMF COM APIs; no bundled third-party source). Unused code and attack surface for a plugin that reads no audio files. |

**Verified effect (T-025, clean Release configure/build, same source tree, only the
`CMakeLists.txt` edit changed between the two builds):**
- VST3 binary (`Investigate.vst3/Contents/x86_64-win/Investigate.vst3`):
  6,903,808 bytes before → 5,954,048 bytes after (−949,760 bytes, ≈13.8% smaller).
  `strings` on the binary shows FLAC (`FLAC__*`, `libFLAC`) and Ogg Vorbis
  (`vorbis_*`) symbols present before and completely absent after.
- Standalone (`Investigate_Standalone`) currently **fails to link** once
  `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1` is set, for the reason described in
  the table row above. See the T-025 task log for the exact linker errors and
  the open question raised to plugin-engineer/the architect.

## Build, test and validate

```
cmake --preset windows-msvc
cmake --build --preset debug
cmake --build --preset release
ctest --preset release                 # unit tests (Catch2)
ctest --preset release -L validation   # pluginval at strictness 10
```

The first configure fetches JUCE, Catch2, pffft and (when `PLUGIN_BUILD_TESTS`
is on) libebur128, and downloads pluginval and the Inter/JetBrains Mono font
zips (network required for all of these); subsequent configures reuse
`third_party/`. `third_party/` and `build/` are gitignored — nothing here is
committed.
