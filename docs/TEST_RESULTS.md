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

## Vocal sample-peak limiter core — 2026-10-22

Standalone strict GCC Release and ASan/UBSan each passed184 limiter checks.
Full Release core CTest13/13 passed. clang-format19.1.7 and cppcheck2.17.1
warning/performance/portability analysis of the new module passed. Checks include
all supported rates/layouts, signed ceilings, linked ratios, analytic release,
instant lower ceilings, partition/reset/invalid-state preservation, finite extreme
inputs/tiny normals and prepare/set/process/reset allocation probes. This is a
sample-peak module outside products, not true-peak, listening or DAW qualification.
Native CI/push/PR/merge are blocked by expired/rejected GitHub authentication401;
no remote validation is claimed.
Full Debug ASan/UBSan core CTest13/13 subsequently passed with leak detection and
halt-on-error enabled; no sanitizer diagnostic reported.
## Factory catalogue expansion — 2026-10-22

Linux Debug product build and all15 CTest suites passed after expanding six
factory snapshots to fourteen. New product render fixture applies every preset
at44.1/48/88.2/96kHz, mono/stereo, variable73/257frame buffers, checking finite
non-silent output. Preset persistence suite checks all34field maps and unique
snapshots, favorites, schema migration, import/export and user-file preservation.
No listening or actual DAW qualification was performed. Native CI pending.
Factory catalogue native follow-up: run39835022228 passed all7jobs on exact
source929926d, including macOS15/Linux products and Linux sanitizers; source
quality39835021918 passed. Credentials worked again on retry. Actual DAW/listening
qualification remains unrun.

Limiter core product follow-up: Linux Debug products built and all16 CTest suites
passed on the core-only revision, including factory catalogue rendering. This does
not validate the subsequently implemented host routing/schema7 controls.

## Limiter host integration v7 — 2026-10-22

Linux Debug product build and16/16 suites passed, including v1..6 preset/state
migration, v7 nondefault limiter roundtrips, invalid Boolean rejection, all14
factory renders and final-output limiter actual audio. Engine regression5783checks
passed; Debug ASan/UBSan core13/13 passed. GUI smoke passed37controls/eighttabs,
all attachments/bypass/factory audio and dry-pitch feedback. Lead inspected the
900x780 screenshot: all8tabs and Limiter enable/ceiling/release visible. clang-format
and cppcheck of changed independent engine/schema/limiter passed. Native CI and
independent host QA pending. No actual DAW or listening qualification claimed.

Lead follow-up found Boolean input could round from fractional double to0/1 before
validation. JSON now checks the numeric Boolean domain before float conversion;
XML validates a double Boolean before existing float parsing. Added raw JSON
1e-50/1.00000001/0.99999999 and XML near-one regressions. Initial writer-based tiny
fixture serialized to valid0, so corrected fixtures emit raw literals. Targeted
preset/plugin suites2/2 now pass. Core/GUI unchanged; native updated source pending.

Independent review additionally demonstrated double underflow (`1e-500`) and
beyond-double near-one literals. Final implementation validates decimal Boolean
text exactly before numeric parsing, including JSON decoded property keys. Shared
helper has an internal1MiB bound and saturated exponent arithmetic; accepts exact
scientific0/1 equivalents while rejecting tiny/high-precision fractions. Independent
Decimal-oracle comparison4425cases passed. Updated raw JSON/XML rejection and
valid scientific-equivalent tests pass in targeted2/2 suites. Formatter/static
checks pass; new native source run pending. Earlier double-only fix was incomplete.

Native limiter final: core39840690725 and host39840737529 each passed all7jobs
on5f79aaa/82a3edd, with corresponding source-quality39840690694/39840737603 passed.
macOS26 ARM product builds, headless suites and both unsigned auval checks passed;
Linux products/GUI and core sanitizer checks passed. PR24/25 normally merged.
Actual GarageBand/Logic/listening remains unrun. Earlier macOS15 allocation failures
are runner failures, not a successful platform result or waived DSP assertion.

## Unsigned developer packaging — 2026-10-23

