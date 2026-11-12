# WORLD source/filter experiment v1 — 2026-10-28

**The uniform waveform-periodicity comparator passes all42 retained synthetic rows.
Earlier fixed estimators remain negative: DIO40/42, Harvest24/42, YIN31/42 and
REAPER25/42. This remains an offline diagnostic, not a qualified product
pitch/formant backend.** No production dependency, host
control, audio callback integration, quality mode, latency claim or manual test
is delivered. The original pitch backend and its failing gates are untouched.

## Reproduction and implementation boundary

WORLD repository https://github.com/mmorise/World, pin
`c794856b537758a3d300757db2e5e31685a822c2`. Initial external checkout was
`/tmp/darkvoice-world-audit`; regenerated checkout is in ignored
`build/qualification/world-upstream`. The experiment compiles only the eight explicitly
listed BSD implementation files below and the independently authored project FFT
adapter. The adapter author did not read/copy WORLD `src/fft.cpp`, and this experiment
does not compile it; the earlier separate licence audit did inspect its
provenance. That file's embedded
Ooura implementation has an unresolved permission-provenance gap. No upstream
source, executable or generated audio is vendored or committed. Upstream source
and binaries must not later enter product distributions without an explicit
source whitelist and notice review. WORLD BSD and macrodefinitions MIT notices
are retained in full below. This is an open-source diagnostic route, not a claim
that the upstream FFT's provenance was resolved.

Own FFT uses standard negative-exponent forward, positive-exponent inverse and
unnormalized inverse (roundtrip N), radix-2 butterflies, prepared twiddles and
bit-reversal indices. Execution copies all inputs before emitting output, so
C2C and packed-real in-place operation is supported. Plans own scratch allocated
with exception-safe temporary ownership before publication. The header plan is
an owning C value: do not destroy copied plans twice or share one scratch across
concurrent executions. This is an offline diagnostic API, not public product API.

Strict O2/Wextra/Werror applies to both authored C++ files. The unchanged upstream
files compile with O2; no claim is made that upstream passes the project warnings.
Small N2..32 are checked against independent direct DFT. Powers2..131072 have
complex/real roundtrip, Parseval, analytic sine/DC/Nyquist and in-place checks.
WORLD's actual minimum-phase complex-transform caller is independently tested
against analytic stable AR(1), coefficient .7, over2049 spectrum bins. Combined:
**1,181,835 successful assertions and zero allocations during FFT execute.**
ASan/UBSan self-test also passed all1,181,835 assertions, leak detection and
halt-on-error enabled; no sanitizer finding. This does not assert no allocations
in upstream WORLD analysis or synthesis, which are offline-only here.

Example build, from this worktree (directory names are freely replaceable):

```python
from pathlib import Path
import subprocess
source = Path('build/qualification/world-upstream')
assert subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'], text=True).strip() == 'c794856b537758a3d300757db2e5e31685a822c2'
build = Path('build/qualification/world-build')
build.mkdir(exist_ok=True)
objects = []
for name in ('cheaptrick','common','d4c','dio','harvest','matlabfunctions','stonemask','synthesis'):
    obj = build/(name+'.o')
    subprocess.run(['c++','-std=c++20','-O2','-I'+str(source/'src'),'-c',str(source/'src'/(name+'.cpp')),'-o',str(obj)], check=True)
    objects.append(str(obj))
reaper = Path('build/qualification/reaper-audit')
assert subprocess.check_output(['git','-C',str(reaper),'rev-parse','HEAD'], text=True).strip() == '1d6e9b95e6b08b500fccbc9a043989dbda747276'
for name in ('epoch_tracker','fft','fd_filter','lpc_analyzer'):
    obj = build/('reaper_'+name+'.o')
    subprocess.run(['c++','-std=c++20','-O2','-I'+str(reaper),'-c',str(reaper/'epoch_tracker'/(name+'.cc')),'-o',str(obj)], check=True)
    objects.append(str(obj))
for name in ('world_fft_shim','world_probe'):
    obj = build/(name+'.o')
    subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-Iinclude','-I'+str(reaper),'-I'+str(source/'src'),'-c','tools/qualification/'+name+'.cpp','-o',str(obj)], check=True)
    objects.append(str(obj))
obj = build/'PitchDetector.o'
subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-Iinclude','-c','src/PitchDetector.cpp','-o',str(obj)], check=True)
objects.append(str(obj))
subprocess.run(['c++',*objects,'-o',str(build/'world_probe')], check=True)
out = Path('/tmp/darkvoice-world-measurements')
out.mkdir(exist_ok=True)
subprocess.run([str(build/'world_probe'),'--fft-self-test'], check=True)
subprocess.run([str(build/'world_probe'),str(out)], check=True)
subprocess.run(['python3','-B','tools/qualification/world_analyse.py',str(out)], check=True)
```

The analyser requires NumPy only for developer measurement. Successful probe or
analyser exit denotes well-formed measurements, **not** passing product quality.
Malformed byte lengths, nonfinite/zero output, invalid F0/time contours and
inconsistent F0 metadata are rejected. Eight adversarial analyser cases were
executed: truncated/trailing-byte output, NaN output, zero output, bad time
contour, negative F0, inconsistent metadata and NaN metadata. Three invalid
probe CLI shapes also returned usage error2.

## Predeclared signal and processing

Three original48k two-second Gaussian-envelope vowels, with identical harmonic
float accumulation to retained fixtures:80 harmonics, amplitude
`.025*(exp(-.5*((h*F0-center)/100)^2)+.002/h)`. Six renders are two separately
reported estimators, each these same three cases:

- F0100, envelope600, pitch+12, formant0.
- F070, envelope600, pitch−12, formant0.
- F0180, envelope1200, pitch+12, formant+7.

Primary estimator DIO with StoneMask; comparator Harvest. Both use fixed50..800Hz
range and5ms frame grid; all other options remain upstream defaults. CheapTrick
FFT4096 and configured50Hz analysis floor are fixed across all six. Actual
CheapTrick replacement floor is `3*48000/(4096-3)=35.181Hz`, below input70Hz.
The separate synthesis cutoff uses integer division `48000/4096+1=12Hz`, so
output35Hz is not replaced. No oracle F0 is supplied to analysis or synthesis.

CheapTrick power envelope is linearly sampled at `outputBin/formantFactor`,
clamping only to the Nyquist edge. D4C aperiodicity is unchanged. Nonzero estimated
F0 is multiplied by pitch factor; zero/unvoiced estimates stay zero. No hidden
normalization, hard clipping, excitation replacement, forced periodicity,
fixture-specific fallback or parameter search. Output is96000 finite nonzero
float32 frames for every row. The output cast is explicitly checked for overflow.

Input estimate gates: final-second voiced coverage>=.90 and absolute median
cents error<=20. Output gates: retained intended-period correlation>.8 and
broad200..3000Hz harmonic-grid envelope peak error<20%. Separate Hann-windowed
unconstrained FFT peaks are reported, not substituted for the retained grid gate.
The final48000 emitted frames are analyzed without shifting/cropping to find a
better region. No elapsed-time or real-time performance claim is made.

## Results

|Estimator / case|Input voiced fraction|Median F0 cents|Target-period correlation|Grid envelope peak Hz|Independent Hann peak Hz|Narrow subset|
|---|---:|---:|---:|---:|---:|---|
|dio-0|0.995000|0.110446|0.987077087|600.000|600.033145|pass|
|dio-1|0.990000|-0.078761|0.959734566|595.000|594.903363|pass|
|dio-2|1.000000|-0.076957|0.980928262|1800.000|1798.240025|pass|
|harvest-0|0.985000|-0.007199|0.972173654|600.000|599.996951|pass|
|harvest-1|0.975000|0.009047|0.973886983|630.000|594.999949|pass|
|harvest-2|1.000000|0.012673|0.986166740|1800.000|1800.012132|pass|

Expected envelope targets600/600/1797.968492Hz. Both envelope measurements pass
20% for all six rows. The70→35Hz rows have odd-harmonic captured/total energy
about.48148/.48159, versus~4.57e−15 for unchanged70 and even-only35 controls.
This supports actual odd-grid content in these fixtures; it is not a universal
fundamental estimator or a newly weakened threshold. Ideal35 has~.5 odd fraction.
All three controls correlate~.999426 at the35Hz period, explicitly preserving the
known octave ambiguity of correlation alone. Candidate captured-grid fraction is
reported too: DIO third case .9090 versus Harvest .99884; passing the retained
correlation/envelope gates does not erase this difference or qualify naturalness.

