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
