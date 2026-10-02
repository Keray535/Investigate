# T-012 unit QA: SpectrumView and SpectrumMath

Date: 2026-10-02. Tester: unit-tester. Verdict: **PASS with 1 defect (D1, allocation while hovering) and 2 minor token findings.** See "Verdict" at the end.

## Test files
- `tests/unit/SpectrumMathTest.cpp` (9 test cases, pure math, no component)
- `tests/unit/SpectrumViewAllocationTest.cpp` (2 test cases; one is hidden, see D1)

## Planned tests and results

| # | Test (Catch2 name) | Spec / criterion | Result |
|---|---|---|---|
| 1 | noteName acceptance examples | T-012 AC: 440 "A4 +0c", 261.626 "C4 +0c", 27.5 "A0 +0c", 452.6 "A4 +49c", 452.9 "A#4 U+2212 50c" | PASS |
| 2 | noteName edge cases | 0, negative, NaN, inf give empty; 880 "A5 +0c"; 20 Hz is D#0; flat note uses U+2212 never ASCII "-" | PASS |
| 3 | Mapping x and y round trips | AC: round trips within 1e-6 relative; 4 frequency ranges x 3 dB ranges, 201 points each, plus pixel to freq to pixel | PASS |
| 4 | dbToYClamped | ui-spec 5.1: out of range pinned to edges, non-finite at the bottom | PASS |
| 5 | formatFrequency | ui-spec 5.5: 43.7 Hz, 437 Hz, 4.37 kHz, 14.3 kHz, 999.6 gives "1.00 kHz", 99.96 gives "100 Hz", 9996 gives "10.0 kHz" | PASS |
| 6 | formatFrequencyForClipboard | ui-spec 5.6: "43.7", "1250", "14300", no unit, 999.6 gives "1000" | PASS |
| 7 | formatLevel | ui-spec 5.5: 1 decimal, half away from zero, "0.0 dB" for tiny values, no "+", U+2212, "-inf" for non-finite or at or below -200 | PASS |
| 8 | interpolateLevel (2 cases) | linear dB over log f, end clamps, empty and single-point frames, -inf / NaN / floor neighbour gives -inf | PASS |
| 9 | placeHoverCard | ui-spec 5.5 steps 1 to 4 (default, flip left, flip below, both, clamp with 4 px inset, oversize card), plus a pointer sweep over the data rect: card never contains the pointer and stays inside the inset rect | PASS |
| 10 | formatFrequencyLabel | "20", "300", "1k", "20k", "1.5k" | PASS |
| 11 | chooseFrequencyLabelSet / chooseDbStep | ui-spec 5.3 resolved examples: 840 px gives the first set, 560 px the second, 150 px decades only; dB step 6 at 564 px and 404 px heights, 12 for -150..+30 at 405 px | PASS |
| 12 | minorFrequencyLinesVisible | 4 px threshold | PASS |
| 13 | reduceToColumns | peak and trough preserved per column, drop outside points, clamp and non-finite handling, reset of all columns, 2x density, zero columns | PASS |
| 14 | SpectrumView::pollFrame with no change allocates nothing | task: no allocation in the vblank callback; 300 polls under `ScopedAllocGuard` | PASS (0 allocations) |
| 15 | SpectrumView::pollFrame while hovering allocates nothing (hidden, `[.spectrumview-hover-alloc]`) | same, with the pointer in the data rect and a range change per poll | FAIL, see D1 |

Note on test 11: the code returns label sets 0, 1 and 2, while ui-spec 5.3 numbers them 1 to 3. The spec's "default uses set 1, minimum uses set 2" therefore matches the code's 0 and 1. By the stated rule (gap of at least 36 px) the default 840 px gives gaps of at least 40.9 px (5 to 7), so the denser first set is correct; the 560 px case is 27 px for 5 to 7, so it falls to the second set. No defect.

## Defects

### D1: SpectrumView::pollFrame allocates heavily while the pointer hovers (medium)
- Expected: task "Allocation": no allocation per frame in paint or in the vblank callback; header comment says hover strings are rebuilt "only when the pointer or a frame changes", but a frame changes up to 60 times a second.
- Actual: with the pointer in the data rect, every poll that sees a new frame or a range change calls `updateHover()`, which builds `juce::String`s (`formatFrequency`, `noteName`, `formatLevel`, `curveName`, row name copies) and calls `getTextWidth` (`GlyphArrangement`, and `toUpperCase()` copies for caps types). Measured: 21660 allocations over 20 polls (about 1080 per poll). With no hover and no change, 0 allocations over 300 polls.
- Test: `InvestigateUnitTests "[.spectrumview-hover-alloc]"` (hidden so the suite stays green; remove the `[.]` tag when fixed).
- Suspected location: `source/gui/SpectrumView.cpp` `updateHover()` (lines 342 to 410), called from `pollFrame()` at lines 189, 202 and 214. A fix would cache the text width per string or compute widths from fixed-size buffers, and format into stack buffers.
- Note: `std::string`-free text is not possible with JUCE `drawText`, so a zero-allocation hover needs the gui-engineer to decide the acceptable bound (for example, only recompute strings when the rounded frequency or rounded level text changes).

