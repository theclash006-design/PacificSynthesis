cmake_minimum_required(VERSION 3.22)

project(PacificSynthesis VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# プラットフォーム判定 (iOS / macOS / Windows)
set(IS_IOS FALSE)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(IS_IOS TRUE)
endif()

# Deployment target (Apple 共通: macOS は 10.15、iOS は 15.0)
if(IS_IOS)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "iOS deployment target")
else()
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "macOS deployment target")
endif()

# JUCEへのパス。`../JUCE` にcloneしてあればそのまま動きます。
# 別の場所にある場合は `cmake -B build -DJUCE_PATH=/path/to/JUCE` で指定してください。
if(NOT DEFINED JUCE_PATH)
    set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" CACHE PATH "Path to JUCE")
endif()

if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE not found at ${JUCE_PATH}.\n"
        "Run: git clone --depth 1 https://github.com/juce-framework/JUCE ${JUCE_PATH}")
endif()

add_subdirectory(${JUCE_PATH} JUCE EXCLUDE_FROM_ALL)

# プラットフォーム別の formats
# iOS は Standalone のみ (VST3/AU は iOS で動かない、AUv3 は別途要設定)
if(IS_IOS)
    set(PLUGIN_FORMATS Standalone)
else()
    set(PLUGIN_FORMATS VST3 AU Standalone)
endif()

juce_add_plugin(PacificSynthesis
    COMPANY_NAME              "Coast Machine"
    BUNDLE_ID                 com.coastmachine.pacificsynthesis
    PLUGIN_MANUFACTURER_CODE  Csmc
    PLUGIN_CODE               Pcfs
    FORMATS                   ${PLUGIN_FORMATS}
    PRODUCT_NAME              "Pacific Synthesis"
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD   TRUE
    # ─ iOS 専用設定 (横向き固定、background audio/MIDI、フルスクリーン) ─
    IPAD_SCREEN_ORIENTATIONS    UIInterfaceOrientationLandscapeLeft
                                UIInterfaceOrientationLandscapeRight
    IPHONE_SCREEN_ORIENTATIONS  UIInterfaceOrientationLandscapeLeft
                                UIInterfaceOrientationLandscapeRight
    REQUIRES_FULL_SCREEN        TRUE
    BACKGROUND_AUDIO_ENABLED    TRUE
    BACKGROUND_MIDI_ENABLED     TRUE
    # iOS Standalone wrapper は AVAudioSessionCategoryPlayAndRecord を使うため
    # 録音しない synth でもマイク権限宣言が必須 (無いと AudioUnit 初期化で SIGABRT)
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT    "This synthesizer does not record audio. iOS requires this declaration for the audio engine to start."
    # macOS/iOS Standalone で Bluetooth MIDI を有効化する時に必要な Info.plist キー
    # (Options -> Audio/MIDI Settings で Bluetooth MIDI を選ぶと OS が要求。
    #  この宣言が無いと TCC が privacy violation で SIGABRT を発生させる)
    BLUETOOTH_PERMISSION_ENABLED  TRUE
    BLUETOOTH_PERMISSION_TEXT     "Pacific Synthesis uses Bluetooth to connect to wireless MIDI controllers."
)

target_sources(PacificSynthesis PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_include_directories(PacificSynthesis PRIVATE Source)

target_compile_definitions(PacificSynthesis PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

if(MSVC)
    target_compile_options(PacificSynthesis PRIVATE /utf-8)
endif()

target_link_libraries(PacificSynthesis PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_audio_processors
    juce::juce_audio_plugin_client
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
