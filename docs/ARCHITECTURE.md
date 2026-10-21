# Architecture

Status: initial foundation. The original product Master Prompt is absent.

Use a dependency-free C++20 planar audio core. A future JUCE adapter owns host
lifecycle, parameter snapshots, UI and plugin serialization. Workers own file I/O,
analysis and AI inference; they never run on the real-time callback.

Initial scope: a prepared mono/stereo gain processor and executable regression
tests. EQ, dynamics, pitch, harmony, AI, mixing, mastering, export and UI are planned.
Processing order, supported platforms, model licenses, latency budgets, musical
behavior and release acceptance criteria require the original product specification.

The lead approves shared contract changes before dependent implementation begins.
No background scheduler or persistent autonomous agent service is configured.
