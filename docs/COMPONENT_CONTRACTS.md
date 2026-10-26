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

# Flanger effects contract v6 — independent modulation stage

Shared `Flanger` core: prepare(rate,maxFrames,channels), valid(parameters),
setParameters, process(in-place planar spans), reset. Four supported rates,
1/2channels, maxFrames1..65536; invalid/failed prepare unprepares. Snapshots are
atomic: flangerRateHz[.05,5]=.3, flangerDepthMs[0,4]=2,
flangerFeedback[-.85,.85]=0, flangerMix[0,1]=0. All values finite. Single owner.
Preallocate per-channel10ms rings off callback. Process rejects bad layout, frame
capacity, null/direct channel aliases before samples/history. No partial overlap.

Per-sample10ms smoothing with endpoint snapping, shared sinusoidal LFO phase0 on
reset and equal phase for both channels; base delay4.5ms plus depth*sin(phase),
range0.5..8.5ms, linear interpolation. Read delayed value before writing
input+feedback*delayed to the ring. Output=(1-mix)*input+mix*delayed, exact dry at
mix0; ring/LFO stay warm while disabled. Reset settles targets and clears rings.
Feedback magnitude<1; clamp only float-range numerical overflow, sanitize
nonfinite/true subnormal audio, preserve tiny normal dry samples. No callback
allocation/locks/I/O, bounded per-sample work. Musical delay adds no host latency
compensation; this is not pitch/formant shifting or a limiter.

Engine routing after chorus/width; existing conservative40second tail covers the
bounded feedback decay. Append four IDs after current31 with AU version hint6;
existing IDs/hints unchanged. State v6 has35fields, preset v6 has34 non-bypass
fields. Strict v1..v5 migration resets new fields to neutral defaults. Add seventh
editor tab with four actual controls; default flanger off in existing factory
presets. No new dependency. Separate feature branch stacked on chorus/width PR #17;
Sculptor remains isolated in PR #19 and is not a dependency.

Acceptance: all rates/layouts, exact neutral/tiny samples, measured4.5ms impulse,
feedback echo timing/geometric signed amplitudes, modulation excursions, stable
feedback decay, stereo equality, partition/automation/reset, invalid warmed state,
allocation-failure/callback probes, host actual-audio controls, v5 migration/state/
preset roundtrip, GUI attachments, Release/sanitizers. Native outcome recorded
from actual Actions only; actual DAW/listening remains unrun.

Flanger signed feedback host range uses symmetric normalisation centered exactly
at0 (normalised0.5), and step snapping anchored at0 rather than the negative bound.
Retain interval metadata and [-.85,.85]bounds. This prevents fused-multiply-add
roundoff from turning migrated neutral feedback into a nonzero value. Other
parameter ranges/automation IDs and all legacy migrations are unchanged.

### Vocal sample-peak limiter core v1

Independent module `VocalLimiter`, initially outside product routing. Parameters:
ceilingDb [-24,0], default -1; releaseMs [5,1000], default100. Always active module;
future host integration must provide explicit default-off enable and schema migration.
API matches NoiseGate: valid, prepare, setParameters, process, reset. Supported
44.1/48/88.2/96kHz, mono/stereo, maxFrames1..65536. Prepare invalidates lifecycle
on failure; invalid parameter snapshots preserve previous targets. Invalid buffers
return false without writes/state advance; zero frames allowed after prepare.

Zero latency; instantaneous linked gain reduction to bound sample peaks, exponential
release toward unity. Both stereo channels receive identical gain. This is NOT
oversampled/true-peak limiting, and fast attenuation may distort transients.
Ceiling reductions must apply immediately, including during release; no makeup.
Finite float-range input handled in double precision; nonfinite/true subnormal input
sanitized to zero. No callback allocation/locks/I/O. Exact dry samples below ceiling
at unity gain, reset clears gain reduction. Acceptance: all rates/layouts, impulses,
sustained peaks, release, stereo ratio, partition equivalence, parameter transitions,
invalid lifecycle/buffers/values, extremes/tiny data and allocation probes; Release
and ASan/UBSan. Host controls/state/UI are a subsequent integration increment.
### Factory catalogue increment (schema 6)

Expand six existing factory sounds to fourteen using existing sound parameters.
Preserve existing factory IDs and values, user files and favorites. New entries:
Dark Wave, Deep Narrator, Industrial Whisper, Grunge Baritone, Acoustic Baritone,
Dark Melodic, Industrial Power, Natural Dark Baritone. No schema, dependency,
latency or callback changes. Six correction/choir/octave/double-track presets
remain pending their actual DSP. Acceptance: unique validated full snapshots,
persistence/migration regressions, and finite non-silent product rendering at all
four supported rates in mono/stereo. Names describe styles, not qualified naturalness.

### Vocal limiter host integration v7

Route VocalLimiter after final dry/wet blend and output gain, before output meters.
Global bypass remains exact and bypasses limiting. Append three parameters after
all35 existing IDs: limiterEnabled boolean default0; limiterCeilingDb [-24,0]
default-1 step.1; limiterReleaseMs [5,1000] default100 step1. AU hint7, state/preset
schema7 with38 total/37 non-bypass fields. Strict versions1..6 migrate enabled=0,
ceiling=-1,release=100; preserve prior settings/IDs/hints. Existing14factories remain
limiter-off. A limiter applies to the final output even at dry/wet0, when enabled.
Enable changes act at block boundary; reset reduction on enabled transition to
avoid stale attenuation. Ceiling changes protect the next sample; release remains
continuous otherwise. No added latency. Not true-peak or oversampled.

Editor adds an eighth Limiter tab with three real controls (enable exposed as
0/1 slider consistent with attachment infrastructure and clearly labelled).
Acceptance: core remains13suites, product limiter audio ceiling after output gain
and dry/wet, linked stereo/release/disable/bypass/neutral defaults; exact v6
migration and v7 nondefault state/preset roundtrips, invalid snapshots atomic,
all rates/layouts/variable buffers and callback allocation safety; GUI37 controls
and all attachments/eighttabs. Native CI/auval and independent review before merge.

