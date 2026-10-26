# Pitch/formant gain-coupling diagnostic — 2026-10-24

Developer-only investigation of the failed experimental pitch/formant core.
This is not a qualified backend, product feature, replacement gate or claim of
natural sound. Manual host/microphone/listening tests remain deferred under the
user's combined-qualification instruction. No existing production source, CMake,
upstream checkout or retained test was changed by this diagnostic.

## Source and experiment boundaries

Repository base: fa4af297 (broader experimental diagnostics), with existing
PitchFormantProcessor.cpp SHA256
d5281af605ba5cd7ecf466404bdfe838b571ca84cddbcc551bdb493857c86055.
Signalsmith Stretch pin a670068d9aeb64913331d5cc29337b19a457a7df and Linear pin
de55e6a50ffcf6f8f43f649692d94691c7025151 are MIT; notices already exist in
third_party/licenses and are copied alongside temporary instrumented files.
See PITCH_BACKEND_AUDIT.md for license/source scope. No dependency was added.

The existing guarded private header includes the near-silence shortcut fix and
experimental linear-energy envelope estimator; its SHA256 is
7003842b8347ba10710f280c33dba494cd3466a54b240cef76cfbbe32a9a93b8.
The diagnostic generator accepts exactly that fingerprint and the wrapper above,
then writes temporary instrumented copies. Original pinned sources stay intact.
Generated instrumented header SHA256:
7c9033f59f7c20f5f643f4b069f7058f590c3e914480e8eefc45006cc8dfeb3c;
generated split-selectable wrapper SHA256:
3b385d1fa70fb16812c0aae1d104e5140ad94418a5fbb3f7b02b68028370586e.

All renders use 48 kHz mono, two seconds/96,000 frames, Offline8192-sample FFT,
1024-sample hop, 512-frame public calls, seed0 and reset-settled static targets.
They retain the original synthetic float-accumulated vowel: 80 harmonics with
amplitude .025*(exp(-.5*((h*F0-center)/100)^2)+.002/h). Analysis uses the final
48,000 emitted frames without latency cropping/flushing, matching the retained
correlation fixture's measurement region. Retained periodicity requires >.8;
no target or tolerance is changed. These probes do not rerun or pass the complete
149-check or optional540-case suites.

| Case | Input F0 / envelope Hz | Pitch / formant semitones | Target F0 Hz |
| --- | --- | --- | ---: |
|0|100 /600|+12 /0|200|
|1|70 /600|-12 /0|35|
|2|180 /1200|+12 /+7|360|

## Phase1: absolute-level controls

Three predeclared gains1,.25,4 are applied after retained float signal generation.
Each is rendered with requested formant processing, and with formant shift0 /
preservationfalse as a periodicity control. The latter deliberately does not
meet the requested independent envelope and cannot qualify the product feature.
Maximum absolute input peak over all cases is .346643686, below the wrapper's
+/-8 FFT-input clamp. All18 renders contained96,000 finite samples with positive total energy;
startup zeros are legitimate.

| Case | Input gain | Requested formant processing correlation | Disabled formant processing correlation |
| --- | ---: | ---: | ---: |
|0|1.00|-0.357723893|0.999618463|
|0|0.25|-0.361280776|0.999618413|
|0|4.00|-0.200112844|0.999618466|
|1|1.00|0.181849690|0.979937272|
|1|0.25|-0.169943746|0.979937299|
|1|4.00|0.254288778|0.979937253|
|2|1.00|-0.011095603|0.994710073|
|2|0.25|0.079804375|0.994710051|
|2|4.00|-0.111628049|0.994710141|

All nine requested-formant cases fail the retained correlation criterion; all
nine disabled-formant controls satisfy that criterion. Power-of-two input scaling
substantially changes compensated results while the controls remain nearly
invariant. This supports investigating numerical/phase-conditioning effects;
it does not prove that a particular noise-floor fallback causes the failures.

At normal gain, compensated strongest-line/envelope-error/grid-capture results:

| Case | Strongest line Hz | Envelope proxy error | Expected grid energy / total |
| --- | ---: | ---: | ---: |
|0|675.000554|+12.500092%|.056731209|
|1|548.157477|-8.640421%|.068230629|
|2|1884.348284|+4.804300%|.001700466|

