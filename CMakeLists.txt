cmake_minimum_required(VERSION 3.22)

# Refuse in-source builds: they litter the repository with generated files.
# CMake has already written CMakeCache.txt and CMakeFiles/ by the time this
# runs, so the message tells the user to delete them.
get_filename_component(_inv_src "${CMAKE_SOURCE_DIR}" REALPATH)
get_filename_component(_inv_bin "${CMAKE_BINARY_DIR}" REALPATH)
if(_inv_src STREQUAL _inv_bin)
    message(FATAL_ERROR
        "In-source builds are not supported. Configure with 'cmake --preset <name>' "
        "(for example windows-msvc) or 'cmake -S . -B build'. Delete the CMakeCache.txt "
        "file and CMakeFiles/ directory that this attempt just created in ${CMAKE_SOURCE_DIR}.")
endif()

project(Investigate VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PLUGIN_BUILD_TESTS "Build the unit test target and register CTest tests" ON)
option(PLUGIN_BUILD_HARNESS "Build the render harness tool" ON)

# Local path to a manually downloaded and extracted EBU loudness test set
# (v5.0, tech.ebu.ch/publications/ebu_loudness_test_set). There is no
# automated fetch script for this fixture (see docs/dependencies.md and the
# T-021 task log): the EBU's terms of use are an accept-by-downloading
# clickwrap restricted to internal, non-commercial R&D use, which this
# project treats as requiring a human to read and accept them, not a CI
# script. A developer who has done so may unpack the set at this path
# (gitignored, since it lives under third_party/) so the true-peak/LRA unit
# tests that need bit-exact reference audio can find it and skip cleanly
# otherwise.
set(INVESTIGATE_EBU_TEST_SET_DIR "${CMAKE_SOURCE_DIR}/third_party/_fixtures/ebu" CACHE PATH
    "Local folder containing the manually downloaded EBU loudness test set (v5.0), or a missing/empty folder to skip the tests that need it")

# Local-only folder with the user's own program material (T-032), currently
# test_audio.wav. Never committed (third_party/ is gitignored, and root audio
# files are ignored too). Tests and the render harness read the path from the
# INVESTIGATE_TEST_AUDIO_DIR compile definition, check that the file exists and
# skip cleanly when it does not (for example in CI). A missing folder never
# fails configuration.
set(INVESTIGATE_TEST_AUDIO_DIR "${CMAKE_SOURCE_DIR}/third_party/_fixtures/audio" CACHE PATH
    "Local folder with user-provided test audio (test_audio.wav); never committed, tests skip when it is missing")
message(STATUS "INVESTIGATE_TEST_AUDIO_DIR = ${INVESTIGATE_TEST_AUDIO_DIR}")

# ---------------------------------------------------------------------------
# Dependencies. Pinned versions live here; see docs/dependencies.md for the
# rationale, licenses and update procedure. All fetched sources are placed
# under third_party/ (gitignored - never vendored) rather than the CMake
# default _deps directory, so the layout matches CLAUDE.md's ownership map.
# ---------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/third_party")

set(INVESTIGATE_JUCE_TAG "8.0.15")
set(INVESTIGATE_CATCH2_TAG "v3.16.0")
set(INVESTIGATE_PFFFT_TAG "v1.1.0")

FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG ${INVESTIGATE_JUCE_TAG}
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(juce)

# pffft (marton78 fork, BSD-like), the product FFT library (T-021, roadmap
# risk #2). SOURCE_SUBDIR points at a directory that does not exist in the
# pffft tree, so FetchContent_MakeAvailable only clones/populates the source
# and does NOT add_subdirectory() pffft's own CMakeLists.txt (which builds
# double-precision code, tests and benchmarks we do not want). We hand-build
# a minimal single-precision-only static library from the populated source
# below instead.
FetchContent_Declare(pffft
    GIT_REPOSITORY https://github.com/marton78/pffft.git
    GIT_TAG ${INVESTIGATE_PFFFT_TAG}
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    SOURCE_SUBDIR investigate-unused-do-not-configure
)
FetchContent_MakeAvailable(pffft)

