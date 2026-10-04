# T-044: Host and platform constraints for M3c, plus band-solo filter references

Date: 2026-10-04. Author: researcher. Documentation only; no product code touched.
Tags: "confirmed (source)", "inferred", "not documented".

## Question
What do hosts and Windows allow for: a second VST3 sidechain bus; Alt gestures (Risk 17); modifier polling without focus; a shared preference file; `FileChooser::launchAsync`; a band-solo filter (T-050); a colour wheel?

## Answer and recommendation
1. **Risk 17 verdict: plausible but unproven; mitigable in the plugin.** No source documents Alt-release opening the host menu for a JUCE plugin in FL Studio, Reaper, Live or the Standalone (not documented). The mechanism is real, though. If the plugin's HWND has focus and no component consumes the Alt key-up, JUCE re-posts `WM_SYSKEYUP` to the host's parent HWND (confirmed, JUCE source), and Windows' default handling then sends `SC_KEYMENU` to the top-level window (confirmed, Microsoft docs). In the Standalone, JUCE blocks `SC_KEYMENU` (confirmed, source), so the Standalone is safe. **Plugin-side mitigation: consume the Alt key-up** (see Q2). It only helps while the editor HWND holds focus. If the host holds focus, the plugin never sees the key and cannot stop it. T-037, T-038 and T-059 must test it by hand. Fallback modifiers are in Q2.
2. **Second sidechain:** FL Studio supports several wrapper inputs and is expected to work (inferred). Reaper works (confirmed, user report). Cubase/Nuendo supports it (confirmed, Steinberg developer). Live, Bitwig: probably only one sidechain. Studio One: unknown. Keep Risk 15 as is, "no input" fallback is right.
3. **Code finding for plugin-engineer:** the existing `.withInput ("Sidechain", AudioChannelSet::disabled(), true)` breaks a JUCE rule. JUCE asserts "Your default layout cannot be disabled" and the VST3 wrapper reports a bus channel count from `getLastEnabledLayout()` (confirmed, source). Declare aux buses as `stereo()` with `isActivatedByDefault = false`.
4. **Modifier polling:** `getCurrentModifiersRealtime()` reads `GetAsyncKeyState` and does not need focus (confirmed, source).
5. **Filter for T-050:** two cascaded 2nd-order TPT/Simper SVF band-passes (12 dB/oct skirts, 0 dB centre at all widths). Not an LR high-pass/low-pass pair (it loses 8 to 11 dB at narrow widths). Details in Q6.

## Q1. Second sidechain bus

