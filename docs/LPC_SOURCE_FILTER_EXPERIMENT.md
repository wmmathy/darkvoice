# Stationary source/filter experiment — 2026-10-26

Finite developer-only candidate; no product/API/parameter/latency changes or main
merge. Pitch/formant qualification remains blocked. Base fa4af2971779737acdd72e7ca9f37070a926aabc;
retain existing MIT backend pins/notices and guarded private generation. Existing
experimental source/tests/upstream checkouts stay unchanged. Prior phase-separation
invariant passed but all compensated periodicity cases still failed; do not repeat
post-shift spectral gain as a proposed repair.

First experiment is fixed-model time-domain inverse filtering -> existing
uncompensated pitch shifter -> fixed stable warped resynthesis. Fit each complete
2s fixture once, explicitly an offline stationary oracle, not a realtime estimator.
Use own double autocorrelation LPC order24 at48k, normalized by maximum absolute
input without F0/formant-base oracle. Exact silence gives identity. Use biased
finite-window autocorrelation over the full96k samples, no gain floor, reflection
clamp, coefficient-order search or hidden bandwidth expansion. Levinson recursion
requires finite positive prediction energy and reflection magnitude<1 at every
stage; report rejection as candidate failure, not a concealed fallback. Record
coefficients, reflection extrema, conditioning/error and all rejected stages.

Inverse residual e[n]=x[n]+sum(a[k]*x[n-k]) with zero states. To avoid existing
wrapper +/-8 clipping, predeclare one whole-fixture residual scale mapping its
maximum absolute sample to.25; record scale, apply before shifting and undo before
resynthesis. This is offline fixture normalization, not a deployable live gain
controller. Record actual shifter-input peaks; no output clipping or gain matching
to hide failed metrics. For zero residual handle exact silence explicitly.

For formant scale s=2^(formantSemitones/12), alpha=(s-1)/(s+1), use
D(z)=(z^-1+alpha)/(1+alpha*z^-1), H(z)=1/A(D(z)). This frozen warp is stable for
stable A but is not a linear frequency scaling: target f=Fs/pi*atan(s*tan(pi*f/Fs)).
Derive and test its scalar feedthrough solution; a naive delay substitution is
incorrect. A stationary allpass-cascade feedback realization is acceptable ONLY
for this probe; record denominator/maximum state and numerical failures. A future
streaming implementation would require a stable lattice/ladder realization and
coefficient timestamp/transition/latency contract, which this experiment excludes.

Before vowel rendering verify known stable AR filters, inverse/resynthesis unity,
complex warp response and level scaling. Known-filter reconstruction relativeRMS
<=2e-7 and peak<=2e-6; source-through-neutral-backend retains original1e-5 identity
gate. No threshold relaxation if conditioning fails. No iterative root search.

Then keep exactly the original three48k/2s/80-harmonic float-accumulated fixtures:
F0=100Hz/envelope600Hz pitch+12/formant0; F0=70Hz/envelope600Hz pitch-12/formant0;
F0=180Hz/envelope1200Hz pitch+12/formant+7. Envelope center is distinct from F0.
Use existing Offline8192/1024 settings, seed/reset,512sample public calls and final
48000sample measurement without hidden flush/crop. Existing shift backend runs
preservationfalse/formant0 on residual; resynthesis applies the desired source
formant independently. Retain >.8 periodicity and20% envelope proxy gates. Include
original uncompensated controls and ideal35/unchanged70/even-only controls, reporting
half/full/double-period correlation, grid/total and odd/total. Do not infer octave
correctness from correlation alone or invent a universal odd-energy threshold.
Only if allthree improve/pass should levels.25/1/4 and future tracking/149-suite
work be proposed; otherwise finish an honest negative report. All lengths/finite
samples/nonzero energy and actual failures are recorded. Manual tests deferred.

