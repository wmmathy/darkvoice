# Experimental streaming pitch/formant core

This is development/qualification infrastructure, not a completed Voice Sculptor
feature. It is OFF by default, is not linked to Vocal/Master/Studio, and exposes no
plugin controls or new preset/state fields. Existing products retain their current
latency, bypass and dry/wet behavior. Host integration is tracked by issue #18 and remains unimplemented.

The isolated PitchFormantProcessor wraps pinned MIT Signalsmith Stretch/Linear
with bounded 64-sample scratch buffers, deterministic parameter updates, a delayed
neutral ring and dry/processed crossfade. Prepare allocates; process/reset/setters
have ordinary allocation probes. Three candidate FFT profiles are available for
qualification only; these are not listening-qualified Live/Studio/Offline modes.
At48k their reported alignment latencies are1152/4608/9216 samples (24/96/192ms).
At44.1k times are higher; above48k block sizes double. No suite-wide latency or
low-latency monitoring promise is made.

cmake/Signalsmith.cmake fetches exact audited SHAs without executing upstream
CMake, verifies required header/license hashes and makes a private header copy.
Hash-guarded changes disable the near-silence immediate-copy shortcut and replace
the morphological envelope stage with a one-fundamental-spacing energy mean.
They preserve numerical floors and leave upstream files unchanged.
Active tiny audio can still be attenuated. The neutral path preserves delayed
normal finite input exactly, including tiny normals. Spectral processing clamps
input to +/-8 for numerical safety; it is not a mastering limiter.

## Reproduce without Actions

```sh
cmake -S . -B build/sculptor -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  -DDARKVOICE_BUILD_SCULPTOR_CORE=ON
cmake --build build/sculptor --parallel 2
ctest --test-dir build/sculptor --output-on-failure
```

Streaming and the original envelope fixtures pass, but the full qualification
command exits nonzero on three combined-shift periodicity cases. The gate remains
a real failing test, never skipped or marked WILL_FAIL. Select
`-R '^sculptor.streaming$'` to inspect streaming behavior separately. Add
`-DDARKVOICE_ENABLE_SANITIZERS=ON` in a separate build for ASan/UBSan.

For audited existing checkouts, CMake supports the standard overrides
FETCHCONTENT_SOURCE_DIR_DARKVOICE_STRETCH and FETCHCONTENT_SOURCE_DIR_DARKVOICE_LINEAR.
Fingerprints still verify every required header/license and final patched header. A tampered-source negative
fixture was rejected before compilation. MIT notices are in third_party/licenses.
Set DARKVOICE_BUILD_SCULPTOR_CORE=OFF to build the current products without these
experimental dependencies; this is the clean-build default.

## Qualification evidence and remaining work

Streaming checks cover profiles/rates/layouts, exact neutral impulse alignment,
small normal samples, reset, allocation failure and callback probes, partition
invariance, numerical stereo equality, quiet active timing and invalid input.
An independent agent confirmed initial small-window pitch errors spectrally;
raising Studio to4096 samples (doubled above48k) meets20cents for16 synthetic
110Hz/shift combinations. This is a narrow fixture result, not arbitrary vocal
accuracy. Backend stereo phase-lock arithmetic is numerically, not bit, equal.

The first multi-formant fixture used overlapping fixed search ranges that excluded
one downshifted target. Review corrected them to geometric boundaries between
resonances without changing tolerance. Independent isolated-formant cases originally
failed the20% envelope-peak gate at F0100Hz/base100Hz:

| Input envelope | Formant shift | Expected peak | Original peak | Corrected peak |
| --- | ---: | ---: | ---: | ---: |
| 600Hz | +7 | 899Hz | 700Hz | 900Hz |
| 1200Hz | -7 | 801Hz | 1000Hz | 800Hz |
| 1200Hz | +7 | 1798Hz | 2200Hz | 1800Hz |

The correction uses a symmetric linear-energy mean over one fundamental spacing,
with preallocated source scratch, compensated double accumulation and periodic
rebasing for O(bands) processing. No F0 reinterpretation, center-specific tuning or
tolerance relaxation is used. All original76checks pass. Sixteen additional cases
at48/96k, F070/180Hz,600/1200Hz envelopes and+/-7semitones pass, bringing the
passing synthetic subset to140checks. The full suite now has149checks, with
three combined-periodicity failures retained below. These grids resolve the expected envelope target;
a20% envelope gate cannot usefully qualify a target between very sparse harmonics.

