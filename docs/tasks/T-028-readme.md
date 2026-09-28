# T-028: Project README
- Owner: build-engineer
- Milestone: M2 (checkpoint)
- Status: DONE
- Depends on: T-027

## Goal
Add a root `README.md` that tells a developer, or the user later, what Investigate is, what state it is in, how to build and test it on Windows, and where every planning and QA document lives. Then commit and push it, together with the architect's pending documentation edits.

## Inputs
- User request, 2026-09-29: "create a README.md".
- `docs/roadmap.md`: Vision, Scope, Milestones, the Status table, the Handoff section and Decisions.
- `docs/dependencies.md`: the build, test and validate commands, the pinned dependencies and the Repository section.
- `docs/research/T-001-span-feature-parity.md` section 8 (IP boundaries) and `docs/research/T-023-third-party-notices.md`.
- The actual tree: `source/`, `tests/`, `tools/`, `cmake/`, `assets/` and `docs/`.

## Content requirements
Write clear English prose with short sections, in this order:
1. **Title and one-paragraph description.** Investigate is a free real-time spectrum analyser and metering plugin (VST3 and Standalone, Windows). Describe it in its own words. **Do not mention SPAN, Voxengo or any other product by name** (IP boundaries in T-001 section 8: no wording that implies affiliation).
2. **Status.** In development. M0 to M2 are complete: scaffold, DSP core, processor wiring. The editor UI (M3) is next. Link `docs/roadmap.md` for the live Status table.
3. **Features (v1 scope).** A concise list taken from roadmap Scope, marked "implemented" (the DSP and processor are done) or "planned" (the editor UI, installer and CI).
4. **Requirements.** Windows 11 x64, Visual Studio 2022 (MSVC), CMake 3.22 or newer, Git, and network access on the first configure (FetchContent).
5. **Build, test and validate.** The exact preset commands from `docs/dependencies.md`: configure, build Debug and Release, `ctest`, `ctest -L validation` for pluginval, and the hidden timing tests (`InvestigateUnitTests.exe "[perf]"`, from the Release test output folder). Say where the built VST3 and Standalone end up in the build tree, and verify the paths in the actual build tree.
6. **Repository layout.** A short annotated tree of the top-level folders and their purpose.
7. **Documentation index.** Links to the roadmap, `docs/tasks/`, `docs/design/` (UI spec and `mockup.html`), `docs/research/`, `docs/qa/` and `docs/dependencies.md`.
8. **Third-party components.** One paragraph naming the linked components (JUCE, the VST3 SDK, pffft, Inter, JetBrains Mono, and the libraries JUCE bundles) and the test-only tools (Catch2, libebur128, pluginval), with a link to `docs/dependencies.md` and `docs/research/T-023-third-party-notices.md`. Add the line "VST is a registered trademark of Steinberg Media Technologies GmbH."
9. **License.** "All rights reserved. The licensing model for the public release is not yet decided." No license file is added.

Constraints:
- `CLAUDE.md` and `.claude/` are gitignored, so do not reference them as files in the repository.
- No email addresses, tokens or personal data.
- Only relative links, so the links work on GitHub.

## Git
After writing the README, stage `README.md` and the architect's pending edits to `docs/roadmap.md`, `docs/tasks/T-018-windows-ci.md`, `docs/tasks/T-028-readme.md` and any other modified file under `docs/`. First review `git status --porcelain` and stage only expected paths. Commit with the message `docs: add README, close T-027, update roadmap handoff` and push to `origin main`.

## Acceptance criteria
- [ ] `README.md` exists at the repository root with all 9 sections, and renders correctly as GitHub Markdown.
- [ ] Every command in the README was run, or checked against the current presets. Every relative link resolves to an existing file.
- [ ] The README does not mention SPAN or Voxengo, contains no personal data, and does not reference `CLAUDE.md` or `.claude/`.
- [ ] The commit is pushed. `git status` is clean apart from ignored files, and `git ls-remote origin main` matches the local HEAD.

## Log
- 2026-09-29 build-engineer: Read CLAUDE.md, this task file, `docs/roadmap.md` (Vision, Scope, Milestones, Status, Handoff, Decisions), `docs/dependencies.md`, `docs/research/T-001-span-feature-parity.md` section 8 and `docs/research/T-023-third-party-notices.md`. Checked the actual tree (`source/`, `tests/`, `tools/`, `cmake/`, `assets/`, `docs/`) and the real build output under `build/` from earlier tasks: `build/Investigate_artefacts/Release/VST3/Investigate.vst3`, `build/Investigate_artefacts/Release/Standalone/Investigate.exe` and `build/tests/Release/InvestigateUnitTests.exe` all exist as named. Confirmed the `windows-msvc` configure preset and the `debug`/`release` build and test presets in `CMakePresets.json`, and the `ctest -L validation` and `[.perf]` commands against `tests/CMakeLists.txt` (label `validation` on the pluginval test, `catch_discover_tests` for the rest). Wrote the root `README.md` with the 9 required sections, following the "Voxengo SPAN" IP-boundary rule from T-001 section 8 (no product-name mentions), no email addresses, and no reference to `CLAUDE.md`/`.claude/`. Every relative link in the README (`docs/roadmap.md`, `docs/tasks/`, `docs/design/ui-spec.md`, `docs/design/mockup.html`, `docs/research/`, `docs/qa/`, `docs/dependencies.md`, `docs/research/T-023-third-party-notices.md`) was checked against the real tree. Reviewed `git status --porcelain`: staged `README.md`, `docs/tasks/T-028-readme.md`, and the architect's pending edits `docs/roadmap.md` and `docs/tasks/T-018-windows-ci.md`. Committed as `docs: add README, close T-027, update roadmap handoff` (commit `<see below>`) and pushed to `origin main`.