Lead and independent QA each passed12real-temporary-Git packaging fixtures.
Checks cover both runtime layouts, full pinned source/notice inclusion, checksums,
wrong/dirty/untracked dependency, stale/missing build provenance, missing products,
unsafe product/source symlinks and exclusive-output/partial-publication cleanup.
Fixtures contain labelled fake binaries, not runnable products. Verified current
project/JUCE pins have no gitlinks or archive attributes excluding tracked source.
Archive commands disable lazy-fetch networking; Python3.11+ is documented. Builder
architecture is identified as provenance, not universal/Intel qualification.
Actual manual workflow/native packaging remains pending; no artifact-exists claim.

Standalone review found Studio had no explicit JUCE microphone-permission metadata.
CMake now enables it only for the Standalone format with a local monitoring/analysis
purpose. Packaging checks actual generated macOS Info.plist for a nonempty usage
entry; fixture/negative tests now13/13 pass. Updated real native build required
before handing over artifacts; no microphone hardware/runtime grant is claimed.

Developer artifacts verified on exact sourcea2f45e7: manual39902701073 passed both
Linux/macOS26 jobs, including actual compiled-runtime packaging and the generated
Studio microphone Info.plist check. Normal CI39902703362 passed all7jobs; source
quality39902703358 passed (13packaging fixtures). Artifacts12480210082Linux and
12479651652macOS are present/unexpired (34,641,269/37,753,609bytes respectively); no public release.
Lead also rebuilt local Debug products16/16 and packaged real Linux binaries:
all4294tracked project/JUCE source files matched archive entries exactly, all5
runtime regular files matched original bytes, and all sidecar checksums passed.
Local Debug archive is development evidence, not the uploaded Release artifact.
Actual GarageBand/Logic/microphone hardware/listening qualification remains pending.

## Stereo delay integration — 2026-10-23

At the stereo-delay working revision, lead Linux Debug product build and all17
CTest suites pass (37.14sec), including fake-playhead actual echo timing and strict
v8/legacy state/preset regressions. Release core14/14 and ASan/UBSan core14/14 pass
(leak detection/halt enabled,33.82sec sanitizer suite). Core388checks cover all
four rates, mono/stereo, three modes,72tempo/division cases, fractional/3sec timing,
feedback decay, invalid calls, allocation, smoothing, partition/reset and extremes.
Engine5839checks and preset1000checks pass. Independent QA reproduced388core and
5839engine checks in strict optimized builds and approved source integration.

Generalized exact-small-integer lexer independently matched70,182Decimal-oracle
cases, preserving Boolean exactness and rejecting fractional/rounded enum tokens.
Real virtual-X GUI smoke passes45attached controls, nine tabs and preset audio;
/tmp/darkvoice-stereo-delay.png inspected. This is local automated evidence;
native CI is pending, and user explicitly defers actual host/microphone/listening
qualification until remaining feature implementation is complete.

## Existing effect fractional-wrap regression — 2026-10-23

Lead reproduced baseline ASan heap-buffer-overflow through public ToneEffects
and Flanger APIs using actual source, not a private ring-only probe. ToneEffects:
48kHz/reverbSize0.03125/reverbMix0.5; Flanger:48kHz/rate3.498542308807373/depth1/
mix1, within21blocks of512samples. Negative epsilon plus ring length could round
to the length. Integer/fraction-before-wrap reads remove the invalid index.

New public impulse/reset/partition/isolation regressions cover four rates and
mono/stereo. Existing tolerances were preserved. Strict optimized focused suites
pass666effects and171flanger checks, including independent QA reproduction and
approval. Focused ASan/UBSan passes without findings; integrated14sanitizer suites
and17JUCEproduct suites pass (33.71sec/37.62sec). Native CI is pending. Manual
host/listening tests remain deferred. No parameter/state/latency/API change.

Stereo delay first native run39954039990: core jobs pass, macOS product suite
failed the invalid-raw-enum regression's exact0.1f output assumption. Independent
QA reproduced JUCE/FMA signed0dB snapping residual: fma(0.1f,240,-24) gives
3.5762786865e-7dB, sufficient to change float output despite correct rejection.
Replaced that assumption with a stronger paired-processor test of every sample
against a held valid snapshot, injecting invalid mode/division plus observable
valid gain changes, then requiring gain divergence after valid enum restoration.
Both NaN and finite/out-of-range enums are covered; no production DSP or quality
tolerance was relaxed. Local corrected plugin suite passes; native rerun pending.

