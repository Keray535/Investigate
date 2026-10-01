# T-011 unit QA report (round 1)

Verdict: **FAIL** (1 defect, plus 3 low-severity findings)

Scope: design-token audit, pure-helper tests, ViewState tolerance, full suite in Release and Debug. Display-dependent behaviour (screenshots, open/close cycles, pluginval) belongs to `T-011-validation.md`.

## 1. Design-token grep audit (source/gui, source/PluginEditor.*)

Commands: `grep -nE "0x[0-9a-fA-F]{6,8}|Colour *\(|fromRGB|fromHSV|Colours::"`, `grep -nE "Font *\(|withHeight|FontOptions"`, and a numeric-literal scan of every code line, all excluding `DesignTokens.h`.

| Class | Result |
|---|---|
| Hex colour literals / `Colour(...)` constructions outside DesignTokens.h | None. |
| Named JUCE colours | `juce::Colours::transparentBlack` only: HeaderButton.cpp:116-117, InvestigateLookAndFeel.cpp:85, SettingsPopover.cpp:319-321. Not a colour number; accepted. |
| Font sizes | None hard-coded. The only size source is `tokens::typeSpec` via `makeFont` (InvestigateLookAndFeel.cpp:110-132). |
| 0, 1, 0.5f halving, loop bounds | Accepted per the brief. |

Findings (none block the criterion, all are minor magic numbers; coder may fold into tokens):
1. ParamControl.cpp:253 and :269-270: highlight rectangle uses literal `- 3` / `+ 6` around the value text. This is a spacing value outside DesignTokens.h. Suggest `L::valueHighlightPadY`.
2. InvestigateLookAndFeel.cpp:236-244 (resize grip): `w / 4.0f` pitch, `0.7f` offset, `3` rows. Geometry constants outside the tokens. The 1 px dot radius itself is tokenised.
3. ControlMath.cpp:18, 54, 61, 154, 158 and TooltipText.cpp:76: behaviour constants (snap steps 10/100 ms, wheel steps 0.5/1.0, 2^(1/6), 2^(1/24), 1 % nudge) and a duplicate 48000 fallback (DisplayContext.h:56 already has `fallbackRate`). These are interaction values, not colour/size/spacing, so they are outside the literal acceptance criterion. Low priority.
4. Focus-order integers in PluginEditor.cpp:19-22, 100-104 and SettingsPopover.cpp:19-20, 292-296 are not spacing. Accepted.

Criterion "no component hard-codes a colour, font size or spacing value outside DesignTokens.h": **PASS with the notes above** (finding 1 is the only literal spacing, 3 px).

## 2. Tests added (all in tests/unit/, no display needed, tag `[gui]`)

| File | Test cases | Covers |
|---|---|---|
| EditorLayoutMathTest.cpp | 14 | Size clamp (limits, independence, extremes); regions at 1200x700, 920x540, collapsed, tiling at 4 sizes x 2 states; data rect (840 x 564 at default); control bar x positions at default, minimum and collapsed against ui-spec 4.3 table (12/80/156/240/320, dividers 398 and 614, 408/508, 624/772, collapsed 920/1068); exact visibility thresholds (615/616, 903/904); group C right alignment; meter strip rects, rowH, table, loudness, correlation and bar track in editor coordinates at both sizes (ui-spec 4.4 table); rowH clamp 22..28 and gap >= 12 sweep; popover bounds. |
| EditorValueFormatTest.cpp | 17 | `formatSignedDb` (U+2212, plus, zero, half away from zero); `formatLevelRange` (one shared "dB", "…"); `splitChipText` for all 8 unit parameters, unit-less values, degenerate text; `formatSampleRate` (44.1, 48, 88.2, 96, 176.4, 192 kHz; 0, negative, NaN); `isParsableNumericText`; `computeEffectiveRange`, `isClamped`, `clampTooltip` (Nyquist at 48/44.1/96 kHz, octave clamp, 12 dB clamp, unknown rate, chained clamp); NoticesText (no "License text missing", 3 licence blocks with real text, T-023 notice content incl. trademark line). |
| EditorControlMathTest.cpp | 15 | `behaviourFor`; `snapMilliseconds`; `roundToThreeSignificantFigures` (incl. NaN, Inf); drag for choice, slope, dB, avgTime, rmsWindow, frequency (grid, monotonicity, clamping, Shift); steps for choice, slope, dB, avg/rms ("at least one snap step", edges), frequency (2^(1/6), 2^(1/24), PageUp x5). |
| EditorViewStateStoreTest.cpp | 17 | Missing child, invalid root, empty child, one missing dimension, clamping, wrong-typed values (string, void, bool, memory block, array), double/int64, NaN/Inf/1e30 stay in limits, write creates the child, write clamps, no duplicate child, no-op write sends no notification, metersCollapsed types, write keeps size, XML round trip, unrelated children, and two end-to-end cases through `InvestigateProcessor` get/setStateInformation. |

