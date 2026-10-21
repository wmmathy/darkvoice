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
