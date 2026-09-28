# T-022: Font license check (Inter, JetBrains Mono)
- Owner: researcher
- Milestone: M3
- Status: DONE
- Depends on: T-010

## Goal
Confirm that the two fonts proposed in the UI spec can be embedded in Investigate's closed-source, freely distributed plugin binary, and record what attribution this requires. The ui-ux-designer may add the font files to `assets/ui/` only after this check passes.

## Inputs
- `docs/design/ui-spec.md`, the section that lists the fonts, weights and fallbacks.
- The proposed fonts:
  - Inter 4.x: https://github.com/rsms/inter
  - JetBrains Mono 2.304: https://github.com/JetBrains/JetBrainsMono
- `docs/roadmap.md`, Decisions (free public release, closed source).

## Deliverable
Write `docs/research/T-022-font-licenses.md` with:
- the exact release tag to use for each font;
- the license text identifier (for example SIL OFL 1.1), confirmed from the LICENSE file at that tag;
- any Reserved Font Name clause, and whether embedding unmodified TTF or OTF files in a binary triggers it;
- the attribution or license-file obligations for a binary distribution (for example shipping the OFL text with the installer, or in an About box);
- the direct download URLs for the exact weight files the spec lists;
- a verdict per font: approved, approved with conditions, or rejected.

## Acceptance criteria
- [ ] Both fonts have a verdict with the pinned tag, the license, and a source URL to the LICENSE file at that tag.
- [ ] The obligations for binary distribution are listed as concrete action items that the installer task (T-019) and the About or credits text can follow.
- [ ] Any subsetting or modification restrictions are stated. Subsetting to reduce binary size is likely.

## Log
- 2026-09-28 researcher: Confirmed both fonts under SIL OFL 1.1 at pinned tags (Inter v4.1, JetBrains Mono v2.304); no Reserved Font Name declared in either LICENSE/OFL.txt, so embedding and subsetting unmodified TTFs is permitted. Verdict: approved with conditions for both -- ship OFL license text + copyright notice with the installer/credits, do not sell fonts standalone. Verified exact weight files (Inter-Medium.ttf, Inter-SemiBold.ttf, JetBrainsMono-Regular.ttf, JetBrainsMono-Medium.ttf) exist in the pinned release archives. Report: docs/research/T-022-font-licenses.md.
