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
| CMake | 3.31.6 in cloud tools | BSD-3-Clause; builds |
| Ninja | 1.11.1.4 Python package | Apache-2.0; build runner |

Inspected JUCE LICENSE.md and modules/juce_audio_processors/format_types/
VST3_SDK/LICENSE.txt before integration. JUCE includes additional third-party
notices (FreeType, HarfBuzz, image libraries, FLAC and others); retain the upstream
license/notice tree when redistributing. JUCE modules' commercial alternative is
not the selected route. VST marks have separate usage guidelines; no certification
or endorsement is claimed. Unsigned developer artifacts now retain the complete pinned JUCE source/notice
tree beside the exact project source, runtime bundles and project license; see
[DEVELOPER_BUILDS.md](DEVELOPER_BUILDS.md). Final redistribution compliance and
notice review remain release gates, not a completed legal audit.

Initial build disables JUCE browser/curl integration. There are no AI models,
model downloads, cloud processing services or MP3 encoder dependencies. Codec,
model, source-separation and SRC selections need separate open-source license
review before their implementation. Do not claim unavailable formats work.

macOS SDK/Xcode and GarageBand/Logic are platform/host validation prerequisites,
not bundled open-source dependencies. No Apple signing credentials are configured;
no signed, notarized or release-ready artifact is claimed.
