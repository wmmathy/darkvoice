# DarkVoice Studio

An AGPLv3, local-first vocal production project using C++20, JUCE and CMake.
This is an early development build. It provides input/output gain, high-pass,
warmth saturation, compression, low/high shelves and a parametric mid EQ,
feedback delay, damped reverb, dry/wet, bypass and peak meters. The grouped editor
has 30 real DSP controls and versioned host automation/state restoration.
A stereo-linked downward expander provides threshold, ratio and release controls;
ratio 1 disables attenuation. It follows compression and precedes EQ/delay/reverb.
Older project states and presets load with neutral gate settings.

The local preset bank offers six working factory sounds plus user save-as, rename,
favorites, search and JSON import/export. Factories are read-only; imports/save-as
create new UUID files and export refuses existing destinations. Presets preserve
bypass; future pitch/harmony settings remain separate. On Linux XDG_CONFIG_HOME is
honored; otherwise the platform application-data directory stores user presets.

The product targets are **DarkVoice Vocal** and **DarkVoice Master** (VST3; AU on
macOS), and **DarkVoice Studio** (standalone, currently hosting the starter chain).
Master currently exposes that same starter chain: LUFS, true-peak limiting and
complete-song Auto Master are not implemented. Track recording/import/editing, track-aware pitch correction, harmony, Voice DNA/AI,
Auto Mix and audio export remain planned. A background analyzer displays the measured dry input note, frequency and
confidence. It is not live pitch correction, target-note coaching or track following.

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

The split-band de-esser adds frequency, threshold and maximum high-band reduction
controls; zero reduction disables it. Existing v1/v2/v3 sessions and presets migrate
with neutral settings. Native CI is paused because GitHub Actions credits are
unavailable; new validation is local until further notice.

Chorus adds rate, depth and mix controls; stereo width ranges from mono fold-down
(0) through neutral (1) to doubled side (2). Width affects the wet stereo path and
is a no-op in mono. Wider settings can increase peaks. New v5 files migrate older
states/presets with chorus off and neutral width.

Flanger development adds a seventh processing tab with rate, depth, signed
feedback and mix. Its0.5–8.5ms modulated delay creates the audible comb effect;
this musical delay does not add host compensation latency. Mix0 disables the
wet contribution while delay/feedback history stays warm. State/preset v6 migrates
v1–v5 with flanger off. This branch is stacked on chorus/width PR #17 and does not
include or depend on experimental pitch/formant shifting.

See [pending development](docs/PENDING_DEVELOPMENT.md) for the current prioritized
backlog, existing functionality, review queue and quality blockers.