# Only pffft.c and pffft_common.c are required for single-precision real and
# complex transforms. pffft_priv_impl.h (included by pffft.c) vendors its own
# fftpack-derived twiddle-factor generation (rffti1_ps/cffti1_ps), so
# fftpack.c is not linked here - upstream only uses it for its own
# benchmarks/tests, which this project does not build. pffft_double.c (double
# precision) and the pf_carrier/pf_cic/pf_mixer/pffastconv extras are out of
# scope: the DSP pipeline only needs single-precision real/complex FFTs.
add_library(investigate_pffft STATIC
    "${pffft_SOURCE_DIR}/src/pffft.c"
    "${pffft_SOURCE_DIR}/src/pffft_common.c"
)
set_target_properties(investigate_pffft PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)
target_include_directories(investigate_pffft
    PUBLIC "${pffft_SOURCE_DIR}/include/pffft"
    PRIVATE "${pffft_SOURCE_DIR}/src"
)
# PFFFT_STATIC_DEFINE: pffft.h's PFFFT_EXPORT macro defaults to
# __declspec(dllimport) on Windows unless told the library is static, which
# fails to compile investigate_pffft's own .c files (a static lib is not
# importing its own symbols). PUBLIC so every consumer (Investigate,
# InvestigateUnitTests) sees the same macro expansion the static library was
# built with.
target_compile_definitions(investigate_pffft PUBLIC PFFFT_STATIC_DEFINE)
# Third-party code: suppress its warnings instead of fixing them, so project
# code stays warning-clean at /W4 without silencing warnings globally (never
# add /w or /W0 to the Investigate or InvestigateUnitTests targets).
target_compile_options(investigate_pffft PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/w>
)
# SIMD: marton78/pffft's single-precision path (simd/pf_sse1_float.h) enables
# SSE (SIMD_SZ=4) automatically whenever _M_X64 is defined, which MSVC always
# defines for an x64 target - no extra compiler flags are needed. The fork
# has no AVX implementation for single precision (only for double, which
# this project does not use via investigate_pffft), so there is no further
# AVX opt-in available to enable here; see docs/dependencies.md.

