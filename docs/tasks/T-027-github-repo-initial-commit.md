# T-027: GitHub repository and initial commit
- Owner: build-engineer
- Milestone: M2 (checkpoint)
- Status: DONE
- Depends on: T-009

## Goal
Checkpoint the work of M0 to M2. Create a private GitHub repository for Investigate, commit every project file produced so far except the Claude Code configuration, and push. This also meets the "GitHub repository exists" precondition of T-018.

## Inputs
- User request, 2026-09-29: "create a repo to ghub and commit all produced files except claude files".
- Architect defaults, recorded in the roadmap Decisions: the repository is **private**, named `Investigate`, under the account `gh` is logged in with, with default branch `main`.
- The existing `.gitignore` from T-003, which already excludes `build/`, `third_party/` and `research/_cache/`.

## Scope
1. **Exclude the Claude files.** Add `CLAUDE.md` and `.claude/` to `.gitignore`, under a comment such as "Claude Code agent configuration (local only)". Confirm with `git check-ignore -v CLAUDE.md .claude/settings.json` that both are ignored. They must never be staged.
2. **Pre-commit checks:**
   - Run `git status --porcelain` and review the list. Only project files may be staged: `CMakeLists.txt`, `CMakePresets.json`, `.gitignore`, `cmake/`, `source/`, `tests/`, `tools/`, `assets/` and `docs/`, plus anything else that clearly belongs to the project. List any unexpected file in the log and do not stage it.
   - Scan for secrets: check the staged content for tokens, keys, passwords and certificates. There should be none. Stop and report if anything is found.
   - Size check: no staged file may exceed 5 MB. Report anything larger than 1 MB.
3. **Author identity.** Use the existing git identity if `git config user.name` and `user.email` are set. Otherwise set them repo-locally only (not with `--global`): the email `k.erayguven@outlook.com`, and the name from the GitHub account (`gh api user --jq '.name // .login'`).
4. **Commit.** Make one commit on branch `main` with this message:
   `Initial commit: M0-M2 (scaffold, DSP core, processor wiring, design, docs)`
   Add a body listing the milestones and pointing to `docs/roadmap.md`.
5. **GitHub.**
   - Check `gh auth status`. If it is not authenticated, stop and set BLOCKED with the exact command for the user (`gh auth login`); do not try interactive login.
   - Create the repository with `gh repo create Investigate --private --source . --remote origin --push`. If the name is already taken on the account, stop and report it; do not pick another name.
6. **Verify:**
   - `git ls-remote origin` shows `main` at the local HEAD;
   - `gh repo view --json name,visibility,url` shows the repository as PRIVATE;
   - `git ls-files | grep -iE '^(CLAUDE\.md|\.claude/)'` returns nothing.
7. **Documentation.** Add a short "Repository" section to `docs/dependencies.md` with the remote URL, the default branch and the visibility.

## Acceptance criteria
- [ ] `CLAUDE.md` and `.claude/` are ignored and absent from `git ls-files` and from the pushed commit.
- [ ] Exactly one commit on `main` contains all the project files listed in scope item 2, and nothing from `build/`, `third_party/` or `research/_cache/`.
- [ ] The secret scan is clean and no file exceeds 5 MB. The results are in the log.
- [ ] The GitHub repository `Investigate` exists, is PRIVATE, and its `main` branch matches the local HEAD. The URL is in the log.
- [ ] No global git configuration was changed.

## Log
- 2026-09-29 build-engineer: Read CLAUDE.md, this task file, `.gitignore` and `docs/dependencies.md`; set status to IN_PROGRESS.
- 2026-09-29 build-engineer: Added `CLAUDE.md` and `.claude/` to `.gitignore` under "Claude Code agent configuration (local only)". Confirmed with `git check-ignore -v CLAUDE.md .claude/settings.json`: both ignored (`.gitignore:37:CLAUDE.md` and `.gitignore:38:.claude/`).
- 2026-09-29 build-engineer: Pre-commit checks. `git status --porcelain` before staging showed only `.gitignore`, `CMakeLists.txt`, `CMakePresets.json`, `assets/`, `cmake/`, `docs/`, `source/`, `tests/`, `tools/` — no unexpected paths, and `research/_cache/` (gitignored from T-003) did not appear. After `git add -A`, `git status --porcelain` listed 155 staged files, all under the expected project directories; none from `build/`, `third_party/` or `research/_cache/`. Secret scan: `git diff --cached` file contents searched for private-key headers, API/secret key patterns, bearer tokens, `ghp_`/`gho_` tokens and AWS access-key IDs — no matches (the `gho_...` masked token that appears in `gh auth status` terminal output was never written to a repo file, so this is unrelated to the scan). Size check: largest staged file is `docs/design/mockup.html` at 90,101 bytes; nothing exceeds 1 MB, so nothing exceeds the 5 MB limit either.
- 2026-09-29 build-engineer: Author identity. `git config user.name` / `user.email` were already set globally (`cloder535` / `kerayguven.23.02@gmail.com`); no repo-local override was made and no global config was changed.
- 2026-09-29 build-engineer: Checked `gh auth status`: authenticated as `Keray535` on github.com (token scopes `gist`, `read:org`, `repo`, `workflow`), so no BLOCKED needed. Confirmed `Investigate` did not already exist on the account (`gh repo view Keray535/Investigate` → "Could not resolve to a Repository") before creating it.
- 2026-09-29 build-engineer: Committed on branch `main` (renamed from the default `master` with `git branch -M main` before pushing) with the exact message specified in the task, commit `71d0313d0f8e8d0e8d9089023460d5f663739ed2` — 155 files, 21,822 insertions, root commit.
- 2026-09-29 build-engineer: `gh repo create Investigate --private --source . --remote origin --push` failed once with a transient GitHub GraphQL server error (`E848:124BA1:A30E7D:A36717:6ABAF552`); verified nothing was created (`git remote -v` empty, `gh repo view` 404) and retried once, which succeeded: repository created at https://github.com/Keray535/Investigate and pushed.
- 2026-09-29 build-engineer: Verification — `git ls-remote origin main` → `71d0313d0f8e8d0e8d9089023460d5f663739ed2 refs/heads/main`, matching local HEAD exactly. `gh repo view --json name,visibility,url` → `{"name":"Investigate","url":"https://github.com/Keray535/Investigate","visibility":"PRIVATE"}`. `git ls-files | grep -iE '^(CLAUDE\.md|\.claude/)'` returned nothing (exit code 1).
- 2026-09-29 build-engineer: Added a "Repository" section to `docs/dependencies.md` (remote URL, default branch `main`, visibility Private) and pushed it as a second commit, `2a9bb43`, since the URL did not exist until after the first push. All acceptance criteria for the initial-commit scope (items 1-6) are satisfied by commit `71d0313`; item 7's documentation follows in `2a9bb43`. Status set to DONE.