One test-side correction: my first expectation for PageUp on frequency (35.7 Hz) and for a "pffft" string inside the licence file were wrong (20 x 2^(5/6) = 35.6; the licence text has "Dario Mambro" and "Redistribution", not "pffft"). Fixed in the tests; production code was right.

## 3. Results

Release and Debug, `ctest -C <cfg> -E pluginval` (pluginval and the VST3 build were not part of this run): 236 tests; 5 skipped (EBU Tech 3341/3342 authentic-programme cases "not run", pre-existing, unrelated); **3 failed**, identical in both configurations. Everything else passes, including all pre-existing T-005..T-010 tests. No compiler warnings in the new files at /W4.

| Test | Result |
|---|---|
| All other [gui] tests (62 of 65) | PASS |
| ViewStateStore: numeric strings as produced by XML parsing are read as numbers | **FAIL** |
| ViewStateStore: size and metersCollapsed persist through the processor's real state save and restore | **FAIL** |
| ViewStateStore: state survives an XML round trip and a replaced root | **FAIL** |

## 4. Defects

**D1 (high): editor size and metersCollapsed are lost after a host state save and restore.**
- Expected: T-011 acceptance "The size persists ... across a state save and restore"; ui-spec 2 Persistence; roadmap State format (tolerant read of `ViewState`).
- Actual: `getStateInformation` writes XML (PluginProcessor.cpp:363-364) and `setStateInformation` reloads it with `ValueTree::fromXml` (:387), which makes every property a **string** ("1432", "811", "1"). `ViewStateStore::isNumeric` (ViewStateStore.cpp:8-11) accepts only int, int64 and double, so `readDimension` (:13-20) returns the fallback and `readMetersCollapsed` (:55) returns false. After a real round trip, `readEditorSize` gives 1200x700 and `readMetersCollapsed` gives false instead of 1432x811 and true.
- Reproduce: `build/tests/Release/InvestigateUnitTests.exe "ViewStateStore: size and metersCollapsed persist through the processor's real state save and restore"` (also the other two tests above).
- Suspected fix: treat a `juce::var` string as numeric when it parses (`v.isString() && v.toString().containsOnly("0123456789-+.")` then `getIntValue`/`getDoubleValue`), and for the bool accept "0"/"1"/"true"/"false". The existing processor test only casts with `(int)`, which is why T-008 did not catch it.
- Related: the gui-engineer's own reasoning in the file header ("tolerates ... wrong type") is right to ignore garbage, but must not ignore XML-decoded numbers.

## 5. Other observations (not failures)
- ViewStateStore.cpp:19: `(int) v` on a NaN or huge double is undefined behaviour in the C++ sense (UBSan float-cast-overflow). On MSVC x64 it yields INT_MIN or similar and the clamp keeps the result inside limits (test passes). Low risk; a `std::isfinite` check would make it clean.
- No sanitizer or coverage preset was requested or run for this task. No real-time audit applies: everything tested is message-thread code.

## 6. Open questions
1. Should `ViewState` reads accept numeric strings (D1 fix), or should the processor normalise `ViewState` property types after `replaceState` (`ensureViewStateDefaults`)? Either fixes it; the architect should pick the owner (gui-engineer for ViewStateStore, plugin-engineer for the processor).
2. Is the 3 px highlight padding in ParamControl.cpp:253/269 meant to be a token (finding 1)?

## 7. Commands
```
cmake --preset windows-msvc
cmake --build build --config Release --target InvestigateUnitTests --parallel
cmake --build build --config Debug   --target InvestigateUnitTests --parallel
build/tests/Release/InvestigateUnitTests.exe "[gui]"
cd build && ctest -C Release -E pluginval --output-on-failure
cd build && ctest -C Debug   -E pluginval --output-on-failure -j4
```

## 8. Round 2 re-test (2026-10-02)

Verdict: **PASS (0 defects)**

- D1 fixed (ViewStateStore.cpp accepts numeric and bool strings from XML). The 3 previously failing ViewStateStore tests now pass.
- Token audit re-check: finding 1 resolved. ParamControl.cpp:253-254 and :269-270 use `L::halfHighlightPadY`. No new literal colours, font sizes or spacing found in source/gui/*.cpp. Findings 2-4 stay as accepted notes.
- `[gui]`: 65 test cases, 2839 assertions, all pass in Release and Debug.
- Full suite `ctest -C <cfg> -E pluginval`, run sequentially (Release, then Debug): 236 of 236 pass in each configuration. The 5 EBU "authentic programme" cases are skipped as before and are unrelated.
- The Debug build produced no warnings or errors.

Commands:
```
cmake --build build --config Release --target InvestigateUnitTests
build/tests/Release/InvestigateUnitTests.exe "[gui]"
cd build && ctest -C Release -E pluginval
cmake --build build --config Debug --target InvestigateUnitTests
build/tests/Debug/InvestigateUnitTests.exe "[gui]"
cd build && ctest -C Debug -E pluginval
```
