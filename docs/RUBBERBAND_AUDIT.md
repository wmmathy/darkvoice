# Rubber Band alternative-backend audit — 2026-10-22

Candidate evaluation only. No new product dependency, wrapper, controls or preset
fields; no Rubber Band binary/source is vendored. PR #19's three failing combined
periodicity gates remain real blockers. This audit does not qualify a replacement.

## Source and open-source build route

Inspected Rubber Band v4.0.0, immutable commit
`1d95888bec3ae0a17c0c4af791810d5a63f6bc35` in breakfastquay/rubberband.
Headers and implementation grant GPL-2.0-or-later; electing GPLv3 supplies the
compatible route for the project's AGPL-3.0-only combined distribution. Preserve
copyright/COPYING and applicable notices if integrating. No commercial licence is
needed for the selected open-source route. This is not an integration decision.

The single translation unit includes built-in FFT and BQ resampler paths. Its
unconfigured Apple default selects vDSP; the probe explicitly supplies
ALREADY_CONFIGURED, USE_BUILTIN_FFT, USE_BQRESAMPLER, NO_THREADING,
NO_THREAD_CHECKS and NO_TIMING. This avoids Accelerate/IPP and external FFT/SRC
libraries. Bundled optional KissFFT/Speex paths are not selected. Track all required
source/notices at integration rather than assuming the API header is sufficient.

Inspected SHA256 fingerprints:

- COPYING: cedd4c1885d75991bf1077cc6e33359103cf04fbc996f20d30ab043d5fc5fcc5
- RubberBandLiveShifter.h: 59bbc6f89327beb984558cb0395b4317a3da2af4845937d0e0c8f559aee25fc1

## API/lifecycle findings

RubberBandLiveShifter exposes same-count fixed-block shift(), getBlockSize(),
getStartDelay(), pitch scale, explicit formant scale and preservation options.
Short/medium configurations both require512 frames in the executed48k mono probes.
Variable host buffers therefore need a preallocated accumulator/output FIFO; API
startup delay is not the complete host-adapter delay. Reported startup delay varies
with initial pitch, so a fixed-latency automation/reset/bypass contract and measured
alignment are required before host integration. No manual base-F0 setter was
identified; existing formantBaseHz semantics must not silently be ignored.

Header documentation says the processing path avoids allocation/locking/blocking;
this audit did not independently certify all dynamic allocation/C-runtime paths,
automation, reset, stereo, other sample rates or callback deadlines. Keep setters
and shift single-owner. Suppress/replace default logging in any future wrapper.
The general R3 Stretcher supports independent formant scale but variable available/
retrieve output; R2 does not support the explicit formant-scale API. LiveShifter is
the simpler streaming candidate, not a promised low-latency monitoring solution.

## Executed synthetic comparison

Linux GCC14/O2, pure built-in backends,48k mono, two-second100Hz-wide Gaussian
harmonic-envelope fixtures with a small harmonic floor, matching PR #19's cases.
Preservation uses OptionFormantPreserved and auto scale0 for formant0; explicit
shift uses exp2((formantSemitones-pitchSemitones)/12) to account for final pitch
resampling, consistent with the inspected automatic inverse-pitch behavior.
The committed developer probe records samples and measurements; it is not CTest
or a production component. It prints observations, not a quality-passed status.

| F0 / envelope | Pitch / formant | Short correlation | Medium correlation | Strongest spectral line | Target envelope |
| --- | --- | ---: | ---: | ---: | ---: |
| 100 / 600Hz | +12 / 0 | 0.999961 | 0.999954 | 600.000Hz | 600Hz |
| 70 / 600Hz | -12 / 0 | 0.994518 | 0.994348 | 455.001Hz | 600Hz |
| 180 / 1200Hz | +12 / +7 | 0.996470 | 0.996473 | 1800.000Hz | 1797.97Hz |

All six outputs were finite; observed steady-region peaks0.00964–0.01834. Target-period
correlation exceeds the existing0.8 gate in these cases. Independent NumPy FFT
(last48000 samples, Hann window,1048576 points with log-peak interpolation) places
the strongest line on the requested harmonic grid, within0.004cents of that grid.
This is not a general pitch-accuracy or naturalness qualification.

The70Hz/octave-down case fails the existing20% isolated-envelope target gate:
455.001/600-1 = -24.17%, in both profiles. Thus improved periodicity is insufficient;
do not replace the backend or relax the gate on this evidence. Reported short
startup delays:2880samples for octave-up and3648 for octave-down (60/76ms).
Medium:3392/4672samples (70.67/97.33ms). These are API reports, not independently
measured impulse alignment; include future adapter buffering before host reports.

## Reproduce the bounded probe

