# Architecture

Requirements baseline: [PRODUCT_SPECIFICATION.md](PRODUCT_SPECIFICATION.md), supplied
2026-10-21. This document records reconciliation, not a completed architecture.

## Required product boundaries

C++20/JUCE/CMake share testable DSP between DarkVoice Vocal (AUv2/VST3), DarkVoice
Master (AUv2/VST3) and DarkVoice Studio Standalone. Apple Silicon macOS and
GarageBand AU are primary; Intel and other named hosts require actual validation.
A dependency-free DSP core remains compatible with the specification, but is not
a substitute for the required JUCE products.

Required modules: AudioEngine, VoiceSculptor, FormantProcessor, PitchDetector,
PitchCorrector, ReferenceAnalyzer, HarmonyEngine, Dynamics/Effects, VoiceDNA,
VocalCoach, AutoVocalProducer, AutoMix, Mastering, LoudnessAnalyzer, Exporter,
Presets, Parameters, PluginProcessor/Editor and StandaloneHost.

Host adapters own lifecycle, automation, state and latency reporting. Workers own
file I/O, reference analysis and local inference. Audio callbacks use preallocated,
bounded processing with no locks, allocation, I/O or GUI access. Coach uses dry
input. Reference timelines require transport alignment and manual/standalone
fallbacks. Mastering consumes the complete stereo song. Profiles remain local and
user-controlled. These are requirements; implementations are pending.

## Dependency order and review gates

1. Dependency/license review and versioned host/core/parameter/state contracts.
2. JUCE/CMake product skeletons, supported-rate/buffer tests and macOS CI.
3. Effects/metering/presets and parameter smoothing; pitch/reference contracts
   enable correction and harmony packages.
4. Voice measurements/profile persistence enable coaching/Auto Vocal Producer;
   standalone recording/import/editing enables complete-song Auto Mix.
5. Loudness/mastering/export/QC, then UI completion, host/performance validation
   and authorized release packaging.

Before independent features begin, the lead must establish concrete shared APIs
for parameters, state, quality modes, latency, reference timeline, worker messages,
profiles and export. A module list is not an established interface contract.

## Preserved provisional work

The v0 mono/stereo gain header, tests and CMake/CI branches are preserved separately.
Gain is a primitive only. Its block-step changes, limited contract and zero-latency
report do not satisfy full automation, quality modes or product latency requirements.
No plugin, standalone, pitch, harmony, AI, mix, master or export feature is complete.

JUCE/SDK/codec/model license evaluation and macOS SDK/host availability remain
unverified. No background scheduler or persistent autonomous service is configured.
