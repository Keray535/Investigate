# T-022: Font license check — Inter 4.x and JetBrains Mono 2.304

## Question
Can Inter 4.x and JetBrains Mono 2.304 be embedded (compiled into the plugin binary via JUCE `BinaryData`) and subset, for a closed-source, freely distributed Windows VST3/Standalone plugin ("Investigate")?

## Answer and recommendation
Both fonts are **approved with conditions**. Both are licensed under the **SIL Open Font License 1.1 (SIL-OFL-1.1)**, which explicitly permits embedding in commercial/closed-source software, bundling in an installer, and modifying (including subsetting), as long as the modified font isn't sold by itself and isn't renamed to claim the original's Reserved Font Name. Neither repository declares a Reserved Font Name in its copyright/license header, so embedding unmodified — or subsetted — TTFs does not trigger any RFN restriction. The only binding obligation is to ship the OFL license text and the fonts' copyright notice alongside the binary (installer and/or an in-app credits/About screen satisfy this).

- **Inter**: pin to tag `v4.1` (matches "Inter 4.x"; latest 4.x release as of 2026-09-28). License: SIL OFL 1.1.
- **JetBrains Mono**: pin to tag `v2.304` exactly as specified. License: SIL OFL 1.1.

## Options compared

| Font | Tag | License | RFN declared? | Sold-alone restriction | Subsetting allowed |
|---|---|---|---|---|---|
| Inter | v4.1 | SIL OFL 1.1 | No | Yes (cannot sell font by itself; bundling with the plugin is fine) | Yes, as a Modified Version, same license |
| JetBrains Mono | v2.304 | SIL OFL 1.1 | No | Same | Same |

## Key details

### License text and source, confirmed at the pinned tag
- Inter `LICENSE.txt` at v4.1: https://raw.githubusercontent.com/rsms/inter/v4.1/LICENSE.txt — copyright line: "Copyright (c) 2016 The Inter Project Authors (https://github.com/rsms/inter)". No Reserved Font Name is listed after the copyright statement, so clause 3 (RFN) does not restrict use of the primary name in an unmodified or subsetted embed.
- JetBrains Mono `OFL.txt` at v2.304: https://raw.githubusercontent.com/JetBrains/JetBrainsMono/v2.304/OFL.txt — copyright line: "Copyright 2020 The JetBrains Mono Project Authors (https://github.com/JetBrains/JetBrainsMono)". Same: no RFN declared.
- Both files are the canonical OFL 1.1 body (permission clauses 1–5, termination, disclaimer), verified verbatim in both repos at the pinned tags.

