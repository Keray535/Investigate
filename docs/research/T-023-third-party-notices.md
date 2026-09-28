# T-023: Third-party notices inventory for the shipped binary

## Recommended compile definitions (put at top per task instructions)

Add to `target_compile_definitions(Investigate PUBLIC ...)` in `CMakeLists.txt` (build-engineer to apply; researcher does not edit CMake):

```
JUCE_USE_FLAC=0
JUCE_USE_OGGVORBIS=0
JUCE_USE_WINDOWS_MEDIA_FORMAT=0
```

- `JUCE_USE_MP3AUDIOFORMAT` and `JUCE_USE_LAME_AUDIO_FORMAT` are **already 0 by default** in JUCE 8.0.15 (confirmed in `modules/juce_audio_formats/juce_audio_formats.h`), so nothing to add for those — MP3/LAME code is not compiled in and carries no notice obligation already.
- All four macro names above and `JUCE_USE_MP3AUDIOFORMAT` / `JUCE_USE_LAME_AUDIO_FORMAT` exist verbatim in `third_party/juce-src/modules/juce_audio_formats/juce_audio_formats.h` (lines ~69–119) at 8.0.15. Setting `JUCE_USE_FLAC`, `JUCE_USE_OGGVORBIS` and `JUCE_USE_WINDOWS_MEDIA_FORMAT` to 0 excludes their source files from compilation via `#if` guards around the codec `.cpp`/wrapper includes, so this removes the code — and its notice obligation — from the binary, not just runtime behaviour. Investigate reads no audio files at all (it is an analyzer, not a file player/recorder), so none of these codecs are needed; leaving them at their default `1` would compile in FLAC and Ogg Vorbis for no functional benefit.
- Investigate does load PNG/SVG UI assets and TrueType fonts, so `juce_graphics`' zlib, libpng, jpeglib (jpeglib is compiled in by `juce_graphics` regardless of whether the app decodes JPEGs — there is no `JUCE_USE_JPEG` off-switch in this JUCE version), HarfBuzz and SheenBidi stay compiled in; there is no clean way to strip them without patching JUCE, and none of these carry restrictive terms.

## Surprising finding

**`docs/dependencies.md` currently states JUCE 8 is "GPLv3 (open source)."** The actual `third_party/juce-src/LICENSE.md` at the fetched 8.0.15 tag says the open-source tier is **AGPLv3**, not GPLv3 (dual-licensed with the commercial JUCE 8 EULA). This is a materially different (stricter) copyleft license: AGPLv3 additionally triggers on network use ("Remote Network Interaction," §13), though that clause is irrelevant to a local audio plugin with no network service. `docs/tasks/T-021-dsp-dependencies.md` (item 5, in progress under a parallel task) already instructs build-engineer to correct this row citing https://juce.com/legal/juce-8-licence/, so no CLAUDE.md-owner conflict — this report flags it for cross-check once T-021 lands, and restates the corrected fact throughout below.

## Question
Inventory every third-party component compiled into or shipped with Investigate's VST3 and Standalone binaries (JUCE 8.0.15 + bundled VST3 SDK + bundled module libraries + pffft + fonts), each with license and notice obligations; answer the Steinberg trademark question; recommend codec compile-definitions; draft notice text.

## Answer and recommendation
Every component in the shipped binary is either permissive (MIT/BSD/zlib/Apache/ISC) or dual AGPLv3-or-commercial (JUCE itself, whose EULA choice is a business decision already flagged as Risk 9 in the roadmap and unrelated to this task). No strong-copyleft *bundled library* is compiled in by default on Windows once the recommended compile definitions are applied (FLAC/OggVorbis/WMF off) — those three were previously compiled in and unused, and were BSD-licensed anyway so they were not a legal blocker, just unnecessary notices and binary size. The VST3 SDK bundled by JUCE 8.0.15 is version **3.8.0**, MIT-licensed (Steinberg Media Technologies GmbH, 2025 copyright header) — confirmed from `LICENSE.txt` and `pluginterfaces/base/... kVstVersionString "VST 3.8.0"` in the fetched tree, so no "VST3 SDK is GPL unless ≥3.8" concern applies. A Steinberg trademark attribution line is required/strongly recommended even without displaying the VST logo, per Steinberg's own usage guidelines, and should be added to both `THIRD-PARTY-NOTICES.txt` and the About view.

