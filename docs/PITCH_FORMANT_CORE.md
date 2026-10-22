# Experimental streaming pitch/formant core

This is development/qualification infrastructure, not a completed Voice Sculptor
feature. It is OFF by default, is not linked to Vocal/Master/Studio, and exposes no
plugin controls or new preset/state fields. Existing products retain their current
latency, bypass and dry/wet behavior. Host integration is blocked by issue #18.

The isolated PitchFormantProcessor wraps pinned MIT Signalsmith Stretch/Linear
with bounded 64-sample scratch buffers, deterministic parameter updates, a delayed
neutral ring and dry/processed crossfade. Prepare allocates; process/reset/setters
have ordinary allocation probes. Three candidate FFT profiles are available for
qualification only; these are not listening-qualified Live/Studio/Offline modes.
At48k their reported alignment latencies are1152/4608/9216 samples (24/96/192ms).
At44.1k times are higher; above48k block sizes double. No suite-wide latency or
low-latency monitoring promise is made.

cmake/Signalsmith.cmake fetches exact audited SHAs without executing upstream
CMake, verifies required header/license hashes and makes a private header copy.
One exact, hash-guarded change disables the near-silence immediate-copy shortcut;
it preserves numerical floors elsewhere and leaves upstream files unchanged.
Active tiny audio can still be attenuated. The neutral path preserves delayed
normal finite input exactly, including tiny normals. Spectral processing clamps
input to +/-8 for numerical safety; it is not a mastering limiter.

## Reproduce without Actions

```sh
cmake -S . -B build/sculptor -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  -DDARKVOICE_BUILD_SCULPTOR_CORE=ON
cmake --build build/sculptor --parallel 2
ctest --test-dir build/sculptor --output-on-failure
```

The last command currently exits nonzero: sculptor.qualification is a retained,
real failing test. It is not skipped or marked WILL_FAIL. To inspect passing
streaming behavior separately, select `-R '^sculptor.streaming$'`; doing so does
not establish a passing overall qualification run. Add
`-DDARKVOICE_ENABLE_SANITIZERS=ON` in a separate build for ASan/UBSan.

For audited existing checkouts, CMake supports the standard overrides
FETCHCONTENT_SOURCE_DIR_DARKVOICE_STRETCH and FETCHCONTENT_SOURCE_DIR_DARKVOICE_LINEAR.
Fingerprints still verify every required header/license. A tampered-source negative
fixture was rejected before compilation. MIT notices are in third_party/licenses.
Set DARKVOICE_BUILD_SCULPTOR_CORE=OFF to build the current products without these
experimental dependencies; this is the clean-build default.

## Qualification evidence and remaining work

Streaming checks cover profiles/rates/layouts, exact neutral impulse alignment,
small normal samples, reset, allocation failure and callback probes, partition
invariance, numerical stereo equality, quiet active timing and invalid input.
An independent agent confirmed initial small-window pitch errors spectrally;
raising Studio to4096 samples (doubled above48k) meets20cents for16 synthetic
110Hz/shift combinations. This is a narrow fixture result, not arbitrary vocal
accuracy. Backend stereo phase-lock arithmetic is numerically, not bit, equal.

The first multi-formant fixture used overlapping fixed search ranges that excluded
one downshifted target. Review corrected them to geometric boundaries between
resonances without changing tolerance. Independent isolated-formant cases still
fail the20% envelope-peak gate at F0100Hz/base100Hz:

| Input envelope | Formant shift | Expected peak | Measured peak |
| --- | ---: | ---: | ---: |
| 600Hz | +7 | 899Hz | 700Hz |
| 1200Hz | -7 | 801Hz | 1000Hz |
| 1200Hz | +7 | 1798Hz | 2200Hz |

Do not silently halve the declared F0/base parameter to tune these fixtures, relax
quality tolerances or expose controls while this gate fails. Investigate a better
envelope estimator/algorithm or compare another compatible backend. Then qualify
automation, transient/unvoiced behavior, CPU and real-vocal listening. Before host
integration, establish and test latency-aligned global dry/mix/bypass and product
boundaries. No reference-aware correction, harmonies or host qualification is
implemented by this core. Actions remains paused; native checks are unrun.

## Automation regression coverage

Sample-positioned parameter events now cover all four rates and all three candidate
profiles in stereo, with independent channel inputs. Events at non-grid offsets
exercise opposite pitch/formant extremes, preservation changes, auto/manual base
changes and return to neutral. Rendering identical events in 37- and 1024-sample
host partitions produces maximum difference0 in the local Release run. Processing
and setters allocate nothing under ordinary new/new[] rejection; latency remains
fixed, both channels stay finite and settle to exact delayed dry. These checks do
not establish click-free perceptual quality or resolve the three envelope failures.
