# T-018: GitHub Actions Windows CI
- Owner: build-engineer
- Milestone: M5
- Status: TODO
- Depends on: T-014

## Goal
Add a GitHub Actions workflow that runs on every push and pull request on Windows:
- build Investigate in Release and Debug;
- run the unit tests and the validation tests (pluginval and the VST3 validator);
- upload the VST3 and Standalone as artifacts.

This makes regressions visible once the user publishes the repository.

## Inputs
- `CMakeLists.txt`, `CMakePresets.json` and `docs/dependencies.md` from T-003 and T-014.
- `docs/roadmap.md`: Scope (Windows only) and Decisions.
- Remote: https://github.com/Keray535/Investigate (private, default branch `main`), created in T-027. GitHub Actions minutes for private repositories are limited by the account plan; keep jobs lean and cache aggressively.

## Scope
- `.github/workflows/ci.yml` using a Windows runner with Visual Studio 2022, running configure, build (Release and Debug), `ctest` with the unit label, and `ctest -L validation`.
- Cache FetchContent, dependency downloads and compiler output where it is safe to do so.
- Upload `Investigate.vst3` and `Investigate.exe` (Release) as artifacts, with a 14-day retention.
- Timing tests are hidden with the `[.perf]` tag (T-026), so `ctest` does not run them. Add a non-gating step (`continue-on-error: true`) that runs `InvestigateUnitTests.exe "[perf]"` and uploads its output for reference.
- Run the `asan` unit tests in a separate job. It may be allowed to fail only if T-017 documented known JUCE false positives.
- No secrets in this task. Signing is T-019.

## Acceptance criteria
- [ ] `actionlint`, or an equivalent check, passes on the workflow.
- [ ] If a remote exists: the workflow runs green on the default branch, and a full run takes ≤ 30 min with a warm cache.
- [ ] If no remote exists: the task is set to `IN_REVIEW`, with an open question asking the user to push. The architect sets it to DONE after the first green run.
- [ ] The artifacts contain a VST3 bundle that passes pluginval when downloaded and run locally.
- [ ] The workflow uses pinned action versions (full commit SHA or major version tag, as documented), and has minimal `permissions`.

## Log
