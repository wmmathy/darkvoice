# DARKVOICE STUDIO — COMPLETE CODEX MASTER PROMPT

## 0. EXECUTION CONTRACT

You are the lead Codex engineering orchestrator responsible for delivering **DarkVoice Studio**, a working, locally operated AI-assisted vocal production, mixing, and mastering suite. **This document is the complete, self-contained product and execution specification.** Do not request an earlier prompt. Save this specification as `docs/PRODUCT_SPECIFICATION.md` and create `AGENTS.md` containing the repository's agent instructions. Start implementation, not just planning. Never claim functionality, parallel agents, tests, pushes, or host validation that did not actually occur.

Proceed autonomously through milestones with tested, buildable increments. Respect user approvals, GitHub permissions, branch protection, licensing, and environmental constraints. If a dependency blocks one workstream, document it and continue independent work. Do not silently omit requirements.

## 1. PRODUCT GOALS

Build three products from a shared C++20/JUCE codebase:

1. **DarkVoice Vocal**: AUv2 and VST3 real-time vocal plugin.
2. **DarkVoice Master**: AUv2 and VST3 stereo mastering plugin.
3. **DarkVoice Studio Standalone**: macOS application for recording/importing vocals and backing tracks, voice assessment, editing, harmony, automatic mixing, offline mastering, and exporting complete songs.

Primary environment: macOS Apple Silicon and GarageBand (AU, **not VST**); support Intel macOS where feasible, and Logic Pro, REAPER and Cubase where testable. GarageBand-specific host restrictions require documented standalone/import/manual-alignment fallbacks. Avoid assuming sidechain, MIDI routing, or transport position is always available. Never claim a host test passed without actually running it.

Target genres: dark new-wave, synth-pop, industrial metal, heavy metal, alternative rock, grunge, gothic and cinematic. Target user voice: natural bass/baritone. Sonic reference *characteristics* (not impersonation or cloning): intimate restrained dark synth baritone reminiscent of Depeche Mode/Dave Gahan; authoritative saturated industrial bass reminiscent of Rammstein/Till Lindemann; forward gritty powerful rock baritone reminiscent of Metallica/James Hetfield. Preserve the singer's own identity. Commercial factory preset labels must be generic, not artist-branded.

## 2. SHARED ARCHITECTURE AND ENGINEERING RULES

Use C++20, JUCE, CMake, modular testable DSP, and local ONNX Runtime/Core ML or equivalent only where genuinely useful. Evaluate and document licensing (JUCE, VST3 SDK, codecs, models, other libraries), build portability, and third-party dependencies before integrating them. Structure independent modules: AudioEngine, VoiceSculptor, FormantProcessor, PitchDetector, PitchCorrector, ReferenceAnalyzer, HarmonyEngine, Dynamics/Effects, VoiceDNA, VocalCoach, AutoVocalProducer, AutoMix, Mastering, LoudnessAnalyzer, Exporter, Presets, Parameters, PluginProcessor/Editor, StandaloneHost. Establish versioned component interfaces first.

Audio callback MUST avoid allocation, blocking locks, file/network access, GUI calls, and unbounded work. Use preallocation, lock-free message passing, bounded processing, smooth parameter changes, denormal protection, explicit gain staging, oversampling for nonlinear DSP where appropriate, and accurate host latency reporting. Support 44.1/48/88.2/96 kHz, variable buffers, mono/stereo, automation and DAW state restoration. Offer Live (low-latency), Studio and Offline Render quality modes. Measure CPU, actual algorithmic latency, glitches, and artifacts; do not promise zero latency.

## 3. VOICE SCULPTOR AND VOCAL FX

Implement functional, independently adjustable pitch shift (-12 to +12 semitones), formant shift, depth, warmth, presence, resonance, body, breathiness, harmonic saturation, character, and dry/wet. Preserve formants when changing pitch; preserve intelligibility, transients, consonants and expression. Include parametric EQ, high-pass, compressor, de-esser, gate/expander, limiter, soft clipping, plate/hall/room reverb, slapback/tempo-sync/stereo delay, chorus, flanger, doubler, stereo width, tape/tube-style saturation and parallel distortion. Provide bypass, sensible signal routing, automation, metering and presets. Use real DSP, not simulated knobs.

## 4. VOICE DISCOVERY, VOICE DNA AND FIND MY VOICE

Guide users through optional spoken sample, sustained notes, ascending/descending comfortable scales, melody, soft/strong phrases and alternate vocal expressions; also accept existing dry recordings. Measure F0, observed and estimated comfortable range, pitch stability, harmonic spectrum, spectral tilt/brightness, breathiness, resonance characteristics, vibrato, dynamics, timing, articulation and likely register transitions, with confidence estimates. Do not infer definitive voice type, vocal health or medical diagnosis from acoustics alone; avoid instructions to force a strained range.

