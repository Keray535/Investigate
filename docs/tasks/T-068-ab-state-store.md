# T-068: A/B state store
- Owner: plugin-engineer
- Milestone: M3c
- Status: TODO
- Depends on: T-053

## Goal
Add A/B comparison (review finding #22, moved into v1 by the user). There are two independent states, A and B. Each holds every parameter value, and a change in one never affects the other. Both are saved with the session.

## Inputs
- `docs/plans/m3-review-spectrum-slots.md`, Architecture: "A/B comparison (#22)", the Budgets (A/B switch ≤ 50 ms) and the Decisions.
- The T-053 log: `PresetStore`, the parameter snapshot and apply code (one host gesture per parameter), and `source/presets/` ownership.
- The T-051 log (state version 2, `ViewState`) and `source/PluginProcessor.*` (state save and load).
- Working rules as in T-051.

## Scope
1. **`source/presets/ABStateStore.*`**, message thread only:
   - holds two parameter snapshots (A and B; plain values for every APVTS parameter) and the active letter;
   - `switchTo (Letter)`: store the current parameters into the active snapshot, then apply the other snapshot through the `PresetStore` apply path, and make it active;
   - `copyActiveToOther()`;
   - `getActive()`;
   - a change listener for the editor.
   - Reuse the T-053 snapshot and apply code; do not duplicate it.
2. **Contents.** The store holds every APVTS parameter, and nothing else: no `ViewState`, colours, display preferences, reference slots, zoom or freeze. `soloWidth` is included; it is a parameter, even though it is not automatable.
3. **Interactions.**
   - Loading a settings preset changes the current parameters, which belong to the active letter. The inactive snapshot is untouched.
   - Host automation also changes the active state.
   - A switch while the host automates a parameter is allowed: the switch applies the values once, and later automation wins.
4. **Session state.**
   - Save both snapshots and the active letter in a child `ABStates` of the state tree (`stateVersion` 2). The active snapshot is refreshed from the live parameters at save time.
   - Loading a state without `ABStates` (version 1, or version 2 before this task) sets A from the loaded parameters, sets B equal to A, and makes A active.
   - Unknown IDs are ignored, and missing IDs take their defaults.
5. **Processor API:** `ABStateStore& getABStates() noexcept`, message thread only.

## Acceptance criteria
- [ ] **Independence.** Set A, switch to B, change 20 random parameters, and switch back to A: every parameter equals A exactly (plain values). Switching to B again restores B's changes exactly.
- [ ] **Copy.** "Copy A→B" makes B equal to A; changes made afterwards in B do not affect A.
- [ ] **Presets.** Loading a preset while B is active changes B only.
- [ ] **Session.** Save while B is active with A and B different, then load into a new instance: B is active, and both snapshots are restored. A version-1 fixture loads with A equal to B, equal to the loaded values.
- [ ] **Speed.** Switching applies every parameter in ≤ 50 ms on the message thread (`[.perf]`, median of 10).
- [ ] **Audio.** The null test stays bit-exact during repeated switches. pluginval at strictness 10 passes.
- [ ] **Real-time safety.** No audio-thread code path touches the store (code review in the unit QA report).
- [ ] Warning-clean at `/W4`. `docs/qa/T-068-unit.md` and `docs/qa/T-068-validation.md` report PASS.

## Log
- 2026-10-06 vst-architect: Created from the M3 review triage. The editor controls are in T-058. Decide with evidence whether one switch should be a single undo step in the Standalone, and record the decision for T-058.