## Components compiled into the shipped binary

| Component | Version | License | Compiled in by default with current CMakeLists.txt? | Notice obligation | Source path / URL |
|---|---|---|---|---|---|
| JUCE Framework (core modules) | 8.0.15 | Dual: **AGPLv3** or commercial JUCE 8 EULA (open-source tier is AGPLv3, not GPLv3 — see Surprising finding above) | Yes (`juce_audio_utils`, `juce_dsp` + transitive core/graphics/events/data_structures modules) | If shipping under the AGPLv3 tier: the whole combined work must be distributable under AGPLv3-compatible terms, including source availability on request/network use. **Decision deferred to M5 per roadmap Risk 9** — this line's final text depends on which tier is chosen. | `third_party/juce-src/LICENSE.md`; https://juce.com/legal/juce-8-licence/ |
| VST3 SDK (bundled by JUCE) | **3.8.0** | **MIT** | Yes (VST3 target only; headers/pluginterfaces linked into `Investigate_VST3`) | Reproduce MIT copyright/permission notice; add Steinberg trademark attribution (see below) | `third_party/juce-src/modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt`; SDK version string in `.../VST3_SDK/pluginterfaces/vst/vsttypes.h` (`kVstVersionString "VST 3.8.0"`) |
| zlib | 1.2.3 (as vendored by JUCE) | **zlib License** (permissive) | Yes (`juce_core` zip support) | Reproduce zlib license text; no source-disclosure requirement | `third_party/juce-src/modules/juce_core/zip/zlib/README` |
| libpng (pnglib) | bundled, PNG Reference Library License v2 | **PNG Reference Library License v2** (permissive, BSD-like) | Yes (`juce_graphics` PNG decode/encode, used for UI assets) | Reproduce copyright/disclaimer notice | `third_party/juce-src/modules/juce_graphics/image_formats/pnglib/LICENSE` |
| jpeglib (IJG) | release 9f (14-Jan-2024) | **Independent JPEG Group License** (permissive, BSD-like; "free for any purpose" subject to notice) | Yes (`juce_graphics`; no off-switch found in this JUCE version, even though Investigate doesn't load JPEGs) | Reproduce IJG attribution ("This software is based in part on the work of the Independent JPEG Group") | `third_party/juce-src/modules/juce_graphics/image_formats/jpglib/README` |
| HarfBuzz | bundled ("Old MIT" per JUCE's own LICENSE.md) | **MIT** (old/HTML-style MIT variant) | Yes (`juce_graphics` text shaping, needed for the UI's custom fonts) | Reproduce MIT-style notice | `third_party/juce-src/modules/juce_graphics/fonts/harfbuzz/COPYING` |
| SheenBidi | bundled | **Apache-2.0** | Yes (`juce_graphics`, bidi text support) | Reproduce Apache-2.0 NOTICE if one exists, plus license text; state any modifications | `third_party/juce-src/modules/juce_graphics/unicode/sheenbidi/LICENSE` |
| FLAC | bundled | **BSD-style ("Flac Licence")** | **Recommend disabling** (`JUCE_USE_FLAC=0`) — not needed, Investigate does not read/write audio files | If left enabled: reproduce BSD notice. If disabled per recommendation: **no notice needed**, code is excluded from compilation | `third_party/juce-src/modules/juce_audio_formats/codecs/flac/Flac Licence.txt` |
| Ogg Vorbis | bundled | **BSD-style ("Ogg Vorbis Licence")**, applies to the header files JUCE includes | **Recommend disabling** (`JUCE_USE_OGGVORBIS=0`) | Same as FLAC: no notice needed once disabled | `third_party/juce-src/modules/juce_audio_formats/codecs/oggvorbis/Ogg Vorbis Licence.txt` |
| Windows Media Format (WMF) reader/writer | JUCE wrapper over the OS-provided WMF COM APIs | No bundled third-party source (thin JUCE wrapper calling Windows system DLLs) — not a redistributed third-party *license*, but still unused code/attack surface | **Recommend disabling** (`JUCE_USE_WINDOWS_MEDIA_FORMAT=0`) since Investigate doesn't read WMA/ASF files | N/A (no bundled source, no notice) | `third_party/juce-src/modules/juce_audio_formats/juce_audio_formats.h` lines ~110–119 |
| MP3 / LAME audio format | bundled decode-only MP3 code / optional LAME encoder wrapper | JUCE's own MP3 code carries a **use-at-your-own-risk patent disclaimer** (not a separate third-party license, since JUCE wrote the decoder) | **Already off by default** (`JUCE_USE_MP3AUDIOFORMAT=0`, `JUCE_USE_LAME_AUDIO_FORMAT=0`) — no action needed, already excluded | N/A while disabled | `third_party/juce-src/modules/juce_audio_formats/juce_audio_formats.h` lines ~87–107 |
| AudioUnitSDK, Oboe, GLEW/Mesa/Khronos, CHOC/QuickJS, LV2, pslextensions, AAX SDK, Box2D, ASIO | bundled in other JUCE modules | Apache-2.0 / Apache-2.0 / BSD+MIT+MIT / ISC+MIT / ISC / Public domain / Proprietary-Avid-or-GPLv3 / zlib / Proprietary-Steinberg-or-GPLv3 respectively | **Not compiled in** — Investigate links only `juce_audio_utils` + `juce_dsp` (Windows VST3/Standalone; no AU, AAX, LV2, WebBrowser (`JUCE_WEB_BROWSER=0`), OpenGL, Box2D, or ASIO device backend requested) | None — excluded from the linked target entirely, so listed here only for completeness/audit trail, not because they need a notice | `third_party/juce-src/LICENSE.md` "JUCE Framework Dependencies" list |
| pffft (marton78 fork) | commit/tag **not yet pinned** — T-021 (build-engineer, in progress in parallel) has not recorded a pin in its log yet; a working copy exists read-only at `third_party/_inspect_pffft/` (untagged inspection checkout, `master` branch content at time of this research) | **BSD-style, 3-clause-like** (four stacked copyright holders: Mambro 2020, Ayguen 2019, Pommier 2013, UCAR 2004; no-endorsement + notice-reproduction conditions, standard disclaimer) | Will be, once T-021 lands (`SpectrumAnalyser`'s FFT backend) — not yet in the tree as a build target as of this report | Reproduce the full license text verbatim (it has 4 copyright lines and a no-endorsement clause, not just a 2-line BSD) | `third_party/_inspect_pffft/` (uncommitted inspection copy); canonical: https://github.com/marton78/pffft/blob/master/LICENSE.txt — **T-021 must record the exact pinned commit/tag in its log; re-verify this license file at that exact pin before shipping, since it is not yet pinned** |
| Inter (font) | v4.1 | **SIL OFL 1.1** | Will be, once T-021 lands (`InterMedium_ttf`, `InterSemiBold_ttf` via `BinaryData`) | Ship OFL 1.1 text + copyright notice (see T-022, already resolved) | https://github.com/rsms/inter/releases/tag/v4.1; `docs/research/T-022-font-licenses.md` |
| JetBrains Mono (font) | v2.304 | **SIL OFL 1.1** | Will be, once T-021 lands (`JetBrainsMonoRegular_ttf`, `JetBrainsMonoMedium_ttf`) | Ship OFL 1.1 text + copyright notice (see T-022, already resolved) | https://github.com/JetBrains/JetBrainsMono/releases/tag/v2.304; `docs/research/T-022-font-licenses.md` |

### Excluded (not in the shipped binary)

| Component | Why excluded |
|---|---|
| **libebur128** | MIT-licensed loudness library, but T-021 scopes it as **test-only**, linked solely into `InvestigateUnitTests`; T-021's acceptance criteria require a CMake check proving the plugin target never links it. Excluded from `THIRD-PARTY-NOTICES.txt` (which ships with the plugin binary) — list it instead in `docs/qa/` or a developer-facing test-dependency note if one exists, not in the shipped notices file. |
| **Catch2** | Test-only (BSL-1.0), never linked into the plugin per `docs/dependencies.md`. |
| **pluginval** | Tool-only, GPLv3, runs out-of-process against the built binary; per `docs/dependencies.md` this does not affect Investigate's own license. |
| **EBU loudness test set audio** (if fetched by T-021) | Fixture data for tests only, not shipped. |
| **AAX SDK, ASIO SDK** | Bundled in JUCE's tree but neither format nor the ASIO device backend is used by this project (Windows VST3/Standalone only); not compiled into the linked target, so their Proprietary/GPLv3 dual terms are moot for Investigate. |

## Steinberg trademark

**Required/strongly recommended even without the VST Compatible Logo.** Per Steinberg's VST 3 Developer Portal usage guidelines and the bundled `VST3_Usage_Guidelines.pdf`, a product using the "VST" name in documentation, product materials or an About screen — even without displaying the VST Compatible Logo — must:
1. Mark the first use of "VST" in any product-related material with the registered-trademark symbol (VST®).
2. Include the attribution line **"VST is a registered trademark of Steinberg Media Technologies GmbH."** in product credits/documentation.

Source: https://steinbergmedia.github.io/vst3_dev_portal/pages/VST+3+Licensing/Usage+guidelines.html (fetched 2026-09-28); the guideline is echoed in `third_party/juce-src/modules/juce_audio_processors_headless/format_types/VST3_SDK/VST3_Usage_Guidelines.pdf` shipped with the SDK (title page could not be rendered in this sandbox — no `pdftoppm`/poppler available — but the same guideline is publicly mirrored at the developer-portal URL above and cross-checked by an independent secondary source referencing the identical PDF, https://biptunia.com/wp-content/uploads/2019/08/VST3_Usage_Guidelines.pdf).

Recommendation: add the attribution line to both `THIRD-PARTY-NOTICES.txt` and the About view's "Software" section (draft text below), and mark the first "VST" mention in any marketing copy or the plugin's own README with the ® symbol.

## Codecs — compile-definition detail

| Macro | Confirmed to exist in 8.0.15 at | Default | Recommended value | Effect of recommended value |
|---|---|---|---|---|
| `JUCE_USE_FLAC` | `modules/juce_audio_formats/juce_audio_formats.h:74` | 1 | **0** | Excludes FLAC codec `.cpp` sources from compilation (guarded by `#if JUCE_USE_FLAC`); removes the BSD notice obligation entirely |
| `JUCE_USE_OGGVORBIS` | same file, line 83 | 1 | **0** | Excludes Ogg Vorbis codec sources; removes its BSD notice obligation |
| `JUCE_USE_MP3AUDIOFORMAT` | same file, line 99 | 0 | 0 (no change needed) | Already excluded |
| `JUCE_USE_LAME_AUDIO_FORMAT` | same file, line 106 | 0 | 0 (no change needed) | Already excluded; this only wraps an external LAME DLL path anyway, no bundled LAME source in the JUCE tree |
| `JUCE_USE_WINDOWS_MEDIA_FORMAT` | same file, lines 113–119 (Windows-only; forced to 0 outside Windows) | 1 on Windows | **0** | Excludes the WMF reader/writer wrapper; no bundled third-party source either way, purely reduces attack surface and an unused OS-integration path |

All three off-switches remove the corresponding `#include`d source files from the compiled translation units (verified by inspecting the `#if`-guarded include blocks in `juce_audio_formats.h`/`.cpp`), not just a runtime feature flag — so disabling them genuinely drops the code, and its associated notice requirement, from the binary.

## Draft notice texts

### `THIRD-PARTY-NOTICES.txt` (full)

```
Investigate — Third-Party Notices
==================================

This product includes the following third-party software.

--------------------------------------------------------------------
JUCE Framework 8.0.15
Copyright (c) Raw Material Software Limited
Licensed under [AGPLv3 / the commercial JUCE 8 licence — FINAL CHOICE
PENDING THE M5 LICENSE DECISION; this line must be updated once that
decision is made, per the roadmap Risk 9 entry].
https://juce.com/legal/juce-8-licence/

--------------------------------------------------------------------
VST3 SDK 3.8.0
Copyright (c) 2025, Steinberg Media Technologies GmbH

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

VST is a registered trademark of Steinberg Media Technologies GmbH.

--------------------------------------------------------------------
zlib
Copyright notice and full license text as distributed with zlib
(zlib License — permissive, no source-disclosure obligation).
See: third_party/juce-src/modules/juce_core/zip/zlib/README

--------------------------------------------------------------------
libpng (PNG Reference Library)
Copyright (c) 1995-2019 The PNG Reference Library Authors.
Copyright (c) 2018-2019 Cosmin Truta.
Copyright (c) 2000-2002, 2004, 2006-2018 Glenn Randers-Pehrson.
Copyright (c) 1996-1997 Andreas Dilger.
Copyright (c) 1995-1996 Guy Eric Schalnat, Group 42, Inc.
Licensed under the PNG Reference Library License version 2 (permissive).

--------------------------------------------------------------------
Independent JPEG Group's JPEG software (jpeglib), release 9f
This software is based in part on the work of the Independent JPEG
Group. Free for any purpose, subject to the IJG's usage conditions;
see third_party/juce-src/modules/juce_graphics/image_formats/jpglib/README.

--------------------------------------------------------------------
HarfBuzz
Licensed under the (old-style) MIT License.
See: third_party/juce-src/modules/juce_graphics/fonts/harfbuzz/COPYING

--------------------------------------------------------------------
SheenBidi
Licensed under the Apache License, Version 2.0.
See: third_party/juce-src/modules/juce_graphics/unicode/sheenbidi/LICENSE

--------------------------------------------------------------------
pffft
Copyright (c) 2020 Dario Mambro (dario.mambro@gmail.com)
Copyright (c) 2019 Hayati Ayguen (h_ayguen@web.de)
Copyright (c) 2013 Julien Pommier (pommier@modartt.com)
Copyright (c) 2004 the University Corporation for Atmospheric Research
("UCAR"). All rights reserved. Developed by NCAR's Computational and
Information Systems Laboratory, UCAR, www.cisl.ucar.edu.

Redistribution and use of the Software in source and binary forms,
with or without modification, is permitted provided that the
following conditions are met:

- Neither the names of NCAR's Computational and Information Systems
Laboratory, the University Corporation for Atmospheric Research, nor
the names of its sponsors or contributors may be used to endorse or
promote products derived from this Software without specific prior
written permission.

- Redistributions of source code must retain the above copyright
notices, this list of conditions, and the disclaimer below.

- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions, and the disclaimer below in the
documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE CONTRIBUTORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH THE
SOFTWARE.

[NOTE: pin the exact pffft commit/tag once T-021 records it, and
re-verify this license text matches that exact pin.]

--------------------------------------------------------------------
Inter (font), version 4.1
Copyright (c) 2016 The Inter Project Authors
(https://github.com/rsms/inter)
Licensed under the SIL Open Font License, Version 1.1.
Full license text: see OFL-Inter.txt shipped alongside this file.
http://scripts.sil.org/OFL

--------------------------------------------------------------------
JetBrains Mono (font), version 2.304
Copyright 2020 The JetBrains Mono Project Authors
(https://github.com/JetBrains/JetBrainsMono)
Licensed under the SIL Open Font License, Version 1.1.
Full license text: see OFL-JetBrainsMono.txt shipped alongside this file.
http://scripts.sil.org/OFL
```

### About view "Software" section (short form, per ui-spec §6.2)

```
Investigate is built with the JUCE Framework (JUCE 8, [AGPLv3 /
commercial licence — pending]), Steinberg's VST 3 SDK (MIT), and
pffft (BSD). UI text uses the Inter and JetBrains Mono typefaces
(SIL Open Font License 1.1). VST is a registered trademark of
Steinberg Media Technologies GmbH. Full license texts are included
in THIRD-PARTY-NOTICES.txt.
```

## Licenses and risks
- **JUCE (AGPLv3-or-commercial)**: the AGPLv3 line above is a **placeholder pending the M5 license-tier decision** (roadmap Risk 9); do not finalize `THIRD-PARTY-NOTICES.txt` or ship a build before that decision is recorded. This report corrects `docs/dependencies.md`'s current "GPLv3" wording to AGPLv3, matching what T-021 is already set to fix.
- **VST3 SDK (MIT)**: permissive, no copyleft; Steinberg trademark attribution is a separate, non-license obligation (see above), not a license condition of the MIT grant itself.
- **zlib, libpng, HarfBuzz (old MIT), pffft, FLAC/Ogg (if left on)**: all permissive/BSD-style; safe for closed-source distribution, notice-reproduction only.
- **SheenBidi (Apache-2.0)**: permissive; if SheenBidi ships a NOTICE file with attribution content beyond the LICENSE, that content must also be reproduced (Apache-2.0 §4(d)) — verify the exact contents of `.../sheenbidi/LICENSE` for an embedded NOTICE section before finalizing (this report read the license's existence and type but did not need to reproduce Apache's own boilerplate here since no separate NOTICE file was found alongside it in the fetched tree).
- **jpeglib (IJG license)**: permissive but has an unusual attribution wording requirement ("based in part on the work of the Independent JPEG Group") — use the exact phrase, not a paraphrase.
- **AAX SDK / ASIO SDK dual Proprietary-or-GPLv3 terms**: irrelevant here since neither is compiled into Investigate's linked target (Windows VST3/Standalone only, no ASIO device backend or AAX format requested) — flagged only so a future roadmap change to add AAX or an ASIO-backed Standalone doesn't silently reintroduce a GPLv3 obligation without review.
- **pffft pin outstanding**: T-021 (parallel, in progress) has not yet recorded pffft's exact pinned commit in its log. The license quoted above is from the current `master` HEAD and from the read-only inspection copy already present at `third_party/_inspect_pffft/`; re-check once T-021's log records the pin, in case a future commit changes the license file.
- **No AGPL "network use" exposure in practice**: Investigate is a local audio plugin with no server component, so AGPLv3 §13 has no practical trigger regardless of which JUCE tier is chosen — this is a note for the M5 decision-maker, not a blocker.

## Sources
- `third_party/juce-src/LICENSE.md` (fetched JUCE 8.0.15 tree)
- `third_party/juce-src/modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt`
- `third_party/juce-src/modules/juce_audio_processors_headless/format_types/VST3_SDK/pluginterfaces/vst/vsttypes.h`
- `third_party/juce-src/modules/juce_audio_formats/juce_audio_formats.h`
- `third_party/juce-src/modules/juce_core/zip/zlib/README`
- `third_party/juce-src/modules/juce_graphics/image_formats/pnglib/LICENSE`
- `third_party/juce-src/modules/juce_graphics/image_formats/jpglib/README`
- `third_party/juce-src/modules/juce_audio_formats/codecs/flac/Flac Licence.txt`
- `third_party/juce-src/modules/juce_audio_formats/codecs/oggvorbis/Ogg Vorbis Licence.txt`
- https://juce.com/legal/juce-8-licence/
- https://steinbergmedia.github.io/vst3_dev_portal/pages/VST+3+Licensing/Usage+guidelines.html
- https://github.com/marton78/pffft/blob/master/LICENSE.txt (via `gh api repos/marton78/pffft/contents/LICENSE.txt`)
- `docs/research/T-022-font-licenses.md`
- `docs/tasks/T-021-dsp-dependencies.md`
- `docs/dependencies.md`, `CMakeLists.txt`
