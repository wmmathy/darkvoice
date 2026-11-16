# Harmonic Character checkpoint — 2026-11-12

This checkpoint preserves the standalone Character DSP and tests. It does not
add application routing, a host parameter, GUI controls or a CMake target. Those
changes remain the dependent integration task. No pitch/formant processing or
physiological voice model is implied.

## Algorithm and dependency

`HarmonicCharacter` replaces its input with an additive harmonic contribution.
Negative amount adds an odd contribution, positive amount adds an even
contribution; zero settles to exact positive-zero output while histories track.
Both branches use guarded 16× resampling, a native 10Hz DC blocker and independently
smoothed weights. The contribution must be summed in parallel with Colour by
the eventual engine; serial routing would add an unwanted second delay.

Prepared Live, Studio and Offline profiles have actual delays of 124, 184 and
368 samples. The three copied dependency files are byte-identical to Colour
commit `5946de2218a55131e20db8a678fa026bf39dc616`:

- `include/darkvoice/BandlimitedColour.hpp`
- `include/darkvoice/QualityMode.hpp`
- `src/BandlimitedColourOversampling.hpp`

These dependencies are included to preserve this isolated checkpoint. Their
CMake and CI integration belongs to the Colour dependency branch.

## Executed validation

The saved final sparse-helper fixed numerical suites each passed **34,506 checks,
zero failures**: Live 101.121s, Studio 167.453s and Offline 320.304s in that cloud
run. Each includes 96 independent 32× reference cases and 288 fixed absolute-alias
fixtures, retaining the declared -55dB/input-RMS and reference-improvement gates.
All four supported rates and the declared model, amplitude and amount combinations
were retained. These are fixture-scoped results, not a universal alias bound.

The saved safety run passed 98,757 checks across all profiles. A fresh C++20 strict
O2 build and safety rerun on 2026-11-12 again passed **98,757 checks, zero failures**:
Live 5.319s, Studio 8.691s and Offline 17.547s. This rerun exercised lifecycle,
partition, automation, allocation, denormal, invalid-input and finite-extreme
checks; it is not a fresh rerun of the full numerical matrix.

Ten malformed or conflicting CLI invocations returned usage status 2. A fresh
Live single-bin scan at bin1724 passed eight checks/two cases, with worst measured
alias -115.027dB/input RMS. This is a targeted reproduction, not a complete scan.
Clang-format19, cppcheck and whitespace checks passed. Credential/dependency
scans are performed on the explicitly staged checkpoint paths before commit.

Current source SHA256 hashes match the prior frozen writer handoff:

```text
HarmonicCharacter.hpp  4ef71c25c2d8dd74332b227ff7904a773e515a561b5e8ec77ba4c87ea1ace379
HarmonicCharacter.cpp  9af38577e0597f6351b22c1d62c2ecdab74a28032fe29b496a437279ce1ed8e6
harmonic_character_tests.cpp  d182b7acea8755fc65a5aba1b90e00eadd06112ce75715a5b4c21c01527241ff
```

Saved test evidence is under ignored `build/qualification/character-v4-sparse-*`
and the fresh checkpoint evidence under `build/qualification/character-checkpoint/`.
No generated binaries, raw audio or build outputs are committed.

## Remaining gates

- Complete dense scan: every integer bin17–2031, both signs, all three profiles
  (4,030 cases per profile), using disjoint deterministic CI shards.
- Independent review, sanitizer runs and native CI for this Character module.
- Engine parallel routing, host/state/preset and GUI integration, including
  aligned dry/bypass/resume and final limiter behavior.
- Target-machine CPU/deadline measurements. Cloud wall times do not establish
  realtime suitability, especially at high rates and Offline quality.
- Manual host, microphone and listening tests remain deferred by user request.

The delta is not bounded to 0.25 by the scale coefficient: FIR/DC transient
behavior can exceed it. The tests derive conservative profile bounds and measure
actual finite-extreme peaks; no false 0.25 peak guarantee is claimed.

This isolated checkpoint uses `[skip ci]` because its old CMake base does not yet
register Character. The lead must integrate the qualified Colour dependency and
add the relevant numerical, safety and dense-scan CI gates before product merge.