Create persistent local, versioned, user-controllable **Voice DNA** profiles; store consented measurements, recordings/history references, preferences and approved DSP settings. Support multiple profiles, session comparisons, export/delete, and local-first privacy. **Find My Voice** compares repeated performances of the same phrase using measurable intonation, resonance, articulation, dynamics and consistency; audition loudness-matched dry/natural-enhanced/alternate character versions. Recommend several suitable vocal directions rather than a single supposedly objective 'hidden voice'.

## 5. AI VOCAL COACH AND AUTO VOCAL PRODUCER

Coach using the **uncorrected dry input**, not corrected output. Show current/target note, cents deviation, stability, timing vs track, dynamics, and reliable register observations. Give concise non-clinical suggestions; visual feedback must not block audio. Background/local AI inference must not run on the real-time thread. Prefer explainable acoustic rules where sufficient; ML must produce real measurable improvements, not placeholder text.

One-click **Auto Vocal Producer** takes Voice DNA, selected reference track, genre, desired character, and dry/live input; analyzes them and applies editable EQ, compression, de-essing, gating, saturation, formant, pitch correction, reverb, delay, harmonies and gain. Intensity modes: Natural, Balanced, Produced, Aggressive. Provide transparent before/after loudness-matched A/B, confidence and override controls.

## 6. LIVE PITCH CORRECTION FOLLOWING A SELECTED TRACK

**Non-negotiable:** while the user sings live into a microphone during playback of a selected backing track, correct pitch in real time to musically appropriate target notes from that track's harmonic context. Support manual key/scale (major, minor, pentatonic, chromatic, custom), strength, speed, transition smoothing, humanization, vibrato preservation, drift control, natural/robotic modes, manual target notes, graphical pitch tracking, voiced/unvoiced handling, and low-latency monitoring.

Reference Analyzer imports WAV, AIFF, FLAC and MP3 if licensed/available; estimates key, scale, BPM, time-varying chords and candidate notes with confidence and manual corrections. **Pre-analyze the reference track** and synchronize harmonic timeline to DAW playback when host position is available; allow offset calibration, manual alignment, and standalone playback synchronization. Optional real-time sidechain analysis only when routing exists. Provide fallback if GarageBand does not expose required host transport/sidechain features. Do not claim perfect chord detection. Correction must update as chords change, and all algorithms must be tested on actual signals.

## 7. HARMONY ENGINE

Generate up to six independent real-time harmony voices subject to CPU limits. Modes: unison doubling, octave up/down, third up/down, fifth up/down, diatonic/chord-aware and custom intervals. Per-voice enable, pitch, level, pan, formant, timing offset, detune, humanization, vibrato, EQ, reverb send. Harmony selection follows the analyzed reference chord timeline or manual/MIDI notes where host allows; GarageBand fallback uses manual controls. Provide a harmony mixer, phase/mono checks and natural-sounding transitions. Pitch correction and harmonies must remain independently configurable from vocal character presets.

## 8. VOCAL PRESETS

Ship working editable/savable factory presets: Dark Synth Baritone, Industrial Bass, Heavy Rock Baritone, Gothic Bass, Dark Wave, Cinematic Bass, Deep Narrator, Industrial Whisper, Grunge Baritone, Clean Studio Baritone, Dark Choir, Epic Metal Choir, Acoustic Baritone, Natural Pitch Correction, Hard Tune, Deep Octave Layer, Double Tracked Rock, Dark Melodic, Industrial Power, Natural Dark Baritone. Implement search/favorites/import/export/rename/save-as, versioning and migrations; never overwrite user presets. A preset must not unexpectedly disable live track-aware pitch correction.

## 9. AUTOMATIC MIXING

Standalone workflow imports one or more dry/processed vocal tracks, stereo instrumental, optional backing vocals and optional separate stems. Analyze vocal intelligibility, loudness, masking, spectral balance, dynamics and stereo/phase. **Auto Mix** applies editable vocal/backing balance, EQ/dynamic EQ, compression, stereo placement, reverb/delay and masking reduction; loudness-match A/B. If only a stereo instrumental exists, do not claim independent instrument control without actual source separation. Optional locally licensed source separation may be added with honest limitations. Export a complete stereo mix for mastering.

## 10. AI MASTERING OF THE FULL SONG

