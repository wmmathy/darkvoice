# Independent Colour alias-energy audit — 2026-10-28

Developer measurement only; no product/source/kernel/test/parameter changes.
The retained8x-vs32x comparison uses independent convolution but the same final
65-tap halfband, so common final-stage leakage can cancel. Its passed residual
and improvement gates establish agreement with that reference, not an absolute
alias-energy bound. Preserve those historical results and all original gates.
Analytic response at native3*719/4096 predicts only about14.8dB final-stage
attenuation; actual public-output measurements are needed before conclusions.

Predeclare32 pure-tone fixtures: four supported rates, Tape/Tube, amplitudes.15/.5,
coherent bins719/1597 in N=4096, drive1/tone16000/mix1. Reset to accepted targets;
warm ceil(rate/N)*N frames (at least1sec), then measure exactly4096 consecutive
frames. Generate input using frame modulo4096 so it repeats bitwise. Process
512-frame mono calls through public Colour API. Record actual float-input RMS,
output length/finite positive energy, settings and current source revision.

Optional NumPy analysis uses rectangular FFT (the source is coherent) and Parseval
normalization. Allowed non-DC bins are only true integer harmonics h*bin<=N/2;
DC is ignored and Nyquist is treated separately. Sum all other output-bin power
as folded alias energy, normalized to actual input RMS. Report the folded third
harmonic and strongest aliased bin as well. Float-input rounding noise remains
included; do not mistake this audit for a universal multi-tone classifier.
Two-tone intermodulation has legitimate in-band combinations and is explicitly
outside this simple mask. No shared oversampling reference enters this metric.

All32 results must be reported, including failures against any illustrative-55dB
comparison line. That line is not retroactively an original qualification gate.
No claim of sonic approval, arbitrary-signal alias bound or backend qualification.
No threshold/kernel tuning or extra fixture search in this task. Preserve raw
files only temporarily; persist commands/results/source pins/interpretation.
Manual host/microphone/listening testing remains deferred.

Audit ownership is limited to new tools/qualification/colour_alias_probe.cpp,
optional new colour_alias_analyse.py and this report, in isolated worktree with
matching feature/qualification branch. Strict C++20 O2/Werror public-core build;
small finite measurements only, no production or CMake edits, commits or pushes.

## Executed public-output measurements

Executed2026-10-28 with GCC14.2.0/C++20/O2 and NumPy2.3.5 on little-endian Linux.
The32 fixtures were generated and rendered directly through public Colour.
Every retained output contained4096 finite samples with positive total energy;
all actual input RMS values and output RMS values are retained in the table below.
The source revision under audit was `2a5a49d26e6cadbe01dfa342ee93e6007d491314`. Source fingerprints:

- `src/Colour.cpp`: `1a26131ff374d27a7b92543c8b803498c028d2b9f2828627ce245924a7192a97`
- `src/ColourOversampling.hpp`: `9f2ad23de05f067fb05103bb0d5a9a288659a7c771986b6b00359b87ec2a69b1`
- `include/darkvoice/Colour.hpp`: `56b4dc77128a3fcb89423cbd18c6eb28d5d6eca99ac3549a7953814313a64eeb`

All16 bin719 cases have off-mask energy above the illustrative−55dB/input-RMS
line; all16 bin1597 cases lie below it. Worst is−22.947266588dB at44.1kHz,
Tape, amplitude.5, bin719. Its strongest off-mask line is the folded third-harmonic
bin1939, at20,876.440430Hz. The bin1939 peak accounts for essentially the measured
off-mask power in this fixture. At the other rates that same bin lies at
22,722.656250/41,752.880859/45,445.312500Hz. These are frequency locations, not a
claim of audibility or inaudibility; listening remains deferred.

Bin1597 behaves differently: Tape/.5 has total off-mask energy about−63.67 to
−65.26dB, with its largest off-mask line at769 rather than the folded-third bin695.
The declared mask reports all remaining power, not just the particular harmonic
predicted from the final-stage response. No extra test tones or threshold tuning
were introduced after these results.

These measurements confirm that prior8x-versus32x residual passes cannot be
interpreted as an absolute alias-energy guarantee. They remain valid agreement
results against their specified reference. This audit does not change those
historical results, the product kernel or any retained test. It provides separate
public-output evidence that future Character/Colour quality decisions must consider.

## Metric and limits

For each4096-frame output, let `X=rfft(output)/4096`. Single-sided powers are
`|X[k]|²`, doubled only for bins1..2047. Their sum matches direct mean-square
power. Ignore DC. Allowed bins are the true positive-frequency integer harmonics
`h*inputBin<=2048`. All other non-DC bins contribute to the off-mask sum; Nyquist
is counted once and is also reported separately. Normalize every power to the
measured float-input RMS squared. No fitted-sine subtraction or shared filtered
reference enters the calculation.

