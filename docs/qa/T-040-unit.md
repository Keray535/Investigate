# T-040 unit QA: freeze-latency perf split
Verdict: PASS (0 defects). Test-only change in `tests/unit/ProcessorViewFreezeTest.cpp`; no production code touched.

## Change
- New shared helper `runFreezeRounds (arrivalMs)` holds the unchanged 20-round freeze/unfreeze logic and every functional check (epoch +1 on freeze and unfreeze, old-epoch or frozen-snapshot-only frames, no live frame under a frozen epoch, frozen frame with no new audio, unfreeze publishes live state). The wait for the frozen frame is bounded (500 ms deadline; after arrival it keeps observing 100 ms to catch live frames under the frozen epoch).
- Default case (`[processor][freeze][T-036]`): "Freeze epoch: ... frozen frame arrives (500 ms liveness bound)". Asserts `violations == 0`; no bound tighter than 500 ms.
- Hidden case (`[processor][freeze][performance][.perf][T-036]`): "Freeze latency: 20 freeze rounds, ... within 20 ms". Asserts max <= 20 ms and `violations == 0`; prints p95 and max.

## Results
| Check | Result |
|---|---|
| `InvestigateUnitTests.exe "[.perf][T-036]"` (Release) | PASS: freeze latency p95 6.593 ms, max 6.843 ms (20 rounds); existing 65536-FFT latency case p95 5.584 / max 7.248 ms (the tag filter selects both) |
| `ctest --preset release --timeout 300`, run 1 / 2 / 3 | 100% passed, 0 failed, each run; 378 tests registered (5 fixture-dependent Tech 3341/3342 cases skipped, as before). Lifecycle #315 did not fail. |
| Default freeze-epoch case duration | about 3.6 s, passed |

## Review of other wall-clock assertions in the file
| Assertion | Decision |
|---|---|
| "100 open/close cycles ... stops within 50 ms each time" (`CHECK (ms <= 50.0)`) | Kept. It is a shutdown contract (thread must not hang), not a latency budget. Measured worst 3.6 ms, about 14x margin. |
| "512 FFT smoke" `CHECK (maxMs <= 200.0)` | Kept. Documented as a loose correctness bound (a frame arrives after a view change); 512-point frames take well under 10 ms, a 20x+ margin. Each wait is itself bounded at 500 ms. |
| 200 view changes at 65536 FFT (p95 <= 10, max <= 20 ms) | Already `[.perf]` and hidden. |
| `waitForAnyFrame` / 500 ms loops elsewhere | Bounded waits for arrival, not performance assertions. Kept. |

## Open questions
none
