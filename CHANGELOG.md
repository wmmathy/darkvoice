# Changelog

## Colour host integration

- Route Colour after Space Reverb and align global dry and actual bypass by 56 samples.
- Resume through a defined dry guard and fade while retaining final gain and enabled limiter protection.
- Append four hint-11 controls, strict schema-11 migrations and a twelfth editor group.
- Preserve prior parameter definitions and factory values; report fixed host latency and clear it on release.
## Voice Sculptor tone core

- Add independent body, warmth tilt and presence processing with fixed filter poles and smooth gain weights.
- Qualify frequency response and automation against independent analytic and state-variable references.
- Keep the core outside application routing and controls until its dependent host integration is validated.

## Oversampled colour core qualification

- Add an independent 8× tape/tube-style colour core with real parallel mix and fixed 56-sample latency.
- Qualify filter response and nonlinear aliasing against an independent 32× reference.
- Keep it outside the application routing and controls until host alignment integration is complete.

## Space reverb

- Add distinct Room, Hall and Plate algorithmic networks with decay, HF tone, predelay and mix.
- Append five hint-10 parameters and strict schema-10 migration; keep all fifteen factories neutral.
- Add an eleventh editor group and report a conservative 60-second active reverb tail.
- Preserve the legacy reverb and musical-delay tail precedence; manual sonic qualification remains deferred.

## Double Tracked Rock factory

- Add the fifteenth required factory sound using the actual simulated doubler and moderate rock tone.
- Preserve the prior fourteen IDs and settings; derive the finite favorites cap from the factory count.
- Retain pending listening qualification and five sounds dependent on correction/harmony DSP.

## Simulated double tracking

- Add two independently wandering delayed copies with mono-compatible stereo weights.
- Append four hint-9 controls and strict schema-9 state/preset migration; existing factories remain neutral.
- Add a tenth effects group and readable sidebar navigation at the minimum editor size.
- This is simulated double tracking, not harmony generation or a second recorded performance.

## Fractional delay ring safety

- Correct near-integer fractional reads in the original delay/reverb and flanger.
- Add public-parameter regression fixtures for a floating wrap that could read past the buffer.
- Preserve interpolation, controls, routing and state schemas.

## Stereo musical delay

- Add independent stereo repeats, single echo/slapback and ping-pong with smooth automation.
- Add beat divisions, manual BPM and validated host-tempo fallback, without transport-phase claims.
- Append eight AU hint-8 controls, schema-8 state/presets and a ninth editor tab; legacy sounds stay neutral.
- Preserve the original delay/reverb and final limiter routing. Manual listening/DAW qualification is deferred.

## Unsigned developer artifacts

- Package runtime bundles with complete pinned project/JUCE source, notices, provenance and checksums.
- Add fail-closed packaging regression tests and developer build/host-test instructions.
- Declare Studio microphone usage on macOS and verify generated privacy metadata before packaging.


## Vocal limiter host integration

- Add default-off final-output sample-peak limiting with enable, ceiling and release controls.
- Append AU hint-7 parameters and schema-7 state/presets; migrate legacy versions with limiting off.
- Include limiter enable in sound presets while keeping global bypass independent.
- This limiter adds no latency and does not establish oversampled true-peak mastering.


## Factory catalogue expansion

- Add eight style presets using existing DSP, bringing the catalogue to fourteen.
- Preserve original factory identities/settings and schema-6 user libraries.
- Keep six sounds dependent on unfinished correction/harmony/doubling pending.


## Chorus/stereo-width continuation

- Add interpolated stereo chorus (rate/depth/mix) and mid/side width, neutral by
  default, with a sixth tab and 30 working sound controls.
- Append AU hint-5 IDs and migrate state/preset versions1–4 to v5.
- Record pinned MIT backend audit and latency/near-silence gates for future
  pitch/formant work. No backend dependency or pitch controls are integrated.
- Continue local-only validation and [skip ci] while Actions is paused.

