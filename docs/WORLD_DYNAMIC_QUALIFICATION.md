# Offline dynamic pitch qualification v1 — 2026-11-19

This is a predeclared diagnostic expansion, not product pitch qualification.
Freeze processor/estimator behavior at aac82bf. Preserve all42 retained and180
stationary hard gates and their negative findings. No streaming, latency, naturalness
or realtime claim follows. Manual listening remains deferred.

## Fixed manifest

Use48/96kHz, exact three-second duration and the retained harmonic/envelope family
(center1200Hz). Integrate phase sample by sample from instantaneous F0; never use
sin(2*pi*F0(t)*t). Voiced regions have10ms edges. Phase/voicing truth is measurement
metadata only and must never be passed into backend estimation or synthesis.

- Vibrato:100*2^(50*sin(2*pi*5.5*t)/1200)Hz.
- Rising glide:70 to180Hz, log-linear from.25 to2.75s with endpoint holds.
- Falling glide: reverse of rising glide.
- Sequence: silence0–.3s;110Hz vowel.3–1s; fricative1–1.35s;
  vowel1.35–2.05s; silence2.05–2.4s; vowel2.4–2.85s; silence thereafter.
- Fricative only: fixed-seed1500–8000Hz noise with20ms edge fades.
- Silence: exact zeros.

First four fixtures: pitch−12/0/+12 × formant0/+7 × two rates =48 renders.
Fricative/silence: pitch−12/0/+12, formant0 × two rates =12 renders. Do not treat
formant-shifted noise as requiring an unchanged spectral envelope.

## Input contour and output measurements

Keep existing85–93ms NCCF windows, acceptance.9, bounds50–800Hz and candidate
selection frozen. Rejection of legitimate vibrato/glides is a reported failure,
not permission to lower acceptance. At5ms timestamps require voiced coverage>=90%,
median absolute error<=20cents, P95<=50cents, fraction exceeding600cents<=1%,
and longest interior missing-voicing gap<=50ms. Exclude initial/final100ms and
±100ms around declared voiced/UV boundaries; publish exclusions and boundary
measurements separately. No interpolation across rejected contour frames.

Output uses expected integrated phase scaled by2^(pitch/12). Resample onto that
phase coordinate and compare one expected cycle in six-cycle windows; correlation
must exceed.8. Use a fixed fractional-cycle search around one cycle to measure
local cents error (median<=20, P95<=50); publish search bounds/grid and endpoint
hits. This search is not an octave detector. Independently expose odd-harmonic
occupancy on the target grid and flag unresolved octave ambiguity. Measure envelope
peaks through phase-aware harmonic coefficients/instantaneous frequencies; retain
20% error bound where resolvable. Static-lag correlation and fixed-frequency FFT
peaks may be diagnostics only for changing F0. Do not search for favorable time
alignment, change source duration or substitute oracle F0 into the backend.

## Analyzer calibration before backend evaluation

Independently generate correct shifted dynamic signals and unchanged, octave-high,
octave-low, even-only, stationary-mean-F0, flattened-vibrato and reversed-glide
controls. Correct controls must pass applicable gates and wrong controls must fail
applicable gates. Publish intrinsic octave ambiguity rather than inventing a
fundamental. Validate noise/silence fixtures and malformed input rejection too.
These are engineering thresholds declared before results, not observed passes.

## Unvoiced gates at formant0

Interior false-voicing fraction<=5%, longest run<=20ms; voicing-boundary displacement
<=50ms. Fricative output RMS within±3dB; normalized1500–8000Hz band-energy vector
similarity>=.9. Fricative output normalized autocorrelation across50–800Hz lags
<=.3; require input<=.15 or reject the fixture. Energy-envelope boundary displacement
on10ms frames<=25ms; report transient overshoot. Pure-silence RMS<=1e-7,
peak<=1e-6. Sequence silence interiors, excluding100ms boundaries, must be<=−60dB
relative to adjacent voiced RMS. Noise waveform identity is not required because
WORLD resynthesizes stochastic excitation. Do not silently apply nonzero-output
requirements to silence or unchanged-spectrum gates to warped noise.

## Bounded first assignment

Implement analyzer calibration only in tools/qualification/world_dynamic_measure.py
and tests/world_dynamic_measure_tests.py. No backend/probe/static-analyzer edits,
no new dependencies beyond existing NumPy, and no product code. Own fixtures must
be deterministic and independently synthesized in tests; expose every measured
metric and every failed gate. Include a runnable --self-test and explicit CLI
error exits. If calibration cannot discriminate a wrong control under the frozen
metrics, report the limitation and discuss it before backend work; do not silently
loosen gates. Lead owns this contract and later backend interfaces. Successful
calibration is a dependency, not permission to represent dynamic backend success.

## Calibration amendment v2: explicit octave ambiguity — 2026-11-19

The v1 calibration failed to reject two wrong controls. At vibrato100Hz and
pitch+12, octave-high output had full-cycle correlation>=.99998334, median error
.0007436cents, P95.002669cents, envelope error.03013 and apparent odd energy
>=4.2164e-8. Even-only output showed a similar false odd floor. These are retained
negative results: dynamic phase/amplitude interpolation invalidates a numerical-zero
odd-energy test. Do not describe v1 as passed or begin backend evaluation.

Add a stricter ambiguity guard, without changing any existing gate, fixture,
search window or timeline. In the same six-cycle phase windows, also measure
normalized correlation at HALF the expected cycle. When full-cycle correlation
exceeds.8 and half-cycle correlation is>=.995, classify the result octave-ambiguous;
it must never be an unqualified pitch pass. Publish per-window half-cycle values,
summary extrema and ambiguity fraction. Wrong octave-high/even-only controls must
fail or be ambiguous; correct controls must pass applicable gates without ambiguity,
or calibration remains failed. Keep the original v1 outputs for comparison.

The.995 threshold is a conservative engineering guard for almost duplicate half
cycles: unit-normalized difference power is about.5% per signal. It is not a
universal fundamental detector. Legitimate signals with very weak odd harmonics
may remain unresolved; do not force a pitch label or relax the guard to admit
them. No backend behavior changes or natural-vocal qualification follow from a
synthetic calibration pass. Any remaining wrong-control pass must be reported
before another contract change.

## Energy-boundary estimator v1 — 2026-11-19

Close the previously unspecified10ms energy-boundary metric before any backend
work. Use fixed10ms RMS windows on a5ms hop, with timestamps at window centers;
use whole windows only, without padding, rescaling or alignment searches. Derive
one threshold from the INPUT: median strictly positive frame RMS times10^(-30/20).
Apply this SAME absolute threshold to input and output. Extract ordered inactive/
active transitions, including an onset at the first frame center if initially
active and an offset at the last frame center if finally active. Match by order
and transition kind; a count/kind mismatch fails, rather than dropping edges or
choosing favorable pairs. Maximum absolute paired displacement must be<=25ms.
Publish threshold, window/hop, timestamps, edge counts and individual displacements.

All-zero input has no meaningful activity reference: report NotApplicable for
this metric and retain the strict independent silence RMS/peak gates; never call
NotApplicable a measured pass. Voiced-to-fricative boundaries without an energy
crossing are not energy edges; the separate5ms contour/boundary gates still apply.
This estimator checks activity timing, not consonant intelligibility or waveform
identity. Calibrate unchanged and gain±3dB signals plus deliberate40/80ms shifted,
extra-edge and missing-edge controls. Source/output duration and time coordinates
remain fixed. Do not change the failed octave calibration or any earlier gate.
