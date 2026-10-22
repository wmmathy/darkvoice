# Provisional core contract v0

Authority: docs/PRODUCT_SPECIFICATION.md. This preserved contract describes only
the isolated gain prototype. It is not the full shared product interface baseline.
Do not begin dependent feature implementation until the lead establishes versioned
parameter/state, quality/latency, timeline, worker and host contracts. Supported
product sample rates are 44.1/48/88.2/96 kHz; acceptance of arbitrary positive rates
here does not establish product support.

C++20; namespace `darkvoice`. Header: `include/darkvoice/GainProcessor.hpp`.

`ProcessSpec { double sampleRate; size_t maxBlockSize; size_t channels; }`.
`GainProcessor::prepare(ProcessSpec) noexcept -> bool`: accept finite positive
sample rate, positive maximum block size and 1 or 2 channels. Invalid preparation
unprepares the processor. Successful preparation sets unity gain.
`setGain(float) noexcept -> bool`: accepts finite gain in [0, 4]; rejects invalid
values without changing gain. Called only on the processing thread, between blocks.
`process(std::span<float*> channels, size_t frames) noexcept -> bool`: in-place
planar gain; exact prepared channel count; frames <= maximum; zero frames allowed.
Reject unprepared state, wrong channels, oversized blocks, null pointers for nonzero
frames and identical channel pointers, returning false without touching any sample.
The caller guarantees valid writable buffers and no partially overlapping channels.
Finite input is required; nonfinite samples propagate according to IEEE arithmetic.
`reset() noexcept`: restore unity gain, preserve prepared spec.
`latencySamples() const noexcept -> size_t`: zero.

No allocations, blocking, logging, I/O or exceptions in processing/reset/setGain.
The instance has one owner thread; cross-thread parameter transport belongs to the
future host adapter. Gain changes take effect at block boundaries; smoothing is
required before exposing live user controls. No hard real-time deadline is certified.

## Remaining product contract gaps

Gain-only zero latency is not a suite-wide claim. Product interfaces must specify
denormal handling, quality modes, gain staging, bounded cross-thread parameter
transport, state restoration, measured latency/CPU and engine nonfinite-input
fault handling. No production real-time certification follows from the prototype.

# Vertical slice contract v1 — established 2026-10-21

Authoritative shared API: include/darkvoice/AudioEngine.hpp. The lead establishes
this bounded initial DSP/host contract before assignments; it does not define
unimplemented pitch/harmony/AI interfaces. Those packages remain dependent on
future versioned contracts.

AudioEngine::prepare accepts only 44100, 48000, 88200, 96000 Hz, positive maxFrames
and 1/2 channels. It resets filter/envelope state and default parameters; invalid
prepare unprepares. setParameters validates the entire snapshot (finite floats;
input/output [-24,24] dB, HP [20,400] Hz, drive [0,1], threshold [-60,0] dB,
ratio [1,20], mix [0,1]); reject atomically with no target change. Values default
as in the header. Parameters smooth per sample over approximately 10 ms.

Signal path: input gain -> one-pole HP -> optional normalized tanh saturation ->
feed-forward envelope compressor -> dry/wet against original input -> output gain.
Compression uses 10ms attack/100ms release envelope and no lookahead. This is a
functional starter chain, not true-peak mastering. Bypass leaves samples unchanged
and freezes DSP history; meters still measure each block. Zero algorithmic latency
applies to this chain alone. reset clears histories/meters and settles current
parameters to targets; prepare resets defaults. Peak meters are block-local linear
amplitudes (not LUFS), returned on the processing thread only.

Prevalidate all channels/frames before touching samples; reject unprepared,
oversized, wrong-count, null nonempty or identical-channel pointers. Zero frames
allowed. Caller owns valid nonoverlapping buffers. Sanitize nonfinite input to zero
in active processing; bypass preserves original samples, excluding nonfinite values
from meters. No allocation/locks/I/O in prepare, setParameters, reset or process;
flush tiny filter/envelope states and use host denormal guard. One owner thread.