## Simulated double tracking — 2026-10-23

Local LinuxDebug products18/18 pass45.66sec, Releasecore15/15 pass3.34sec and
ASan/UBSan core15/15 pass47.29sec with leak/halt enabled. NewDoubler381checks pass
strict optimized and sanitizer standalone runs and independent optimized QA.
Tests include independent absolute-history/random-knot oracle, fractional impulse
weights,4rates/layouts, DC/convex peak/57ms tail, deterministicreset, activecontrol
response and automated partition invariance, allocationfailure/callback allocation
checks, invalid warmed state and hostilefloats. Preset suite1209checks passes.

Actual host delay-copy timing, neutral/state migration and rawenum atomicity tests
pass. Drymix uses exact pairedreference comparison rather than assuming nominal
0dB normalizes exactly underFMA; globalbypass exactness remains asserted. GUI
smoke passes49attachments/10sidebar groups and factoryaudio; inspected
/tmp/darkvoice-doubler.png. Independent source/sidebar/test QA approved. NativeCI
and actual host/listening remain separate, pending/deferred respectively.

## Fifteenth factory preset — 2026-10-23

Lead product build and2/2relevant preset/host suites pass23.95sec. Testsfingerprint
all14originalfactory ID/name/49floats, validate15uniquecompletefactories and
persistence, then isolate actual DoubleTrackedRock audio against identical
settings withdoublerMix0 atfourrates/mono/stereo. Independent QA approved; native
checks remain pending. No perceptual/actualsecondperformer claim, manualaudition
deferred. StereoDelay andringfix native results/merges recorded in status.

## Space Reverb — 2026-10-24

Standalone strictRelease and ASan/UBSan697checks0failures; independently reproduced
strictoptimized697.47allocation-failure paths; independent analytic early/Fdn/
Plate/tone/predelay oracles, allrates/layouts, crossfade/history andpartition,
normalizeddecay andweightedfeedback/passivity gates pass. Initial testdevelopment
replaced an arbitrary uncontracted25percent densityguess with actualstructural
echo/lateenergy measures; no publishednetwork/analyticgate was changed. Room
is structurally sparse; no dense/physical-room/perceptualquality claim.

Lead19/19JUCEproduct suites87.10sec, engine6770checks21.04sec, plugin43.93sec,
preset1515checks pass. GUI54attachments/11groups passes and screenshot inspected.
ASan firstconcurrent16suite run timed out engine at60.04sec, not anassertion or
sanitizer finding. Identicalengine alone passes45.62sec; unchanged60sec limit.
Sequential16/16ASan/UBSan run110.11sec passes (leak/halt enabled). NativeCI pending;
actualmanualDAW/listening/microphone qualification explicitlydeferred.

Static48k40sec late35sec energy ratios: Room8.77e-39,Hall1.61e-21,Plate5.81e-32.
These measuredstatic fixtures are not universalautomation proof. Independent
roundedlength weightedloopnorm maxima atalpha.4/s remain<1 (Hall.99393549).
A conservative movingpredelay Schurfactor explains60sec hosttail choice; bounds
are relative tofiniteinputl2, notpeak/inaudibility/exactsilence. Existinglegacy
reverb remains unchanged andall15factorymaps newfieldsneutral.

Release16/16passes5.58sec ontheintegratedsource. Formatting and cppcheck pass.
The original60sec engine timeout remains unchanged; independentQA approved
sequentialresource-isolated verification. No source/numericgate workaround.

## Colour core qualification — 2026-10-24

478strictRelease/ASan standalonechecks pass; independentstrictRelease478passes.
LeadCMakeReleaseeffects.colour passes2.53s,ASan/UBSanpasses58.65s(leak/halt).
48predeclaredfixtures:4096coherentframes after4096warmup;bins719/1597 andtwo-tone
683+1129,amplitudes.15/.5,bothstyles,drive1,tone16000,fourrates. Independent32x
reference recomputeskernel andgenericzero-insert/convolution (notproduction
polyphase),latency62vs56 aligned6. Worstresidual-63.664dB/inputRMS<fixed-55gate,
minimumimprovement50.118dB>fixed20gate vs1x. No fixture/thresholdchanges.