### Unsigned developer artifact contract v1

Manual workflow only, never a GitHub release, signing or notarization. Build/test
exact selected repository HEAD with immutable JUCE8.0.6 source. Package ready
Vocal/Master VST3, Studio standalone and macOS AU bundles, not static intermediates.
Artifact includes exact project source plus full pinned JUCE source (bundled SDKs
and upstream notices retained), project license/dependency policy, build SHA,
platform/unsigned/unqualified status, checksums and build instructions using the
bundled JUCE tree. No experimental pitch backend enters default products.

Packaging script accepts explicit build/JUCE/output paths, verifies source pin,
rejects dirty tracked source/untracked dependency contamination, archives tracked
source (never .git or environment files), preserves bundle-relative symlinks and
rejects unsafe external/absolute symlinks. Fail missing products/source/provenance;
output must not overwrite source or user files. No network/secret access in script.
Tests: real temp Git fixtures and tar/source/provenance integrity, dirty/wrongpin/
missing/unsafe-link failure without partial deliverables. Full local real packaging
or actual manual workflow run required before claiming developer artifacts exist.
Final redistribution legal/host/listening/signing gates remain separate.

Standalone microphone declaration: Studio alone enables JUCE microphone permission
metadata with local monitoring/analysis purpose text. macOS packaging must validate
its generated Info.plist has a nonempty NSMicrophoneUsageDescription; plugin hosts
own their microphone permission. Runtime granting and actual hardware tests remain
user/OS actions, never inferred from metadata or auval success.

### Stereo musical delay core v1 / host schema8

New independent StereoDelay module; existing ToneEffects classic delay/reverb and
all38 old parameter IDs/hints remain unchanged. Insert after flanger, before final
mix/output gain/limiter. Defaults neutral mix0. Three modes: stereo repeats,
single echo (use short manual times for slapback), ping-pong crossfeedback. Mono
ping-pong falls back to ordinary repeats; no fabricated stereo output from mono.
Time20..2000ms default100; feedback0..0.8 default0.25; mix0..0.5 default0. Manual
syncBPM20..300 default120; divisions in beats [0.25,1/3,0.5,2/3,0.75,1] default
quarter(index5). Sync defaultoff; optional host-tempo selection defaultoff uses
valid host BPM20..300 else manualBPM. Sync defines interval, not transport phase.
Maximum effective delay3seconds. No dependency or algorithmic host latency.

DSP header/source StereoDelay.hpp/.cpp: parameters timeMs/feedback/mix/bpm,
mode0..2, syncfalse, division0..5; valid/prepare/setParameters/process/reset API
matching other effects. Exact supported4rates, mono/stereo, maxFrames1..65536.
Prepare allocates bounded3second rings; invalid prepare unprepares. Validate full
snapshots and buffers before replacing targets/writing/history. No callback
allocation/locks/I/O. Interpolated delay with10ms per-sample time/mix/feedback and
mode-route smoothing; reset settles controls and clears histories. Read both
channel taps before feedback writes; crossfeedback convex and <=0.8, single mode
settles feedback0. Mix0 preserves exact finite normal dry samples. Sanitize NaNs,
infinities and true subnormals; prevent float overflow. Zero frames do not advance.

Append8 IDs/AU hint8: stereoDelayMode[0,2]/0/step1;
stereoDelayMs[20,2000]/100/1; stereoDelayFeedback[0,.8]/.25/.001;
stereoDelayMix[0,.5]/0/.001; stereoDelaySync Boolean0;
stereoDelayBpm[20,300]/120/.1; stereoDelayDivision[0,5]/5/1;
stereoDelayHostTempo Boolean0. State46fields/presets45(non-bypass), strictv1..7
migration to defaults; factories remain neutral. Ninth Stereo Delay tab8actual
controls; meaningful mode/division text, host-tempo availability/manual fallback
status. Keep bypass independent. Tail reporting conservatively extends to240seconds
when this delay is audible (3s repeats at.8feedback), otherwise retain40seconds.

Acceptance: analytic impulses/timing/single-vs-repeat/signed stereo alternation,
beat division/BPM/maxdelay, mono fallback, partition/reset/automation, allrates/layouts,
invalid warmed state, extremes/tiny audio and callback allocation probes. Host
actual audio, fake playhead tempo/fallback, schema7 migration and schema8 Boolean
roundtrip/rejection, GUI45controls/ninetabs, Release/sanitizers/nativeCI/review.
Listening/actual host tests explicitly deferred until combined feature completion.

## Fractional ring-wrap safety correction — 2026-10-23

A public ToneEffects sanitizer probe confirms an out-of-bounds read at48kHz,
reverbSize0.03125 and reverbMix0.5 after891samples. A nearly integral delay of
891.00000000000011 can wrap a negative epsilon to the ring size. Correct ring
reads by splitting integer delay/fraction before integer-index wrapping, keeping
interpolation and fractional phase mathematically equivalent. Apply to the same
unsafe Flanger read pattern with a public regression if reproducible. No API,
parameter/state, signal routing, latency or dependency changes. Do not weaken
existing effect/timing/partition tolerances. Acceptance: public near-integer
impulse at four rates/mono/stereo, finite/causal bounded output, reset/partition,
all existing core/product tests, ASan/UBSan, independent QA and native CI.
DSP agent owns onlysrc/ToneEffects.cpp,src/Flanger.cpp,tests/effects_tests.cpp and
tests/flanger_tests.cpp in isolated fractional-ring-wrap worktree. Lead owns docs,
integration and reviewed normal merge. Manual qualification remains deferred.

## Doubler v1 / schema9 — 2026-10-23

Separate dependency-free Doubler.hpp/.cpp provides simulated automatic double
tracking, not harmony/pitch correction or another recorded performance. Lifecycle
valid/prepare/setParameters/process/reset matches StereoDelay; four rates, one/two
channels and1..65536maxFrames. Parameters aggregate order: separationMs20(default,
10..40), variationMs2(0..6), rateHz0.4(0.1..2), mix0(0..0.5). Validate snapshots
and buffers atomically, preallocate at most60ms+2samples per channel, no callback
allocation/locking/I/O; invalid prepare unprepares.