Use a temporary checkout; the project CMake does not fetch or link this library.
The probe writes only its named /tmp/darkvoice-rb-spectrum-*.raw measurement files.
On the tested little-endian Linux machine these contain native32-bit float samples.

```sh
git clone https://github.com/breakfastquay/rubberband.git /tmp/darkvoice-rubberband-audit
git -C /tmp/darkvoice-rubberband-audit checkout --detach 1d95888bec3ae0a17c0c4af791810d5a63f6bc35
g++ -std=c++20 -O2 -DNDEBUG -DALREADY_CONFIGURED -DUSE_BUILTIN_FFT \
  -DUSE_BQRESAMPLER -DNO_THREADING -DNO_THREAD_CHECKS -DNO_TIMING \
  -I/tmp/darkvoice-rubberband-audit \
  /tmp/darkvoice-rubberband-audit/single/RubberBandSingle.cpp \
  tools/qualification/rubberband_probe.cpp -o /tmp/darkvoice-rubberband-probe
/tmp/darkvoice-rubberband-probe
```

Next: compare R3/configuration and voiced excitation/envelope estimation against
retained failures and the broader source/filter matrix; verify explicit formant
semantics independently. Only then consider a separately contracted wrapper with
all rates/layouts, allocation/automation/reset/latency/tail/CPU checks and legally
usable vocal listening fixtures. Existing product processing remains unchanged.

Independent spectral reproduction (developer-only NumPy, not a product dependency):

```sh
python3 - <<'PY'
import numpy as np
for medium in (0, 1):
    for f0, center, pitch, formant in ((100,600,12,0), (70,600,-12,0), (180,1200,12,7)):
        x = np.fromfile(f'/tmp/darkvoice-rb-spectrum-{medium}-{f0}.raw', dtype='<f4')
        assert np.isfinite(x).all()
        spectrum = np.abs(np.fft.rfft(x[-48000:] * np.hanning(48000), n=1048576))
        i = int(spectrum.argmax())
        v = np.log(np.maximum(spectrum[i-1:i+2], 1e-30))
        delta = .5 * (v[0]-v[2]) / (v[0]-2*v[1]+v[2])
        peak = (i+delta) * 48000 / 1048576
        expected = center * 2**(formant/12)
        print(medium, f0, round(peak, 3), 'Hz', round(100*(peak/expected-1), 2), '%')
PY
```

## General R3 Stretcher comparison — 2026-10-24

Fresh GCC14.2.0/O2 reproduction uses the same immutable v4.0.0 commit above,
with built-in FFT/BQ and no threading, thread checks or timing. No product code,
CMake dependency, host interface or pitch/formant gate changes. The optional
probe and NumPy2.3.5 analysis are developer tools only; upstream source and raw
measurements remain outside the repository. This comparison **does not qualify a
replacement**, and PR #19's retained failures remain blocked.

Seven fixtures were declared before this run, each48kHz/mono/two seconds:
three harmonic-envelope cases matching the previous audit and four110Hz pure
sines shifted -12,-7,+7,+12 semitones. Harmonic fixtures retain80 harmonics,
Gaussian100Hz width and the same small harmonic floor. Both modes use
EngineFiner (confirmed engine3), default WindowStandard, ThreadingNever and
timeRatio1. Harmonic fixtures use FormantPreserved; pure sines use FormantShifted.
For zero formant shift, the API's scale0 selects automatic inverse-pitch
compensation; the explicit +7/+12 case uses exp2((7-12)/12)=0.749153538.

Offline mode studies the complete input, processes512-frame chunks and drains
available output. The library returns96000 frames with no manual trimming;
reported pad/delay0 is offline duration compensation, **not realtime zero
latency**. Realtime mode first feeds getPreferredStartPad() zeros, drains during
processing, discards getStartDelay() frames and retains96000 frames. Final
available() was -1 for every completed render. Every render contained exactly
96000 retained finite samples, with no retrieval stalls. Probe exit0 establishes
only rendering, file-I/O, length and finite/nonzero measurement validity; it does
not encode a sound-quality pass.

| F0 / envelope Hz | Pitch / formant semitones | Offline target-period correlation | Realtime correlation | Offline / realtime strongest line Hz | Envelope error offline / realtime |
| --- | --- | ---: | ---: | ---: | ---: |
|100 / 600|+12 / 0|0.882385650|0.946615990|599.999815 / 599.996803|-0.000031% / -0.000533%|
|70 / 600|-12 / 0|0.575208452|0.575208276|424.920674 / 424.920674|-29.179888% / -29.179888%|
|180 / 1200|+12 / +7|0.942251077|0.987747733|1802.022686 / 1801.941810|+0.225487% / +0.220989%|

