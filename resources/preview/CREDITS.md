# Preview recording credits and provenance

These are natural human recordings, not generated male/female voice conversions.
Published source metadata supplies the male/female labels. Spoken material is
labelled speech; the VocalSet material is sustained sung /a/, not spoken words.
The four original downloaded WAV files are retained byte for byte. Suggested
playback excerpts are metadata only; the runtime renderer may excerpt, apply
shared edge fades and process effects. It does not change these original files.

## English spoken samples — CMU ARCTIC

`male_speech.wav`: CMU ARCTIC BDL, published male speaker.
`female_speech.wav`: CMU ARCTIC SLT, published female speaker.

Copyright (c) 2003 Carnegie Mellon University. The recordings are supplied under
the permissive CMU ARCTIC voice licence, permitting commercial and other uses
subject to preserving its copyright, conditions, original authors and disclaimer,
and clearly marking modifications. The complete publisher notice is retained
unaltered in [CMU_ARCTIC_COPYING.txt](licenses/CMU_ARCTIC_COPYING.txt). Its English
section applies to these two files; its separate FLEURS section concerns other
recordings that are NOT bundled here. This is not an assertion that the audio
uses the mirror application's code licence.

Original source: http://www.festvox.org/cmu_arctic/
Suggested bibliographic credit: Kominek, J. and Black, A. W. (2004),
“The CMU Arctic speech databases.”

Pinned distribution mirror: JimLiu/baocut,
`bc57be6f9afde8d738e25d3c8438da0377efd041`,
`packages/models/assets/tts-voices/en-male.wav` and `en-female.wav`.
The publisher identifies each as original `arctic_a0001.wav` plus
`arctic_a0002.wav` from its BDL/SLT corpus. The publisher joined the two sentences
with 0.3 seconds silence, trimmed outer silence retaining 0.15 seconds padding,
and peak-normalized to -3.5 dBFS. It reports no speed or spoken-content change.
These prior modifications remain part of the attribution.

The suggested first-sentence excerpts end 150 ms inside the measured silent
separator: 54,077 frames for male and 56,002 for female at 16 kHz. The first
sentence is “Author of the danger trail, Philip Steels, etc.” Do not describe
these 16 kHz recordings as full-band 44.1/48 kHz capture after resampling.

## Singing samples — VocalSet

`male_singing.wav`: VocalSet singer m8, published male/bass,
original filename `m8_long_straight_a.wav`.
`female_singing.wav`: VocalSet singer f1, published female/soprano,
original filename `f1_long_straight_a.wav`.

Wilkins, J., Seetharaman, P., Wahl, A., & Pardo, B. (2018).
“VocalSet: A Singing Voice Dataset.” Proceedings of the 19th International
Society for Music Information Retrieval Conference (ISMIR 2018).

Recordings licensed Creative Commons Attribution 4.0 International (CC BY 4.0):
https://creativecommons.org/licenses/by/4.0/
Source record (v1.2): https://zenodo.org/records/1442513
Retain attribution, the licence link and an indication of excerpting, fades,
resampling or effects if material is adapted. The original recording notice,
singer metadata and mirror attribution are retained in `licenses/`.

Pinned distribution mirror: mblackman/vocalset-vowel-voicebanks,
`c48ab544e94e9788fec11be52b365026bb4a017c`, `source/vocalset/`.
These are full source-file recordings, NOT the mirror's cut, faded and
normalized 80 ms `voicebanks/` outputs. The mirror MIT code/arrangement licence
does not replace CC BY 4.0 for the recordings.

## Verification limits

The manifest records immutable source URLs, published labels, source/destination
SHA256, original PCM format and proposed cue bounds. No primary CMU archive byte
comparison was performed. Direct Zenodo and DataCite access was denied by the
research environment's network policy, so identity against the primary VocalSet
archive remains unverified. The pinned mirrors supply explicit recording
provenance and licence notices; no stronger primary-archive proof is claimed.
Objective PCM/hash/clipping checks are not manual listening qualification.