Read two fractional taps at separation+variation*wanderA and separation+11ms+
variation*wanderB, using integer/fraction-before-wrap interpolation. Two independent
seeded uint32 pseudorandom trajectories bounded[-1,1], smoothly interpolated between
knots with cubic3t²-2t³. A runs at smoothed rate, B at1.173times that rate; fixed
reset seeds and states for reproducibility. Smooth fourcontrols10ms per sample,
settle targets/reset histories and generators on reset. No feedback. Mono wet=
0.5A+0.5B; stereo Lwet=.75A_L+.25B_L, Rwet=.25A_R+.75B_R. No channel bleed;
dual-mono fold-down matches mono within float rounding. Convex dry/wet mix, exact
normal finite dry at mix0, sanitize nonfinite/true subnormal and protect finite
float extremes. Effect tail at mostceil(.057*rate) samples; no algorithmic host
latency (delayed copies are the effect).

Insert after Flanger and before StereoDelay; final global mix/output/limiter
unchanged. Append fourIDs to schema9/AU hint9: doublerSeparationMs10..40 step.1,
doublerVariationMs0..6 step.1, doublerRateHz.1..2 step.01,doublerMix0...5 step.001.
Defaults20/2/.4/0. Preserve old46IDs/hints and strict versions1..8 migration to
neutral defaults. State50fields/presets49excluding onlybypass; ten editor tabs,
newfourcontrols labeled simulated double tracking. Existing14factories neutral;
no new factory until its actual DSP is qualified. Existing conservative tail
reporting remains sufficient; no newtransport/BPM dependencies.

Acceptance: independent variation0 impulse timing/weights (incl fractionalrates),
causal bounded57ms tail, DC unity, historic-input convex peak bound, variation/rate
measurably change audio, deterministic reset, active/automated partition
invariance, dualmono fold-down/channel isolation, allrates/layouts, invalid warmed
snapshots/buffers, allocation-failure/no callbackalloc, extremes/subnormals, strict
Release/ASan. Host actualaudio, migration/preset/attachments/GUI and nativeCI.
Independent QA and normal rules-respecting merge required. Actual manual host/
listening qualification remains deferred by user; no perceptual certification.

## Double Tracked Rock factory sound — 2026-10-23

With the actual Doubler numerically qualified, add the specification-required
editable Double Tracked Rock factory preset using existing realDSP. Schema9 stays
unchanged; old14IDs/settings remain unchanged. NewIDfactory-double-tracked-rock,
generic nameDouble Tracked Rock; rock is a descriptive genre, not a new API field. Use moderate forward rock tone and
actual doubler mix>0/variation>0, retaining neutral correction/harmony independence.
No actual second-performance or listening-quality claim; simulated ADT. Test15
unique factories, complete valid49field maps, oldidentities/values unchanged,
actual host render difference against same settings with doublerMix0 and normal
persistence/GUI/product coverage. Agent host owns onlysrc/PresetBank.cpp,
tests/preset_tests.cpp,tests/plugin_tests.cpp in isolated double-tracked-rock
worktree. Lead docs/integration/CI, independent QA. No publicrelease or manual
listening gate (deferred byuser).

## SpaceReverb v1 / schema10 — 2026-10-23

A separate dependency-free SpaceReverb provides real distinct Room/Hall/Plate
algorithmic networks, not physical room/plate emulation or sonic qualification.
Legacy ToneEffects reverb unchanged. API valid/prepare/setParameters/process/reset
matches Doubler; rates44.1/48/88.2/96kHz,mono/stereo,maxFrames1..65536. Parameter
aggregate order int model0(0Room,1Hall,2Plate);float decay.5(0..1),toneHz8000
(1000..16000),preDelayMs0(0..100),mix0(0...5). Invalid snapshot/buffers atomic;
invalid prepare unprepares. All allocation duringprepare, no callback allocation/
locks/I/O. Reset clears every model, settles parameters and selects target model.

Fixed integer delay L(ms)=max(1,llround(rate*ms/1000)). Allpass AP(t,.5): read
v atL(t),y=v-.5*x,write x+.5*y. Fixed feedback LP q=a*qold+(1-a)*d,
a=exp(-2pi*cutoff/rate). Read all feedback taps before writing. Allmodels run
continuously independently perchannel, receiving the same predelayedinput. No
channelcrossfeed or artificialstereo claim. g=.25+.70*decay in[.25,.95].

Room4FDN: times[29,37,43,53]ms;feedbackLP10000Hz. H4ij=(-1)^popcount(i&j)/2,
b=[1,1,1,1]/2,c=[1,-1,1,-1]/2. Write w=(1-g)*b*x+g*H4*q,late=c dot d.
EarlyFIR x times[3,5,8,12,17,23]ms weights[.30,.24,.18,.13,.09,.06].
Roomoutput=.60*early+.40*late.

Hall8FDN: times[43,53,61,71,79,89,101,113]ms;feedbackLP6000Hz.
u=AP(7.3,.5)(AP(4.7,.5)(x));H8ij=(-1)^popcount(i&j)/sqrt8,
b=allones/sqrt8,c=[1,-1,1,-1,-1,1,-1,1]/sqrt8.
Write w=(1-g)*b*u+g*H8*q,late=c dot d. EarlyFIR receives x times
[12,19,27,37]ms weights[.40,.30,.20,.10]. Halloutput=.20*early+.80*late.

Plate2tank: u=AP(3.1,.5)(AP(1.7,.5)(x));tanktimes[23.9,31.3]ms read d0,d1;
feedbackLP8000Hz q0,q1; a0=AP(7.7,.5)(q0),a1=AP(11.3,.5)(q1).
Simultaneously write w0=(1-g)*u/sqrt2+g*a1,w1=(1-g)*u/sqrt2-g*a0.
Plateoutput=(d0+d1)/sqrt2. No earlyreflection FIR.

