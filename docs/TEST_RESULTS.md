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


## Experimental pitch/formant core — 2026-10-22, issue #18

This target defaults OFF and is not linked to products. Existing Linux product
regression build/tests pass14/14. Experimental opt-in Release run passes12/13
suites: the retained sculptor.qualification suite fails, so the overall run exits8.
No failed numerical assertions are marked expected-failure or removed.

- sculptor.streaming:209checks pass in Release and ASan/UBSan across all four
  rates, mono/stereo and three candidate configurations. Exact delayed-neutral
  impulse/tiny-normal output, reported latency/drain bounds, resets, allocation
  failure and callback probes, fixed-grid partitions, numerical stereo equality,
  warmed invalid-operation history and hostile inputs are covered. Active impulse
  drain fits8704samples at48k Studio, with remaining signal0 after that bound.
- The fingerprint-guarded private patch changes near-silence impulse peak offset
  from0 to1152samples in Live48k while leaving upstream checkouts pristine.
  Active tiny amplitude is still attenuated by spectral floors; only neutral
  delayed dry has exact transparency.
- Studio4096/hop512 (doubled above48k) sine fixtures at110Hz shifted-12/-7/+7/+12
  pass all16 cases within20cents; worst observed about14.9cents. Smaller original
  windows failed up to74.7cents, independently confirmed spectrally by an agent.
- Qualification:76checks,3failures in both Release and ASan/UBSan, with no reported
  memory/undefined-behavior findings. At48k/F0100Hz, formant+7 moves a600Hz
  isolated envelope peak to700 instead of899;1200Hz with-7 gives1000 instead of801;
 1200Hz with+7 gives2200 instead of1798. These fail the20% gate. No silent base-F0
  retuning or relaxed tolerance was used. Full reproductions are in sculptor_tests.
- Initial fixed multi-formant search windows excluded a downshifted target and
  overlapped resonances. QA requested geometric inter-resonance boundaries; those
  corrected fixtures pass, while the broad isolated-envelope fixtures still fail.
  Stereo bit equality was replaced with justified relative RMS tolerance1e-4;
  independent measurements found rounding-level differences1e-6..1e-5.
- Tampered dependency header was rejected at configuration before compilation.
  Required header and MIT license hashes, exact pins, private patched hash and
  source cleanliness were checked. Formatting and limited dependency/credential
  scans apply; they are not comprehensive security/legal audits.

No new GUI/state parameters, host latency/bypass changes, live correction, real
voice/listening/CPU deadline, native macOS/AU, signing or release is validated.
Actions was not dispatched/rerun. See PITCH_FORMANT_CORE.md for opt-in commands;
its full CTest command intentionally continues to report the unresolved failures.

## Voice Sculptor automation regression — 2026-10-22

- Streaming269checks pass in Release and ASan/UBSan (60new assertions); no reported
  sanitizer findings. Four rates times three profiles, stereo with different input
  frequencies per channel, sample-positioned events at137/4093/8191/12289/17003
  exercise extreme targets, preserve flag, auto/manual base and return to neutral.
- Identical events rendered in37/1024sample partitions have maximum difference0
  in every tested configuration. Both outputs remain finite, ordinary new/new[]
  rejection reports no callback allocation, reported latency stays fixed and the
  final half-second exactly matches delayed dry on both channels.
- Full Release rerun remains12/13 suites: qualification76checks retains the same
  3formant failures. No quality tolerance or production code changed. The earlier
  14/14 product result belongs to the preceding core increment; no new GUI/native
  host test or listening/CPU-deadline qualification was performed here.

## Actions retry result — 2026-10-22

At b663d94, GitHub accepted the push/PR triggers. Superseded push runs were
cancelled by existing concurrency rules. PR build run39736030287 and source
quality run39736030307 failed before executing jobs. GitHub annotations state:
“The job was not started because recent account payments have failed or your
spending limit needs to be increased. Please check the 'Billing & plans' section
in your settings”. No new compiler, test, native AU or quality result exists.
No repeated rerun is requested; [skip ci] resumes for further commits until
billing/spending availability changes. Existing local results remain applicable
only to their recorded scope.