### Minor token findings (low)
- `source/gui/SpectrumView.cpp:806`: literal `0.22f` for the primary A legend swatch fill alpha; not in `DesignTokens.h`.
- `source/gui/SpectrumView.cpp:978`: toast border stroke is the literal `1.0f`; should be `tk::stroke::hairline`.
- `drawSwatch` and the sidechain swatch constructor (lines 63 to 66, 800 to 830) use literal geometry (4.75, 3.5, 7.5, 2.0, 4.0, 5.0, 11.0, 12.0). These are the 16x10 mockup swatch drawings, not tokens; listed for completeness.

## Allocation audit: paint and pollFrame
JUCE's own rasteriser (EdgeTable, `fillPath`, `strokePath`), glyph layout (`drawText`) and image drawing allocate internally; this audit covers only the SpectrumView code itself. Audit of `source/gui/SpectrumView.cpp`:

Allocation-free after first frame:
- `paintCurves`, `tracePolyline`: `pointX` and `columns` are sized in the constructor (`assign`, lines 52 and 53); `pathA`, `pathScratch`, `fillPath` use `preallocateSpace (3 * maxVertices + 16)`, and `Path::clear()` keeps capacity; vertex count is bounded by `2 * numCols` and `numCols <= maxColumns`. `frame` is a heap `SpectrumFrame` made once. `PathStrokeType`, `FillType` and `Colour` are stack values.
- `rebuildCacheIfNeeded`: allocates the `cache` Image only on resize, scale or range change (static cache, as designed).
- `pollFrame` without hover and without change: tested at 0 allocations (test 14). `getLatestSpectrumFrame` copies into the existing frame.
- `paintLegend`, `paintHover` read stored `juce::String`s; no construction.

Allocates in SpectrumView code (heap `juce::String` or per-call objects):
- `updateHover()` via `pollFrame` while hovering (D1).
- `paintBadge` (line 922 onward): `look.getTextWidth ("Frozen", ...)` and `drawText (..., "Frozen", ...)` construct a `juce::String` from a literal on every paint while frozen. Also `toUpperCase()` in the look-and-feel makes a copy for caps types. Small, but not zero; fix by caching the width and string.
- `paintToasts`: uses stored text; the only allocations are inside JUCE's `drawText`.
- `showToast`, `mouseDown`, `mouseUp`: build strings, but run on user clicks, not per frame (acceptable).
- Every `look.drawText` call goes through `juce::Font` copies and `GlyphArrangement` (JUCE internal).

Conclusion: the curve path (the per-frame cost) is allocation-free; text drawing and the hover recompute are not, per D1 and the "Frozen" badge item.

## Design-token grep audit (SpectrumView.cpp, SpectrumView.h, SpectrumMath.h)
Commands (Git Bash, from `source/gui`):
- `grep -nE '0x[0-9A-Fa-f]{6,8}|#[0-9A-Fa-f]{6}\b|juce::Colour *\(|Colours::' SpectrumView.cpp SpectrumView.h SpectrumMath.h` : no matches (no raw colours).
- `grep -nE 'juce::Font *\(|FontOptions|withHeight|setFont' ...` : no matches (fonts via `tk::Type`).
- `grep -nE 'withAlpha *\( *[0-9]|withMultipliedAlpha *\( *[0-9]' SpectrumView.cpp` : one match, line 806 (`0.22f`).
- Radius and stroke literal scan : one match, line 978 (`1.0f`), line 911 uses `tk::radius` and `sp::cardBorder`.
Result: PASS apart from the three minor findings above. `SpectrumMath.h` has no literals beyond spec formulas; the thresholds (36, 4, 20, floor -200, card offset 18, inset 4) come from `tokens::spectrum`.

## Suite results
- Debug: `ctest -C Debug -E pluginval` in `build`: 251 of 251 passed, 0 failed (the 5 EBU "not run" cases are the existing skipped placeholders).
- Release: `ctest -C Release -E pluginval`: 251 of 251 passed, 0 failed.
- Both new test files compile warning-free at `/W4` (no `warning C` lines in the build log). The pluginval CTest entry was excluded; it needs the built VST3 and belongs to plugin-validator.

