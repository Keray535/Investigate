# T-023: Third-party notices inventory for the shipped binary
- Owner: researcher
- Milestone: M3
- Status: DONE
- Depends on: T-003, T-022

## Goal
List every third-party component that is compiled into, or shipped with, the Investigate VST3 and Standalone binaries, together with the notice each license requires. The About view (T-011) and `THIRD-PARTY-NOTICES.txt` (T-019) must be complete, and the build should switch off bundled components that Investigate does not need, so there are fewer notices to carry.

## Inputs
- `docs/dependencies.md` (JUCE 8.0.15, Catch2, pluginval, and pffft, libebur128 and the fonts once T-021 adds them).
- The fetched JUCE 8.0.15 sources in `third_party/` (read only). Look at `LICENSE.md`, `modules/juce_audio_processors/format_types/VST3_SDK/` (or the equivalent path in 8.0.15) and its license files, and the third-party code bundled in JUCE modules, such as zlib in `juce_core`, libpng and libjpeg in `juce_graphics`, and FLAC and Ogg Vorbis in `juce_audio_formats`.
- `CMakeLists.txt`, for the JUCE modules and compile definitions in use (`juce_audio_utils`, `juce_dsp`, and their dependencies).
- `docs/research/T-002-dsp-references.md` section 10, and `docs/research/T-022-font-licenses.md`.
- `docs/design/ui-spec.md` section 6.2 (the About view's "Software" notices).

## Deliverable
Write `docs/research/T-023-third-party-notices.md` with:
1. **Components.** A table of every third-party component in the shipped binary: component, version, license, whether it is compiled in by default with Investigate's current modules, the notice text requirement, and a source path or URL. Include:
   - JUCE 8.0.15 itself, with the open-source option confirmed from `LICENSE.md` at that tag (AGPLv3 or otherwise);
   - the VST3 SDK version bundled by JUCE 8.0.15 and its license (MIT only if the SDK is 3.8 or newer);
   - every library bundled inside the JUCE modules in use;
   - pffft;
   - the fonts.
2. **Steinberg trademark.** State whether a notice such as "VST is a registered trademark of Steinberg Media Technologies GmbH" is required or recommended for a VST3 plugin that does not display the VST logo. Cite Steinberg's current usage guidelines.
3. **Codecs.** Recommend which bundled codecs and libraries can be disabled through JUCE compile definitions, because Investigate does not read or write audio or image files beyond what the editor needs. Examples: `JUCE_USE_FLAC=0`, `JUCE_USE_OGGVORBIS=0`, `JUCE_USE_MP3AUDIOFORMAT=0`, `JUCE_USE_LAME_AUDIO_FORMAT=0` and `JUCE_USE_WINDOWS_MEDIA_FORMAT=0`. For each, confirm the definition name in JUCE 8.0.15, and whether disabling it removes the component's code, and therefore its notice obligation, from the binary.
4. **Notices text.** A draft of the exact text for `THIRD-PARTY-NOTICES.txt`, and a shorter version of the "Software" section for the About view. Mark the JUCE line as dependent on the M5 license decision.

## Acceptance criteria
- [ ] Every component compiled into the shipped binary is listed with its version, license and notice obligation, and a source path in the fetched JUCE tree or a URL.
- [ ] The JUCE open-source license at 8.0.15 and the bundled VST3 SDK version and license are confirmed from the files themselves, with paths cited.
- [ ] The Steinberg trademark question is answered with a citation.
- [ ] The recommended JUCE compile definitions are listed, each with a confirmation that the name exists in 8.0.15.
- [ ] The draft notice texts are complete and use only the license texts' own wording where wording is required.

## Log
- 2026-09-28 researcher: Wrote docs/research/T-023-third-party-notices.md. Confirmed from third_party/juce-src at 8.0.15: JUCE open-source tier is AGPLv3 (not GPLv3 as docs/dependencies.md currently states — T-021 already scopes this same correction); bundled VST3 SDK is 3.8.0, MIT (Steinberg, 2025 copyright). Listed all bundled module libraries (zlib, libpng, jpeglib/IJG, HarfBuzz, SheenBidi, FLAC, Ogg Vorbis, WMF wrapper) with licenses and compiled-in status. Recommended JUCE_USE_FLAC=0, JUCE_USE_OGGVORBIS=0, JUCE_USE_WINDOWS_MEDIA_FORMAT=0 (all confirmed to exist in juce_audio_formats.h at 8.0.15; MP3/LAME already default off). pffft license confirmed from upstream (BSD-style, 4 copyright holders) but its exact pin is not yet recorded in T-021's log — flagged as open item. Answered the Steinberg trademark question: attribution line required/recommended even without the VST logo, cited from Steinberg's VST3 developer portal usage guidelines. Drafted full THIRD-PARTY-NOTICES.txt text and a short About-view "Software" section, with the JUCE line marked pending the M5 license-tier decision. libebur128, Catch2 and pluginval documented as excluded (test/tool-only, not shipped). Did not modify third_party/, ran no CMake or builds.