Raw audio and F0/time contours plus detailed JSON are local generated measurements
in `/tmp/darkvoice-world-measurements`, not committed artifacts. The deterministic
probe can regenerate them. Reports preserve numeric results if temporary storage
is cleared. The narrow results were preserved before the separately authorized42-render
expansion below.

## Expansion manifest and unresolved work

The retained suite is **149 assertions over42 renders**, not149 audio cases:
`tests/sculptor_tests.cpp` in the preserved pitch-coupling-audit worktree at
`fa4af2971779737acdd72e7ca9f37070a926aabc` (`qualification` CLI):

- `accuracy`:16 sine renders, four rates44.1/48/88.2/96k × pitch−12/−7/+7/+12,
  Studio setting, input110Hz. These explicitly disable formant preservation;
  WORLD expansion must predeclare envelope shift by pitch factor for these cases.
- `formants`:three48k multiresonance vowels, F0100, centers600/1200/2500;
  (pitch,formant)=(+7,0),(0,+7),(0,−7). Two independent geometrically separated
  peak gates and retained correlation gate.
- `isolatedFormants`:four48k/F0100 renders, center600/1200 × formant±7.
- `envelopeMatrix`:16 renders, rates48/96k × F070/180 × centers600/1200 × formant±7.
- `combinedPeriodicity`:the three original failing cases investigated here.

A further optional `matrix` CLI has180 renders/540 assertions, separate from149.
No new matrix policy or source changes were made. For a future common WORLD
configuration, FFT4096 at44.1/48k and8192 at88.2/96k keeps actual analysis floor
below50Hz. No UV/transient fixtures occur in the149; passing it would still leave
those, genuine streaming, mono/stereo, reset/automation, no-allocation integration,
latency, Live/Studio/Offline, CPU/deadlines, samples/listening and DAW qualification.
Upstream allocating analysis/synthesis remains offline-only in this diagnostic.
Manual testing is deferred under the user's instruction.

## Retained42 expansion — measured outcome

Lead contract73d098e authorized a fixed primaryDIO+StoneMask expansion after
narrow3 passed. **40/42 substantive rows pass;2 fail.** Input-estimator criteria
also fail those same2 rows; they remain separately reported. All42 outputs have
the expected duration, finite nonzero float samples. Exact original durations:
16 sine cases1.5seconds;26 vowel cases2seconds. All metrics use the final half,
including the original sine zero-crossing estimator and20cent threshold.
FFT4096/8192 follows declared rate policy; no new estimator settings were tuned.

Rows21/22 are the same input (48k,F0100,Gaussian envelope1200) with isolated
formant−7/+7. The estimated input F0 has median−62.251799cents error, voiced
coverage.995; intended-grid output correlation is.237860245/−.649135040, failing
>.8. Broad envelope peaks800/1900Hz remain within20% of their800.903913/
1797.968492Hz targets. Reporting passing envelope gates cannot erase these
periodicity failures. No input-oracle replacement, alternate estimator fallback,
fixture removal or threshold relaxation was applied.

Both original retained substantive gates and added estimator gates remain named
separately in detailed JSON. The original149 includes backend lifecycle/render
assertions, so **this is not a claim149 checks passed**. The3 combined regressions
remain passing under this WORLD diagnostic. A new backend remains unqualified.
No optional180-render matrix, streaming or product implementation was performed.

Run the same executable/analyser with these extra arguments:

```sh
mkdir -p /tmp/darkvoice-world-retained
/tmp/darkvoice-world-build/world_probe --suite retained /tmp/darkvoice-world-retained
python3 -B tools/qualification/world_analyse.py --suite retained /tmp/darkvoice-world-retained
```

|Row|Rate|Signal|F0|Pitch|Formant|Input cents|Correlation|Sine cents or envelope error|Retained row|Input estimate|
|---|---:|---|---:|---:|---:|---:|---:|---|---|---|
|retained-0|44100|sine|110|-12|0|-1.890236|0.995373626|-1.613310c|pass|pass|
|retained-1|44100|sine|110|-7|0|-1.890236|0.991116978|-1.909876c|pass|pass|
|retained-2|44100|sine|110|7|0|-1.890236|0.995546525|-0.754732c|pass|pass|
|retained-3|44100|sine|110|12|0|-1.890236|0.998389929|-0.649549c|pass|pass|
|retained-4|48000|sine|110|-12|0|-1.426020|0.994473044|-0.697667c|pass|pass|
|retained-5|48000|sine|110|-7|0|-1.426020|0.997121250|0.675834c|pass|pass|
|retained-6|48000|sine|110|7|0|-1.426020|0.995503777|0.078219c|pass|pass|
|retained-7|48000|sine|110|12|0|-1.426020|0.997592228|0.291289c|pass|pass|
|retained-8|88200|sine|110|-12|0|-2.070688|0.995419012|-1.668291c|pass|pass|
|retained-9|88200|sine|110|-7|0|-2.070688|0.991046631|-1.966768c|pass|pass|
|retained-10|88200|sine|110|7|0|-2.070688|0.995544528|-0.796232c|pass|pass|
|retained-11|88200|sine|110|12|0|-2.070688|0.998439729|-0.139442c|pass|pass|
|retained-12|96000|sine|110|-12|0|-1.425122|0.994444581|-0.656105c|pass|pass|
|retained-13|96000|sine|110|-7|0|-1.425122|0.996920354|0.746082c|pass|pass|
|retained-14|96000|sine|110|7|0|-1.425122|0.995460007|0.093818c|pass|pass|
|retained-15|96000|sine|110|12|0|-1.425122|0.997579103|0.305186c|pass|pass|
|retained-16|48000|multi|100|7|0|0.107183|0.987211242|0.001129/0.001129|pass|pass|
|retained-17|48000|multi|100|0|7|0.107183|0.974724254|0.001130/0.001130|pass|pass|
|retained-18|48000|multi|100|0|-7|0.107183|0.987118716|0.001129/0.001129|pass|pass|
|retained-19|48000|gaussian|100|0|-7|0.110446|0.991716885|0.001129|pass|pass|
|retained-20|48000|gaussian|100|0|7|0.110446|0.988086341|0.001130|pass|pass|
|retained-21|48000|gaussian|100|0|-7|-62.251799|0.237860245|0.001129|FAIL|FAIL|
|retained-22|48000|gaussian|100|0|7|-62.251799|-0.649135040|0.056748|FAIL|FAIL|
|retained-23|48000|gaussian|70|0|-7|-0.078761|0.978990091|0.048815|pass|pass|
|retained-24|48000|gaussian|70|0|7|-0.078761|0.962813933|0.012254|pass|pass|
|retained-25|48000|gaussian|70|0|-7|0.022023|0.958276986|0.038586|pass|pass|
|retained-26|48000|gaussian|70|0|7|0.022023|0.854254843|0.012254|pass|pass|
|retained-27|48000|gaussian|180|0|-7|0.070704|0.993556694|0.101016|pass|pass|
|retained-28|48000|gaussian|180|0|7|0.070704|0.981781759|0.001130|pass|pass|
|retained-29|48000|gaussian|180|0|-7|-0.076957|0.994222635|0.101016|pass|pass|
|retained-30|48000|gaussian|180|0|7|-0.076957|0.978349259|0.098983|pass|pass|
|retained-31|96000|gaussian|70|0|-7|0.160181|0.965548830|0.048815|pass|pass|
|retained-32|96000|gaussian|70|0|7|0.160181|0.959934071|0.012254|pass|pass|
|retained-33|96000|gaussian|70|0|-7|0.022376|0.979041819|0.038586|pass|pass|
|retained-34|96000|gaussian|70|0|7|0.022376|0.911696013|0.012254|pass|pass|
|retained-35|96000|gaussian|180|0|-7|0.069332|0.993289364|0.101016|pass|pass|
|retained-36|96000|gaussian|180|0|7|0.069332|0.987222256|0.001130|pass|pass|
|retained-37|96000|gaussian|180|0|-7|-0.151112|0.997332727|0.101016|pass|pass|
|retained-38|96000|gaussian|180|0|7|-0.151112|0.989091109|0.098983|pass|pass|
|retained-39|48000|combined|100|12|0|0.110446|0.987077087|0.000000|pass|pass|
|retained-40|48000|combined|70|-12|0|-0.078761|0.959734566|0.008333|pass|pass|
|retained-41|48000|combined|180|12|7|-0.076957|0.980928262|0.001130|pass|pass|

