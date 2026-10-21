# DarkVoice Studio

An AGPLv3, local-first vocal production project using C++20, JUCE and CMake.
This is an early development build. The current slice provides input/output gain,
high-pass filtering, warmth saturation, envelope compression, dry/wet, bypass,
peak meters and host parameter/state restoration. Controls operate on real DSP.

The product targets are **DarkVoice Vocal** and **DarkVoice Master** (VST3; AU on
macOS), and **DarkVoice Studio** (standalone, currently hosting the starter chain).
Master currently exposes that same starter chain: LUFS, true-peak limiting and
complete-song Auto Master are not implemented. Recording/import/editing,
track-aware pitch, harmony, Voice DNA/AI, Auto Mix and export remain planned.

## Build

Requirements: C++20 compiler, CMake 3.24+, JUCE's native audio/GUI development
libraries. JUCE 8.0.6 is fetched at an immutable commit. See
[dependency licenses](docs/DEPENDENCIES.md) and [cloud setup](docs/CLOUD_SETUP.md).

Core and DSP tests without GUI dependencies:

```sh
cmake -S . -B build/core -DCMAKE_BUILD_TYPE=Release
cmake --build build/core --parallel 2
ctest --test-dir build/core --output-on-failure --no-tests=error
```

Product builds (add `-DDARKVOICE_JUCE_SOURCE_DIR=/path/to/JUCE` for an existing
verified JUCE 8.0.6 checkout):

```sh
cmake -S . -B build/products -DCMAKE_BUILD_TYPE=Debug -DDARKVOICE_BUILD_PRODUCTS=ON
cmake --build build/products --parallel 2
ctest --test-dir build/products --output-on-failure --no-tests=error
```

Run the Studio executable under `build/products/DarkVoiceStudio_artefacts/Debug/Standalone/`
(or the `.app` bundle there on macOS). Use the standalone audio settings to select
input/output devices; use headphones when monitoring a microphone. Install the
VST3/AU artifacts in the appropriate user plugin directories to test a DAW.
Builds are unsigned development artifacts. Actual host compatibility and listening
quality require explicit validation; see [test evidence](docs/TEST_RESULTS.md).

## Development

Read [AGENTS.md](AGENTS.md), the authoritative
[product specification](docs/PRODUCT_SPECIFICATION.md), [architecture](docs/ARCHITECTURE.md),
[v1 contracts](docs/COMPONENT_CONTRACTS.md), [roadmap](docs/ROADMAP.md) and
[current status](docs/DEVELOPMENT_STATUS.md). Future dependencies must be open source.
CI builds/tests and manual artifact packaging do not authorize public releases.