The70Hz/octave-down case fails both the retained0.8 periodicity and20% envelope
criteria. The other two narrow vowel cases satisfy those two criteria; that is
not the complete149-case qualification matrix. The strongest line is an envelope
proxy on these synthetic fixtures, not a measured human-vowel formant estimator.

|110Hz sine shift|Offline crossing error cents|Realtime crossing error cents|Realtime pad / discard samples|Realtime raw output frames|
| --- | ---: | ---: | ---: | ---: |
|-12|+1.289756|+1.289756|2048 / 4096|106544|
|-7|-2.803114|-2.813998|2048 / 3069|104389|
|+7|+3.461952|-4.668289|3069 / 2048|103424|
|+12|-0.032208|+2.781217|4096 / 2048|104646|

All four sine cases in both modes satisfy the retained20-cent crossing criterion;
their interpolated FFT peak errors are below0.003 cents. Vowel realtime raw
output/pad/discard counts are104646/4096/2048 for100/+12 and180/+12,+7, and
106544/2048/4096 for70/-12. Raw counts include startup/tail output. These API
counts do not establish an impulse-aligned, constant-latency streaming wrapper,
allocation safety, deadline performance, stereo support or automation behavior.

### Octave ambiguity control

An unchanged70Hz synthetic vowel yields correlation0.999425659 at the requested
35Hz period, strongest line629.999999Hz (within20% of600Hz), yet only
4.5684878948e-15 of total spectral energy on the *odd*35Hz harmonics. Its70Hz
partials lie on the even35Hz grid. Thus high target-period correlation and an
acceptable envelope proxy can both falsely accept unchanged audio as octave-down.
Future backend qualification must independently discriminate the requested
fundamental, retain existing sine tests and run the complete source/filter matrix.

The actual R3 octave-down output differs from that control: correlation at the
70Hz period is0.032032693 offline /0.032032530 realtime; odd35Hz-grid energy is
0.678606 of the energy captured near the expected grid, but that grid captures
only0.357663 of total energy (odd-grid/total0.242712). Together with its failed
0.575208 target-period correlation and envelope error, this is not evidence of a
qualified harmonic35Hz result. Grid windows are +/-2Hz around up to80 harmonics
on a one-second Hann-windowed region; these diagnostics do not establish perceived
pitch or naturalness.

### Reproduce R3 observations

From the repository root on little-endian Linux with GCC14 and optional NumPy
installed, use a fresh temporary checkout. Existing product dependencies stay
unchanged. The probe writes named `/tmp/r3-{off,rt}-case-{0..6}.raw` native float32
files; its static assertions enforce little-endian32-bit floats for the analyser.
No binary, upstream source or measurement file is intended for a commit.

```sh
git clone https://github.com/breakfastquay/rubberband.git /tmp/darkvoice-rb-r3
git -C /tmp/darkvoice-rb-r3 checkout --detach 1d95888bec3ae0a17c0c4af791810d5a63f6bc35
g++ -std=c++20 -O2 -DNDEBUG -DALREADY_CONFIGURED -DUSE_BUILTIN_FFT \
  -DUSE_BQRESAMPLER -DNO_THREADING -DNO_THREAD_CHECKS -DNO_TIMING \
  -I/tmp/darkvoice-rb-r3 -c /tmp/darkvoice-rb-r3/single/RubberBandSingle.cpp \
  -o /tmp/darkvoice-r3-upstream.o
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -I/tmp/darkvoice-rb-r3 \
  tools/qualification/rubberband_stretcher_probe.cpp /tmp/darkvoice-r3-upstream.o \
  -o /tmp/darkvoice-r3-probe
/tmp/darkvoice-r3-probe > /tmp/darkvoice-r3-off.log
/tmp/darkvoice-r3-probe --realtime > /tmp/darkvoice-r3-rt.log
python3 -B tools/qualification/rubberband_stretcher_analyse.py > /tmp/darkvoice-r3-analysis.log
```

Additional inspected SHA256 fingerprints for this exact pin:

- RubberBandStretcher.h: deaf2147bd57a72aeec876f8dd76a6e282282d8dd1f52d8aa01f93fa70b5ed44
- single/RubberBandSingle.cpp: 991566737e449c3cc0f7b3bf7d7c21dc2b9fc52ea7479fa4165145d220b9a2fb

No broader configuration search,149-case pass, realtime host wrapper or listening
qualification is claimed. R3's two failed criteria in the retained70Hz case make
further integration unjustified on this evidence. Manual host/listening tests
remain deferred under the user's combined-qualification instruction.

Tool checks on this run: strict own-probe warnings-as-errors compilation,
clang-format19 check, Python syntax compilation and git diff whitespace check
passed. An unknown probe option returned usage/status2; a deliberately truncated
measurement was rejected before FFT analysis, then the original fixture was
restored. These validate diagnostic handling, not audio quality.