`folded_h3_db_input` is the power at the bin where the third harmonic folds; it
is not a decomposition excluding higher orders that could hit the same bin.
Off-mask energy includes float-input/output rounding and any residual numerical
or periodic-state errors. Higher folded orders can coincide with an allowed bin,
so this metric does not identify every possible aliased component. The method
applies to these coherent single tones; it is not an arbitrary-signal or two-tone
intermodulation classifier. An observed `-inf` denotes an exactlyzero measured FFT
bin in this capture, not a universal spectral-zero guarantee.

## All32 results

The following CSV preserves every result, including actual input/output RMS,
folded-third-bin power, strongest off-mask bin/frequency and Nyquist power.
The model, amplitude, input bin and rate are predeclared above; drive1,
HFtone16000 and mix1 apply throughout. Warmup is45056/49152/90112/98304frames
at44.1/48/88.2/96kHz respectively, followed by exactly4096 measured frames.

```csv
case,rate,model,amplitude,bin,allowed_bins,input_rms,output_rms,folded_db_input,folded_h3_bin,folded_h3_db_input,peak_alias_bin,peak_alias_hz,peak_alias_db_input,nyquist_db_input
0,44100,Tape,0.15,719,719;1438,0.1060660171256255,0.435121487594992,-23.93564538555107,1939,-23.935645391063638,1939,20876.4404296875,-23.935645391063638,-inf
1,44100,Tape,0.15,1597,1597,0.1060660171256255,0.3843770490723665,-97.85779174245143,695,-98.12016718556683,695,7482.7880859375,-98.12016718556683,-inf
2,44100,Tape,0.5,719,719;1438,0.3535533897977427,0.5745795987567248,-22.947266588434122,1939,-22.947266642850735,1939,20876.4404296875,-22.947266642850735,-inf
3,44100,Tape,0.5,1597,1597,0.35355338979774265,0.5054486266535267,-63.6734809906977,695,-100.47546803793935,769,8279.5166015625,-63.67505009606233,-inf
4,44100,Tube,0.15,719,719;1438,0.1060660171256255,0.3753916391536002,-50.29235600625681,1939,-50.292357672730276,1939,20876.4404296875,-50.292357672730276,-176.96063816154336
5,44100,Tube,0.15,1597,1597,0.1060660171256255,0.32655611850137645,-111.6494777755517,695,-118.85332886983201,902,9711.474609375,-114.00596578009112,-181.53318643001313
6,44100,Tube,0.5,719,719;1438,0.3535533897977427,0.7847538129149664,-24.156670315891763,1939,-24.15667039354147,1939,20876.4404296875,-24.15667039354147,-178.84895314415357
7,44100,Tube,0.5,1597,1597,0.35355338979774265,0.6834568518187898,-82.58158466391741,695,-99.52879559130056,828,8914.74609375,-82.77181237168921,-178.75013476698345
8,48000,Tape,0.15,719,719;1438,0.1060660171256255,0.42744540747233956,-24.257892473355085,1939,-24.257892479279167,1939,22722.65625,-24.257892479279167,-inf
9,48000,Tape,0.15,1597,1597,0.1060660171256255,0.36911842332326317,-97.97494184430816,695,-98.24206319226602,695,8144.53125,-98.24206319226602,-inf
10,48000,Tape,0.5,719,719;1438,0.3535533897977427,0.563358177360021,-23.32427340378296,1939,-23.32427346148737,1939,22722.65625,-23.32427346148737,-inf
11,48000,Tape,0.5,1597,1597,0.35355338979774265,0.48471324606087596,-63.69533954327922,695,-100.63816301029195,769,9011.71875,-63.6968664717718,-inf
12,48000,Tube,0.15,719,719;1438,0.1060660171256255,0.3692304554345603,-50.65759310845007,1939,-50.6575948160595,1939,22722.65625,-50.6575948160595,-178.20871581945337
13,48000,Tube,0.15,1597,1597,0.1060660171256255,0.3140595511246202,-111.86180343610091,695,-118.96739050914493,902,10570.3125,-114.20869490046854,-182.66609235686144
14,48000,Tube,0.5,719,719;1438,0.3535533897977427,0.7719058993410051,-24.521911411743243,1939,-24.521911492332855,1939,22722.65625,-24.521911492332855,-178.14511779089406
15,48000,Tube,0.5,1597,1597,0.35355338979774265,0.6573024966617473,-82.75025409133099,695,-99.65862417572998,828,9703.125,-82.94105434071973,-190.5567459145001
16,88200,Tape,0.15,719,719;1438,0.1060660171256255,0.34594102302506935,-27.662619099295714,1939,-27.662619110239994,1939,41752.880859375,-27.662619110239994,-inf
17,88200,Tape,0.15,1597,1597,0.1060660171256255,0.24685538648073516,-99.5247251116952,695,-99.87960626328159,695,14965.576171875,-99.87960626328159,-inf
18,88200,Tape,0.5,719,719;1438,0.3535533897977427,0.45174396206301315,-26.991460192841593,1939,-26.991460290048547,1939,41752.880859375,-26.991460290048547,-inf
19,88200,Tape,0.5,1597,1597,0.35355338979774265,0.3219012390734907,-64.93246877600662,695,-102.46824825432357,769,16559.033203125,-64.93397009565155,-inf
20,88200,Tube,0.15,719,719;1438,0.1060660171256255,0.3001657336158817,-54.25856564677184,1939,-54.25856766830287,1939,41752.880859375,-54.25856766830287,-181.81683828146754
21,88200,Tube,0.15,1597,1597,0.1060660171256255,0.2116521717254227,-114.12638256341161,695,-120.63951408865778,902,19422.94921875,-116.47018239829853,-171.92758759707786
22,88200,Tube,0.5,719,719;1438,0.3535533897977427,0.6277129107015023,-28.12288064128614,1939,-28.122880755545836,1939,41752.880859375,-28.122880755545836,-187.44074495457943
23,88200,Tube,0.5,1597,1597,0.35355338979774265,0.442971724467123,-84.81138809363878,695,-101.33844424552424,828,17829.4921875,-85.01298145002609,-182.02472655232495
24,96000,Tape,0.15,719,719;1438,0.1060660171256255,0.3313122832089714,-28.252830480764942,1939,-28.25283049276164,1939,45445.3125,-28.25283049276164,-inf
25,96000,Tape,0.15,1597,1597,0.1060660171256255,0.23050075098696782,-99.85268414185121,695,-100.23092340436698,695,16289.0625,-100.23092340436698,-inf
26,96000,Tape,0.5,719,719;1438,0.3535533897977427,0.4322830553669221,-27.6052175107507,1939,-27.605217615713737,1939,45445.3125,-27.605217615713737,-inf
27,96000,Tape,0.5,1597,1597,0.35355338979774265,0.3003742958387843,-65.25989748757547,695,-102.83696500104921,769,18023.4375,-65.26143154012505,-inf
28,96000,Tube,0.15,719,719;1438,0.1060660171256255,0.28756048764722253,-54.865383378866774,1939,-54.865385444158704,1939,45445.3125,-54.865385444158704,-176.15443806695026
29,96000,Tube,0.15,1597,1597,0.1060660171256255,0.1977770590741381,-114.55194852839718,695,-120.99470449669008,902,21140.625,-116.91845651042465,-186.38510080004974
30,96000,Tube,0.5,719,719;1438,0.3535533897977427,0.6013737557571561,-28.72970711018679,1939,-28.729707231221727,1939,45445.3125,-28.729707231221727,-190.24659035830123
31,96000,Tube,0.5,1597,1597,0.35355338979774265,0.4139321800330185,-85.22622653287803,695,-101.69280253830647,828,19406.25,-85.43054015035871,-180.1576934853897
```

