# Changelog

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