Filterresponseerror.000164014<.0002,stopband-75.267dB<-75. Identity/DC/groupdelay,
exactneutral56delay ofnormal±floatextremes andsanitization,partialmixalignment,
TapeHFcompression/memory,Tubeevenharmonics/DCsettling,reset/partition/automation/
stereoisolation andcallbackallocation/invalidlifecycle checks pass. TapeHFlevel
compressionratio.2881 vsLF.3968;TubeH2.07037 vsTape~7e-15 onrecordedfixture.
Theseestablish numericalcharacter/alias evidence,not physicalemulation or sonic
approval. Existingapplication routing/controls/latency unchanged inthiscore-only
increment. FullcombinednativeCIpending;actualmanualqualificationdeferred.

## Colour host integration verification — 2026-10-24

Integrated frozen engine/host work under the versioned aligned-engine contract.
Schema11 appends4fields,59state/58sound parameters,12groups. IndependentQA
verified55prior definitions byte-identical, all15factories neutral in9Space/Colour
fields, and no blocking source/test findings. IndependentO2boundaryprobe4588checks
passed; independent transitionoracle8800samples matched exactly, including early
56guard/128fade, Colour resets, gain/mix automation and interrupted transitions.

Lead product build passes. Presets0.29sec pass; firstpluginCTest hitsold60sec
budget without assertion output. Samebinary isolateddirect passes70.053secwall,
75.713secuser, with allassertions retained. Increased only hostsuite runtimebudget
to180sec for continuous8xColour andexpandedcases; not a DSPdeadline/numericwaiver.
Integrated Debugengine15490checks pass25.258sec (old60sec budget unchanged).
RealGUI58attachments/12groups passes; /tmp/darkvoice-colour.png inspected, all
sidebarlabels/controlbounds fit900x780 and latency56samples visible. Packaging
13fixtures andsecret-scanner selftest pass. FullsequentialRelease/ASan/product,
format/static/scans/native/normalmerge gates stillpending atthischeckpoint.
Manualhost/microphone/listening remains deferred/unrun peruserinstruction.

Isolated integrated ASan/UBSan engine15490checks pass92.355secwall/91.975user,
leak/halt enabled, without findings. Thus engine's former60sec CTest budget is
alsoinsufficient for thisincrement's actualcontinuous8xworkload andexpandedoracle;
increasedengine runtimebudget to180sec aftermeasurement, preservingassertions.
Prior SpaceReverb60sec budget remains historical, not a currentresult. Fullsuite
runs follow sequentially toavoid CPUcontention. Formatting/cppcheck/scans pass.

Integratedfullproduct20/20passes118.62sec (host64.22sec withinmeasured180budget),
Release17/17passes10.36sec. Sequentialfullsanitizersuite isrunning; independentQA
approvedbothmeasuredtimeoutadjustments. No assertions/numericgates reduced.

SequentialfullASan/UBSan17/17passes190.71sec withleak/halt enabled; everyfixture
andnumericalgate retained. Finalformat/static/scans alreadypass onintegratedsource.
ColourcorePR33 ismergedcd33a6a; appchanges hereawaitnativeCI/normalmerge. R3audit
PR34 merged5395776 afterbb64e41 all7native40098123157 andquality40098123175 passed;
its reproducedfailedqualitycriteria remain blocked, no productbackend introduced.
## VoiceSculptor tone core qualification — 2026-10-24

New independentcore supplies Body180Hz/Q.85bell, Warmth450Hzcomplementarytilt,
Presence3000Hz/Q.7bell, eachgain[-6,6]neutral0. Fixedpoles;10msdoubleweights;
normalneutralbits/signedzeroexact whilefilterhistoriescontinue. Noaddedlatency,
externaldependency, hostcontrols orpitch/formantclaim. Coreonly; dependentengine/
schema12/UIintegration requires separatecontract andgates. Other tonalparameters
remainoutstanding ratherthanbeing relabeledasthesecontrols.

