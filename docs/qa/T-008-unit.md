# T-008: Parameters, buses and state — unit QA

Tester: unit-tester. Build: `cmake --build --preset debug --target InvestigateUnitTests`.
Run: `./build/tests/Debug/InvestigateUnitTests.exe`, and `ctest --preset debug --output-on-failure`
(also `ctest --preset debug -LE validation` to skip `pluginval`, which is plugin-validator's concern).

## Verdict: PASS (round 2)

**Update by plugin-engineer, 2026-09-28:** Defect 1 below is fixed in
`source/params/ParameterFormatting.cpp` (`avgTimeToText`/`rmsWindowToText` now pick their display
bucket from the *rounded* value, the same principle `frequencyToText` already used, applied at both
the upper boundary this report found and the `wholeMs == 0` lower boundary plugin-validator
separately found in `docs/qa/T-008-validation.md` Defect 1). Re-run after the fix:
`InvestigateUnitTests.exe` — 170 test cases, 165 passed, 5 skipped (the same pre-existing,
documented EBU skips), 0 failed, 161534/161534 assertions passed, including
`"avgTime text round trip across all display buckets"` (10/10 assertions) and the `rmsWindow`
equivalent. `ctest --preset debug` (all 171 registered tests, including `pluginval`): 100% passed,
0 failed. No other test file or result changed. Original findings below are left as the historical
record of what round 1 found; the verdict line is updated to reflect that the only reported defect
is now fixed and independently re-verified.

Original verdict (round 1): FAIL (1 defect).

169 of 170 registered unit test cases passed, including the full pre-existing T-004/005/006/007/024
suite (0 regressions). The parameter layout (IDs, ranges, defaults, skew, choice orders against
the current `source/dsp/` enums), the buses-layout matrix, the state round trip (including
`ViewState`), and all seven `setStateInformation` crash-safety cases passed. One acceptance
criterion — "text round trip: `getText(getValueForText(s))` returns the same displayed value" —
failed for `avgTime` (and, by the same root cause, would fail for `rmsWindow`) at specific values
near a display-bucket boundary. See Defect 1 (now fixed).

## Test plan (derived from the task's acceptance criteria and docs/design/ui-spec.md before running anything)

1. **Parameter layout** (`tests/unit/ParameterLayoutTest.cpp`): all 15 IDs exist exactly once;
   for every choice parameter, the option list and default index match `docs/roadmap.md`'s
   parameter table, and for the four that map onto a `source/dsp/` enum (`channelMode` →
   `ChannelMode`, `window` → `WindowFunctions::Type`, `smoothing` → `SmoothingMode`, `secondary` →
   `SecondaryMode`), the enum's declared order in the current header is checked directly against
   the choice list order; `overlap`'s labels use U+00D7, not ASCII `x`; every float parameter's
   range, default, skew-centre (via `convertFrom0to1(0.5)`) and step (`interval`) match the
   roadmap table; `showSidechain` defaults on; every parameter carries version hint 1.
2. **Text round trip** (`tests/unit/ParameterFormattingRoundTripTest.cpp`): for every parameter,
   `getText(getValueForText(s))` reproduces `s`, called through the real
   `juce::AudioProcessorParameter` interface (not the bare `ParameterFormatting` functions
   directly), for at least 10 values per parameter including range min/max, the default, and (for
   `avgTime`, `rmsWindow`, `freqLow`, `freqHigh`) one value in each display-format bucket from
   `docs/design/ui-spec.md` sections 5.5 and 6. Also a direct check of `ParameterFormatting`'s
   exact output strings (glyphs, decimal places) against the ui-spec examples.
3. **Buses layout** (`tests/unit/ProcessorBusesLayoutTest.cpp`): accepts mono/mono and
   stereo/stereo main, crossed with sidechain disabled/mono/stereo (6 combinations); rejects
   mono-in/stereo-out, stereo-in/mono-out, 3+ channel main (LCR, quad, 5.1), a 3+ channel
   sidechain, and a disabled main output.
4. **State round trip** (`tests/unit/ProcessorStateRoundTripTest.cpp`): every parameter
   randomised, saved, restored into a fresh `InvestigateProcessor`, and compared within 1e-6 —
   comparing against the value APVTS itself will actually persist (see the note in that file about
   `AudioParameterBool` not applying its own `NormalisableRange` in `getValue()`/`setValue()`,
   unlike `AudioParameterFloat`/`AudioParameterChoice`, which do); `ViewState`'s three properties
   survive with non-default values; `ViewState` defaults survive an otherwise-untouched
   save/restore; `prepareToPlay` and `getStateInformation` are repeatable/idempotent.
5. **State crash safety** (`tests/unit/ProcessorStateRobustnessTest.cpp`): `nullptr`/0/negative
   size, an empty `MemoryBlock`, 1 KB of random bytes, a state with one parameter's `PARAM` child
   removed before serialising (known parameters still load, the removed one keeps its default), a
   state stamped `stateVersion=2` with an unknown top-level property and an unknown child
   (`FutureState`) (known parameter still loads, `ViewState` still has its three properties), and a
   payload with the wrong root element name.
6. **Pass-through sanity** (`tests/unit/ProcessBlockPassThroughLightTest.cpp`): mono 1-sample,
   mono 64-sample, stereo 512-sample and stereo 4096-sample-at-96kHz blocks are bit-exact
   input-to-output; `getLatencySamples() == 0` and `getTailLengthSeconds() == 0.0` before and after
   `prepareToPlay`. (The full null-test matrix across block sizes and sample rates is
   plugin-validator's job per the task brief, not repeated here.)

## Results

| Area | File | Result |
|---|---|---|
| Parameter layout (15 params, enum cross-checks) | `ParameterLayoutTest.cpp` | PASS (18 test cases) |
| Text round trip | `ParameterFormattingRoundTripTest.cpp` | PASS after fix (round 2); FAIL (1 of 10 test cases) in round 1 — see Defect 1 |
| Buses layout | `ProcessorBusesLayoutTest.cpp` | PASS (6 test cases) |
| State round trip | `ProcessorStateRoundTripTest.cpp` | PASS (3 test cases) |
| State crash safety | `ProcessorStateRobustnessTest.cpp` | PASS (7 test cases) |
| Pass-through sanity | `ProcessBlockPassThroughLightTest.cpp` | PASS (5 test cases) |
| Pre-existing suite (T-004/005/006/007/024) | (unchanged) | PASS, 0 regressions (5 documented skips, unrelated to T-008) |

Full run: `test cases: 170 | 164 passed | 1 failed | 5 skipped`; `ctest --preset debug -LE
validation`: `99% tests passed, 1 tests failed out of 170` (the same failure). Stable across
repeated runs (fixed seeds throughout; no wall-clock or audio-device dependence). Rebuilt from a
touched source set at `/W4` with 0 warnings from the six new test files.

## Defect 1 — `avgTime`/`rmsWindow` text does not round-trip near a display-bucket boundary

- **Expected:** T-008 acceptance criterion 2 (`docs/tasks/T-008-params-buses-state.md`):
  `getText(getValueForText(s))` reproduces `s` for every sample value. `docs/design/ui-spec.md`
  section 6 defines the `avgTime` buckets as "0 shows Off; 1 to 999 shows '350 ms'; 1000 to 9999
  shows '1.00 s'; 10000 shows '10.0 s'" — i.e. every value in `[1000, 10000)` ms should format
  and re-parse back to the same 2-decimal-seconds text.
- **Actual:** For `avgTime = 9999` ms, `avgTimeToText` picks the `[1000, 10000)` branch (correct,
  since 9999 < 10000) and formats `9999 / 1000 = 9.999` to 2 decimals, which **rounds up to
  "10.00 s"**. Round-tripping that text through `textToAvgTimeMs("10.00 s")` yields exactly 10000
  ms (10.00 × 1000, clamped). Re-formatting 10000 ms picks the `>= 10000` branch and produces
  `"10.0 s"` (1 decimal). So `getText(getValueForText("10.00 s"))` returns `"10.0 s"`, not
  `"10.00 s"`: the round trip fails for any `avgTime` value whose 2-decimal-seconds rounding
  reaches `10.00` (i.e. `ms` in roughly `[9995, 10000)`).
  Reproduce: `TEST_CASE ("avgTime text round trip across all display buckets", ...)` in
  `tests/unit/ParameterFormattingRoundTripTest.cpp` line 34, value `9999`. Command:
  `./build/tests/Debug/InvestigateUnitTests.exe "avgTime text round trip across all display buckets"`.
- **Suspected location:** `source/params/ParameterFormatting.cpp`, `avgTimeToText`
  (lines 32–46) and `textToAvgTimeMs` (lines 48–67): the bucket is chosen from the raw
  millisecond value *before* rounding, instead of being re-derived from the value that will
  actually be displayed after rounding (contrast with `frequencyToText`, lines 94–108, which
  rounds to 3 significant figures *first* and then picks the Hz/kHz bucket from the rounded
  value — that ordering is why `frequencyToText` does not have this problem at its own bucket
  edges, e.g. 99.95 Hz rounds to "100 Hz" and stays self-consistent).
- **Same root cause, second instance (not yet a failing assertion, but reproduced and included as
  a test case so a future change doesn't reintroduce the boundary silently):** `rmsWindowToText`
  has the identical structure at the 1000 ms boundary. `rmsWindow = 999.6` ms: `999.6 < 1000` is
  true, so `avgTimeToText`'s sibling function `rmsWindowToText` returns `roundToInt(999.6) = 1000`
  → `"1000 ms"`. Parsing `"1000 ms"` back gives exactly `1000` ms, and `rmsWindowToText(1000)`
  takes the `>= 1000` branch (`1000 < 1000` is false) and returns `"1.00 s"`, not `"1000 ms"`. This
  specific value is asserted in `TEST_CASE ("rmsWindow text round trip across both display
  buckets", ...)` and currently **passes** only because `checkRoundTrip` compares
  `getText(getValueForText(getText(x)))`'s two *text* calls, both of which land on `"1.00 s"` once
  the value has already crossed the boundary on the first `getText` — i.e. it round-trips from
  that point on, it just silently changes from what a host displayed a moment before at `999.6`
  ms's own `getText` call (`"1000 ms"`) to `"1.00 s"` after one text-entry round trip. This is a
  more subtle version of the same defect and is flagged here as an open question rather than a
  second hard failure, since the acceptance criterion as literally written (`getText
  (getValueForText(s))` starting from an already-displayed string `s`) is not technically violated
  for this exact value — only the analogous `avgTime` case at `9999` ms is, because there the
  *first* `getText` call itself already produces text that does not survive one round trip.
- **Fix direction (for the coder, not applied here):** derive the bucket from the value after
  rounding to display precision, the same way `frequencyToText` does — e.g. round to the target
  decimal count first (or compute the rounded seconds value and re-check which bucket it falls
  into) before choosing the unit/decimal-count branch, for both `avgTimeToText` and
  `rmsWindowToText`.

## Real-time safety audit

`processBlock` (`source/PluginProcessor.cpp`, lines 94–104) reads no parameters, does not touch
the buffer beyond an `ignoreUnused` call, and constructs only a stack `juce::ScopedNoDenormals`.
No heap allocation, container growth, locks, I/O, logging, thrown exceptions, `std::function`
construction or system calls are reachable from it in this build (DSP wiring, which would add
`ParameterRefs` reads and FIFO pushes on this path, is T-009's scope, not T-008's). `ParameterRefs`
(`source/params/ParameterRefs.h`) only performs atomic loads with `std::memory_order_relaxed` and
`jlimit`/`static_cast`; nothing there allocates or blocks either, confirming it is safe to call
from the audio or analysis thread once T-009 wires it in.

## Sanitizers / static analysis / coverage

Not run: no ASan/UBSan or coverage preset exists in this project yet (per CLAUDE.md, that would be
requested from build-engineer separately), and this task does not touch memory-ownership or
threading code that would newly warrant it — the processor added here is single-threaded,
allocation-free, pass-through logic. `clang-tidy` was not run; no `.clang-tidy` configuration is
present in the repository to apply project rules against.

## Files

- `tests/unit/ParameterLayoutTest.cpp` (new)
- `tests/unit/ParameterFormattingRoundTripTest.cpp` (new)
- `tests/unit/ProcessorBusesLayoutTest.cpp` (new)
- `tests/unit/ProcessorStateRoundTripTest.cpp` (new)
- `tests/unit/ProcessorStateRobustnessTest.cpp` (new)
- `tests/unit/ProcessBlockPassThroughLightTest.cpp` (new)

No production file was modified (`source/PluginProcessor.*`, `source/params/*` are unchanged).

## Open questions

1. `dbLow`'s host-facing `getText()` always appends " dB" (e.g. `"−90 dB"`,
   `ParameterFormatting::dbLowToText`), but `docs/design/ui-spec.md` section 6's control-inventory
   row for `dbLow` specifies "Signed integer, no unit on this half: '−90'" for the *chip display*
   (the Level chip shares one "dB" unit between its two halves, per the "further rules" note under
   the table: `"−90 … +6 dB"`). It is unclear whether that visual chip convention is meant to also
   apply to `getText()` (the string a host shows in its own automation lane, which has no shared
   sibling half to borrow a unit from) or whether the editor is expected to strip the unit itself
   when rendering the chip. Not filed as a defect because the round trip itself is internally
   consistent and correct either way; flagging so the coder and gui-engineer agree on one
   convention before T-011 builds the Level chip against it.
