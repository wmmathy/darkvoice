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

## Executed implementation and result

**Negative audio experiment:** exact reference-state separation passed, but every
nonunity requested-formant case still failed retained periodicity. This option
must not be enabled as a backend repair or integrated into products. No 149-gate
rerun, sanitizer, deadline/allocation, broader quality or manual qualification was
justified by this result. No gates, product source, public interfaces, parameters,
latency contracts, pinned checkouts or dependencies changed. No commit/push/PR was
performed for the implementation.

`DARKVOICE_EXPERIMENT_PHASE_DECOUPLING` defaults OFF inside the existing optional
experimental core configuration. OFF still generates the original private header,
SHA256 `7003842b8347ba10710f280c33dba494cd3466a54b240cef76cfbbe32a9a93b8`.
ON applies `cmake/SignalsmithPhaseDecoupling.cmake` only after that verification;
every replacement requires exactly one anchor, and the resulting private header
is verified as SHA256
`86bcf041a086f2dd7c1c8a94e91a16c522416bd6c62de003524dfe840d4184ae`.
The existing MIT notices and original dependency/license checks remain in place.

The new per-band `synthesisInputEnergy` is initialized during configuration/reset
and refreshed from original `inputEnergy` after the map step on every spectrum
processing frame, even when the analysis is repeated or formants are disabled.
Only this copy receives formant energy ratios. All original analysis energies,
persistent `Prediction::energy`, temporal denominators, persistent complex
`Band::output`, in-place short/long vertical neighbor histories, channel-leader
selection, cross-channel locking and `makeOutput` fallback remain uncompensated.
At the existing STFT-copy step, the separate energy is mapped through the same
fractional interpolation and `freqGrad`, and reference complex output is multiplied
by its square-root energy ratio. Unity copies reference complex output directly;
zero reference energy emits zero without selecting a new phase. The emitted
spectrum is never written into prediction history. Inspected Linear STFT synthesis
consumes that spectrum into overlap-add buffers; rendered output is not a source
for Stretch's prediction in this path.

One separately controlled lifecycle change exists only with ON: configure and
reset explicitly value-initialize `channelPredictions`. Upstream reset cleared
bands but left those predictions. A reset-only temporary header (SHA256
`1e95a414450c9dc852d45ffc918587cd65abfad31e4aa553ee1f3620a6056cf3`)
produced **bitwise identical** files to OFF for all18 ordinary renders and all3
independent controls; it does not explain the changed compensated results. Matched
reset/reconfigure twins are covered below; this is not a general upstream reset
qualification.

## Exact reference invariant

`tools/qualification/phase_decoupling_probe.cpp` supplies developer macros only.
After every `processSpectrum` step, bounded hooks record tagged scalar bits for
original complex input/previous input/reference output, original input energy,
prediction energy/input, frequency map, shared/smoothed energies and smoothing
state, peaks, step/new-spectrum/time-factor, and complete custom RNG state.
`makeOutput` arguments and fallback decisions and maximum-energy-channel leaders
are also recorded. Thus preliminary temporal predictions and each vertical chunk's
neighbor history are compared, including the in-place completed lower-neighbor
outputs; phase-predictor arguments cover intermediate weighted sums. This is direct
bit comparison of tagged values, not a digest or angle-tolerance comparison.
Each twin reserves4,000,000 events before processing; overflow fails. Callback
tracing performs bounded writes without allocating. This harness is not a general
real-time allocation or CPU qualification.

Fourteen matched stereo twins execute160 variable calls each: split OFF/ON and
unity, measured, analytic, fixed nonuniform, time-varying, zero-notch and unequal
channel gains. Both twins receive identical input, pitch, seed, settings and
schedules; only envelope gains differ. Calls include sizes1/37/64/137/511,
zero-input repeated spectra, output/input stretch3 activating randomized vertical
prediction, silence/resume, formant disable/enable, neutral/restored pitch, reset,
seek, and reconfigure512→1024. Stock-seeded unchanged coupling diagnostics below
also retain identical fallback statistics across gain modes; the complete RNG
state/draw invariant specifically uses the probe's explicitly observable engine.

