# T-058: Presets menu, reference slots, CSV import and export
- Owner: gui-engineer
- Milestone: M3c
- Status: TODO
- Depends on: T-057, T-068, T-070 (build order)

## Goal
Finish the M3c user-facing features:
- the header Presets menu over the settings preset store;
- four reference slots that take, show, offset and keep snapshots of the spectrum with the session;
- CSV export of the curves or a slot, and CSV import into a slot.

## Inputs
- `docs/design/ui-spec.md`: the M3c addendum from T-045 (the Presets menu, the reference panel, the reference curve style, the CSV flows and toasts), plus `mockup.html`.
- `docs/plans/m3c-feature-expansion.md`: design items [12], [13] and [14], the Storage summary, and the Budgets (CSV import ≤ 200 ms; session state ≤ 150 KB).
- The task logs of T-048 (`ReferenceGrid.h`, `SpectrumCapture`), T-052 (`requestSpectrumCapture`, `getSpectrumCapture`) and T-053 (`PresetStore`, `UserDataPaths.h`).
- `docs/research/T-043-span-m3c-reference.md`, question 7 (SPAN's CSV layout), and `docs/research/T-044-host-platform-constraints.md`, question 5 (`FileChooser` in hosts).
- `source/gui/` and `source/PluginEditor.*`.
- Working rules as in T-054.

## Scope
1. **The Presets menu**, per the spec:
   - Default; the user presets; Save as, with inline name entry and the validation messages from `PresetStore`; Overwrite; Delete, with confirmation; Open presets folder;
   - the current name, with a modified mark from `isModified()`;
   - loading never changes colours, `ViewState` or the zoom.
2. **Reference slots.**
   - Four slots: name (at most 16 characters, default "Ref 1" to "Ref 4"), visibility, an offset from −40 to +40 dB, Take (primary), Take 2nd (secondary), Clear.
   - Take requests a capture and stores the chosen curve on the reference grid. While frozen it stores the frozen data.
   - The editor draws each visible slot through the view mapping, adding the current slope and the slot's offset, in its spec style and role colour. "No data" points are skipped.
   - The slots persist in the `ViewState` child `ReferenceSlots`, as base64 float arrays.
   - The slots are not part of settings presets.
3. **`source/gui/CsvSpectrum`** (pure, unit-tested):
   - **The writer:**
     - writes `#` header lines with the product version, sample rate, block size, window, overlap, smoothing, slope, Align, the channel mode and the underlay;
     - then a header row, `frequency_hz` plus one column per curve;
     - "." decimals and "," separators.
   - **The parser:**
     - accepts comma, semicolon or tab separators, and a "," decimal comma with semicolons;
     - skips `#` lines and non-numeric header lines;
     - reads the first column as the frequency in Hz (above 0; sorted if needed) and the second as the level in dB;
     - reads at most 65,536 rows;
     - resamples onto the reference grid with log-frequency interpolation and no extrapolation;
     - reports errors with a line number.
4. **CSV actions:**
   - "Export curves" writes every visible live curve from a capture.
   - "Save CSV" and "Load CSV" work per slot.
   - Files are chosen through `juce::FileChooser::launchAsync`.
   - A load error shows the spec's toast and leaves the slot unchanged.

## Acceptance criteria
- [ ] **Presets UI.** Save as, load, overwrite and delete work end to end in the Standalone. Invalid names show the reason. The modified mark follows parameter changes. Loading never changes colours, slots, labels or the editor size.
- [ ] **Slots.**
  - Take, then a changed input: the slot keeps its curve.
  - The offset and the visibility work.
  - Take while frozen stores the frozen curve.
  - A state round trip restores all four slots within 0.001 dB.
  - The session state with four full slots is ≤ 150 KB.
- [ ] **CSV round trip.** Export a slot, then import it into another slot: equal within 0.01 dB at every grid point with data.
- [ ] **CSV parser** unit tests:
  - comma, semicolon with decimal comma, and tab files;
  - a SPAN-style linear-frequency file;
  - unsorted rows;
  - a file with only a header;
  - a non-numeric row in the middle (an error with its line number);
  - more than 65,536 rows (only the first 65,536 read, with a warning);
  - an empty file.
- [ ] **CSV import** of 65,536 rows takes ≤ 200 ms on the message thread (`[.perf]`).
- [ ] **Budgets** with four visible references plus every live layer, at a 60 fps cap, median of 3: whole-editor message thread ≤ 25 % at 1280×720 and 2560×1440, and paint p95 ≤ 4 ms and ≤ 8 ms.
- [ ] Warning-clean at `/W4`. `docs/qa/T-058-unit.md` and `docs/qa/T-058-validation.md` report PASS.

## Log
- 2026-10-03 vst-architect: Created from `docs/plans/m3c-feature-expansion.md`.
- 2026-10-06 vst-architect: **Amended by `docs/plans/m3-review-spectrum-slots.md`** (review findings #21 and #22):
  - **A/B (#22, now in v1).** Add the A, B and "Copy A→B" controls per T-065, wired to `ABStateStore` (T-068): switch, copy, and an indicator of the active letter. Switching applies every parameter as T-068 defines, without freezing the UI (≤ 50 ms). A settings preset load affects the active state only.
    - Criteria: switching restores every parameter of the other state exactly; the active letter and both states survive a state round trip; undo in the Standalone treats one switch as one step, or T-068 documents why not.
  - **#21 is cancelled.** No factory presets, no "User" label, and no panel visibility in presets. The existing Presets menu scope (Default, the user presets, Save as, Overwrite, Delete, Open folder, and the "modified" mark) is unchanged.
  - **References.** "Take" can capture **any slot** (T-048), chosen per T-065, replacing "Take" and "Take 2nd".
  - **CSV.** The reference grid now reaches 96 kHz (T-048). Export writes one column per visible slot, named by the slot name. The import is unchanged.
  - **Build order.** This task follows T-070, and needs T-068.
