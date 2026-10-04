# T-046: Spectrum engine: underlay model and a second sidechain input (additive)
- Owner: dsp-engineer
- Milestone: M3c
- Status: TODO
- Depends on: T-043, T-038 (build order: one C++ task at a time)

## Goal
Generalise the spectrum pipeline from fixed dual modes to SPAN's model: a primary channel combination plus an optional underlay combination. Add a second sidechain input with its own curve. Every change is additive: existing APIs, settings and frame fields keep their meaning, so the current processor and editor build and behave exactly as before until T-052 and T-055 switch over.

## Inputs
- `docs/plans/m3c-feature-expansion.md`:
  - design items [4] (channel model and the legacy alias table) and [7] (three inputs);
  - the Budgets;
  - the Parameter table: the `underlay` choice order is Off, Mid, Side, Left, Right, Stereo.
- `docs/research/T-043-span-m3c-reference.md`, question 6. If it changes what "Stereo" means (power mean, sum or maximum), follow the roadmap Decisions entry that records it.
- `source/dsp/SpectrumAnalyser.*`, `SpectrumSettings.h`, `SpectrumEngine.*`, `SpectrumMapper.*`, `SpectrumFrame.h` and `SampleFifo.*`, with the T-004, T-005, T-035 and T-039 task logs.
- Working rules from the roadmap handoff:
  - one C++ task at a time;
  - timing assertions only in `[.perf]`;
  - `ctest --timeout 300`;
  - build in `build/` through the presets;
  - coders never edit `docs/qa/`.

## Scope
1. **`SpectrumSettings` and `SpectrumAnalyser::Settings`:**
   - add `underlay` (an enum: off, mid, side, left, right, stereo; default off);
   - add `numSidechain2Channels` (default 0).

   The existing `channelMode` field keeps its 7 values. The analyser resolves the effective pair: L/R dual gives Left plus Right, M/S dual gives Mid plus Side, and in both cases `underlay` is ignored. Otherwise the pair is `channelMode` plus `underlay`. An underlay equal to the primary counts as Off. With a mono main input there is no underlay.
2. **The analyser** computes the primary and, when present, the underlay. Each has its averaged power spectrum and its secondary accumulator, as the dual modes do today. Its secondary accumulators reset when the **effective** primary or underlay changes, as well as on today's triggers.
3. **Second sidechain:**
   - `SpectrumEngine::prepare` gains an overload with a fourth channel count (`numSidechain2Channels`), and the existing signature keeps working (0 channels);
   - a new FIFO and `pushSidechain2 (const float* const*, int, int) noexcept` for the audio thread;
   - the Sidechain 2 curve follows the same combination rule as Sidechain 1: the primary combination with no underlay, and the Stereo power mean with one.
4. **`SpectrumFrame`:**
   - keeps `primaryBDb`, `secondaryBDb` and `isDual`, which now mean "the underlay curve, its secondary curve, an underlay is present";
   - keeps `sidechainDb` and `hasSidechain` for Sidechain 1;
   - adds `sidechain2Db` (capacity `maxPoints`) and `hasSidechain2`;
   - update the size comment.
5. **The freeze snapshot and the re-map path (M3b)** include the Sidechain 2 curve. Preallocate the snapshot in `prepare` for the largest bin count.
6. Comments cite the plan and the SPAN source.

## Acceptance criteria
- [ ] **No regression.**
  - With `underlay` off and `numSidechain2Channels` 0, every frame is bit-identical to the current build, for every `channelMode` value (including both legacy dual values) and every secondary mode, at Fit and zoomed.
  - Every existing test, including the T-009, T-012, T-035, T-036 and T-039 tests, passes unchanged in Debug and Release.
- [ ] **Equivalence.** Left plus underlay Right produces frames bit-identical to legacy L/R dual. Mid plus underlay Side produces frames bit-identical to legacy M/S dual. Tested for every secondary mode.
- [ ] **New pairs** (for example Stereo plus Side, or Left plus Mid) produce the correct curves against a reference computed from separate single-mode runs, within 0.01 dB.
- [ ] **Sidechain 2.** It produces a curve equal to Sidechain 1's within 0.01 dB when fed the same signal. `hasSidechain2` follows its channel count. Its FIFO overflow behaviour matches the other FIFOs: drop, count, never block or allocate.
- [ ] **Freeze.** While frozen, a view change re-maps all curves, including Sidechain 2, from the snapshot.
- [ ] **Budgets:**
  - a worst-case re-map (underlay, both secondary curves, two stereo sidechains, 1/3-octave smoothing, minimum span) takes ≤ 1.0 ms median on the analysis thread (`[.perf]`);
  - analysis CPU in the worst case (65536, 8x overlap, underlay, both secondary curves, two stereo sidechains, 192 kHz) is ≤ 10 % of one core (`[.perf]`).
- [ ] `processPending` makes 0 allocations after `prepare` when no `fftSize`, window or layout change occurs (allocation hook). `pushSidechain2` is real-time safe.
- [ ] Warning-clean at `/W4`. `docs/qa/T-046-unit.md` reports PASS.

## Log
- 2026-10-03 vst-architect: Created from `docs/plans/m3c-feature-expansion.md`. Additive by design: T-052 (processor) and T-055 (editor) switch over later, so do not change the meaning of any existing field or API.
