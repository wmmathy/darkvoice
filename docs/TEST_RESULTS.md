# Validation record — 2026-10-21

## Current cloud instance

| Check | Result | Evidence / limits |
| --- | --- | --- |
| Linux GCC 14 Debug products | Passed | Vocal VST3, Master VST3 and Studio Standalone built with JUCE 8.0.6 |
| Product CTest | Passed, 7/7 suites | Five preserved core suites, engine regression, JUCE state/audio integration |
| Engine signals | Passed, 111 checks | Required four rates; HP attenuation, saturation/compression effect, smoothing, partition equivalence, stereo isolation, invalid snapshots, bypass and sanitization |
| Release core/engine | Passed, 6/6 suites | GCC14 -O3/NDEBUG, warnings as errors |
| ASan/UBSan core/engine | Passed, 6/6 suites | Leak detection and halt-on-error; no findings |
| Real editor smoke | Passed | Seven controls, slider/bypass attachments, rendered PNG; virtual X display |
| Standalone window startup | Passed | Real executable displayed; screenshot inspected; own processes stopped |
| clang-format 19.1.7 | Passed | All tracked project C++ headers/sources |
| cppcheck 2.17.1 exhaustive | Passed | Engine source and two core headers; no suppressions; not all JUCE code |
| Credential/dependency checker | Passed | 35 tracked text files; planted-file rejection test; pinned FetchContent check |
| Cloud setup repeat | Passed | Signed APT metadata, local prefix; existing dependencies reused |
| Specification preservation | Passed at intake | Byte-for-byte comparison; no specification edits |

Processor tests now initialize the JUCE message runtime; CTest explicitly fails
JUCE assertion/leak messages. The editor smoke exposed non-ASCII literals passed
to an ASCII JUCE String constructor; fixed and rerun without assertions. Screenshot
artifacts are /tmp/darkvoice-ui.png and /tmp/darkvoice-standalone.png (not tracked).
The standalone defaults to muted audio input to avoid feedback. No physical
microphone, audible monitoring, audio device or listening test was performed.

Allocation probes cover ordinary C++ new/new[] only, not aligned allocations,
direct malloc, locks or deadlines. No measured CPU budget, hard real-time guarantee,
perceptual quality or complete engine certification follows from these results.
Starter compression is unlinked per-channel and saturation is not oversampled.

## GitHub observations

Earlier core CI ran successfully on Linux/macOS at 5f10c33. At 1d1e16e, core macOS
Debug/Release, Linux Debug and sanitizer jobs passed; Linux Release and both
product jobs failed. Local GCC Release failure was reproduced and corrected by
keeping the test allocator uninlined, without suppressing warnings. Product CI
failures are not yet fully diagnosed; remote job logs require a currently blocked
GitHub log-download hostname. Saved a domain addition to the environment draft.
New product builds emit error annotations and preserve underlying build exit
status, improving API-readable failure evidence.

At 5a11233, all five remote core configurations (Linux/macOS Debug/Release plus
Linux ASan/UBSan) passed. macOS product build failed with an API-readable compiler
annotation: floating-point std::from_chars is deleted in that Apple libc++ version.
Replaced it with classic-locale full-token stream parsing. A new locale roundtrip
regression then exposed JUCE's locale-dependent XML numeric serialization; fixed by
canonical serialization and validated numeric normalization before state replacement.
All seven local suites pass again, including that regression. The rerun results below supersede that failure; they do not establish DAW host testing. The older Linux product job at 5a11233 was superseded by the corrected source revision.

At 796a409, source-quality CI and all five core configurations passed. The macOS
product job passed native AU/VST3/Standalone builds, all processor/DSP suites and
both auval invocations (Vocal Dvvc/Dkvc and Master Dvms/Dkvc). Job
119365682023 in run 39641368426 confirms each step succeeded. This validates the
macOS development targets, not actual GarageBand/Logic sessions or signing.
Linux product job 119365682224 also passed: native Vocal/Master VST3 and Studio
Standalone Release builds, seven CTest suites, real GUI attachment smoke under
Xvfb and retained test evidence. Build-and-tests run 39641368426 and source-quality
run 39641368435 both completed successfully at code revision 796a409.

Some duplicate push/PR runs were cancelled by the configured concurrency policy;
cancelled is not passed. No merge, release publication or fresh cloud snapshot
restoration was verified.

