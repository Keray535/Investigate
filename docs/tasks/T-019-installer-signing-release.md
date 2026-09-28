# T-019: Windows installer, code signing and release docs
- Owner: build-engineer
- Milestone: M5
- Status: TODO
- Depends on: T-018, and user decisions (see below)

## Goal
Produce a signed Windows installer for Investigate's free public release. The installer puts the VST3 and the Standalone in their standard locations and can be uninstalled cleanly. Document the release process in `docs/release.md`.

## User decisions required before dispatch
The architect asks the user for these and records the answers in the roadmap Decisions log and in this task's log:
1. **JUCE license.** The JUCE Starter licence (closed source) or AGPLv3 (the source is published under AGPLv3).
2. **Code signing.** For example Azure Trusted Signing, an OV certificate or an EV certificate, or ship unsigned with SmartScreen warnings. Also: who holds the credentials.
3. **Identity.** The final company name, manufacturer and plugin codes, and bundle ID. The placeholders in the roadmap may be kept.
4. **Legal text.** The EULA or license text shown in the installer, plus the version number for the first release.

## Inputs
- `docs/research/T-002-dsp-references.md`: section 10 (the Inno Setup and WiX license verdicts).
- `docs/roadmap.md`: Scope, Identity and Decisions.
- The CI workflow from T-018.

## Scope
- An installer script using the tool T-002 approved, placed under a build-engineer-owned path (for example `packaging/windows/`). It installs `Investigate.vst3` to `%CommonProgramFiles%\VST3\` and the Standalone to `%ProgramFiles%\Investigate\`, with a Start menu entry for the Standalone and an uninstaller. The version comes from the CMake project version.
- Signing of the VST3 binary, the Standalone `.exe` and the installer, in CI, using secrets only. A local dry run uses a self-signed test certificate that is never committed.
- A CI release job triggered by a version tag (`v*`) that builds, signs, packages and attaches the installer to a GitHub release draft.
- `docs/release.md`: the release checklist, the secret names and how to rotate them, versioning, and how to verify a signature.
- Third-party notices: the installer installs a `THIRD-PARTY-NOTICES.txt` next to the Standalone. Start from the draft in `docs/research/T-023-third-party-notices.md` section 4. It covers JUCE, the VST3 SDK 3.8.0 (MIT), zlib, libpng, jpeglib, HarfBuzz, SheenBidi, pffft, Inter and JetBrains Mono, plus the Steinberg trademark line. Before finalising:
  - re-verify pffft's license text at the commit T-021 pinned;
  - check whether SheenBidi ships an Apache-2.0 NOTICE file; if it does, reproduce it;
  - replace the JUCE placeholder line with the wording for the licence tier the user chooses;
  - drop any codec that T-014 disabled.

## Acceptance criteria
- [ ] The installer installs and uninstalls cleanly on a Windows 11 machine, leaving no files behind other than user data, if any.
- [ ] `signtool verify /pa /v` passes on the VST3 binary, the Standalone and the installer when signed with the real credentials. It also passes with the self-signed certificate in the dry run, which is documented.
- [ ] pluginval at strictness 10 passes on the installed VST3 at its installed path.
- [ ] No certificate, password or key exists in the repository or in the CI logs.
- [ ] `THIRD-PARTY-NOTICES.txt` is installed and covers every font and every linked third-party library.
- [ ] `docs/release.md` is complete enough for the user to cut a release by following it step by step.

## Log
