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
100Hz/F1600 pitch+12/formant0;70Hz/F1600 pitch-12/formant0;
180Hz/F11200 pitch+12/formant+7. Here F1 denotes envelope center, not source F0.
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
