# test_audio.wav inspection

## Question
Properties, metadata, levels and git state of `test_audio.wav` (repo root). Read-only inspection (pure Python; no numpy or ffprobe available).

## Answer
Stereo 48 kHz 32-bit IEEE float WAV, 200.79 s (9,637,941 frames), 77,103,712 bytes (73.5 MiB / 77.1 MB). Continuous, bass-heavy program material (music), exported from FL Studio 2025. Peaks -0.22 dBFS, no clipping. Near-mono but truly stereo (corr 0.956). Untracked, not ignored.

## Container
RIFF/WAVE (not RF64). Chunks: JUNK (28 B, zeros), `fmt ` (16 B), `data` (77,103,528 B), `smpl` (60 B), `LIST` (28 B).
fmt: tag 3 (IEEE float), 2 ch, 48000 Hz, 32 bit, block align 8, 384000 B/s.

## Metadata
- `LIST/INFO`: `ISFT` = "FL Studio 2025" (only field). No artist, title, copyright, originator, date.
- `smpl`: manufacturer 0, product 0, sample period 20833 ns, MIDI unity note 60 (0x3C), no loops (1 loop count field, see raw: 0 loops listed as zero-length), a DAW default.
- No `bext`, `id3 `, `iXML`.
- Provenance: rendered by FL Studio; origin of the music and redistribution rights are NOT stated in the file. Treat as unlicensed until the user confirms.

## Levels (per channel, L / R)
| | L | R |
|---|---|---|
| Sample peak | -0.22 dBFS | -0.22 dBFS |
| RMS | -13.23 dBFS | -13.28 dBFS |
| Samples >= 0 dBFS | 0 | 0 |
| DC offset | +3.9e-5 | +1.1e-5 |

Correlation L/R = 0.956: not dual mono, not inverted, truly stereo but narrow (strong centre content).

## Content
- Starts at about -14.8 dB RMS in the first 0.5 s, so no leading silence. Ends with a fade/tail: last 0.5 s about -72.7 dB RMS (fade-out or near silence at the end).
- Energy split (mono sum, one-pole splits at 200 Hz and 4 kHz, approximate): below 200 Hz 84.4 %, 200 Hz-4 kHz 15.4 %, above 4 kHz 0.2 %. Very bass-dominant (likely electronic/bass-heavy music), little HF energy.
- Loudness not computed (use project meter and libebur128 oracle).

## Git
- `git status --porcelain --ignored test_audio.wav`: `?? test_audio.wav` (untracked, not ignored).
- `git check-ignore -v`: no match (exit 1).
- 77 MB: do not commit as is (GitHub 100 MB limit is close; bloats history); keep untracked, add to .gitignore or use LFS if it should be shared.

## Sources
Local file only; parsed with Python `struct`/`array`.
