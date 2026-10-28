"""Pure-tone public-output folded-energy audit, not a universal alias classifier.

Rectangular transforms are intentional: each float input period repeats exactly.
Only true integer harmonics below native Nyquist are allowed. DC is excluded;
Nyquist has its correct single-sided weight and is also reported separately.
No oversampled reference, shared reconstruction filter or fitted fundamental
subtraction enters the metric. Float-input and output rounding remain included.
"""

import argparse
import csv
import math
from pathlib import Path
import re
import sys

import numpy as np

FRAMES = 4096
RATES = (44100, 48000, 88200, 96000)
AMPLITUDES = (.15, .5)
BINS = (719, 1597)


def load(path):
    raw = path.read_bytes()
    if len(raw) != FRAMES * 4:
        raise ValueError(f"wrong float32 measurement length: {path}")
    signal = np.frombuffer(raw, dtype="<f4").astype(np.float64)
    if not np.isfinite(signal).all() or not float(np.dot(signal, signal)) > 0:
        raise ValueError(f"nonfinite or zero-energy measurement: {path}")
    return signal


def db_power(power, reference):
    return 10 * math.log10(power / reference) if power > 0 else -math.inf


def powers(signal):
    spectrum = np.fft.rfft(signal) / FRAMES
    power = np.abs(spectrum) ** 2
    power[1:-1] *= 2
    if not np.isclose(float(power.sum()), float(np.mean(signal ** 2)), rtol=1e-12, atol=0):
        raise ValueError("Parseval normalization mismatch")
    return power


def analyse(directory):
    with (directory / "metadata.csv").open(newline="") as stream:
        metadata = list(csv.DictReader(stream))
    expected = [(rate, model, amp, bin_number) for rate in RATES for model in (0, 1)
                for amp in AMPLITUDES for bin_number in BINS]
    if len(metadata) != len(expected):
        raise ValueError("audit requires all32 predeclared fixtures")
    revisions = set()
    results = []
    for index, (row, settings) in enumerate(zip(metadata, expected)):
        rate, model, amplitude, bin_number = settings
        warm = math.ceil(rate / FRAMES) * FRAMES
        if (int(row["case"]), int(row["rate"]), int(row["model"]),
                float(row["amplitude"]), int(row["bin"]), int(row["warm_frames"]), int(row["frames"])) != (
                index, rate, model, amplitude, bin_number, warm, FRAMES):
            raise ValueError(f"unexpected settings or fixture order: case{index}")
        revision = row["source_revision"]
        if not re.fullmatch(r"[0-9a-f]{40}", revision):
            raise ValueError("compile the probe with an explicit40-character source revision")
        revisions.add(revision)
        source = load(directory / f"case-{index}-input.f32")
        output = load(directory / f"case-{index}-output.f32")
        input_power = float(np.mean(source ** 2))
        if not math.isclose(math.sqrt(input_power), float(row["input_rms"]), rel_tol=1e-12):
            raise ValueError("input RMS metadata mismatch")
        if not math.isclose(math.sqrt(float(np.mean(output ** 2))), float(row["output_rms"]), rel_tol=1e-12):
            raise ValueError("output RMS metadata mismatch")
        power = powers(output)
        allowed = set(range(bin_number, FRAMES // 2 + 1, bin_number))
        alias = np.ones(FRAMES // 2 + 1, dtype=bool)
        alias[0] = False
        alias[list(allowed)] = False
        residual_power = float(power[alias].sum())
        folded_third = (3 * bin_number) % FRAMES
        folded_third = min(folded_third, FRAMES - folded_third)
        peak_bin = int(np.argmax(np.where(alias, power, -1)))
        results.append({
            "case": index, "rate": rate, "model": "Tape" if model == 0 else "Tube",
            "amplitude": amplitude, "bin": bin_number, "allowed_bins": ";".join(map(str, sorted(allowed))),
            "input_rms": math.sqrt(input_power), "output_rms": math.sqrt(float(np.mean(output ** 2))),
            "folded_db_input": db_power(residual_power, input_power),
            "folded_h3_bin": folded_third, "folded_h3_db_input": db_power(float(power[folded_third]), input_power),
            "peak_alias_bin": peak_bin, "peak_alias_hz": peak_bin * rate / FRAMES,
            "peak_alias_db_input": db_power(float(power[peak_bin]), input_power),
            "nyquist_db_input": db_power(float(power[-1]), input_power),
        })
    if len(revisions) != 1:
        raise ValueError("mixed source revisions")
    return results, revisions.pop()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", nargs="?", type=Path, default=Path("/tmp/darkvoice-colour-alias"))
    args = parser.parse_args()
    results, revision = analyse(args.directory)
    writer = csv.DictWriter(sys.stdout, fieldnames=list(results[0]))
    writer.writeheader()
    writer.writerows(results)
    worst = max(results, key=lambda row: row["folded_db_input"])
    count = sum(row["folded_db_input"] > -55 for row in results)
    print(f"source={revision} fixtures={len(results)} worst={worst['folded_db_input']:.9f}dB "
          f"case={worst['case']} above_illustrative_minus55={count}; no quality-pass exit status", file=sys.stderr)


if __name__ == "__main__":
    main()
