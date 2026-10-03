# T-013 unit QA: meter panel

Verdict: PASS (round 2: D-1 and D-2 fixed in round 2, no open defects)

Tester: unit-tester. Date: 2026-10-02. Tree: working copy on `main` with the uncommitted T-013 changes.

## Scope

- `source/gui/MeterMath.h` (pure), `source/gui/MeterPanel.h/.cpp`, `SpectrumView` test hooks, `TooltipText::correlation`, meter additions in `DesignTokens.h`.
- Not covered here (plugin-validator, T-013-validation): processor-driven readouts, screenshots, paint cost, CPU matrix, pluginval strictness 10.

## Tests added (49 test cases, 4 new files, 1 tag change)

| File | Cases | Content |
|---|---|---|
| `tests/unit/MeterMathTest.cpp` | 20 | scales, zones, tick sets, `yFor`, every formatter, NaN/inf/huge fuzz, `FixedText` overflow, `sanitizeLevel`, `FallingLevel`, `HeldLevel`, `CrestRing`, `CorrelationFilter`, `StaleDetector`, `thicknessPx` |
| `tests/unit/MeterPanelTest.cpp` | 22 | panel through `update()`: clip latch, readouts, headroom, balance, ballistics, reset, crest, correlation, stale rule, mono, scale switch, software-image renders (all scales, mono, 5 strip heights), NaN snapshots, quantised repaint, 10 Hz text, spectrum-paced hint, collapse, resize, reset button, tooltip text |
| `tests/unit/MeterPanelAllocationTest.cpp` | 5 | allocation counts of `update()` |
| `tests/unit/SpectrumViewPacingPendingTest.cpp` | 2 | pacing deferral through `isFrameRepaintPendingForTesting()`, no window |
| `tests/unit/SpectrumViewRenderTest.cpp` | (1 un-hidden) | `[.]` removed from "SpectrumView: clip-limited paint is bit-identical to the full paint" |

Tags: `[gui][meter][...]`, `[metermath]`, `[alloc]`, `[pacing]`. No on-screen window is used by the new tests, so `GuiTestEnvironment.h` gating is not needed. The only clock is JUCE's ms timer in the pacing test (retries when the machine stalls, SKIPs if every attempt stalls).

## Results

| # | Requirement | Tests | Result |
|---|---|---|---|
| 1 | Scales: -20 dBFS peak sine (reported -23.01) shows 0.00 on K-20 (+-0.01), -23.01 in dBFS; K-14 reads -6.0 and K-12 -8.0 (0 K sits at -N dBFS); bar offset only in K modes; zones; ticks (K-20 "+24" to "-40", step 8 under 20 px) | `MeterMath: -20 dBFS peak sine ...`, `zones`, `tick sets`, `yFor`, `RMS formatting per scale`, `MeterPanel: meterScale changes the RMS cell text` | PASS |
| 2 | Formatting: -inf, -0.04 -> "0.0", +0.05 -> "+0.1", -0.05 -> U+2212 "0.1", headroom incl. negative flag and dash, crest 3.0 / dash, counts 9999 / "9999+", M/S below -99.9 -> "-inf", I/Mmax/Smax dash, LRA dash while I non-finite, correlation, balance, no NaN | `level formatting`, `headroom`, `crest`, `counts`, `loudness formatting`, `correlation and balance`, `no formatter ever prints NaN, inf or an ASCII minus` (14 inputs x 15 formatters), `NaN and infinite snapshots never reach the text` | PASS |
| 3 | Ballistics: 20 dB/s fall, hold 1500 ms (exactly: 1500 holds, >1500 falls) then 30 dB/s, never below the floor, TP hold, 256-entry crest ring (covers 3 s), correlation step response at tau 50 ms (63.2 % at 50 ms, frame-rate independent), stale detector (250 ms, `!=`, wrap) | `FallingLevel`, `HeldLevel`, `CrestRing`, `CorrelationFilter`, `StaleDetector`, panel-level `peak bar falls`, `hold never drops below the bar`, `correlation display filter` | PASS |
| 3b | Stale panel rule: live fields read as silence, bars fall, maxima / counts / headroom / I / LRA / M max / S max / correlation / balance hold, recovery on first counter change, wrap | `MeterPanel: stale snapshot rule` (+2 sections) | PASS |
| 4 | Panel behaviour: clip latch / clear / 9999+; reset clears holds, TP hold and crest ring; headroom; balance -6 -> "L 6.0 dB"; mono (single bar, left cells empty, dash, switch back); scale change; quantised repaint; 10 Hz text; spectrum-paced hint (hold 34 ms, flush on `spectrumRepainted`); collapse; resize | `MeterPanel: ...` cases listed in the file | PASS |
| 5 | Allocation: 0 in unchanged / moving / falling paths; <= 20 per refresh when all cells change | `allocates nothing` x3 PASS; `a refresh tick where every cell changes stays within 20 allocations` | PASS (D-1 fixed in round 2) |
| 6 | `[strict]` clip-limited paint test un-hidden and passing (Debug and Release); pacing test via `isFrameRepaintPendingForTesting()` | `SpectrumView: clip-limited paint is bit-identical ...`, `SpectrumView pacing (no window) ...` x2 | PASS |
| 7 | Design-token grep audit | see below | PASS with notes |
| 8 | Full suite Debug and Release; /W4 clean | see below | PASS |
| - | `TooltipText::correlation` uses U+2212 (ui-spec 8.5) | `TooltipText::correlation: windows, U+2212 minus and the mono text` | PASS (D-2 fixed in round 2) |

