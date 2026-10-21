# Development orchestration

The lead coordinates Architecture, DSP, Pitch Correction, Harmony, Voice AI,
Mixing, Mastering, UI/UX, Integration, QA and Documentation responsibilities.
Native agents execute finite assigned tasks in the active session; there is no
persistent scheduler. Read-only reviewers can share a checkout. Every mutating
agent uses an isolated worktree and exclusive file ownership.

Before delegation record task, acceptance criteria, contract revision, branch,
owned files, dependencies and test command in DEVELOPMENT_STATUS.md. Shared-file
changes require a lead integration task. Never discard user work.

Branches: feature/<component>/<task>, fix/<component>/<issue>,
refactor/<component>/<task>, docs/<topic>. Main is the intended primary branch;
protection and review settings must be checked through GitHub before integration.
Commit logical tested units using Conventional Commits and explicit staged paths.
Push only to origin for wmmathy/darkvoice. Do not force-push or bypass reviews.
Create a PR for each package; merge only with passing CI, satisfied required
reviews and permitted protection rules. Empty repositories need a base branch
before GitHub can compare a PR; never report a PR that was not created.

GitHub issues are authoritative once API access is available. Until then the
local backlog is a pending synchronization record. Check for existing issues
before creating any; include owner, acceptance, priority, status and dependencies.
Release publication always requires explicit user authorization. A manually
triggered artifact build is not publication approval.

For every handoff preserve commit SHAs, commands/results, known failures and next
work in docs. Real macOS hosts are required for AU and DAW compatibility testing.