A strongest line near the desired envelope can therefore coexist with severely
off-grid output. It is a synthetic envelope proxy, not a fundamental or formant
estimator. The existing retained combined regression asserts periodicity, and
these reported envelope errors do not turn any failed regression into a pass.

## Phase2: predeclared gain/scheduling matrix

For each original case at gain1, compare splitComputationtrue/false and four
modes. Mode0 is the existing experimental gain. Mode1 forces energy ratio1
while keeping all three formant-processing steps, isolating their scheduling
from their amplitude modification. Mode2 records ratios through48 completed
formant updates then freezes that complete ratio vector. The48 updates span at least
47hop intervals (1.002667 seconds), depending on startup scheduling. The retained
final-half measurement starts at1second, so its beginning overlaps the last
dynamic updates; mode2 is not an entirely frozen-window oracle. The reported
measurement window and numeric results remain unchanged. Mode3 uses
the known synthetic Gaussian-plus-tail envelope instead of the measured envelope:
A(f)=exp(-.5*((f-center)/100)^2)+.002/max(f/F0,1), with energy ratio
[A(f*pitchScale/formantScale)/A(f)]^2. This is a fixture-specific diagnostic
oracle, not a deployable estimator or permitted product dependency on source F0.

Splittrue reports9216-sample latency; splitfalse8192. No latency correction is
applied to either stationary final measurement region. Every matrix render has
96,000 finite samples with positive total energy, allowing startup zeros. All24 predeclared matrix cases were executed.

| Split | Case | Current gain | Unity gain, same steps | Frozen measured gain | Analytic gain |
| --- | --- | ---: | ---: | ---: | ---: |
|1|0|-0.357723893|0.999618463|-0.357724077|-0.162573093|
|1|1|0.181849690|0.979937272|0.181849094|0.207876339|
|1|2|-0.011095603|0.994710073|-0.011095599|0.070117787|
|0|0|-0.356803634|0.999618609|-0.356803535|-0.162009289|
|0|1|0.184400333|0.979967312|0.184399587|0.206326532|
|0|2|-0.013161155|0.994710224|-0.013161162|0.069880293|

Unity-gain mode satisfies the periodicity criterion in all six rows. Current,
frozen and analytic gain modes fail it in all18 remaining rows. The instrumented
splittrue/current output reproduces the original three correlations exactly to
the printed precision. Freezing measured ratios changes those correlations by
less than1e-6; removing split computation preserves the failure pattern. This
argues against steady envelope drift or split scheduling as simple explanations.
Replacing the estimated envelope with an analytic one also fails; another
estimator substitution alone is not justified by this evidence.

### Bounded prediction instrumentation

The temporary header counts Prediction::makeOutput calls and its
phaseNorm<=noiseFloor input-phase fallback. It also sums that function's requested
prediction energy for all calls and fallback calls. These are aggregate internal
prediction-energy sums across all frames/bands, including startup and silence;
they are **not** measured rendered-output energy or independent causal attribution.
Raw fallback counts are dominated by quiet/empty bands and should not be read as
an audible-error percentage. Maximum ratios may also occur in quiet bands.

Splittrue/current-gain observations:

| Case | Prediction calls | Fallback calls | Fallback prediction-energy share | Maximum energy ratio |
| --- | ---: | ---: | ---: | ---: |
|0|383488|341476|0.000095117|899913920|
|1|383488|368351|0.023548055|92920368|
|2|383488|357492|0.000037598|115304696|

Only the70Hz/octave-down case has more than .01 fallback prediction-energy share;
the other two shares are below.0001. Thus a claim that directly audible fallback
energy alone explains all failures is unsupported. Large gains, absolute-level
dependence and coupled amplitude/phase histories still warrant investigation.
No noise-floor removal, phase-normalization repair or further backend/configuration
search was executed. The previously rejected synthesis-only gain experiment is
not claimed as a new solution.

## Independent octave-ambiguity controls

The probe additionally renders an ideal35Hz Gaussian-envelope source, unchanged
70Hz source, and35Hz source with every odd harmonic removed. These are generated
independently of the shifter. CorrHalfT measures the target35Hz period divided by2;
CorrDoubleT twice that period. Harmonic energy uses +/-2Hz bins around the first80
expected harmonics in a one-second Hann-windowed FFT. Always report captured-grid
energy/total alongside odd-grid energy/total; odd/captured alone can hide off-grid
energy. No fixed odd-energy qualification threshold is invented here.