JUCE adapter uses AudioProcessorValueTreeState stable IDs inputDb/outputDb/
highPassHz/drive/thresholdDb/ratio/mix/bypass, reads atomics once per block,
applies validated snapshots, reports latency 0 for this chain only, and persists
state schema version 1. UI uses APVTS attachments; audio never accesses GUI.
Vocal and Master share starter chain but remain distinct products, visibly marked
as development builds. Studio Standalone initially hosts the vocal chain; complete
record/import/mix/master journey remains outstanding. Live/Studio/Offline modes
are future contracts; no nonfunctional mode controls are exposed.

## Starter-chain limitations verified during integration

Compression envelopes are independent per channel; linked stereo/M/S behavior is
not established for mastering. Saturation is not oversampled. These targets are
development shells and must not be presented as the finished Master product.
Preset/state parsing rejects oversized, malformed, unknown-version, duplicated,
nonfinite and out-of-range parameter state before replacement. The GUI smoke target
exercises actual slider/bypass attachments and renders the real editor; it requires
a display session. Product names are taken from each JUCE target.

# M2 contract v2 — EQ, spatial effects and presets

Established before feature delegation. ParameterSchema.hpp is the single parameter
ID/range/default/step/skew list: existing 8 IDs remain stable, followed by 13 new
float IDs. AudioEngine.hpp and ToneEffects.hpp are the shared source interfaces.
ToneEffects is a dependency-free module: low/high shelves (fixed Q), parametric
mid peak, feedback delay (linear interpolated/smoothed time), and damped Schroeder
reverb. Exact ranges/defaults are in the schema. Supported rates/layouts remain
44.1/48/88.2/96 kHz, mono/stereo. maxFrames is now bounded to 65536.

prepare may allocate bounded filter/ring/scratch storage off the audio thread and
returns false/unprepares on invalid input or allocation failure. prepare resets
default targets/history. reset clears histories, settles controls to current
targets, and never allocates. setParameters rejects any invalid snapshot without
changing targets. process/reset/setParameters never allocate/lock/I/O. Reject
invalid blocks before writes/history changes. All controls smooth over about 10ms;
zero EQ gain/delayMix/reverbMix must preserve neutral signal within FP tolerance.
Tail processing must continue on zero-valued input; zero frames do not advance.
Sanitize nonfinite active input and flush denormal history; bypass preserves input
and freezes all engine/effect history. Preallocated wet buffers preserve original
dry samples for the global mix. Peak meters include resulting wet/tail output.

v2 chain: input gain -> HP -> saturation/compression -> EQ -> delay -> reverb ->
global dry/wet -> output gain. This ordering is explicit, not a final vocal chain.
No new algorithmic latency: musical delay/reverb are effects, not host compensation.
Advertise a conservative 40-second maximum tail (delay <=1s, feedback <=0.8).
Saturation remains non-oversampled and compression remains stereo-unlinked.

JUCE state schema v2 contains all 21 parameters. Load v1 exact 8-ID snapshots with
new IDs initialized to defaults; reject unknown versions, missing/duplicate/unknown
IDs, malformed/nonfinite/out-of-range values without changing live parameters.
Preserve locale-independent canonical save/validated numeric load behavior.

PresetBank API will be shared through src/PresetBank.hpp. Presets contain exactly
the 20 non-bypass sound parameters (future pitch/harmony are separate). Applying
one validates the whole candidate first and preserves bypass/pitch/harmony state.
Six initial factory sounds: Dark Synth Baritone, Industrial Bass, Heavy Rock
Baritone, Clean Studio Baritone, Gothic Bass, Cinematic Bass. They operate on actual
current DSP; the remaining required factory sounds await their real processors.
Factories are read-only; save-as creates a user copy. User presets support search,
favorites, rename, import/export, version-1 migration, explicit file-size/range/name
validation and local UUID-based filenames. Never overwrite another user preset on
import/save-as. Updates use atomic file replacement, outside the audio callback.
No project media, profiles, credentials or network services are part of this bank.

