# Pitch/formant backend audit

Read-only dedicated-agent audit, 2026-10-21. Candidate qualification only: no
backend is integrated or installed as a DarkVoice product dependency. Requirements
remain in PRODUCT_SPECIFICATION.md; proposed integration in PITCH_FORMANT_PLAN.md.
No Actions, native macOS, DAW or listening tests were performed.

## Inspected sources and licenses

- Signalsmith Stretch a670068d9aeb64913331d5cc29337b19a457a7df, internal version
  1.3.2, https://github.com/Signalsmith-Audio/signalsmith-stretch/tree/a670068d9aeb64913331d5cc29337b19a457a7df .
  LICENSE.txt: MIT, copyright2022 Geraint Luff / Signalsmith Audio Ltd.
- Signalsmith Linear de55e6a50ffcf6f8f43f649692d94691c7025151, resolving upstream
  tag0.6.4, https://github.com/Signalsmith-Audio/linear/tree/de55e6a50ffcf6f8f43f649692d94691c7025151 .
  LICENSE.txt: MIT, copyright2025 Signalsmith Audio. Retain both license notices.
- Stretch CMake fetches Linear through a mutable tag. A future integration must
  pin both SHAs independently, providing Linear's target before adding Stretch.
  Pure C++ FFT needs no additional library. Disable SIGNALSMITH_USE_ACCELERATE
  (upstream Apple default ON) and leave IPP/PFFFT/xsimd/CMSIS OFF initially.
  The cmd/util submodule is unnecessary for library use.

## API and scoped real-time inspection

signalsmith-stretch.h provides transpose/formant factor and semitones, optional
pitch compensation, formant base frequency, block/hop/split configuration,
input/output latency, reset/seek/flush. Input/output buffers must be distinct.
Configure FFT/vector storage off callback; use the explicitly seeded constructor
instead of random_device. No locks or I/O were found in inspected default paths.
Processing has resize/assign/peak emplacement within configured capacities in the
probed paths. Do not introduce custom std::function frequency maps on callback.
Setters have no documented per-sample ramp: wrapper automation needs qualification.
Base-frequency README prose says Nyquist-normalized, while example and freqToBin
use Hz/sampleRate; follow inspected source and test this explicitly.

## Executed local probes

Agent used GCC14.2/C++20/O2 against checkouts /tmp/darkvoice-stretch-audit and
/tmp/darkvoice-linear-audit; probes /tmp/darkvoice-stretch-{probe,edge}*.
Temporary artifacts are instance-local, not a reproducible repository test suite.

Four rates, mono/stereo, default/cheaper/manual1024/128 split configurations,
variable blocks1/37/64/512/1024, +/-12-semitone pitch and formant changes, explicit
base and reset produced finite output and zero ordinary new/new[] calls during
those operations. This does not certify every backend path or hard deadline.

At48kHz a neutral impulse peak matched reported input+output latency, magnitude1:

| Configuration | Samples | Time |
| --- | ---: | ---: |
| Default | 5760 | 120ms |
| Cheaper with split computation | 6720 | 140ms |
| Manual1024/128 with split computation | 1152 | 24ms |

Manual-config silence/DC with formant compensation and automatic/explicit base
produced finite output. Smaller manual settings have no bass/formant quality
qualification; default presets must not be labelled low-latency Live.

## Unresolved integration gates

The near-silence shortcut (noiseFloor1e-15, inspected header lines240–271) eventually
copies input directly. Test timing explicitly; impulse latency alone does not
establish fixed delay for every near-silent signal. This is a required design
investigation before host integration, not an approved workaround.

Qualify spectral-envelope preservation independently of F0 accuracy using vowels,
transients, consonants/unvoiced signals, stereo/mono, automation and flushing.
Add sanitizers, allocation probes and CPU checks. Current bypass freezes history
and returns undelayed input; a latent backend requires a versioned aligned dry/mix/
bypass contract and measured host latency. No pitch/formant controls should be
exposed until these gates pass. Local evidence cannot replace listening/native
host qualification. No source modifications or license notices were vendored here.