Envelope errors are relative fractions; multi shows the two separate peak errors.
Sine retained gate uses crossing-frequency cents, not the diagnostic correlation.
F0 estimates are stored before pitch modification for independent reproduction.
The raw42 data/contours and JSON are in `/tmp/darkvoice-world-retained`.

## Harvest42 comparator — preserved negative outcome

The predeclared Harvest comparator was expanded unchanged under contract03e5757.
It passes24/42 retained rows and fails18; the same18 input estimator criteria
fail. All16 pure-sine accuracy rows have only.013333 voiced fraction, median
voiced error−22.89..−35.86cents, and output crossing-frequency errors~1609..1985c.
The48k F0100/envelope1200 isolatedformant±7 rows have.435 voiced fraction;
correlations.068488913/.097713589 fail>.8, despite passing broad envelope gates.
No unvoiced filling, forcing, interpolation or fallback was introduced.

OriginalOctober28 Harvest results were regenerated onOctober31 after temporary
storage was cleared. The full negative table is retained below and detailed JSON
plus raw outputs/contours persist in ignored
`build/qualification/world-retained-harvest`. DIO's exact frozen source/report/
JSON remain in `build/qualification/world-dio42-frozen`. Both negative outcomes
are preserved independently, not combined per fixture into a hybrid processor.

Use `--suite retained-harvest` with both executable and analyser.

|Row|Rate|Signal|Input F0|Pitch|Formant|Voiced fraction|Input cents|Output correlation|Retained error|Retained row|Estimator|
|---|---:|---|---:|---:|---:|---:|---:|---:|---|---|---|
|retained-harvest-0|44100|sine|110|-12|0|0.013333|-22.887372|-0.066159124|1984.809132c|FAIL|FAIL|
|retained-harvest-1|44100|sine|110|-7|0|0.013333|-22.887372|-0.039999760|1775.278151c|FAIL|FAIL|
|retained-harvest-2|44100|sine|110|7|0|0.013333|-22.887372|0.056451477|1729.445185c|FAIL|FAIL|
|retained-harvest-3|44100|sine|110|12|0|0.013333|-22.887372|-0.043495347|1608.713404c|FAIL|FAIL|
|retained-harvest-4|48000|sine|110|-12|0|0.013333|-35.862509|0.006230815|1885.306604c|FAIL|FAIL|
|retained-harvest-5|48000|sine|110|-7|0|0.013333|-35.862509|0.057668743|1810.669296c|FAIL|FAIL|
|retained-harvest-6|48000|sine|110|7|0|0.013333|-35.862509|-0.066773898|1695.629358c|FAIL|FAIL|
|retained-harvest-7|48000|sine|110|12|0|0.013333|-35.862509|-0.036653590|1648.316361c|FAIL|FAIL|
|retained-harvest-8|88200|sine|110|-12|0|0.013333|-35.722285|-0.038793522|1968.721325c|FAIL|FAIL|
|retained-harvest-9|88200|sine|110|-7|0|0.013333|-35.722285|0.104676612|1814.881894c|FAIL|FAIL|
|retained-harvest-10|88200|sine|110|7|0|0.013333|-35.722285|-0.081769103|1734.371240c|FAIL|FAIL|
|retained-harvest-11|88200|sine|110|12|0|0.013333|-35.722285|-0.005404241|1645.606899c|FAIL|FAIL|
|retained-harvest-12|96000|sine|110|-12|0|0.013333|-35.789571|0.019489986|1973.619704c|FAIL|FAIL|
|retained-harvest-13|96000|sine|110|-7|0|0.013333|-35.789571|-0.006099135|1878.974978c|FAIL|FAIL|
|retained-harvest-14|96000|sine|110|7|0|0.013333|-35.789571|0.056064936|1697.158842c|FAIL|FAIL|
|retained-harvest-15|96000|sine|110|12|0|0.013333|-35.789571|0.011674280|1628.759012c|FAIL|FAIL|
|retained-harvest-16|48000|multi|100|7|0|0.985000|-0.023694|0.970741698|0.001129/0.001129|pass|pass|
|retained-harvest-17|48000|multi|100|0|7|0.985000|-0.023694|0.977653575|0.001130/0.001130|pass|pass|
|retained-harvest-18|48000|multi|100|0|-7|0.985000|-0.023694|0.976137724|0.001129/0.001129|pass|pass|
|retained-harvest-19|48000|gaussian|100|0|-7|0.985000|-0.007199|0.991377588|0.001129|pass|pass|
|retained-harvest-20|48000|gaussian|100|0|7|0.985000|-0.007199|0.973257517|0.001130|pass|pass|
|retained-harvest-21|48000|gaussian|100|0|-7|0.435000|4.715334|0.068488913|0.001129|FAIL|FAIL|
|retained-harvest-22|48000|gaussian|100|0|7|0.435000|4.715334|0.097713589|0.112367|FAIL|FAIL|
|retained-harvest-23|48000|gaussian|70|0|-7|0.975000|0.009047|0.966860910|0.048815|pass|pass|
|retained-harvest-24|48000|gaussian|70|0|7|0.975000|0.009047|0.964784996|0.012254|pass|pass|
|retained-harvest-25|48000|gaussian|70|0|-7|0.975000|-0.491252|0.925993414|0.038586|pass|pass|
|retained-harvest-26|48000|gaussian|70|0|7|0.975000|-0.491252|0.892673890|0.012254|pass|pass|
|retained-harvest-27|48000|gaussian|180|0|-7|1.000000|-0.001953|0.998785738|0.101016|pass|pass|
|retained-harvest-28|48000|gaussian|180|0|7|1.000000|-0.001953|0.994191457|0.001130|pass|pass|
|retained-harvest-29|48000|gaussian|180|0|-7|1.000000|0.012673|0.992799490|0.101016|pass|pass|
|retained-harvest-30|48000|gaussian|180|0|7|1.000000|0.012673|0.980214666|0.001130|pass|pass|
|retained-harvest-31|96000|gaussian|70|0|-7|0.975000|0.010200|0.982635645|0.048815|pass|pass|
|retained-harvest-32|96000|gaussian|70|0|7|0.975000|0.010200|0.974690424|0.012254|pass|pass|
|retained-harvest-33|96000|gaussian|70|0|-7|0.975000|-0.878481|0.963142045|0.038586|pass|pass|
|retained-harvest-34|96000|gaussian|70|0|7|0.975000|-0.878481|0.892628571|0.012254|pass|pass|
|retained-harvest-35|96000|gaussian|180|0|-7|1.000000|-0.001905|0.998896174|0.101016|pass|pass|
|retained-harvest-36|96000|gaussian|180|0|7|1.000000|-0.001905|0.994898639|0.001130|pass|pass|
|retained-harvest-37|96000|gaussian|180|0|-7|1.000000|0.014939|0.993790638|0.101016|pass|pass|
|retained-harvest-38|96000|gaussian|180|0|7|1.000000|0.014939|0.982545237|0.001130|pass|pass|
|retained-harvest-39|48000|combined|100|12|0|0.985000|-0.007199|0.972173654|0.000000|pass|pass|
|retained-harvest-40|48000|combined|70|-12|0|0.975000|0.009047|0.973886983|0.050000|pass|pass|
|retained-harvest-41|48000|combined|180|12|7|1.000000|0.012673|0.986166740|0.001130|pass|pass|

Retained error is sine cents, single-envelope relative error, or the two separate
multi-envelope relative errors. All fractional envelope errors use20% gate.
Pure-sine retained gates use crossing-frequency cents, independent of reported
correlation. Their full errors and independent spectral peaks remain in JSON.
The failed18 are rows0..15 and21/22. This comparator remains unqualified.

## Fixed project YIN comparator — 2026-10-31

Lead contractbc9f2d3 predeclared a third independent comparator, using unchanged
project `PitchDetector` (YIN) rather than changing WORLD's DIO/Harvest settings.
Every5ms WORLD frame is analyzed using a centered4096-sample float window at
44.1/48k or8192 at88.2/96k, zero-padded outside the original input. Start index is
`floor(frameTime*rate)-N/2`, using WORLD's exact frame-grid arithmetic. Detector
range50..800Hz; voiced frequency or0 is used exactly as returned. No filling,
quantization, smoothing, oracle, per-fixture fallback or detector code change.
CheapTrick/D4C/envelope transformation/synthesis and all metrics remain unchanged.

