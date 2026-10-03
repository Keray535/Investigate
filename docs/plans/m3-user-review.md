# M3 hands-on review: checklist for the user

This review is the last step of M3. Use it once T-013 (the meter panel) has passed QA and the architect has said the build is ready. It takes about 20 to 30 minutes. Note anything that looks wrong, feels slow or is missing, and report it in plain words; the architect turns each point into a task.

## 1. Get the build
- Build output: `build/Investigate_artefacts/Release/`
  - VST3: `VST3/Investigate.vst3` (a folder bundle)
  - Standalone: `Standalone/Investigate.exe`
- The build does not install the plugin automatically. To use it in a DAW, do one of the following:
  - copy `Investigate.vst3` to `C:\Program Files\Common Files\VST3\` (this needs administrator rights), or
  - add `build/Investigate_artefacts/Release/VST3/` as an extra VST3 search folder in your DAW, if it supports that.
- Rescan plugins in the DAW. Investigate appears as an analyser effect (category "Fx|Analyzer"), vendor "Investigate Audio".

## 2. Standalone app
- [ ] Launch `Investigate.exe`. Open its audio settings: **"Mute audio input" starts unchecked** (carried over from M2).
- [ ] Choose an input (a microphone or a line input) and check that the spectrum and the meters move. Use headphones or a separate output device: the app passes the input straight through, so a microphone can feed back through speakers.
- [ ] Close and reopen the app. The window size and the meter-collapse state are remembered.

## 3. In a DAW (insert on a track or the master bus)
**Audio**
- [ ] Playback sounds exactly the same with the plugin on and off: it never changes the audio.
- [ ] No added latency is reported by the DAW.

**Spectrum**
- [ ] The curve follows the music smoothly. The hover readout shows frequency, note with cents, and level, and it follows the mouse.
- [ ] Try every mode: Mid, Side, L, R, L+R, L/R, M/S. The dual modes show two curves with distinct styles.
- [ ] Settings in the bottom bar: Block, Overlap, Window, Avg time, Smooth, Slope, 2nd curve, Frequency and Level ranges. Each change is visible, and a double-click resets the setting to its default.
- [ ] Freeze stops the display. Changing the mode while frozen unfreezes it.
- [ ] A left click on the spectrum clears the all-time curves. A right click copies the frequency under the mouse to the clipboard; paste it somewhere to check.
- [ ] Sidechain: route a reference track to the plugin's sidechain input. The reference trace appears, and the SC button hides and shows it.

**Meters (T-013)**
- [ ] Peak, RMS, true peak, clip counts, headroom, the loudness block (M, S, I, LRA, max values), correlation and balance all react plausibly to the music.
- [ ] The Scale chip switches between dBFS, K-20, K-14 and K-12. The scale and the RMS values shift accordingly.
- [ ] Reset clears the max values, clip counts and integrated loudness.
- [ ] On a mono track the panel shows a single bar and a dimmed correlation meter.
- [ ] The meter-collapse button hides the panel, and the state is remembered after the project is saved and reopened.

**Window and smoothness**
- [ ] Resize from the corner grip, between the minimum size (920×540) and the maximum (2560×1600). Save the project, reopen it, and check that the size is kept.
- [ ] **Frame pacing (decision point):** make the window large (larger than about 1920×1080), or use L/R or M/S with a secondary curve and the sidechain trace on. The spectrum then redraws at 30 fps instead of 60, to keep the DAW's UI responsive. Say whether that looks acceptable or too choppy.
- [ ] The DAW's own UI (transport, scrolling, other plugin windows) stays responsive while Investigate is open.
- [ ] If you have a 1440p or 4K monitor, try the plugin on it. The large-window CPU numbers were measured on 1080p displays only.

**Look**
- [ ] Overall look against the approved mockup (`docs/design/mockup.html`): colours, fonts and spacing. Note anything that looks off.
- [ ] The About view (from Settings) shows the version and the third-party credits.

## 4. Reporting
For each finding, give the DAW name, the window size and mode, and what you expected compared with what you saw. A screenshot helps. "Looks fine" for a whole section is a useful answer too.
