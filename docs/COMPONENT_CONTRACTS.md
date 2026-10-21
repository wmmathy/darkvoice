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
