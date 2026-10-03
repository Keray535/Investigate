# T-032: Local-only test audio: keep it out of git and make it available to the testers
- Owner: build-engineer
- Milestone: M3
- Status: TODO
- Depends on: none (git and CMake only; run it when no other C++ build is in flight)

## Goal
The user added `test_audio.wav`, their own produced track (a raw mix and master), at the repository root for the test agents to use. The user decided it must **never be committed** to the repository. Keep it out of git for good, move it to the local fixtures location, and give the tests a single path to find it, so that checks with real program material run when the file is present and skip cleanly when it is not (for example in CI).

## Inputs
- `test_audio.wav` at the repository root (currently untracked and not ignored).
- `docs/research/test-audio-inspection.md` (researcher), which gives the format, length and levels.
- `.gitignore`, `CMakeLists.txt` and `CMakePresets.json`.
- The existing local-fixture convention: `third_party/_fixtures/` (gitignored as part of `third_party/`), as planned for the EBU set (`INVESTIGATE_EBU_TEST_SET_DIR`, roadmap Decisions 2026-09-28).

## Scope
1. **Move** `test_audio.wav` to `third_party/_fixtures/audio/test_audio.wav`. Do not change or re-encode the file. Confirm the size and SHA-256 are unchanged after the move, and record the hash in the log.
2. **`.gitignore`.** Add root-level audio patterns (`/*.wav`, `/*.flac`, `/*.aif`, `/*.aiff`, `/*.mp3`) so that an audio file dropped at the root again cannot be committed. Confirm with `git check-ignore -v`.
3. **Test hook** (amended by the architect on 2026-10-03: a folder, not a single file, so a second file such as a separate master can be added later). Add a CMake cache path `INVESTIGATE_TEST_AUDIO_DIR`, defaulting to `${CMAKE_SOURCE_DIR}/third_party/_fixtures/audio`. Pass it to the unit-test target and the render-harness targets (a compile definition or an environment property on their CTest entries), so tests can find `test_audio.wav` and any later files inside it. Do not fail configuration when the folder or the file is missing.
4. **Document** the fixture in `docs/dependencies.md`: local only, never committed, owned by the user, and the tests skip when it is absent. Also state that `third_party/_fixtures/` holds user-provided files that a dependency refresh must not delete. If you ever clear `third_party/`, keep `_fixtures/`.
5. **Commit and push** only `.gitignore`, the CMake changes, `docs/dependencies.md` and this task file. Before committing, verify that no `.wav` file is staged (`git diff --cached --name-only | grep -i '\.wav$'` must print nothing).

## Acceptance criteria
- [ ] `git status --porcelain --ignored` no longer lists `test_audio.wav` as untracked. The file exists at `third_party/_fixtures/audio/test_audio.wav` with the same SHA-256 as before.
- [ ] `git check-ignore -v` matches both the fixture path and a test file named `/test.wav` at the root.
- [ ] `cmake --preset windows-msvc` succeeds with the file present and with it temporarily renamed. The resolved `INVESTIGATE_TEST_AUDIO_DIR` value is printed or visible in the cache.
- [ ] The commit contains no audio file. Report the hash in the log.

## Log
- 2026-10-03 vst-architect: Created from the user's decision. The track is the user's own work, but they do not want it in the repository, so it stays local-only. Testers use it for real-program checks (whole-editor CPU with music, loudness and true peak against libebur128, and a bit-exact null test on real material) and skip those checks when it is absent.
- 2026-10-03 vst-architect: This task was written in a parallel architect session. It is reconciled with the main session, which had planned `tests/fixtures/audio/` and an item in T-014. This task's `third_party/_fixtures/` location is kept, for consistency with the EBU fixture hook. The hook is now a folder variable, `INVESTIGATE_TEST_AUDIO_DIR`. The T-014 item 3b was removed, and T-015 and T-016 now point here. Properties of the file are in `docs/research/test-audio-inspection.md`: one file, 48 kHz, 32-bit float, stereo, 200.8 s, 77 MB.
