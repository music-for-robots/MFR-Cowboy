cmake_minimum_required(VERSION 3.22)
project(MFRCowboy VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Offline DSP tests (no JUCE needed)
add_executable(CowboyTest tools/CowboyTest.cpp)

option(COWBOY_BUILD_PLUGIN "Build the plugin" ON)
if(COWBOY_BUILD_PLUGIN)
    add_subdirectory(JUCE)

    juce_add_plugin(Cowboy
        COMPANY_NAME "Music For Robots"
        PLUGIN_MANUFACTURER_CODE MfRb
        PLUGIN_CODE Cwby
        BUNDLE_ID com.musicforrobots.mfrcowboy
        FORMATS VST3 Standalone
        PRODUCT_NAME "MFR-Cowboy"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        VST3_CATEGORIES Fx Distortion Reverb
        COPY_PLUGIN_AFTER_BUILD FALSE)

    target_sources(Cowboy PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

    target_compile_definitions(Cowboy PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(Cowboy
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    # Renders the UI to a PNG with no audio device (for docs and the website).
    juce_add_console_app(CowboySnapshot PRODUCT_NAME "CowboySnapshot")
    target_sources(CowboySnapshot PRIVATE
        tools/Snapshot.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_compile_definitions(CowboySnapshot PRIVATE
        "JucePlugin_VersionString=\"${PROJECT_VERSION}\""
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(CowboySnapshot
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
