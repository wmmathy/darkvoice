# Pending development — 2026-10-22

Scope: authoritative PRODUCT_SPECIFICATION.md. This is an implementation backlog,
not a claim that the complete suite is delivered. Check DEVELOPMENT_STATUS.md and
TEST_RESULTS.md for revision-specific evidence. Public repository/default main are
verified; Actions executes again. Actual DAW/listening qualification remains unrun.

## Current foundation and review queue

Implemented: JUCE Vocal/Master/Studio shells; gain/HP/compression/starter saturation,
three-band EQ, delay/reverb, gate/expander; worker dry-pitch telemetry; real controls,
versioned state/presets and six editable factory sounds. Preset search, favorites,
import/export, rename and save-as already work; they are not pending features.

De-esser (PR #15) and chorus/width (PR #17) are now merged into main after
independent QA and verified relevant native/product evidence. Experimental pitch/formant core and alignment utility
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
| P1 Effects, #3 | Flanger review/integration (implemented locally), doubler, plate/hall/room choices, slapback/tempo-sync/stereo delay variants, tape/tube saturation and parallel distortion, vocal limiter | Versioned real controls, routing, DSP evidence, safe automation, state/preset migration, GUI and native validation; existing starter effects are partial coverage |
| P1 Integration, #14/#16/#18 | Review/integrate flanger and eventual qualified Sculptor increments; de-esser/chorus now merged | Required repository checks/review; no automatic bypass or release; preserve stacked bases until dependencies integrate |
| P2 Presets, #3 | Complete all20 required vocal factory sounds, audition/tune them and preserve correction/harmony independence | Underlying DSP must exist; existing six sounds and library operations remain functional; do not add presets implying unavailable correction/choir features |
| P2 Track-aware correction, #4 | Reference WAV/AIFF/FLAC/MP3 import where available/licensed, key/scale/BPM/chord timeline/confidence and manual edits; manual/chromatic/custom correction, musical transitions/humanization/vibrato/drift; graphical tracking | Qualified shift backend and real-time pitch tracker; versioned analysis/timeline/transport contracts, preanalysis, offsets and GarageBand manual/standalone fallbacks |
| P2 Harmony, #4 | Up to six independent voices, interval/diatonic/chord/custom modes, per-voice pitch/formant/timing/detune/pan/level/EQ/sends and mixer | Qualified shifter, manual/timeline/MIDI selection as supported, CPU budget, phase/mono/transition/listening checks |
| P3 Discovery/DNA, #5 | Optional guided assessment or dry-recording import; measured range/stability/spectrum/breathiness/vibrato/dynamics/timing/articulation with confidence; local versioned profiles and history/export/delete; repeated-phrase comparisons | Dry analysis/profile contracts, consented local data and recordings, no medical/definitive voice-type claims; actual measured comparisons and loudness-matched auditions |
| P3 Coach/Auto Produce, #5 | Target/cents/stability/timing/dynamics feedback, explainable suggestions; editable Natural/Balanced/Produced/Aggressive automatic settings from DNA/reference/genre | Uncorrected dry analysis, workers for optional licensed local models, measurable DSP improvements, confidence/overrides and loudness-matched A/B |
| P3 Standalone sessions/Auto Mix, #6 | Microphone recording, backing/vocal import, multitrack editing/playback, project persistence; vocal balance/masking/dynamics/stereo/sends and complete mix render | Session/track/transport/file contracts, reversible edits, local sources, loudness-matched A/B; no stem control claim from stereo input |
| P3 Stereo mastering, #7 | Full-song loudness/LRA/true peak/crest/spectrum/correlation analysis; EQ/dynamic EQ, broadband/multiband/mid-side, transient shaping, saturation and oversampled true-peak limiter; editable Auto/Reference Master and ten presets | Separate complete-song path, measured latency/ceilings, whole-song analysis/render, loudness-matched A/B and listening; current Master shell is not this feature |
| P3 Export/QC, #7 | WAV/AIFF/FLAC/MP3 as licensed/available, PCM/float formats, SRC/dither, batch/names/metadata, streaming/CD/high-resolution/custom profiles and QC reports | Accurate LUFS/true peak, offline render/integrity, clipping/dropout/silence/imbalance/mono checks; distinguish delivery requirements from normalization recommendations |
| P4 UI/docs, #8 | Full navigation/waveforms/pitch/chords/harmony/LUFS/CPU/latency, Simple/Advanced journeys, accessibility and installation/user manuals | Actual underlying features, scalable AU/VST-sized views and standalone workflows; current grouped editor is an implemented subset |
| P4 Qualification/release, #9 | GarageBand/Logic/REAPER/Cubase as available; real microphone/vocals/listening, repeated target-machine performance, automation/state/transport/format/security checks; installers and packaging | Successful relevant CI is necessary but not a DAW/listening test. Signing/notarization require credentials; public release requires separate approval |

## Active bounded work

Flanger is implemented and locally validated end to end on feature/effects/flanger, stacked directly
on chorus/width PR #17. Four parameters, signed feedback and sinusoidal short delay,
seventh editor tab, append-only AU hints and state/preset schema6 migration. It does
not depend on or integrate the experimental Sculptor branch. DSP agent supplied its core/test files in an isolated worktree; lead completed all
shared integration and final validation. Release/ASan core12/12, Linux products
15/15 and34-control GUI smoke pass; native CI outcome is pending.

After this increment: review its local/native evidence and draft PR, then continue
independent effects while addressing the compensation-path quality blocker with
measured experiments. No unattended scheduler or perpetual background agent is
configured; the list persists the remaining work for subsequent sessions.

Automatic commits/normal merges are explicitly authorized. De-esser and chorus
are integrated; flanger awaits native runner validation, Sculptor remains blocked.
CI is moving from deprecated macos-14 to supported macos-15 Arm64 after runner
acquisition failures; no numerical or AU gate is removed.