| Control | CorrHalfT | CorrT | CorrDoubleT | Strongest line Hz | Captured grid / total | Odd grid / total |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
|ideal35|-0.000000656|0.999425598|0.999936188|595.000000622|1.000000000|0.500000327|
|unchanged70|0.999744762|0.999425659|0.999936187|629.999998540|0.999999981|0.000000000|
|evenOnly35|0.999744758|0.999425651|0.999936186|629.999998540|1.000000000|0.000000000|

Both unchanged70 and ideal35 have target-period correlation about.999426 and
an envelope proxy within20% of600Hz. Only the ideal source has meaningful odd35Hz
harmonic energy (.500000327 of total); the unchanged control is approximately
zero. This demonstrates the existing periodicity/envelope false-acceptance risk,
not a universal fundamental detector. Missing-fundamental/even-only material can
be intrinsically ambiguous. Future acceptance should retain sine/global spectral
frequency checks, use independent ideal/unchanged controls, and declare ambiguity
rather than impose an unsupported universal odd-energy threshold.

## Reproduce

Executed with GCC14.2.0, C++20/O2, CMake3.31.6 and optional developer NumPy2.3.5
on little-endian Linux. The probe enforces IEEE float32/little-endian storage.
Source checkouts below already existed at the exact MIT pins above and were clean.
If reproducing elsewhere, obtain those exact pins first; do not substitute mutable
tags. From the repository root containing these developer tools, create a
separate temporary worktree at the experimental source revision. This does not
add the experimental backend to main:

```sh
pitchProbeRoot="$PWD"
git worktree add --detach /tmp/darkvoice-pitch-coupling-source fa4af2971779737acdd72e7ca9f37070a926aabc
cd /tmp/darkvoice-pitch-coupling-source
source tools/cloud-env.sh
cmake -S . -B /tmp/pitch-coupling-base -DCMAKE_BUILD_TYPE=Release \
  -DDARKVOICE_BUILD_SCULPTOR_CORE=ON \
  -DFETCHCONTENT_SOURCE_DIR_DARKVOICE_STRETCH=/workspace/dependencies/signalsmith-stretch \
  -DFETCHCONTENT_SOURCE_DIR_DARKVOICE_LINEAR=/workspace/dependencies/signalsmith-linear
python3 -B "$pitchProbeRoot/tools/qualification/pitch_coupling_analyse.py" --prepare-private \
  /tmp/pitch-coupling-base/darkvoice-stretch-private /tmp/pitch-coupling-private
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  -I/tmp/pitch-coupling-base/darkvoice-stretch-private \
  src/PitchFormantProcessor.cpp "$pitchProbeRoot/tools/qualification/pitch_coupling_probe.cpp" \
  -o /tmp/pitch-coupling-probe
/tmp/pitch-coupling-probe > /tmp/pitch-coupling.log
python3 -B "$pitchProbeRoot/tools/qualification/pitch_coupling_analyse.py" > /tmp/pitch-coupling-analysis.csv
for split in 1 0; do
  g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror \
    -DDARKVOICE_COUPLING_INSTRUMENTED -DDARKVOICE_COUPLING_SPLIT=$split \
    -Iinclude -I/tmp/pitch-coupling-private \
    /tmp/pitch-coupling-private/PitchFormantProcessor.cpp \
    "$pitchProbeRoot/tools/qualification/pitch_coupling_probe.cpp" -o /tmp/pitch-matrix-s$split
  /tmp/pitch-matrix-s$split --matrix > /tmp/pitch-matrix-s$split.log
done
python3 -B "$pitchProbeRoot/tools/qualification/pitch_coupling_analyse.py" --matrix > /tmp/pitch-matrix-analysis.csv
```

The raw measurements and upstream/private source copies remain temporary and are
not intended for commits. Probe exit0 validates rendering/I/O, exact length, finite samples and positive total energy;
it does not encode a sound-quality pass. Optional analysis validates exact file
length, finite/nonzero energy and interpolable FFT maxima. Strict compile and
clang-format19 checks passed. Deliberate malformed-length, zero-energy and NaN
measurements were rejected; an unknown probe argument returned usage/status2.
No full149-check pass, sanitizer/real-time allocation/CPU deadline, automation,
stereo/native-host or listening qualification follows. Experimental integration
and issue18 remain blocked. No product parameters or presets are added.
