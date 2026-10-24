"""Optional NumPy observations, not a product dependency or quality-pass test.

Read the bounded probe's little-endian float32 measurements from /tmp. Target
period correlation and harmonic-grid proximity alone cannot establish the
fundamental: the unchanged-70-Hz calibration demonstrates that octave ambiguity.
"""

from pathlib import Path

import numpy as np


SAMPLE_RATE = 48000
OUTPUT_FRAMES = 96000
FFT_FRAMES = 1048576
CASES = (
    (100, 600, 12, 0),
    (70, 600, -12, 0),
    (180, 1200, 12, 7),
    (110, 0, -12, 0),
    (110, 0, -7, 0),
    (110, 0, 7, 0),
    (110, 0, 12, 0),
)


def spectral_peak(signal):
    """Hann-windowed, zero-padded FFT peak with log-parabolic interpolation."""
    spectrum = np.abs(np.fft.rfft(signal * np.hanning(len(signal)), n=FFT_FRAMES))
    index = int(spectrum.argmax())
    if index == 0 or index == len(spectrum) - 1:
        raise ValueError("spectral maximum is outside the interpolable interior")
    values = np.log(np.maximum(spectrum[index - 1:index + 2], 1e-30))
    denominator = values[0] - 2 * values[1] + values[2]
    if not np.isfinite(denominator) or denominator == 0:
        raise ValueError("undefined spectral peak interpolation")
    delta = 0.5 * (values[0] - values[2]) / denominator
    return (index + delta) * SAMPLE_RATE / FFT_FRAMES


def correlation(signal, frequency):
    lag = round(SAMPLE_RATE / frequency)
    left, right = signal[lag:], signal[:-lag]
    denominator = np.sqrt(np.dot(left, left) * np.dot(right, right))
    if denominator <= 0 or not np.isfinite(denominator):
        raise ValueError("zero or nonfinite correlation energy")
    return float(np.dot(left, right) / denominator)


def harmonic_energy(signal, fundamental):
    """Energy in +/-2-Hz bins around the first 80 expected harmonics.

    These windows are a diagnostic, not an estimator of perceived pitch. The
    retained one-second region gives 1-Hz bins; all fixtures keep windows apart.
    """
    spectrum = np.abs(np.fft.rfft(signal * np.hanning(len(signal)))) ** 2
    odd = even = 0.0
    for harmonic in range(1, 81):
        index = round(harmonic * fundamental)
        if index + 2 >= len(spectrum):
            break
        energy = float(spectrum[max(0, index - 2):index + 3].sum())
        if harmonic % 2:
            odd += energy
        else:
            even += energy
    total = float(spectrum.sum())
    if total <= 0 or odd + even <= 0:
        raise ValueError("zero spectral or harmonic-grid energy")
    return odd / (odd + even), (odd + even) / total, odd / total


def main():
    for mode in ("off", "rt"):
        for index, (fundamental, center, pitch, formant) in enumerate(CASES):
            path = Path(f"/tmp/r3-{mode}-case-{index}.raw")
            signal = np.fromfile(path, dtype="<f4").astype(np.float64)
            if signal.size != OUTPUT_FRAMES or not np.isfinite(signal).all():
                raise ValueError(f"malformed or nonfinite measurement: {path}")
            signal = signal[-SAMPLE_RATE:]
            peak = spectral_peak(signal)
            target = fundamental * 2 ** (pitch / 12)
            envelope = center * 2 ** (formant / 12)
            odd_grid, captured, odd_total = harmonic_energy(signal, target)
            nearest_harmonic = round(peak / target)
            if nearest_harmonic <= 0:
                raise ValueError("peak below the first expected harmonic")
            print(
                mode, index,
                "peakHz", round(peak, 6),
                "envelopeErrorPct", round(100 * (peak / envelope - 1), 6) if center else "n/a",
                "targetCorr", round(correlation(signal, target), 9),
                "halfTargetCorr", round(correlation(signal, 2 * target), 9),
                "oddGridEnergyFraction", round(odd_grid, 9),
                "gridCapturedEnergy", round(captured, 9),
                "oddFractionTotal", round(odd_total, 9),
                "targetGridCents", round(1200 * np.log2(peak / (nearest_harmonic * target)), 6),
            )

    # Unchanged 70-Hz harmonics also lie on the 35-Hz grid. A high correlation at
    # the latter period must not be accepted as evidence of octave-down shifting.
    time = np.arange(SAMPLE_RATE) / SAMPLE_RATE
    control = np.zeros_like(time)
    for harmonic in range(1, 81):
        frequency = 70 * harmonic
        amplitude = 0.025 * (np.exp(-0.5 * ((frequency - 600) / 100) ** 2) + 0.002 / harmonic)
        control += amplitude * np.sin(2 * np.pi * frequency * time)
    _, _, odd_total = harmonic_energy(control, 35)
    print(
        "unchanged70_control_target35",
        "peakHz", round(spectral_peak(control), 6),
        "targetCorr", round(correlation(control, 35), 9),
        "oddFractionTotal", odd_total,
    )


if __name__ == "__main__":
    main()
