cmake_minimum_required(VERSION 3.22)

if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

project(Vireo VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)

# Use a local JUCE checkout if there is one, otherwise fetch it
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt)
  add_subdirectory(JUCE)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
  set(VIREO_FORMATS AU VST3 Standalone)
else()
  set(VIREO_FORMATS VST3 Standalone)
endif()

juce_add_plugin(Vireo
  COMPANY_NAME "bxzex"
  COMPANY_WEBSITE "https://bxzex.com"
  COMPANY_COPYRIGHT "Copyright 2026 bxzex"
  COMPANY_EMAIL ""
  DESCRIPTION "Vireo polyphonic synthesizer by bxzex"
  BUNDLE_ID com.bxzex.vireo
  PLUGIN_MANUFACTURER_CODE Bxzx
  PLUGIN_CODE Vir8
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  VST3_CATEGORIES Instrument Synth
  FORMATS ${VIREO_FORMATS}
  PRODUCT_NAME "Vireo")

juce_add_binary_data(VireoData SOURCES
  Resources/BarlowCondensed-SemiBold.ttf
  Resources/BarlowCondensed-Bold.ttf
  Resources/ShareTechMono-Regular.ttf)

target_sources(Vireo PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(Vireo PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)
target_link_libraries(Vireo
  PRIVATE VireoData juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# offline render check, not shipped: cmake --build build --target VireoRender
juce_add_console_app(VireoRender PRODUCT_NAME "VireoRender")
target_sources(VireoRender PRIVATE tools/Render.cpp)
target_compile_definitions(VireoRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(VireoRender PRIVATE Vireo juce::juce_audio_formats)
set_target_properties(VireoRender PROPERTIES EXCLUDE_FROM_ALL TRUE)