## Split-band de-esser continuation

- Add linked high-band de-essing with frequency, threshold and maximum-reduction
  controls and a fifth editor tab. Zero reduction is neutral.
- Append stable AU hint-4 IDs and migrate state/preset versions 1–3 to v4.
- Keep Actions paused per user instruction; commits use [skip ci], validation local.
- Record independent DSP review and a pitch/formant architecture handoff; no new
  dependency or pitch/formant feature is represented as implemented.

## Linked gate/expander continuation

- Add a stereo-linked downward expander with threshold, ratio and release controls,
  a separate editor tab and neutral defaults. Preserve delay/reverb tails.
- Append three stable AU automation IDs; migrate v1/v2 project states and presets
  into v3 with the gate disabled. No new dependencies.
- Add numerical gate, stereo, automation, release, lifecycle, real-time allocation,
  engine/host audio and legacy migration coverage.

## Unreleased

- Establish architecture and agent ownership contracts for the initial foundation.
- Persist the consolidated product specification verbatim and add shared agent
  instructions; reconcile provisional architecture without starting new features.
- Add a smoothed audio engine with high-pass, saturation, compressor, dry/wet,
  bypass, sanitization and peak metering, with actual-signal regression tests.
- Add Vocal/Master plugin and Studio standalone JUCE targets, functional dark
  controls, host automation snapshots and validated versioned state restoration.
- Add product, sanitizer, formatting, static-analysis and limited credential/dependency
  checks; preserve open-source AGPLv3 licensing and reproducible cloud setup.
- Correct state-number parsing, optimized test allocator warnings and JUCE UI
  string encoding assertions found during QA/integration.
- Make state parsing portable across Apple libc++ versions and preserve decimal
  state roundtrips under non-English global locales.
- Add smoothed low/high shelves and parametric mid EQ, interpolated feedback delay
  and damped reverb with preallocated callback storage and tail tests.
- Add six working factory sounds and local user preset save-as, rename, favorites,
  search and JSON import/export with migration and no-overwrite protection.
- Add 20 functional controls, v1 project-state migration and stable AU automation
  version hints; honor XDG_CONFIG_HOME for writable Linux preset storage.
- Add worker-only monophonic dry pitch analysis with synthetic accuracy/voicing
  tests; live track-aware correction and harmony remain future work.
- Display measured dry input note/frequency/confidence using a bounded worker
  queue; verify dry-before-DSP input, overflow/lifecycle and concurrency behavior.

## Flanger continuation

- Add real short-delay flanging with rate, depth, signed feedback and mix controls
  on a seventh editor tab. Default mix0 preserves existing sounds; no dependency.
- Append AU hint6 and state/preset schema6 with strict v1–v5 neutral migration.
- Add measured impulse/feedback/modulation/decay and realtime-safety regressions,
  engine/host audio checks and a34-control GUI smoke test.
- Publish a prioritized pending-development list, distinguishing existing preset
  library operations from remaining factory sounds and unfinished product scope.

### Flanger neutral feedback correction

- Center the new signed-feedback host mapping and snapping on exact zero to avoid
  fused-rounding residuals during migration; preserve existing exact assertions
  and append regression checks. No prior parameter identity/range is changed.

### Reviewed effects integration and backend comparison

- Merge de-esser, chorus/width and flanger into main after relevant validation and
  independent QA; fix exact-neutral signed feedback and use supported macOS15 CI.
- Record a reproducible open-source alternative-backend probe; periodicity improves
  but one envelope fixture still fails. No backend replacement or release claimed.

## Vocal sample-peak limiter core

- Add independent linked mono/stereo sample-peak limiter with immediate ceiling
  protection, exponential release and no algorithmic latency or dependency.
- Cover supported rates, variable partitions, stereo ratio, exact unity, float
  extremes, parameter transitions, lifecycle and callback allocation behavior.
- Keep product routing/controls and oversampled mastering true-peak limiting pending.