Planned task lpc_source_filter owns only new tools/qualification/lpc_source_filter_probe.cpp,
optional new analyser and this report, in isolated lpc-source-filter worktree.
Use strict C++20 O2/Werror standalone compile against the pinned existing wrapper;
no CMake/product/test/dependency changes. Root owns further qualification decisions.

## Executed finite experiment — 2026-10-28

**Negative result: only 2/3 original cases pass both unchanged numerical gates.**
The 70Hz/-12 case fails the20% envelope proxy. No149-case expansion, tracking,
product integration or listening claim follows. All numerical invariants pass;
no LPC stage rejects. Manual/native/listening tests remain deferred. No commit,
push, dependency, CMake, product, source-wrapper, upstream or existing-test edit
was made. Added code is project-authored AGPL-3.0-only; existing pinned backend
and linear headers remain MIT with existing notices.

The finite task used one agent in its assigned isolated worktree at
`7bb5e3c69c59fab1563ea7498e62c74ea750c9f3` (contract on base
`fa4af2971779737acdd72e7ca9f37070a926aabc`). No persistent execution is configured.
Local compiler: GCC14.2.0; analyser: Python3/NumPy2.3.5. No network was used.
Stretch pin `a670068d9aeb64913331d5cc29337b19a457a7df`; Linear pin
`de55e6a50ffcf6f8f43f649692d94691c7025151`. The unchanged CMake guard generated
and verified private header SHA256
`7003842b8347ba10710f280c33dba494cd3466a54b240cef76cfbbe32a9a93b8`.

### Realization and interpretation

For each allpass stage, given previous-stage value `v[k-1]`, its output is
`v[k] = alpha*v[k-1] + state[k]`, followed by
`state[k] = v[k-1] - alpha*v[k]`. At the beginning of a sample, recursively set
`q[0]=0`, `q[k]=alpha*q[k-1]+state[k]`. Therefore
`v[k]=alpha^k*y+q[k]`, so the scalar solution is exactly
`y=(e-sum(a[k]*q[k]))/(1+sum(a[k]*alpha^k))`. Only after solving this equation
are all states advanced. A naive delay substitution would miss this feedthrough.
All states and denominator are checked finite; zero denominator rejects.

Known filters include real pole .7 and conjugate poles of radius sqrt(.7),
with denominator `[1,-1.5,.7]`. Inverse/resynthesis relativeRMS and peak gates
are2e-7/2e-6; known-filter and fitted-filter round trips pass. Twelve complex
impulse-response comparisons cover alpha0,+.2,-.2 and four frequencies;
maximum absolute response error is below7.2e-15 (gate2e-10). Gain4 linearity
is exact in these runs. Exact silence fits identity and reconstructs exactly.
Neutral wrapper identity error is4.65654e-10, below original1e-5. All invariants
execute before vowel rendering. This is a fixed stationary allpass-feedback
prototype, without streaming coefficient transitions or a stability guarantee
for future live estimators.

Each original fixture is exactly96000 float-accumulated samples at48k, with
80 harmonics: each harmonic adds float(.025*(exp(-.5*((h*F0-center)/100)^2)
+.002/h)*sin(2*pi*h*F0*n/48000)). F0/center/pitch/formant are
100/600/+12/0,70/600/-12/0,180/1200/+12/+7. Complete-fixture max normalization
precedes biased autocorrelation and strict order24 Levinson recursion. All24
reflection magnitudes are below1 and all prediction energies positive/finite;
no clamp, order search, floor, root search or bandwidth expansion is used.
`R0/E` below is a prediction-energy conditioning proxy, not a matrix condition
number. Raw coefficient/stage records follow below.

| Case | R0/E | max abs reflection | residual peak | residual scale | synthesis denominator | max state |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
|100/+12|920181.205|.999754957|.004965489|50.3475106|1|.09484435|
|70/-12|662116.722|.999724538|.007058802|35.4167744|1|.56483617|
|180/+12/+7|386686.356|.999753290|.005516738|45.3166370|.628904698|.07168735|

