# Cloud development setup

This task already has an isolated checkout at /workspace/darkvoice. Use it;
create additional worktrees only for explicitly assigned parallel development.

On the Debian cloud image, run `bash tools/cloud-setup.sh`, then
`source tools/cloud-env.sh`. Dependencies live outside the checkout in
/workspace/.darkvoice-tools, /workspace/dependencies/JUCE and /workspace/sysroot.
APT uses signed Debian metadata and HTTPS; no TLS/signature checks are disabled.
The helper preserves existing JUCE changes by refusing a dirty or mismatched tree.

```sh
cd /workspace/darkvoice
source tools/cloud-env.sh
cmake -S . -B build/products -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DDARKVOICE_BUILD_PRODUCTS=ON \
  -DDARKVOICE_JUCE_SOURCE_DIR=/workspace/dependencies/JUCE
cmake --build build/products --parallel 2
ctest --test-dir build/products --output-on-failure --no-tests=error
```

For a display-backed smoke check, start /workspace/sysroot/usr/bin/Xvfb on a free
display with a 1200x900x24 screen, set DISPLAY for that command, and run
`build/products/darkvoice_gui_smoke /tmp/darkvoice-ui.png`. It creates the real
editor, exercises slider/bypass attachments and saves a rendered snapshot.
A virtual display does not provide a microphone or audible monitoring. Actual
standalone startup and DSP/adapter tests supply different evidence; report each.
Stop only processes you started. Live processes do not survive cloud snapshots.

Required destinations: github.com, api.github.com, package registries and Debian
package mirrors. No product API keys, AI models or signing credentials are needed
for the current starter chain. Linux does not substantiate AU/Apple Silicon/DAW
compatibility; macOS CI and actual host tests must provide that evidence.

The setup helper also retains clang-format 19.1.7 and cppcheck. The relocated Debian
cppcheck binary finds its standard library config when invoked from
/workspace/sysroot/usr/lib/x86_64-linux-gnu/cppcheck/cfg; use absolute project paths.
Set XDG_CONFIG_HOME=/workspace/.darkvoice-user-config and
XDG_CACHE_HOME=/workspace/.darkvoice-user-cache (create those directories) before
GUI smoke/standalone startup to keep user configuration/cache in writable storage.