### Full suite

- Debug: `ctest --test-dir build -C Debug --timeout 300`: 326 tests, 1 failed (D-1), 5 skipped by design (Tech 3341/3342 authentic-programme cases without audio files). pluginval test passed (18 s). The tooltip test was added after this run; Debug direct run of it fails (D-2).
- Release: `ctest --test-dir build -C Release --timeout 300`: 327 tests, 2 failed (D-1, D-2); the same 5 skipped; pluginval passed.
- No other test regressed, including the 92-case spectrum tests, the pacing window test and the T-029 render tests.
- `[.perf]` tests: none cover the GUI (the hidden perf tests are DSP / processor only), so none were run. The meter paint cost belongs to plugin-validator.
- /W4: touched and rebuilt `MeterPanel.cpp`, `MeterMath.h` (through its includers), `TooltipText.cpp`, `SpectrumView.cpp` and the new tests in Release: 0 warnings. (My own tests had one C4459 for a global named `inf` hiding a JUCE name; fixed in the tests.)

## Round 2 result (final)

- Rebuilt Debug and Release from the presets (`build/`): 0 warnings at /W4 in the touched files (`MeterPanel.cpp`, `TooltipText.cpp`, includers of `MeterMath.h`).
- `ctest --test-dir build -C Debug --timeout 300`: 327 tests, 100 % passed, 0 failed, 5 skipped by design (Tech 3341/3342 authentic-programme cases without audio files).
- `ctest --test-dir build -C Release --timeout 300`: 327 tests, 100 % passed, 0 failed, the same 5 skipped.
- Tests #105 (allocation, limit still `<= 20` per refresh tick) and #186 (tooltip requires U+2212) pass in both configurations, unweakened.
- Token audit: the channel-letter rectangle now uses `tokens::meter::letterPad` (DesignTokens.h line 278, MeterPanel.cpp line 572); the `4` / `8` literals are gone. Remaining literals are the structural ones listed in the audit (`/ 2` and `/ 3` column splits, 1 px line padding, sentinels).
- The resized()-clears-cells observation (item 1 below) is also resolved by the fix (resized no longer clears text).

## Defects (history; both fixed in round 2)

### D-1 (medium, fixed in round 2): text refresh allocates 26 times when every cell changes, limit is 20

- Expected: roadmap Decisions 2026-10-03 and the task acceptance criterion: text readouts rebuild strings only when the displayed text can change, at most 10 Hz, with <= 20 allocations per frame (spec 12).
- Actual: with all 26 cells changing in one refresh (stereo), `MeterPanel::update` makes 26 allocations on the refresh frame (worst 26, mean 22.25 over 20 refreshes). The count is exactly one `juce::String` per changed cell. Frames between refreshes: 0 allocations (steady, moving bars, falling bars all pass). Mono / scale switch passes (<= 20).
- Repro: `InvestigateUnitTests.exe "MeterPanel::update: a refresh tick where every cell changes stays within 20 allocations"` (Debug and Release); ctest test #105.
- Suspected cause: `MeterPanel.cpp` `setCell`, line 389: `cell.string = text.toString();` builds a heap `juce::String` for each changed cell (`FixedText::toString`, `MeterMath.h` line 173).
- Suggested fix: keep only the `FixedText` per cell and build the `juce::String` where it is used (`paintCells`, `getCellText`; paint allocations are outside this criterion), or draw from a preallocated buffer. Either removes all per-cell allocations at refresh.

