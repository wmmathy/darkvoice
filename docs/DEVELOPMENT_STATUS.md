# Development status — 2026-10-21

Current phase: M1 application foundation implemented and validated locally on
Linux and by Linux/macOS product CI plus macOS auval. Repository review/integration
gates remain outstanding.
Requirements: docs/PRODUCT_SPECIFICATION.md (unchanged); AGENTS.md governs shared
instructions. User selected AGPLv3 distribution and open-source future dependencies.

## Implemented and locally tested

- v1 AudioEngine lifecycle/snapshots: smoothed input/output gain, HP, saturation,
  envelope compression, mix, bypass, input sanitization and peak meters.
- JUCE Vocal/Master plugin shells and Studio Standalone wrapper, dark scalable
  editor with seven actual controls and bypass, stable automation IDs and validated
  schema-1 state persistence. Studio currently hosts the starter vocal chain.
- CMake pinned JUCE, Linux VST3/standalone and conditional Apple AU targets;
  core/DSP/processor tests, source-quality and native-product workflows.
- Linux targets compile; seven CTest suites pass; engine has 111 signal checks.
  Release and sanitizers pass six suites. Real editor control smoke and standalone
  display startup pass. See TEST_RESULTS.md for exact scopes and limitations.
- Cloud installation helper and initialization scripts tested for repeatability;
  reusable install_script/start_skill saved in the environment draft.

## Actual agent work

Lead: contracts, dependency/license setup, integration, functional GUI checks,
GitHub issue/PR and documentation. DSP agent: engine and regression tests; host/UI
agent: processor/editor/state tests; build agent: CMake/products/CI; QA agent:
independent review, numeric-state-parser defect request and source-quality gates.
All mutating agents used distinct worktrees and file ownership. Their logical
commits are integrated into feature/application/foundation; original branches and
worktrees are preserved. Agents have completed finite assignments; there is no
persistent scheduler or continuous background development service.

## GitHub and environment

Issue #1, milestone M1 and draft PR #2 track this work. Milestones M2–M8 and
issues #3–#9 now track the remaining authoritative requirements without duplicates. Feature/package branches
were pushed. Main was created at the specification baseline without overwriting
existing refs. Changing the repository default branch and reading protection rules
returned HTTP403; default remains feature/integration/bootstrap unless changed
externally. No protection bypass, merge or release publication occurred.

Source-quality CI passed at ea7569d; all five remote core configurations passed
at 5a11233. macOS product compilation exposed unavailable floating from_chars;
replaced with portable classic-locale parsing and locale-invariant state save/load,
verified by local regression. At 796a409, source-quality/all core CI passed and
macOS native product builds, processor tests and both auval runs passed. Linux
Release product builds, seven tests and GUI smoke passed too. Both CI workflows
are green at source revision 796a409. Real DAW/microphone/listening tests remain unrun. GitHub log downloads were denied
at results-receiver.actions.githubusercontent.com. That domain addition and complete
install/start instructions are saved in the environment draft, not proof of runtime
application/publication. Review/save in environment settings and publish when ready.
No secrets or API keys are needed for the current slice. No fresh-task restoration
or new snapshot publication is claimed.

## Remaining scope and next tasks

M1 build/test evidence is recorded in TEST_RESULTS.md; permitted review/integration
is still required. PR #2 is ready for review, not merged. Next independent development begins with versioned effects/presets
and pitch/reference/timeline contracts, then working vertical slices from ROADMAP.md.
The complete specification remains outstanding: pitch/formant sculpting, live
track-aware correction, six-voice harmony, Voice DNA/coach/Auto Produce, multitrack
record/import/editing, Auto Mix, full stereo mastering, QC/export and complete UI.
The Master shell is not functional full-song mastering. No true-peak/LUFS or AI
capability is represented by the current controls. Signing/notarization, actual
DAW/listening/CPU tests and release qualification remain unrun.

## M2 active work packages

User reaffirmed automatic commits/pushes and continued feature development. PR #2
is confirmed merged into main at 2eb1f28 (externally); checkout is based on that
unchanged source tree. Main default-branch setting remains separate.

Shared v2 contracts/parameter schema established before assignment. Effects DSP
owns ToneEffects.cpp and effects_tests.cpp. Engine integration owns AudioEngine.cpp
and engine_tests.cpp (AudioEngine.hpp private details only, coordinate API changes).
Preset persistence owns PresetBank.hpp/.cpp and preset_tests.cpp (public API fixed).
Host/UI owns PluginProcessor/Editor and plugin_tests/gui_smoke. Build integration
owns CMake/workflows and review of dependency/cache setup. QA is read-only across
packages. Each uses an isolated feature branch/worktree. Lead owns contracts,
integration and handoff docs. Acceptance: numerical EQ/delay/reverb signal tests,
real controls, schema-v1 migration/v2 roundtrip, persistent preset operations,
actual preset changes in rendered audio, all-rate/layout and sanitizer coverage.


## M2 continuation — implemented and tested locally

