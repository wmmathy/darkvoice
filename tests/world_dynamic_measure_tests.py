"""Independent synthetic controls for the offline measurement calibration."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'tools/qualification/world_dynamic_measure.py'
SPEC = importlib.util.spec_from_file_location('world_dynamic_measure', SCRIPT)
m = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(m)


def trajectory(name, rate):
    time = np.arange(3 * rate) / rate
    ramp = np.clip((time - .25) / 2.5, 0, 1)
    if name == 'vibrato':
        return 100 * np.exp2(50 * np.sin(11 * np.pi * time) / 1200)
    if name == 'rising':
        return 70 * np.exp(np.log(180 / 70) * ramp)
    if name == 'falling':
        return 180 * np.exp(np.log(70 / 180) * ramp)
    raise ValueError(name)


def synth(f0, rate, center=1200, formant=0, even_only=False):
    # Independent direct sum; golden phase never enters an estimator or WORLD.
    phase = np.cumsum(f0, dtype=np.float64) / rate - f0 / rate
    envelope = center * 2 ** (formant / 12)
    sigma = 100 * 2 ** (formant / 12)
    result = np.zeros(len(f0))
    for harmonic in range(2 if even_only else 1, 81, 2 if even_only else 1):
        amplitude = .025 * (np.exp(-.5 * ((harmonic * f0 - envelope) / sigma) ** 2)
                            + .002 / harmonic)
        result += amplitude * np.sin(2 * np.pi * harmonic * phase)
    fade = np.minimum(1, np.minimum(np.arange(len(f0)), np.arange(len(f0))[::-1]) / (.01 * rate))
    return result * fade


def band_noise(rate, seed=953):
    random = np.random.default_rng(seed).normal(size=3 * rate)
    spectrum = np.fft.rfft(random)
    frequency = np.fft.rfftfreq(len(random), 1 / rate)
    spectrum[(frequency < 1500) | (frequency > 8000)] = 0
    result = np.fft.irfft(spectrum)
    fade = np.minimum(1, np.minimum(np.arange(len(result)), np.arange(len(result))[::-1]) / (.02 * rate))
    return result * fade * .05


def sequence(rate, pitch=0, formant=0):
    time = np.arange(3 * rate) / rate
    voice = np.zeros(len(time), dtype=bool)
    signal = np.zeros(len(time))
    target = np.full(len(time), 110 * 2 ** (pitch / 12))
    vowel = synth(target, rate, formant=formant)
    for start, end in ((.3, 1.), (1.35, 2.05), (2.4, 2.85)):
        selected = (time >= start) & (time < end)
        voice[selected] = True
        fade = np.minimum(1, np.minimum(time[selected] - start, end - time[selected]) / .01)
        signal[selected] = vowel[selected] * fade
    fricative = (time >= 1.) & (time < 1.35)
    fade = np.minimum(1, np.minimum(time[fricative] - 1., 1.35 - time[fricative]) / .02)
    signal[fricative] = band_noise(rate)[fricative] * fade
    silence = ~(voice | fricative)
    return signal, target, voice, silence


class CalibrationTests(unittest.TestCase):
    def test_correct_dynamic_controls(self):
        for rate in (48000, 96000):
            for fixture in ('vibrato', 'rising', 'falling'):
                f0 = trajectory(fixture, rate)
                for pitch in (-12, 0, 12):
                    target = f0 * 2 ** (pitch / 12)
                    for formant in (0, 7):
                        with self.subTest(rate=rate, fixture=fixture, pitch=pitch, formant=formant):
                            result = m.output_metrics(synth(target, rate, formant=formant), rate,
                                                      target, np.ones(len(f0)),
                                                      1200 * 2 ** (formant / 12))
                            scalar = {key: value for key, value in result['gates'].items()
                                      if key != 'no_octave_ambiguity_v2'}
                            self.assertTrue(all(scalar.values()), scalar)
                            # Unit correctness preserves this negative result. The full
                            # calibration remains failed and --self-test returns1.
                            known_ambiguous = fixture in ('rising', 'falling') and pitch == 12 and formant == 0
                            self.assertEqual(result['gates']['no_octave_ambiguity_v2'],
                                             not known_ambiguous)
                            self.assertEqual(m.all_gates(result), not known_ambiguous)

    def test_wrong_dynamic_controls(self):
        rate = 48000
        for fixture in ('vibrato', 'rising', 'falling'):
            f0 = trajectory(fixture, rate)
            target = f0 * 2
            controls = {'unchanged': f0, 'octave_high': target * 2,
                        'octave_low': target / 2,
                        'stationary_mean': np.full(len(f0), np.mean(target)),
                        'reversed': target[::-1]}
            if fixture == 'vibrato':
                controls.pop('reversed')  # a phase reversal is not an independent pitch error
                controls['flattened'] = np.full(len(f0), 200.)
            for name, contour in controls.items():
                with self.subTest(fixture=fixture, control=name):
                    result = m.output_metrics(synth(contour, rate), rate, target, np.ones(len(f0)))
                    self.assertFalse(m.all_gates(result), {k: v for k, v in result.items()
                                                          if k != 'frames'})
            even = m.output_metrics(synth(target, rate, even_only=True), rate,
                                    target, np.ones(len(f0)))
            # This checks whether numerical-zero odd occupancy can resolve an
            # even-only dynamic control. Failure is a calibration limitation,
            # never permission to introduce a tuned occupancy threshold.
            self.assertFalse(m.all_gates(even), {k: v for k, v in even.items() if k != 'frames'})

    def test_contour_and_masks(self):
        rate = 48000
        f0 = trajectory('vibrato', rate)
        voiced = np.ones(len(f0), dtype=bool)
        times, indices, eligible, boundaries = m.interior_grid(voiced, rate)
        self.assertEqual(len(boundaries), 0)
        reference = f0[indices]
        result = m.contour_metrics(reference, reference, voiced[indices], eligible)
        self.assertTrue(m.all_gates(result))
        for wrong in (reference * 2, np.zeros(len(reference))):
            self.assertFalse(m.all_gates(m.contour_metrics(wrong, reference,
                                                          voiced[indices], eligible)))
        missing = reference.copy()
        missing[100:130] = 0
        self.assertFalse(m.contour_metrics(missing, reference, voiced[indices], eligible)['gates']['missing_run'])

    def test_noise_and_silence(self):
        for rate in (48000, 96000):
            source = band_noise(rate)
            result = m.noise_metrics(source, band_noise(rate, 444), rate)
            self.assertTrue(m.all_gates(result), result)
            self.assertFalse(m.all_gates(m.noise_metrics(source, source * 2, rate)))
            periodic = .05 * np.sin(2 * np.pi * 2000 * np.arange(len(source)) / rate)
            self.assertFalse(m.all_gates(m.noise_metrics(source, periodic, rate)))
            self.assertTrue(m.all_gates(m.silence_metrics(np.zeros(3 * rate))))
            self.assertFalse(m.all_gates(m.silence_metrics(np.full(3 * rate, 2e-6))))

    def test_sequence_and_voicing(self):
        for rate in (48000, 96000):
            for pitch in (-12, 0, 12):
                for formant in (0, 7):
                    with self.subTest(rate=rate, pitch=pitch, formant=formant):
                        signal, target, voice, silence = sequence(rate, pitch, formant)
                        result = m.output_metrics(signal, rate, target, voice,
                                                  1200 * 2 ** (formant / 12))
                        self.assertTrue(m.all_gates(result), {k: v for k, v in result.items()
                                                             if k != 'frames'})
            signal, target, voice, silence = sequence(rate)
            times, indices, _, _ = m.interior_grid(voice, rate)
            contour = np.where(voice[indices], target[indices], 0)
            self.assertTrue(m.all_gates(m.voicing_metrics(contour, voice, rate)))
            self.assertFalse(m.all_gates(m.voicing_metrics(np.full(len(times), 110.), voice, rate)))
            shifted = np.roll(contour, 20)
            self.assertFalse(m.voicing_metrics(shifted, voice, rate)['gates']['boundary_displacement'])
            reference_rms = float(np.sqrt(np.mean(signal[voice] ** 2)))
            self.assertTrue(m.all_gates(m.sequence_silence_metrics(signal, rate, silence,
                                                                  reference_rms)))
            leaked = signal + silence * .001
            self.assertFalse(m.all_gates(m.sequence_silence_metrics(leaked, rate, silence,
                                                                   reference_rms)))

    def test_malformed(self):
        rate = 48000
        f0 = np.full(3 * rate, 100.)
        signal = np.zeros(len(f0))
        voice = np.ones(len(f0))
        for args in ((signal[:-1], rate, f0, voice), (signal, 44100, f0, voice),
                     (signal, rate, -f0, voice), (signal, rate, f0, voice + 1),
                     (signal + np.nan, rate, f0, voice),
                     (signal, rate, f0[:-1], voice)):
            with self.subTest(shape=[np.shape(x) for x in args]):
                with self.assertRaises(ValueError):
                    m.output_metrics(*args)
        with self.assertRaises(ValueError):
            m.noise_metrics(signal, signal, rate)

    def test_cli_malformed_archives(self):
        rate = 48000
        f0 = np.full(3 * rate, 100.)
        voice = np.ones(len(f0))
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bad = []
            np.save(root / 'array.npy', np.zeros(3))
            bad.append(root / 'array.npy')
            (root / 'truncated.npz').write_bytes(b'PK\x03\x04incomplete')
            bad.append(root / 'truncated.npz')
            for name, signal, extra in (('complex', np.zeros(len(f0), dtype=complex), {}),
                                        ('nonfinite', np.full(len(f0), np.nan), {}),
                                        ('short', np.zeros(len(f0) - 1), {}),
                                        ('unknown_key', np.zeros(len(f0)), {'unexpected': 1})):
                path = root / f'{name}.npz'
                np.savez(path, signal=signal, target_f0=f0, voiced=voice,
                         rate=rate, envelope_hz=1200., **extra)
                bad.append(path)
            for path in bad:
                result = subprocess.run([sys.executable, '-B', str(SCRIPT), '--input', str(path)],
                                        capture_output=True)
                self.assertEqual(result.returncode, 2, (path, result.stderr))
            unvoiced = m.output_metrics(np.zeros(len(f0)), rate, f0, np.zeros(len(f0)))
            self.assertFalse(m.all_gates(unvoiced))
            self.assertIn('separate unvoiced', unvoiced['applicability'])

    def test_cli(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            result = subprocess.run([sys.executable, '-B', str(SCRIPT), '--self-test',
                                     '--output', str(root / 'report.json')], capture_output=True)
            self.assertEqual(result.returncode, 1, result.stderr)
            report = json.loads((root / 'report.json').read_text())
            self.assertIn('no backend', report['scope'])
            self.assertEqual(report['correct_total_count'], 48)
            self.assertEqual(report['v1_calibration']['status'], 'failed')
            self.assertTrue(report['v1_calibration']['false_pass_controls'])
            self.assertEqual(report['v2_calibration']['status'], 'failed')
            self.assertEqual(len(report['v2_calibration']['correct_controls_failing_or_ambiguous']), 4)
            self.assertEqual(report['v2_calibration']['wrong_controls_false_passing'], [])
            self.assertEqual(report['energy_boundary_metric']['status'], 'unspecified_and_unrun')
            for args in ([], ['--input', str(root / 'missing.npz')],
                         ['--self-test', '--input', str(root / 'missing.npz')]):
                result = subprocess.run([sys.executable, '-B', str(SCRIPT), *args], capture_output=True)
                self.assertEqual(result.returncode, 2, result.stderr)


if __name__ == '__main__':
    unittest.main()
