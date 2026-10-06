# T-059: M3c gate validation
- Owner: plugin-validator
- Milestone: M3c
- Status: TODO
- Depends on: T-058

## Goal
Validate all 14 M3c items together, as a host and a user would meet them, before M4 starts. The per-task QA reports cover each feature in isolation. This task covers:
- the interactions between features;
- compatibility with sessions saved by earlier builds;
- every M3c budget with all features enabled.

## Inputs
- `docs/plans/m3c-feature-expansion.md`: Scope, Budgets, Risks 15 to 23, and the user hands-on checklist.
- Every M3c QA report: `docs/qa/T-046-unit.md` to `docs/qa/T-058-validation.md`.
- `docs/research/T-044-host-platform-constraints.md`: the host table and the Alt-key verdict.
- The version-1 state fixtures from T-051.
- pluginval and the existing render harness (`tools/render-harness/`). The test audio in `third_party/_fixtures/audio/` if present (`INVESTIGATE_TEST_AUDIO_DIR`); skip with a reason otherwise.
- Working rules: `ctest --timeout 300`; timing assertions only in `[.perf]`; CPU is the median of 3 runs with the T-012/T-029 harness.

## Scope and acceptance criteria
- [ ] **Host validation.** pluginval at strictness 10 passes on the Release VST3 with the three-input bus configuration.
- [ ] **Bit-exact pass-through.**
  - The null test is exactly 0 with the solo idle, across mono and stereo main; each sidechain disabled, mono and stereo; analysis on and off; 44.1, 48 and 96 kHz; and block sizes 1, 128 and random 1 to 4096.
  - Every parameter is randomised per block, and the editor is opened and closed during the run.
  - If the test audio is present, it is used as the main input.
- [ ] **Solo safety.** Over 1000 random solo press and release sequences, with random sweeps and editor closes mid-solo:
  - the output is bit-exact whenever the solo is idle;
  - no NaN or infinity appears;
  - with the editor gone, the solo always releases within 250 ms + one block.
- [ ] **Compatibility.** The T-051 version-1 fixtures, and a state saved by the current M3b build, load in the M3c build:
  - the same channel display (legacy L/R and M/S shown as their pairs);
  - the same meter scale, ranges and `ViewState`;
  - a state round trip in the M3c build preserves everything, including reference slots and labels.
- [ ] **Presets and colours together:**
  - loading a settings preset never changes colours or slots;
  - loading a colour preset never changes parameters;
  - colours persist across two Standalone launches;
  - two simultaneous instances in one host process see one colour change.
- [ ] **CSV.** Export then import across two instances gives equal curves within 0.01 dB. A SPAN-style CSV file loads.
- [ ] **Hosts.** In FL Studio (the user's DAW), plus Reaper if available, and the Standalone:
  - Sidechain 1 and Sidechain 2 can be routed, or the limitation is recorded;
  - Alt+wheel and Alt+drag zoom without opening a host menu, or the behaviour is recorded per host (Risk 17);
  - Ctrl+click solo works.

  Record exact steps and screenshots in the report.
- [ ] **Budgets** (the plan's Budgets table), measured at 48 kHz, 128 samples, stereo main plus two stereo sidechains:
  - `processBlock` mean ≤ 20 µs and p99 ≤ 50 µs with the solo engaged;
  - the analysis thread ≤ 2 % at the defaults and ≤ 10 % in the worst case;
  - capture ≤ 20 ms;
  - with every M3c layer visible at a 60 fps cap, Adaptive on, through the real editor path at 1280×720 and 2560×1440, at the native refresh rate and at 60 Hz: whole-editor message thread ≤ 25 % and paint p95 ≤ 4 ms and ≤ 8 ms;
  - memory ≤ 64 MB per instance at maximum settings.

  The 144 fps numbers are reported, not gated.
- [ ] `docs/qa/T-059-validation.md` lists every check with PASS or FAIL, the numbers, the machine specification and the commands to reproduce. Each defect names its owning agent.

## Log
- 2026-10-03 vst-architect: Created from `docs/plans/m3c-feature-expansion.md`. M4 (T-015, T-016, T-017) depends on this task.
- 2026-10-06 vst-architect: **Amended by `docs/plans/m3-review-spectrum-slots.md`.** The gate also covers:
  - **Slots.** Every slot works with each of the three sources (Main, SC1, SC2) and each channel. Link on and off behave as specified. The default slots reproduce the pre-M3c look.
  - **Compatibility.** Version-1 states load with slot 2 as the old secondary curve and slot 3 as the old sidechain trace, and legacy L/R and M/S display as Left and Mid.
  - **Sidechain meters.** On identical input they equal the main meter. They appear and disappear with the bus.
  - **Other features.** Offset (Normalize, Center, Spectrum 2 to 4, including the inactive state), Fill, the 96 kHz axis at 48 and 192 kHz, and the Max crest readout.
  - **Compact layout and size.** The compact layout below 600 px wide, and the new minimum size, including the small-window glitch check in FL Studio (#16).
  - **A/B.** A round trip through the session, switch time ≤ 50 ms, and A and B independent.
  - **M3d fixes still hold:** a smooth low-frequency curve at every Smooth setting, no block-size stutter, no tooltips except the hover readout.
  - **Revised budgets:** `processBlock` mean ≤ 40 µs and p99 ≤ 90 µs with three banks; analysis ≤ 4 % at the defaults and ≤ 20 % in the worst case; re-map ≤ 2.0 ms; the whole editor ≤ 25 %.
  - **The user's M3c checklist** in the M3c plan is extended with these items before the gate.
