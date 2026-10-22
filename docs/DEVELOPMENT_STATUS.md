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

## Flanger continuation and current backlog — 2026-10-22

Lead established effects contract v6 and feature/effects/flanger directly from
chorus/width, independent of the experimental Sculptor branch. DSP agent wrote
only Flanger header/source/tests in isolated feature/effects/flanger-dsp worktree
and reported123 passing initial signal checks. Its final turn hit model capacity;
lead preserved that worktree, copied only the three assigned files, reviewed and
completed integration/formatting/Release/sanitizer tests directly. No agent commit
or completed independent final review is claimed. Agent files/worktree preserved.

Flanger uses a preallocated causal interpolated0.5–8.5ms sinusoidal delay, signed
feedback and smoothed controls, after chorus/width. Four real controls/seventh tab,
append-only AU hint6 and schema6 state/preset migration; factory sounds stay off.
No new dependency or host latency. Full Release and ASan/UBSan core12/12 pass;
Linux products15/15 and34-control GUI/tab/audio smoke pass. Initial legacy fixture
trimmed by child order instead of parameter ID; corrected ID-based fixture keeps
all assertions and migration checks. New native CI outcome remains pending.

PENDING_DEVELOPMENT.md now records remaining specification scope and review queue.
Preset search/favorites/import/export/rename/save-as already work; remaining preset
scope is complete factory coverage and actual DSP-dependent sounds. Latest PR #19
CI passed core/product jobs but retained its two experimental qualification failures;
no Sculptor quality fix, integration, merge, actual DAW/listening or release is claimed.

## Authorized automatic integration — 2026-10-22

User explicitly requested automatic development/commits/merges without confirmation.
GitHub reports main protected=false, branch rules=[] and rulesets=[]; no protection
change or bypass performed. Independent read-only merge_review agent reviewed
PR15/17/21, rebuilt209 de-esser/182 modulation/123 flanger standalone checks and
found no blocking source issue. It verified relevant de-esser/chorus sources are
unchanged in bfc9b54, whose native/core/product jobs passed. That is descendant
evidence, not a separate parent-head workflow run.

Normal head-SHA-matched merges completed: de-esser PR #15 at2f9e33d, chorus/width
PR #17 at9917fb1. Issues #14/#16 closed; branches/worktrees preserved. PR #21 and
blocked experimental PR #19 retargeted to main. Flanger source QA passes, but its
macOS jobs failed to acquire hosted runners and remain unqualified at this point.
A failed-jobs retry was requested; no failing source test is waived.

Official actions/runner-images README now labels macos-14 deprecated and lists
macos-15 as supported Arm64. Move build and manual unsigned-artifact workflow
labels to macos-15, preserving every test/auval gate and time budget. No artifact
workflow dispatched or release published. New CI outcomes pending.

## Native flanger neutral correction — 2026-10-22

macos-15 acquired runners and passed core/build jobs. Its product test exposed an
exact signed-feedback neutral migration assertion; auval remained skipped. Root
reproduced possible fused-rounding residual and centered only the new feedback
range/conversion/snap at zero. Existing migration thresholds/assertions retained;
new exact-neutral/symmetry regressions and local15/15 product suites pass. Native
retry on the corrected source is pending, so PR #21 stays unmerged for now.

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

## Factory catalogue continuation

Fourteen factory presets now use implemented DSP; the original six retain their
IDs and parameter snapshots. Eight additions cover Dark Wave, Deep Narrator,
Industrial Whisper, Grunge/Acoustic Baritone, Dark Melodic, Industrial Power and
Natural Dark Baritone. Six correction/choir/octave/double-track sounds remain
blocked by missing DSP. No schema, dependency or latency changes. Numerical
rendering does not establish listening qualification.