PresetBank JSON format v2: object with schemaVersion=2, name and parameters (all
20 known non-bypass IDs, finite numbers only); stored user files also carry UUID
id. v1 uses schemaVersion=1 and exactly the 7 original non-bypass IDs; migrate by
adding v2 defaults. This is separate from APVTS state schema. Files <=1MiB, names
trimmed/nonempty <=80 characters without controls, direct user files bounded to
512. Import regenerates UUID regardless of external id. Export refuses existing
destination. Favorites are separate local metadata, including factory IDs. refresh
keeps valid entries/factories, reports malformed user files without deleting them.
The shared PresetBank public API is in src/PresetBank.hpp; pointers from find are
invalidated on mutations. Public API changes require lead coordination; private
helpers are implementation-owned.

# Pitch analysis contract v1 (worker/offline foundation, not correction)

PitchDetector.hpp establishes a separate mono dry-signal analyzer. prepare accepts
only the four supported rates, windows 1024..16384, minimumHz>=20,
maximumHz<=4000 and minimum<maximum, with window>=2*ceil(rate/minimum)+2.
Invalid prepare unprepares; allocations occur only during prepare and failures
return false. analyze requires exactly the prepared window and finite input;
invalid/unprepared, RMS-after-DC-removal <1e-5 or insufficient periodicity yields
an unvoiced zero estimate. Valid output: finite frequency in configured range,
fractional MIDI note and confidence [0,1]. No medical/register inference follows.

Bounded time-domain YIN difference/normalized dip with parabolic refinement;
minimum confidence 0.8. DC removal, synthetic/harmonic/noise/range tests required.
No allocations during analyze. It is worker/offline-only, not an audio callback,
live correction, key/chord reference synchronization or user-facing coach feature.
No detection accuracy or voicing certainty is guaranteed for arbitrary recordings.

# Dry pitch telemetry contract v1

DryPitchAnalyzer.hpp: fixed-capacity SPSC ring (32768 available frames), copied
mono average of the unprocessed input, one analysis worker, 8192-sample window and
4096-sample hop. prepare/release run off the callback and stop/join the worker
before resetting shared state. push is audio-thread-only, bounded by ring capacity,
nonblocking, no allocation, and never changes input or audio processing results.
Overrun drops the incoming block, flags discontinuity, and the worker discards
queued/history data rather than estimating across a gap. Results are lock-free
atomic float frequency/confidence, zero for unvoiced/unavailable data. Low-level
nonfinite input is sanitized for analysis. It works independently of bypass.

The processor taps dry input before any DSP. The editor displays measured note,
frequency and confidence at UI refresh rate. This is analysis telemetry, not
correction, target-note coaching or reference synchronization; window/worker
latency does not contribute to the audio path's compensation. prepare/release must
not race processBlock; usual JUCE host lifecycle serialization is required. No
profile recording, file/network/model access or stored voice audio is added.

# Dynamics contract v3 — linked downward expander

Established before implementation. NoiseGate.hpp exposes NoiseGateParameters
{thresholdDb=-50 [-80,0], ratio=1 [1,20], releaseMs=120 [10,1000]}, prepare,
setParameters, reset and process with the same supported rates/layouts/capacity
and atomic invalid-block rejection as ToneEffects. It has no storage allocation,
locks, I/O or algorithmic latency. One audio-thread owner; prepare off callback.

The linked detector takes the maximum absolute sample across channels, with
instant attack and 20ms peak release. Below threshold, gain dB is
(ratio-1)*(detector dB-threshold dB), bounded to [-80,0]. Gain opens over 1ms
and closes using releaseMs; controls smooth over 10ms. Ratio 1 is disabled;
fresh/default state preserves normal finite samples exactly. Active processing
sanitizes nonfinite input and flushes tiny history. No lookahead, hysteresis or
hard-mute promise; it can attenuate initial transients after silence. Linked gain
preserves stereo balance, but does not link the earlier starter compressor.

Engine order: input gain/HP/saturation/compressor -> linked expander -> EQ/delay/
reverb -> global dry/wet/output. Detector therefore sees the compressed wet signal;
reverb/delay tails remain outside the gate. Global bypass freezes gate history.

