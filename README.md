# Investigate

Investigate is a free, real-time audio analyser plugin for mixing and
mastering engineers. It shows a high-resolution FFT spectrum with adjustable
block size, window, overlap, slope, time averaging and fractional-octave
smoothing, a secondary curve for holding a real-time or all-time maximum or
average, and a hover readout of frequency, musical note and level. Alongside
the spectrum it shows level and loudness statistics (sample peak, RMS, true
peak, clip counts, and EBU R128 momentary, short-term and integrated
loudness plus loudness range), a stereo correlation meter, and Mid/Side and
per-channel spectrum modes. It also accepts a sidechain input and draws it
as a second, reference spectrum, so a mix can be compared against another
track. Investigate never changes the audio: the signal passes through the
plugin bit-exact, with zero added latency.

## Status

In development. Milestones M0 to M2 are complete: the project scaffold, the
DSP core (spectrum analysis and metering) and the processor wiring
(parameters, state, buses and the analysis thread) all have passing test
reports. The editor UI (M3) is next, followed by validation (M4) and Windows
packaging (M5). See the live Status table, decisions and open questions in
[`docs/roadmap.md`](docs/roadmap.md).

## Features (v1 scope)

- Real-time FFT spectrum: block size 512–65536, four window functions,
  overlap, time averaging, fractional-octave smoothing, and slope with a
  1 kHz pivot — **implemented**
- A secondary curve (real-time max, all-time max or all-time average) and a
  freeze toggle — **implemented**
- Channel modes: Mid, Side, Left, Right, L+R power mean, and L/R and M/S dual
  curves — **implemented**
- A sidechain reference spectrum drawn as its own curve — **implemented**
- Level meters: sample peak, RMS, true peak (BS.1770 oversampling), sample
  and true-peak clip counters, and held peak maxima — **implemented**
- Loudness to ITU-R BS.1770 / EBU R128: momentary, short-term and integrated
  loudness, and loudness range — **implemented**
- A stereo correlation meter and an L/R balance readout — **implemented**
- State save and restore, and a resizable editor — **implemented**
  (processor side; the resizable editor itself is planned)
- Interactive editor: spectrum view, meter panel, a hover readout with
  frequency and musical note, click-to-reset, right-click-to-copy-frequency,
  and a collapsible meter panel — **planned** (M3)
- A signed Windows installer for a free public release — **planned** (M5)
- Continuous integration on Windows (build, test, pluginval) — **planned**
  (M5)

## Requirements

- Windows 11 x64
- Visual Studio 2022 (MSVC), with the "Desktop development with C++"
  workload
- CMake 3.22 or newer
- Git
- Network access on the first configure: CMake's `FetchContent` downloads
  JUCE, Catch2, pffft and (for test builds) libebur128, and also downloads
  pluginval and the bundled font files. Later configures reuse the
  `third_party/` cache.

## Build, test and validate

From the repository root, using the `windows-msvc` CMake preset:

```
cmake --preset windows-msvc
cmake --build --preset debug
cmake --build --preset release
ctest --preset release                 # unit tests (Catch2)
ctest --preset release -L validation   # pluginval at strictness 10
```

The hidden performance tests (tagged `[.perf]`, excluded from the default
`ctest` run so timing noise never fails the suite) are run explicitly against
the Release test binary:

```
build/tests/Release/InvestigateUnitTests.exe "[perf]"
```

Build outputs land under `build/` (gitignored):

- VST3 plugin: `build/Investigate_artefacts/Release/VST3/Investigate.vst3`
- Standalone app: `build/Investigate_artefacts/Release/Standalone/Investigate.exe`
- Unit test executable: `build/tests/Release/InvestigateUnitTests.exe`

(Debug builds use the same layout under `Debug` instead of `Release`.)

## Repository layout

```
CMakeLists.txt, CMakePresets.json   Build configuration and presets
cmake/                              Committed CMake helper scripts (dependency downloads)
source/                             Plugin source: processor, parameters, DSP (source/dsp/) and editor (source/gui/)
assets/ui/                          UI artwork (SVG icons and logo marks)
tests/                               Catch2 unit tests and the CTest/pluginval integration
tools/render-harness/               Offline render harness for validation
docs/                                Roadmap, task files, design spec, research and QA reports
```

`third_party/` (fetched dependencies) and `build/` (build output) are
gitignored and not part of the repository.

## Documentation index

- [`docs/roadmap.md`](docs/roadmap.md): vision, scope, architecture,
  milestones, the live status table and the decisions log
- [`docs/tasks/`](docs/tasks/): the task file for every unit of work
- [`docs/design/ui-spec.md`](docs/design/ui-spec.md) and
  [`docs/design/mockup.html`](docs/design/mockup.html): the approved UI
  specification and its HTML mockup
- [`docs/research/`](docs/research/): feature-parity, DSP-standards and
  dependency-license research
- [`docs/qa/`](docs/qa/): unit and validation test reports
- [`docs/dependencies.md`](docs/dependencies.md): pinned dependency
  versions, licenses and update procedures

## Third-party components

Investigate links the JUCE framework and the VST3 SDK it bundles, and the
pffft FFT library; its editor embeds the Inter and JetBrains Mono
typefaces. Test builds additionally use Catch2 as the test framework,
libebur128 as a test-only loudness oracle, and pluginval as an out-of-process
host validator; none of these three is linked into the shipped plugin. Full
version, license and notice details are in
[`docs/dependencies.md`](docs/dependencies.md) and
[`docs/research/T-023-third-party-notices.md`](docs/research/T-023-third-party-notices.md).
VST is a registered trademark of Steinberg Media Technologies GmbH.

## License

All rights reserved. The licensing model for the public release is not yet
decided.