The detector is explicitly worker/offline O(window*maximumLag). Probe logs report
actual cloud wall seconds per complete render; these are observations, not a Live
CPU/deadline guarantee. Existing unchanged detector source/header fingerprints
are included below. `--suite retained-yin` selects the same42 manifest;
`--suite narrow-yin` selects the original3 fixtures. No product code was changed.

### Fixed YIN measured negative outcome

The fixed comparator passes31/42 retained rows and fails11. The same11 input
estimator gates fail; source estimates are biased rather than replaced by oracle
F0. Failed rows21/22,25/26,29/30,33/34,37/38 and41 are Gaussian1200Hz envelope
fixtures. Biases are approximately+103.7,+150.1 or+275.8cents, with intended-grid
output correlations all below>.8. The existing detector selects its first CMNDF
trough below.15; no threshold/search alteration was made in this experiment.
This is evidence of an estimator limitation, not permission to change the output
target or discard difficult fixtures. WORLD with this fixed estimator remains
unqualified. No additional YIN parameter search was performed.

|Row|Rate|Input F0|Envelope|Pitch|Formant|Voiced fraction|Input cents|Output correlation|Retained error|Retained row|Estimator|
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|
|retained-yin-0|44100|110|0|-12|0|1.000000|0.005242|0.993128598|0.229870c|pass|pass|
|retained-yin-1|44100|110|0|-7|0|1.000000|0.005242|0.997658166|1.106860c|pass|pass|
|retained-yin-2|44100|110|0|7|0|1.000000|0.005242|0.995033361|1.120790c|pass|pass|
|retained-yin-3|44100|110|0|12|0|1.000000|0.005242|0.995535102|1.546245c|pass|pass|
|retained-yin-4|48000|110|0|-12|0|1.000000|0.002773|0.990615887|0.192644c|pass|pass|
|retained-yin-5|48000|110|0|-7|0|1.000000|0.002773|0.997519633|-0.070428c|pass|pass|
|retained-yin-6|48000|110|0|7|0|1.000000|0.002773|0.995421237|1.256617c|pass|pass|
|retained-yin-7|48000|110|0|12|0|1.000000|0.002773|0.995581301|1.381540c|pass|pass|
|retained-yin-8|88200|110|0|-12|0|1.000000|0.001210|0.993149918|0.221603c|pass|pass|
|retained-yin-9|88200|110|0|-7|0|1.000000|0.001210|0.997565399|1.101412c|pass|pass|
|retained-yin-10|88200|110|0|7|0|1.000000|0.001210|0.995137858|1.126554c|pass|pass|
|retained-yin-11|88200|110|0|12|0|1.000000|0.001210|0.995503971|1.556341c|pass|pass|
|retained-yin-12|96000|110|0|-12|0|1.000000|0.000882|0.990503542|0.228032c|pass|pass|
|retained-yin-13|96000|110|0|-7|0|1.000000|0.000882|0.997408679|-0.069026c|pass|pass|
|retained-yin-14|96000|110|0|7|0|1.000000|0.000882|0.995400349|1.259850c|pass|pass|
|retained-yin-15|96000|110|0|12|0|1.000000|0.000882|0.995568168|1.388312c|pass|pass|
|retained-yin-16|48000|100|0|7|0|1.000000|0.003723|0.991629770|0.001129/0.001129|pass|pass|
|retained-yin-17|48000|100|0|0|7|1.000000|0.003723|0.992444931|0.001130/0.001130|pass|pass|
|retained-yin-18|48000|100|0|0|-7|1.000000|0.003723|0.995234575|0.001129/0.001129|pass|pass|
|retained-yin-19|48000|100|600|0|-7|1.000000|0.003775|0.995426458|0.001129|pass|pass|
|retained-yin-20|48000|100|600|0|7|1.000000|0.003775|0.994962713|0.001130|pass|pass|
|retained-yin-21|48000|100|1200|0|-7|1.000000|150.119530|-0.492312082|0.001129|FAIL|FAIL|
|retained-yin-22|48000|100|1200|0|7|1.000000|150.119530|-0.827846317|0.001130|FAIL|FAIL|
|retained-yin-23|48000|70|600|0|-7|0.995000|0.001119|0.992912699|0.048815|pass|pass|
|retained-yin-24|48000|70|600|0|7|0.995000|0.001119|0.993691046|0.012254|pass|pass|
|retained-yin-25|48000|70|1200|0|-7|0.990000|103.698249|-0.459804644|0.048815|FAIL|FAIL|
|retained-yin-26|48000|70|1200|0|7|0.990000|103.698249|-0.838607751|0.051186|FAIL|FAIL|
|retained-yin-27|48000|180|600|0|-7|1.000000|0.007547|0.999269044|0.101016|pass|pass|
|retained-yin-28|48000|180|600|0|7|1.000000|0.007547|0.997886071|0.001130|pass|pass|
|retained-yin-29|48000|180|1200|0|-7|1.000000|275.807802|-0.335936550|0.123730|FAIL|FAIL|
|retained-yin-30|48000|180|1200|0|7|1.000000|275.807802|-0.802714231|0.098983|FAIL|FAIL|
|retained-yin-31|96000|70|600|0|-7|0.995000|0.000227|0.994996324|0.048815|pass|pass|
|retained-yin-32|96000|70|600|0|7|0.995000|0.000227|0.995598332|0.012254|pass|pass|
|retained-yin-33|96000|70|1200|0|-7|0.995000|103.695175|-0.504853857|0.038586|FAIL|FAIL|
|retained-yin-34|96000|70|1200|0|7|0.995000|103.695175|-0.839514857|0.026679|FAIL|FAIL|
|retained-yin-35|96000|180|600|0|-7|1.000000|0.002136|0.999376029|0.101016|pass|pass|
|retained-yin-36|96000|180|600|0|7|1.000000|0.002136|0.998558146|0.001130|pass|pass|
|retained-yin-37|96000|180|1200|0|-7|1.000000|275.799082|-0.386905678|0.123730|FAIL|FAIL|
|retained-yin-38|96000|180|1200|0|7|1.000000|275.799082|-0.805157766|0.098983|FAIL|FAIL|
|retained-yin-39|48000|100|600|12|0|1.000000|0.003775|0.997579484|0.000000|pass|pass|
|retained-yin-40|48000|70|600|-12|0|0.995000|0.001119|0.988789761|0.008333|pass|pass|
|retained-yin-41|48000|180|1200|12|7|1.000000|275.807802|-0.357816000|0.001130|FAIL|FAIL|

The independent narrow3 comparator passes2 and fails1:

|Case|Input cents|Correlation|Grid envelope peak Hz|Retained row|Estimator|
|---|---:|---:|---:|---|---|
|yin-0|0.003775|0.997579484|600.000|pass|pass|
|yin-1|0.001119|0.988789761|595.000|pass|pass|
|yin-2|275.807802|-0.357816000|1800.000|FAIL|FAIL|

Measured per-render cloud wall times range1.419737–13.670727s,
sum227.874683s for42 renders (signal generation, offline estimation and
WORLD processing). Cloud contention affects walltime; these observations do not
establish real-time capacity. Analysis uses one BLAS thread to avoid developer
resource oversubscription; gates are unchanged. Persistent generated measurements
are in ignored `build/qualification/world-retained-yin` and `world-narrow-yin`.

## Fixed REAPER comparator — 2026-10-31

Lead contractfc443eb authorized one uniform REAPER trajectory, with narrow3 gate
before any retained42 expansion. Pin google/REAPER
`1d6e9b95e6b08b500fccbc9a043989dbda747276`, Apache2. Only
`epoch_tracker/{epoch_tracker,fft,fd_filter,lpc_analyzer}.cc` are compiled,
unchanged. All eight corresponding headers/source files carry Google2015
Apache2 notices; no NOTICE file occurs in this audited checkout. Licence and
source fingerprints are retained below; no REAPER source/binary is committed or
packaged as a product dependency. This remains an allocating offline diagnostic.

F0 analysis alone converts original float input by32768, round-to-nearest with
half ties away from zero, clamp to signed16-bit range. Clip counts are reported
per fixture. WORLD CheapTrick/D4C/synthesis receive the original float waveform,
never the quantized F0-analysis copy. REAPER Init always50..800Hz, highpass=false,
Hilbert=false; ComputeFeatures, TrackEpochs, ResampleAndReturnResults(.005f).
All other upstream defaults, including its own residual processing, stay fixed.
No per-fixture normalization or selection among estimators.