Whole-fixture residual normalization maps actual float backend input peak to.25
in every case; its inverse scale is applied before resynthesis. There is no
output clipping or loudness matching. Offline8192/1024, seed0/reset and512-frame
public calls are unchanged. Exactly96000 samples are processed; final48000
are measured with no flush/crop. Existing uncompensated controls use the original
input and the same wrapper with preservefalse/formant0/base0. Latency9216 is
reported, not reinterpreted as a new candidate latency contract.

| Case | candidate corr | original broad grid peak / target Hz | proxy error | independent Hann peak Hz | independent error | result |
| --- | ---: | --- | ---: | ---: | ---: | --- |
|100/+12|.999342433|600/600|0|599.573924|.000710127|pass|
|70/-12|.977634536|210/600|.65|383.863250|.360227917|FAIL|
|180/+12/+7|.997461745|1800/1797.968492|.001129891|1800.453823|.001382299|pass|

The original20% proxy searches200–3000Hz on the declared intended harmonic grid,
with strict relative error<.2. The +7 allpass center is1793.39281Hz, compared
with linear target1797.96849Hz; both are reported without treating this warp as
linear frequency scaling. For70/-12 the exact-grid maximum disagrees with the
independent spectral maximum, confirming why it must remain a proxy.
Independent Hann FFT peaks also fail that case; the negative decision is robust
to that diagnostic difference. Uncompensated independent peaks are1200.72045,
313.96345 and2520.11015Hz, all outside their formant gates. Candidate case0
correlation is slightly lower than uncompensated; case1 is also lower. The
experiment does not establish improvement on every metric.

Direct rectangular exact-grid projections and independent Hann +/-2Hz integrated
bands are separately labelled in raw output. They answer different questions;
none is a universal octave certificate. Ideal35, unchanged70 and even-only35
controls all exceed.9994 full-period correlation at35Hz. The last two have
essentially zero odd-grid energy and yet pass the envelope proxy. This exposes
the octave ambiguity without inventing an odd-energy threshold. The finite
model therefore fails the acceptance condition; no levels.25/1/4 sweep or future
tracking work is proposed on this result.

### Reproduction and actual commands

From this worktree; local source overrides prevent dependency downloads:

```sh
source tools/cloud-env.sh
cmake -S . -B /tmp/lpc-source-filter-build -DCMAKE_BUILD_TYPE=Release \
  -DDARKVOICE_BUILD_SCULPTOR_CORE=ON \
  -DFETCHCONTENT_SOURCE_DIR_DARKVOICE_STRETCH=/workspace/dependencies/signalsmith-stretch \
  -DFETCHCONTENT_SOURCE_DIR_DARKVOICE_LINEAR=/workspace/dependencies/signalsmith-linear \
  > /tmp/lpc-source-filter-configure.log
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  -I/tmp/lpc-source-filter-build/darkvoice-stretch-private \
  src/PitchFormantProcessor.cpp tools/qualification/lpc_source_filter_probe.cpp \
  -o /tmp/lpc-source-filter-probe
/tmp/lpc-source-filter-probe > /tmp/lpc-source-filter.log 2>&1
# exit1: actual numerical gate failure, expected for this negative experiment
python3 -B tools/qualification/lpc_source_filter_analyse.py \
  > /tmp/lpc-source-filter-analysis.log
/workspace/.quality-tools/clang_format/data/bin/clang-format --dry-run --Werror \
  tools/qualification/lpc_source_filter_probe.cpp
git diff --check
```

Configure, strict compile, independent analyser, formatter19.1.7 and diff check
passed. Probe exited1 with2/3, not a tooling failure. No full CTest or sanitizer
suite was run. During setup, bare `cmake` was absent from PATH; the attempted
compile against its nonexistent output failed. Sourcing the existing cloud env
fixed this; the final run above compiled against fresh guarded headers. An early
exploratory peak search was narrowed around target; it was corrected to the
original broad200–3000Hz proxy before the recorded final run. No threshold was
relaxed. Instance `/tmp` measurements are transient; exact output is retained
below, and the commands regenerate all nine native-double measurement files.