Predelayring ceil(.100*rate)+2: write sanitized currentinput BEFORE reading, so
zero delay returns currentinput and fraction<1 blends current/previous. Correct
integer/fraction-before-wrap reads. Modelweights convexonehot smoothing20ms;
othercontrols10ms persample. Apply outputHFtone LP exp(-2pi*toneHz/rate) AFTER
modelblend, then convexdry/wet. HFtone is not spectraldecay or exactdigital3dB
claim; Decay has no seconds/RT60label. Mix0 exactnormalfinite dry, sanitize
nonfinite/true subnormal and protectfinitefloat extremes. Globalbypassfreezesall.

Stability: normalizedHadamard/signedswap norm1, fixed delays/allpasses/LP l2gain
<=1; automatedfeedbackg<=.95 strictsmallgain. Not instantpeaklimiting. ActiveSpace
reports60s hosttail (240s musicaldelay dominates;Spaceoffretainsold40s). Actual
roundedlength weightedfeedbackbounds at exponent.4/second remain<1. Universal
40s/-90dBclaim was corrected beforeintegration: automatedpredelay canrepeatinput
energy. With zero initialstate/finiteinputenergy, prescribedlinearnetwork bound
includes sqrt(Dmax+2) predelaySchurfactor; conservativewet60s bound<-127dB
relativeinputl2, notexactsilence/inaudibility. Modelweights/tone preserve a
pointwiseexponentialenvelope, notarbitrarytimevaryingfilterl2passivity. Static
48k40staildiagnostic remainsuseful; nospecialdecayseconds/RT60 claim. NewDSP adds noalgorithmichostlatency; predelay is musical.

Insert after StereoDelay before globalmix/output/limiter. Append AUhint10/schema10
IDs spaceModel0..2 step1(default0),spaceDecay0..1 step.001(.5),spaceToneHz
1000..16000 step1(8000),spacePreDelayMs0..100 step.1(0),spaceMix0...5 step.001(0).
Preserve prior50IDs/hints; state55/preset54excludingonlybypass. Strictv1..9neutral
migration; extend exactlexicalsmallintegerguards tospaceModel max2 inJSON/XML
and safeinvalidenum sentinel beforecasts. Eleven readableeditor groups, five
actualcontrols, realmodelnames. Existingfactorymaps neutral forSpaceReverb.

Acceptance: public impulse withmix.5 recoverwet=2output-input; analytically
accounttonepole. FirstrawarrivalRoom L3 coefficient.18,Hall L12 coefficient.08,
Plate L23.9 coefficient(1-g)/8. AllRoom/Hall earlycoefficients beforeFDNarrival
verifiedwithindependenttone recurrence; zero/integer/fractionalpredelay oracle;
allrate/layout fingerprints/diffusion/earlyenergy asstructuralevidence only.
Normalizeddecaycurves0/.5/1 (absoluteenergy mayfall asinputscaled1-g). Published
matrix orthogonality/passivity/source review; long48kmaxdecaytail andactualrounded
weightedfeedbackbounds. Reset,activeautomatedpartition,modelcrossfade,invalid
warmedstate,allocationfailure/no callbackalloc,extremes/subnormals. StrictRelease/
ASan/host/preset/GUI/nativeCI andindependentQA. Listening/DAWmanual testsdeferred.

Agent deesser_review owns ONLYinclude/darkvoice/SpaceReverb.hpp,src/SpaceReverb.cpp,
tests/space_reverb_tests.cpp inisolatedspace-reverb-core. Leadowns sharedcontracts,
CMake/docs/integration; hostassignment follows thisfrozenAPI withisolatedownership.

## Colour core v1 qualification contract — 2026-10-24

Independent dependency-free tape/tube-style saturation and actualparallel wetmix.
Not physical tapehysteresis/circuit emulation. Legacydrive unchanged. COREONLY
qualification first; no hostcontrols/schema/latencybehavior entersproducts until
allnewgates pass andlead establishes preciseenginealignment/bypasscontract.
Planned futureplacementafterSpaceReverb beforeglobalmix/output/limiter processes
effecttails; planned schema11 hint11 adds4fields afterSpace55 (59state/58preset).
Do not change oldzero-latency engine inthis assignment.

Colour.hpp/.cpp lifecyclevalid/prepare/setParameters/process/reset asDoubler.
ColourParameters aggregateorder:int model0(0Tape,1Tube);float drive.25(0..1),
toneHz16000(1000..20000),mix0(0..1). Allfourrates/mono/stereo/maxFrames1..65536;
invalidsnapshot/buffersatomic,invalidprepareunprepares; preparepreallocates,
callbacknoallocation/locking/I/O;resetclearshistories/settlesparameters.
latencySamples() returns56whenprepared,otherwise0. Puremodulemix0 exactly
sanitizednormaldry delayed56samples, withzero startupfill. No hostintegration
claim. Nonfinite/truefloatsubnormal sanitize0;finitefloatextremes remainfinite.

Fixed8xoversampling:3cascaded2xstages,each65tap symmetricBlackmanhalfband.
n0..64,m=n-32,w=.42-.5cos(2pi*n/64)+.08cos(4pi*n/64); hcenter=.5,
hother=sin(pi*m/2)/(pi*m)*w, exactzero fornonzeroevenm. Normalizeallh bysum.
Upsampleinsertzero/gain2,downsamplefilter/gain1 andselectevenphase starting0.
CommonFIRgroupdelay56native;allrate/model/mixindependent. Privateinternalheader
src/ColourOversampling.hpp mayexpose detailhelpers torealunittests; no public
test-onlyprocessingmode. Identityresampler impulse/DC/symmetry/groupdelay56,
perstagepassbanderror<.0002 through.4nativeFs andstopband<-75dB from.6nativeFs.
Thesearefiltergates,notnonlinearaliasclaims. Useboundedpolyphase/halfband
implementationwherehelpful, mathematicallyequivalentcoefficients.