## Experimental Sculptor performance probe — 2026-10-22

Built and executed new developer-only benchmark with GCC14.2 Release, Linux x86_64.
Parsed stdout as JSON and checked24 unique rate/profile/block cases, finite outputs,
nonempty callbacks, ordered nearest-rank percentiles and ratio arithmetic. All
measurements completed. Observed processing/audio wall-time ratio0.0240–0.0659 and
2callback-budget overruns; this does not pass a real-time deadline gate. Three
seconds per case follow one-second warm-up; harmonic/noise/transient stereo input
and changing parameters are included. No product, device, listening or native test
is implied. Raw output is /tmp/darkvoice-sculptor-bench.json in this instance.

## Corrected experimental envelope — 2026-10-22

Local Release full CTest13/13 passes, including streaming269 and qualification140
checks. The original76 qualification checks and their tolerances are preserved.
Sixteen new fixtures span48/96k, F070/180Hz,600/1200Hz isolated envelopes and+/-7
formant shifts; envelope peak and F0-periodicity assertions pass. The original
600Hz/+7,1200Hz/-7 and1200Hz/+7 peaks now measure900/800/1800Hz against targets
899/801/1798Hz. No F0 retuning or original tolerance relaxation was used.

The private patched header is fingerprinted as
7003842b8347ba10710f280c33dba494cd3466a54b240cef76cfbbe32a9a93b8;
upstream source checkouts remain pristine. The new bounded sliding estimator uses
compensated double sums with per-window rebasing and preallocated source storage.
A repeated24case Release benchmark reports processing/audio wall-time ratios
0.0232–0.0624 with1 observed callback-budget overrun (shared Linux cloud). This
neither proves deadlines nor establishes target/native performance. Raw instance
output: /tmp/darkvoice-sculptor-bench-envelope.json.

CI YAML parses with isolated Linux/macOS Sculptor jobs. Actions is now reauthorized
following verified public visibility/default main; native/remote outcomes are
pending. Products remain unlinked to this experimental target, which defaults OFF.

Current correction validation also passes full ASan/UBSan13/13 (no reported
findings) and the unchanged Linux product regression14/14. Formatter19.1.7 and
scoped cppcheck2.17.1 on wrapper/benchmark pass; limited credential/dependency
scans pass. No new GUI, actual DAW or listening qualification is claimed.

Independent review of the exact compensated/rebased stage found no blocking
bounds, lifecycle, allocation or O(bands) issue. A temporary exploratory combined
pitch/formant matrix reported38 envelope-peak failures in144cases. Its exact-grid
amplitude estimator has not been independently verified against pitch errors, so
these are unresolved exploratory results, not a passed preservation qualification
or a proven diagnosis. Preserve issue #18 and investigate broader simultaneous
pitch/formant behavior before exposing the core. The committed140check suite
qualifies its stated synthetic subset only. No claim of arbitrary formant
preservation or perceptual completion follows from correcting the original cases.

## Retained combined-periodicity regression — 2026-10-22

Independent spectral review found that some exploratory envelope-peak failures
were exact-harmonic-grid estimator artifacts, while poor intended-F0 periodicity
is real. Three representative cases are now retained in sculptor.qualification,
using the existing vowel correlation threshold>0.8 without altered targets:
100/600Hz,pitch+12/formant0 gives-0.357724;70/600Hz,pitch-12/formant0 gives0.181850;
180/1200Hz,pitch+12/formant+7 gives-0.011096. All use48k Offline and declared baseF0.

Release qualification149checks has3failures. The original140checks still pass,
so the envelope correction is retained; the complete quality gate remains blocked.
Issue #18 remains open. The prior13/13 result describes the narrower140check
revision8bb2186, not this expanded gate. No naturalness/combined-preservation or
host-integration claim is made.

ASan/UBSan qualification reproduces the same149checks/3 numerical failures, with
no reported sanitizer findings. The test-only follow-up uses [skip ci] to preserve
the in-progress8bb2186 native product validation rather than restart its builds;
this is not a renewed Actions pause. CI success at that revision covers the
narrower140check gate only. No complete current quality success is claimed.