Foundation PR #2 is confirmed merged into main (2eb1f28), and issue #1 is closed.
This branch adds three-band tone EQ, feedback delay/reverb, six actual factory
sounds and persistent user presets, a grouped 20-control editor, stable legacy AU
version hints, and state-v1 migration. Issue #10 tracks this subset of M2 (#3), not
the entire milestone. A worker-only YIN dry pitch analyzer supports later M3 work;
no live correction/reference synchronization or coach UI is represented by it.

Lead integrated the isolated agent work, corrected JSON primitive-key parsing,
version overflow/depth/trailing-input checks, bounded file reads and exclusive
writes, corrected legacy fixtures and completed the GUI/runtime validation.
All completed source changes are committed/pushed to feature/effects/presets-m2.
Local products build; 11 CTest suites pass; Release/sanitized core suites pass8/8.
GUI smoke exercises all20 controls, actual factory-preset audio and default storage;
standalone startup/screenshot pass. Scoped cppcheck and formatter pass. Native CI
for this revision is pending; previous milestone's evidence is not reused as a
passing claim for these changes. No public release is authorized/published.


Dry pitch telemetry now runs on a dedicated worker behind a bounded SPSC ring,
with actual note/frequency/confidence display. It adds no audio-path correction
or saved voice recordings. Local11/11 suites, GUI feedback and a focused
ThreadSanitizer harness pass. MacOS new-test failures in the initial CI revision
are under investigation; do not merge or reuse foundation AU evidence for this
increment. The lead continues direct integration from the preserved work packages.


## Current integration checkpoint

The macOS identity failures were isolated to zero-gain EQ cancellation rounding.
The fix uses identity coefficients for neutral bands and preserves the strict test.
After the fix: local products 11/11, Release core 8/8 and ASan/UBSan core 8/8
pass; the GUI/standalone display checks pass again. Native CI at 716c599 passes all seven jobs: Linux/macOS Debug and Release
core, Linux sanitizers, and both product jobs. Both unsigned development AU
validations and the Linux GUI smoke pass. PR #11 is ready for review; no merge
or completed review is claimed. Subsequent documentation commits rerun CI.
No M2/M3 completion or actual DAW/listening qualification is claimed.

## Linked gate/expander increment

PR #11 is confirmed merged at 5c41191; issue #10 is closed. New branch
feature/dynamics/gate-expander tracks issue #12 and partial M2 (#3) progress.
The lead established Dynamics contract v3 and implements this increment directly.
No additional agents were launched for this package; prior worktrees are preserved.

Add a linked downward expander before EQ/delay/reverb, with threshold/ratio/release
and a fourth editor tab (23 sound controls total). Ratio 1 is neutral. Append-only
AU hints preserve old identities; state/preset v3 migrate v1/v2 with gate disabled.
No new dependencies. Release and ASan/UBSan core suites pass 9/9 locally;
products pass 12/12, and the 23-control GUI/gate-tab/standalone checks pass.
At implementation b1a7196, native run 39661596101 passes all seven jobs, including
both product jobs, the Linux GUI smoke and both unsigned macOS AU validations.
Source-quality run 39661596167 passes. PR #13 is ready for review; documentation
updates trigger fresh CI, and no completed review or merge is claimed.
Full sculptor, correction/harmony,
AI/discovery, sessions/mixing, mastering/export and host qualification remain open.


## Actions pause and de-esser continuation

User reports exhausted GitHub Actions credits until further notice. Do not dispatch
or rerun workflows. New commits carry [skip ci]; local evidence does not establish
new macOS/AU validation. Keep PRs draft and preserve merge gates.

PR #13 is confirmed merged at b276d49. Lead owns implementation/integration on
feature/dynamics/de-esser. Two actual dedicated read-only agents completed
architecture and DSP review; neither edited files or ran Actions. The DSP review
identified tiny-normal neutral preservation; lead corrected it and added a fixture.
The next pitch/formant proposal is in PITCH_FORMANT_PLAN.md, not an implemented API.

Issue #14 tracks this bounded M2 package. De-esser v4 contract precedes implementation: linked high-band attenuation before
EQ/spatial effects, three real controls/fifth tab, appended AU IDs and strict
v1/v2/v3-to-v4 state/preset migration with neutral defaults. Release core passes
10/10; local products pass 13/13, ASan/UBSan core 10/10 and the de-esser-tab/
26-control GUI/standalone smoke pass. Final host migration assertions and all 13 product suites pass after read-only
QA strengthening. New native CI/AU validation is unrun. Full sculptor/pitch/
formants, remaining effects, reference/harmony, AI, sessions/mixing/mastering/export
and real-host/listening qualification remain outstanding. No new dependencies.


## Chorus/width continuation — stacked on PR #15

Main remains b276d49; draft de-esser PR #15 is unmerged. Branch
feature/effects/chorus-width builds on feature/dynamics/de-esser, preserving prior
work. Shared Effects v5 contract precedes implementation. Lead owns mutations;
dedicated read-only agents audit the future pitch backend and review modulation.
No Actions are dispatched; commits use [skip ci] and native validation is unrun.