StrictO2writer4241checks0failures; leadCMakeRelease1/1passes.82sec andASan/UBSan
1/1passes8.61sec withleak/halt enabled. Independentcomplexanalogtransfer,
trapezoidalstatevariable/closedexponentialautomationoracles, all4rates/layouts,
21settings×7frequencies, combinedextremes/densefrequencybound, signedzero/normal
extremes/reset/partition/invalidatomicity/allocation checks retained. Spectral
errorgate.02dB,complex2e-5,automation2e-7. Static+15dBfrequencybound isnot apeak/
automationguarantee. Truefloatoutputsubnormals anddoublesubnormalhistories/weights
clearwithout arbitraryepsilons. Negativecontrols removingonlyweightcleanup fail8
longdecaychecks; removingonlyoutputflush fails16activecut±FLTminchecks, while
priorchecks pass. Meaningfuldenormalregressions. Formatting/cppcheck pass.
IndependentreadonlysourceQAapproved; separateexecution/native/normalmergepending.
Manualqualification remainsdeferred/unrun. Atthischeckpoint ColourhostPR35 is
pendingnative; productcapability follows itsmergedrevision,not thislibraryalone.

IndependentQA actuallyrebuilt strictO2suite4241/0 andran224ownimpulsetransform
comparisons maxcomplexerror7.69278e-8<2e-7 plusstereoscaling. Approvedfinalcore;
lead Release/ASan alreadycompleted above. Trackedsecret/dependencyscans pass.

## VoiceSculptor tone host verification — 2026-10-24

Integrated frozen engine/host files under schema12 contract. Independent QA
rebuilt strictO2 engine:16104checks/0, all eight composed rate/layout oracles
maxerror0. Host retains all59prior definitions, appends3tonefields; exactzero-
centered conversion/snapping onlynewfields prevents FMA residual atnominalzero.
Core remains qualified; application uses62state/61sound fields and13groups here.

First fullproductrun passed20/21, failed new steady-state tone transfer fixture
at48kmono180Hz/body+6: error2.84806e-5 above retained2e-5 gate. Fixed2048sample
preroll was insufficient for mandatory20HzHP startup. Replaced onlyfixturepreroll
with ceil(.2*rate), kept2048fit/allcases/callbacks/2e-5 gate. IndependentQA reviewed
analytic decay andchange. FreshhostCTest passes81.07sec; previousother20tests
unchanged/pass. Earlytransients remain covered byall-sampleengineoracle.

GUI61attachments/13groups passes and /tmp/darkvoice-sculptor-tone.png inspected;
all labels fit900x780 and actual56samplelatency visible. Formatting/cppcheck/
whitespace pass. FullRelease/ASan, scans/native/normalmerge pending. Manualhost/
microphone/listening qualification remains deferred/unrun. ColourPR35 ismerged
daddba6 after41c270 native40098919922 all7jobs/quality40098919947; tonecorePR36
remains draft. No pitch/formant backend qualifies from this increment.

## Tone integration evidence recovery — 2026-10-26

Resumed workspace preserves completed CTest logs despite temporary output logs
being gone. build/flanger/Testing/Temporary/LastTest.log records18passed/0failed;
build/flanger-sanitized/Testing/Temporary/LastTest.log likewise18passed/0failed,
including VoiceSculptor4241 and engine16104 checks. Those runs finished on
October24; this is recovered evidence, not newly executed sanitizer validation.
The product suite is rerunning now for a complete current21-test report.

TonecorePR36 merged49ecce6 after exact925f1ea native40099826216 all7jobs and
quality40099826232 passed. Currentmain remains schema11/58soundcontrols/56sample
latency because dependenttonehost integration remainslocal pendingcommit/native.
Allmanualhost/microphone/listening qualification remains explicitlydeferred.

Current complete product rerun:21/21 passed217.34sec, host125.71sec within existing
180sec runtimebudget. No code/threshold adjustment was needed. Recovered18/18
Release and18/18ASan results remain the corresponding completed October24 runs;
GUI61controls/13groups and independent16104engine checks were already verified.
No manual qualification is claimed. Commit/native CI and normal merge follow.

Historical core integration checkpoint:

Colour host PR35 merged daddba6 after exact41c2707 passed all seven native jobs
40098919922 and source-quality40098919947. This tone-core branch now includes
that merged base; current application schema11/58sound/12groups/fixed56 latency.
The new tone library is still outside application routing until its own dependent
host increment passes. All existing Colour tests and numerical gates are retained.
