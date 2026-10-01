# T-011 validation (plugin-validator)

Verdict: PASS (0 defects)

## Environment
- Windows 11 (10.0.26200), MSVC VS2022 x64, JUCE 8 pinned, commit beb8834 plus the uncommitted T-011 working tree (editor in `source/PluginEditor.*`, `source/gui/`).
- Release: `Investigate.vst3`, Standalone and `InvestigateEditorCheck`. Debug: `InvestigateEditorCheck` only (JUCE leak detector is active in Debug).
- Display present (the editor is added to the desktop, so VBlankAttachment and the message loop run). pluginval ran with GUI tests on.

## Method
`tools/render-harness/editor_check.cpp` (new target `InvestigateEditorCheck`, added to the harness CMakeLists; it needs `-DPLUGIN_BUILD_HARNESS=ON`, which I restored to OFF afterwards). It uses only `createEditor()`, `createComponentSnapshot()` and the APVTS. It finds child controls by walking the component tree. Layout is measured from pixels of the rendered snapshot, with the token colours as the oracle, not from the editor's layout code. Expected values are from ui-spec section 4.

Commands:
```
cmake -S . -B build -DPLUGIN_BUILD_HARNESS=ON
cmake --build build --config Release --target InvestigateEditorCheck Investigate_VST3
cmake --build build --config Debug --target InvestigateEditorCheck
build/tools/render-harness/Release/InvestigateEditorCheck.exe docs/qa 50
build/tools/render-harness/Debug/InvestigateEditorCheck.exe <tmpdir> 50
third_party/pluginval/1.0.4/pluginval.exe --strictness-level 10 --validate build/Investigate_artefacts/Release/VST3/Investigate.vst3 --output-dir <dir>
```

## Results
| # | Check | Result |
|---|---|---|
| 1 | Screenshots saved: `docs/qa/editor-1200x700.png`, `editor-920x540.png`, `editor-1200x700-collapsed.png` | PASS |
| 1 | Region edges vs spec section 4 (+-2 px) at 1200x700, 920x540 and 1200x700 collapsed | PASS, all 0 px deviation (table below) |
| 2 | 50 open/close cycles, Release | PASS, no crash (2.3 s) |
| 2 | 50 open/close cycles, Debug, JUCE leak detector | PASS, exit 0, no leak assertion. Cycles mix on-desktop and off-desktop editors, resizes and Settings popover opens |
| 3 | Default size 1200x700 | PASS |
| 3 | Size 1500x900 and metersCollapsed persist across editor reopen | PASS |
| 3 | Same across getStateInformation, then setStateInformation on a fresh processor | PASS |
| 3 | Restore clamp: 100x100 gives 920x540; 9999x9999 gives 2560x1600; 919x1601 gives 920x1600; 2561x539 gives 2560x540 | PASS |
| 3 | Live (host) resize via constrainer: 100x100 gives 920x540; 5000x5000 gives 2560x1600 | PASS |
| 3 | ViewState written on every resize (1234x777 seen in state) | PASS |
| 4 | 13 float and choice params: control value equals the parameter after a message-thread set (2 controls each: control bar and Settings) | PASS |
| 4 | Same after a set from a non-message thread (host style), checked after a 17 ms pump (about one frame) | PASS |
| 4 | channelMode 0..6 reflected by the Mode selector. showSidechain reflected by the SC toggle | PASS |
| 5 | pluginval 1.0.4, strictness 10, Release VST3 (includes Editor, Open editor whilst processing, Editor Automation, Fuzz parameters) | PASS (SUCCESS, exit 0) |
| 6 | Freeze auto-unfreezes on a host channelMode change (Freeze button state follows); stays on when the same value is re-set; unfreezes on a second change | PASS |

Total of the editor check: 121 checks, 0 failed, in both Release and Debug.

### Region measurements (px; measured = spec in every row)
| Edge | 1200x700 | 920x540 | 1200x700 collapsed |
|---|---|---|---|
| Header bottom border y | 43 | 43 | 43 |
| Plot top / bottom (exclusive) | 44 / 644 | 44 / 484 | 44 / 644 |
| Plot right (exclusive) | 904 | 624 | 1200 |
| Control bar top border y | 644 | 484 | 644 |
| Meter strip left border x | 904 | 624 | absent |
| Data rect frame L/T/R/B | 48/56/887/619 | 48/56/607/459 | 48/56/1183/619 |
| Chip x positions | 12 80 156 240 320 408 508 624 772 | 12 80 156 240 320 408 508 (C hidden) | 12 80 156 240 320 408 508 920 1068 |

These match spec 4.3's resolved-position table exactly. The Settings dot is visible at 920x540 and absent at 1200x700.

## Not covered or noted (not defects)
- Meter bars, table, loudness and correlation positions (spec 4.4 table) are not checked. MeterPanel and SpectrumView are placeholders until T-012 and T-013. The strip chrome edge and the Scale, RMS and Reset chips are present.
- The Level chip renders "-90 ... +6 dB" with an ASCII hyphen-minus. The spec text writes U+2212. Raise with gui-engineer if the typographic minus is wanted. I did not treat it as a spec violation, because the rendered glyph is not checked in the spec's acceptance criteria.
- The leak check relies on the JUCE leak detector's assertion at shutdown (no output stream). Exit code 0 means no assertion fired.
- The `InvestigateEditorCheck` pump uses `PeekMessage` and `DispatchMessage`, so it is Windows-only. macOS needs a different pump if the check is ported.

## Defects
None.

## Open questions
1. build-engineer: consider turning `PLUGIN_BUILD_HARNESS` on in CI or a preset so `InvestigateEditorCheck` and the existing render harness run there. No root CMake change is needed beyond that.
2. gui-engineer or architect: confirm whether the Level chip should use U+2212 in "-90".

## Files
- `docs/qa/T-011-validation.md`
- `docs/qa/editor-1200x700.png`, `docs/qa/editor-920x540.png`, `docs/qa/editor-1200x700-collapsed.png`
- `tools/render-harness/editor_check.cpp`, `tools/render-harness/CMakeLists.txt` (added target)