Timestamp convention was inspected before rendering: upstream resampler uses
`RoundUp(resid_index/(sample_rate*.005))`, and its MakeF0Output exporter assigns
returned indexi the timestampi*.005 starting0. Thus returned indexk maps directly
to WORLD framek. Upstream own nearest-epoch replication and endpoint extension
remain; this adapter adds no interpolation/hold/fill. Nonpositive values map0,
missing frames map0, extra trailing frames are truncated. Raw source contours,
source-frame counts and mapped contours are persisted and independently checked
by the analyser. Invalid metadata, nonfinite contours or any mapping discrepancy
are rejected. Zero clipping does not prove quantization is perceptually neutral.

REAPER narrow3 passed, authorizing the unchanged42-manifest expansion. All
three narrow renders and the1,181,835 FFT assertions also pass a fully rebuilt
ASan/UBSan executable with leak/halt enabled; no sanitizer finding. This is not
a42-render sanitizer claim. Six extra clipping/source-frame/index-mapping
rejection guards passed, in addition to the eight general measurement guards
and three CLI guards. Select
`--suite narrow-reaper` or `--suite retained-reaper` for executable/analyser.

### REAPER measured outcome

Narrow3 passes3/3, with zeroPCM clips. The fixed42 expansion passes25/42 retained
rows, fails17;26/42 input estimation gates pass. All16 pure110Hz sine inputs are
classified entirely unvoiced in the analysis region, failing both estimator and
sine-accuracy gates. Do not replace those estimates with a different algorithm.
The additional retained failure is row28:48k,F0180, Gaussian600, formant+7.
Its nominal harmonic-grid peak1080Hz versus898.984246Hz target gives
20.1355869% error, above the unchanged20% gate. Correlation.972686740 and
input median−2.162663cents pass. Independent unconstrained Hann peak898.875868Hz
passes its diagnostic20% envelope check; **it is not substituted for the retained
failed harmonic-grid gate**. This uniform REAPER/WORLD candidate remains
unqualified. No tuning, hybrid or further parameter search followed this failure.

|Narrow case|Input voiced fraction|Median cents|Output correlation|Grid envelope Hz|Source/WORLD frames|Missing/truncated|PCM clips|Row|
|---|---:|---:|---:|---:|---|---|---:|---|
|reaper-0|0.975000|0.000000|0.942172354|600.000|395/401|6/0|0|pass|
|reaper-1|0.970000|-0.721133|0.905534493|630.000|394/401|7/0|0|pass|
|reaper-2|0.975000|-2.162663|0.949331934|1800.000|398/401|3/0|0|pass|

|Row|Rate|Signal|F0|Pitch|Formant|Voiced fraction|Input cents|Output correlation|Retained error|Source/WORLD frames|PCM clips|Retained row|Estimator|
|---|---:|---|---:|---:|---:|---:|---|---:|---|---|---:|---|---|
|retained-reaper-0|44100|sine|110|-12|0|0.000000|n/a (unvoiced)|-0.067342699|1946.411971c|298/301|0|FAIL|FAIL|
|retained-reaper-1|44100|sine|110|-7|0|0.000000|n/a (unvoiced)|-0.081900867|1795.675118c|298/301|0|FAIL|FAIL|
|retained-reaper-2|44100|sine|110|7|0|0.000000|n/a (unvoiced)|0.050410231|1758.498825c|298/301|0|FAIL|FAIL|
|retained-reaper-3|44100|sine|110|12|0|0.000000|n/a (unvoiced)|-0.052887505|1629.581931c|298/301|0|FAIL|FAIL|
|retained-reaper-4|48000|sine|110|-12|0|0.000000|n/a (unvoiced)|0.048383357|1892.724692c|298/301|0|FAIL|FAIL|
|retained-reaper-5|48000|sine|110|-7|0|0.000000|n/a (unvoiced)|0.013283450|1857.839901c|298/301|0|FAIL|FAIL|
|retained-reaper-6|48000|sine|110|7|0|0.000000|n/a (unvoiced)|-0.028478210|1742.437078c|298/301|0|FAIL|FAIL|
|retained-reaper-7|48000|sine|110|12|0|0.000000|n/a (unvoiced)|-0.051917430|1643.474327c|298/301|0|FAIL|FAIL|
|retained-reaper-8|88200|sine|110|-12|0|0.000000|n/a (unvoiced)|-0.010410220|1977.678981c|298/301|0|FAIL|FAIL|
|retained-reaper-9|88200|sine|110|-7|0|0.000000|n/a (unvoiced)|-0.033394101|1858.455460c|298/301|0|FAIL|FAIL|
|retained-reaper-10|88200|sine|110|7|0|0.000000|n/a (unvoiced)|-0.055367890|1716.851060c|298/301|0|FAIL|FAIL|
|retained-reaper-11|88200|sine|110|12|0|0.000000|n/a (unvoiced)|-0.025356799|1677.113090c|298/301|0|FAIL|FAIL|
|retained-reaper-12|96000|sine|110|-12|0|0.000000|n/a (unvoiced)|-0.068443911|1882.796385c|298/301|0|FAIL|FAIL|
|retained-reaper-13|96000|sine|110|-7|0|0.000000|n/a (unvoiced)|-0.027319442|1813.797812c|298/301|0|FAIL|FAIL|
|retained-reaper-14|96000|sine|110|7|0|0.000000|n/a (unvoiced)|0.055304948|1735.999785c|298/301|0|FAIL|FAIL|
|retained-reaper-15|96000|sine|110|12|0|0.000000|n/a (unvoiced)|-0.002420329|1662.890335c|298/301|0|FAIL|FAIL|
|retained-reaper-16|48000|multi|100|7|0|0.980000|0.000000|0.969867368|0.001129/0.001129|396/401|0|pass|pass|
|retained-reaper-17|48000|multi|100|0|7|0.980000|0.000000|0.966933521|0.001130/0.001130|396/401|0|pass|pass|
|retained-reaper-18|48000|multi|100|0|-7|0.980000|0.000000|0.972820347|0.001129/0.001129|396/401|0|pass|pass|
|retained-reaper-19|48000|gaussian|100|0|-7|0.975000|0.000000|0.936499780|0.001129|395/401|0|pass|pass|
|retained-reaper-20|48000|gaussian|100|0|7|0.975000|0.000000|0.959909228|0.001130|395/401|0|pass|pass|
|retained-reaper-21|48000|gaussian|100|0|-7|0.980000|0.000000|0.946764481|0.001129|396/401|0|pass|pass|
|retained-reaper-22|48000|gaussian|100|0|7|0.980000|0.000000|0.878022430|0.001130|396/401|0|pass|pass|
|retained-reaper-23|48000|gaussian|70|0|-7|0.970000|-0.721133|0.947918355|0.048815|394/401|0|pass|pass|
|retained-reaper-24|48000|gaussian|70|0|7|0.970000|-0.721133|0.948543464|0.012254|394/401|0|pass|pass|
|retained-reaper-25|48000|gaussian|70|0|-7|0.980000|-0.721133|0.948541319|0.038586|396/401|0|pass|pass|
|retained-reaper-26|48000|gaussian|70|0|7|0.980000|-0.721133|0.969267026|0.026679|396/401|0|pass|pass|
|retained-reaper-27|48000|gaussian|180|0|-7|0.975000|-2.162663|0.983192818|0.101016|398/401|0|pass|pass|
|retained-reaper-28|48000|gaussian|180|0|7|0.975000|-2.162663|0.972686740|0.201356|398/401|0|FAIL|pass|
|retained-reaper-29|48000|gaussian|180|0|-7|0.975000|-2.162663|0.950565006|0.101016|398/401|0|pass|pass|
|retained-reaper-30|48000|gaussian|180|0|7|0.975000|-2.162663|0.958814528|0.101243|398/401|0|pass|pass|
|retained-reaper-31|96000|gaussian|70|0|-7|0.970000|0.541077|0.956470953|0.048815|394/401|0|pass|pass|
|retained-reaper-32|96000|gaussian|70|0|7|0.970000|0.541077|0.944636686|0.012254|394/401|0|pass|pass|
|retained-reaper-33|96000|gaussian|70|0|-7|0.980000|0.541077|0.900548591|0.038586|396/401|0|pass|pass|
|retained-reaper-34|96000|gaussian|70|0|7|0.980000|0.541077|0.960222936|0.026679|396/401|0|pass|pass|
|retained-reaper-35|96000|gaussian|180|0|-7|0.980000|1.082299|0.992092664|0.101016|396/401|0|pass|pass|
|retained-reaper-36|96000|gaussian|180|0|7|0.980000|1.082299|0.966368946|0.199096|396/401|0|pass|pass|
|retained-reaper-37|96000|gaussian|180|0|-7|0.975000|1.082299|0.963422072|0.101016|398/401|0|pass|pass|
|retained-reaper-38|96000|gaussian|180|0|7|0.975000|1.082299|0.981619060|0.101243|398/401|0|pass|pass|
|retained-reaper-39|48000|combined|100|12|0|0.975000|0.000000|0.942172354|0.000000|395/401|0|pass|pass|
|retained-reaper-40|48000|combined|70|-12|0|0.970000|-0.721133|0.905534493|0.050000|394/401|0|pass|pass|
|retained-reaper-41|48000|combined|180|12|7|0.975000|-2.162663|0.949331934|0.001130|398/401|0|pass|pass|

