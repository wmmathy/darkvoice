# Pitch/formant development handoff

An experimental isolated core now implements the first wrapper/qualification step;
see PITCH_FORMANT_CORE.md. Formant quality still fails, and the host integration
plan below remains blocked. No public pitch/formant controls are exposed.

Read-only architecture-agent proposal for the next Voice Sculptor package.
This is an implementation plan, not an established API or a completed feature.
Authority remains PRODUCT_SPECIFICATION.md. Lead must finalize a versioned contract
and acceptance tolerances before implementation/delegation.

Keep the existing 8192-sample/4096-hop worker YIN analyzer as dry telemetry. Its
window and scheduling latency are unsuitable for driving a real-time shifter.
Build an independent streaming pitch/formant processor before adding controls.

Candidate backend: evaluate Signalsmith Stretch first; verify/pin exact source,
license and transitive licenses, allocation behavior and parameter/latency APIs.
An initial source/license/latency audit is now recorded in PITCH_BACKEND_AUDIT.md;
full wrapper/quality qualification remains outstanding. Rubber Band is a second candidate requiring exact GPL compatibility
and dependency review. No backend has been integrated or installed. A simple
resampling/granular shifter must not be described as formant-preserving.

Proposed interface: prepare(sampleRate, maxFrames, channels, quality),
setParameters(pitchSemitones, formantSemitones, preserveFormants), process, reset,
latencySamples and tailSamples. Pitch range is specification-defined +/-12;
formant range and Live/Studio/Offline algorithms/tolerances need explicit decisions.
Quality changes require off-callback preparation. Preallocate FFT/backend/delay
storage; bound all work; no callback allocations/locks/worker round trips/I/O.
Validate snapshots/blocks atomically; synchronize stereo and parameter ramps.

Latency must be measured for each quality mode. Global dry/wet and bypass need
latency-aligned dry paths; current undelayed bypass/freezing history cannot simply
be retained with a latent shifter. Keep the backend warm or establish a tested
alternative, report host latency and define reset/tail/offline-flush behavior.
Pitch/formant shifting belongs before nonlinear/dynamics stages in Vocal/Studio;
retain dry telemetry before processing. Do not expose vocal-only pitch controls
in the Master shell without an appropriate product contract.

Acceptance must include semitone accuracy across rates/layouts/partitions;
synthetic source/filter vowels proving pitch-only preserves envelope peaks and
formant-only preserves F0; unvoiced/consonant/transient/DC/silence tests; aligned
neutral/mix/bypass; stereo and mono compatibility; automation continuity;
allocation/sanitizer/CPU checks. Establish numeric tolerances before implementation.
Real-vocal listening remains separate; sine tests cannot establish naturalness,
especially at +/-12 semitones. Native validation remains unrun during the user's
Actions pause; no release or deadline claim follows from this proposal.
