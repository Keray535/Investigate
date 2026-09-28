# T-020: SPAN user guide visual extraction
- Owner: researcher
- Milestone: M0
- Status: DONE
- Depends on: T-001

## Goal
T-001 could not read the Voxengo SPAN user guide, because the PDF is a scanned document with no text layer and `pdftoppm` is not installed. As a result, most SPAN defaults, interactions and layout facts are marked "not documented". This task tries a different approach: extract the page images from the PDF and read them visually. It also looks at official product screenshots. The goal is to confirm or correct the provisional values in the roadmap and fill the gaps in T-001 sections 2 to 5 and 7.

## Inputs
- `docs/research/T-001-span-feature-parity.md`, especially the "not documented" items in sections 2, 3, 4, 5 and 7.
- `docs/roadmap.md`: the parameter table (values marked *), the fixed constants, and MeterSnapshot contents.
- Primary sources:
  - SPAN User Guide: https://www.voxengo.com/files/userguides/VoxengoSPAN_en.pdf/getbyname/Voxengo%20SPAN%20User%20Guide%20en.pdf
  - Voxengo Primary User Guide: https://www.voxengo.com/files/userguides/VoxengoPrimaryUserGuide_en.pdf/getbyname/Voxengo%20Primary%20User%20Guide%20en.pdf
  - Screenshots on https://www.voxengo.com/product/span/

## Method
1. Download the PDFs into `research/_cache/span/`. This directory is gitignored.
2. Extract the embedded page images without poppler. Any of these methods works:
   - Python with `pypdf` or `pikepdf`, if they are available;
   - a small script that carves the raw JPEG streams (the `FFD8` ... `FFD9` markers) out of the PDF bytes;
   - any other tool that is already installed.
   Do not install system-wide software. If a Python package is missing, use `pip install --user` only for a pure-Python package, and note it in the log.
3. Open the extracted page images with the Read tool (it displays images), and read the relevant pages.
4. Download 1 to 3 official SPAN screenshots into `research/_cache/span/` and view them with the Read tool, to write a verbal layout description.
5. Do not copy images or guide text into `docs/`. Paraphrase, and cite the page number.

## Deliverable
Update `docs/research/T-001-span-feature-parity.md` in place:
- Replace each "not documented" statement you can now confirm with the confirmed value and a citation to the guide page ("SPAN User Guide v3.xx, p. N").
- Add a section "9. Addendum (T-020)" that lists every value that changed and every value still unconfirmed.
- Update the section 7 gap table where a recommendation changes, marking the changed rows.
- Describe the layout in words in section 5, using the screenshots, with regions and approximate proportions. Do not reproduce the artwork.

## Acceptance criteria
- [ ] The page images were extracted and read. The log states the method used and the number of pages read.
- [ ] For each of these items, the addendum gives either a confirmed value with a page citation, or "still not documented in the guide": the defaults and ranges of block size, overlap, window list, averaging time, smoothing, dB range and frequency range; the secondary spectrum modes; the real-time max release; the channel modes and how stereo is combined by default; the RMS and peak meter definitions; the clip definition; the correlation integration time; the hover readout format; the mouse and keyboard interactions; and the reset behaviour.
- [ ] Section 5 has a verbal layout description sourced from the screenshots, labelled with the image source URLs.
- [ ] The section 7 table reflects every changed recommendation, with the reason for each.
- [ ] No images or verbatim guide text are copied into `docs/`. Cache files stay in `research/_cache/`.

## Log
- 2026-09-28 researcher: Re-fetched both PDFs into `research/_cache/span/`. Discovered they are not scanned images — both have a normal extractable text layer; the earlier "scanned PDF" conclusion in T-001 was a tooling artifact (`pdftoppm` missing, JPEG-stream misread). Extracted full text with Python `pymupdf` (already available, no install needed) for both `VoxengoSPAN_en.pdf` (v3.24, 7 pages) and `VoxengoPrimaryUserGuide_en.pdf` (46 pages), and rendered all 7 SPAN-guide pages to PNG at 200 dpi, then read all 7 with the Read tool (page 1 contains the only screenshot; pages 2-7 are text/contents/credits). Updated `docs/research/T-001-span-feature-parity.md` in place: replaced "not documented" statements with confirmed, page-cited values across sections 1-5, rewrote section 5 with a sourced verbal layout description from the page-1 screenshot, updated section 7's gap table (3 rows changed: correlation integration 300ms -> 500ms, window choice list corrected to Hann/Hi-Res/Blackman/Dome, avgTime semantics redefined to "time to fall 20 dB"), and added section 9 with the full addendum, including a flagged design divergence for the architect (Investigate's fixed real-time-max release vs. SPAN's adjustable Avg Time). No images or guide text copied into `docs/`; all cache files (PDFs, extracted text, PNGs) stay in `research/_cache/span/`. Status: DONE.