## Unrun product qualification

Explicit Apple Silicon/Intel architecture coverage; VST3 validator;
GarageBand/Logic/REAPER/Cubase; microphone/recording;
DAW automation/session restoration; measured latency/CPU/glitches; listening and
all future pitch/harmony/AI/mix/master/export acceptance remain outstanding.

## Exploratory cloud timing (not a real-time guarantee)

GCC14 -O3, stereo 256-frame blocks, 5,000 blocks/rate (1.28M frames), copied
synthetic input, HP80Hz/drive0.4/ratio3, one processing thread:

| Rate | Elapsed | Audio-duration ratio | Maximum observed block |
| --- | --- | --- | --- |
| 44.1kHz | 0.259s | 0.89% | 1.114ms |
| 48kHz | 0.254s | 0.95% | 0.458ms |
| 88.2kHz | 0.302s | 2.08% | 2.232ms |
| 96kHz | 0.263s | 1.98% | 3.363ms |

Average ratio includes buffer copying and clock overhead. Cloud scheduling can
produce outliers: the observed 96kHz maximum exceeds its 2.667ms buffer period.
This is a throughput observation, not deadline qualification or a physical-device
glitch test. Reproduce with retained /tmp/darkvoice-benchmark.cpp and binary in
this instance; a portable hardware benchmark harness remains future work.


## M2 continuation — local validation

- Linux Debug Vocal/Master VST3 and Studio Standalone builds passed.
- 10/10 CTest suites passed: legacy core5, engine339 checks, effects618 checks,
  pitch168 checks, presets639 checks, plus processor/state integration.
- Release core8/8 and ASan/UBSan core8/8 passed; no findings.
- GUI smoke: all20 control attachments, real factory-preset rendering, bypass
  preservation and XDG default storage passed; standalone window startup and
  screenshot inspected. No physical audio device/listening/DAW test was run.
- Full-token legacy/v2 project-state validation, stable old AU version hints1/new2,
  locale-independent persistence and legacy parameter defaults are tested.
- Presets: malformed/deep/duplicate/trailing/oversized/overflow JSON, finite/range/
  name/identifier checks, symlink rejection, concurrent exclusive exports,
  import UUID regeneration, favorites/rename/migration/preservation all pass.
- Formatter19.1.7 and exhaustive cppcheck2.17.1 passed all8 scoped DSP/header files;
  limited credential/dependency scans and planted-fixture self-test passed.

New reverb is a damped Schroeder design, not separate plate/room/hall algorithms.
Delay changes can alter pitch naturally. Channels remain independent. Preset file
I/O is bounded and off the audio thread but synchronous on the message thread.
No-overwrite commits use same-volume hard links and fail safely where unsupported.
Path checks reject symlinks; this is not a sandbox against adversarial same-user
parent-directory replacement races. Pitch analysis is bounded worker-only YIN,
not live correction; octave errors/real-vocal accuracy remain unqualified.
No callback allocation, hard deadline or listening certification follows from
ordinary new/new[] probes and synthetic signals. New native CI/auval is pending.


## Dry pitch telemetry continuation

- Local products rebuild and 11/11 CTest suites pass. analysis.worker has14 checks
  covering actual F0, untouched input, ring overrun recovery, rate/lifecycle changes,
  stereo downmix and ordinary per-producer-thread allocation checks.
- The processor test detects a quiet dry A2 even when input/output gains attenuate
  rendered audio below the detector gate, proving the tap precedes DSP.
- Real GUI timer feedback shows A2 from generated input; all20 controls and actual
  preset audio still pass the display smoke. The harness pumps real timers because
  it has no native event loop. Standalone startup/default writable storage pass.
- Focused ThreadSanitizer harness compiled JUCE core/audio-basics, PitchDetector and
  DryPitchAnalyzer with instrumentation, exercised concurrent producer/consumer and
  shutdown, and exited0 with no findings. Harness/source/logs remain under
  /tmp/darkvoice-thread-* in this instance. This is not full host qualification.
- Native CI at0a4e2e5 passed Linux products and Linux core/sanitizers, but macOS
  tests failed. Raw log host remains unavailable; test commands now emit
  API-readable failure annotations while preserving actual exit status. Diagnosis
  and final-revision native validation remain in progress.

## Neutral EQ portability fix