### D-2 (low, fixed in round 2): correlation tooltip prints mojibake instead of U+2212

- Expected: ui-spec 8.5, "When shipped, it uses U+2212 for the minus sign": "... 0 unrelated, −1 out of phase ...".
- Actual: the text reads "... 0 unrelated, â\u0088\u00921 out of phase ...". The source literal is a double-encoded sequence (bytes C3 A2 C2 88 C2 92), so it renders as "a" with a circumflex plus two control characters.
- Repro: `InvestigateUnitTests.exe "TooltipText::correlation: windows, U+2212 minus and the mono text"`; ctest test #186.
- Location: `source/gui/TooltipText.cpp` line 132: `juce::String::fromUTF8 ("...")`. Use the file's own `minus()` helper (line 10) or `"\xE2\x88\x92"`.

## MeterPanel.cpp review (no further defects that a test could prove)

Checked: repaint bookkeeping (`requested` / `wanted` reset on resize, show, scale / mono change and pixel-scale change; counters match what is requested), mono switching (bar 1 unlit, cells hidden and cleared, cache keyed on mono), scale switching (cache keyed on scale; panel-wide repaint on change), collapse (`update` returns early while hidden; `visibilityChanged` resets clocks; the stale detector simply sees the changed counter on return), stale handling (applied to a copy, snapshot untouched).

Observations that are not asserted as defects:

1. `resized()` calls `layoutCells()`, which clears every cell's text, and `lastTextMs` is reset, so the cells stay blank until the next `update()` (one vblank). A paint between the two could flash empty numbers during live resizing. Low; cannot be shown without a real window (plugin-validator may check).
2. `computeBarPixels` computes `lampLit` from `sampleClipCount + truePeakClipCount > 0`; at uint64 wrap the sum could be 0. Unreachable in practice.
3. No `lookAndFeelChanged()` handler: the cached static layer is keyed on size / scale / meterScale / mono but not on the look and feel. The editor never swaps it at run time, so no action.
4. `MeterPanel::update` makes the first change after a quiet period flush at once even when the spectrum is paced and did not repaint on that vblank (`nowMs - lastFlushMs >= 34`). That is consistent with the documented rule (at most 34 ms hold) but means one extra window present in that case. Design note for the CPU matrix.
5. The ring buffer drops pushes closer than max(8 ms, window / 240). A sample-peak pulse shorter than the spacing can be missed by a feed faster than 60 Hz; the 50 ms peak window in the snapshot makes this harmless at 30 to 60 Hz. Covered by the 60 Hz feed test.
6. In mono, `channels[1]` mirrors channel 0, so after a switch to stereo channel 1's hold tick may show channel 0's old peak for up to 1.5 s. Cosmetic.

## Real-time safety and allocation notes

- `MeterPanel::update`, `applySnapshot`, `refreshText` (apart from `toString`, D-1), `flushRepaints`, all `MeterMath` calls: no `new`, no containers growing, no locks, no `std::function` construction (the two `std::function` members are set in the constructor). The `RectangleList`s are sized in the constructor (`ensureStorageAllocated (2 * minBatch)`); `flushList` pads in place.
- Not measurable by the allocation hook: `MeterPanel::paint` and everything inside it (JUCE rasteriser, `drawText` text layout, Direct2D), and `repaint()` into a real peer. The allocation tests run `update()` with the panel inside a parent that is not on the desktop. `paint` allocation behaviour is for the on-screen validation run.
- Audio thread: the panel never touches `processBlock`.

## Design-token grep audit (new code)

- Hard-coded colours (hex literals, `juce::Colour (`, `Colours::`): none in `MeterMath.h`, `MeterPanel.h/.cpp`, `TooltipText.*`. All colours come from `tokens::colour`, `tokens::alpha` and `tokens::stroke`.
- Fonts: no `juce::Font` or sizes; all text goes through `tokens::Type` and `look.drawText`.
- Spacing and sizes: the meter layout lives in `tokens::meter` (DesignTokens.h). Remaining literals in `MeterPanel.cpp`:
  - line 582: `{ x - 4, ..., M::barW + 8, ... }` is the bar letter box padding (4 px each side). Should be a `tokens::meter` constant.
  - line 656: `rc.getWidth() / 3` (three correlation scale labels) is structural.
  - line 564: `0.5f` is a centre-of-line factor; line 695: `-1000.0f` is an off-clip padding sentinel; line 850: `0.01f` is a scale floor. Not design values.