DarkVoice Master processes the **complete stereo song**, not just the vocal. Support real-time master-bus plugin and standalone offline mastering. Analyze integrated/short-term/momentary LUFS, LRA, sample/true peak, crest factor, dynamics, spectral balance, stereo correlation/width, phase, transients, low-frequency energy and clipping indicators. Implement functional minimum/linear-phase EQ as feasible, dynamic EQ, broadband/multiband compression, mid/side, imaging, saturation, transient shaping, oversampled true-peak limiter, proper gain staging and dither. Report latency.

One-click **Auto Master** analyzes whole song, chooses explainable adjustable settings for a selected genre, renders, and offers loudness-matched A/B and manual override. Presets: Dark Wave Master, Industrial Metal Master, Heavy Rock Master, Gothic Rock Master, Synth Pop Master, Cinematic Master, Acoustic Master, Electronic Master, Modern Pop Master, Natural Dynamic Master. **Reference Mastering** compares tonal balance, dynamics, width and loudness to a user-supplied reference without copying its audio. Do not confuse matching EQ with matching perceived sound.

## 11. EXPORT, RELEASE PROFILES AND QC

Export WAV, AIFF, FLAC and MP3 where supported/licensed, 16/24-bit PCM, 32-bit float where format supports, 44.1/48/96 kHz, high-quality SRC and appropriate dither on integer bit-depth reduction; preserve sources and support batch formats, names and metadata. Profiles: Spotify, Apple Music, YouTube Music, General Streaming, CD, High Resolution, Custom. Distinguish platform delivery requirements from nonmandatory loudness normalization recommendations. Configurable LUFS and true-peak ceilings. Automated QC: loudness, true peak, clipping, excessive limiting, dropouts, unintended silence, channel imbalance, phase/mono, format and integrity. Export report and warnings; never guarantee release quality without final human listening.

## 12. USER EXPERIENCE

Professional Retina/scalable dark charcoal/gunmetal UI with restrained red accents, clear labels, meters, pitch curves, chords, waveform, harmony mixer, LUFS and CPU/latency displays. Navigation: Dashboard, Voice Discovery, Vocal Coach, Vocal Producer, Pitch, Harmony, Reference Analyzer, Auto Mix, Master, Export, Presets, Settings. **Simple Mode**: choose profile, backing track, character, Auto Produce, pitch correction, harmonies, Auto Mix, Auto Master, Export. **Advanced Mode** exposes all parameters. Keep the AU/VST interfaces appropriate for plugin size; use standalone for complex workflows.

## 13. END-TO-END USER JOURNEY

Launch; record/import dry vocal; run Voice Discovery; save Voice DNA; compare recommended vocal directions; import instrumental; analyze chords/key/tempo; select vocal character; sing live with track-aware pitch correction; generate harmonies; Auto Produce; Auto Mix vocal with instrumental; Auto Master entire song; A/B; run release QC; export song and save editable project state. Every automatic stage must be optional, reversible and adjustable.

## 14. AUTONOMOUS MULTI-AGENT ORCHESTRATION

The lead Codex agent is **Orchestrator / Lead Architect**. Automatically delegate bounded work packages to native subagents where actually available. Required role specializations: Architecture, DSP/Effects, Pitch/Reference, Harmony, Voice AI/Coach, Mixing, Mastering/Export, UI/UX, Integration/Build, QA/Security, Documentation. Agents receive scoped task, acceptance tests, dependencies, interface contracts, assigned branch/worktree and allowed files. Parallelize only independent work. If spawning subagents is unsupported, perform sequential role-based work and record that limitation; do not pretend agents were launched.

Create `AGENTS.md` for shared coding, audio-thread, security, test, commit and ownership rules. Optionally create `agents/<role>.md` for task briefs; these documents alone do not launch agents. Maintain `docs/ARCHITECTURE.md`, `docs/COMPONENT_CONTRACTS.md`, `docs/DEVELOPMENT_STATUS.md`, `docs/TEST_RESULTS.md`, `CHANGELOG.md`, `README.md`. Track states accurately: planned, in-progress, implemented, tested, integrated, released. Record dependency graph and work package ownership. Integration agent owns compatibility; orchestrator resolves contract changes.

## 15. GITHUB SOURCE CONTROL — CONTINUOUS COMMITS

GitHub is the authoritative remote repository **once configured**. First inspect `git status`, remote URL, default branch, authentication, permissions, history, existing user changes and branch protections. Never discard uncommitted changes, overwrite unrelated work, create an unauthorized remote, expose credentials, or force-push protected branches.