This remains experimental. Wider pitch/F0/material coverage, transient/unvoiced
behavior, CPU/listening and aligned host dry/mix/bypass still require qualification.
No reference-aware correction, harmonies or host integration is implemented by
this core. Issue #18 stays open. The user has made the repo public and reauthorized
Actions; new native results must be recorded from actual runs.

## Automation regression coverage

Sample-positioned parameter events now cover all four rates and all three candidate
profiles in stereo, with independent channel inputs. Events at non-grid offsets
exercise opposite pitch/formant extremes, preservation changes, auto/manual base
changes and return to neutral. Rendering identical events in 37- and 1024-sample
host partitions produces maximum difference0 in the local Release run. Processing
and setters allocate nothing under ordinary new/new[] rejection; latency remains
fixed, both channels stay finite and settle to exact delayed dry. These checks do
not establish click-free perceptual quality or establish perceptual formant quality.

## CPU wall-time probe

With the experimental option enabled, run `build/sculptor/darkvoice_sculptor_bench`
without arguments and redirect stdout to a JSON file. It measures24 combinations:
four rates, three candidate profiles and64/512-frame stereo callbacks. It warms
one second and measures at least three seconds per combination. Signal creation,
allocation, sorting, validation and reporting are outside the callback interval;
setParameters/process and clock overhead are inside. Noise, harmonic and transient
sections alternate with parameter changes. Durations are rounded to whole blocks.
Schema1 reports compiler/configuration, actual durations, latency and callback wall
time percentiles/maximum, processing/audio ratio and observed budget overruns.

Initial GCC14.2 Release x86_64 Linux shared-cloud run: processing/audio ratios
0.0240–0.0659, with2 observed callback-budget overruns. These are observed wall
times, not process CPU utilization or hard real-time certification. Max callbacks
were1.77ms Live,0.84ms Studio and1.55ms Offline in that run. The fixture has no audio
device and does not measure audible glitches, host CPU or macOS performance. Do
not turn this noisy shared-machine measurement into a timing-sensitive CTest gate.
For comparisons retain raw JSON and record machine/load/toolchain; run on the
actual target machine with repeated measurements before setting performance gates.

Independent review of the exact compensated/rebased stage found no blocking
bounds, lifecycle, allocation or O(bands) issue. A temporary exploratory combined
pitch/formant matrix reported38 envelope-peak failures in144cases. Its exact-grid
amplitude estimator has not been independently verified against pitch errors, so
these are unresolved exploratory results, not a passed preservation qualification
or a proven diagnosis. Preserve issue #18 and investigate broader simultaneous
pitch/formant behavior before exposing the core. The committed140check suite
qualifies its stated synthetic subset only. No claim of arbitrary formant
preservation or perceptual completion follows from correcting the original cases.

## Retained combined-shift periodicity blocker

The broad exploratory exact-grid amplitude estimator overstates some envelope
errors. Independent Hann-windowed FFT analysis nevertheless confirms off-grid
output in representative combined pitch/preservation cases. A committed regression
uses the same >0.8 expected-period correlation criterion as the existing vowels.
It does not infer one trustworthy F0 from these inharmonic outputs. At48k Offline:

| Input F0 / envelope | Pitch / formant semitones | Target F0 | Measured target-period correlation |
| --- | --- | ---: | ---: |
| 100 / 600Hz | +12 / 0 | 200Hz | -0.358 |
| 70 / 600Hz | -12 / 0 | 35Hz | 0.182 |
| 180 / 1200Hz | +12 / +7 | 360Hz | -0.011 |

`sculptor.qualification` now runs149checks with3failures: the original140checks
pass, the three new setup/render pairs pass and all three periodicity gates fail.
Keep issue #18 open and investigate pitch phase/frequency mapping or compare a
compatible backend. The envelope correction improves a documented subset only.

## Compensation-path investigation and alignment prerequisite

A read-only follow-up compared the three failing source/filter fixtures with
preserveFormants=false and formant shift0. Target-period correlations then reach
0.999618/0.979937/0.994710, isolating the observed limitation to the compensation
path for those fixtures. An experimental private copy kept raw energy/phase history
and applied the envelope ratio only when copying to the synthesis spectrum; it
still failed all three periodicity gates. That experiment is rejected and was
not integrated. Boosting weak mapped spectral regions is a candidate explanation,
not a proven complete diagnosis. Investigate excitation-aware transfer or a
compatible backend; do not insert an arbitrary gain clamp to tune fixtures.

