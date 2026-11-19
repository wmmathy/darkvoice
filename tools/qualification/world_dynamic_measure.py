#!/usr/bin/env python3
"""Offline phase-aware measurement calibration; this never runs a pitch backend.

Golden phase is measurement metadata only. It must not be supplied to WORLD.
All failures are retained in JSON. A zero exit means applicable calibration gates
passed, not that any streaming or product implementation has been qualified.
"""
import argparse
import json
from pathlib import Path
import sys
import zipfile

import numpy as np

PHASE_SAMPLES = 256
LAGS = np.arange(243, 270)  # fixed .94921875..1.05078125 cycles
ODD_NUMERICAL_FLOOR = 64 * np.finfo(np.float64).eps  # numerical zero only, not a vocal gate
HALF_CYCLE_AMBIGUITY = .995  # lead-approved conservative v2 guard, not a vocal detector


def vector(value, name, count=None):
    raw = np.asarray(value)
    if raw.dtype.kind not in 'fiu':
        raise ValueError(f'{name}: real numeric data required')
    result = np.asarray(raw, dtype=np.float64)
    if result.ndim != 1 or len(result) < 2 or (count is not None and len(result) != count):
        raise ValueError(f'{name}: invalid one-dimensional length')
    if not np.isfinite(result).all():
        raise ValueError(f'{name}: nonfinite data')
    return result


def validate(signal, rate, target_f0, voiced):
    if rate not in (48000, 96000):
        raise ValueError('rate must be 48000 or 96000')
    signal = vector(signal, 'signal', int(3 * rate))
    f0 = vector(target_f0, 'target_f0', len(signal))
    if np.any(f0 <= 0) or np.any(f0 >= rate / 2):
        raise ValueError('target_f0 must be strictly positive and below Nyquist')
    mask = np.asarray(voiced)
    if mask.ndim != 1 or len(mask) != len(signal) or not np.isin(mask, [0, 1]).all():
        raise ValueError('voiced: invalid binary mask')
    return signal, f0, mask.astype(bool)


def phase_clock(f0, rate):
    # Phase at sample n is sum(f0[0:n])/rate, in cycles.
    return np.r_[0., np.cumsum(f0[:-1], dtype=np.float64) / rate]


def interior_grid(voiced, rate):
    times = np.arange(1, 600, dtype=np.float64) * .005
    eligible = (times > .1 + 1e-12) & (times < 2.9 - 1e-12)
    changes = np.flatnonzero(voiced[1:] != voiced[:-1]) + 1
    for boundary in changes / rate:
        eligible &= np.abs(times - boundary) > .1 + 1e-12
    indices = np.minimum(np.rint(times * rate).astype(int), len(voiced) - 1)
    return times, indices, eligible, changes / rate


def longest_run(mask, seconds=.005):
    longest = current = 0
    for flag in mask:
        current = current + 1 if flag else 0
        longest = max(longest, current)
    return float(longest * seconds)


def contour_metrics(estimated, reference, voiced, eligible):
    reference = vector(reference, 'reference contour')
    if np.any(reference <= 0):
        raise ValueError('reference contour must be positive')
    estimated = vector(estimated, 'estimated contour', len(reference))
    voiced = np.asarray(voiced)
    eligible = np.asarray(eligible)
    if voiced.shape != reference.shape or eligible.shape != reference.shape:
        raise ValueError('contour mask shape mismatch')
    if not np.isin(voiced, [0, 1]).all() or not np.isin(eligible, [0, 1]).all():
        raise ValueError('invalid contour masks')
    voiced, eligible = voiced.astype(bool), eligible.astype(bool)
    if np.any(estimated < 0):
        raise ValueError('estimated contour has negative F0')
    selected = voiced & eligible
    valid = selected & (estimated > 0)
    if not selected.any():
        return None
    errors = np.abs(1200 * np.log2(estimated[valid] / reference[valid]))
    count = int(selected.sum())
    result = {
        'voiced_frames': count, 'estimated_frames': int(valid.sum()),
        'coverage': float(valid.sum() / count),
        'median_cents': float(np.median(errors)) if len(errors) else None,
        'p95_cents': float(np.percentile(errors, 95)) if len(errors) else None,
        'over_600_fraction': float(np.count_nonzero(errors > 600) / count),
        'longest_missing_seconds': longest_run(selected & ~valid),
    }
    result['gates'] = {
        'coverage': result['coverage'] >= .9,
        'median_cents': bool(len(errors) and result['median_cents'] <= 20),
        'p95_cents': bool(len(errors) and result['p95_cents'] <= 50),
        'octave_outliers': result['over_600_fraction'] <= .01,
        'missing_run': result['longest_missing_seconds'] <= .05 + 1e-12,
    }
    return result


