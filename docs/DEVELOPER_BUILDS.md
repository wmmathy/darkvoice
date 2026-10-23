# Unsigned developer builds

These are development products for testing. They are unsigned and not notarized;
no public release or GarageBand/Logic compatibility or listening qualification is
claimed. DarkVoice Master is still a development shell, and pitch/correction,
harmony, full mastering/export and other specification work remain incomplete.
Do not replace working project copies or rely on these builds for a final delivery.

The manually dispatched **Prepare unsigned development artifacts** workflow builds
and tests the exact selected commit on Linux and macOS ARM. It never publishes a
GitHub release or invokes signing/notarization. Download the platform artifact from
that workflow's completed run. A successful packaging run does not establish DAW
compatibility. Keep the source, license and notice files with any redistributed
copy; final redistribution/legal and host qualification remain release gates.

Each platform artifact contains:

- `darkvoice-unsigned-products.tar.gz`: runtime VST3 bundles, Studio standalone,
  and AU bundles on macOS; no static build intermediates.
- `darkvoice-source.tar.gz`: tracked project source at `COMMIT.txt`, plus the full
  pinned JUCE 8.0.6 source under `darkvoice-source/dependencies/JUCE`. Bundled SDKs,
  headers and upstream license/notice trees are retained, not only download links.
- `LICENSE`, `DEPENDENCIES.md`, this guide, `BUILD_STATUS.txt`, build/source manifest
  and `SHA256SUMS.txt`. The manifest records builder architecture, platform and both pins; it does not
  certify Intel or universal-binary compatibility.

Verify the SHA256 checksums before unpacking. On macOS use
`shasum -a 256 -c SHA256SUMS.txt`; on Linux use `sha256sum -c SHA256SUMS.txt`.
Extract archives into a new empty directory. Product bundles are under `products/`.
There is no installer and nothing is automatically copied into host plugin folders.
The operating system may prevent unsigned applications from opening; no automatic
security-setting changes are made by the packaging workflow.

## Build from the bundled source

Install CMake 3.24 or later and a C++20 compiler; macOS requires Xcode/platform SDKs.
Linux needs the audio/window development packages listed in `docs/CLOUD_SETUP.md`.
Then, inside the extracted `darkvoice-source` directory:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  -DDARKVOICE_BUILD_PRODUCTS=ON \
  -DDARKVOICE_JUCE_SOURCE_DIR="$PWD/dependencies/JUCE"
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure --no-tests=error
```

No JUCE network fetch is needed with that explicit source path. macOS builds AU,
VST3 and Studio standalone; Linux builds VST3 and standalone. Apple SDKs and host
applications are external prerequisites and are not included in the source bundle.
JUCE uses its AGPLv3 route; the bundled VST3 SDK uses its GPLv3 option. See
`docs/DEPENDENCIES.md`, project `LICENSE`, and the complete upstream JUCE notice tree.

## Local artifact preparation

Artifact preparation requires Python 3.11 or later (standard library only).
Use clean Git checkouts of the project and the exact JUCE pin documented in
`docs/DEPENDENCIES.md`. Configure the build with explicit `DARKVOICE_JUCE_SOURCE_DIR`,
build and run tests. Only after that successful build/test sequence, record the
source commit and package:

```sh
git rev-parse HEAD > build/DARKVOICE_BUILD_COMMIT.txt
python3 tools/package-development.py --source . --build build \
  --juce-source /absolute/path/to/pinned/JUCE --platform linux \
  --output build/artifacts --expected-commit "$(git rev-parse HEAD)"
```

Use `--platform macos` for the macOS product layout. The output directory must not
already exist. The script checks pins, clean source, configured build paths, runtime
products and the post-test commit stamp. That stamp is operator/build-workflow
provenance, not a cryptographic proof of binary reproducibility. The script does
not access the network and does not read credentials or package Git metadata,
environment files, unrelated worktrees or untracked dependency additions.

## macOS manual testing

Use a test project and preserve any existing DarkVoice plugin bundles. GarageBand
uses the AU `.component` bundles, not VST3. With the host closed, place the desired
components in `~/Library/Audio/Plug-Ins/Components/`, enable Audio Units in the host
and reopen it. For VST3-capable hosts use `~/Library/Audio/Plug-Ins/VST3/`. Studio
standalone is the `.app` bundle. These are manual development-test locations;
unsigned code may require approval through macOS's normal security interface.

Check microphone monitoring, parameter automation, preset save/load and project
state restoration at your supported device rates. For the limiter, enable it,
set a ceiling, raise output gain and verify the plugin output peak stays at that
sample ceiling; repeat at dry/wet zero, then confirm global bypass restores the
original signal. Compare the fourteen presets using your own vocal material.
Report host/OS/device rate, mono/stereo layout and exact build commit with results.
AU validation on CI does not replace this host/listening test.