| Schedule | Events compared per gain mode | Unity changed samples | Other modes changed samples |
| --- | ---: | ---: | ---: |
| split OFF |62,327,218|0|50,272 (notch50,271)|
| split ON |62,007,276|0|49,760 (notch49,759)|

All870,341,458 compared tagged events matched exactly, with no overflow. Coverage
across both twins:2,664,228 RNG draws,14,924 repeated-spectrum snapshots,
6,465,536 true fallback decisions and3,491,712 channel-leader selections.
All6,981,632 emitted-bin checks were finite/nonnegative;3,611,006 zero-reference
bins emitted zero and2,212,097 unity bins copied complex bits exactly.

Emitted-energy acceptance is separate: compare double-precision norm of emitted
complex output against `norm(referenceComplex)*desiredEnergy/referenceEnergy`,
with denominator at least float minimum-normal energy and relative tolerance
`2e-6`. This tests application of gain to the actual reference complex output,
including existing fallback regularization; it does not incorrectly assert the
reference norm always equals requested energy. Observed maximum relative error
`2.70179908e-7`, zero invalid checks.

The harness detects a deliberately contaminated temporary header: changing only
the preliminary prediction's energy interpolation from `inputEnergy` to
`synthesisInputEnergy` rejects split OFF/measured mode at call2, first unequal
event61,561 (exit1). A temporary altered first patch anchor is rejected by CMake
with `Phase experiment anchor missing` (exit1). These controls are not retained
source changes.

## Unchanged42-render diagnostic

The existing coupling probe/analyzer were invoked from their separate worktree.
Fixtures, float accumulation, sample rate48kHz, Offline8192/1024 FFT/hop,
2seconds/96,000frames, seed0,512-frame public calls, final48,000-frame measurement,
correlation threshold `>.8`, three cases and all gain/mode/schedule choices remain
unchanged. All42 renders contain96,000 finite samples with positive energy; startup
zeros remain legitimate. No latency cropping or revised envelope gate was added.

| Case | Gain | Requested-formant correlation | Disabled-formant correlation |
| --- | ---: | ---: | ---: |
|0|1|-.331621304|.999618463|
|0|.25|-.330873772|.999618413|
|0|4|-.331591142|.999618466|
|1|1|-.127238644|.979937272|
|1|.25|-.143794674|.979937299|
|1|4|.299755141|.979937253|
|2|1|-.230338874|.994710073|
|2|.25|-.229955169|.994710051|
|2|4|-.230487569|.994710141|

All9 requested-formant rows fail; all9 disabled controls pass correlation only.
The three gain1 requested rows are the unchanged original combined-periodicity
cases. All9 disabled emitted files match OFF bitwise. The three split-ON matrix
unity/all-formant-steps files match the corresponding disabled files bitwise.

| Split | Case | Measured gain | Unity/same steps | Frozen measured | Analytic gain |
| --- | --- | ---: | ---: | ---: | ---: |
|ON|0|-.331621304|.999618463|-.331621376|-.318917693|
|ON|1|-.127238644|.979937272|-.127244214|-.073318293|
|ON|2|-.230338874|.994710073|-.230338881|-.233354818|
|OFF|0|-.331696927|.999618609|-.331697192|-.319029278|
|OFF|1|-.122473375|.979967312|-.122478884|-.068179364|
|OFF|2|-.230427083|.994710224|-.230427079|-.233434048|

All18 nonunity matrix renders fail; all6 unity renders pass periodicity only.
Frozen gains use exactly the existing48-update freeze and its overlapping initial
measurement window; they are not reclassified as a fully frozen-window oracle.
Latency remains9216 split ON and8192 split OFF. Gain-mode changes now leave all
prediction call/fallback totals and reference fallback-energy shares identical
within a case/schedule, supporting the independently instrumented separation.
Split ON shares for cases0/1/2 are `.000001022/.000000135/.000000387`; these remain
aggregate internal prediction-energy statistics, not rendered error attribution.

For the gain1 compensated outputs:

