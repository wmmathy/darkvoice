# Exact audited revisions. Upstream CMake is intentionally not executed: it
# defaults to platform backends and fetches Linear through a mutable tag.
include(FetchContent)
FetchContent_Declare(darkvoice_stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG a670068d9aeb64913331d5cc29337b19a457a7df
    GIT_SUBMODULES ""
    SOURCE_SUBDIR darkvoice-no-upstream-cmake)
FetchContent_Declare(darkvoice_linear
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/linear.git
    GIT_TAG de55e6a50ffcf6f8f43f649692d94691c7025151
    GIT_SUBMODULES ""
    SOURCE_SUBDIR darkvoice-no-upstream-cmake)
FetchContent_MakeAvailable(darkvoice_stretch darkvoice_linear)
function(darkvoice_verify_file path expected)
    file(SHA256 "${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Audited Signalsmith source fingerprint mismatch: ${path}")
    endif()
endfunction()
darkvoice_verify_file("${darkvoice_stretch_SOURCE_DIR}/signalsmith-stretch.h" 1188667959ac19dd40c0a6abbce694e44705615ec4f0e8db8af0f1cfb4c5dea7)
darkvoice_verify_file("${darkvoice_stretch_SOURCE_DIR}/LICENSE.txt" ee2ef82481ffb445ecdd4b3a4c1f82c0cddb2da8fe39d8e8dc384fffb3e7f06f)
darkvoice_verify_file("${darkvoice_linear_SOURCE_DIR}/stft.h" 7422ff5ddc2a017dc0587dcce99ce5ba30a2414bc2996081f6b00d56905d9b8c)
darkvoice_verify_file("${darkvoice_linear_SOURCE_DIR}/fft.h" fb55f69ac14cdbac66b9c76053069ff63bc36d18af20d8c028b9acc0aad6516a)
darkvoice_verify_file("${darkvoice_linear_SOURCE_DIR}/LICENSE.txt" 072b5e9eb5b22880bdf6256324654cf3ce53dba828edb911921121e1be09d9c3)
set(darkvoice_stretch_private "${CMAKE_CURRENT_BINARY_DIR}/darkvoice-stretch-private")
file(MAKE_DIRECTORY "${darkvoice_stretch_private}/signalsmith-linear")
file(READ "${darkvoice_stretch_SOURCE_DIR}/signalsmith-stretch.h" darkvoice_stretch_header)
string(REGEX MATCHALL "totalEnergy < noiseFloor" darkvoice_quiet_matches "${darkvoice_stretch_header}")
list(LENGTH darkvoice_quiet_matches darkvoice_quiet_count)
if(NOT darkvoice_quiet_count EQUAL 1)
    message(FATAL_ERROR "Expected exactly one audited near-silence shortcut")
endif()
string(REPLACE "if (totalEnergy < noiseFloor) {" "if (false && totalEnergy < noiseFloor) {"
    darkvoice_stretch_header "${darkvoice_stretch_header}")
# Replace the audited envelope stage, preserving the declared fundamental units.
string(REPLACE "formantMetric.resize(bands + 2);"
    "formantMetric.resize(bands + 2);\n\t\tformantSource.resize(bands);"
    darkvoice_stretch_header "${darkvoice_stretch_header}")
string(REPLACE "std::vector<Sample> formantMetric;"
    "std::vector<Sample> formantMetric;\n\tstd::vector<Sample> formantSource;"
    darkvoice_stretch_header "${darkvoice_stretch_header}")
string(FIND "${darkvoice_stretch_header}" "\t\t\tSample decay = 1 - 1/(freqEstimate*Sample(0.5) + 1);" darkvoice_envelope_start)
string(FIND "${darkvoice_stretch_header}" "\n\t\t} else {\n\t\t\tauto getFormant" darkvoice_envelope_end)
if(darkvoice_envelope_start LESS 0 OR darkvoice_envelope_end LESS darkvoice_envelope_start)
    message(FATAL_ERROR "Audited Signalsmith envelope stage not found")
endif()
math(EXPR darkvoice_envelope_length "${darkvoice_envelope_end} - ${darkvoice_envelope_start}")
string(SUBSTRING "${darkvoice_stretch_header}" ${darkvoice_envelope_start} ${darkvoice_envelope_length} darkvoice_envelope_original)
file(READ "${CMAKE_CURRENT_LIST_DIR}/signalsmith-formant-envelope.inc" darkvoice_envelope_replacement)
string(STRIP "${darkvoice_envelope_replacement}" darkvoice_envelope_replacement)
# STRIP removes initial indentation only; restore it for a stable private header.
string(REPLACE "${darkvoice_envelope_original}" "\t\t\t${darkvoice_envelope_replacement}"
    darkvoice_stretch_header "${darkvoice_stretch_header}")
file(WRITE "${darkvoice_stretch_private}/signalsmith-stretch.h" "${darkvoice_stretch_header}")
darkvoice_verify_file("${darkvoice_stretch_private}/signalsmith-stretch.h" 7003842b8347ba10710f280c33dba494cd3466a54b240cef76cfbbe32a9a93b8)
configure_file("${darkvoice_linear_SOURCE_DIR}/stft.h" "${darkvoice_stretch_private}/signalsmith-linear/stft.h" COPYONLY)
configure_file("${darkvoice_linear_SOURCE_DIR}/fft.h" "${darkvoice_stretch_private}/signalsmith-linear/fft.h" COPYONLY)