Retained error uses the same units as the preceding tables. Zero-voiced medians
are undefined: table reports n/a; probe/JSON stores the explicit finite1e30
failure sentinel, never an inferred measurement. Source-frame shortfall is
mapped0 and reported in detailedJSON; no returned frame exceeds WORLD duration
in this fixed set. Raw source contours and mapping/clipping metadata are in
ignored `build/qualification/world-{narrow,retained}-reaper`.

## Uniform waveform-periodicity comparator — checkpoint 2026-11-12

Lead contracts2a5e8ac/fab8418 define one waveform-driven selection/refinement
algorithm used uniformly on every fixture. It obtains the four unchanged fixed
DIO/StoneMask, Harvest, YIN and REAPER candidate contours. At every5ms frame it
scores each available candidate against the same mean-centered input window,
clamped to the source boundaries:4096 frames at44.1/48k,8192 at88.2/96k.
A candidate's integer-lag local NCCF maximum receives the prescribed parabolic
refinement; fractional-lag NCCF scores choose the winner. The global maximum's
1e-6 tie set prefers the highest frequency; score>=.9 and50..800Hz are required
for voicing. No expected fixture F0, parameter routing by fixture, musical-note
rounding, gap filling or output normalization participates in selection.
An explicit backend analysis failure makes only that candidate unavailable and
is reported; exceptions remain hard failures. WORLD analysis, envelope warping,
aperiodicity and synthesis policy remain unchanged.

**All42 retained renders pass both their original substantive output gates and
the separately declared input-estimation gates.** The three original narrow
combined cases also pass. This is not a claim that the old backend's149 lifecycle
and numeric assertions ran here. The complete42-case manifest and all earlier
negative tables above are preserved. In particular, independent Hann peaks never
replace the retained nominal harmonic-grid envelope measurement.

The checkpoint freshly compiles all eight whitelisted WORLD files, four REAPER
files, unchanged project PitchDetector and the two authored C++ files. The authored
files and project detector pass strict C++20/O2/Wall/Wextra/Wpedantic/Werror.
The FFT self-test passes1,181,835 assertions with zero execute allocations.
Reanalysis of saved42 renders independently recomputes every candidate period,
NCCF score, tie selection and selected F0 in NumPy; stored selected indices agree
exactly. This reanalysis validates existing raw measurements, not a new42-render
run. A separate fresh narrow3 render from the checkpoint binary is recorded below.
No fresh42-render sanitizer or realtime test is claimed.

Across the42 rows, minimum final-half voiced coverage is0.990000; maximum absolute median input error is0.002529084cents. Maximum independent recomputation errors are9.402e-11samples (period),9.437e-15 (NCCF score) and1.185e-11Hz (F0). No PCM clipping occurred.

|Row|Rate|Signal|Input F0|Pitch|Formant|Median input cents|Intended-period correlation|Retained / input gates|
|---:|---:|---|---:|---:|---:|---:|---:|---|
|0|44100|sine|110|-12|0|-0.000060259|0.993202908|pass / pass|
|1|44100|sine|110|-7|0|-0.000060259|0.997662744|pass / pass|
|2|44100|sine|110|7|0|-0.000060259|0.995104746|pass / pass|
|3|44100|sine|110|12|0|-0.000060259|0.995558815|pass / pass|
|4|48000|sine|110|-12|0|-0.000137170|0.993091249|pass / pass|
|5|48000|sine|110|-7|0|-0.000137170|0.997611890|pass / pass|
|6|48000|sine|110|7|0|-0.000137170|0.995186262|pass / pass|
|7|48000|sine|110|12|0|-0.000137170|0.995602894|pass / pass|
|8|88200|sine|110|-12|0|-0.000014225|0.993251347|pass / pass|
|9|88200|sine|110|-7|0|-0.000014225|0.997563129|pass / pass|
|10|88200|sine|110|7|0|-0.000014225|0.995261518|pass / pass|
|11|88200|sine|110|12|0|-0.000014225|0.995572826|pass / pass|
|12|96000|sine|110|-12|0|-0.000052702|0.993100848|pass / pass|
|13|96000|sine|110|-7|0|-0.000052702|0.997497883|pass / pass|
|14|96000|sine|110|7|0|-0.000052702|0.995129780|pass / pass|
|15|96000|sine|110|12|0|-0.000052702|0.995571566|pass / pass|
|16|48000|multi|100|7|0|0.000003198|0.991617904|pass / pass|
|17|48000|multi|100|0|7|0.000003198|0.992465717|pass / pass|
|18|48000|multi|100|0|-7|0.000003198|0.995234706|pass / pass|
|19|48000|gaussian|100|0|-7|-0.000228536|0.995427284|pass / pass|
|20|48000|gaussian|100|0|7|-0.000228536|0.994965069|pass / pass|
|21|48000|gaussian|100|0|-7|0.000289224|0.970888255|pass / pass|
|22|48000|gaussian|100|0|7|0.000289224|0.961292166|pass / pass|
|23|48000|gaussian|70|0|-7|-0.000267038|0.994531358|pass / pass|
|24|48000|gaussian|70|0|7|-0.000267038|0.993221625|pass / pass|
|25|48000|gaussian|70|0|-7|-0.001017462|0.975428901|pass / pass|
|26|48000|gaussian|70|0|7|-0.001017462|0.990010003|pass / pass|
|27|48000|gaussian|180|0|-7|-0.001005945|0.998558168|pass / pass|
|28|48000|gaussian|180|0|7|-0.001005945|0.996015042|pass / pass|
|29|48000|gaussian|180|0|-7|-0.002529084|0.976314876|pass / pass|
|30|48000|gaussian|180|0|7|-0.002529084|0.974363345|pass / pass|
|31|96000|gaussian|70|0|-7|0.000019701|0.994298811|pass / pass|
|32|96000|gaussian|70|0|7|0.000019701|0.993973870|pass / pass|
|33|96000|gaussian|70|0|-7|0.000075098|0.971841511|pass / pass|
|34|96000|gaussian|70|0|7|0.000075098|0.990178388|pass / pass|
|35|96000|gaussian|180|0|-7|-0.000005418|0.999376304|pass / pass|
|36|96000|gaussian|180|0|7|-0.000005418|0.998548000|pass / pass|
|37|96000|gaussian|180|0|-7|0.000298408|0.977102032|pass / pass|
|38|96000|gaussian|180|0|7|0.000298408|0.978372009|pass / pass|
|39|48000|combined|100|12|0|-0.000228536|0.997345756|pass / pass|
|40|48000|combined|70|-12|0|-0.000267038|0.991349845|pass / pass|
|41|48000|combined|180|12|7|-0.002529084|0.971887969|pass / pass|

Waveform-only controls preserve octave ambiguity explicitly. The ideal35Hz signal
is outside this comparator's fixed50Hz floor and is unvoiced; it is not falsely
reported as an accurate35Hz estimate. Unchanged70 and even-only35 signals both
produce approximately70Hz, while their diagnostic35Hz-period correlations also
approach1. Neither correlation alone proves a new fundamental. Noise and silence
remain unvoiced. Silence records REAPER TrackEpochs status3 (unavailable); no
candidate is forced voiced. These controls do not establish general voiced/
unvoiced discrimination on real vocals.