Add 3–27ms interpolated stereo chorus and mid/side width after spatial effects,
four real controls/sixth tab, stable appended AU IDs and strict v1–v4 migration
to v5 with neutral new defaults. Issue #16 tracks this package. Release core and ASan/UBSan pass11/11, including
182 focused modulation checks. Local products pass14/14; the 30-control chorus/
width GUI and standalone smoke pass. Read-only QA identified a JSON double/float
minimum-boundary issue; the fix and boundary/overflow regressions pass. An initial
host fixture used an out-of-bounds magnitude-query overload; correction retains
JUCE assertions and the full suite now passes. The MIT candidate
audit found material latency/near-silence integration gates; see
PITCH_BACKEND_AUDIT.md. No pitch/formant processor has been integrated.


## 2026-10-22 — experimental pitch/formant streaming core

Branch feature/sculptor/pitch-formant-core is stacked on unmerged chorus/width
PR #17. Issue #18 blocks host integration. The lead owns mutations; dedicated
read-only agents reproduced backend pitch/formant failures and reviewed wrapper,
source-integrity guards and qualification fixtures. Actions remains paused and
all commits use [skip ci]. No merge/release or new native qualification is claimed.

Implemented an isolated PImpl wrapper with fixed delayed neutral output, bounded
64-sample scratch/control grid, deterministic seeded backend, parameter smoothing
and crossfade. Pin MIT Stretch/Linear, retain notices and fingerprint required
headers/licenses. A private-copy patch removes the immediate near-silence shortcut
without changing numerical floors or upstream checkouts. Default products do not
link this target; DARKVOICE_BUILD_SCULPTOR_CORE defaults OFF.

Existing products pass14/14. Experimental Release matrix is12/13 suites: streaming
209checks passes; qualification76checks has3 real formant-envelope failures.
ASan/UBSan streaming passes; qualification reproduces the same3 numerical failures
with no reported sanitizer findings. Studio sine pitch fixtures pass all16 tested
rate/shift combinations within20cents after increasing its FFT size. Original
small-window errors were real, not an estimator artifact. Geometric search ranges
correct an initial multi-formant test defect; isolated cases still fail.

Do not expose pitch/formant controls or mark the quality milestone complete.
Resolve/replace the backend envelope algorithm, then qualify further material,
CPU/listening and aligned host dry/mix/bypass. Reproduce with
PITCH_FORMANT_CORE.md; failing qualification is not skipped or WILL_FAIL.

## Voice Sculptor automation continuation — 2026-10-22

Lead extended the isolated core regression coverage on the existing draft PR #19;
no new agents or product controls were added in this increment. All12 rate/profile
stereo combinations now exercise sample-positioned pitch/formant/preservation/base
automation, opposite extremes and return to exact neutral. Streaming now passes
269checks in Release and ASan/UBSan. Full experimental Release remains12/13 suites
with the same3 formant qualification failures. Issue #18 remains open and host
integration remains blocked. Actions stays paused.

## Actions retry authorized — 2026-10-22

User authorized trying GitHub Actions again. The lead lifted the agent pause rule
and triggers existing build/test and source-quality workflows on this branch.
Results are pending; no available credits or new native success is assumed.
Experimental Sculptor remains OFF in existing CI; its three local formant failures
continue to block integration even if default product CI passes. Draft PRs and
merge/review gates remain in force.

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

## Sculptor CPU probe continuation — 2026-10-22

Lead added an opt-in developer CPU wall-time probe with reproducible synthetic
workload, JSON output and24 rate/profile/block combinations; no product changes.
The read-only formant_investigation agent independently investigates the remaining
envelope failures in temporary copies only. Benchmark runs locally and reports
actual observed callback-budget overruns rather than certifying real-time safety.
Actions remains billing-blocked; all new commits use [skip ci].

## Formant estimator correction and public-repository CI — 2026-10-22

GitHub confirms public visibility and default branch main. User reauthorized Actions;
normal CI triggering resumes. Historical billing failures remain historical only.
Lead integrated a guarded private-header estimator correction proposed/tested by
read-only formant_investigation agent. Preallocated scratch and compensated/rebased
linear-energy sliding mean replace morphological dilation/erosion, retaining F0
units and existing numerical floors. The three retained formant failures now pass
without changed thresholds. Expanded qualification140checks and streaming269checks
pass in the local13/13 Release run. New isolated Linux/macOS CI jobs exercise these
gates; remote results are pending. Existing product links/defaults remain unchanged.
Issue #18 stays open for broader material/listening and aligned host integration.

Current correction validation also passes full ASan/UBSan13/13 (no reported
findings) and the unchanged Linux product regression14/14. Formatter19.1.7 and
scoped cppcheck2.17.1 on wrapper/benchmark pass; limited credential/dependency
scans pass. No new GUI, actual DAW or listening qualification is claimed.
