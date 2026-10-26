# Experimental phase/amplitude separation — 2026-10-26

This finite developer experiment remains outside main and products. The original
149 gates and broader diagnostics are preserved. Pitch/formant qualification is
still blocked. Reference evidence: PITCH_COUPLING_AUDIT.md in the separate
qualification integration branch; source base fa4af2971779737acdd72e7ca9f37070a926aabc,
Stretch a670068d9aeb64913331d5cc29337b19a457a7df, Linear
de55e6a50ffcf6f8f43f649692d94691c7025151 (MIT). No upstream checkout edits.

Keep all original Band::inputEnergy, Prediction::energy and persistent Band::output
on an uncompensated reference trajectory. This includes temporal denominators,
all vertical neighbor reads, maximum-energy-channel decisions, cross-channel
locking and makeOutput fallback. Add separately allocated synthesis energy storage;
copy original analysis energy into it every processing frame, including repeated
spectra and disabled-formant frames. Formant step2 changes only the separate
energy. Map it through the same outputMap/interpolation/freqGrad, then apply its
nonnegative square-root energy ratio ONLY while copying reference bins into the
STFT synthesis spectrum. Never write compensated output back into reference state.
Unity ratios must preserve exact reference behavior; zero reference energy emits
zero rather than choosing a gain-dependent phase. Audit configure/reset/seek/
silence/repeated-spectrum lifecycle, including channelPredictions initialization.
No public processor API, parameter, preset or latency contract change.

Implementation must be opt-in and default OFF behind a developer CMake option.
Preserve existing generation and fingerprint checks; add a bounded private patch
step with exact anchors/checksums and retained notices. Generated upstream copies
stay in build directories. Instrumentation behind a developer macro must compare
all reference-state fields and mappings frame-by-frame without unbounded callback
allocation; same executable/platform and same schedule twins must be bit-identical
when only envelope gains differ. Include unity/measured/analytic/fixed-nonuniform/
time-varying output gains, stereo unequal channels, notches, silence/resume,
reset/reprepare, split on/off matched schedules, variable calls/repeated spectra.
All emitted energies/samples must remain finite. Specify numerical tolerance for
emitted energy scaling separately from the exact reference-state invariant.

First establish the invariant, then rerun unchanged 42-render diagnostic and the
three original combined-periodicity cases, retaining >.8 correlation. Report all
failures; do not infer qualification from the invariant. Only after evidence
improves run the unchanged149 gates, envelopes, sine/global-frequency, automation,
length/flush/allocation and broader configurations. Preserve independent ideal35,
unchanged70 and even-only controls; report half/full/double-period correlations,
harmonic-grid/total and odd-grid/total. No universal odd-energy threshold.

Writer owns cmake/Signalsmith.cmake, one new guarded private-patch CMake file,
new tools/qualification/phase_decoupling_probe.cpp and optional analyser, and this
audit document for actual commands/results. Existing product source/tests stay
unchanged. Root owns any further integration decision. No main PR or backend
merge until qualification genuinely passes; manual listening remains deferred.