Nativecontrol smoothing10ms drive/tone/mix and20ms styleweightt (targetmodel0/1),
snaptolerance1e-7. d=1+5drive,k=.5*(1-t). Nativepreemphasis: a=exp(-2pi1500/rate),
p=a*p+(1-a)*x,v=x+k*(x-p). Upsamplev,at8xcompute
Tape=tanh(d*v)/tanh(d);Tube=(tanh(d*(v+.12))-tanh(.12*d))/tanh(d);
blend(1-t)*Tape+t*Tube,downsample. Nativecomplementarydeemphasis:
y=(s-a*sprev+(1+k)*a*yprev)/(1+k*a). Thenfixed10HzDCblock
z=y-yprevDC+r*zprev,r=exp(-2pi10/rate);thenHFtoneonepoleexp(-2pi*toneHz/rate).
Wetblendwith exact56sampledelayeddry, mix0exactpath. AllDSPstates continuous
initially; no uncontractedneutralfastpath/reset policy. Allprecisioninternal
double;publicfloatbound/sanitization. Styleinterpolation isdefinedalgorithm,
not claimcircuitinterpolation. Tapehasfiltermemory/frequencydependentcompression,
Tubeasymmetry/evenharmonics/DCrejection,parallelmix isrealprocessedblend.

Predeclared nonlinearqualification: compare settledcoherent tone/twotonerenders
against independent32xreference (5stages,samepublishedkernel, commonlatency62;
align6). Nativepre/postfilters identicalsemanticstoisolateantialiasquality.
Relativeinbandresidual RMS < -55dB/inputRMS and >=20dB improvement over1x at
maxdrive and representativeamplitudes. Choose/recordboundedfixturematrix before
runs, include highbands/twotones andbothstyles; do not tune thresholds toresult.
Ifgatefails investigate/report,donotrelax or exposecontrols. Referencealgorithm
mustindependentlycomputezero-insert/convolution,notcallproductionresampler.

Othergates: tangibleTapeleveldependentHFcompression/memory,Tubeevenharmonics
andDCsettling/stylesdifferent;allrates/layoutsreset/partition/automation,
invalidwarmedstate/no callbackalloc/allocationfailure/extremes/subnormals;
exactdelayedneutral and impulsealignment atpartialmixes. StrictRelease/ASan,
independentQA andactualnativeCIbeforequalifiedcoremerge. Measurecosthonestly;
no CPUdeadline/listening/DAWclaim. Manualqualificationdeferred.

Agent space_core_finish owns ONLYinclude/darkvoice/Colour.hpp,src/Colour.cpp,
src/ColourOversampling.hpp,tests/colour_tests.cpp inisolatedcolour-coreworktree.
No shared/CMake/docs/commit/push;leadownsqualification/integration.

## Aligned AudioEngine v2 / Colour host schema11 — 2026-10-24

Dependency: Colour core is qualified and merged at cd33a6a (PR33), after478
checks, independent QA and native40075994215 all7jobs/quality40075994126. This
contract deliberately replaces immediate zero-latency output with fixed56-sample
alignment. Preserve prior55 parameter IDs/hints and all15 factory settings.

Append EngineParameters fields int colourModel=0; float colourDrive=.25f,
colourToneHz=16000,colourMix=0. Append schema11/AU hint11 IDs colourModel[0,1]
step1/default0 (Tape/Tube); colourDrive[0,1] step.001/default.25; colourToneHz
[1000,20000] step1/skew.5/default16000; colourMix[0,1] step.001/default0. Total
59state/58preset fields excluding onlybypass; strict v1..10 migration to neutral
Colour defaults. All15 factories add neutraldefaults. Retain original14factory
fingerprints on original49 fields; separately assert nine Space/Colour defaults.
Exact raw JSON/XML small-integer guards include colourModel max1, before numeric
rounding. Validate raw discrete values before any float-to-int cast.

New engine API: process(span<float*>,frames), processBypassed(same), release()
noexcept, latencySamples() noexcept. Both process methods share one implementation;
effective bypass ONLY target.bypass||forceBypass. Forced host bypass never changes
parameter state. Successful prepare reports56; failed prepare/release reports0.
Reset clears histories/rawqueue/counters and settles parameters, with normal
active startup rather than a synthetic resume. Entire buffer validation precedes
all history writes. Empty calls do not advance queues/counters/smoothing/bypass
state. Supported rates/layout/frame limits unchanged. All storage is prepared;
callback processing/reset-on-resume allocates/locks nothing.

Engine owns one continuously advancing56-sample ORIGINAL RAW alignment queue per
channel. Store uint32 float bits, preserving signedzero/subnormals/Inf/NaNpayload
on actual bypass; initial/reset fill is zero bits. This queue advances on both
normal and bypassed audio. Sanitize processed branches, not raw bypass.
Enabled route: legacywet -> SpaceReverb -> Colour -> aligned globaldry/wet ->
outputgain -> limiter -> outputmeters. Module colourMix0 returns the old wet
chain delayed56, without added filter coloration. Globaldry is the rawqueue's
sanitized delayed input. Here sanitization clears only nonfinite/true-float-
subnormal values, preserving ALL normal finite amplitudes before final gain.
Do not move the old1e-20 final cleanup ahead of possible positive output gain:
9e-22 at+24dB must survive the final threshold. Existing inputgain/HP/compressor/FX processing remains
at input-frame time; finalglobalmix/outputgain automation remains at output-frame
time. Do not simply delay the complete old automated output.

True bypass immediately emits bit-exact delayedraw, skips gain/limiter and freezes
all effect histories/smoothing; only raw alignment and meters advance. Inputpeak
uses current original input; outputpeak uses emitted delayed audio. Ignore
nonfinite values in meters, while preserving their raw output bits.