- `MeterMath.h` keeps spec numbers as named constants where they are design values (`aes17OffsetDb`, `barFloorDb`, `barCeilDb`, `silenceDb`, hold and fall rates, ring size, `tauSeconds`, `staleAfterMs`). Remaining inline spec literals: tick pixel threshold 20 px and K steps 4 / 8 (`computeTicks`), `-99.9` (`formatLoudnessLive`), `0.05` and `60` (`formatBalance`), ring spacing 8 ms and window / 240, `-200` / `+200` (`sanitizeLevel`). These are ui-spec numbers, not tokens; listing them as named constants would be tidier but is not required.
- Notes: `TooltipText.cpp` line 132 has a hard-coded wrong-encoded literal (D-2).

## Open questions

1. Should K-14 and K-12 RMS display as negative K values for a -20 dBFS sine (-6.0 and -8.0 K)? The tests assume yes (0 K sits at -14 and -12 dBFS), which follows from the spec formula `rms + 3.0103 + N`. The task text "K-14 and K-12 likewise" is read the same way.
2. The 20-allocation limit: if the architect prefers to keep one `juce::String` per cell, the limit must be raised to 26 or the cell count must drop. The test asserts the limit as written.
3. Coverage and sanitizer presets were not run (no coverage preset exists; the change touches no threading code).

## Commands

```
cmake --preset windows-msvc                       # re-glob the new test files
cmake --build build --config Debug --target InvestigateUnitTests --parallel
cmake --build build --config Release --parallel
ctest --test-dir build -C Debug --timeout 300
ctest --test-dir build -C Release --timeout 300
build/tests/Release/InvestigateUnitTests.exe "[meter]" -r compact
build/tests/Debug/InvestigateUnitTests.exe "[strict]"
build/tests/Debug/InvestigateUnitTests.exe "[pacing]"
```

## Round 3 (re-test of the round-2 fixes and O-2 / O-3)

Verdict: PASS (0 defects).

New tests:

| Test | Result |
|---|---|
| `MeterMath::formatLoudnessMax: -inf below -99.9, dash when non-finite` (-153.7, -100, -99.95 give "−inf", -99.9 gives "−99.9", NaN / ±inf give the dash, same text as `formatLoudnessLive`) | PASS |
| `UpdateRateLimiter: 144 Hz vblank is capped at 60 per second` (worst 1 s window <= 60, 10 s total 595 to 600) | PASS |
| `UpdateRateLimiter: 60 Hz with +-1 ms jitter is not thinned` (>= n-2 of 600 accepted) | PASS |
| `UpdateRateLimiter: 30 Hz passes everything, a long gap resets` (10 s stall: one update, no burst catch-up) | PASS |
| `MeterPanel::resetHeldValues: text refreshes on every update for 250 ms` (8 updates 1 ms apart, M max / S max change each call) | PASS |
| `MeterPanel::resetHeldValues: the 100 ms cadence returns after the window` | PASS |
| `MeterPanel: maxima below -99.9 LUFS and non-finite maxima` (panel cell shows "−inf" / dash) | PASS |
| `MeterPanel::update: forced refreshes after resetHeldValues stay within 20 allocations each` (all cells changing every call, 10 calls) | PASS |

Existing allocation tests (D-1 round 1 bound of 20) still pass. D-2 (MONO label width 44) is not asserted by a unit test (paint geometry; left to the on-screen run).

Full suite: Release 334/334 pass; Debug passed 334/334 on the first run. On a second Debug run test #315 (`setAnalysisActive(true)/(false) 100 times while audio runs concurrently ...`, ProcessorAnalysisLifecycleTest) hit the 300 s timeout once (system also busy with a parallel build); 6 isolated reruns all pass in seconds. Unrelated to T-013 code; reported as an intermittent hang (possible race/deadlock in setAnalysisActive, plugin-engineer) to watch. Only the existing skipped Tech 3341/3342 cases are 'not run'.

Notes:
- `forceTextUntilMs` is set from `juce::Time::getMillisecondCounterHiRes()` but compared against the `nowMs` passed to `update()`. That is consistent only if the caller passes the same clock (the editor does). Tests therefore start simulated time from the real clock. Anyone feeding a different time base would see the forced window never end or never start; worth a comment in `MeterPanel.h`.
- `tools/render-harness/meter_check.cpp` (plugin-validator's file) has two C4996 warnings (`Displays::userArea` deprecated); not touched.

Commands: `cmake --build build --config Debug|Release --target InvestigateUnitTests`; `build/tests/Debug/InvestigateUnitTests.exe "[limiter],[reset],[metermath],[alloc]"`.
