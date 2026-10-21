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
