# Unit test target. It links Investigate's shared-code target (the one
# juce_add_plugin creates, compiled once and reused by Investigate_VST3 and
# Investigate_Standalone) so tests can call into source/dsp/ and source/params/
# code without JUCE being compiled a second time in a conflicting
# configuration. See docs/dependencies.md for the rationale.
file(GLOB_RECURSE INVESTIGATE_UNIT_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp"
)

# ---------------------------------------------------------------------------
# libebur128, a test-only BS.1770/EBU R128 loudness oracle (T-021 scope item
# 2, roadmap decision 2026-09-28). Linked only into InvestigateUnitTests -
# never into Investigate; the root CMakeLists.txt has a configure-time check
# that fails the build if that rule is ever broken.
# ---------------------------------------------------------------------------
set(INVESTIGATE_LIBEBUR128_TAG "v1.2.6")

FetchContent_Declare(libebur128
    GIT_REPOSITORY https://github.com/jiixyj/libebur128.git
    GIT_TAG ${INVESTIGATE_LIBEBUR128_TAG}
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    SOURCE_SUBDIR investigate-unused-do-not-configure
)
FetchContent_MakeAvailable(libebur128)

add_library(investigate_libebur128 STATIC
    "${libebur128_SOURCE_DIR}/ebur128/ebur128.c"
)
target_include_directories(investigate_libebur128
    PUBLIC "${libebur128_SOURCE_DIR}/ebur128"
    # libebur128 unconditionally includes <sys/queue.h>, which MSVC's CRT
    # does not provide; this ships the project's own BSD-derived compat
    # header, matching upstream's ENABLE_INTERNAL_QUEUE_H path for non-BSD
    # platforms (see ebur128/CMakeLists.txt in the libebur128 source tree).
    PRIVATE "${libebur128_SOURCE_DIR}/ebur128/queue"
)
target_compile_definitions(investigate_libebur128 PRIVATE _USE_MATH_DEFINES)
# Third-party code: suppress its warnings instead of fixing them, so project
# code stays warning-clean at /W4 without silencing warnings globally.
target_compile_options(investigate_libebur128 PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/w>
)

add_executable(InvestigateUnitTests ${INVESTIGATE_UNIT_TEST_SOURCES})

target_link_libraries(InvestigateUnitTests
    PRIVATE
        Investigate
        Catch2::Catch2WithMain
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
        investigate_libebur128
)

target_compile_definitions(InvestigateUnitTests PRIVATE
    INVESTIGATE_EBU_TEST_SET_DIR="${INVESTIGATE_EBU_TEST_SET_DIR}"
    INVESTIGATE_TEST_AUDIO_DIR="${INVESTIGATE_TEST_AUDIO_DIR}"
)

target_compile_options(InvestigateUnitTests PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/W4>
)

list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)
catch_discover_tests(InvestigateUnitTests)

# ---------------------------------------------------------------------------
# pluginval, registered as a CTest test labelled "validation" so it can be
# run on its own with `ctest -L validation`. Runs against the built VST3.
# ---------------------------------------------------------------------------
if (WIN32)
    include("${CMAKE_SOURCE_DIR}/cmake/DownloadPluginval.cmake")

    add_test(NAME pluginval
        COMMAND "${PLUGINVAL_EXECUTABLE}"
                --strictness-level 10
                --validate-in-process
                "$<TARGET_FILE:Investigate_VST3>"
    )
    set_tests_properties(pluginval PROPERTIES
        LABELS "validation"
        TIMEOUT 300
    )
endif ()
