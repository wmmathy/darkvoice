# Direct user requirements added after the original baseline

Keep PRODUCT_SPECIFICATION.md verbatim. These later explicit user instructions
extend that baseline and take precedence where relevant.

## Vocal processor references and effect audition — 2026-11-15

The user requested additional features and effects found in leading vocal
processors, naming:

- TC Helicon VoiceLive 3 Extreme
- Boss VE-22 Vocal Performer
- Zoom V6 SP Vocal Processor

The user also requested a preview option for every effect, using a clean vocal
as input, with male/female selection.

### Required behavior

Every implemented effect, including future additions, must have an audible
preview. Offer clean male and female sample choices. Preserve the ongoing
Voice Sculptor work and its numerical, native and realtime-safety gates.

### Implementation interpretation and current work

Use the named processors as sources for a generic vocal-feature comparison and
backlog. Research actual features before claiming an exact correspondence. This
request does not require proprietary algorithms, vendor presets or demo audio.

Start by auditioning the effects that already run in DarkVoice. Keep unavailable
pitch/harmony features out of the playable catalog until their DSP is qualified.
Use independently rendered preview audio, with dry/effect comparison, so preview
selection does not mutate the live sound or its global quality setting. Playback
must use the host audio path, with bounded realtime work and unchanged raw-bypass
behavior; do not open a competing audio device inside a plugin.

Use natural recordings with verified redistribution licenses, published sample
metadata and retained credits. Label spoken material as speech if singing samples
are unavailable. Male/female refers to the selected published sample, not an
inference about the user or a physiological voice-transformation claim.

Exact worker/player/catalog interfaces and future DSP additions must be recorded
in COMPONENT_CONTRACTS.md before independent implementation assignments. Manual
microphone, listening and DAW testing remains deferred as previously requested.
