cmake_minimum_required(VERSION 3.24)
project(DarkVoiceStudio VERSION 0.1.0 LANGUAGES CXX)

option(DARKVOICE_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)

add_library(darkvoice_core INTERFACE)
add_library(DarkVoice::Core ALIAS darkvoice_core)
target_compile_features(darkvoice_core INTERFACE cxx_std_20)
target_include_directories(darkvoice_core INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")

include(CTest)
if(BUILD_TESTING)
    add_executable(darkvoice_core_tests tests/core_tests.cpp)
    target_link_libraries(darkvoice_core_tests PRIVATE DarkVoice::Core)
    set_target_properties(darkvoice_core_tests PROPERTIES CXX_EXTENSIONS OFF)
    if(MSVC)
        target_compile_options(darkvoice_core_tests PRIVATE /W4 /WX)
    else()
        target_compile_options(darkvoice_core_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    if(DARKVOICE_ENABLE_SANITIZERS)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT MSVC)
            target_compile_options(darkvoice_core_tests PRIVATE
                -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(darkvoice_core_tests PRIVATE
                -fsanitize=address,undefined -fno-omit-frame-pointer)
        else()
            message(FATAL_ERROR "Sanitizers require a GCC or Clang compatible toolchain")
        endif()
    endif()
    foreach(suite IN ITEMS lifecycle gain buffers realtime render)
        add_test(NAME core.${suite} COMMAND darkvoice_core_tests ${suite})
        set_tests_properties(core.${suite} PROPERTIES TIMEOUT 30)
    endforeach()
elseif(DARKVOICE_ENABLE_SANITIZERS)
    message(FATAL_ERROR "DARKVOICE_ENABLE_SANITIZERS requires BUILD_TESTING=ON")
endif()

# JUCE, standalone, AU and VST3 targets are not implemented in this foundation.
