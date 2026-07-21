cmake_minimum_required(VERSION 3.24)
project(BrittLizer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── JUCE ──────────────────────────────────────────────────────────────────────
set(JUCE_TAG "8.0.12")

include(FetchContent)
set(FETCHCONTENT_BASE_DIR "${PROJECT_SOURCE_DIR}/Libs" CACHE PATH "FetchContent download cache" FORCE)

FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${JUCE_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE)

FetchContent_MakeAvailable(juce)

# ── Plugin Target ─────────────────────────────────────────────────────────────
juce_add_plugin(${PROJECT_NAME}
    PRODUCT_NAME             "Carmy BrittLizer"
    FORMATS                  AU VST3 Standalone
    COMPANY_NAME             "Carmy Code"
    BUNDLE_ID                "com.carmycode.brittlizer"
    COMPANY_WEBSITE          "https://ambrosiogomez.com"
    COMPANY_EMAIL            "brosio@example.com"
    PLUGIN_MANUFACTURER_CODE CaCo
    PLUGIN_CODE              BrLz
    AU_MAIN_TYPE             kAudioUnitType_Effect
    VST3_CATEGORIES          "Fx|Distortion|Dynamics"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    COPY_PLUGIN_AFTER_BUILD  TRUE)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

target_link_libraries(${PROJECT_NAME} PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_plugin_client
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# ── Generate JuceHeader.h (explicit in JUCE 8) ────────────────────────────────
juce_generate_juce_header(${PROJECT_NAME})

# ── Sources ───────────────────────────────────────────────────────────────────
add_subdirectory(Source)

# ── Universal Binary ──────────────────────────────────────────────────────────
set_target_properties(${PROJECT_NAME} PROPERTIES
    XCODE_ATTRIBUTE_ARCHS "arm64 x86_64"
    XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "12.0")
