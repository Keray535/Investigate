# T-008 black-box render harness (plugin-validator). Links the Investigate
# shared-code target directly (the "link the processor directly" option
# described in the plugin-validator agent's own brief), the same technique
# tests/unit/CMakeLists.txt uses for InvestigateUnitTests. This drives only
# the public juce::AudioProcessor host-facing surface (isBusesLayoutSupported,
# setBusesLayout, processBlock, getStateInformation/setStateInformation, the
# AudioProcessorParameter interface, getLatencySamples/getTailLengthSeconds)
# -- the same surface a real VST3/Standalone host uses -- not any internal
# DSP class. It does not replace pluginval (still run via CTest, see
# tests/CMakeLists.txt); it covers the specific matrices (bus layouts, null
# test across block sizes/sample rates, state round trip at the binary level)
# that pluginval does not enumerate itself.
#
# Included by the top-level CMakeLists.txt's existing guarded
# add_subdirectory(tools/render-harness) when PLUGIN_BUILD_HARNESS is ON and
# this file exists -- no root CMakeLists.txt edit needed.

add_executable(InvestigateRenderHarness
    main.cpp
)

target_link_libraries(InvestigateRenderHarness
    PRIVATE
        Investigate
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

target_compile_options(InvestigateRenderHarness PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/W4>
)

# T-011 editor black-box check (plugin-validator): offscreen screenshots,
# region-edge measurements, open/close cycles, size persistence, automation
# to control, Freeze auto-unfreeze. Drives the public editor and processor
# surface (createEditor, createComponentSnapshot, the APVTS).
add_executable(InvestigateEditorCheck
    editor_check.cpp
)

target_include_directories(InvestigateEditorCheck PRIVATE "${CMAKE_SOURCE_DIR}/source")

target_link_libraries(InvestigateEditorCheck
    PRIVATE
        Investigate
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

target_compile_options(InvestigateEditorCheck PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/W4>
)

# T-012 spectrum view black-box check (plugin-validator): 1 kHz placement,
# hover/freeze/click/right-click, curve and flag screenshots, paint benchmark.
add_executable(InvestigateSpectrumCheck
    spectrum_check.cpp
)

target_include_directories(InvestigateSpectrumCheck PRIVATE "${CMAKE_SOURCE_DIR}/source")

target_link_libraries(InvestigateSpectrumCheck
    PRIVATE
        Investigate
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

target_compile_options(InvestigateSpectrumCheck PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/W4>
)