On actual bypass exit, reset Colour ONLY to accepted target parameters, keeping
legacy effect histories frozen until processing resumes. Resume upstream/Colour
processing immediately. For first56 unbypassed samples replace the PRE-GAIN
global blend with sanitized aligneddry. For next128 samples fade at weights
k/128,k=1..128 from sanitized aligneddry to the aligned globaldry/wet blend.
Then use the full blend. Re-entry interrupts and a later exit restarts reset/
guard/fade. These counters span calls/chunks and ignore empty calls.
CRITICAL: guard is NOT effective bypass. Every actual-unbypassed sample, including
guard/fade, resumes outputgain smoothing and passes through the enabled FINAL
limiter. No unlimited guard exception. Exactraw applies only actual bypass;
processed dry is exact only with neutral gain/limiting and applicable sanitization.
This defined transition avoids stale Colour FIR/IIR history, not a click-free claim.

Processor overrides float processBlockBypassed through the same chunked helper/
engine queue as processBlock. Preserve existing JUCE wrapper bypass-ID behavior;
do NOT add getBypassParameter override in this increment. prepare/release update
JUCE latency56/0 outside callbacks; automation never changes it. releaseResources
calls engine.release. Dry analysis still receives current original input before
alignment in both paths. Host oversized-buffer chunking is retained.

Editor adds four real controls/12th Colour group (Tape/Tube, drive, HF tone,
parallelmix),58soundcontrols plusbypass. Keep900x780 minimum; use fitting horizontal
sidebar buttons (derive height from availablebarheight/count, about38px for12).
Alllabels/controlbounds must fit; report actual56sample latency in meaningful
user-facing status, without pretending physical emulation or full Master features.

Test migration must retain old numeric tolerances and signal intent. Static legacy
fixtures use explicit56shift/startupzero/tailflush or a dedicated STATIC aligned
render helper. Never globally add zero-flush/cropping to streaming/automation
helpers: that changes histories/control timing. Delayed legacy-output oracle is
valid only for static final controls; automated finalmix/gain need explicit
aligned-source/control-envelope oracle. Keep every prior cutoff/gain/dynamics/
preset assertion, shifted measurement windows rather than widened tolerances.

Acceptance: exactraw56 queue incl floatbit edgecases; allrates/layouts/call
partitions/empty/invalid calls; forced/APVTS bypass interleaving without mutation;
Colour-only reset and full56guard/128fade/legacy-frozen-history independent oracle;
first actual-unbypassed delayed input above ceiling with positivegain ALWAYS
limited, including guard. Test normal startup/neutral56 impulse and unchanged
latency through allautomation, release/failure0. Coloured FIR has pre-ringing:56
is common groupdelay, not a claim no activewet response before sample56. Actual
Colour hostaudio, strict migration/roundtrip/tokens, originalfactorypreservation,
58attachments/12readablegroups, callbackallocation, Release/ASan/native/independent
QA. Measure uncontended test/runtime; no gate changes without actual evidence.
Manual host/microphone/listening qualification remains deferred by user.

Engine writer owns ONLY include/darkvoice/AudioEngine.hpp,src/AudioEngine.cpp,
tests/engine_tests.cpp in colour-engine isolated worktree. Host writer owns ONLY
include/darkvoice/ParameterSchema.hpp,src/PluginProcessor.hpp/.cpp,
src/PluginEditor.hpp/.cpp,src/PresetBank.cpp,tests/plugin_tests.cpp,
tests/preset_tests.cpp,tests/gui_smoke.cpp in colour-host isolated worktree.
Lead owns sharedcontract/CMake/docs/integration/runtime/native gates. CoreColour
files stay frozen unless a concrete reviewed core defect requires coordination.

## VoiceSculptor tone core v1 — 2026-10-24

Next independent increment, core only. Pitch/formant quality remains blocked;
this module supplies three explicit tonal operations, not pitch lowering or
anatomical/AI inference. No external dependency. Host integration follows Colour
schema11 and requires its own dependent contract before changing shared files.

Public include/darkvoice/VoiceSculptor.hpp: VoiceSculptorParameters fields float
bodyDb=0,warmthDb=0,presenceDb=0, each finite [-6,6]. VoiceSculptor constructor
noexcept, destructor, deleted copy; bool prepare(double rate,size_t maxFrames,
size_t channels) noexcept; static bool valid(const parameters&) noexcept;
bool setParameters(const parameters&) noexcept; bool process(span<float*>,size_t)
noexcept; void reset() noexcept; size_t latencySamples() const noexcept always0.
Pimpl allocation only prepare; same4rates/mono-stereo/maxFrames1..65536 as Colour.
Failed prepare leaves unprepared. Invalid snapshots/calls change no targets,
history or output; validate all channels before mutation. Empty calls advance
nothing. Reject null/nonempty and duplicate channel pointers. reset clears state
and settles accepted targets. No callback allocation/lock/I/O; independent channels.
Sanitize nonfinite/true float subnormal input to0; preserve normal finite neutral
samples exactly, including signedzero. Double histories cleared for nonfinite/
true double subnormal; finite output clamp float bounds; nonfinite output ->0.
After float conversion clear only true float-subnormal output; preserve signed
zero and normal neutral samples. Active cuts must not emit persistent denormals.

Cascade Body -> Warmth -> Presence, fixed coefficients at prepare. Body/presence
are constant-peak RBJ bandpasses at180Hz/Q.85 and3000Hz/Q.7 respectively: with
w=2*pi*f/rate,alpha=sin(w)/(2Q), normalize coefficients
b0=alpha/(1+alpha),b1=0,b2=-b0,a1=-2cos(w)/(1+alpha),
a2=(1-alpha)/(1+alpha). Track each filter continuously at neutral. Stage output
x+(G-1)*BP(x), G=10^(gainDb/20). Warmth uses complementary bilinear first-order
LP450: K=tan(pi*450/rate),b0=b1=K/(1+K),a1=(K-1)/(K+1),
y=b*x+(a-b)*LP(x), a=10^(warmthDb/40), b=10^(-warmthDb/40).
Positive warmth maximum means+3dB DC/-3dB Nyquist. Store/smooth independent
weights body=(Gbody-1),presence=(Gpresence-1),warmA=a,warmB=b in double with
coefficient1-exp(-1/(.010*rate)), once each sample BEFORE channel processing.
Target weights computed only on accepted setParameters, reset settles exactly.
No epsilon snap; test exact envelope. Only true double-subnormal/nonfinite
smoothing states are cleared after recurrence, matching filter denormal handling.
Neutral weights0/1 produce exact input,
while states keep running. Guard explicit neutral operations if signedzero would
otherwise be lost. No added delay; no time-varying poles. Static whole-cascade
magnitude bounded+15dB, not a time-domain peak/automation bound. Retain final
engine limiter in eventual integration.

