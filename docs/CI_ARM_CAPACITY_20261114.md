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

Both labels passed in run43634362047 on workflow commit
7e6a2fe0209c6092c9d284cdfa37d255e25050cc. Both executed on arm64:

| Label | Job | macOS | Xcode | Runner image |
| --- | --- | --- | --- | --- |
| macos-15 |132567789218|15.8.2 build24H232|16.4 build16F6|20261104.0514|
| macos-26 |132567789454|26.7.2 build25G332|26.6 build17F113|20261102.0473|

The actual downloaded run-log archive is preserved in the ignored
build/probe-evidence directory. These outcomes establish availability at probe
time, not a guarantee of future capacity or product compatibility.

Unrelated standard Build-and-tests run43634361899 was cancelled only on this
diagnostic branch. Actual feature validation runs were untouched. No production
labels changed. Since the existing macos-15 pool is available again, a single
retry of cancelled feature jobs is the least disruptive next step. The alternative
macos-26 ARM pool is also proven available if a later fallback is needed. Any
label still needs full native product/core/sanitizer/AU validation on the reviewed
feature head before normal integration; a probe is not product qualification.
