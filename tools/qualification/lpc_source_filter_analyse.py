"""AGPL-3.0-only. Independent spectral diagnostics; no quality-pass exit status."""
from pathlib import Path
import numpy as np

RATE = 48000
COUNT = 96000
FFT = 1048576
CASES = [(200, 600), (200, 600), (35, 600), (35, 600),
         (360, 1200 * 2 ** (7 / 12)), (360, 1200 * 2 ** (7 / 12)),
         (35, 600), (35, 600), (35, 600)]
for index, (f0, target) in enumerate(CASES):
    raw = Path(f"/tmp/lpc-source-filter-{index}.f64").read_bytes()
    if len(raw) != COUNT * 8:
        raise ValueError("measurement length")
    x = np.frombuffer(raw, dtype="=f8")[COUNT // 2:]
    if not np.isfinite(x).all() or np.dot(x, x) <= 0:
        raise ValueError("invalid signal")
    w = np.hanning(len(x))
    padded = np.abs(np.fft.rfft(x * w, n=FFT))
    k = int(padded.argmax())
    if k <= 0 or k >= len(padded) - 1:
        raise ValueError("boundary peak")
    v = np.log(padded[k - 1:k + 2])
    offset = .5 * (v[0] - v[2]) / (v[0] - 2 * v[1] + v[2])
    peak = (k + offset) * RATE / FFT
    spectrum = np.abs(np.fft.rfft(x * w)) ** 2
    odd = even = 0.
    # Disjoint +/-2Hz bands around first80 intended harmonics, as prior probe.
    for h in range(1, 81):
        center = int(np.floor(h * f0 + .5))
        if center + 2 >= len(spectrum):
            break
        energy = float(spectrum[max(0, center - 2):center + 3].sum())
        if h % 2:
            odd += energy
        else:
            even += energy
    total = float(spectrum.sum())
    error = abs(peak / target - 1)
    print(f"row{index} independentPeakHz{peak:.9f} targetHz{target:.9f} "
          f"relativeError{error:.9f} envelopePass{int(error < .2)} "
          f"gridOverTotal{(odd+even)/total:.9f} oddOverTotal{odd/total:.9f}")