Acceptance: independent complex-transfer oracle for all4rates/layouts, isolated
endpoint/intermediate controls and combined extremes; settled sine fitting within
.02dB of analytic response, exact center gains and asymptotic tilt endpoints;
prove controls spectrally distinct. Neutral identity incl floatbit edges, channel
isolation, superposition, reset reproducibility/zero-delay, explicit sample-weight
automation oracle (isolated and combined), irregular partition invariance,
invalid-atomicity/empty/nonfinite/subnormal/finiteextremes, allocationfailures and
callback guard; strict Release/ASan and independent QA. No product controls yet.
Core writer owns only new header/src/voice_sculptor_tests.cpp in isolated worktree;
lead owns CMake/docs/integration. Future planned routing after de-esser and before
ToneEffects, adding only body/warmth/presence controls. Depth/resonance/breathiness/
character remain outstanding; do not silently equate them to these three bands.

## VoiceSculptor tone host v1 / schema12 — 2026-10-24

Dependencies: qualified VoiceSculptor core and Colour aligned-engine/schema11.
Append EngineParameters float sculptBodyDb=0,sculptWarmthDb=0,sculptPresenceDb=0.
AudioEngine validates allthree atomically with VoiceSculptor::valid, prepares/
sets/resets module with existinglegacy effects. Route after DeEsser before
ToneEffects. Actualbypass freezes histories/weights; Colour-only reset onresume
mustnotreset VoiceSculptor. Engine/host latency remains fixed56, release/failure0.
No change to rawalignment/guard/fade/finalgain/limiter or input/outputautomation.
All prior signal/gate/latency/bypass tests retained. New independentlycomposed
routingoracle mustexercise tonecontrols, frozenhistories throughbypass and full
resume, continuousneutralhistory and finalenabledlimiter under combinedboost.
Allrates/layout/partitions, invalidsnapshotatomicity, allocationguard required.

Append schema12/AUhint12 IDs sculptBodyDb,sculptWarmthDb,sculptPresenceDb inthatorder,
range[-6,6],step.1,default0,linear. Total62state/61preset excludingonlyglobalbypass.
Strictv1..11migration zerosnewfields; v12roundtrip; rawfinite/ranges exactexisting
parserguards unchanged. Preserve all59priorIDs/hints/ranges/defaults and15factory
priorvalues; allnewtonefieldsneutralin everyfactory. Original14hashes continue
coveringoriginal49fields, assert all12Space/Colour/tone neutralfields separately.

Add13th VoiceSculptor editorgroup with3real controls BODY180Hz, WARMTH TILT,
PRESENCE3kHz; dB units, clearwarmthtooltip +6span means+3dB low/-3dB high; no pitch/
anatomicalclaim. 61soundattachments plusbypass, fit900x780sidebar13labels (derive
height fromavailablebar/count), preserveColouractualstatus56samples. Teststrict
state/presetmigrations/invalidfloats, actualhosttoneaudio andpairedneutralrouting,
allpriorfactoryvalues,13visiblegroups/controls. Neverexpose pitch/formant knobs.

Enginewriter owns ONLY AudioEngine.hpp/.cpp and tests/engine_tests.cpp. Hostwriter
owns ONLY ParameterSchema.hpp,PluginProcessor.hpp/.cpp,PluginEditor.hpp/.cpp,
PresetBank.cpp,tests/plugin_tests.cpp,preset_tests.cpp,gui_smoke.cpp inisolated
worktrees. Leadowns CMake/docs/integration; qualifiedtonecorefilesremainfrozen.
Release/ASan/product/GUI/independentQA/nativechecks before normalmerge. Existing
measured180secengine/host testbudgets retained; no numericrelaxation. Manualtests
remain deferred. This incrementdoesnotcomplete depth/resonance/breathiness/character.

## Breathiness texture core v1 — 2026-10-24

Interpretation: synthetic center-vocal aspiration texture, independently adjustable;
this is no physiological measurement or claim of naturalness. Core only; no new
product control until separate engine/host integration. Own local DSP, no dependency.

New Breathiness.hpp/.cpp: BreathinessParameters {float amount=0;}; Breathiness
constructor noexcept/destructor/deleted copy; prepare(rate,maxFrames,channels),
static valid(params), setParameters(params), process(span<float*>,frames), reset,
latencySamples() all existing lifecycle signatures/noexcept. Amount finite[0,1].
Four usual rates, mono/stereo,maxFrames1..65536. Failed prepare unprepares; invalid
snapshots/buffers atomic, entire validation before mutation; empty advances nothing.
No callback allocation/lock/I/O. All storage allocated during prepare. Input/output
nonfinite and true float subnormals clear; normal neutral bits and signedzero exact.
Float output clamps finite bounds; clear only emitted float subnormals after cast.

For sanitized input M=mono or .5*double(L)+.5*double(R), maintain exact finite20ms
RMS window N=rate*.020 (882/960/1764/1920). Use a preallocated nonnegative sum tree:
power-of-two leaves>=N, one ring leaf=M*M replaced/sample, recompute ancestors
from child sums. Unused leaves zero. This bounded<=11-level update avoids rolling
subtract cancellation after huge->quiet input and gives exact0 when window empty.
Envelope e=sqrt(root/N). Gate t=clamp((e-10^(-90/20))/(10^(-70/20)-10^(-90/20)),0,1),
g=t*t*(3-2*t). Fixed thresholds are full-scale RMS, not a vocal-health judgment.
Smooth amount once/frame BEFORE excitation using double coefficient
1-exp(-1/(.010*rate)), reset settles target; clear only true double subnormal/
nonfinite state after recurrence, no epsilon snap.

