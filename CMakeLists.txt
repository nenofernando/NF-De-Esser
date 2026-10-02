cmake_minimum_required(VERSION 3.22)
project(NFDeEsser VERSION 1.0.2 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS Universal Binary")
option(NFDeEsser_FETCH_JUCE "Fetch JUCE when not installed" ON)
option(NFDeEsser_ENABLE_AAX "Build AAX (licensed SDK required)" OFF)
# The AAX SDK is expected in ~/Documents/AAX_SDK (owner's Mac). Override with -DNFDeEsser_AAX_SDK_PATH=...
# (the Windows script does this from the AAX_SDK_PATH environment variable).
if(WIN32)
 set(_nfdeesser_home "$ENV{USERPROFILE}")
else()
 set(_nfdeesser_home "$ENV{HOME}")
endif()
set(NFDeEsser_AAX_SDK_PATH "${_nfdeesser_home}/Documents/AAX_SDK" CACHE PATH "Licensed AAX SDK path")

find_package(JUCE CONFIG QUIET)
if(NOT JUCE_FOUND)
 if(NOT NFDeEsser_FETCH_JUCE)
  message(FATAL_ERROR "JUCE not found. Set JUCE_DIR or enable NFDeEsser_FETCH_JUCE.")
 endif()
 include(FetchContent)
 FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
 FetchContent_MakeAvailable(JUCE)
endif()

set(NFDeEsser_FORMATS VST3)
if(APPLE)
 list(APPEND NFDeEsser_FORMATS AU)
endif()
if(NFDeEsser_ENABLE_AAX)
 if(NOT EXISTS "${NFDeEsser_AAX_SDK_PATH}")
  message(FATAL_ERROR "NFDeEsser_ENABLE_AAX requires the AAX SDK at NFDeEsser_AAX_SDK_PATH (${NFDeEsser_AAX_SDK_PATH}).")
 endif()
 juce_set_aax_sdk_path("${NFDeEsser_AAX_SDK_PATH}")
 list(APPEND NFDeEsser_FORMATS AAX)
endif()

juce_add_plugin(NFDeEsser
 COMPANY_NAME "NF Audio Tools"
 COMPANY_COPYRIGHT "NF Audio Tools by Nenno Fernando. (C) 2026 NF Audio Tools. All rights reserved."
 BUNDLE_ID "com.nfaudiotools.nfdeesser"
 AAX_IDENTIFIER "com.nfaudiotools.nfdeesser"
 PLUGIN_MANUFACTURER_CODE Nfat
 PLUGIN_CODE Nfde
 FORMATS ${NFDeEsser_FORMATS}
 PRODUCT_NAME "NF De-Esser"
 PLUGIN_NAME "NF De-Esser"
 DESCRIPTION "Simple split-band de-esser"
 IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE
 EDITOR_WANTS_KEYBOARD_FOCUS FALSE COPY_PLUGIN_AFTER_BUILD FALSE
 VST3_CATEGORIES Fx Dynamics Stereo
 AU_MAIN_TYPE kAudioUnitType_Effect
 AAX_CATEGORY AAX_ePlugInCategory_Dynamics)
juce_generate_juce_header(NFDeEsser)

# Artwork embedded as binary data (header NFDeEsserBinaryData.h).
file(GLOB NFDeEsser_READY_ASSETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Assets/PNG_READY_1200x400/*.png" "${CMAKE_CURRENT_SOURCE_DIR}/Assets/PNG_READY_800x800/*.png")
juce_add_binary_data(NFDeEsserAssets HEADER_NAME NFDeEsserBinaryData.h NAMESPACE NFDeEsserBinaryData SOURCES ${NFDeEsser_READY_ASSETS} "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Manuals/NF_DeEsser_Manual_Portugues.pdf" "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Manuals/NF_DeEsser_Manual_English.pdf")
target_sources(NFDeEsser PRIVATE
 Source/PluginProcessor.cpp Source/PluginProcessor.h Source/PluginEditor.cpp Source/PluginEditor.h
 Source/DSP/DeEsser.h
 Source/ManualManager.cpp Source/ManualManager.h
 Source/UI/ValueCapsule.cpp Source/UI/ValueCapsule.h
 Source/PresetManager.cpp Source/PresetManager.h
 Source/UI/NFDeEsserLookAndFeel.cpp Source/UI/NFDeEsserLookAndFeel.h)
target_compile_definitions(NFDeEsser PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)
target_link_libraries(NFDeEsser PRIVATE NFDeEsserAssets juce::juce_audio_utils juce::juce_dsp juce::juce_cryptography PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

add_executable(NFDeEsserDspTests Tests/DeEsserTests.cpp)
if(MSVC)
 target_compile_options(NFDeEsserDspTests PRIVATE /W4)
else()
 target_compile_options(NFDeEsserDspTests PRIVATE -Wall -Wextra -Wpedantic)
endif()
enable_testing()
add_test(NAME NFDeEsserDspTests COMMAND NFDeEsserDspTests)

# Licence regression tests (need the JUCE core/events/cryptography modules). Not part of "all": build them on request:
#   cmake --build <dir> --target NFDeEsserLicenseTests   (run it also with -fsanitize=thread to check the activated flag)
add_executable(NFDeEsserLicenseTests EXCLUDE_FROM_ALL Tests/LicenseTests.cpp)
target_compile_definitions(NFDeEsserLicenseTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(NFDeEsserLicenseTests PRIVATE juce::juce_core juce::juce_events juce::juce_cryptography juce::juce_recommended_config_flags)
add_test(NAME NFDeEsserLicenseTests COMMAND NFDeEsserLicenseTests)