if (PLUGIN_BUILD_TESTS)
    FetchContent_Declare(catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG ${INVESTIGATE_CATCH2_TAG}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(catch2)

    enable_testing()
endif ()

# ---------------------------------------------------------------------------
# Plugin target
# ---------------------------------------------------------------------------
juce_add_plugin(Investigate
    PRODUCT_NAME "Investigate"
    COMPANY_NAME "Investigate Audio"
    PLUGIN_MANUFACTURER_CODE Ivga
    PLUGIN_CODE Ivst
    BUNDLE_ID "com.investigateaudio.investigate"
    FORMATS VST3 Standalone
    VST3_CATEGORIES Fx Analyzer
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(Investigate)

target_compile_definitions(Investigate
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        # T-025: activates source/StandaloneApp.cpp (plugin-engineer's custom
        # Standalone app, which starts "Mute audio input" off per roadmap
        # risk #7 / T-002 section 9) in place of JUCE's stock Standalone
        # app. See T-008 open question 1.
        JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
        # T-025 (moved from T-014, per docs/research/T-023-third-party-notices.md
        # section 3): Investigate never reads or writes audio files, so these
        # codecs are unused. Setting them to 0 excludes their source from
        # compilation, reducing binary size and removing their notice
        # obligations from docs/dependencies.md and THIRD-PARTY-NOTICES.txt.
        JUCE_USE_FLAC=0
        JUCE_USE_OGGVORBIS=0
        JUCE_USE_WINDOWS_MEDIA_FORMAT=0
)

target_compile_features(Investigate PRIVATE cxx_std_20)

# PUBLIC (not PRIVATE) juce_audio_utils/juce_dsp: InvestigateUnitTests links
# this target as its shared code and needs the JUCE module include paths to
# transitively reach it, per the approach documented in docs/dependencies.md.
target_link_libraries(Investigate
    PUBLIC
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
        investigate_pffft
)

# Project code (everything under source/) is compiled at /W4 on MSVC. JUCE's own
# sources keep whatever warning suppression juce_recommended_warning_flags applies;
# we never touch warnings globally.
target_compile_options(Investigate PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/W4>
)

# ---------------------------------------------------------------------------
# Source globbing. Coders add .cpp/.h files under these directories and the
# next configure (CONFIGURE_DEPENDS re-runs CMake automatically) picks them up
# without any CMakeLists.txt edit. GLOB_RECURSE with a "source/*.cpp"-style
# pattern already descends into source/dsp, source/params and source/gui, so
# the four glob results below are de-duplicated before being handed to
# target_sources.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE INVESTIGATE_SOURCES_TOP    CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/source/*.cpp")
file(GLOB_RECURSE INVESTIGATE_SOURCES_DSP    CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/source/dsp/*.cpp")
file(GLOB_RECURSE INVESTIGATE_SOURCES_PARAMS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/source/params/*.cpp")
file(GLOB_RECURSE INVESTIGATE_SOURCES_GUI    CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/source/gui/*.cpp")

set(INVESTIGATE_SOURCES
    ${INVESTIGATE_SOURCES_TOP}
    ${INVESTIGATE_SOURCES_DSP}
    ${INVESTIGATE_SOURCES_PARAMS}
    ${INVESTIGATE_SOURCES_GUI}
)
list(REMOVE_DUPLICATES INVESTIGATE_SOURCES)

target_sources(Investigate PRIVATE ${INVESTIGATE_SOURCES})
# PUBLIC so InvestigateUnitTests (which links this target as shared code) can
# #include headers under source/ without a duplicate include path definition.
target_include_directories(Investigate PUBLIC "${CMAKE_SOURCE_DIR}/source")

# ---------------------------------------------------------------------------
# Assets. Only created when at least one asset file exists, so an empty
# assets/ui/ still configures cleanly.
#
# Fonts (Inter, JetBrains Mono) and the license texts they and pffft require
# are pinned third-party files (T-021, roadmap decision 2026-09-28): they are
# downloaded with checksum verification into third_party/fonts/, not added to
# assets/ui/ (owned by ui-ux-designer), and compiled into the same
# InvestigateAssets binary-data target as the SVGs below - a single target,
# because two targets would both default to the BinaryData namespace and
# clash.
# ---------------------------------------------------------------------------
include("${CMAKE_SOURCE_DIR}/cmake/DownloadFonts.cmake")

file(GLOB_RECURSE INVESTIGATE_ASSET_FILES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/assets/ui/*.svg"
    "${CMAKE_SOURCE_DIR}/assets/ui/*.ttf"
    "${CMAKE_SOURCE_DIR}/assets/ui/*.otf"
    "${CMAKE_SOURCE_DIR}/assets/ui/*.png"
    "${CMAKE_SOURCE_DIR}/third_party/fonts/*.ttf"
    "${CMAKE_SOURCE_DIR}/third_party/fonts/*.txt"
)

if (INVESTIGATE_ASSET_FILES)
    juce_add_binary_data(InvestigateAssets SOURCES ${INVESTIGATE_ASSET_FILES})
    target_link_libraries(Investigate PRIVATE InvestigateAssets)
endif ()

# ---------------------------------------------------------------------------
# Tests and validation tooling
# ---------------------------------------------------------------------------
if (PLUGIN_BUILD_TESTS)
    add_subdirectory(tests)
endif ()

# ---------------------------------------------------------------------------
# Guard: libebur128 (declared in tests/CMakeLists.txt as investigate_libebur128)
# is a test-only loudness oracle and must never reach the shipped plugin
# binary (T-021 scope item 2). This runs after tests/ has been configured, so
# it sees Investigate's final link-library list.
# ---------------------------------------------------------------------------
if (TARGET investigate_libebur128)
    get_target_property(_investigate_plugin_link_libs Investigate LINK_LIBRARIES)
    if (_investigate_plugin_link_libs AND "investigate_libebur128" IN_LIST _investigate_plugin_link_libs)
        message(FATAL_ERROR "libebur128 (investigate_libebur128) must never be linked into the Investigate plugin target; it is test-only. See docs/dependencies.md.")
    endif ()

    get_target_property(_investigate_plugin_iface_libs Investigate INTERFACE_LINK_LIBRARIES)
    if (_investigate_plugin_iface_libs AND "investigate_libebur128" IN_LIST _investigate_plugin_iface_libs)
        message(FATAL_ERROR "libebur128 (investigate_libebur128) must never be linked into the Investigate plugin target; it is test-only. See docs/dependencies.md.")
    endif ()
endif ()

# Reserved for T-014: the render harness lives under tools/render-harness/ and
# is only added once that task creates its CMakeLists.txt, so an empty tree
# still configures.
if (PLUGIN_BUILD_HARNESS AND EXISTS "${CMAKE_SOURCE_DIR}/tools/render-harness/CMakeLists.txt")
    add_subdirectory(tools/render-harness)
endif ()