Use `main` as protected integration branch. Feature branches `feature/<component>/<task>`, fixes `fix/<component>/<issue>`, docs `docs/<topic>`; parallel agents use **isolated git worktrees**. Create **frequent, meaningful Conventional Commits** after tested logical changes, not merely at phase end. Examples: `feat(pitch): implement track-aware correction`; `feat(ai): add voice DNA profile`; `feat(mastering): add true-peak limiter`; `test(harmony): validate chord-aware intervals`. Before each commit inspect diff, format, run relevant tests, secret-scan and stage only owned files. Do not blindly `git add .` or commit build outputs. Commit docs/tests with the related feature.

When authorized and authenticated, automatically push feature branches after meaningful commits. On failure, retain local commits, report status and retry only appropriately. Create PRs with linked issues, implementation notes, test evidence and architectural impacts. QA and Integration agents review. Auto-merge **only if repository policy allows, required checks and reviews pass, no conflicts remain, and no explicit approval is required**. Never bypass branch protections. Do not claim remote push/PR/merge succeeded without confirmation.

Create GitHub Issues and milestones for scoped features, bugs, integration work and release blockers; avoid duplicates. Update development status after milestones. If session ends, persist current state and next steps in tracked docs; **do not claim unattended background work unless a real persistent runner/scheduler exists**.

## 16. CI/CD AND RELEASES

Create GitHub Actions for CMake builds, unit/integration tests, DSP offline rendering/regression, static analysis/formatting, dependency/security/secret checks, macOS AU/VST3 build and validation, and versioned release packaging. Use appropriate macOS runners for AU validation. Host application tests (GarageBand/Logic) require a genuinely available host environment; report untested hosts honestly. Preserve CI logs and artifact summaries. Package signed/notarized macOS builds only when signing credentials and authorization exist; otherwise document unsigned developer build limitations. Never publish public releases without approval.

## 17. TEST PLAN AND QUALITY GATES

Use CTest and appropriate test frameworks. Build synthetic audio fixtures and legally usable vocal samples. Test pitch detection/correction accuracy, formant preservation, voiced/unvoiced behavior, chord/key estimation and confidence, harmony intervals/phase, Voice DNA persistence, AI recommendations controlling real DSP, Auto Mix gain/masking, LUFS/true peak and limiter ceiling, SRC/dither, export integrity, preset/state roundtrips, DAW automation, sample-rate/buffer changes, mono/stereo, real-time safety, thread/memory checks, CPU and measured latency. Use perceptual listening checks in addition to numeric metrics. Gate merges on build, relevant tests, no critical regression, documented contracts and repository protection rules. Do not silently replace DSP with stubs or mocked results.

## 18. DELIVERY MILESTONES

1. Repository inspection, license review, requirements persistence, shared contracts, JUCE/CMake AU/VST3/standalone skeleton with CI.
2. Functional Voice Sculptor, effects, metering, presets, DSP tests.
3. Pitch detector/corrector, reference track import/key/chord timeline, live synchronization/fallback, harmony voices.
4. Voice Discovery/Voice DNA/Find My Voice, coach, local inference and Auto Vocal Producer.
5. Standalone recording/editing/import, Auto Mix, optional stems.
6. Full stereo mastering DSP, Auto Master, reference comparison, release QC/export.
7. Full polished UI, accessibility, documentation, installation.
8. Integration/performance/security/host validation, packaging and approved release.

At each milestone: delegate, implement, test, commit, push, PR, integrate when allowed, and update status. Prefer working vertical slices over giant unverified code dumps. Do not ask for confirmation between ordinary tasks; ask only for essential permissions, credentials, license decisions or user-only actions.

## 19. DEFINITION OF DONE

Complete only when AU/VST3 and standalone builds are validated on supported environments; microphone and recorded vocals process with actual DSP; live track-aware correction works (with documented host fallback); six-voice harmony is functional within documented performance limits; voice profile and AI recommendations affect actual processing; Auto Mix produces a stereo song; mastering processes the entire stereo mix; exports/QC work; presets/project state persist; tests and host compatibility are accurately reported; source, docs, commits, PRs and approved release artifacts exist. Explicitly list incomplete/blocked items rather than claiming completion.

## 20. IMMEDIATE START INSTRUCTIONS

1. Read this document as the sole product specification; **do not ask for an earlier master prompt**.
2. Inspect environment and existing repository without overwriting changes.
3. Persist this specification at `docs/PRODUCT_SPECIFICATION.md` and create `AGENTS.md`.
4. Define interfaces and dependency graph; establish backlog/issues and agent assignments.
5. Create a buildable JUCE/CMake foundation and CI.
6. Begin iterative feature development with meaningful tested commits, pushes and PRs where authorized.
7. Continue through milestones autonomously within actual available execution capabilities.
8. Report real progress, tested functionality, blockers and next tasks accurately.

**Deliver a functioning, locally operated, professionally engineered DarkVoice Studio—not just an architecture or a mockup.**
