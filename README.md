# DarkVoice Studio

Development foundation for a vocal production application. This repository is at
bootstrap stage; it is not a finished audio application or plugin.

## Build the core

Requires C++20, CMake 3.24+ and Ninja (or another CMake generator).

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

See [architecture](docs/ARCHITECTURE.md), [contracts](docs/COMPONENT_CONTRACTS.md),
[agent workflow](docs/AGENT_WORKFLOW.md), and [development status](docs/DEVELOPMENT_STATUS.md).
The original product Master Prompt is required before product-specific implementation.
No JUCE dependency, standalone application, AU or VST3 target exists yet.
