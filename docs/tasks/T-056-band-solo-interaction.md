# T-056: Band solo interaction and outside-band tint
- Owner: gui-engineer
- Milestone: M3c
- Status: TODO
- Depends on: T-055

## Goal
Let the user find and hear one frequency band:
- **Ctrl held:** previews the band at the pointer.
- **Ctrl and the left button held:** solo the band on the output, and sweep it with the pointer.
- **Ctrl+wheel:** sets the width.
- **Releasing:** returns the full signal.

The parts of the spectrum outside the band are tinted. The solo must never stay engaged after the gesture ends.

## Inputs
- `docs/design/ui-spec.md`: the M3c band-solo section from T-045 (the preview, the audible state, the tint, the edge markers, the readout, the width steps, the cursor, the release rules, the interaction with the hover card, the zoom panel, the toasts and Freeze). Also §5.6 and §5.9, as amended by T-042.
- `docs/plans/m3c-feature-expansion.md`: design item [1], and Risks 16, 17 and 18.
- `docs/research/T-044-host-platform-constraints.md`, question 3: polling modifiers without keyboard focus.
- The T-052 log: `setBandSolo (active, centreHz)`, `bandSoloHeartbeat()`, the 250 ms failsafe, and `soloWidth`.
- `source/gui/SpectrumView.*`, `ZoomMath.h` (view mapping), and the editor update path.
- Working rules as in T-054.

## Scope
1. **Modifier polling.** On every editor update while the pointer is over the data rect, read `juce::ModifierKeys::getCurrentModifiersRealtime()`. The editor has no keyboard focus, so events alone cannot be trusted.
2. **The states:**

| State | How it starts | What happens |
|---|---|---|
| Preview | Ctrl held, no button | The band and the tint are drawn at the pointer. No audio change. |
| Solo | Ctrl and the left button held | `setBandSolo (true, centreHz)` on press and on every pointer move, at most once per editor update. `bandSoloHeartbeat()` on every editor update. |
| Idle | anything else | No band, no solo. |

   The solo ends, with `setBandSolo (false, …)` the same update, when:
   - the button is released;
   - Ctrl is released;
   - the pointer leaves the editor;
   - the editor loses focus or is hidden;
   - the editor is destroyed.
3. **Width.** Ctrl+wheel changes `soloWidth` in the spec's steps, as a host gesture. This works in the Preview and Solo states. The event is consumed and never zooms.
4. **No interference.** A Ctrl press never pans, zooms, box-zooms, or triggers the deferred spectrum reset or the right-click copy.
5. **The tint and markers.** The outside-band regions get the spec's tint, plus the edge markers and the centre and width readout. Drawn per the T-029 rules: one rectangle list per colour, no full cache rebuild per frame.
6. **Accessibility.** The solo state is shown without relying on colour, per the spec.

## Acceptance criteria
- [ ] **State machine.** Simulated events and a fake modifier source cover every row of the spec's band-solo table:
  - press, sweep and release;
  - Ctrl released before the button;
  - the button released before Ctrl;
  - the pointer leaving the editor;
  - focus loss;
  - editor destruction while soloing.

  Each ends with exactly one `setBandSolo (false, …)` and no further heartbeat.
- [ ] **Centre.** The frequency sent equals the pointer's frequency under the current view mapping, within 0.5 px, at Fit and zoomed.
- [ ] **Width.** Ctrl+wheel changes `soloWidth` by the spec's step and stays within the parameter's range. It never changes the view.
- [ ] **No interference.** With Ctrl held, a drag never pans or box-zooms, a click never resets the curves, and the wheel never zooms. Alt+wheel and Alt+drag still zoom and box-zoom (T-042).
- [ ] **End to end** (validation, in the Standalone or a host, with a real editor):
  - the output is band-limited while soloing;
  - it is bit-exact again after release (null-test capture);
  - closing the editor window mid-solo returns the full signal within 250 ms + one block.
- [ ] **Budgets** while sweeping continuously with the tint visible, at a 60 fps cap, median of 3: whole-editor message thread ≤ 25 % at 1280×720 and 2560×1440, and paint p95 ≤ 4 ms and ≤ 8 ms.
- [ ] Warning-clean at `/W4`. `docs/qa/T-056-unit.md` and `docs/qa/T-056-validation.md` report PASS.

## Log
- 2026-10-03 vst-architect: Created from `docs/plans/m3c-feature-expansion.md`. The plugin-validator part must include FL Studio if available (user's DAW), noting any host behaviour around Ctrl and Alt (Risk 17).