|Control|Final-half voiced fraction|Median estimated Hz|Diagnostic NCCF at35Hz|Diagnostic NCCF at70Hz|
|---|---:|---:|---:|---:|
|ideal35|0.000000|n/a|0.999999984|-0.000000541|
|unchanged70|0.995000|69.999989203|0.999999984|0.999999989|
|evenOnly35|0.995000|69.999989201|0.999999984|0.999999989|
|noise|0.000000|n/a|0.017136766|0.012027766|
|silence|0.000000|n/a|-1.000000000|-1.000000000|

Ten temporary corruptions are rejected: stored score, period, selection index,
selected F0, backend availability, REAPER mapping metadata, output byte length,
time grid, candidate NaN and extra selection bytes. Original raw files are never
modified. This supplements, rather than replaces, earlier measurement guards.
Current compile/self-test, independent-analysis JSON and corruption logs are in
ignored `build/qualification/checkpoint-20261112`; original full inputs, output,
all four candidate contours, scores and selected indices remain under
`build/qualification/world-retained-periodicity`.

Reproduce the checkpoint analysis after the explicit build above:

```sh
build/qualification/world-build/world_probe --fft-self-test
python3 -B tools/qualification/world_analyse.py build/qualification/world-retained-periodicity --suite retained-periodicity
python3 -B tools/qualification/world_analyse.py build/qualification/world-narrow-periodicity --suite narrow-periodicity
python3 -B tools/qualification/world_analyse.py build/qualification/world-periodicity-controls --suite periodicity-controls
```

For a fresh42 render, create an empty ignored output directory, then run
`world_probe --suite retained-periodicity <directory>` before its matching
analyser command. Successful analyser exit indicates valid measurements; inspect
`retainedPass`, `inputEstimatePass` and their conjunction `retainedSubsetPass`.
Do not count sine rows' irrelevant envelope diagnostic as their retained gate.

Fresh checkpoint narrow3 rendering passes all retained and input-estimation
gates. All45 raw audio, contour and metadata files are byte-identical to the
saved narrow3 dataset; the probe log's wall time is not compared. This confirms
the current strict binary reproduces those saved samples without source tuning.

|Fresh narrow case|Input median cents|Intended-period correlation|Grid envelope peak Hz|Retained / input gates|
|---|---:|---:|---:|---|
|periodicity-0|-0.000228536|0.997345756|600.000000|pass / pass|
|periodicity-1|-0.000267038|0.991349845|595.000000|pass / pass|
|periodicity-2|-0.002529084|0.971887969|1800.000000|pass / pass|

Pending: independent agent review of this final comparator and current snapshot;
the optional180-render combined matrix; broader transient, noisy/aperiodic and
changing-F0 material; source/license integration; a genuine bounded streaming
implementation with measured latency/CPU/reset/automation and mono/stereo tests;
product controls and quality modes. None follows from these stationary fixtures.
Manual listening, microphone and DAW testing remains deferred as requested.

## Fingerprints

All SHA256, measured after the reported experiment:

|File|SHA256|
|---|---|
|tools/qualification/world_fft_shim.cpp|`8bebb1c5d3d7f586b60458652cda999889570aacc37e94e86d084228d046b4b9`|
|tools/qualification/world_probe.cpp|`c606471c27440515612949a8a58de36809026b5fa0a112ae74af02dbdcaf2810`|
|tools/qualification/world_analyse.py|`c52fb6924210078062bf2500697b6d901b630732b09bfa140a75bb3ee3401df6`|
|WORLD/LICENSE.txt|`6921ef0a65a931509b4055075fd847d7257c4094609075dd0615e8e997215f7e`|
|WORLD/src/world/fft.h|`ac321b81dea5e8118933ecc66d2d9cbfb7721da52892a64d8a4f2e18a65829d6`|
|WORLD/src/world/macrodefinitions.h|`6d5417f864a4a4d95b470e72e3cb0114e8c96ba33cc189341e83a9f345e664f0`|
|WORLD/src/cheaptrick.cpp|`6481ea68ebc3a90151852cea8a2efbf1bf6683e823b737847ce57a69f7cbe00b`|
|WORLD/src/common.cpp|`ef4a982d103bfd4c0efe6f1e6fa1192809e07ac39289f29951f3d95f1af8ca51`|
|WORLD/src/d4c.cpp|`a988f6154b5c3332a17f2153a1e32afb04e798d57fa99ed08670c6a64e0ad28a`|
|WORLD/src/dio.cpp|`38449de8a16c6ac6405b7289ed2e8adac9400885d53d181639ebd063366756da`|
|WORLD/src/harvest.cpp|`e9e80047a9d9ed3ff8521b966985c0b912ac09415565da1cc821319dc454975e`|
|WORLD/src/matlabfunctions.cpp|`250d4cdc31b64cb3bda3a0c988d3853a09e66dd85f15f167161948d938662a4c`|
|WORLD/src/stonemask.cpp|`de68ab826ff94176bfa89b5caf7ba46a6a0497eaaf1957e2e942245b7b663e76`|
|WORLD/src/synthesis.cpp|`9888973e28f2194a0eb9807e18e36680f3d912bef3ddf174dc640fedc4e08b4c`|


REAPER pin source SHA256 (unchanged):

|File|SHA256|
|---|---|
|REAPER/LICENSE|`cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30`|
|REAPER/epoch_tracker/epoch_tracker.cc|`12479e2bdba27096ecdf8f02c4c1aea7f134098da8cdb541b0d71f7a88a1eb6d`|
|REAPER/epoch_tracker/epoch_tracker.h|`074f3ee3bde6fd346e28b9b4f8dbb5281870d3424d58c99baef73b6e315af80f`|
|REAPER/epoch_tracker/fft.cc|`26cb1c7a37fef7883aa08b40653d6ca352aae48882d00333e31ba3c246c6c09d`|
|REAPER/epoch_tracker/fft.h|`0a6d8a9e3b45f1c9ce3356fff157a5cd543ccd39f0f90099afc19654f754174b`|
|REAPER/epoch_tracker/fd_filter.cc|`55d7936d46ba1df2b7217b9541e03ae0118c0433bd9e09972fea5ecaa7d62e6d`|
|REAPER/epoch_tracker/fd_filter.h|`4b6e6bdb34305e2ba726a9735b2850bdff62de8b223ca13824cc56c55086f167`|
|REAPER/epoch_tracker/lpc_analyzer.cc|`fed730bd53dc8142919162d6388b6fae3af9c06db2ad1c8224d469d4528cdca3`|
|REAPER/epoch_tracker/lpc_analyzer.h|`a2009fad56bf64dd8cff42c75e0724b9df5c5fdf2ab92486c48b91acb2f5b945`|

Project detector files (unchanged):

- `include/darkvoice/PitchDetector.hpp` SHA256 `70d3545c540cdea9d722b890476e8316053ede46ac86e6da8decd7e1c1569526`.
- `src/PitchDetector.cpp` SHA256 `579ca53d68619fff39a9f94a03c496297a31163135842e750875483e7b8ab985`.

## Retained WORLD BSD notice

```text
/* ----------------------------------------------------------------- */
/*           WORLD: High-quality speech analysis,                    */
/*           manipulation and synthesis system                       */
/*           developed by M. Morise                                  */
/*           http://www.kisc.meiji.ac.jp/~mmorise/world/english/     */
/* ----------------------------------------------------------------- */
/*                                                                   */
/*  Copyright (c) 2010  M. Morise                                    */
/*                                                                   */
/* All rights reserved.                                              */
/*                                                                   */
/* Redistribution and use in source and binary forms, with or        */
/* without modification, are permitted provided that the following   */
/* conditions are met:                                               */
/*                                                                   */
/* - Redistributions of source code must retain the above copyright  */
/*   notice, this list of conditions and the following disclaimer.   */
/* - Redistributions in binary form must reproduce the above         */
/*   copyright notice, this list of conditions and the following     */
/*   disclaimer in the documentation and/or other materials provided */
/*   with the distribution.                                          */
/* - Neither the name of the M. Morise nor the names of its          */
/*   contributors may be used to endorse or promote products derived */
/*   from this software without specific prior written permission.   */
/*                                                                   */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND            */
/* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,       */
/* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF          */
/* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE          */
/* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS */
/* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,          */
/* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED   */
/* TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,     */
/* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON */
/* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,   */
/* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY    */
/* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE           */
/* POSSIBILITY OF SUCH DAMAGE.                                       */
/* ----------------------------------------------------------------- */
```

## Retained macrodefinitions MIT notice

