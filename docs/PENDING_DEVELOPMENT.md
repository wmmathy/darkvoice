# Pending development — 2026-10-24

Scope: authoritative PRODUCT_SPECIFICATION.md. This is an implementation backlog,
not a claim that the complete suite is delivered. Check DEVELOPMENT_STATUS.md and
TEST_RESULTS.md for revision-specific evidence. Public repository/default main are
verified; Actions executes again. Actual DAW/listening qualification remains unrun.
User postpones manual qualification until feature implementation is complete; it
must not pause independent development or be misreported as passed.

## Current foundation and review queue

Implemented: JUCE Vocal/Master/Studio shells; gain/HP/compression/starter saturation,
three-band EQ, legacy delay/reverb, gate/expander, de-esser, chorus/width, flanger,
final sample-peak limiter, stereo musical delay, simulated doubler and distinct
Room/Hall/Plate reverb; worker dry-pitch telemetry; real controls,
versioned state/presets and fifteen factory sounds. Preset search, favorites,
import/export, rename and save-as already work; they are not pending features.

De-esser (PR #15), chorus/width (PR #17) and flanger (PR #21) are now merged into
main after independent QA and relevant native/product validation. Experimental pitch/formant core and alignment utility
are in draft PR #19, outside products. The three original isolated-envelope failures
were corrected, but three combined-shift periodicity regressions still fail. Issue
#18 remains a quality/integration blocker. Do not merge around failing gates.

At bfc9b54 the latest Sculptor workflow39740257159 passed core/Linux-sanitizer and
both product jobs; its two experimental jobs failed the retained quality gate.
Source-quality run39740257019 passed. This is not a passing overall build result.

## Prioritized backlog

| Priority / tracking | Pending implementation | Dependency / acceptance |
| --- | --- | --- |
| P1 Sculptor, #3/#18 | Qualified independent pitch/formant shifting and formant preservation; depth/body/warmth/presence/resonance/breathiness/character controls; real quality modes | Resolve combined-periodicity failures, broader F0/transient/unvoiced material, listening/CPU; integrate delayed dry/mix/bypass and host latency only after qualification |
| P1 Integration, #14/#16/#18 | Review/integrate eventual qualified Sculptor and remaining effects; de-esser/chorus/flanger now merged | Required repository checks/review; no automatic bypass or release; preserve stacked bases until dependencies integrate |
| P2 Presets, #3 | Complete all20 required vocal factory sounds, audition/tune them and preserve correction/harmony independence | Underlying DSP must exist; fifteen sounds now use existing DSP; five correction/choir/octave sounds remain pending; do not add presets implying unavailable correction/choir features |
| P2 Track-aware correction, #4 | Reference WAV/AIFF/FLAC/MP3 import where available/licensed, key/scale/BPM/chord timeline/confidence and manual edits; manual/chromatic/custom correction, musical transitions/humanization/vibrato/drift; graphical tracking | Qualified shift backend and real-time pitch tracker; versioned analysis/timeline/transport contracts, preanalysis, offsets and GarageBand manual/standalone fallbacks |
| P2 Harmony, #4 | Up to six independent voices, interval/diatonic/chord/custom modes, per-voice pitch/formant/timing/detune/pan/level/EQ/sends and mixer | Qualified shifter, manual/timeline/MIDI selection as supported, CPU budget, phase/mono/transition/listening checks |
| P3 Discovery/DNA, #5 | Optional guided assessment or dry-recording import; measured range/stability/spectrum/breathiness/vibrato/dynamics/timing/articulation with confidence; local versioned profiles and history/export/delete; repeated-phrase comparisons | Dry analysis/profile contracts, consented local data and recordings, no medical/definitive voice-type claims; actual measured comparisons and loudness-matched auditions |
| P3 Coach/Auto Produce, #5 | Target/cents/stability/timing/dynamics feedback, explainable suggestions; editable Natural/Balanced/Produced/Aggressive automatic settings from DNA/reference/genre | Uncorrected dry analysis, workers for optional licensed local models, measurable DSP improvements, confidence/overrides and loudness-matched A/B |
| P3 Standalone sessions/Auto Mix, #6 | Microphone recording, backing/vocal import, multitrack editing/playback, project persistence; vocal balance/masking/dynamics/stereo/sends and complete mix render | Session/track/transport/file contracts, reversible edits, local sources, loudness-matched A/B; no stem control claim from stereo input |
| P3 Stereo mastering, #7 | Full-song loudness/LRA/true peak/crest/spectrum/correlation analysis; EQ/dynamic EQ, broadband/multiband/mid-side, transient shaping, saturation and oversampled true-peak limiter; editable Auto/Reference Master and ten presets | Separate complete-song path, measured latency/ceilings, whole-song analysis/render, loudness-matched A/B and listening; current Master shell is not this feature |
| P3 Export/QC, #7 | WAV/AIFF/FLAC/MP3 as licensed/available, PCM/float formats, SRC/dither, batch/names/metadata, streaming/CD/high-resolution/custom profiles and QC reports | Accurate LUFS/true peak, offline render/integrity, clipping/dropout/silence/imbalance/mono checks; distinguish delivery requirements from normalization recommendations |
| P4 UI/docs, #8 | Full navigation/waveforms/pitch/chords/harmony/LUFS/CPU/latency, Simple/Advanced journeys, accessibility and installation/user manuals | Actual underlying features, scalable AU/VST-sized views and standalone workflows; current grouped editor is an implemented subset |
| P4 Qualification/release, #9 | GarageBand/Logic/REAPER/Cubase as available; real microphone/vocals/listening, repeated target-machine performance, automation/state/transport/format/security checks; installers and packaging | Successful relevant CI is necessary but not a DAW/listening test. Signing/notarization require credentials; public release requires separate approval |

## Current integration and next work

Merged effects and factory increments include StereoDelay PR28 (04a0b404),
fractional-ring safety PR29 (0598ce915), Doubler PR30 (e61efc138), Double Tracked
Rock PR31 (d67a64c2), SpaceReverb PR32 (cc49c99) and Colour core PR33 (cd33a6a).
Each passed all seven native jobs and source-quality checks on its reviewed head;
exact run IDs and local evidence are in DEVELOPMENT_STATUS.md and TEST_RESULTS.md.

Colour host PR35 is merged at daddba6 after all seven native checks and
source-quality checks passed. The application uses schema11, 58 sound controls
plus global bypass, 12 groups and fixed56-sample latency. Tape/Tube parallel
colour, aligned raw bypass and defined limiter-protected resume are implemented.

VoiceSculptor body/warmth/presence core is merged in PR36. Its engine/schema12/UI
integration is PR38: local checks and Linux native checks pass, but macOS ARM
runner acquisition failed; one failed-job retry is in progress. The breath-texture
core is qualified locally in PR39; its schema13 engine/UI integration also passes
local checks and awaits commit/native integration. Neither is a pitch shifter.
Depth and resonance tonal-core work is active under a recorded v2 contract;
character and qualified pitch/formant processing remain outstanding. Product
capabilities follow merged main revisions, not unmerged branch implementations.

PR19 remains draft/blocked by three combined-periodicity failures; its optional
540-case diagnostic has 61 periodicity failures. Rubber Band LiveShifter audit
also fails one preserved-envelope case. No qualified pitch/formant replacement
is integrated. Backend evaluation must distinguish true octave-down results from
unchanged signals that also correlate at a multiple of their original period.
Independent tone shaping can proceed without claiming pitch/formant completion.

Five required factory sounds still depend on correction/choir/octave processing.
The fifteen implemented sounds and their persistence operations are available;
manual audition/tuning remains deferred together with host/microphone/listening
qualification until the other features are implemented, as requested by the user.

Automatic commits and normal merges are explicitly authorized. No unattended
scheduler or perpetual development agent is configured; GitHub CI and active
session work are distinct. Manual qualification remains unrun; public release,
signing and notarization are separate from development integration.

## Current Voice Sculptor priority — 2026-10-28

Tone PR38, texture core PR39, texture host PR40 and depth/resonance core PR42 now
merged with all seven native jobs/source quality green. The supported macOS15
ARM pool replaced unavailable26 capacity without waiving validation. Schema14
depth/resonance host is locally validated, pending committed-head CI/merge.
Remaining Sculptor work: Character, qualified independent pitch/formant, real
Live/Studio/Offline profiles and stronger absolute Colour alias suppression.
Bandlimited Colour profile implementation and isolated WORLD diagnostic active;
no production pitch controls until retained gates pass. Earlier pitch failures
and shared-filter reference limitation remain recorded. Manual tests deferred.