`LatencyAlignedMix` now independently implements delayed original-dry blending
and smoothed bypass with warm dry history. It is not connected to this backend
or any product. Callers still own backend warming, actual matching delay and host
latency reporting. Its contract and tests do not waive the existing quality gate.

## Peak coherence and excitation-map investigation — 2026-10-22

A separate isolated investigation reproduced the unchanged149check qualification
suite with the same3failures, plus269passing streaming and464passing alignment
checks. Temporary private-header experiments kept dependency pins and fixtures
unchanged. Correlations below use the same final-one-second, rounded target-period
lag test as the retained suite, not a new tolerance:

| Temporary algorithm | 100/600, +12/0 | 70/600, -12/0 | 180/1200, +12/+7 |
| --- | ---: | ---: | ---: |
| Retained implementation | -0.357724 | 0.181850 | -0.011096 |
| Peak-locked envelope gain in input-energy path | 0.995317 | 0.301105 | 0.999089 |
| Above plus compensated-energy peak remapping | 0.995352 | 0.483413 | 0.998906 |
| Peak-locked gain only at final synthesis | 0.995306 | -0.058471 | 0.996333 |
| Whitened-excitation peak map plus peak-locked final synthesis gain | 0.999631 | 0.792134 | 0.997502 |

Peak locking uses one envelope ratio per detected source peak's nearest-peak
region, preserving the analysis-lobe gain within that region. Whitening here means
using source energy divided by its estimated source envelope solely for peak-map
detection; raw phase/energy history stays unchanged and envelope gain is applied
when copying the final spectrum. This improves the observed subset but does not
qualify the algorithm. Interframe-phase peak localization still left the low-F0
case below0.8; accepting weaker phase data caused another retained case to fail.
No experiment, arbitrary gain clamp or relaxed threshold was integrated.

A broader180case Offline matrix compared retained code with the whitened-excitation
prototype: rates48/96kHz; F0=70/100/180Hz; envelope=600/1200Hz; pitch=-12/-7/0/+7/+12;
formant=-7/0/+7; preservation enabled and exact manual input F0 supplied. Retained
code failed61 intended-periodicity cases; the prototype failed13. The same-grid
amplitude search reported31 versus10 exploratory envelope-peak discrepancies among
targets with a harmonic within20percent, and2 targets had no such harmonic. These
envelope counts are **not** independently verified formant errors: an exact-grid
amplitude search is unreliable for off-grid/inharmonic outputs. All180 periodicity
checks remained active, including both sparsely resolved targets. The prototype
passed269streaming checks but still failed the original qualification gate; it was
rejected and the original generated header restored to SHA256
`7003842b8347ba10710f280c33dba494cd3466a54b240cef76cfbbe32a9a93b8`.

The optional diagnostic CLI reproduces the **retained implementation** matrix and
prints rate/F0/envelope/pitch/formant, correlation, exploratory harmonic-grid peak,
target and resolvability, followed by totals. Each two-second fixture is measured
in its final second; the correlation starts one rounded target period into that
window. Resolvability means the nearest intended output harmonic is within20percent
of the target envelope; it does not skip the periodicity assertion. This CLI exits
nonzero on periodicity failures and is outside the short CTest suites. Existing
149qualification assertions and their thresholds remain unchanged:

```sh
cmake -S . -B build/sculptor -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  -DDARKVOICE_BUILD_SCULPTOR_CORE=ON
cmake --build build/sculptor --parallel 2
build/sculptor/darkvoice_sculptor_tests matrix > combined-matrix.log 2>&1
# Current retained code: 540 checks, 61 failures (expected unresolved defects).
```

The evidence supports investigating excitation-aware mapping and stable source/
filter reconstruction; it does not prove a complete root cause or establish any
vocal/transient/perceptual qualification. PR #19 and issue #18 remain blocked.

Lead integration independently rebuilt the unchanged-backend tree and reproduced
269/269 streaming,464/464 alignment and149checks/3qualificationfailures after adding
the diagnostic CLI. Independent read-only QA reviewed metric definitions/default
suite separation; no180case independent rerun is claimed. No product integration.
