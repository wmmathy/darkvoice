# macOS ARM capacity probe — 2026-11-14

The native validation runs for quality integration (43284105649, head c22d580)
and Character core (43287344040, head157d5b1) completed their Linux jobs, while
macOS15 jobs were cancelled before any step executed. GitHub annotations say:

> The job was not acquired by Runner of type hosted even after multiple attempts

and report capacity constraints for macOS arm64 runners. This is infrastructure
failure, not a passing product test or evidence of a DSP/compiler failure.

The official actions/runner-images README at immutable revision
95e6f51a068507e8f43d061d2166a39d0ef36ce8 lists both macos-15 and macos-26 as
supported ARM64 labels. The isolated push-only workflow tests both once, asserts
uname=arm64 and reports macOS/Xcode versions. It does not check out source,
install dependencies, access secrets or build products. Permissions are read only.
No production runner labels or qualification gates are changed by this probe.

Probe results are pending. Unrelated standard build CI triggered by this isolated
branch may be cancelled explicitly to avoid performing product validation on a
diagnostic-only change. Never cancel the actual feature validation runs. Any
healthy label needs full native product/core/sanitizer/AU validation on the
reviewed feature head before normal integration; a successful probe alone is
not product qualification. No repeated retries are planned if both pools fail.