CI failure annotations at 98f1c8a isolated all four supported-rate failures to
the exact neutral-effects identity assertion on macOS. Zero-gain RBJ bands now
use identity coefficients directly, avoiding platform-dependent cancellation
rounding without relaxing the test. Linux products rebuild and 11/11 CTest suites
pass after this fix; native CI for the fix remains pending.


Final implementation validation at 716c599: GitHub Actions run 39656700867
passes all seven build/test jobs, including macOS Debug/Release core, Linux
Debug/Release core and sanitizers, both native product jobs, Linux GUI smoke,
and unsigned Vocal/Master auval. Source-quality run 39656701080 also passes.
Local post-fix results: products 11/11, Release core 8/8, ASan/UBSan core 8/8,
GUI/standalone display smoke pass. Documentation-only successors require their
own CI; this evidence identifies the tested implementation commit explicitly.
Actual GarageBand/other DAW sessions, microphone and listening remain unrun.


## Linked gate/expander (issue #12) — local validation

- Full Linux Debug products build (Vocal/Master VST3, Studio standalone) and
  current CTest run pass 12/12. Release and ASan/UBSan core suites pass 9/9.
- dynamics.gate passes 153 checks: four rates, mono/stereo, exact default identity,
  -60 dBFS -> -20 dB gain at threshold -40/ratio 2, linked stereo balance,
  reopening/release timing, neutral disable automation, partition invariance,
  invalid inputs/prepare/snapshots, nonfinite/subnormal samples and ordinary
  new/new[] allocation probes. Engine and actual plugin audio exercise the gate.
- v1/v2 project and preset migrations retain prior values and disable the gate;
  v3 roundtrips active controls. Old AU hints remain 1/2 and appended IDs use 3.
  Invalid state/preset values still reject before replacement.
- Xvfb smoke verifies all 23 real attachments, factory audio, bypass and dry-pitch
  feedback; selects/renders the gate tab. Standalone startup and default writable
  preset directory pass. Current snapshot: /tmp/darkvoice-gate-ui.png.
- Formatter19.1.7, exhaustive cppcheck2.17.1 on four scoped DSP/schema files,
  limited credential/dependency scans and scanner self-test pass.
- Initial host check used a stale pre-v3 test object; rebuilding its target and
  rerunning the full current suite passes. No assertions were disabled.
- Native CI/AU validation for this increment is pending. Earlier PR #11 results
  do not establish new native validation. No physical device/DAW/listening or hard
  real-time deadline qualification was performed. Expander has no lookahead or
  hysteresis and may attenuate initial transients; maximum attenuation is 80 dB.