Append gateThresholdDb, gateRatio, gateReleaseMs to the existing 21 parameters.
Old eight AU hints remain 1, next thirteen remain 2, new three use hint 3.
Project state v3 has 24 IDs; v1 (8 IDs) and v2 (21 IDs) migrate with neutral gate
values. Preset JSON v3 has 23 non-bypass fields; v1 (7) and v2 (20) migrate to
neutral gate defaults, retaining strict count/type/range/duplicate validation.
A separate gate/expander tab exposes all three real automatable controls.
Acceptance: all rates/layouts, identity/attenuation/opening/release/stereo-link,
partition invariance, invalid snapshots/blocks, allocation probe, finite behavior,
engine/preset/state integration and GUI attachment tests. No new dependencies.

# Dynamics contract v4 — split-band de-esser

Established before implementation. DeEsser.hpp: DeEsserParameters
{frequencyHz=6000 [2000,12000], thresholdDb=-24 [-60,0], maxReductionDb=0 [0,18]}.
prepare/setParameters/reset/process follow NoiseGate's rates/layout/capacity,
invalid-input and single-owner rules. No allocation, lock or I/O, no lookahead
or added sample delay. prepare resets defaults; reset settles targets/history.

Per-channel one-pole low-pass L=(1-a)*input+a*L, a=exp(-2*pi*frequency/rate).
High band H=input-L; linked max-absolute high-band detector uses 0.3ms attack,
50ms release. Desired reduction = min(maxReductionDb, max(0, level-threshold)),
smoothed with 1ms attenuation attack/50ms recovery; sub-micro-dB gain settles
to exact unity. Output=input+(gain-1)*H.
Controls smooth 10ms and settle to exact targets. Max reduction 0 disables
attenuation, preserving normal finite samples exactly. Nonfinite active inputs
are sanitized and tiny history flushed. No sibilance classification, brick-wall
split, linear-phase or transparent transient guarantee. Stereo gain is shared;
maximum reduction describes high-band gain, not whole-output attenuation.

Engine order: existing compressor -> gate -> de-esser -> EQ/delay/reverb, then
global dry/wet/output. Reverb tails remain outside dynamics. Append deEssHz,
deEssThresholdDb, deEssReductionDb after the existing 24 IDs, AU hint 4. State v4
has 27 IDs; preset JSON v4 has 26 non-bypass values. v1/v2/v3 migrate strictly
using neutral new defaults. Existing IDs/hints/counts remain stable. Expose three
real controls on a dedicated fifth tab; no new dependencies.

Acceptance: four rates, mono/stereo, neutral and disabled automation, measured
high-frequency reduction and low-frequency preservation, stereo linking,
partition invariance, no callback allocations, finite/invalid-input behavior,
engine/host audio, v1/v2/v3 migration, GUI attachments and local sanitizer builds.
Native CI/AU checks remain unrun while Actions is paused by user instruction.

# Effects contract v5 — chorus and stereo width

Established before implementation. ModulationEffects.hpp exposes parameters
{chorusRateHz=.8 [.1,5], chorusDepthMs=6 [0,12], chorusMix=0 [0,.5],
stereoWidth=1 [0,2]}. Same four rates, mono/stereo, maxFrames1..65536 and one
owner as existing modules. prepare allocates two bounded 30ms rings off callback,
resets defaults/history and unprepares on failure; reset settles targets and clears
rings/phase. Validate entire snapshots/blocks before writes or history changes.
Processing/reset/setParameters have no allocation, lock or I/O.

Each channel mixes original input with an interpolated delay of
15ms+depth*sin(phase), minimum3ms/maximum27ms. Phase advances at rateHz/sampleRate;
stereo right is offset a quarter cycle. Controls smooth 10ms and settle exactly.
No feedback. Delay remains a musical effect, not compensation latency; dry path
is immediate. Zero mix is exact neutral for normal finite samples. Then stereo
width uses M=(L+R)/2, S=(L-R)/2, output M +/- width*S. Width1 explicitly preserves
samples; width0 is mono fold-down, width2 doubles side. Width is a no-op in mono.
Mono sum is preserved within floating-point tolerance absent finite-limit clamping.
Wide settings may increase peaks or reduce mono compatibility; no limiter or
loudness normalization is claimed. Sanitize nonfinite/true-subnormal input and
bound extreme outputs to float range. Ordinary signal controls do not clip.