def voicing_metrics(estimated, voiced, rate):
    """Score a supplied contour; this function never estimates or fills F0."""
    mask = np.asarray(voiced)
    if rate not in (48000, 96000) or mask.shape != (3 * rate,) or not np.isin(mask, [0, 1]).all():
        raise ValueError('invalid dense reference voicing')
    times, indices, eligible, boundaries = interior_grid(mask.astype(bool), rate)
    estimated = vector(estimated, 'estimated contour', len(times))
    if np.any(estimated < 0):
        raise ValueError('estimated contour has negative F0')
    predicted = estimated > 0
    truth = mask[indices].astype(bool)
    unvoiced = eligible & ~truth
    false = unvoiced & predicted
    changes = np.flatnonzero(predicted[1:] != predicted[:-1]) + 1
    predicted_boundaries = times[changes] - .0025
    directions = np.diff(predicted.astype(int))[changes - 1]
    reference_changes = np.flatnonzero(mask[1:] != mask[:-1]) + 1
    reference_directions = np.diff(mask.astype(int))[reference_changes - 1]
    matching = (len(boundaries) == len(predicted_boundaries) and
                np.array_equal(directions, reference_directions))
    displacement = float(np.max(np.abs(boundaries - predicted_boundaries))) if matching and len(boundaries) else (0. if matching else None)
    fraction = float(false.sum() / unvoiced.sum()) if unvoiced.any() else 0.
    return {'interior_unvoiced_frames': int(unvoiced.sum()),
            'false_voiced_fraction': fraction,
            'longest_false_voiced_seconds': longest_run(false),
            'reference_boundaries': boundaries.tolist(),
            'estimated_boundaries': predicted_boundaries.tolist(),
            'boundary_displacement_seconds': displacement,
            'gates': {'false_voicing': fraction <= .05,
                      'false_run': longest_run(false) <= .02 + 1e-12,
                      'boundary_count_and_direction': bool(matching),
                      'boundary_displacement': displacement is not None and displacement <= .05}}


def sequence_silence_metrics(output, rate, silence, reference_voiced_rms):
    output = vector(output, 'sequence output', 3 * rate)
    silence = np.asarray(silence)
    if rate not in (48000, 96000) or silence.shape != output.shape or not np.isin(silence, [0, 1]).all():
        raise ValueError('invalid declared silence mask')
    if not np.isfinite(reference_voiced_rms) or reference_voiced_rms <= 0:
        raise ValueError('invalid adjacent voiced RMS')
    _, indices, eligible, _ = interior_grid(silence.astype(bool), rate)
    selected = eligible & silence[indices].astype(bool)
    energy = []
    half = int(.005 * rate)
    for index in indices[selected]:
        block = output[index-half:index+half]
        energy.append(float(np.sqrt(np.mean(block * block))))
    worst = max(energy, default=None)
    db = float(20 * np.log10(max(worst / reference_voiced_rms, 1e-300))) if worst is not None else None
    return {'interior_frames': len(energy), 'maximum_relative_rms_db': db,
            'gates': {'interior_available': bool(energy),
                      'silence_level': db is not None and db <= -60}}


