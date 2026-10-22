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

### Factory catalogue increment (schema 6)

Expand six existing factory sounds to fourteen using existing sound parameters.
Preserve existing factory IDs and values, user files and favorites. New entries:
Dark Wave, Deep Narrator, Industrial Whisper, Grunge Baritone, Acoustic Baritone,
Dark Melodic, Industrial Power, Natural Dark Baritone. No schema, dependency,
latency or callback changes. Six correction/choir/octave/double-track presets
remain pending their actual DSP. Acceptance: unique validated full snapshots,
persistence/migration regressions, and finite non-silent product rendering at all
four supported rates in mono/stereo. Names describe styles, not qualified naturalness.