Order: existing gate/de-esser/EQ/delay/reverb -> chorus -> stereo width -> global
original dry/wet and output gain. Thus width operates on the wet path; global mix
can restore dry stereo. Bypass freezes all histories and leaves original samples.
This adds no algorithmic compensation delay; do not equate it with pitch/formant
processing. Current conservative 40-second spatial tail remains unchanged.

Append four IDs after27: chorusRateHz, chorusDepthMs, chorusMix, stereoWidth,
AU hint5; existing version hints stay unchanged. State v5 has31 IDs, preset v5
has30 non-bypass fields. Versions1/2/3/4 migrate strictly with new neutral defaults.
Sixth editor tab exposes four actual controls. No new dependency.
Acceptance: all rates/layouts, exact neutral defaults, width algebra/mono sum,
measured chorus delay/modulation/rate/depth/mix, automation and partition invariance,
invalid inputs/allocation failures, no callback allocations, engine/plugin audio,
v4 migration, GUI attachments, local Release/sanitizers. Native CI remains unrun
under the user's Actions pause; every commit includes [skip ci].

Preset numeric validation is performed after checked conversion to the float
parameter domain. Exact JSON decimal minima such as0.1/0.2 map to the advertised
float bounds; finite/overflow/type checks and meaningful out-of-range rejection
remain strict. This also corrects the existing mid-Q decimal boundary.

# Sculptor core contract v1 — streaming pitch/formant qualification

Separate PitchFormantProcessor module/library, not yet connected to AudioEngine,
plugin controls or saved presets. Existing products retain their latency/bypass
behavior. Header owns a PImpl; prepare/reset/setParameters/process single-owner.
Prepare accepts four rates, mono/stereo, maxFrames1..65536 and quality enum
Live/Studio/Offline. This optional module defaults OFF pending broader quality and host qualification.
These are candidate DSP profiles, not listening-qualified host
modes: FFT blocks1024/4096/8192 at44.1/48k, doubled at88.2/96k, hop=block/8,
split computation. Query measured backend input+output latency; do not promise
low-latency monitoring. Quality changes require off-callback preparation.

Parameters: pitchSemitones[-12,12]=0, formantSemitones[-12,12]=0,
preserveFormants=true, formantBaseHz=0 (auto) or40..1000. Reject any invalid
snapshot without changing target. At fixed64-sample boundaries, smooth semitones
and base over10ms; base auto/manual and preserve switches occur at boundaries.
Process prevalidates all buffers/layout/capacity before samples/history. Nonfinite
and true-subnormal input become0; FFT input clamps to +/-8 for numerical safety.
Neutral delayed dry retains all normal finite values. Backend output sanitizes
nonfinite values and bounds float range. Numerical safety is not a peak limiter.

Use explicitly seeded Signalsmith Stretch/Linear, pure C++ FFT, both MIT and exact
pinned sources/licenses. A fingerprint-guarded configure-time private-header copy
disables the near-silence direct-copy condition; preserve noiseFloor elsewhere
for spectral stability. The estimator revision below further replaces envelope
smoothing while retaining the guarded source-copy strategy. Originals remain untouched. Quiet
active audio may still be attenuated by the backend noise floor; no transparency
claim follows. Neutral output uses an exact latency-aligned dry ring while the
backend stays warm. Crossfade between dry and processed paths over20ms; settle
controls/blend to exact endpoints. Invalid prepare unprepares; failure is caught.
Prepare owns allocation; process/setParameters/reset must not allocate/lock/I/O.

Reset retains targets, settles them and clears backend/rings/cursors. process may
be called with zero input to drain; tailSamples reports latency+FFT-block as a
conservative bounded drain length, not a musical sustain guarantee. No streaming
worker, recording, inference or file access. Existing dry pitch telemetry remains
independent. Host dry/wet and bypass alignment must be implemented/tested before
future host integration; no inactive public controls will be added in this stage.

Acceptance: profiles/rates/layouts, exact neutral impulse delay and tiny samples,
near-silence timing with the patch, pitch accuracy within20cents on stable synthetic
bass/baritone tones, independent F0/formant-envelope fixtures (initial synthetic
formant peaks within20% for preserved pitch and within20% of intended shift),
partitions/reset/drain, finite inputs/automation, ordinary callback allocation
probes, Release/ASan/UBSan, existing product regression. Tolerances qualify only
these fixtures, not arbitrary voices/transients/naturalness. Listening/native CI
remain unrun while Actions is paused. Every commit uses [skip ci].

