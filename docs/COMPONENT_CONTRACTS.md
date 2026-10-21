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
