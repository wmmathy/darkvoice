"""Optional NumPy developer diagnostics; no quality-pass exit status.

Consume only the fixed probe measurements. Spectral peaks describe the synthetic
spectral envelope, not the fundamental. Correlation and harmonic-grid proximity
alone can falsely accept an unchanged signal as an octave-down result.
"""
from pathlib import Path
import hashlib
import shutil
import sys

import numpy as np

RATE = 48000
FRAMES = 96000
FFT_FRAMES = 1048576
FIXTURES = ((100, 600, 12, 0), (70, 600, -12, 0), (180, 1200, 12, 7))


def load(path):
    raw = path.read_bytes()
    if len(raw) != FRAMES * 4:
        raise ValueError(f"malformed measurement length: {path}")
    x = np.frombuffer(raw, dtype="<f4").astype(np.float64)
    if not np.isfinite(x).all():
        raise ValueError(f"nonfinite measurement: {path}")
    x = x[FRAMES // 2:]
    if not float(np.dot(x, x)) > 0:
        raise ValueError(f"zero-energy measurement: {path}")
    return x


def correlation(x, frequency):
    lag = int(np.floor(RATE / frequency + 0.5))
    left, right = x[lag:], x[:-lag]
    denominator = np.sqrt(np.dot(left, left) * np.dot(right, right))
    if not np.isfinite(denominator) or denominator <= 0:
        raise ValueError("undefined correlation")
    return float(np.dot(left, right) / denominator)


def metrics(x, target):
    window = np.hanning(len(x))
    padded = np.abs(np.fft.rfft(x * window, n=FFT_FRAMES))
    index = int(padded.argmax())
    if not 0 < index < len(padded) - 1:
        raise ValueError("spectral maximum outside interpolation interior")
    values = np.log(np.maximum(padded[index - 1:index + 2], 1e-30))
    denominator = values[0] - 2 * values[1] + values[2]
    if not np.isfinite(denominator) or denominator == 0:
        raise ValueError("undefined spectral interpolation")
    offset = 0.5 * (values[0] - values[2]) / denominator
    peak = (index + offset) * RATE / FFT_FRAMES
    spectrum = np.abs(np.fft.rfft(x * window)) ** 2
    odd = even = 0.0
    for harmonic in range(1, 81):
        center = int(np.floor(harmonic * target + 0.5))
        if center + 2 >= len(spectrum):
            break
        energy = float(spectrum[max(0, center - 2):center + 3].sum())
        if harmonic % 2:
            odd += energy
        else:
            even += energy
    total = float(spectrum.sum())
    if total <= 0 or odd + even <= 0:
        raise ValueError("undefined spectral energy")
    return (
        correlation(x, target * 2),  # half target period
        correlation(x, target),
        correlation(x, target / 2),  # double target period
        peak, (odd + even) / total, odd / total, odd / (odd + even),
    )



def prepare_private(source, destination):
    """Create a fingerprint-guarded instrumented developer copy, never upstream edits."""
    header = (source / "signalsmith-stretch.h").read_bytes()
    expected = "7003842b8347ba10710f280c33dba494cd3466a54b240cef76cfbbe32a9a93b8"
    if hashlib.sha256(header).hexdigest() != expected:
        raise ValueError("experimental private header fingerprint mismatch")
    text = header.decode()

    def replace_once(before, after):
        nonlocal text
        if text.count(before) != 1:
            raise ValueError(f"instrumentation anchor mismatch: {before[:70]}")
        text = text.replace(before, after)

    declarations = """
// DarkVoice developer-only instrumentation. No product use or quality-pass claim.
namespace darkvoice_coupling {
inline int gainMode = 0;
inline double sourceF0 = 100, centerHz = 600, pitchScale = 1, formantScale = 1;
inline unsigned long long predictions = 0, fallbacks = 0;
inline double predictionEnergy = 0, fallbackEnergy = 0;
inline double maximumRatio = 0;
inline void clear() {
 predictions = fallbacks = 0;
 predictionEnergy = fallbackEnergy = maximumRatio = 0;
}
}
"""
    replace_once("namespace signalsmith { namespace stretch {", declarations +
                 "namespace signalsmith { namespace stretch {")
    replace_once("peaks.reserve(bands/2);", "peaks.reserve(bands/2);\n"
                 "\t\tcouplingFrozenRatio.assign(bands, 1);\n")
    replace_once("std::vector<Sample> formantMetric;", "std::vector<Sample> formantMetric;\n"
                 "\tstd::vector<Sample> couplingFrozenRatio;\n"
                 "\tint couplingFormantFrames = 0;")
    replace_once("auto getFormant = [&](Sample band) -> Sample {",
                 "++couplingFormantFrames;\n\t\t\tauto getFormant = [&](Sample band) -> Sample {")
    replace_once("Sample formantRatio = targetE/(inputE + Sample(1e-30));", """
                Sample formantRatio = targetE/(inputE + Sample(1e-30));
                if (darkvoice_coupling::gainMode == 1) {
                    formantRatio = 1; // Keep all three formant-processing steps.
                } else if (darkvoice_coupling::gainMode == 2) {
                    // Freeze after 48 completed formant-update hops (>1s at1024/48k).
                    if (couplingFormantFrames <= 48) couplingFrozenRatio[b] = formantRatio;
                    else formantRatio = couplingFrozenRatio[b];
                } else if (darkvoice_coupling::gainMode == 3) {
                    const double inputHz = inputF*48000;
                    const double outputHz = inputHz*darkvoice_coupling::pitchScale/
                                            darkvoice_coupling::formantScale;
                    const auto envelope = [](double hz) {
                        const double h = std::max(hz/darkvoice_coupling::sourceF0, 1.0);
                        return std::exp(-.5*std::pow((hz-darkvoice_coupling::centerHz)/100, 2))+
                               .002/h;
                    };
                    const double ratio = envelope(outputHz)/envelope(inputHz);
                    formantRatio = static_cast<Sample>(ratio*ratio);
                }
                darkvoice_coupling::maximumRatio = std::max(
                    darkvoice_coupling::maximumRatio, static_cast<double>(formantRatio));
""")
    replace_once("Sample phaseNorm = _impl::norm(phase);", """
            ++darkvoice_coupling::predictions;
            darkvoice_coupling::predictionEnergy += energy;
            Sample phaseNorm = _impl::norm(phase);
""")
    replace_once("if (phaseNorm <= noiseFloor) {", """if (phaseNorm <= noiseFloor) {
                ++darkvoice_coupling::fallbacks;
                darkvoice_coupling::fallbackEnergy += energy;
""")
    destination.mkdir(parents=True, exist_ok=True)
    (destination / "signalsmith-linear").mkdir(exist_ok=True)
    (destination / "signalsmith-stretch.h").write_text(text)
    for name in ("stft.h", "fft.h"):
        shutil.copyfile(source / "signalsmith-linear" / name,
                        destination / "signalsmith-linear" / name)
    repo = Path(__file__).resolve().parents[2]
    wrapper = (repo / "src/PitchFormantProcessor.cpp").read_bytes()
    expected_wrapper = "d5281af605ba5cd7ecf466404bdfe838b571ca84cddbcc551bdb493857c86055"
    if hashlib.sha256(wrapper).hexdigest() != expected_wrapper:
        raise ValueError("experimental wrapper fingerprint mismatch")
    wrapper = wrapper.decode()
    anchor = "static_cast<int>(next->block / 8), true);"
    if wrapper.count(anchor) != 1:
        raise ValueError("split-processing wrapper anchor mismatch")
    wrapper = wrapper.replace(anchor,
                              "static_cast<int>(next->block / 8), DARKVOICE_COUPLING_SPLIT);")
    (destination / "PitchFormantProcessor.cpp").write_text(wrapper)
    for component in ("stretch", "linear"):
        name = f"signalsmith-{component}-MIT.txt"
        shutil.copyfile(repo / "third_party/licenses" / name, destination / name)
    print("instrumented-header-sha256", hashlib.sha256(text.encode()).hexdigest())
    print("split-wrapper-sha256", hashlib.sha256(wrapper.encode()).hexdigest())


def main():
    print("case,gain,compensation,corrHalfT,corrT,corrDoubleT,peakHz,"
          "capturedGridOverTotal,oddGridOverTotal,oddOverCaptured,envelopeErrorPct")
    for case, (f0, center, pitch, formant) in enumerate(FIXTURES):
        target = f0 * 2 ** (pitch / 12)
        envelope = center * 2 ** (formant / 12)
        for gain in (1.0, 0.25, 4.0):
            for compensation in (1, 0):
                path = Path(f"/tmp/pitch-coupling-c{case}-g{gain:.2f}-m{compensation}.raw")
                x = load(path) / gain
                values = metrics(x, target)
                # For m0 this compares against the requested preserved/shifted
                # envelope deliberately: m0 is a periodicity negative control,
                # not a working formant-preserving product setting.
                error = 100 * (values[3] / envelope - 1)
                print(f"{case},{gain:.2f},{compensation}," +
                      ",".join(f"{v:.9f}" for v in (*values, error)))
    print("controls,targetHz,corrHalfT,corrT,corrDoubleT,peakHz,"
          "capturedGridOverTotal,oddGridOverTotal,oddOverCaptured")
    for index, label in enumerate(("ideal35", "unchanged70", "evenOnly35")):
        x = load(Path(f"/tmp/pitch-coupling-control{index}.raw"))
        print(f"{label},35," + ",".join(f"{v:.9f}" for v in metrics(x, 35)))


def matrix_analysis():
    print("split,case,mode,corrHalfT,corrT,corrDoubleT,peakHz,"
          "capturedGridOverTotal,oddGridOverTotal,oddOverCaptured,envelopeErrorPct")
    for split in (1, 0):
        for case, (f0, center, pitch, formant) in enumerate(FIXTURES):
            target = f0 * 2 ** (pitch / 12)
            envelope = center * 2 ** (formant / 12)
            for mode in range(4):
                path = Path(f"/tmp/pitch-matrix-s{split}-c{case}-m{mode}.raw")
                values = metrics(load(path), target)
                error = 100 * (values[3] / envelope - 1)
                print(f"{split},{case},{mode}," +
                      ",".join(f"{v:.9f}" for v in (*values, error)))


if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "--prepare-private":
        prepare_private(Path(sys.argv[2]), Path(sys.argv[3]))
    elif len(sys.argv) == 2 and sys.argv[1] == "--matrix":
        matrix_analysis()
    elif len(sys.argv) == 1:
        main()
    else:
        raise SystemExit("usage: analyse.py [--matrix | --prepare-private SOURCE_DIR DESTINATION_DIR]")