### Host table
| Host | Second bus offered | Routing steps | Caveats | Status |
|---|---|---|---|---|
| **FL Studio** (user's DAW) | Yes, expected | 1. Select the source mixer track; right-click the arrow at the bottom of the destination track and choose "Sidechain to this track". Repeat for the second source. 2. Put Investigate in a mixer effect slot of the destination track. 3. Open the wrapper (cog icon) > Processing tab > Connections. 4. Right-click each plugin input and pick which sidechained mixer track feeds it. "Auto map inputs" assigns the sidechained tracks to the plugin inputs in order, lowest track first. | See FL details below. | Wrapper steps: confirmed ([Image-Line manual](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/plugins/wrapper.htm), [Image-Line forum](https://forum.image-line.com/viewtopic.php?f=100&t=243147)). A second VST3 bus appearing as inputs 5/6: inferred. |
| Ableton Live 12 | Unlikely (one only) | Plugin's Sidechain section: "Audio From" track + tap point | Reports say only one sidechain bus connects; a plugin with more input buses was rejected in Live 10 | Weak sources: [iPlug2 forum](https://iplug2.discourse.group/t/2-or-more-sidechain-input-buses-3-or-more-total-inputs-tried-many-things-but-cant-get-a-second-sidechain-bus-to-connect/1009) (Live 10), search summaries for Live 12. Live 12 not directly tested: unknown |
| Reaper 7 | Yes | Give the track 6 channels, send source A to 3/4 and B to 5/6, open the plugin's Pin Connector > I/O > "Request VST3 bus channel count" > stereo (not "auto") | The default "auto" channel count may leave aux buses at zero channels | Confirmed by user reports: iPlug2 thread (4 inputs on 8-2), [FabFilter forum](https://www.fabfilter.com/forum/add-comment?id=3113). Reaper 7 specifically: inferred |
| Cubase / Nuendo | Yes | Plugin side-chain button; enabled buses appear as routing targets of tracks and sends | Cubase's own Frequency plugin uses several. Third-party use is rare, so exposure of the second bus in the UI is not documented | Confirmed ([Steinberg forum](https://forums.steinberg.net/t/sidechain-protocol-multiple-locations-within-the-same-plugin/1019829)); UI steps: not documented |
| Studio One | Unknown | Send from the source track, pick it in the plugin's sidechain menu | Only one sidechain menu is mentioned in the sources | Not documented |
| Bitwig | Probably no | One sidechain input per device | Old forum posts: 1 stereo input + 1 stereo sidechain; workaround is a helper track with several Audio Receivers | Weak ([KVR Bitwig forum](https://www.kvraudio.com/forum/viewtopic.php?t=552244)); current version unverified |

### FL Studio details
- The wrapper exposes a flat list of plugin input pins in the Connections section, for any plugin with several inputs (confirmed, manual). Whether pins are grouped by VST3 bus name is not documented. Expect main = inputs 1/2, Sidechain 1 = 3/4, Sidechain 2 = 5/6 (inferred, matches a how-to that uses "inputs 3 and 4" for the first sidechain: [ringmodsidechain.com](https://ringmodsidechain.com/tutorials/sidechain-routing-in-fl-studio-complete-guide), low-quality source).
- Input mapping applies to plugins in mixer effect slots only; an instrument channel cannot receive audio input (confirmed, manual).
- In a mixer slot the numbers are indexes into the inputs routed to that track (confirmed, manual). The manual advises naming the mixer tracks used by a plugin so FL counts them in PDC.
- Existence proof: Scrollscope (a JUCE VST3 scope) advertises "sidechain input graphing from up to five additional channels" in FL Studio ([README](https://github.com/ardura/Scrollscope)). The repository has no licence file (GitHub API returns 404). **Verdict: rejected as a code source** (all rights reserved); usable only as evidence that the pattern works.
- Bridged plugins ("Make bridged") run in another process and always steal keyboard focus when selected (confirmed, manual). This matters for Q2 and Q4.
- Test needed on the user's FL build: load Investigate, check that the wrapper lists inputs 1 to 6 (or only 1 to 4), and that a mono-vs-stereo choice per bus is honoured.

### How JUCE 8.0.15 declares buses (confirmed, source)
- `BusesProperties().withInput (name, defaultSet, isActivatedByDefault)`. The `Bus` constructor sets `layout = isDfltEnabled ? defaultLayout : disabled`, and asserts `! dfltLayout.isDisabled()` (`juce_AudioProcessor.cpp`, line ~1013).
- VST3 wrapper (`juce_audio_plugin_client_VST3.cpp`, ~3085-3131): first input = `kMain`, every other input = `kAux`; `kDefaultActive` is set only if `isEnabledByDefault()`; `channelCount = getLastEnabledLayout().size()`.
- Proposed declaration:
```cpp
.withInput ("Input", stereo(), true).withOutput ("Output", stereo(), true)
.withInput ("Sidechain 1", stereo(), false)
.withInput ("Sidechain 2", stereo(), false)
```
- `isBusesLayoutSupported` pattern (own code, for T-046), extending the current check at `PluginProcessor.cpp` line 267:
```cpp
for (int i = 1; i < layouts.inputBuses.size(); ++i)
{
    const auto s = layouts.getChannelSet (true, i);
    if (! s.isDisabled() && s != juce::AudioChannelSet::mono() && s != juce::AudioChannelSet::stereo())
        return false;
}
```
  Do not require the aux layouts to match the main layout or each other (VST3 lets the host enable buses in any combination: [JUCE bus tutorial](https://docs.juce.com/master/tutorial_audio_bus_layouts.html), search summary).
- Pitfalls: (a) hosts that enable every bus: the audio code must read `getBus (true, i)->isEnabled()` and `getBusBuffer` per block, never assume a bus has channels. (b) Hosts that require matching layouts: accepting mono and stereo independently covers it. (c) A bus with a disabled default set (current code) gives a zero-channel bus in the VST3 info, which some hosts may not list. (d) Live rejects plugins it cannot configure, so a three-input plugin might fail to load there at all (Live 10 report, weak): the T-046 gate should include "loads in Live 12 with only Sidechain 1".
- pluginval and the Steinberg VST3 validator with three input buses: not documented in what I found. Inferred: both enumerate all buses and try `setBusArrangements` and activation combinations, so the `isBusesLayoutSupported` above must accept every combination (disabled/mono/stereo per aux). Run pluginval at strictness 10 on the T-046 build and report the result.

## Q2. Alt on Windows (Risk 17)

Facts:
- `DefWindowProc` handling of `WM_SYSKEYUP` for the Alt (or F10) key sends `WM_SYSCOMMAND` / `SC_KEYMENU` to the **top-level** window; an application that processes system keys should return 0 ([Microsoft: WM_SYSKEYUP](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-syskeyup), confirmed). Not passing it on to `DefWindowProc` also disables Alt+Tab-style system keys while that window has focus (Microsoft docs, same family of pages).
- JUCE 8.0.15, `juce_Windowing_windows.cpp`:
  - `WM_KEYUP`/`WM_SYSKEYUP` call `doKeyUp`; if it returns false, `forwardMessageToParent` does `PostMessage (GetParent (hwnd), ...)` (~line 3889-3895). So an unconsumed Alt key-up reaches the host's HWND (confirmed).
  - `doKeyUp` returns `handleKeyUpOrDown (false)`, which calls `keyStateChanged (false)` on the focused component and its parents; returning true means "used" (confirmed, `juce_ComponentPeer.cpp` line 235).
  - `SC_KEYMENU` is suppressed for JUCE's own windows unless `JUCE_WINDOWS_ALT_KEY_TRIGGERS_MENU` is set (~line 4043). The comment says it exists because Alt "causes problems if you're trying to use the ALT key as a modifier for mouse actions". The Standalone window is a JUCE window, so Alt-release does nothing there (confirmed by source; not tested).
  - `WM_MENUCHAR` is answered with `MNC_CLOSE` to avoid the beep (confirmed).
- If the plugin HWND does not hold keyboard focus (host kept it, which is common after clicking a host control), Alt key messages go to the host and JUCE never sees them. Then the plugin can only read the key state with polling, not consume it.
- JUCE forum: in Cubase, Studio One, FL Studio and Reaper (Alt) the Alt state seen in `mouseDrag` is stale and `modifierKeysChanged` is not called while dragging ([thread](https://forum.juce.com/t/modifier-keys-in-plugin-builds-bug/49488), a user report). So during an Alt+drag use `getCurrentModifiersRealtime()` rather than `e.mods`.
- Per host: FL Studio, Reaper, Live: whether the menu bar opens after Alt+wheel or Alt+drag in a plugin: **not documented**. A mouse button press during Alt generally cancels the pending Alt-only menu activation in ordinary Windows apps (inferred, not verified), but wheel-only gestures have no such cancel, so Alt+wheel is the riskier one. Bridged FL plugins steal keyboard focus (manual), which makes the plugin the key target and therefore makes mitigation possible there.
- Voxengo and iZotope: no documentation found on how they avoid it (not documented). Do not cite them as proof.

Plugin-side mitigation (inferred from the confirmed mechanism, untested): in the editor component (focus-capable, `setWantsKeyboardFocus (true)`, grab focus on mouseDown/mouseEnter), track `altWasDown` and override `keyStateChanged (bool isDown)`: when `altWasDown && ! isDown` and Alt is now up, return true (consume). Do not return true for other keys, or host shortcuts such as space for transport stop working. Consuming also keeps the Alt-down away from the host only if returned true on the down event too; Alt-down alone does not open the menu, so leave it forwarded. Acceptance test for T-037/T-038/T-059: hold Alt, wheel 5 notches, release Alt; the host menu bar must not highlight, in FL, Reaper, Live and the Standalone.

Fallback modifiers for the user to choose (if the test fails in a major host):
| Option | Level zoom | Box zoom | Notes |
|---|---|---|---|
| A (recommended) | Shift+wheel | Shift+left-drag | Shift release has no Windows menu effect. Conflicts: only with any fine-adjust use of Shift in M3c; check T-042 |
| B | Shift+wheel | Right-button drag | No modifier on the drag. Needs the context menu moved to a non-drag right click |
| C | Ctrl+Shift+wheel | Ctrl+Shift+left-drag | Frees Alt, but the user wanted Ctrl alone for solo; Ctrl+Shift must not trigger the solo. Also collides with the Risk 18 pinch path |
| D | Alt as planned, behind a "Use Alt for zoom" setting | | Mitigation on; user can switch off |
Note for Risk 18: JUCE derives the Ctrl flag from `GetAsyncKeyState`, not from the wheel message's `MK_CONTROL` bit (`updateModifiersFromWParam` only reads mouse buttons). So a touchpad pinch, which Windows sends as a wheel message with `MK_CONTROL` and no physical Ctrl key, probably does not appear as Ctrl in JUCE (inferred; test on a laptop). If so the pinch behaves as a plain wheel, not as solo-width.

## Q3. Modifiers without keyboard focus
**Yes, reliable (confirmed, source).** `ModifierKeys::getCurrentModifiersRealtime()` calls `ComponentPeer::getCurrentModifiersRealtime()`, which calls the peer's `getNativeRealtimeModifiers`. On Windows it is `getMouseModifiers()`, which calls `updateKeyModifiers()` and reads `GetAsyncKeyState` for Shift, Ctrl, Alt (and AltGr mapping) and the three mouse buttons (`juce_Windowing_windows.cpp` ~1681, 3551). That is the global physical key state, independent of focus.
Caveats: (a) it is physical state, so synthetic Ctrl (pinch) is not seen (inferred); (b) a remote desktop or other-desktop session may not report it (inferred); (c) `e.mods` in events can be stale during drags in some hosts (forum thread above), so poll in a `Timer`/vblank callback; (d) AltGr is mapped to Alt without Ctrl by JUCE, so AltGr on non-US layouts counts as Alt, and left-Ctrl+right-Alt combos are handled by the JUCE workaround at line ~1691. Release detection: the poll sees Ctrl up on the next tick; the heartbeat failsafe in Risk 16 still applies.

## Q4. Shared preference file
- **Pattern (confirmed, source):** `PropertiesFile::Options` has `processLock` (an `InterProcessLock*`), `millisecondsBeforeSaving`, `storageFormat`, and `commonToAllUsers`. `reload()` and saving take the process lock; saving writes through a `TemporaryFile` (atomic replace) (`juce_PropertiesFile.cpp` ~138-156, 295). `InterProcessLock` is a named mutex: `CreateMutexW ("Global\\" + name)`, falling back to `Local\\` (`juce_Threads_windows.cpp` ~339-347).
- Use one `juce::SharedResourcePointer<PrefsHolder>` per process, holding the `InterProcessLock` and the `PropertiesFile`. This shares state between instances in one process (inferred; standard JUCE pattern).
- Gap: `PropertiesFile` writes its in-memory copy. Two processes that both changed values can overwrite each other (last writer wins). Safe write recipe: enter an outer `InterProcessLock` scope, `reload()`, apply the one changed key, `saveIfNeeded()`, exit. The lock is a named mutex and the same thread re-entering it through `PropertiesFile` is expected to be safe (inferred; test it, since reentrancy is not documented in the header).
- **Change notification across processes: none** (confirmed: `PropertySet` is a `ChangeBroadcaster` that only fires locally; no file watcher in `juce_core` or `juce_data_structures` in 8.0.15). Options: poll `File::getLastModificationTime()` from an editor timer at 1 s and call `reload()`; or reload when the editor opens (the plan's choice for Risk 19). Polling is enough for a colour/fps preference.
- **Processes:** FL Studio "Make bridged" runs the plugin in a separate process (confirmed, manual). Bitwig hosts plugins "Within Bitwig", "By plug-in", "By manufacturer" or "Individually", so instances can sit in separate processes ([Bitwig user guide](https://www.bitwig.com/userguide/latest/vst_plug-in_handling_and_options/), summary). Hence the file and the named mutex are required, `SharedResourcePointer` alone is not enough. Sandboxed hosts (AppContainer) could block `Global\` mutexes or the folder (inferred; JUCE falls back to `Local\`).
- **Location:** `Options::getDefaultFile()` on Windows uses `File::userApplicationDataDirectory` = `CSIDL_APPDATA` = `%APPDATA%` (roaming), or `%ProgramData%` if `commonToAllUsers`; folder = `folderName` (else `applicationName`); file = `applicationName` + `filenameSuffix` (confirmed, `juce_PropertiesFile.cpp` ~100-120, `juce_Files_windows.cpp` 721). Use `%APPDATA%\<Company>\Investigate\`, not the plugin's own folder. On macOS set `osxLibrarySubFolder = "Application Support"` or JUCE asserts. The plan's `UserDataPaths.h` should build the path with `File::getSpecialLocation`, never a hardcoded string.
- Never touch the file from the audio thread. Defaults on a corrupt file: `loadedOk` tells you whether it parsed (confirmed).

## Q5. File dialogs (`FileChooser::launchAsync`)
Confirmed from source (`juce_FileChooser_windows.cpp`): `Win32NativeFileChooser` runs the dialog on its own thread and calls `dialog.Show (GetActiveWindow())` there (~line 335). On a thread that owns no windows, `GetActiveWindow()` is null, so the native dialog has **no owner**. The JUCE component enters a modal state, which blocks only JUCE windows, not the host.
Consequences (forum reports, user-level, not JUCE-official):
- **Focus/z-order:** in Reaper, Cubase and Pro Tools on Windows 11, clicking the host or moving the plugin window can push the chooser behind the host, with no way to bring it back ([JUCE forum: BUG REPORT: FileChooser](https://forum.juce.com/t/bug-report-filechooser/54889), [Windows FileChooser parent window](https://forum.juce.com/t/windows-filechooser-parent-window/25436)). Hosts do not block their message loop for it; the host stays usable (consistent with the source).
- **Modality:** the old synchronous modal loop is not allowed in plugins (cannot be unwound on plugin unload), so use `launchAsync` only, keep the `FileChooser` as a member (`std::unique_ptr`), and use a `SafePointer` in the callback.
- **Lifetime:** closing the editor or unloading the plugin with a dialog open: the dialog survives ([thread](https://forum.juce.com/t/windows-native-file-dialog-stays-open-after-closing-audio-editor/44201), reports). Cancel on editor destruction (destroying the `FileChooser` signals the thread to close it, ~lines 90-110).
- **DPI:** not documented as an issue; the native dialog is system-owned and DPI-aware per the host process (inferred). FL bridged plugins: not documented.
- Recommendation: use `FileBrowserComponent::canSelectFiles | saveMode`, `useNativeBox = true`, parent component = the editor (so the Windows build keeps its modal state), and have a fallback to non-native `useNativeBox = false` if the QA run finds a host where the dialog hides. T-056/T-057 should test FL Studio (bridged and not), Reaper, Live.

## Q6. Band-solo filter (T-050)

### Recommendation
**Topology:** two identical 2nd-order TPT state-variable band-pass stages in series (4th order overall), with a unity-peak band-pass output. Steps per sample (Simper/Cytomic, [SvfLinearTrapOptimised2](https://www.cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf); equations re-derived from the paper's well-known form, check against the PDF in T-050):
```
g = tan(pi * fc / fs);  k = 1 / Q
a1 = 1 / (1 + g*(g+k));  a2 = g*a1;  a3 = g*a2
v3 = x - ic2eq
v1 = a1*ic1eq + a2*v3            // band-pass node
v2 = ic2eq + a2*ic1eq + a3*v3    // low-pass node
ic1eq = 2*v1 - ic1eq;  ic2eq = 2*v2 - ic2eq
y = k * v1                       // unity gain at fc (v1 alone peaks at Q)
```
**Width to band edges:** the solo band is W octaves wide, geometric around the pointer frequency fc: f_lo = fc·2^(−W/2), f_hi = fc·2^(W/2), f_lo·f_hi = fc². For a 2nd-order band-pass the −3 dB edges satisfy Q·(f_hi/fc − fc/f_hi) = 1, so for one stage Q₁ = 1 / (2^(W/2) − 2^(−W/2)) = 2^(W/2)/(2^W − 1) (matches the standard octave-bandwidth formula, [sengpielaudio](https://sengpielaudio.com/calculator-bandwidth.htm), [MathWorks octavebw2q](https://www.mathworks.com/help/audio/ref/octavebw2q.html)). With two identical stages the cascade is −3 dB at the edges when each stage is −1.5 dB there: Q_stage = sqrt(√2 − 1) / (2^(W/2) − 2^(−W/2)) ≈ 0.6436 / (2^(W/2) − 2^(−W/2)) (own derivation).

| Width W | Q one stage | Q per stage (2 stages) | Centre gain | Edge gain | Skirt slope |
|---|---|---|---|---|---|
| 1/12 oct | 17.31 | 11.14 | 0 dB | −3 dB | 12 dB/oct |
| 1/6 | 8.65 | 5.57 | 0 dB | −3 dB | 12 |
| 1/3 | 4.32 | 2.78 | 0 dB | −3 dB | 12 |
| 1 | 1.41 | 0.91 | 0 dB | −3 dB | 12 |
| 2 | 0.67 | 0.43 | 0 dB | −3 dB | 12 |
| 4 | 0.27 | 0.17 | 0 dB | −3 dB | 12 |
(Computed with a short script; the centre gain is exactly 1 for the unity-peak band-pass at any Q, so it needs no compensation.) The passband is a bell, not flat. A "flat" top at narrow widths is not possible with a band-pass this narrow.

### Comparison with a Linkwitz-Riley pair (HP at f_lo, LP at f_hi, LR4 each)
LR4 is two cascaded 2nd-order Butterworth sections, −6 dB at its corner ([Linkwitz-Riley filter](https://en.wikipedia.org/wiki/Linkwitz%E2%80%93Riley_filter)). The centre gain of the pair is 1/(1 + 2^(−2W))² (own derivation):
| W | 1/12 | 1/6 | 1/3 | 1 | 2 | 4 |
|---|---|---|---|---|---|---|
| LR pair centre gain | −11.1 dB | −10.2 | −8.5 | −3.9 | −1.05 | −0.07 |
| LR pair edge gain | −11.1 | −10.3 | −8.9 | −6.6 | −6.05 | −6.0 |
At 1/12 and 1/3 octave the pair is no band-pass: the centre and the edges differ by under 0.5 dB, so it neither isolates nor sounds like a band. It only works from about 2 octaves up, where it is flatter than the SVF (steeper 24 dB/oct skirts, 0 dB centre within 1 dB). Direct-form biquads also click when their coefficients are modulated per sample; TPT/SVF forms tolerate it. A resonant biquad band-pass (RBJ constant-0-dB-peak) has the same shape as the SVF stage but weaker modulation behaviour.

### Stability and modulation
- The Simper/TPT SVF is built for audio-rate modulation of cutoff and Q (the paper's stated aim: zero-delay-feedback, trapezoidal; Zavalishin ch. 3-4 [The Art of VA Filter Design](https://www.native-instruments.com/fileadmin/ni_media/downloads/pdf/VAFilterDesign_2.1.0.pdf), rev 2.1.2 Feb 2020). The state is the capacitor voltages ic1eq/ic2eq, so changing coefficients does not inject a transient the way a direct-form biquad's stored samples do.
- Smoothing: smooth log2(fc) and W (not Q) with a one-pole (about 10 to 20 ms; the pointer follows the mouse), recompute g, k, a1..a3 every 16 or 32 samples (or per sample if the CPU allows), and clamp fc to [20 Hz, 0.45·fs]. Q ranges 0.17 to 11, which keeps the filter well inside its stable range (k = 1/Q between about 0.09 and 6).
- Ring time: a stage with Q = 11 at 100 Hz has a time constant near Q/(π·fc) ≈ 35 ms, so the narrowest band rings audibly at low frequencies. Expected, document it.
- Engage and release: run the filter from the start of the engage ramp and use a 10 ms raised-cosine crossfade between the dry pass-through and the filtered signal, as in the plan. Because the filtered signal is phase-shifted against the dry one, an equal-power crossfade is not needed, but the mid-ramp level dips by up to a few dB for narrow bands; this is acceptable at 10 ms. Release: ramp back to dry, keep the filter running until the ramp ends, then reset the states to zero so the next engage starts clean. (Engineering judgement; no source found that prescribes this.)

### Licences of code considered
| Reference | Licence | Verdict |
|---|---|---|
| Cytomic technical papers, SvfLinearTrapOptimised2 | "placed in the public domain" ([cytomic.com/technical-papers](https://cytomic.com/technical-papers/)) | Usable (equations and own implementation; keep a citation comment) |
| Zavalishin, The Art of VA Filter Design (PDF) | Free PDF, © Native Instruments; no code licence stated | Usable as a reference for equations; do not copy text or figures |
| RBJ Audio EQ Cookbook | No licence in the document (public usage widely assumed) | Reference only; not needed |
| JUCE `dsp::StateVariableTPTFilter` | Part of JUCE (AGPLv3 or the JUCE 8 commercial licence, per `LICENSE.md`); the project's JUCE licence applies | Usable under the project's JUCE licence, but fixed topology and no Q-from-octave mapping; own class is simpler |

## Q7. Colour wheel
- **`juce::ColourSelector` (JUCE 8.0.15, confirmed):** flags `showAlphaChannel`, `showColourAtTop`, `editableColour`, `showSliders`, `showColourspace`. The "colourspace" is a **square HSV field**: a hue strip plus a saturation/brightness square, plus RGB sliders and an alpha slider. It has `setCurrentColour`, `getCurrentColour`, and a `ChangeListener` hook. It has **no circular hue wheel, no hex field, no swatches/presets, no HSV/HSL numeric fields** (confirmed by header and class reference).
- **Options:**
  - Use `ColourSelector` as is, in a `CallOutBox`, with an own hex `TextEditor` and a preset row beside it. Lowest cost.
  - [reFX/ColourSelector](https://github.com/reFX/ColourSelector): "A better color selector for JUCE", Photoshop-like, drop-in API in namespace `reFX`. Licence: **MIT** (GitHub API SPDX), last push 2026-03-23, not archived. **Verdict: usable** (attribution; add to THIRD_PARTY notices). Whether it has a round wheel: not documented in its README; read `refx_colourselector/` before choosing.
  - Own wheel: a ~150-line `juce::Component` drawing hue as an `Image`-cached conic gradient (pixel loop with `Colour::fromHSV`) and a saturation/brightness square, using only JUCE. No third-party licence. Recommended if the user insists on a circular wheel.
  - [matheuslazarotto/ColorWheelApp](https://github.com/matheuslazarotto/ColorWheelApp) (HSV/RGB mapping widget): licence not verified in this session. **Verdict: rejected** until a LICENSE file is confirmed; not JUCE.
- Contrast warning (Risk 20): `juce::Colour::getPerceivedBrightness()` and a WCAG ratio computed from `getRed()/getGreen()/getBlue()` are available in JUCE and need no library.

## Licences and risks
- No strong-copyleft code is recommended. Nothing from Scrollscope (no licence) or ColorWheelApp (unverified) may be used.
- JUCE itself: AGPLv3 or the JUCE 8 commercial licence (`third_party/juce-src/LICENSE.md`, current official text). An AGPL build means the plugin must be AGPL-compatible; this is a project-level decision outside this task, noted only because the Decisions on licences say "no strong copyleft".
- Risks: (a) Risk 17 stays open until tested in FL Studio, Reaper, Live. (b) Risk 15 is real for Live and Bitwig. (c) Disabled default layout on the aux bus (current code) must be fixed before T-046 or the second bus may not appear in some hosts. (d) Risk 19: no cross-process notification, use poll or reload on open. (e) Q5: the chooser can fall behind the host window; QA needed.

## Sources
- https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/plugins/wrapper.htm
- https://forum.image-line.com/viewtopic.php?f=100&t=243147
- https://github.com/ardura/Scrollscope
- https://iplug2.discourse.group/t/2-or-more-sidechain-input-buses-3-or-more-total-inputs-tried-many-things-but-cant-get-a-second-sidechain-bus-to-connect/1009
- https://forums.steinberg.net/t/sidechain-protocol-multiple-locations-within-the-same-plugin/1019829
- https://www.fabfilter.com/forum/add-comment?id=3113
- https://www.kvraudio.com/forum/viewtopic.php?t=552244
- https://docs.juce.com/master/tutorial_audio_bus_layouts.html
- https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-syskeyup
- https://forum.juce.com/t/modifier-keys-in-plugin-builds-bug/49488
- https://forum.juce.com/t/bug-report-filechooser/54889
- https://forum.juce.com/t/windows-filechooser-parent-window/25436
- https://forum.juce.com/t/windows-native-file-dialog-stays-open-after-closing-audio-editor/44201
- https://www.bitwig.com/userguide/latest/vst_plug-in_handling_and_options/
- https://www.cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf and https://cytomic.com/technical-papers/
- https://www.native-instruments.com/fileadmin/ni_media/downloads/pdf/VAFilterDesign_2.1.0.pdf
- https://sengpielaudio.com/calculator-bandwidth.htm, https://www.mathworks.com/help/audio/ref/octavebw2q.html
- https://en.wikipedia.org/wiki/Linkwitz%E2%80%93Riley_filter
- https://github.com/reFX/ColourSelector
- Local, JUCE 8.0.15: `modules/juce_gui_basics/native/juce_Windowing_windows.cpp`, `juce_FileChooser_windows.cpp`, `windows/juce_ComponentPeer.cpp`; `modules/juce_audio_plugin_client/juce_audio_plugin_client_VST3.cpp`; `modules/juce_audio_processors_headless/processors/juce_AudioProcessor.cpp`; `modules/juce_data_structures/app_properties/juce_PropertiesFile.*`; `modules/juce_core/threads/juce_InterProcessLock.h`; `modules/juce_core/native/juce_Threads_windows.cpp`; `modules/juce_gui_extra/misc/juce_ColourSelector.h`; `LICENSE.md`.
