# Product backlog and dependency graph

Authoritative detail and definition of done: PRODUCT_SPECIFICATION.md. Work is
tracked through merged foundation PR #2 (issue #1 closed), with
milestones M2–M8 and issues #3–#9 for remaining work. No unstarted component is
represented as implemented.

| Milestone | Owners | Dependencies | Acceptance / specification coverage | State |
| --- | --- | --- | --- | --- |
| M1 Foundation | Architecture, Integration, DSP, UI, QA | requirements, licenses, v1 interfaces | Three JUCE products; real starter chain, parameter/state/GUI tests; native CI; AU validation evidence | Integrated via PR #2; issue #1 closed |
| M2 Sculptor/effects | DSP, UI, QA | M1, versioned effects/preset interfaces | Independent formant/pitch/character controls; EQ/dynamics/all listed FX; full editable factory presets and migrations (sections 3, 8) | In progress: PR #11 merged; linked gate/expander issue #12; issue #3 |
| M3 Track-aware pitch/harmony | Pitch/Reference, Harmony, QA | M1, timeline/transport/latency contracts | Reference import/key/BPM/chords/confidence/manual correction; live changing targets and fallbacks; six voices and mixer; signal accuracy/phase tests (6, 7) | In progress: dry pitch analysis foundation merged in PR #11; issue #4 |
| M4 Discovery/AI | Voice AI/Coach, DSP, UI | dry-analysis/profile contracts; M2/M3 | Optional safe assessments; versioned local DNA export/delete; repeat phrase comparisons; dry-input coaching; editable measurable Auto Produce (4, 5) | Planned, issue #5 |
| M5 Standalone sessions/mixing | Mixing, UI, Integration | session/track/import/export contracts; M2 | Recording/import/editing and persistent projects; actual vocal/instrumental Auto Mix, masking and loudness-matched A/B; stereo export (9, 13) | Planned, issue #6 |
| M6 Complete-song master/export | Mastering/Export, QA | loudness/offline/format contracts; M5 | Full stereo mastering, measured LUFS/true peak, oversampled limiter, Auto/Reference Master; SRC/dither, formats/profiles/batch/QC reports (10, 11) | Planned, issue #7 |
| M7 Complete user experience | UI/UX, Documentation | all feature interfaces | All navigation, Simple/Advanced workflows, scalable/accessibility, user/developer installation docs (12, 13) | Planned, issue #8 |
| M8 Qualification/packaging | QA/Security, Integration, Documentation | all prior acceptance | Host/microphone/listening/performance/security tests, unsigned or authorized signed/notarized packaging; approved publication only (16–19) | Planned, issue #9 |

Cross-cutting requirements (section 2): quality modes, denormal protection, bounded
real-time processing, smooth automation, accurate latency, all rates/layouts and
state restoration. Starter v1 establishes only an incremental subset. Before each
later package, establish exact versioned interfaces and measurable acceptance;
do not infer that a generic module list establishes them. No medical diagnosis,
artist cloning, perfect detection, independent stereo-stem control, zero-latency
suite claim or automatic release-quality guarantee is part of the implementation.

## Current ownership

Lead: shared contracts, integration, dependency setup, status and current issue #12.
Architecture/DSP agent: AudioEngine.cpp and engine_tests.cpp, completed/reviewed.
UI/host agent: processor/editor and plugin tests, completed/reviewed.
Build agent: CMake/product/CI targets, completed/static reviewed.
QA agent: independent review, state parser defect correction request, quality gates.
Initial package work used isolated worktrees; the lead now maintains integration
directly. Preserved worktrees retain preliminary copies. No persistent agent
scheduler is configured.