### Relevant OFl 1.1 clauses (from the text fetched above)
1. Neither the Font Software nor its components may be **sold by itself** — bundling it inside a paid or free plugin binary is not "selling the font by itself," so this does not block Investigate's distribution model.
2. Original or Modified Versions may be **bundled, redistributed and/or sold with any software**, provided each copy contains the copyright notice and the license text (as a standalone file, human-readable header, or in machine-readable metadata that's easily viewable).
3. A **Modified Version** (which subsetting produces) may not use the Reserved Font Name unless granted permission — moot here since no RFN is declared, but as a matter of good practice a subsetted font should not claim to be the unmodified "Inter"/"JetBrains Mono" release if its `name` table isn't adjusted; simplest is to keep the original name table intact when only glyphs are removed (subsetting glyph coverage, not renaming, is the norm and is explicitly allowed).
4. The Font Software, modified or unmodified, **must stay under OFL 1.1** — Investigate cannot re-license the font files themselves, only their own code.
5. No promotional use of the Copyright Holder's name without permission.

### Direct download URLs (exact files the ui-spec lists, §3.2)
Verified present at the release archives for the pinned tags (`gh api` / GitHub Releases, checked by listing zip contents read-only):

- Inter v4.1 release asset `Inter-4.1.zip` → https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip
  - `extras/ttf/Inter-Medium.ttf` (weight 500)
  - `extras/ttf/Inter-SemiBold.ttf` (weight 600)
  - Note: Inter 4.x no longer ships loose static TTFs in the repo tree (only `InterVariable.ttf` and web `.woff2` under `docs/font-files/`); the static per-weight TTFs the spec calls for are inside this release zip's `extras/ttf/` folder. (There is also `InterDisplay-*` — do not use; the spec wants the regular Inter family, not InterDisplay.)
- JetBrains Mono v2.304 release asset `JetBrainsMono-2.304.zip` → https://github.com/JetBrains/JetBrainsMono/releases/download/v2.304/JetBrainsMono-2.304.zip
  - `fonts/ttf/JetBrainsMono-Regular.ttf` (weight 400)
  - `fonts/ttf/JetBrainsMono-Medium.ttf` (weight 500)
  - Note: do not confuse with the `JetBrainsMonoNL-*` (no-ligatures) variant, which is a different file the spec does not ask for.

### Subsetting for binary size
OFL 1.1 clause 3 defines a subset as a "Modified Version" (deleting components). This is explicitly permitted; the only conditions are (a) it stays under OFL 1.1, (b) it doesn't claim the Reserved Font Name if one existed (none does here), and (c) the OFL text and copyright notice still accompany distribution. Recommendation: subset to the Latin glyph set actually used by the UI strings (digits, punctuation, uppercase/lowercase Latin, the note-name symbols, U+2212 minus, U+00D7 multiplication sign used per the ui-spec header) to shrink the embedded `BinaryData` footprint. Keep the font's internal `name` table copyright/license strings intact in the subset if the subsetting tool supports it (e.g. `fonttools subset` preserves the `name` table by default), which also helps satisfy clause 2's "machine-readable metadata" allowance.

## Licenses and risks
- **SIL Open Font License 1.1** (both fonts) — **permissive for this use case**: embedding in a closed-source, freely distributed binary is allowed, selling the plugin with the font embedded is allowed, subsetting is allowed. Risk is limited to omitting the required notice/license text, which is a documentation gap, not a legal blocker.
- No GPL/AGPL exposure: OFL 1.1 does not require the surrounding software (the plugin) to be open-sourced. It only keeps the *font files themselves* under OFL.
- No trademark risk found: no Reserved Font Name clause is invoked by either project's current copyright header.

## Action items (binary distribution obligations)
For `build-engineer` (T-019, installer) and whoever writes the About/credits text:
1. Ship a copy of the OFL 1.1 license text with the installer/package, e.g. `assets/ui/fonts/LICENSE-OFL-Inter.txt` and `assets/ui/fonts/LICENSE-OFL-JetBrainsMono.txt` (or one combined `THIRD-PARTY-FONTS.txt`), each preceded by the respective copyright line quoted above.
2. Add both fonts' copyright notices and "licensed under the SIL Open Font License, Version 1.1" line to the plugin's About/credits screen (or a `NOTICE`/`THIRD_PARTY_LICENSES` file bundled by the installer) — this satisfies OFL clause 2's "easily viewable" requirement without requiring the full license text to appear in the UI itself, as long as the full text ships in the package.
3. Do not sell or offer the font files as a standalone download from the plugin's own store page (OFL clause 1).
4. If gui-engineer/ui-ux-designer subsets the TTFs, keep the subsetting tool's default behavior of preserving the font's `name`/copyright table; do not rename the family to something implying it is a different, unlicensed font.
5. Record the exact source URLs and tags above in `assets/ui/fonts/SOURCES.md` (or equivalent) so the pinned versions are traceable for a future audit.
6. Pin the tags in the design/build inputs: Inter `v4.1`, JetBrains Mono `v2.304`. If the architect later wants Inter's absolute latest patch instead of v4.1, re-run this same check against the new tag's `LICENSE.txt` (unlikely to change materially, since Inter has used OFL 1.1 with no RFN since v3.x).

## Verdicts
- **Inter 4.x (pin `v4.1`)**: **Approved with conditions** — ship OFL license text + copyright notice with the installer/credits; do not sell standalone; subsetting permitted.
- **JetBrains Mono 2.304 (pin `v2.304`)**: **Approved with conditions** — same conditions as above.

Both verdicts are unconditional on the *technical* embedding path (JUCE `BinaryData` compiling a TTF into the binary is not a "sale of the font by itself" and is squarely within clause 2's "bundled... with any software").

## Sources
- Inter GitHub repo and tags: https://github.com/rsms/inter
- Inter LICENSE at v4.1: https://raw.githubusercontent.com/rsms/inter/v4.1/LICENSE.txt
- Inter v4.1 release: https://github.com/rsms/inter/releases/tag/v4.1
- Inter v4.1 release asset: https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip
- JetBrains Mono GitHub repo and tags: https://github.com/JetBrains/JetBrainsMono
- JetBrains Mono OFL.txt at v2.304: https://raw.githubusercontent.com/JetBrains/JetBrainsMono/v2.304/OFL.txt
- JetBrains Mono v2.304 release: https://github.com/JetBrains/JetBrainsMono/releases/tag/v2.304
- JetBrains Mono v2.304 release asset: https://github.com/JetBrains/JetBrainsMono/releases/download/v2.304/JetBrainsMono-2.304.zip
- SIL OFL 1.1 FAQ (background on RFN, embedding, selling): https://scripts.sil.org/OFL
