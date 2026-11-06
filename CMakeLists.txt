cmake_minimum_required(VERSION 3.22)
project(Spectro VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.6
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Spectro
  COMPANY_NAME "Aron H"
  COMPANY_WEBSITE "https://www.instagram.com/aronh.official"
  BUNDLE_ID com.aronh.spectro
  PLUGIN_MANUFACTURER_CODE ArnH
  PLUGIN_CODE Spct
  FORMATS VST3 AU Standalone
  PRODUCT_NAME "Spectro"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  AU_MAIN_TYPE kAudioUnitType_Effect
  VST3_CATEGORIES Fx Analyzer)

target_sources(Spectro PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_compile_definitions(Spectro PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Spectro PRIVATE
  juce::juce_audio_utils
  juce::juce_dsp
  PUBLIC
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags)
