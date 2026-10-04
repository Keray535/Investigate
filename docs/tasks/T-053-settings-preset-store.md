# T-053: Settings preset store
- Owner: plugin-engineer
- Milestone: M3c
- Status: TODO
- Depends on: T-052

## Goal
Let the user save every analysis and metering setting as a named preset and load it back. A preset stores the parameter values only. Colours, editor layout, reference slots and display preferences are never part of it. The editor's Presets menu (T-058) uses this store.

## Inputs
- `docs/plans/m3c-feature-expansion.md`: design item [14] and the Storage summary.
- `docs/research/T-044-host-platform-constraints.md`, question 4: the `%APPDATA%` conventions.
- `source/params/` and `source/PluginProcessor.*`.
- Roadmap Decision of 2026-10-03: `source/presets/` is a new directory owned by plugin-engineer.
- Working rules as in T-051.

## Scope
1. **`source/presets/UserDataPaths.h`.** One place that defines the user data folder (`%APPDATA%\Investigate Audio\Investigate\`) and its subfolders (`Presets\Settings\`, `Presets\Colours\`) and the preference file name. gui-engineer reuses it for `GlobalSettings` and colour presets (read-only include). The company folder name follows the roadmap Identity section, which may change before M5.
2. **`source/presets/PresetStore`** (message thread only):
   - list the presets, sorted by name, case-insensitive;
   - `save (name)`: refuses an empty name, a name with characters invalid for Windows file names, or a name longer than 64 characters, with a reason the UI can show;
   - `overwrite (name)` and `remove (name)`;
   - `load (name)`;
   - a built-in "Default" that resets every parameter to its default (never a file).
3. **The file format:**
   - XML root `InvestigatePreset`, with the attributes `name`, `productVersion` and `stateVersion`;
   - one `PARAM` element (`id`, `value`) per APVTS parameter, with plain, unnormalised values, written with enough digits to round-trip a float exactly.
4. **Loading:**
   - unknown IDs are ignored, missing IDs take their default, and values are clamped to range;
   - each parameter is set with `beginChangeGesture`, `setValueNotifyingHost` and `endChangeGesture`;
   - a corrupt or unreadable file changes nothing and returns an error.
5. **Dirty tracking:** `getCurrentPresetName()` and `isModified()`, which compares the current parameter values with the loaded preset within 1e-6 normalised. The name is kept in memory only, not in the session.
6. Writes go through a temporary file and a rename, so a crash never leaves a half-written preset.

## Acceptance criteria
- [ ] **Round trip.** Randomise every parameter, save, reset to Default, load: every value matches within 1e-6 normalised. Run it 20 times.
- [ ] **Exclusions.** Loading a preset never changes `ViewState` (size, collapse, sections, labels, reference slots) or any file outside `Presets\Settings\`.
- [ ] **Robustness.**
  - A corrupt file, an empty file, a file with unknown IDs, and a file missing half its parameters each behave as specified: no crash, and no partial application for the corrupt case.
  - Invalid names are refused with a reason.
  - A missing folder is created on the first save.
- [ ] **Host gestures.** Loading produces one begin, set and end sequence per parameter (tested with a gesture-recording listener). It takes ≤ 50 ms for the full parameter set (`[.perf]`).
- [ ] `isModified()` turns true after any parameter change, and false after loading or saving.
- [ ] Tests use a temporary folder, never the real `%APPDATA%`. Warning-clean at `/W4`. `docs/qa/T-053-unit.md` reports PASS.

## Log
- 2026-10-03 vst-architect: Created from `docs/plans/m3c-feature-expansion.md`. The VST3 program list stays unused (`getNumPrograms()` stays 1).