Qualification is a separate retained, currently failing experimental CTest suite;
it is not marked expected-failure or skipped when the experimental core is enabled.
Streaming-safety passes do not establish passing formant or listening quality.
Stereo equality is numerical (relative RMS error<1e-4), not bit identity, because
backend channel phase-lock arithmetic differs by floating-point rounding.

## Sculptor performance probe v1

Developer-only `darkvoice_sculptor_bench`, built only with the experimental core.
It does not link into products or register a timing-sensitive CTest gate. No new
runtime dependency. A deterministic stereo signal alternates harmonic, noise and
transient content and parameter targets. Preallocate signal/timing arrays and warm
one second before timing three seconds of processing, including setters, across
four rates, three candidate profiles and 64/512-frame host blocks. Signal creation,
prepare, sorting and stdout reporting remain outside timed processing. Report JSON
schema1 with compiler/build metadata, callback count, audio duration, algorithmic
latency, total/median/p95/p99/max callback wall time, processing/audio ratio and
observed callback-budget overruns. Percentiles use nearest rank. Count nonfinite
output as an error; fail prepare/process/setter errors. Never interpret throughput
or absence of observed overruns as a hard real-time, CPU utilization, listening or
native host qualification. Shared-machine scheduler noise remains in wall time.

### Sculptor envelope estimator revision1 (experimental)

Replace backend morphological envelope dilation/erosion with a symmetric
linear-energy mean across one fundamental spacing (radius ceil(F0-bins/2), clamped
1..bands-1). F0 retains Hz/sampleRate semantics; no fixture-specific centers or
retuning. Sum stereo energy before smoothing, then reuse the existing envelope
ratio and frequency mapping. Preallocate source scratch at configure. Use a
compensated double sliding sum, rebased once per window width to control drift;
work stays O(bands), storage O(bands). Normalize edge windows by actual bin count.
Keep the independent near-silence patch and source/final fingerprints. Preserve
all original qualification gates; expand source/filter fixtures across F0/rates
and shifts with harmonic-grid resolution recorded explicitly. Passing synthetic
fixtures permits further experiments only; defaults/products remain unchanged
pending listening, transients/unvoiced, CPU and aligned-host qualification.

## Latency-aligned mix contract v1

Independent `LatencyAlignedMix` DSP utility; no product routing or latency change
until explicit host integration. Lead owns include/src/tests and CMake wiring.
prepare(rate,maxFrames,channels,latencySamples) accepts the four standard rates,
1/2channels,1..65536maxFrames and0..65536delay samples, allocates/clears dry rings.
Latency changes require off-callback prepare. Invalid/failed prepare unprepares.
setParameters({mix,bypass}) atomically rejects nonfinite/out-of-[0,1]mix; default
mix1,bypassfalse. Smooth effective wet fraction (bypass ?0:mix) over10ms, snap
within1e-6 to exact endpoints. reset retains/settles targets and clears dry history.

process(originalDry,processedWet,frames) validates shape/capacity/null/direct
cross-channel aliases before state/output. Inputs must have at least frames
samples, with no partial overlap. Same-channel dry/wet alias is permitted; capture
all channel inputs before writing outputs. Wet is replaced by mixed output;
originalDry is not modified unless it aliases wet. Delay dry by exactly the
configured samples, including during bypass/mix0. Caller must keep its backend
warm and report the matching host latency. Utility neither runs nor compensates
an incorrectly aligned wet backend. Backend tails remain caller-owned.

Prepare allocates; setters/process/reset perform no allocation, locks or I/O.
Sanitize nonfinite/subnormal audio to0, retain normal tiny dry values and clamp
mixed overflow to float range. Exact wet/dry endpoints avoid cancellation and
unnecessary rounding. Acceptance: impulse/neutral timing including zero delay,
all rates/layouts, known dry/wet blend, settled bypass, history kept while bypassed,
partition invariance, invalid warmed-state rejection, hostile audio, allocation
failure and callback probes, Release/sanitizers. Host and listening remain separate.
