cmake_minimum_required(VERSION 3.24)
project(DarkVoiceStudio VERSION 0.1.0 LANGUAGES C CXX)

option(DARKVOICE_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
option(DARKVOICE_BUILD_PRODUCTS "Build JUCE Vocal, Master and Studio products" OFF)
set(DARKVOICE_JUCE_SOURCE_DIR "" CACHE PATH "Existing JUCE 8.0.6 checkout (otherwise fetch pinned source)")

add_library(darkvoice_core INTERFACE)
add_library(DarkVoice::Core ALIAS darkvoice_core)
target_compile_features(darkvoice_core INTERFACE cxx_std_20)
target_include_directories(darkvoice_core INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")

add_library(darkvoice_engine STATIC src/AudioEngine.cpp src/ToneEffects.cpp src/PitchDetector.cpp src/NoiseGate.cpp src/DeEsser.cpp src/ModulationEffects.cpp)
add_library(DarkVoice::Engine ALIAS darkvoice_engine)
target_link_libraries(darkvoice_engine PUBLIC DarkVoice::Core)
set_target_properties(darkvoice_engine PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)

function(darkvoice_strict_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()
darkvoice_strict_warnings(darkvoice_engine)

include(CTest)
if(DARKVOICE_ENABLE_SANITIZERS)
    if(NOT BUILD_TESTING)
        message(FATAL_ERROR "DARKVOICE_ENABLE_SANITIZERS requires BUILD_TESTING=ON")
    endif()
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" OR MSVC)
        message(FATAL_ERROR "Sanitizers require a GCC or Clang compatible toolchain")
    endif()
    # Propagate instrumentation and runtime linkage to every engine consumer.
    target_compile_options(darkvoice_engine PUBLIC
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(darkvoice_engine PUBLIC
        -fsanitize=address,undefined -fno-omit-frame-pointer)
endif()

if(BUILD_TESTING)
    add_executable(darkvoice_core_tests tests/core_tests.cpp)
    target_link_libraries(darkvoice_core_tests PRIVATE DarkVoice::Engine)
    set_target_properties(darkvoice_core_tests PROPERTIES CXX_EXTENSIONS OFF)
    darkvoice_strict_warnings(darkvoice_core_tests)
    foreach(suite IN ITEMS lifecycle gain buffers realtime render)
        add_test(NAME core.${suite} COMMAND darkvoice_core_tests ${suite})
        set_tests_properties(core.${suite} PROPERTIES TIMEOUT 30)
    endforeach()

    add_executable(darkvoice_engine_tests tests/engine_tests.cpp)
    target_link_libraries(darkvoice_engine_tests PRIVATE DarkVoice::Engine)
    set_target_properties(darkvoice_engine_tests PROPERTIES CXX_EXTENSIONS OFF)
    darkvoice_strict_warnings(darkvoice_engine_tests)
    add_test(NAME engine.regression COMMAND darkvoice_engine_tests)
    set_tests_properties(engine.regression PROPERTIES TIMEOUT 60)

    add_executable(darkvoice_effects_tests tests/effects_tests.cpp)
    target_link_libraries(darkvoice_effects_tests PRIVATE DarkVoice::Engine)
    set_target_properties(darkvoice_effects_tests PROPERTIES CXX_EXTENSIONS OFF)
    darkvoice_strict_warnings(darkvoice_effects_tests)
    add_test(NAME effects.regression COMMAND darkvoice_effects_tests)
    set_tests_properties(effects.regression PROPERTIES TIMEOUT 60)

    add_executable(darkvoice_gate_tests tests/gate_tests.cpp)
    target_link_libraries(darkvoice_gate_tests PRIVATE DarkVoice::Engine)
    set_target_properties(darkvoice_gate_tests PROPERTIES CXX_EXTENSIONS OFF)
    darkvoice_strict_warnings(darkvoice_gate_tests)
    add_test(NAME dynamics.gate COMMAND darkvoice_gate_tests)
    set_tests_properties(dynamics.gate PROPERTIES TIMEOUT 60)

    add_executable(darkvoice_deesser_tests tests/deesser_tests.cpp)
    target_link_libraries(darkvoice_deesser_tests PRIVATE DarkVoice::Engine)
    set_target_properties(darkvoice_deesser_tests PROPERTIES CXX_EXTENSIONS OFF)
    darkvoice_strict_warnings(darkvoice_deesser_tests)
    add_test(NAME dynamics.deesser COMMAND darkvoice_deesser_tests)
    set_tests_properties(dynamics.deesser PROPERTIES TIMEOUT 60)

    add_executable(darkvoice_modulation_tests tests/modulation_tests.cpp)
    target_link_libraries(darkvoice_modulation_tests PRIVATE DarkVoice::Engine)
    set_target_properties(darkvoice_modulation_tests PROPERTIES CXX_EXTENSIONS OFF)
    darkvoice_strict_warnings(darkvoice_modulation_tests)
    add_test(NAME effects.modulation COMMAND darkvoice_modulation_tests)
    set_tests_properties(effects.modulation PROPERTIES TIMEOUT 60)

    add_executable(darkvoice_pitch_tests tests/pitch_tests.cpp)
    target_link_libraries(darkvoice_pitch_tests PRIVATE DarkVoice::Engine)
    set_target_properties(darkvoice_pitch_tests PROPERTIES CXX_EXTENSIONS OFF)
    darkvoice_strict_warnings(darkvoice_pitch_tests)
    add_test(NAME pitch.analysis COMMAND darkvoice_pitch_tests)
    set_tests_properties(pitch.analysis PROPERTIES TIMEOUT 60)
endif()

if(DARKVOICE_BUILD_PRODUCTS)
    # JUCE is AGPLv3/commercial dual-licensed; this project uses its AGPL route.
    # VST3 sources are the open-source SDK included in this exact JUCE revision.
    set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
    if(DARKVOICE_JUCE_SOURCE_DIR)
        get_filename_component(darkvoice_juce_dir "${DARKVOICE_JUCE_SOURCE_DIR}" ABSOLUTE)
        if(NOT EXISTS "${darkvoice_juce_dir}/CMakeLists.txt")
            message(FATAL_ERROR "DARKVOICE_JUCE_SOURCE_DIR must point to a JUCE source checkout")
        endif()
        add_subdirectory("${darkvoice_juce_dir}" "${CMAKE_BINARY_DIR}/juce" EXCLUDE_FROM_ALL)
    else()
        include(FetchContent)
        FetchContent_Declare(juce
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 51a8a6d7aeae7326956d747737ccf1575e61e209
            GIT_SHALLOW FALSE)
        FetchContent_MakeAvailable(juce)
        set(darkvoice_juce_dir "${juce_SOURCE_DIR}")
    endif()
    get_directory_property(darkvoice_juce_version DIRECTORY "${darkvoice_juce_dir}" DEFINITION JUCE_VERSION)
    if(NOT darkvoice_juce_version VERSION_EQUAL "8.0.6")
        message(FATAL_ERROR "DarkVoice requires JUCE 8.0.6; got ${darkvoice_juce_version}")
    endif()

    set(darkvoice_plugin_formats VST3)
    if(APPLE)
        list(APPEND darkvoice_plugin_formats AU)
    endif()

    function(darkvoice_product target product code mastering)
        juce_add_plugin(${target}
            COMPANY_NAME "DarkVoice"
            BUNDLE_ID "studio.darkvoice.${target}"
            PLUGIN_MANUFACTURER_CODE Dkvc
            PLUGIN_CODE ${code}
            PRODUCT_NAME "${product}"
            DESCRIPTION "${product} development build"
            FORMATS ${ARGN}
            IS_SYNTH FALSE
            NEEDS_MIDI_INPUT FALSE
            NEEDS_MIDI_OUTPUT FALSE
            IS_MIDI_EFFECT FALSE
            EDITOR_WANTS_KEYBOARD_FOCUS FALSE
            COPY_PLUGIN_AFTER_BUILD FALSE
            VST3_CATEGORIES Fx
            AU_MAIN_TYPE kAudioUnitType_Effect)
        target_sources(${target} PRIVATE src/PluginProcessor.cpp src/PluginEditor.cpp src/PresetBank.cpp src/DryPitchAnalyzer.cpp)
        target_include_directories(${target} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
        target_link_libraries(${target} PRIVATE DarkVoice::Engine juce::juce_audio_utils
            juce::juce_recommended_config_flags)
        target_compile_features(${target} PRIVATE cxx_std_20)
        target_compile_definitions(${target} PUBLIC
            DARKVOICE_MASTER=${mastering}
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)
        juce_generate_juce_header(${target})
    endfunction()

    darkvoice_product(DarkVoiceVocal "DarkVoice Vocal" Dvvc 0 ${darkvoice_plugin_formats})
    darkvoice_product(DarkVoiceMaster "DarkVoice Master" Dvms 1 ${darkvoice_plugin_formats})
    darkvoice_product(DarkVoiceStudio "DarkVoice Studio" Dvst 0 Standalone)

    if(BUILD_TESTING)
        add_executable(darkvoice_plugin_tests tests/plugin_tests.cpp)
        # Reuse Vocal's shared-code archive instead of compiling JUCE modules again.
        target_link_libraries(darkvoice_plugin_tests PRIVATE DarkVoiceVocal)
        target_include_directories(darkvoice_plugin_tests PRIVATE
            "${darkvoice_juce_dir}/modules" "${CMAKE_CURRENT_SOURCE_DIR}/include")
        target_compile_features(darkvoice_plugin_tests PRIVATE cxx_std_20)
        target_compile_definitions(darkvoice_plugin_tests PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1)
        add_executable(darkvoice_preset_tests tests/preset_tests.cpp)
        target_link_libraries(darkvoice_preset_tests PRIVATE DarkVoiceVocal)
        target_include_directories(darkvoice_preset_tests PRIVATE
            "${darkvoice_juce_dir}/modules" "${CMAKE_CURRENT_SOURCE_DIR}/include")
        target_compile_features(darkvoice_preset_tests PRIVATE cxx_std_20)
        target_compile_definitions(darkvoice_preset_tests PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1)
        add_executable(darkvoice_analysis_worker_tests tests/analysis_worker_tests.cpp)
        target_link_libraries(darkvoice_analysis_worker_tests PRIVATE DarkVoiceVocal)
        target_include_directories(darkvoice_analysis_worker_tests PRIVATE
            "${darkvoice_juce_dir}/modules" "${CMAKE_CURRENT_SOURCE_DIR}/include")
        target_compile_features(darkvoice_analysis_worker_tests PRIVATE cxx_std_20)
        target_compile_definitions(darkvoice_analysis_worker_tests PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1)
        add_test(NAME analysis.worker COMMAND darkvoice_analysis_worker_tests)
        set_tests_properties(analysis.worker PROPERTIES TIMEOUT 30
            FAIL_REGULAR_EXPRESSION "JUCE Assertion failure;Leaked objects detected")
        add_test(NAME presets.persistence COMMAND darkvoice_preset_tests)
        set_tests_properties(presets.persistence PROPERTIES TIMEOUT 60
            FAIL_REGULAR_EXPRESSION "JUCE Assertion failure;Leaked objects detected")
        # Explicit display-backed smoke target, run under Xvfb or a desktop.
        # Not registered in headless CTest; it requires a window-system session.
        add_executable(darkvoice_gui_smoke tests/gui_smoke.cpp)
        target_link_libraries(darkvoice_gui_smoke PRIVATE DarkVoiceVocal)
        target_include_directories(darkvoice_gui_smoke PRIVATE
            "${darkvoice_juce_dir}/modules" "${CMAKE_CURRENT_SOURCE_DIR}/include")
        target_compile_features(darkvoice_gui_smoke PRIVATE cxx_std_20)
        target_compile_definitions(darkvoice_gui_smoke PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1)
        add_test(NAME plugin.state_and_audio COMMAND darkvoice_plugin_tests)
        set_tests_properties(plugin.state_and_audio PROPERTIES TIMEOUT 60
            FAIL_REGULAR_EXPRESSION "JUCE Assertion failure;Leaked objects detected")
    endif()
endif()
