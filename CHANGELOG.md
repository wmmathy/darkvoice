# Changelog

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