### Final probe output

```text
synthesis alpha0 denominator1 maxState0.042038348561360649 outputPeak0.042038348561360649
reconstruction relativeRMS0 peak0
synthesis alpha0 denominator1 maxState0.066702918953281787 outputPeak0.066702918953281787
reconstruction relativeRMS2.9046185948744813e-16 peak8.3266726846886741e-17
stage1 reflection-0.99686210239498141 E0.0019947317579238357
stage2 reflection0.99663745862399877 E1.3392182337425515e-05
stage3 reflection-0.045154729958051636 E1.3364876352102154e-05
stage4 reflection-0.35198701807669691 E1.1709036855585931e-05
stage5 reflection-0.35776860936783328 E1.0210299131896855e-05
stage6 reflection-0.28278176841010672 E9.3938271652106646e-06
stage7 reflection-0.1812788240331793 E9.0851271037790368e-06
stage8 reflection-0.072604776955634123 E9.0372352774672357e-06
stage9 reflection0.025325403481913981 E9.0314390110979287e-06
stage10 reflection0.099174378278618455 E8.942609775140046e-06
stage11 reflection0.14620712594016194 E8.7514478655593764e-06
stage12 reflection0.17158825200488892 E8.4937831147584615e-06
stage13 reflection0.18253354143360856 E8.2107830551500098e-06
stage14 reflection0.18498933192809597 E7.9298014136459387e-06
stage15 reflection0.18292628162440094 E7.6644542043915727e-06
stage16 reflection0.1787067859241418 E7.4196813109377151e-06
stage17 reflection0.17361992091308454 E7.196023350582627e-06
stage18 reflection0.1683043545005955 E6.9921862332128849e-06
stage19 reflection0.16303201921600238 E6.8063378437252971e-06
stage20 reflection0.15788586463810625 E6.6366698197804039e-06
stage21 reflection0.15286583253097971 E6.4815843666119577e-06
stage22 reflection0.14794782813357793 E6.3397118192841595e-06
stage23 reflection0.14311242119424275 E6.2098671348264823e-06
stage24 reflection0.13835472935514281 E6.0909976647878251e-06
model inputMax0.066702918953281787 R00.31834472620774246 E6.0909976647878251e-06 EoverR01.9133339312217844e-05 conditionProxyR0overE52264.792030392564 reflectionAbsMin0.025325403481913981 reflectionAbsMax0.99686210239498141
coefficients 1 -1.3549270996323859 0.07487937002076625 0.070562481039684946 0.061998600036802194 0.051617208853640031 0.041370134060405142 0.032481748142009118 0.02543964679453518 0.020149289358503544 0.016164640327355966 0.012918015314170162 0.0098959187851638715 0.0067358939368027271 0.0032450305013898111 -0.00064040802914887328 -0.0049239517286047297 -0.0096145387126262694 -0.014761612645341288 -0.020443850728631849 -0.026709723113624156 -0.033484277682557594 -0.040470969436982029 -0.047087613393640704 0.13835472935514281
reconstruction relativeRMS5.5132667749542267e-14 peak9.7769015106052848e-15
synthesis alpha0 denominator1 maxState1.55 outputPeak1.55
warp alpha0 omega0.01 complexAbsError4.4409259796892702e-15
warp alpha0 omega0.1 complexAbsError7.2164496600635175e-16
warp alpha0 omega0.7 complexAbsError4.5775667985222375e-16
warp alpha0 omega2 complexAbsError2.0014830212433605e-16
levelScaling alpha0 gain4 peakError0
synthesis alpha0.20000000000000001 denominator0.72799999999999998 maxState2.0781251840151542 outputPeak2.2098780340538582
warp alpha0.2 omega0.01 complexAbsError8.8828683330073648e-16
warp alpha0.2 omega0.1 complexAbsError5.3432186456850503e-15
warp alpha0.2 omega0.7 complexAbsError7.1607233460988954e-15
warp alpha0.2 omega2 complexAbsError8.3266726846886741e-17
levelScaling alpha0.2 gain4 peakError0
synthesis alpha-0.20000000000000001 denominator1.3280000000000001 maxState1.266768269798733 outputPeak1.0747621054693441
warp alpha-0.2 omega0.01 complexAbsError8.9425626145875542e-16
warp alpha-0.2 omega0.1 complexAbsError6.6400247763745666e-15
warp alpha-0.2 omega0.7 complexAbsError2.2887833992611187e-16
warp alpha-0.2 omega2 complexAbsError1.2412670766236366e-16
levelScaling alpha-0.2 gain4 peakError0
model exact-silence identity
reconstruction relativeRMS0 peak0
backend frames96000 latency9216 actualInputPeak0.01299965288490057 pitch0 preserve0 formant0 base0
neutralBackendIdentityPeak4.6565405351084443e-10 gate1e-5
invariants PASS
CASE0 F0100 center600 pitch12 formant0
stage1 reflection-0.99687484363736467 E0.00094098036919850695
stage2 reflection0.99975495705538209 E4.6110469884280209e-07
stage3 reflection-0.6928371792918393 E2.3976367337162295e-07
stage4 reflection-0.39124524143558781 E2.0306236721250921e-07
stage5 reflection-0.25310298792204983 E1.9005396403236991e-07
stage6 reflection-0.16865186879468419 E1.8464817306785122e-07
stage7 reflection-0.10751716603603713 E1.8251365108282857e-07
stage8 reflection-0.056664335881178862 E1.8192762768093472e-07
stage9 reflection-0.011371047045291068 E1.8190410430934209e-07
stage10 reflection0.027306949069053049 E1.8176846397275297e-07
stage11 reflection0.056321051554462619 E1.8119188334474746e-07
stage12 reflection0.074575340346300048 E1.8018418805791129e-07
stage13 reflection0.084344477907014034 E1.7890235937412035e-07
stage14 reflection0.089711016919362369 E1.7746254127872568e-07
stage15 reflection0.093825377814010485 E1.7590030276525312e-07
stage16 reflection0.09727363965570672 E1.7423590578548793e-07
stage17 reflection0.098858506386072131 E1.7253309753166554e-07
stage18 reflection0.097404833237987487 E1.7089615499683771e-07
stage19 reflection0.093389414290108927 E1.6940567004767305e-07
stage20 reflection0.088555227578957688 E1.6807718598363211e-07
stage21 reflection0.084524876565592419 E1.6687636613244805e-07
stage22 reflection0.081457241905177891 E1.6576909434083661e-07
stage23 reflection0.078192563018770644 E1.6475556934849627e-07
stage24 reflection0.073546245829892595 E1.6386439819069346e-07
model inputMax0.060641176998615265 R00.15078493943558988 E1.6386439819069346e-07 EoverR01.0867424744411604e-06 conditionProxyR0overE920181.20531658956 reflectionAbsMin0.011371047045291068 reflectionAbsMax0.99975495705538209
coefficients 1 -2.1485913515743151 1.1737375204254854 -0.0051597457980285923 -0.0073180141901240722 -0.0044589913834783366 0.00076854569172717351 0.0042862533763743625 0.0036950790251627768 0.00013042515915986347 -0.0031214466929160327 -0.0035269452247842139 -0.0012546862442549913 0.0013354437969692017 0.0019124457004166505 0.0003867629102239635 -0.0017335612580937055 -0.0023680695825658224 -0.0012837038996835644 0.00040305125706408966 0.0010911156211502347 0.00041806969570249293 -0.00069707657478576546 -0.080251212216647244 0.073546245829892595
reconstruction relativeRMS2.0458819971619792e-13 peak2.4467407266914876e-14
residual peak0.0049654887989163399 scale50.347510612562374
backend frames96000 latency9216 actualInputPeak0.25 pitch12 preserve0 formant0 base0
synthesis alpha0 denominator1 maxState0.094844348202310011 outputPeak0.094844348202310011
warpMappedCenterHz600 linearTargetHz600
metrics candidate frames96000 finite1 last48000Energy1.1208358843102338e-05 halfPeriodCorr-0.193849375 fullPeriodCorr0.999342433 doublePeriodCorr0.997572364 gridOverTotal0.320985135 oddOverTotal0.302415546 peakHz600 targetEnvelopeHz600 envelopeRelativeError0 periodicityPass1 envelopePass1
backend frames96000 latency9216 actualInputPeak0.060641176998615265 pitch12 preserve0 formant0 base0
metrics uncompensated frames96000 finite1 last48000Energy0.00053045901059799595 halfPeriodCorr0.169560284 fullPeriodCorr0.999618463 doublePeriodCorr0.998859878 gridOverTotal0.123437459 oddOverTotal0.0512621553 peakHz1200 targetEnvelopeHz600 envelopeRelativeError1 periodicityPass1 envelopePass0
CASE1 F070 center600 pitch-12 formant0
stage1 reflection-0.99687472380630804 E0.00065798489108124939
stage2 reflection0.99972453840566555 E3.6244920699573397e-07
stage3 reflection-0.6162680137959351 E2.2479597648095835e-07
stage4 reflection-0.37097109111063725 E1.9385965525696924e-07
stage5 reflection-0.25171419567310366 E1.8157670046213129e-07
stage6 reflection-0.1746854802621122 E1.7603588435810986e-07
stage7 reflection-0.11681137696485268 E1.7363389270894846e-07
stage8 reflection-0.069956935791831876 E1.7278413314952014e-07
stage9 reflection-0.031024069753345075 E1.7261782964742948e-07
stage10 reflection0.0012325186382564693 E1.7261756742330578e-07
stage11 reflection0.027374815457955393 E1.7248821118063473e-07
stage12 reflection0.047891397522530889 E1.7209259464178123e-07
stage13 reflection0.063224868966987813 E1.7140467444782296e-07
stage14 reflection0.074056747942543161 E1.7046462232289991e-07
stage15 reflection0.081064998823163037 E1.6934440825567802e-07
stage16 reflection0.0850686571990594 E1.6811891756660619e-07
stage17 reflection0.0867881240420183 E1.6685261587451562e-07
stage18 reflection0.086969067199934746 E1.6559060631734814e-07
stage19 reflection0.086081376835122334 E1.6436357935526416e-07
stage20 reflection0.084542149609233785 E1.6318881120731761e-07
stage21 reflection0.082454954973222958 E1.6207931991923996e-07
stage22 reflection0.079886704759256355 E1.6104494824182838e-07
stage23 reflection0.076808127130188877 E1.6009486543888461e-07
stage24 reflection0.07323051923985921 E1.5923632327144433e-07
model inputMax0.086660921573638916 R00.10543303242707912 E1.5923632327144433e-07 EoverR01.5103077243043095e-06 conditionProxyR0overE662116.72224653978 reflectionAbsMin0.0012325186382564693 reflectionAbsMax0.99972453840566555
coefficients 1 -2.1359290629936094 1.151709491467106 -0.00068598151179968076 -0.00091318873222166188 -0.00088877491604367901 -0.00065869997059025314 -0.0003434054063326673 -2.1942178889933389e-05 0.00011595259682558776 0.00014083307747804691 -0.00011428846323800094 -0.00030081039590108588 -0.00056144765926071359 -0.00060791245694852183 -0.00061572269429297963 -0.00039855431741496738 -0.0002918448079764391 -8.7036856508699711e-05 -0.00011555894596507899 -0.00012983767604372274 -0.00022440325780241811 -0.00027681201246622378 -0.080018966842989966 0.07323051923985921
reconstruction relativeRMS2.7053032368669401e-13 peak4.3685541295523933e-14
residual peak0.0070588020607829094 scale35.416774382857803
backend frames96000 latency9216 actualInputPeak0.25 pitch-12 preserve0 formant0 base0
synthesis alpha0 denominator1 maxState0.56483616860030916 outputPeak0.56483616860030916
warpMappedCenterHz600 linearTargetHz600
metrics candidate frames96000 finite1 last48000Energy2.383189220073393e-05 halfPeriodCorr-0.127524264 fullPeriodCorr0.977634536 doublePeriodCorr0.930748286 gridOverTotal0.0166569083 oddOverTotal0.00961583063 peakHz210 targetEnvelopeHz600 envelopeRelativeError0.65 periodicityPass1 envelopePass0
backend frames96000 latency9216 actualInputPeak0.086660921573638916 pitch-12 preserve0 formant0 base0
metrics uncompensated frames96000 finite1 last48000Energy0.00072547042650099737 halfPeriodCorr-0.00162977216 fullPeriodCorr0.979937272 doublePeriodCorr0.937383294 gridOverTotal0.0260914125 oddOverTotal0.0244040451 peakHz385 targetEnvelopeHz600 envelopeRelativeError0.358333333 periodicityPass1 envelopePass0
CASE2 F0180 center1200 pitch12 formant7
stage1 reflection-0.98732461858397935 E0.0062080354055349994
stage2 reflection0.99975328984697709 E3.0627928720823381e-06
stage3 reflection-0.80516024837767175 E1.0772362422819387e-06
stage4 reflection-0.34365213071837425 E9.5001810328528168e-07
stage5 reflection-0.16333794742260296 E9.2467229948775536e-07
stage6 reflection-0.015252108274074671 E9.2445719592338529e-07
stage7 reflection0.12057127054904311 E9.1101796296467384e-07
stage8 reflection0.17101613070982594 E8.8437386065726591e-07
stage9 reflection0.17581001654424561 E8.5703860580279345e-07
stage10 reflection0.19317129558583462 E8.2505808215286142e-07
stage11 reflection0.1971989462649521 E7.9297369835092634e-07
stage12 reflection0.17699416995589723 E7.6813226189569576e-07
stage13 reflection0.16904622889784721 E7.4618163237324943e-07
stage14 reflection0.16934345467365008 E7.2478322825628905e-07
stage15 reflection0.15313825342337006 E7.0778610146119706e-07
stage16 reflection0.13855927580297578 E6.9419754760825971e-07
stage17 reflection0.13714919048885565 E6.8113976084390046e-07
stage18 reflection0.12692794503125671 E6.7016612029888805e-07
stage19 reflection0.11032019973517748 E6.6200983238943257e-07
stage20 reflection0.10517802490073408 E6.5468640361706442e-07
stage21 reflection0.098818892256762034 E6.482932773193533e-07
stage22 reflection0.082739651939313527 E6.438551587848466e-07
stage23 reflection0.073477366813592529 E6.4037903408007194e-07
stage24 reflection0.068983449702075775 E6.3733165191141323e-07
model inputMax0.034501008689403534 R00.24644745405094459 E6.3733165191141323e-07 EoverR02.5860752117149755e-06 conditionProxyR0overE386686.3560154704 reflectionAbsMin0.015252108274074671 reflectionAbsMax0.99975328984697709
coefficients 1 -2.0908107206272173 1.1727956464179818 -0.096144010912854808 0.014102836520313125 0.056443179608145058 -0.022505251121730529 -0.025801426328689044 0.026281695638371298 0.0026987533271640492 -0.024228076094813509 0.0088857880783375781 0.012425249168639888 -0.016668548564776835 -0.0026983119972331529 0.014522480463478855 -0.0072261846759308131 -0.009320996719200246 0.011873070031043405 0.001968965288802557 -0.011805176176175235 0.004488015023924167 0.0095379658682242241 -0.071103627314902815 0.068983449702075775
reconstruction relativeRMS1.7518880075977435e-13 peak1.0245710529988017e-14
residual peak0.005516737699508667 scale45.316637044800146
backend frames96000 latency9216 actualInputPeak0.25 pitch12 preserve0 formant0 base0
synthesis alpha0.19945789750539875 denominator0.62890469839252006 maxState0.071687350693537669 outputPeak0.088853805210171513
warpMappedCenterHz1793.3928051121468 linearTargetHz1797.9684922520178
metrics candidate frames96000 finite1 last48000Energy2.7012617181866396e-06 halfPeriodCorr-0.938496353 fullPeriodCorr0.997461745 doublePeriodCorr0.995469463 gridOverTotal0.476054263 oddOverTotal0.468198581 peakHz1800 targetEnvelopeHz1797.96849 envelopeRelativeError0.00112989063 periodicityPass1 envelopePass1
backend frames96000 latency9216 actualInputPeak0.034501008689403534 pitch12 preserve0 formant0 base0
metrics uncompensated frames96000 finite1 last48000Energy0.00029235046019783186 halfPeriodCorr-0.494405033 fullPeriodCorr0.994710073 doublePeriodCorr0.993285606 gridOverTotal0.756128132 oddOverTotal0.7193914 peakHz2520 targetEnvelopeHz1797.96849 envelopeRelativeError0.401581847 periodicityPass1 envelopePass0
metrics ideal35 frames96000 finite1 last48000Energy0.0015830882701892133 halfPeriodCorr-6.5581406e-07 fullPeriodCorr0.999425598 doublePeriodCorr0.999936188 gridOverTotal1 oddOverTotal0.500000327 peakHz595 targetEnvelopeHz600 envelopeRelativeError0.00833333333 periodicityPass1 envelopePass1
metrics unchanged70 frames96000 finite1 last48000Energy0.00079181423290733313 halfPeriodCorr0.999744762 fullPeriodCorr0.999425659 doublePeriodCorr0.999936187 gridOverTotal1 oddOverTotal1.05273888e-28 peakHz630 targetEnvelopeHz600 envelopeRelativeError0.05 periodicityPass1 envelopePass1
metrics evenOnly35 frames96000 finite1 last48000Energy0.00079154361687814375 halfPeriodCorr0.999744758 fullPeriodCorr0.999425651 doublePeriodCorr0.999936186 gridOverTotal1 oddOverTotal1.03145327e-28 peakHz630 targetEnvelopeHz600 envelopeRelativeError0.05 periodicityPass1 envelopePass1
RESULT candidatePasses2/3; static-oracle only; no suite expansion or streaming claim

```