def output_metrics(signal, rate, target_f0, voiced, envelope_hz=1200.):
    signal, f0, voiced = validate(signal, rate, target_f0, voiced)
    if not np.isfinite(envelope_hz) or not 0 < envelope_hz < rate / 2:
        raise ValueError('invalid envelope_hz')
    times, indices, eligible, boundaries = interior_grid(voiced, rate)
    phase = phase_clock(f0, rate)
    selected = eligible & voiced[indices]
    rows = []
    support_failures = 0
    n = 6 * PHASE_SAMPLES
    # Centered seven-cycle support (plus fixed fractional-lag search margin).
    offsets = np.arange(n + int(LAGS[-1]), dtype=np.float64) / PHASE_SAMPLES - 3.5
    for time, index in zip(times[selected], indices[selected]):
        coordinate = phase[index] + offsets
        if coordinate[0] < phase[0] or coordinate[-1] > phase[-1]:
            support_failures += 1
            continue
        local = np.interp(coordinate, phase, signal)
        a = local[:n]
        windows = np.lib.stride_tricks.sliding_window_view(local, n)[LAGS]
        energy = float(np.sum(a * a))
        denominator = np.sqrt(energy * np.sum(windows * windows, axis=1))
        scores = np.divide(np.sum(windows * a, axis=1), denominator,
                           out=np.full(len(LAGS), -1.), where=denominator > 1e-30)
        half = local[PHASE_SAMPLES // 2:PHASE_SAMPLES // 2 + n]
        half_denominator = np.sqrt(energy * np.sum(half * half))
        half_correlation = float(np.sum(a * half) / half_denominator) if half_denominator > 1e-30 else -1.
        best = int(np.argmax(scores))
        lag = float(LAGS[best])
        if 0 < best < len(scores) - 1:
            curvature = scores[best-1] - 2 * scores[best] + scores[best+1]
            if curvature < 0:
                lag += float(np.clip(.5 * (scores[best-1] - scores[best+1]) / curvature,
                                     -.5, .5))
        cents = float(abs(1200 * np.log2(PHASE_SAMPLES / lag)))
        spectrum = np.abs(np.fft.rfft(a)) / n
        harmonics = spectrum[np.arange(1, 81) * 6]
        power = harmonics * harmonics
        total = float(power.sum())
        odd = float(power[::2].sum() / total) if total > 1e-30 else 0.
        peak = int(np.argmax(harmonics))
        refined = float(peak + 1)
        resolvable = 0 < peak < len(harmonics) - 1
        if resolvable:
            log = np.log(np.maximum(harmonics[peak-1:peak+2], 1e-30))
            curvature = log[0] - 2 * log[1] + log[2]
            if curvature < 0:
                refined += float(np.clip(.5 * (log[0] - log[2]) / curvature, -.5, .5))
            else:
                resolvable = False
        measured_envelope = refined * f0[index]
        rows.append({'time': float(time), 'one_cycle_correlation': float(scores[13]),
                     'half_cycle_correlation': half_correlation,
                     'octave_ambiguous': bool(scores[13] > .8 and half_correlation >= HALF_CYCLE_AMBIGUITY),
                     'cents': cents, 'search_endpoint': best in (0, len(scores)-1),
                     'odd_energy_fraction': odd,
                     'envelope_hz': float(measured_envelope),
                     'envelope_resolvable': bool(resolvable),
                     'envelope_error': float(abs(measured_envelope / envelope_hz - 1))})
    result = {'frames': rows, 'eligible_voiced_frames': int(selected.sum()),
              'excluded_frames': int((~eligible).sum()),
              'boundary_seconds': boundaries.tolist(), 'support_failures': support_failures,
              'lag_search_cycles': [float(LAGS[0] / PHASE_SAMPLES),
                                    float(LAGS[-1] / PHASE_SAMPLES)],
              'lag_grid_cycles': 1 / PHASE_SAMPLES,
              'odd_numerical_floor': ODD_NUMERICAL_FLOOR,
              'half_cycle_ambiguity_threshold': HALF_CYCLE_AMBIGUITY,
              'octave_scope': 'numerical-zero odd energy is ambiguous; nonzero is not proof of a fundamental'}
    if not rows:
        result.update({'gates': {'voiced_measurements_available': False},
                       'applicability': 'no voiced phase windows; use separate unvoiced metrics',
                       'minimum_correlation': None, 'median_cents': None, 'p95_cents': None,
                       'minimum_odd_energy_fraction': None, 'maximum_envelope_error': None,
                       'search_endpoint_frames': 0, 'unresolved_envelope_frames': 0,
                       'minimum_half_cycle_correlation': None, 'maximum_half_cycle_correlation': None,
                       'octave_ambiguous_fraction': None})
        return result
    correlations = np.array([row['one_cycle_correlation'] for row in rows])
    cents = np.array([row['cents'] for row in rows])
    odds = np.array([row['odd_energy_fraction'] for row in rows])
    envelopes = np.array([row['envelope_error'] for row in rows
                          if row['envelope_resolvable']])
    result.update({'minimum_correlation': float(correlations.min()),
                   'minimum_half_cycle_correlation': float(min(row['half_cycle_correlation'] for row in rows)),
                   'maximum_half_cycle_correlation': float(max(row['half_cycle_correlation'] for row in rows)),
                   'octave_ambiguous_fraction': float(sum(row['octave_ambiguous'] for row in rows) / len(rows)),
                   'median_cents': float(np.median(cents)),
                   'p95_cents': float(np.percentile(cents, 95)),
                   'minimum_odd_energy_fraction': float(odds.min()),
                   'maximum_envelope_error': float(envelopes.max()) if len(envelopes) else None,
                   'search_endpoint_frames': sum(row['search_endpoint'] for row in rows),
                   'unresolved_envelope_frames': sum(not row['envelope_resolvable'] for row in rows)})
    result['gates'] = {'support': support_failures == 0,
                       'correlation': bool(np.all(correlations > .8)),
                       'median_cents': result['median_cents'] <= 20,
                       'p95_cents': result['p95_cents'] <= 50,
                       'no_numerically_zero_odd_energy': bool(np.all(odds >= ODD_NUMERICAL_FLOOR)),
                       'envelope': bool(len(envelopes) and np.all(envelopes < .2)),
                       'no_octave_ambiguity_v2': result['octave_ambiguous_fraction'] == 0}
    return result


def noise_correlation(signal, rate):
    signal = vector(signal, 'noise')
    worst = 0.
    # Evaluate the whole interior at every integer50..800Hz lag, no chosen subregion.
    for lag in range(int(np.ceil(rate / 800)), int(np.floor(rate / 50)) + 1):
        a, b = signal[:-lag], signal[lag:]
        denominator = np.sqrt(np.sum(a * a) * np.sum(b * b))
        if denominator > 1e-30:
            worst = max(worst, abs(float(np.sum(a * b) / denominator)))
    return worst


def noise_metrics(source, output, rate):
    source = vector(source, 'noise source')
    output = vector(output, 'noise output', len(source))
    if rate not in (48000, 96000) or len(source) < int(.1 * rate):
        raise ValueError('invalid noise sample rate or duration')
    source_energy = float(np.mean(source * source))
    if source_energy <= 1e-30:
        raise ValueError('noise fixture has no energy')
    output_energy = float(np.mean(output * output))
    rms_db = float(10 * np.log10(max(output_energy, 1e-300) / source_energy))
    bands = np.linspace(1500, 8000, 14)
    frequencies = np.fft.rfftfreq(len(source), 1 / rate)
    energies = []
    for signal in (source, output):
        powers = np.abs(np.fft.rfft(signal)) ** 2
        energies.append(np.array([powers[(frequencies >= low) & (frequencies < high)].sum()
                                  for low, high in zip(bands[:-1], bands[1:])]))
    denominator = np.linalg.norm(energies[0]) * np.linalg.norm(energies[1])
    similarity = float(np.dot(*energies) / denominator) if denominator else 0.
    input_corr = noise_correlation(source, rate)
    if input_corr > .15:
        raise ValueError(f'invalid fricative fixture autocorrelation {input_corr}')
    output_corr = noise_correlation(output, rate)
    return {'rms_db': rms_db, 'band_similarity': similarity,
            'input_autocorrelation': input_corr, 'output_autocorrelation': output_corr,
            'gates': {'rms': abs(rms_db) <= 3, 'spectrum': similarity >= .9,
                      'periodicity': output_corr <= .3}}


def silence_metrics(signal):
    signal = vector(signal, 'silence')
    rms = float(np.sqrt(np.mean(signal * signal)))
    peak = float(np.max(np.abs(signal)))
    return {'rms': rms, 'peak': peak, 'gates': {'rms': rms <= 1e-7, 'peak': peak <= 1e-6}}


def energy_boundary_metrics(source, output, rate):
    """Fixed input-referenced activity timing; no output gain or time search."""
    if rate not in (48000, 96000):
        raise ValueError('energy-boundary rate must be48000 or96000')
    source = vector(source, 'energy source', 3 * rate)
    output = vector(output, 'energy output', len(source))
    window, hop = int(.010 * rate), int(.005 * rate)
    starts = np.arange(0, len(source) - window + 1, hop)
    times = (starts + window / 2) / rate

    def frame_rms(signal):
        frames = np.lib.stride_tricks.sliding_window_view(signal, window)[::hop]
        # Scaling preserves finite RMS for extreme finite amplitudes without
        # changing the mathematical10ms RMS estimator.
        scale = np.max(np.abs(frames), axis=1)
        normalized = np.divide(frames, scale[:, None], out=np.zeros_like(frames),
                               where=scale[:, None] > 0)
        return scale * np.sqrt(np.mean(normalized * normalized, axis=1))

    input_rms, output_rms = frame_rms(source), frame_rms(output)
    result = {'window_samples': window, 'hop_samples': hop,
              'window_seconds': .010, 'hop_seconds': .005,
              'frame_center_seconds': times.tolist(),
              'input_frame_rms': input_rms.tolist(),
              'output_frame_rms': output_rms.tolist(),
              'threshold_reference': 'median strictly positive INPUT frame RMS *10^(-30/20)',
              'threshold_db_below_input_median': -30.,
              'maximum_allowed_displacement_seconds': .025}
    if not np.any(source):
        result.update({'status': 'NotApplicable', 'threshold': None,
                       'input_positive_median_rms': None,
                       'input_edges': [], 'output_edges': None,
                       'input_edge_count': 0, 'output_edge_count': None,
                       'paired_edges': [], 'maximum_displacement_seconds': None,
                       'gates': {}, 'silence_output': silence_metrics(output)})
        return result
    positive = input_rms[input_rms > 0]
    if not len(positive):
        raise ValueError('input activity RMS is outside representable range')
    median = float(np.median(positive))
    threshold = float(median * 10 ** (-30 / 20))
    if threshold <= 0 or not np.isfinite(threshold):
        raise ValueError('input activity threshold is outside representable range')

    def edges(rms):
        active = rms >= threshold  # same absolute threshold for both signals
        found = []
        if active[0]:
            found.append({'kind': 'onset', 'time': float(times[0]), 'frame_index': 0})
        for index in np.flatnonzero(active[1:] != active[:-1]) + 1:
            found.append({'kind': 'onset' if active[index] else 'offset',
                          'time': float(times[index]), 'frame_index': int(index)})
        if active[-1]:
            found.append({'kind': 'offset', 'time': float(times[-1]),
                          'frame_index': len(times) - 1})
        return found

    input_edges, output_edges = edges(input_rms), edges(output_rms)
    matching = (len(input_edges) == len(output_edges) and
                [edge['kind'] for edge in input_edges] ==
                [edge['kind'] for edge in output_edges])
    paired = []
    if matching:
        for before, after in zip(input_edges, output_edges):
            # Use the integer sample clock so an exact25ms displacement is
            # accepted inclusively, without floating timestamp subtraction error.
            displacement = (after['frame_index'] - before['frame_index']) * hop / rate
            paired.append({'kind': before['kind'], 'input_time': before['time'],
                           'output_time': after['time'],
                           'input_frame_index': before['frame_index'],
                           'output_frame_index': after['frame_index'],
                           'displacement_seconds': displacement,
                           'absolute_displacement_seconds': abs(displacement)})
    maximum = max((edge['absolute_displacement_seconds'] for edge in paired), default=None)
    timing_passed = maximum is not None and maximum <= .025
    result.update({'status': 'passed' if matching and timing_passed else 'failed',
                   'threshold': threshold, 'input_positive_median_rms': median,
                   'input_edges': input_edges, 'output_edges': output_edges,
                   'input_edge_count': len(input_edges), 'output_edge_count': len(output_edges),
                   'paired_edges': paired, 'maximum_displacement_seconds': maximum,
                   'gates': {'edge_count_and_kind': bool(matching),
                             'maximum_displacement': bool(timing_passed)}})
    return result


def energy_boundary_calibration():
    """Predeclared timing controls; report zero input as N/A, not a timing pass."""
    reports = {}
    correct, wrong, not_applicable = [], [], []
    for rate in (48000, 96000):
        time = np.arange(3 * rate) / rate
        source = .02 * (np.sin(2 * np.pi * 220 * time) + .4 * np.sin(2 * np.pi * 1370 * time))
        source *= ((time >= .3) & (time < .95)) | ((time >= 1.6) & (time < 2.4))
        for name, gain in (('unchanged', 1.), ('gain_minus3db', 10 ** (-3 / 20)),
                           ('gain_plus3db', 10 ** (3 / 20))):
            key = f'{rate}_{name}'
            reports[key] = energy_boundary_metrics(source, source * gain, rate)
            correct.append(key)
        for milliseconds in (-80, -40, 40, 80):
            frames = int(rate * milliseconds / 1000)
            output = np.zeros_like(source)
            if frames > 0:
                output[frames:] = source[:-frames]
            else:
                output[:frames] = source[-frames:]
            key = f'{rate}_shift{milliseconds}ms'
            reports[key] = energy_boundary_metrics(source, output, rate)
            wrong.append(key)
        extra = source.copy()
        extra[(time >= 1.15) & (time < 1.35)] = .02
        missing = source.copy()
        missing[(time >= 1.6) & (time < 2.4)] = 0
        for name, output in (('extra_edge', extra), ('missing_edge', missing)):
            key = f'{rate}_{name}'
            reports[key] = energy_boundary_metrics(source, output, rate)
            wrong.append(key)
        key = f'{rate}_all_zero'
        reports[key] = energy_boundary_metrics(np.zeros_like(source), np.zeros_like(source), rate)
        not_applicable.append(key)
    correct_failed = [key for key in correct if not all_gates(reports[key])]
    wrong_false_pass = [key for key in wrong if all_gates(reports[key])]
    invalid_na = [key for key in not_applicable
                  if reports[key]['status'] != 'NotApplicable' or all_gates(reports[key]) or
                  not all_gates(reports[key]['silence_output'])]
    return {'status': 'passed' if not (correct_failed or wrong_false_pass or invalid_na) else 'failed',
            'correct_count': len(correct), 'wrong_control_count': len(wrong),
            'not_applicable_count': len(not_applicable), 'reports': reports,
            'correct_controls_failing': correct_failed,
            'wrong_controls_false_passing': wrong_false_pass,
            'invalid_not_applicable_results': invalid_na}


def all_gates(result):
    return bool(result['gates']) and all(result['gates'].values())


def self_test():
    """Full synthetic calibration; known failures intentionally return exit1."""
    reports = {}
    correct_names = []
    wrong_names = []

    def make(contour, rate, formant=0, even=False):
        phase = phase_clock(contour, rate)
        center, width = 1200 * 2 ** (formant / 12), 100 * 2 ** (formant / 12)
        result = np.zeros(len(contour))
        for harmonic in range(2 if even else 1, 81, 2 if even else 1):
            amplitude = .025 * (np.exp(-.5 * ((harmonic * contour - center) / width) ** 2)
                                + .002 / harmonic)
            result += amplitude * np.sin(2 * np.pi * harmonic * phase)
        fade = np.minimum(1, np.minimum(np.arange(len(result)),
                                        np.arange(len(result))[::-1]) / (.01 * rate))
        return result * fade

    def add(name, signal, rate, target, voiced, formant=0, correct=True):
        report = output_metrics(signal, rate, target, voiced, 1200 * 2 ** (formant / 12))
        report['v1_scalar_gates_pass'] = all(value for key, value in report['gates'].items()
                                            if key != 'no_octave_ambiguity_v2')
        reports[name] = report
        (correct_names if correct else wrong_names).append(name)

    for rate in (48000, 96000):
        time = np.arange(3 * rate) / rate
        ramp = np.clip((time - .25) / 2.5, 0, 1)
        contours = {'vibrato': 100 * np.exp2(50 * np.sin(2 * np.pi * 5.5 * time) / 1200),
                    'rising': 70 * np.exp(np.log(180 / 70) * ramp),
                    'falling': 180 * np.exp(np.log(70 / 180) * ramp)}
        for fixture, f0 in contours.items():
            for pitch in (-12, 0, 12):
                target = f0 * 2 ** (pitch / 12)
                for formant in (0, 7):
                    name = f'{rate}_{fixture}_pitch{pitch}_formant{formant}'
                    add(name, make(target, rate, formant), rate, target,
                        np.ones(len(target)), formant)
            # Mandatory wrong controls use a fixed +12 target, never oracle routing.
            target = f0 * 2
            wrong = {'unchanged': f0, 'octave_high': target * 2,
                     'octave_low': target / 2,
                     'stationary_mean': np.full(len(target), np.mean(target))}
            if fixture == 'vibrato':
                wrong['flattened_vibrato'] = np.full(len(target), 200.)
            else:
                wrong['reversed_glide'] = target[::-1]
            for control, actual in wrong.items():
                add(f'{rate}_{fixture}_{control}', make(actual, rate), rate, target,
                    np.ones(len(target)), correct=False)
            add(f'{rate}_{fixture}_even_only', make(target, rate, even=True), rate,
                target, np.ones(len(target)), correct=False)
        # Twelve correct sequence controls complete the48 voiced calibration rows.
        voice = np.zeros(len(time), dtype=bool)
        edges = ((.3, 1.), (1.35, 2.05), (2.4, 2.85))
        for start, end in edges:
            voice |= (time >= start) & (time < end)
        noise_bins = np.fft.rfft(np.random.default_rng(953).normal(size=len(time)))
        frequencies = np.fft.rfftfreq(len(time), 1 / rate)
        noise_bins[(frequencies < 1500) | (frequencies > 8000)] = 0
        noise = .05 * np.fft.irfft(noise_bins)
        fricative = (time >= 1.) & (time < 1.35)
        for pitch in (-12, 0, 12):
            target = np.full(len(time), 110 * 2 ** (pitch / 12))
            for formant in (0, 7):
                vowel = make(target, rate, formant)
                signal = np.zeros(len(time))
                for start, end in edges:
                    selected = (time >= start) & (time < end)
                    fade = np.minimum(1, np.minimum(time[selected] - start,
                                                    end - time[selected]) / .01)
                    signal[selected] = vowel[selected] * fade
                fade = np.minimum(1, np.minimum(time[fricative] - 1.,
                                                1.35 - time[fricative]) / .02)
                signal[fricative] = noise[fricative] * fade
                add(f'{rate}_sequence_pitch{pitch}_formant{formant}', signal, rate,
                    target, voice, formant)
    false_passes = [name for name in wrong_names if reports[name]['v1_scalar_gates_pass']]
    correct_failures = [name for name in correct_names if not all_gates(reports[name])]
    v2_false_passes = [name for name in wrong_names if all_gates(reports[name])]
    passed = not correct_failures and not v2_false_passes
    return {'reports': reports, 'correct_dynamic_count': 36, 'correct_sequence_count': 12,
            'correct_total_count': len(correct_names), 'wrong_control_count': len(wrong_names),
            'v1_calibration': {'status': 'failed' if false_passes else 'not_reproduced',
                               'false_pass_controls': false_passes},
            'v2_calibration': {'status': 'passed' if passed else 'failed',
                               'correct_controls_failing_or_ambiguous': correct_failures,
                               'wrong_controls_false_passing': v2_false_passes},
            'energy_boundary_metric': energy_boundary_calibration(),
            'scope': 'analyzer calibration only; no backend was run'}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--input', type=Path, help='voiced-measurement NPZ: signal, target_f0, voiced, rate, envelope_hz')
    parser.add_argument('--output', type=Path, help='write full JSON report')
    args = parser.parse_args(argv)
    try:
        if args.self_test == bool(args.input):
            raise ValueError('choose exactly one of --self-test or --input')
        if args.self_test:
            result = self_test()
            passed = (result['v2_calibration']['status'] == 'passed' and
                      result['energy_boundary_metric']['status'] == 'passed')
        else:
            loaded = np.load(args.input, allow_pickle=False)
            if not isinstance(loaded, np.lib.npyio.NpzFile):
                raise ValueError('input must be an NPZ archive')
            with loaded as data:
                required = {'signal', 'target_f0', 'voiced', 'rate', 'envelope_hz'}
                if set(data.files) != required:
                    raise ValueError('unexpected NPZ keys')
                if data['rate'].shape != () or data['envelope_hz'].shape != ():
                    raise ValueError('rate and envelope_hz must be scalars')
                result = output_metrics(data['signal'], float(data['rate']), data['target_f0'],
                                        data['voiced'], float(data['envelope_hz']))
                passed = all_gates(result)
        encoded = json.dumps(result, indent=2, allow_nan=False)
        if args.output:
            args.output.write_text(encoded + '\n')
        else:
            print(encoded)
        return 0 if passed else 1
    except (ValueError, OSError, KeyError, AssertionError, EOFError, zipfile.BadZipFile) as error:
        print(f'calibration error: {error}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