## Commands used
```
cmake -S . -B build
cmake --build build --config Debug   --target InvestigateUnitTests -j 8
cmake --build build --config Release --target InvestigateUnitTests -j 8
build/tests/Debug/InvestigateUnitTests.exe "[spectrummath],[spectrumview]"
build/tests/Debug/InvestigateUnitTests.exe "[.spectrumview-hover-alloc]"     # fails, D1
ctest -C Debug   -E pluginval --test-dir build
ctest -C Release -E pluginval --test-dir build
```

## Open questions
1. D1: what allocation bound is acceptable while hovering? JUCE text layout cannot be made allocation-free, so the architect or gui-engineer should decide between caching text metrics and accepting a per-frame hover cost with a measured budget.
2. Sanitizers and coverage were not run (no sanitizer or coverage preset; the change adds no threading and no audio-thread code).

## Verdict
Unit-level acceptance for `SpectrumMath` is PASS (all acceptance examples and round trips). Allocation criterion: PASS for the curve path and idle `pollFrame`; FAIL for hover (D1) and a minor per-paint allocation in the Frozen badge. Overall: PASS for `SpectrumMath`, conditional on a decision on D1 for the allocation criterion.

---

# Round 2 (2026-10-02)

## Changes to tests
- `tests/unit/SpectrumViewAllocationTest.cpp`: the hidden `[.spectrumview-hover-alloc]` test is now the normal test "SpectrumView::pollFrame while hovering stays within 20 allocations per poll" (`[gui][alloc][spectrumview]`). It runs 40 hover polls (pointer in the data rect, a dB range change plus `context.refresh()` before each poll) under `ScopedAllocGuard`, and asserts both average <= 20 and worst single poll <= 20. The idle test (300 polls, expect 0) is unchanged.
- How the count was taken: the link-wide `operator new` hook counts every allocation in the process. `pollFrame` never draws, so it never enters JUCE's `drawText` or `GlyphArrangement`; every counted allocation is therefore made by Investigate's `updateHover` string rebuilds (plus the small `juce::String` internals those builds need). No separation by call site was needed or possible with the hook.
- Result: 520 allocations over 40 polls (13.0 per poll, worst poll 13). Idle: 0 over 300 polls. Both PASS.

## Frozen badge re-check
Fixed by code reading: `frozenText` is a cached `juce::String` member (SpectrumView.h:197) and `badgeTextW` is cached at SpectrumView.cpp:93; `paintBadge` (lines 949 to 961) reads both and constructs no string. Remaining allocations in paint are inside JUCE (`drawText`, rasteriser), which the amended criterion excludes. paint was not counted by a test because JUCE internals dominate the figure and cannot be separated by the hook.

## Token grep audit (re-run)
Same commands as round 1 on `SpectrumView.cpp`, `SpectrumView.h`, `SpectrumMath.h`: no raw colours, no font construction, no numeric `withAlpha`/`withMultipliedAlpha`. The `0.22f` and toast `1.0f` findings are fixed. Remaining numeric literals are `1.0f` alpha/scale identities (lines 852, 939, 981, 999), the `1.0f / scale` physical-pixel line width (590) and hover line width `1.0f` (892, a 1 px device line, arguably `tk::stroke::hairline`; low, informational), and swatch geometry (accepted by the architect, not a defect). PASS.

## Suite and warnings
- Debug: `ctest -C Debug -E pluginval --test-dir build`: 252 of 252 passed, 0 failed.
- Release: 252 of 252 passed, 0 failed.
- New/changed test files compile warning-free at /W4.

## Defects (round 2)
- D2 (low, build hygiene): `source/gui/SpectrumView.cpp:356` in `updateHover()`: `auto& look = getLaf();` is now unused after the cached-width change, giving MSVC `warning C4189: local variable initialized but not referenced` in Debug and Release (project rule: warning-clean at /W4). Fix: delete that line. Does not affect behaviour or any test.

## Commands (round 2)
```
cmake --build build --config Debug   --target InvestigateUnitTests -j 8
cmake --build build --config Release --target InvestigateUnitTests -j 8
build/tests/Debug/InvestigateUnitTests.exe "[spectrumview]" -s
ctest -C Debug   -E pluginval --test-dir build
ctest -C Release -E pluginval --test-dir build
```

## Final verdict (round 2): PASS (functional criteria). D1 closed. One low build-hygiene defect, D2, must be fixed by gui-engineer before the "warning-clean" convention is met; it needs no further test round.
