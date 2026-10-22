# Dependencies and distribution

User decision: AGPLv3 open-source distribution. User also requires open-source
choices for future dependencies. Project-authored source is AGPL-3.0-only; LICENSE
contains the full text retrieved from SPDX license-list-data, AGPL-3.0-only.txt.
Preserve this file and source availability when distributing developer artifacts.
No commercial JUCE license or proprietary AI service is assumed.

| Dependency | Pinned version | Selected license / purpose |
| --- | --- | --- |
| JUCE | 8.0.6, commit 51a8a6d7aeae7326956d747737ccf1575e61e209 | AGPLv3 option; audio/UI/host adapters |
| Bundled VST3 SDK | JUCE pinned tree | GPLv3 option; VST3 wrapper |
| Bundled AudioUnitSDK | JUCE pinned tree | Apache-2.0; Apple AU wrapper |
| Signalsmith Stretch | a670068d9aeb64913331d5cc29337b19a457a7df (1.3.2) | MIT; isolated pitch/formant qualification core |
| Signalsmith Linear | de55e6a50ffcf6f8f43f649692d94691c7025151 | MIT; pure C++ FFT/STFT for isolated sculptor core |
| CMake | 3.31.6 in cloud tools | BSD-3-Clause; builds |
| Ninja | 1.11.1.4 Python package | Apache-2.0; build runner |

Inspected JUCE LICENSE.md and modules/juce_audio_processors/format_types/
VST3_SDK/LICENSE.txt before integration. JUCE includes additional third-party
notices (FreeType, HarfBuzz, image libraries, FLAC and others); retain the upstream
license/notice tree when redistributing. JUCE modules' commercial alternative is
not the selected route. VST marks have separate usage guidelines; no certification
or endorsement is claimed. Final redistributable notice bundling remains a release
gate, not a completed legal audit.

Initial build disables JUCE browser/curl integration. There are no AI models,
model downloads, cloud processing services or MP3 encoder dependencies. Codec,
model, source-separation and SRC selections need separate open-source license
review before their implementation. Do not claim unavailable formats work.

macOS SDK/Xcode and GarageBand/Logic are platform/host validation prerequisites,
not bundled open-source dependencies. No Apple signing credentials are configured;
no signed, notarized or release-ready artifact is claimed.

Signalsmith core sources and MIT licenses were inspected before integration. Both
notices are retained in third_party/licenses. cmake/Signalsmith.cmake pins both
revisions, checks exact required header/license fingerprints and skips upstream
CMake entirely, avoiding mutable transitive tags and platform FFT defaults. It
copies only Stretch and Linear FFT/STFT into a private build directory. One guarded
patch disables the near-silence direct-copy shortcut; numerical noise floors
remain unchanged. Upstream checkouts remain untouched.

This is an isolated optional core target (DARKVOICE_BUILD_SCULPTOR_CORE), not yet
linked into Vocal/Master/Studio. No Accelerate/IPP/PFFFT/xsimd/CMSIS backend is
selected. Product controls and latency/bypass behavior are unchanged. The module's
streaming tests pass, but independent formant quality qualification fails; the
option defaults OFF. No live-correction or listening qualification follows from
adding this dependency.