Native gate validation at b1a7196: [Actions run 39661596101](https://github.com/wmmathy/darkvoice/actions/runs/39661596101)
passes all seven jobs: macOS and Linux Debug/Release core, Linux sanitizers,
and both full product builds/tests. Linux editor smoke and unsigned Vocal/Master
AU validations pass. Source-quality run 39661596167 also passes. Documentation
successors rerun CI; these results identify the tested implementation explicitly.
No DAW/microphone/listening, signing, notarization or public release was performed.


## Split-band de-esser — local-only validation (issue #14)

User paused GitHub Actions because credits are exhausted. No workflow was
dispatched/rerun. New commits use [skip ci]; native macOS/AU validation is unrun
for this increment. Earlier passing native evidence does not validate these changes.

- Linux Debug Vocal/Master VST3 and Studio standalone build. Product suites pass
  13/13, Release core 10/10, ASan/UBSan core 10/10.
- dynamics.deesser passes 209 checks across four rates and mono/stereo: neutral
  identity (including tiny normals), measured high-band reduction, low-band and
  mixed-tone preservation, stereo linking, threshold/frequency/depth effects,
  burst recovery, disable automation, buffer partitions, invalid blocks/snapshots
  preserving warmed history, hostile samples and ordinary allocation probes.
- Engine/plugin audio tests exercise actual high-band attenuation and bypass.
  State/presets v4 roundtrip, strict v1/v2/v3 migration and invalid-value rejection
  pass. Old AU hints remain stable; new three IDs use hint 4.
- Xvfb smoke verifies all 26 attachments, actual factory audio, bypass/dry-pitch
  feedback and the fifth de-esser tab; standalone startup and writable default
  preset storage pass. Snapshot /tmp/darkvoice-deesser-ui.png in this instance.
- Formatter19.1.7, exhaustive cppcheck2.17.1 on four DSP/schema files, limited
  credential/dependency scans and planted-fixture self-test pass.
- Dedicated read-only DSP QA found a tiny-normal neutral mismatch; lead corrected
  input/output handling and added explicit normal/subnormal regressions. Recheck
  found no blocker and requested stronger active-state migration fixtures; lead
  added all-three-field v2/v3 reset checks. Dedicated architecture agent produced
  PITCH_FORMANT_PLAN.md; no new backend or pitch/formant feature was implemented.

The one-pole split is not linear-phase or a classifier for sibilance. Maximum
reduction describes high-band gain, not the whole output. No new dependency,
physical-device/DAW/listening qualification, signing or public release was added.


## Chorus/stereo width — local-only validation (issue #16)

Stacked on unmerged draft de-esser PR #15. No workflow was dispatched/rerun;
commits use [skip ci]. Native macOS/AU, real device/DAW/listening, signing and
release qualification remain unrun for this increment.

- Linux Debug Vocal/Master VST3 and Studio standalone build; product suites pass
  14/14. Release and ASan/UBSan core suites pass11/11.
- effects.modulation passes182 checks at44.1/48/88.2/96kHz: fractional 15ms impulse
  timing, exact ramp delay oracle for rate/depth/mix/stereo phase, width algebra
  and mono sum, side-only/extreme peak behavior, mono no-op, tiny-normal neutrality,
  partition invariance, invalid warmed blocks/snapshots, allocation failure and
  ordinary callback allocation probes, disabled automation and finite samples.
- Engine/host tests exercise real chorus echoes, width fold-down, original dry
  stereo and bypass. State/presets v5 roundtrip and strict v1–v4 migrations pass,
  including resetting active new fields. Existing AU hints are stable; new hint5.
- Dedicated read-only QA caught a generic JSON double-versus-float minimum bug.
  Checked float-domain validation accepts decimal0.1/0.2 and rejects meaningful
  below/above-bound/overflow values. Regression fixtures pass without relaxing
  type, finite, count, duplicate-key or atomic-replacement validation.
- Initial product suite correctly failed on JUCE assertions caused by a new test
  using the wrong getMagnitude overload. The corrected channel/start/count query
  and full rerun pass; assertions and failure detection remain enabled.
- Xvfb smoke passes all30 attachments, factory audio, bypass/dry-pitch feedback
  and sixth chorus/width tab. Standalone startup and default writable preset
  directory pass. Snapshot /tmp/darkvoice-modulation-ui.png in this instance.
- Formatter19.1.7, exhaustive cppcheck2.17.1 on four scoped DSP/schema files and
  limited credential/dependency scans/self-test pass. No hard real-time deadline
  or aligned-allocation audit follows from ordinary new/new[] probes.

Width applies to the wet path, can raise peaks and is not a mastering limiter.
Chorus has no feedback and its delay is musical rather than host compensation.
A separate read-only agent pinned/inspected MIT Stretch/Linear and ran scoped
latency/allocation probes, recorded in PITCH_BACKEND_AUDIT.md. That backend remains
unintegrated and its quality/near-silence/aligned-bypass gates remain outstanding.

## Flanger integrated increment — 2026-10-22

- Full Release core12/12 and ASan/UBSan core12/12 pass locally. New Flanger suite
  has123checks: all four rates/layouts, exact neutral/tiny samples, measured4.5ms
  impulse centroids, signed geometric feedback echoes, LFO excursion/rate, decay,
  stereo equality, partitioned automation/reset, warmed rejection, hostile audio,
  allocation failure and ordinary callback allocation probes. No sanitizer findings.
- Linux Debug Vocal/Master VST3 and Studio Standalone build; product suites15/15
  pass, including actual flanger impulse audio and all v1–v5 state defaults when
  restoring over active flanger targets, v6 roundtrips and unchanged older AU hints.
  Preset v5 migration retains chorus while adding neutral flanger; v6 bank roundtrip
  and validation pass. Engine tests cover delayed wet and exact global dry mix.
- Initial expanded legacy-state test incorrectly truncated APVTS children by order.
  APVTS ordering is not a schema guarantee; ID-based filtering fixes the fixture
  without changing parser checks or migration assertions. Full product rerun passes.
- Display-backed smoke passes34 parameter attachments, seventh Flanger tab, actual
  factory audio, bypass and dry-pitch display; screenshot inspected at
  /tmp/darkvoice-flanger-ui.png. Xvfb uses local UNIX display with TCP disabled.
- Scoped exhaustive cppcheck2.17.1, formatter19.1.7 and limited credential/dependency
  scans apply. Native CI results pending; prior PR #19 native success is not reused
  as this increment's success. No actual DAW/microphone/listening validation.

## Integration review and runner maintenance — 2026-10-22

Independent read-only QA rebuilt DeEsser209/209, modulation182/182 and Flanger
123/123 under GCC14/O2/strict warnings in /tmp. No blocking source issue found in
PR15/17/21, old AU identities or migrations. Exact relevant parent sources match
bfc9b54 native/product-passing evidence. PR15 andPR17 were merged normally; this
does not qualify the unrelated experimental Sculptor failures.

Flanger native run39744937318 passed Linux core/sanitizer/product jobs; macOS
jobs had no steps and annotated “The job was not acquired by Runner of type
hosted even after multiple attempts”. Retry only the failed/cancelled jobs.
Upstream runner-images README confirms deprecated macos-14 and supported Arm64
macos-15. Both build and manual unsigned-artifact workflow labels updated, with
all checks retained. YAML validation and source review apply; native outcomes
for the new label remain pending. No release/artifact dispatch is performed.

## Flanger signed-neutral native correction — 2026-10-22

Supported-runner workflow39748010215 passed both macOS core configurations and
all Linux jobs. macOS product compilation passed; headless plugin.state_and_audio
failed its exact v5-to-v6 neutral-feedback migration assertion, so auval was not
executed. Check-run annotations identified the precise failing assertion; direct
artifact download was denied by destination policy, so annotations supplied the
diagnostic evidence. No failed gate is waived or softened.

A local std::fma(.001f,850.f,-.85f) probe demonstrates the nonzero residual possible
in JUCE's start-relative signed snapping; the new feedback range now uses symmetric
zero-centered conversions and zero-anchored snapping. Interval metadata retained,
other parameter ranges unchanged. Add exact neutral/normalisation/symmetry checks;
existing exact migration assertions remain. Local product suites15/15 pass after
this correction. New native result is pending; do not reuse earlier AU evidence.

Independent reviewer reproduced the old1.65309757e-08 residual under -O3/-mfma/
-ffp-contract=fast and checked all1701 legal signed-feedback grid values against
the custom conversion/snap: exact zero, odd symmetry, bounded roundtrip error0.
No blocking fix issue found. Native full product behavior still requires rerun.

## Automatic integration complete and next backend evidence — 2026-10-22

Flanger corrected source d1ba798 passes all7 jobs in39818430357, including macOS15
headless suites and both unsigned auval validations; source-quality39818430358
also passes. PR #21 merged normally at938aa83 with expected head verified, after
independent source/FMA review. Issue #20 closed; source branches/worktrees retained.
Together with de-esser/chorus PR15/17, eligible effects are now integrated in main.
No failing gate bypass, admin override, actual DAW/listening or release claim.

A lead-run pinned Rubber Band v4 pure-built-in probe passes target-period
correlation on six configurations of the three previously problematic inputs,
but independent FFT finds a preserved-envelope miss of24.17% on octave-down70Hz.
It is an unqualified candidate, not an integrated dependency. Source, license/API
notes, reported variable startup delays and reproducible developer probe are in
RUBBERBAND_AUDIT.md. PR #19 remains draft/blocked. Product/runtime source is
unchanged by this documentation/probe increment.

The repository developer probe was compiled independently with GCC14/O2 and
-Wall/-Wextra/-Wpedantic/-Werror, then linked to the pinned pure-built-in library
and executed across six cases. Independent read-only QA reproduced full-buffer
finiteness, periodicity and FFT peaks, verified pin/fingerprints and found no
blocking documentation/probe issue. The exact FFT reproduction command is also
retained in RUBBERBAND_AUDIT.md. This follow-up changes no production source,
headers, tests, CMake or workflows; redundant native CI is skipped for this
reviewed docs/probe commit. Product validation remains at source d1ba798, not a
claim that the alternative backend is qualified. Normal functional CI stays enabled.