One deterministic shared xorshift32: seed0x4e4f4953; per frame xor<<13,xor>>17,
xor<<5 with uint32 wrap. u=((word>>8)+.5)/8388608.-1 (24-bit midpoint uniform),
ideal midpoint-uniform variance (1-2^-48)/3 for normalization. Finite PRNG
realizations are checked statistically, not asserted IID or exactly uniform.
Advance every valid nonempty frame even at amount0.
Excitation v=.1*smoothedAmount*e*g*u. Filter excitation AFTER this multiplication
with a fixed129tap symmetric Blackman-windowed ideal2k..10k bandpass:
k=n-64, window=.42-.5cos(2pi*n/128)+.08cos(4pi*n/128), ideal(k0)=2*(10000-2000)/rate,
otherwise (sin(2pi*10000*k/rate)-sin(2pi*2000*k/rate))/(pi*k).
Subtract sum of initial taps from center tap for DC correction, then divide taps
by sqrt(variance(u)*sum(tap*tap)), normalizing stationary noise RMS to1. Fixed
coefficient preparation only. Use a preallocated129-double excitation ring and
bounded direct convolution; clear true double subnormal/nonfinite histories.
Add same filtered b to mono or both stereo dry channels. Dry has no added delay;
texture FIR groupdelay64 is generation timing, not host alignment latency. API
latency0, eventual engine remains56. Reset clears tree/rings, resets seed and weight.

Stereo intent: center-driven addition, dry Side preserved within float rounding
when neither output reaches the finite float clamp;
pure antiphase input generates none after finite old-history tail. A silent channel
can receive center texture from the other channel: intentional crossfeed, not
channel isolation. Mono fold compares to processing M within declared rounding in the unclamped
range; saturation can change the stereo algebra.
Initial silence exact; after zero-M input, by N+128 zero frames addition exactly0.
Do not claim instantaneous zero after active audio or a physically recovered breath.
FIR l1<=8.20 at all rates => added peak<=.820*recent peak over N+128 history frames
at amount1. This is not a limiter; eventual final limiter remains necessary.

Acceptance: independent brute-force finite RMS+PRNG vectors+generic convolution
oracle for amount/gate/filter/automation all rates/layouts; irregular partition and
reset exactness. Stationary2s M=.1 amount1 additionRMS .01 within5%; amount.5 half
RMS after settling. Independent coefficient/complex-transfer/denseFFT oracle,
DC<=1e-10, >=99.9% stationary excitation power1..12k, <=.02% below1k, l1<=8.20.
This is declared digital noise color, not an analog source aliasing claim. Full
silence tail bound incl huge->quiet recovery, exact neutral float edges, Side/
antiphase/centerfeed/fold tests, finite extremes/invalidatomicity/allocationfailure/
callbackguards, Release/ASan/independentQA/native. Listening remains deferred.
Writer owns ONLY new include/darkvoice/Breathiness.hpp,src/Breathiness.cpp,
tests/breathiness_tests.cpp; lead owns CMake/docs/integration. No host edits here.

## Breath texture host v1 / schema13 — 2026-10-26

Dependencies: qualified Breathiness core and schema12 tone integration. Append
EngineParameters float sculptBreathiness=0 and validate via Breathiness::valid
before any target change. Prepare/set/reset the module with the legacy chain.
Route DeEsser -> VoiceSculptor -> Breathiness -> ToneEffects -> remaining FX ->
Colour -> aligned global blend -> output gain -> enabled limiter. Preserve fixed56
engine/host latency, raw-bit bypass, input/output automation timing, and existing
resume guard/fade. Actual bypass freezes all Breathiness state/PRNG. Resume resets
ONLY Colour; finite Breathiness history resumes rather than being silently reset.
Tail bounds count processed unbypassed frames, not wall time while bypass freezes.
The new finite texture tail is shorter than existing musical tails; no host tail
reporting change. Keep every prior signal, limiter and lifecycle assertion.

Append schema13/AU hint13 ID sculptBreathiness, finite[0,1], step.001, default0,
linear. Total63 state/62 sound-preset fields excluding only global bypass. Strict
v1..12 migration adds zero; schema13 roundtrip and invalid values atomic. Preserve
all62 prior definitions/IDs/hints and all15 factory maps; the new field is neutral
in every factory. Keep original14 hashes on original49 fields, separately verify
all13 appended Space/Colour/tone/breath fields are neutral. No correction/harmony
preset or parameter implication.

Add one real BREATH TEXTURE amount control to existing VoiceSculptor group (fourth
row), retaining13 fitting groups at900x780 and actual56sample latency. Tooltip
explains center-driven added texture following recent vocal level. No physiological
or naturalness claim.62 sound attachments plus global bypass. APVTS default zero
is exact with the existing unsigned range; existing signed-tone snapping unchanged.
Dry pitch analysis continues on original input before engine processing.

Acceptance: independently composed engine reference through tone -> Breathiness
-> EQ/time effects -> Colour, all rates/mono/stereo, default identity, active amount
and automation, full frozen-history bypass/forced bypass/resume and final limiting.
Invalid snapshot/buffer atomicity, reset, irregular partition and callback allocation
checks. Host paired neutral reference plus deterministic actual texture evidence,
old state/preset migrations and factory values, invalid float rejection, actual
GUI attachment/group/label checks. Preserve unclamped stereo qualifications from
core; do not claim full-chain Side preservation when other effects can alter it.
Release/ASan/product/GUI/independent QA/native checks before normal merge. Manual
host/microphone/listening qualification remains deferred.

Engine writer owns ONLY AudioEngine.hpp/.cpp and tests/engine_tests.cpp. Host
writer owns ONLY ParameterSchema.hpp, PluginProcessor.hpp/.cpp, PluginEditor.hpp/
.cpp, PresetBank.cpp, tests/plugin_tests.cpp, preset_tests.cpp, gui_smoke.cpp in
separate worktrees. Lead owns contracts/CMake/docs/integration; core files frozen.
