# T-046: Spectrum engine: spectrum slots and a second sidechain input (additive)
- Owner: dsp-engineer
- Milestone: M3c
- Status: TODO
- Depends on: T-043, T-063, T-066 (build order: one C++ task at a time)

## Goal
Generalise the spectrum pipeline into four identical spectrum slots. Each slot has a source (Main, Sidechain 1 or Sidechain 2), a channel combination, a type (Off, Real-time, RT max, All-time max, All-time avg), and its own analysis settings or a link to slot 1. Add a second sidechain input.

Every change is additive. Existing APIs, settings and frame fields keep their meaning, so the current processor and editor build and behave exactly as before until T-052 and T-055 switch over.

(Rewritten on 2026-10-06. The original underlay design is superseded; the file name is kept for history.)

## Inputs
- `docs/plans/m3-review-spectrum-slots.md`, which wins over the M3c plan:
  - Architecture: "Spectrum slot model", "Signal flow" and "Processor API additions";
  - Budgets;
  - the Parameter changes table (choice orders for Source, Channel and Type).
- `docs/plans/m3c-feature-expansion.md`, item [7] (three inputs, buses). Item [4] is superseded.
- `docs/research/T-043-span-m3c-reference.md`, question 6 (what "Stereo" means: power mean is the current decision).
- `source/dsp/SpectrumAnalyser.*`, `SpectrumSettings.h`, `SpectrumEngine.*`, `SpectrumMapper.*`, `SpectrumFrame.h` and `SampleFifo.*`, with the T-004, T-005, T-035, T-039, T-062 and T-063 task logs.
- Working rules from the roadmap handoff:
  - one C++ task at a time;
  - timing assertions only in `[.perf]`;
  - `ctest --timeout 300`;
  - build in `build/` through the presets;
  - coders never edit `docs/qa/`;
  - after a session, check for leftover build processes.

## Scope
1. **A `SpectrumSlot` type** (or an equivalent structure inside the engine), with one settings struct per slot:
   - `source` (main, sidechain1, sidechain2);
   - `channel` (mid, side, left, right, stereo);
   - `type` (off, realTime, rtMax, allTimeMax, allTimeAvg);
   - `link` (bool);
   - `fftSize`, `overlap`, `window`, `avgTime`, `smoothing` and `slope`, which are used when `link` is off.

   Slot 1 is locked to main and realTime.
2. **The engine** holds 4 slots.
   - Each input FIFO (main, SC1, SC2) is drained once per `processPending` call, and its samples are fed to every active slot whose source is that input.
   - **Sharing.** Slots with the same source, channel and effective analysis settings share one analyser instance, and keep only their own type accumulator. Document the sharing key. At the defaults (slot 2 = Main, Mid, RT max, linked), slots 1 and 2 must cost what the primary and secondary curves cost today.
   - A slot with type off, or with a missing source, costs nothing.
3. **Second sidechain:**
   - a `prepare` overload with a fourth channel count (`numSidechain2Channels`); the existing signature keeps working with 0 channels;
   - a new FIFO and `pushSidechain2 (const float* const*, int, int) noexcept`.
4. **`SpectrumFrame`**, additive:
   - per-slot curves (indices 1 to 4) with capacity `maxPoints`;
   - per-slot flags: active, type, source present;
   - per-slot `binHz`;
   - the existing fields (`primaryDb`, `secondaryDb`, `sidechainDb`, the flags) keep being filled exactly as today, so the current editor works unchanged.
   - Keep the frame within about 150 KB, and update the size comment.
5. **Legacy mapping, in the compatibility path.** The existing `SpectrumSettings` and `channelMode` input keep producing today's frames:
   - `channelMode` with `secondary` gives slot 1 plus slot 2 (linked, Main);
   - the sidechain gives slot 3 (linked, Stereo when the main mode is dual, otherwise the main combination, as today).

   The legacy dual values keep producing today's dual frames through the old fields until T-052 switches over. The slot API resolves L/R to Left and M/S to Mid.
6. **The freeze snapshot and the re-map path (M3b)** cover every active slot. Preallocate the snapshot in `prepare` for the largest bin count and four slots.
7. **Resets.** A slot's accumulators reset when its source, channel or effective analysis settings change, and on today's triggers. `resetSecondary()` clears every slot.
8. Comments cite the plan.

## Acceptance criteria
- [ ] **No regression.**
  - Through the existing API (no slot settings given, `numSidechain2Channels` 0), every frame is bit-identical to the current build: for every `channelMode` value (including both legacy dual values) and every secondary mode, at Fit and zoomed.
  - Every existing test, including the T-009, T-012, T-035, T-036, T-039, T-062 and T-063 tests, passes unchanged in Debug and Release.
- [ ] **Slot equivalence.**
  - At the default slot settings, slot 1 and slot 2 through the slot API equal today's primary and secondary curves bit for bit.
  - Slot 3 (SC1, linked) equals today's sidechain curve bit for bit.
- [ ] **Independent settings.**
  - An unlinked slot with a different `fftSize`, window, overlap, avgTime, smoothing or slope produces the curve of a separate single-slot run with those settings, within 0.01 dB.
  - The `binHz` of each slot matches its own `fftSize`.
- [ ] **Sources and channels.** Every slot works with each of the three sources and each of the five channels. Each combination matches a reference single run within 0.01 dB. A mono source gives its single channel for every channel choice.
- [ ] **Types.** Real-time, RT max, All-time max and All-time avg match today's primary and secondary semantics (`avgTime` conventions from the roadmap).
- [ ] **Sharing.** Two slots with an identical sharing key run one FFT per hop (counted by a test hook). Changing one setting splits them.
- [ ] **Sidechain 2.** It produces the same curve as Sidechain 1 within 0.01 dB when fed the same signal. Its source-present flag follows its channel count. Its FIFO overflow behaviour matches the other FIFOs: drop, count, never block or allocate.
- [ ] **Freeze.** While frozen, a view change re-maps every active slot from the snapshot.
- [ ] **Budgets** (`[.perf]`):
  - a worst-case re-map of every slot (four unlinked slots, 3-octave smoothing, minimum span) takes ≤ 2.0 ms median;
  - analysis CPU at the defaults with every slot active and both sidechains connected is ≤ 4 % of one core;
  - in the worst case (four unlinked slots at 65536 points, 8x overlap, 192 kHz) it is ≤ 20 %.
- [ ] `processPending` makes 0 allocations after `prepare` while no `fftSize`, window, slot-setting or layout change occurs (allocation hook). `pushSidechain2` is real-time safe.
- [ ] Warning-clean at `/W4`. `docs/qa/T-046-unit.md` reports PASS.

## Log
- 2026-10-03 vst-architect: Created from `docs/plans/m3c-feature-expansion.md`. Additive by design: T-052 (processor) and T-055 (editor) switch over later, so do not change the meaning of any existing field or API.
- 2026-10-06 vst-architect: **Rewritten for the spectrum slot model** (`docs/plans/m3-review-spectrum-slots.md`; the user's decisions on review findings #9 and #12). The underlay design is dropped before it was built. The build order now follows M3d (T-062, T-063) and T-066. T-047 (1 Hz, 96 kHz, Align) and T-067 (Offset) build on the per-slot mapping introduced here, so keep the mapper call per slot and parameterised.