### Independent spectral output

```text
row0 independentPeakHz599.573923607 targetHz600.000000000 relativeError0.000710127 envelopePass1 gridOverTotal0.997114199 oddOverTotal0.593869296
row1 independentPeakHz1200.720453494 targetHz600.000000000 relativeError1.001200756 envelopePass0 gridOverTotal0.999409923 oddOverTotal0.415000650
row2 independentPeakHz383.863249546 targetHz600.000000000 relativeError0.360227917 envelopePass0 gridOverTotal0.996801161 oddOverTotal0.562866823
row3 independentPeakHz313.963450046 targetHz600.000000000 relativeError0.476727583 envelopePass0 gridOverTotal0.998810094 oddOverTotal0.500029528
row4 independentPeakHz1800.453822594 targetHz1797.968492252 relativeError0.001382299 envelopePass1 gridOverTotal0.999536557 oddOverTotal0.970379494
row5 independentPeakHz2520.110152083 targetHz1797.968492252 relativeError0.401643112 envelopePass0 gridOverTotal0.999783210 oddOverTotal0.748824816
row6 independentPeakHz595.000000622 targetHz600.000000000 relativeError0.008333332 envelopePass1 gridOverTotal1.000000000 oddOverTotal0.500000327
row7 independentPeakHz629.999998540 targetHz600.000000000 relativeError0.049999998 envelopePass1 gridOverTotal0.999999981 oddOverTotal0.000000000
row8 independentPeakHz629.999998540 targetHz600.000000000 relativeError0.049999998 envelopePass1 gridOverTotal1.000000000 oddOverTotal0.000000000

```