## Reproduce

From a checkout containing these developer tools, use a separate worktree at the
exact audited source revision. No JUCE, product dependencies or CMake changes are
needed. The temporary raw float32 files and metadata are not intended for commits.

```sh
auditToolsRoot="$PWD"
git worktree add --detach /tmp/darkvoice-colour-audit-source 2a5a49d26e6cadbe01dfa342ee93e6007d491314
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror \
  -I/tmp/darkvoice-colour-audit-source/include \
  '-DDARKVOICE_AUDIT_REVISION="2a5a49d26e6cadbe01dfa342ee93e6007d491314"' \
  /tmp/darkvoice-colour-audit-source/src/Colour.cpp \
  "$auditToolsRoot/tools/qualification/colour_alias_probe.cpp" \
  -o /tmp/colour-alias-probe
/tmp/colour-alias-probe /tmp/darkvoice-colour-alias
python3 -B tools/qualification/colour_alias_analyse.py /tmp/darkvoice-colour-alias \
  > /tmp/colour-alias-results.csv
```

Probe exit0 establishes successful rendering/file output/finite positive-energy
measurements, not a quality pass. The analyser validates the exact32-case manifest,
4096-float lengths, finite positive energy, actual RMS metadata, common40-character
source revision and Parseval normalization. Illustrative threshold exceedances do
not alter its exit status. Actual settings/results must be reviewed explicitly.

Strict warnings-as-errors compilation, clang-format19 and Python syntax checks
passed. Ten deliberate temporary corruptions were rejected: truncated length,
zero energy, NaN, Inf, wrong warmup, wrong bin, invalid/mixed revision, wrong RMS
and missing case. All measurement files were restored byte-for-byte afterwards.
A synthetic calibration containing allowed fundamentals/harmonics, DC, a known
folded-bin sinusoid and Nyquist cosine matched expected combined off-mask power
within1.4e-8dB and Nyquist power within1.1e-9dB. This confirms DC exclusion and
Nyquist's single-sided weight. Unknown probe options return usage/status2.

No product source/helper/kernel, original test, parameter, CMake configuration,
threshold, dependency or manual-qualification state changed. No corrective
strategy or Character implementation is part of this audit.
