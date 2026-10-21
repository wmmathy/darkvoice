# Development status — 2026-10-21

Current phase: M1 application foundation implemented and validated on Linux;
remote product CI/macOS validation and integration gates outstanding.
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
verified by local regression. Final remote product validation remains outstanding. GitHub log downloads were denied
at results-receiver.actions.githubusercontent.com. That domain addition and complete
install/start instructions are saved in the environment draft, not proof of runtime
application/publication. Review/save in environment settings and publish when ready.
No secrets or API keys are needed for the current slice. No fresh-task restoration
or new snapshot publication is claimed.

## Remaining scope and next tasks

M1 still requires final remote product build/auval evidence and permitted review/
integration. Next independent development begins with versioned effects/presets
and pitch/reference/timeline contracts, then working vertical slices from ROADMAP.md.
The complete specification remains outstanding: pitch/formant sculpting, live
track-aware correction, six-voice harmony, Voice DNA/coach/Auto Produce, multitrack
record/import/editing, Auto Mix, full stereo mastering, QC/export and complete UI.
The Master shell is not functional full-song mastering. No true-peak/LUFS or AI
capability is represented by the current controls. Signing/notarization, actual
DAW/listening/CPU tests and release qualification remain unrun.
