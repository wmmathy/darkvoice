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

Planned colour_alias_audit task owns only new tools/qualification/colour_alias_probe.cpp,
optional new colour_alias_analyse.py and this report, in isolated worktree with
matching feature/qualification branch. Strict C++20 O2/Werror public-core build;
small finite measurements only, no production or CMake edits, commits or pushes.