| Case | Half-period corr | Full-period corr | Double-period corr | Peak Hz | Grid/total | Odd-grid/total | Envelope proxy error |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
|0|-.475229055|-.331621304|-.625613989|662.853564436|.040596729|.000004861|+10.475594073%|
|1|-.007705096|-.127238644|.176616267|580.624973962|.130549083|.013841788|-3.229171006%|
|2|-.493078688|-.230338874|-.714735443|1908.964190773|.011517941|.000015997|+6.173395084%|

Envelope-proxy proximity still coexists with severe off-grid energy. Exact
reference independence is insufficient to ensure the amplitude-reshaped STFT
output is periodic; amplification/reweighting of reference spectral components
is a remaining synthesis concern, not proof of a particular causal mechanism.
No further estimator, noise-floor or gain patch is justified as qualified by this
negative result.

Independent controls remain unchanged:

| Control | Half-period corr | Full-period corr | Double-period corr | Peak Hz | Grid/total | Odd-grid/total |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
|ideal35|-.000000656|.999425598|.999936188|595.000000622|1.000000000|.500000327|
|unchanged70|.999744762|.999425659|.999936187|629.999998540|.999999981|.000000000|
|evenOnly35|.999744758|.999425651|.999936186|629.999998540|1.000000000|.000000000|

The unchanged70 control still passes target35Hz period correlation; retain the
sine/global-frequency checks and octave-ambiguity interpretation. No universal
odd-energy threshold was introduced.

## Commands, source integrity and temporary evidence

Executed locally with GCC14.2.0, C++20/O2, CMake3.31.6, NumPy2.3.5 and
clang-format19.1.7 on little-endian IEEE float32 Linux. Strict warning compilation,
formatter `--dry-run --Werror`, configure-time guards and `git diff --check` passed.
Initial invocation errors (missing new qualification directory; external generator
assuming its own checkout contained experimental source) were corrected before
successful runs. They are not counted as numerical test failures or passes.

From this isolated worktree, with the already existing exact dependency pins:

```sh
source tools/cloud-env.sh
cmake -S . -B /tmp/phase-baseline-build -DCMAKE_BUILD_TYPE=Release \
  -DDARKVOICE_BUILD_SCULPTOR_CORE=ON \
  -DFETCHCONTENT_SOURCE_DIR_DARKVOICE_STRETCH=/workspace/dependencies/signalsmith-stretch \
  -DFETCHCONTENT_SOURCE_DIR_DARKVOICE_LINEAR=/workspace/dependencies/signalsmith-linear
cmake -S . -B /tmp/phase-decoupling-build -DCMAKE_BUILD_TYPE=Release \
  -DDARKVOICE_BUILD_SCULPTOR_CORE=ON -DDARKVOICE_EXPERIMENT_PHASE_DECOUPLING=ON \
  -DFETCHCONTENT_SOURCE_DIR_DARKVOICE_STRETCH=/workspace/dependencies/signalsmith-stretch \
  -DFETCHCONTENT_SOURCE_DIR_DARKVOICE_LINEAR=/workspace/dependencies/signalsmith-linear
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror \
  -I/tmp/phase-decoupling-build/darkvoice-stretch-private \
  tools/qualification/phase_decoupling_probe.cpp -o /tmp/phase-decoupling-probe
/tmp/phase-decoupling-probe > /tmp/phase-decoupling-invariant.log
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  -I/tmp/phase-decoupling-build/darkvoice-stretch-private \
  src/PitchFormantProcessor.cpp \
  /workspace/worktrees/pitch-coupling-integration/tools/qualification/pitch_coupling_probe.cpp \
  -o /tmp/phase-coupling-probe
/tmp/phase-coupling-probe > /tmp/phase-coupling.log
python3 -B /workspace/worktrees/pitch-coupling-integration/tools/qualification/pitch_coupling_analyse.py \
  > /tmp/phase-coupling-analysis.csv
```

The matrix uses the same external generator with exactly two temporary adapter
changes: expected input-header fingerprint becomes the ON hash above, and its
`repo` source location becomes this experimental worktree. No fixture/metric/gain
code is changed. The generator's original fingerprint cannot accept an altered
header, so an adapter is necessary and is explicitly bounded here:

