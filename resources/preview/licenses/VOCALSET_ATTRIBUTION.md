# Attribution & provenance

## Original recordings

The vowel recordings in `source/vocalset/` are a subset of **VocalSet**, a singing
voice dataset:

> Wilkins, J., Seetharaman, P., Wahl, A., & Pardo, B. (2018).
> *VocalSet: A Singing Voice Dataset.*
> Proceedings of the 19th International Society for Music Information Retrieval
> Conference (ISMIR 2018).

- Source record (v1.2): https://zenodo.org/records/1442513
- **License: Creative Commons Attribution 4.0 International (CC BY 4.0)** —
  https://creativecommons.org/licenses/by/4.0/

VocalSet contains 10.1 hours of monophonic recordings of 20 professional singers
(9 female, 11 male) performing a range of vocal techniques on the five vowels
`a e i o u`. This repository uses only the **"long straight"** tokens — single
sustained tones with no vibrato — which are the cleanest steady-state vowels in
the set. The singer voice-type notes shipped with the dataset are preserved in
[`vocalset/README-vocalset.txt`](vocalset/README-vocalset.txt), and the license
in [`vocalset/LICENSE-vocalset.txt`](vocalset/LICENSE-vocalset.txt).

## What this repository adds

- `make_voicebanks.py` — finds the steady state of each sustained tone, cuts an
  80 ms clip, applies micro-fades, and peak-normalizes it (see the repo README).
- `voicebanks/` — the resulting processed clips (`female/`, `male/`).
- `index.html` + `manifest.json` — a browser preview of the processed clips.

These additions are released under the MIT License (see the top-level `LICENSE`).
The underlying recordings remain under **CC BY 4.0**; because the clips here are
modified, attribution must note that changes were made — the repository README,
`manifest.json`, and preview page all carry the required credit and license link.