```text
//-----------------------------------------------------------------------------
// Copyright (c) 2008-2011 The Department of Arts and Culture,
// The Government of the Republic of South Africa.
//
// Contributors:  Meraka Institute, CSIR, South Africa.
//                Giulio Paci.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
```

## Retained REAPER copyright and licence

Copyright2015 Google Inc. All rights reserved. Author: David Talkin.
No upstream file was modified. Apache2 notice applies to the explicitly listed
tracker files. Any future binary/source distribution must preserve this licence
and its applicable notices.

```text

                                 Apache License
                           Version 2.0, January 2004
                        http://www.apache.org/licenses/

   TERMS AND CONDITIONS FOR USE, REPRODUCTION, AND DISTRIBUTION

   1. Definitions.

      "License" shall mean the terms and conditions for use, reproduction,
      and distribution as defined by Sections 1 through 9 of this document.

      "Licensor" shall mean the copyright owner or entity authorized by
      the copyright owner that is granting the License.

      "Legal Entity" shall mean the union of the acting entity and all
      other entities that control, are controlled by, or are under common
      control with that entity. For the purposes of this definition,
      "control" means (i) the power, direct or indirect, to cause the
      direction or management of such entity, whether by contract or
      otherwise, or (ii) ownership of fifty percent (50%) or more of the
      outstanding shares, or (iii) beneficial ownership of such entity.

      "You" (or "Your") shall mean an individual or Legal Entity
      exercising permissions granted by this License.

      "Source" form shall mean the preferred form for making modifications,
      including but not limited to software source code, documentation
      source, and configuration files.

      "Object" form shall mean any form resulting from mechanical
      transformation or translation of a Source form, including but
      not limited to compiled object code, generated documentation,
      and conversions to other media types.

      "Work" shall mean the work of authorship, whether in Source or
      Object form, made available under the License, as indicated by a
      copyright notice that is included in or attached to the work
      (an example is provided in the Appendix below).

      "Derivative Works" shall mean any work, whether in Source or Object
      form, that is based on (or derived from) the Work and for which the
      editorial revisions, annotations, elaborations, or other modifications
      represent, as a whole, an original work of authorship. For the purposes
      of this License, Derivative Works shall not include works that remain
      separable from, or merely link (or bind by name) to the interfaces of,
      the Work and Derivative Works thereof.

      "Contribution" shall mean any work of authorship, including
      the original version of the Work and any modifications or additions
      to that Work or Derivative Works thereof, that is intentionally
      submitted to Licensor for inclusion in the Work by the copyright owner
      or by an individual or Legal Entity authorized to submit on behalf of
      the copyright owner. For the purposes of this definition, "submitted"
      means any form of electronic, verbal, or written communication sent
      to the Licensor or its representatives, including but not limited to
      communication on electronic mailing lists, source code control systems,
      and issue tracking systems that are managed by, or on behalf of, the
      Licensor for the purpose of discussing and improving the Work, but
      excluding communication that is conspicuously marked or otherwise
      designated in writing by the copyright owner as "Not a Contribution."

      "Contributor" shall mean Licensor and any individual or Legal Entity
      on behalf of whom a Contribution has been received by Licensor and
      subsequently incorporated within the Work.

   2. Grant of Copyright License. Subject to the terms and conditions of
      this License, each Contributor hereby grants to You a perpetual,
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable
      copyright license to reproduce, prepare Derivative Works of,
      publicly display, publicly perform, sublicense, and distribute the
      Work and such Derivative Works in Source or Object form.

   3. Grant of Patent License. Subject to the terms and conditions of
      this License, each Contributor hereby grants to You a perpetual,
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable
      (except as stated in this section) patent license to make, have made,
      use, offer to sell, sell, import, and otherwise transfer the Work,
      where such license applies only to those patent claims licensable
      by such Contributor that are necessarily infringed by their
      Contribution(s) alone or by combination of their Contribution(s)
      with the Work to which such Contribution(s) was submitted. If You
      institute patent litigation against any entity (including a
      cross-claim or counterclaim in a lawsuit) alleging that the Work
      or a Contribution incorporated within the Work constitutes direct
      or contributory patent infringement, then any patent licenses
      granted to You under this License for that Work shall terminate
      as of the date such litigation is filed.

   4. Redistribution. You may reproduce and distribute copies of the
      Work or Derivative Works thereof in any medium, with or without
      modifications, and in Source or Object form, provided that You
      meet the following conditions:

      (a) You must give any other recipients of the Work or
          Derivative Works a copy of this License; and

      (b) You must cause any modified files to carry prominent notices
          stating that You changed the files; and

      (c) You must retain, in the Source form of any Derivative Works
          that You distribute, all copyright, patent, trademark, and
          attribution notices from the Source form of the Work,
          excluding those notices that do not pertain to any part of
          the Derivative Works; and

      (d) If the Work includes a "NOTICE" text file as part of its
          distribution, then any Derivative Works that You distribute must
          include a readable copy of the attribution notices contained
          within such NOTICE file, excluding those notices that do not
          pertain to any part of the Derivative Works, in at least one
          of the following places: within a NOTICE text file distributed
          as part of the Derivative Works; within the Source form or
          documentation, if provided along with the Derivative Works; or,
          within a display generated by the Derivative Works, if and
          wherever such third-party notices normally appear. The contents
          of the NOTICE file are for informational purposes only and
          do not modify the License. You may add Your own attribution
          notices within Derivative Works that You distribute, alongside
          or as an addendum to the NOTICE text from the Work, provided
          that such additional attribution notices cannot be construed
          as modifying the License.

      You may add Your own copyright statement to Your modifications and
      may provide additional or different license terms and conditions
      for use, reproduction, or distribution of Your modifications, or
      for any such Derivative Works as a whole, provided Your use,
      reproduction, and distribution of the Work otherwise complies with
      the conditions stated in this License.

   5. Submission of Contributions. Unless You explicitly state otherwise,
      any Contribution intentionally submitted for inclusion in the Work
      by You to the Licensor shall be under the terms and conditions of
      this License, without any additional terms or conditions.
      Notwithstanding the above, nothing herein shall supersede or modify
      the terms of any separate license agreement you may have executed
      with Licensor regarding such Contributions.

   6. Trademarks. This License does not grant permission to use the trade
      names, trademarks, service marks, or product names of the Licensor,
      except as required for reasonable and customary use in describing the
      origin of the Work and reproducing the content of the NOTICE file.

   7. Disclaimer of Warranty. Unless required by applicable law or
      agreed to in writing, Licensor provides the Work (and each
      Contributor provides its Contributions) on an "AS IS" BASIS,
      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
      implied, including, without limitation, any warranties or conditions
      of TITLE, NON-INFRINGEMENT, MERCHANTABILITY, or FITNESS FOR A
      PARTICULAR PURPOSE. You are solely responsible for determining the
      appropriateness of using or redistributing the Work and assume any
      risks associated with Your exercise of permissions under this License.

   8. Limitation of Liability. In no event and under no legal theory,
      whether in tort (including negligence), contract, or otherwise,
      unless required by applicable law (such as deliberate and grossly
      negligent acts) or agreed to in writing, shall any Contributor be
      liable to You for damages, including any direct, indirect, special,
      incidental, or consequential damages of any character arising as a
      result of this License or out of the use or inability to use the
      Work (including but not limited to damages for loss of goodwill,
      work stoppage, computer failure or malfunction, or any and all
      other commercial damages or losses), even if such Contributor
      has been advised of the possibility of such damages.

   9. Accepting Warranty or Additional Liability. While redistributing
      the Work or Derivative Works thereof, You may choose to offer,
      and charge a fee for, acceptance of support, warranty, indemnity,
      or other liability obligations and/or rights consistent with this
      License. However, in accepting such obligations, You may act only
      on Your own behalf and on Your sole responsibility, not on behalf
      of any other Contributor, and only if You agree to indemnify,
      defend, and hold each Contributor harmless for any liability
      incurred by, or claims asserted against, such Contributor by reason
      of your accepting any such warranty or additional liability.

   END OF TERMS AND CONDITIONS

   APPENDIX: How to apply the Apache License to your work.

      To apply the Apache License to your work, attach the following
      boilerplate notice, with the fields enclosed by brackets "[]"
      replaced with your own identifying information. (Don't include
      the brackets!)  The text should be enclosed in the appropriate
      comment syntax for the file format. We also recommend that a
      file or class name and description of purpose be included on the
      same "printed page" as the copyright notice for easier
      identification within third-party archives.

   Copyright [yyyy] [name of copyright owner]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
```