```sh
python3 -B - <<'PY'
from pathlib import Path
p = Path('/workspace/worktrees/pitch-coupling-integration/tools/qualification/pitch_coupling_analyse.py')
s = p.read_text().replace(
    '7003842b8347ba10710f280c33dba494cd3466a54b240cef76cfbbe32a9a93b8',
    '86bcf041a086f2dd7c1c8a94e91a16c522416bd6c62de003524dfe840d4184ae')
s = s.replace('repo = Path(__file__).resolve().parents[2]', 'repo = Path.cwd()')
namespace = {'__file__': str(p), '__name__': 'phase_prepare'}
exec(compile(s, str(p), 'exec'), namespace)
namespace['prepare_private'](Path('/tmp/phase-decoupling-build/darkvoice-stretch-private'),
                             Path('/tmp/phase-matrix-private'))
PY
for split in 1 0; do
  g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror \
    -DDARKVOICE_COUPLING_INSTRUMENTED -DDARKVOICE_COUPLING_SPLIT=$split \
    -Iinclude -I/tmp/phase-matrix-private \
    /tmp/phase-matrix-private/PitchFormantProcessor.cpp \
    /workspace/worktrees/pitch-coupling-integration/tools/qualification/pitch_coupling_probe.cpp \
    -o /tmp/phase-matrix-s$split
  /tmp/phase-matrix-s$split --matrix > /tmp/phase-matrix-s$split.log
done
python3 -B /workspace/worktrees/pitch-coupling-integration/tools/qualification/pitch_coupling_analyse.py \
  --matrix > /tmp/phase-matrix-analysis.csv
/workspace/.quality-tools/clang_format/data/bin/clang-format --dry-run --Werror \
  tools/qualification/phase_decoupling_probe.cpp
git diff --check
```

The actual42 renders used the preceding ON-header fingerprint
`c2b39d8c97667554cd8b4cfa2c84d308bae645de2bcde2c44c04c5a6040f02d6`.
The final hash differs solely because the developer-only snapshot additionally
records `smoothEnergyState`; this is preprocessed out of all42 rendering builds.
The final expanded invariant was executed with the final header. The original
matrix-instrumented header hash was
`7e7cfbb10768881f0fd16a8320c6c76573aafee75c5c1ed6caf005b210e5fe30`;
the split wrapper hash remains
`3b385d1fa70fb16812c0aae1d104e5140ad94418a5fbb3f7b02b68028370586e`.

Temporary logs: `/tmp/phase-decoupling-invariant.log`, `/tmp/phase-coupling.log`,
`/tmp/phase-coupling-analysis.csv`, `/tmp/phase-matrix-s{0,1}.log`,
`/tmp/phase-matrix-analysis.csv`, `/tmp/phase-baseline.log`, `/tmp/phase-reset.log`,
`/tmp/phase-contaminated.log`, `/tmp/phase-anchor-negative.log`.
Raw ordinary/independent-control files are preserved under
`/tmp/phase-decoupling-results/{phase,baseline,reset}`; matrix files retain the
external probe's `/tmp/pitch-matrix-s*-c*-m*.raw` names. Temporary reset/control
headers, generated wrappers, adapter scripts and measurements are not committed.

Final source fingerprints remained unchanged: wrapper
`d5281af605ba5cd7ecf466404bdfe838b571ca84cddbcc551bdb493857c86055`,
pinned Stretch header
`1188667959ac19dd40c0a6abbce694e44705615ec4f0e8db8af0f1cfb4c5dea7`,
pinned Linear STFT
`7422ff5ddc2a017dc0587dcce99ce5ba30a2414bc2996081f6b00d56905d9b8c`.
The original149 checks, offline/envelope/sine/global-frequency/automation/
length/flush/allocation and broader configuration gates are retained, **not run**
for this failed candidate. Manual microphone/listening/native-host qualification
remains deferred. The backend and existing PR19 remain blocked.
