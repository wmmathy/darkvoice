"""Developer-only WORLD measurements; passing these six rows is not qualification.

Narrow48k two-second Gaussian fixtures and optional retained42 manifest.
Final-half metrics retained from pitch
coupling audit. Correlation alone cannot distinguish octave-down from unchanged
70Hz; ideal35/unchanged70/even-only controls explicitly demonstrate ambiguity.
Malformed inputs fail; valid negative quality results are reported, not hidden.
The matrix-periodicity suite also exits nonzero for any hard-gate failure.
"""
from pathlib import Path
import argparse
import json
import numpy as np

RATE = 48000
FRAMES = 96000
FFT_FRAMES = 1048576
CASES = ((100, 600, 12, 0), (70, 600, -12, 0), (180, 1200, 12, 7))


def load(path, dtype, count):
    if path.stat().st_size != np.dtype(dtype).itemsize*count:
        raise ValueError(f"invalid measurement byte length: {path}")
    data = np.fromfile(path, dtype=dtype).astype(np.float64)
    if len(data) != count or not np.isfinite(data).all():
        raise ValueError(f"invalid length or nonfinite data: {path}")
    return data


def correlation(x, hz, rate=RATE):
    lag = int(np.floor(rate / hz + .5))
    a, b = x[lag:], x[:-lag]
    denominator = np.sqrt(np.dot(a, a) * np.dot(b, b))
    if not np.isfinite(denominator) or denominator <= 0:
        raise ValueError("undefined correlation")
    return float(np.dot(a, b) / denominator)


def metrics(x, target, envelope, rate=RATE, broad_hann=False):
    if len(x) < 2 or not np.isfinite(x).all() or np.dot(x, x) <= 0:
        raise ValueError("invalid final-second signal")
    padded = np.abs(np.fft.rfft(x * np.hanning(len(x)), n=FFT_FRAMES))
    k = int(padded.argmax())
    if not 0 < k < len(padded) - 1:
        raise ValueError("noninterpolable peak")
    y = np.log(np.maximum(padded[k-1:k+2], 1e-30))
    denominator = y[0] - 2*y[1] + y[2]
    if denominator == 0 or not np.isfinite(denominator):
        raise ValueError("undefined peak interpolation")
    peak = float((k + .5*(y[0]-y[2])/denominator)*rate/FFT_FRAMES)
    # The retained broad-envelope measurement scans expected harmonic amplitudes
    # between 200 and 3000Hz, rectangular final-half DFT. A separate unconstrained
    # Hann spectral peak avoids conflating on-grid projection with dominant sound.
    times = np.arange(len(x)) / rate
    grid_peak = best = 0.0
    for h in range(1, int(3000/target)+1):
        hz = h*target
        if hz < 200:
            continue
        amplitude = 2*abs(np.dot(x, np.exp(-2j*np.pi*hz*times)))/len(x)
        if amplitude > best:
            best, grid_peak = amplitude, hz
    powers = np.abs(np.fft.rfft(x*np.hanning(len(x))))**2
    odd = even = 0.0
    for h in range(1, 81):
        center = int(np.floor(h*target*len(x)/rate+.5))
        if center+2 >= len(powers):
            break
        energy = float(powers[max(0, center-2):center+3].sum())
        if h % 2:
            odd += energy
        else:
            even += energy
    total = float(powers.sum())
    if total <= 0 or odd+even <= 0 or grid_peak <= 0:
        raise ValueError("undefined spectral energy")
    corr = correlation(x, target, rate)
    error = abs(grid_peak/envelope-1)
    result=dict(corrHalfT=correlation(x, 2*target, rate), corrT=corr,
                corrDoubleT=correlation(x, target/2, rate), gridPeakHz=grid_peak,
                hannPeakHz=peak, envelopeError=error,
                hannEnvelopeError=abs(peak/envelope-1), capturedGrid=(odd+even)/total,
                oddOverTotal=odd/total, oddOverCaptured=odd/(odd+even),
                periodicityPass=corr > .8, envelopePass=error < .2,
                hannEnvelopePass=abs(peak/envelope-1) < .2)
    if broad_hann:
        low_hz,high_hz=200.,3000.
        low=int(np.ceil(low_hz*FFT_FRAMES/rate));high=int(np.floor(high_hz*FFT_FRAMES/rate))
        selected=low+int(np.argmax(padded[low:high+1]));offset=0.
        if low<selected<high:
            amplitudes=np.log(np.maximum(padded[selected-1:selected+2],1e-30))
            curve=amplitudes[0]-2*amplitudes[1]+amplitudes[2]
            if np.isfinite(curve) and curve!=0:
                offset=float(np.clip(.5*(amplitudes[0]-amplitudes[2])/curve,-1,1))
        result.update(broadHannPeakHz=float((selected+offset)*rate/FFT_FRAMES),
                      broadHannSearchLowHz=low_hz,broadHannSearchHighHz=high_hz)
    return result


def controls():
    time = np.arange(RATE)/RATE
    rows = {}
    for label, fundamental, even_only in (("ideal35", 35, False),
                                           ("unchanged70", 70, False),
                                           ("evenOnly35", 35, True)):
        x = np.zeros(RATE)
        for h in range(1, 81):
            if even_only and h % 2:
                continue
            f = h*fundamental
            amplitude = .025*(np.exp(-.5*((f-600)/100)**2)+.002/h)
            x += amplitude*np.sin(2*np.pi*f*time)
        rows[label] = metrics(x, 35, 600)
    if not all(r["corrT"] > .8 for r in rows.values()):
        raise ValueError("periodicity negative control changed")
    if rows["ideal35"]["oddOverTotal"] <= .1 or any(
        rows[name]["oddOverTotal"] >= 1e-8 for name in ("unchanged70", "evenOnly35")
    ):
        raise ValueError("odd-energy controls changed")
    return rows


def manifest():
    rows = []
    for rate in (44100,48000,88200,96000):
        for pitch in (-12,-7,7,12):
            rows.append((rate,110,0,pitch,0,"sine"))
    for pitch,formant in ((7,0),(0,7),(0,-7)):
        rows.append((48000,100,0,pitch,formant,"multi"))
    for center in (600,1200):
        for formant in (-7,7):
            rows.append((48000,100,center,0,formant,"gaussian"))
    for rate in (48000,96000):
        for f0 in (70,180):
            for center in (600,1200):
                for formant in (-7,7):
                    rows.append((rate,f0,center,0,formant,"gaussian"))
    rows.extend((48000,*c,"combined") for c in CASES)
    assert len(rows)==42
    return rows


def matrix_manifest():
    rows = []
    for rate in (48000, 96000):
        for f0 in (70, 100, 180):
            for center in (600, 1200):
                for pitch in (-12, -7, 0, 7, 12):
                    for formant in (-7, 0, 7):
                        rows.append((rate, f0, center, pitch, formant, "matrix"))
    assert len(rows) == 180
    return rows


def peak_on_grid(x, rate, target, low, high):
    time = np.arange(len(x))/rate
    best = peak = 0.0
    for h in range(1,int(high/target)+1):
        hz = h*target
        if hz < low:
            continue
        amplitude = 2*abs(np.dot(x,np.exp(-2j*np.pi*hz*time)))/len(x)
        if amplitude > best:
            best,peak = amplitude,hz
    if peak <= 0:
        raise ValueError("empty harmonic peak search")
    return peak


def crossing_frequency(x, rate):
    indices = np.flatnonzero((x[:-1] <= 0) & (x[1:] > 0))+1
    if len(indices)<2:
        return 0.0
    crossings = indices-1-x[indices-1]/(x[indices]-x[indices-1])
    return float((len(indices)-1)*rate/(crossings[-1]-crossings[0]))


def recompute_periodicity(directory,name,original,rate,time,f0,verification_cache=None):
    count=len(time)
    cache_key=None
    if verification_cache is not None:
        suffixes=("-time.f64","-f0.f64","-candidate-0.f64","-candidate-1.f64",
                  "-candidate-2.f64","-candidate-3.f64","-backend-status.i32",
                  "-periods.f64","-scores.f64","-selected.i32")
        cache_key=(rate,(directory/(name+"-input.raw")).read_bytes(),
                   *((directory/(name+suffix)).read_bytes() for suffix in suffixes))
        if cache_key in verification_cache:
            previous,candidates=verification_cache[cache_key]
            return dict(previous,decisionVerificationReused=True),candidates
    candidates=np.stack([load(directory/(name+f"-candidate-{i}.f64"),"<f8",count) for i in range(4)],axis=1)
    statuses=load(directory/(name+"-backend-status.i32"),"<i4",4)
    if ((statuses<0)|(statuses>4)).any():
        raise ValueError("invalid candidate backend status")
    for i in range(4):
        if statuses[i]!=0 and (candidates[:,i]!=0).any():
            raise ValueError("unavailable backend has nonzero contour")
    expected_periods=np.zeros((count,4));expected_scores=np.full((count,4),-1.)
    expected_selected=np.full(count,-1,dtype=np.int32);expected_f0=np.zeros(count)
    size=4096 if rate<=48000 else 8192
    def nccf(a,b):
        denominator=np.sqrt(np.dot(a,a)*np.dot(b,b))
        return float(np.dot(a,b)/denominator) if denominator>0 and np.isfinite(denominator) else -1.
    for frame,t in enumerate(time):
        start=min(max(int(np.floor(t*rate))-size//2,0),len(original)-size)
        window=original[start:start+size].copy();window-=np.mean(window)
        for method,frequency in enumerate(candidates[frame]):
            if not 50<=frequency<=800:
                continue
            lag=rate/frequency;k=int(np.floor(lag+.5))
            values=[nccf(window[:-j],window[j:]) if 1<=j<size else -1. for j in (k-1,k,k+1)]
            left,middle,right=values;curvature=left-2*middle+right
            if middle>=left and middle>=right and curvature<0:
                lag=k+float(np.clip(.5*(left-right)/curvature,-1,1))
            expected_periods[frame,method]=lag
            low=int(np.floor(lag));ceiling=int(np.ceil(lag));fraction=lag-low
            indices=np.arange(size-ceiling)
            delayed=window[indices+low]+fraction*(window[np.minimum(indices+low+1,size-1)]-window[indices+low])
            expected_scores[frame,method]=nccf(window[:size-ceiling],delayed)
        maximum=float(expected_scores[frame].max())
        eligible=[i for i in range(4) if expected_periods[frame,i]>0 and expected_scores[frame,i]>=maximum-1e-6]
        if eligible:
            chosen=max(eligible,key=lambda i:rate/expected_periods[frame,i])
            expected_selected[frame]=chosen
            frequency=rate/expected_periods[frame,chosen]
            if expected_scores[frame,chosen]>=.9 and 50<=frequency<=800:
                expected_f0[frame]=frequency
    periods=load(directory/(name+"-periods.f64"),"<f8",count*4).reshape(count,4)
    scores=load(directory/(name+"-scores.f64"),"<f8",count*4).reshape(count,4)
    selected=load(directory/(name+"-selected.i32"),"<i4",count)
    period_error=float(np.max(np.abs(periods-expected_periods)))
    score_error=float(np.max(np.abs(scores-expected_scores)))
    f0_error=float(np.max(np.abs(f0-expected_f0)))
    if period_error>1e-7 or score_error>1e-10 or f0_error>1e-7 or not np.array_equal(selected,expected_selected):
        raise ValueError(f"independent periodicity recompute mismatch {period_error} {score_error} {f0_error}")
    result=dict(periodRecomputeMaxError=period_error,scoreRecomputeMaxError=score_error,
                f0RecomputeMaxError=f0_error,backendStatus=statuses.astype(int).tolist())
    if verification_cache is not None:
        verification_cache[cache_key]=(result,candidates)
        result=dict(result,decisionVerificationReused=False)
    return result,candidates


def analyse_periodicity_controls(directory):
    rows={};rate=48000;count=401
    for name in ("ideal35","unchanged70","evenOnly35","noise","silence"):
        original=load(directory/(name+"-input.raw"),"<f4",96000)
        time=load(directory/(name+"-time.f64"),"<f8",count)
        f0=load(directory/(name+"-f0.f64"),"<f8",count)
        if not np.allclose(time,np.arange(count)*.005,rtol=0,atol=1e-12):
            raise ValueError("control time grid mismatch")
        row,candidates=recompute_periodicity(directory,name,original,rate,time,f0)
        mapping=(directory/(name+"-reaper.txt")).read_text().split()
        if len(mapping)!=6 or mapping[::2]!=["pcmClips","sourceFrames","worldFrames"]:
            raise ValueError("control REAPER mapping metadata")
        clips,source_count,world_count=map(int,mapping[1::2])
        rounded=np.sign(original)*np.floor(np.abs(original)*32768+.5)
        if clips!=np.count_nonzero((rounded < -32768)|(rounded > 32767)) or world_count!=count or source_count<0:
            raise ValueError("control REAPER mapping counts")
        raw=load(directory/(name+"-reaper-f0.raw"),"<f4",source_count)
        mapped=np.zeros(count);common=min(count,source_count);mapped[:common]=np.maximum(raw[:common],0)
        if not np.array_equal(mapped,candidates[:,3]):
            raise ValueError("control REAPER mapping mismatch")
        final=f0[(time>=1)&(time<2)];voiced=final[final>0]
        start=rate-2048;window=original[start:start+4096].copy();window-=np.mean(window)
        def score(lag):
            low=int(np.floor(lag));ceiling=int(np.ceil(lag));fraction=lag-low
            indices=np.arange(len(window)-ceiling)
            delayed=window[indices+low]+fraction*(window[np.minimum(indices+low+1,len(window)-1)]-window[indices+low])
            direct=window[:len(window)-ceiling]
            denominator=np.sqrt(np.dot(direct,direct)*np.dot(delayed,delayed))
            return float(np.dot(direct,delayed)/denominator) if denominator>0 else -1.
        row.update(inputKind=name,voicedFraction=len(voiced)/len(final),medianFrequencyHz=float(np.median(voiced)) if len(voiced) else None,
                   diagnosticNccf35=score(rate/35),diagnosticNccf70=score(rate/70),pcmClips=clips,
                   reaperSourceFrames=source_count,falseVoiceOnNoiseOrSilence=bool(len(voiced)>0 and name in ("noise","silence")))
        rows[name]=row
    return dict(suite="periodicity-controls",rows=rows)


def analyse_row(directory,method,index,case,verification_cache=None):
    rate,fundamental,center,pitch,formant,kind=case
    name = f"{method}-{index}"
    seconds = 1.5 if kind=="sine" else 2
    frames = int(seconds*rate)
    output = load(directory/(name+".raw"), "<f4", frames)
    original = load(directory/(name+"-input.raw"), "<f4", frames)
    if np.dot(original, original) <= 0:
        raise ValueError("empty input fixture")
    count = int(seconds/.005)+1
    time = load(directory/(name+"-time.f64"), "<f8", count)
    f0 = load(directory/(name+"-f0.f64"), "<f8", count)
    if not np.allclose(time, np.arange(count)*.005, rtol=0, atol=1e-12) or (f0 < 0).any():
        raise ValueError("invalid F0/time contour")
    reaper_info = {}
    periodicity_candidates=None
    if method in ("periodicity","retained-periodicity","matrix-periodicity"):
        reaper_info,periodicity_candidates=recompute_periodicity(directory,name,original,rate,time,f0,verification_cache)
    if method in ("reaper","retained-reaper","periodicity","retained-periodicity","matrix-periodicity"):
        mapping = (directory/(name+"-reaper.txt")).read_text().split()
        if len(mapping)!=6 or mapping[::2]!=["pcmClips","sourceFrames","worldFrames"]:
            raise ValueError("invalid REAPER mapping metadata")
        clips,source_count,world_count = map(int,mapping[1::2])
        rounded = np.sign(original)*np.floor(np.abs(original)*32768+.5)
        actual_clips = int(np.count_nonzero((rounded < -32768) | (rounded > 32767)))
        if clips!=actual_clips or world_count!=count or source_count<0 or (source_count==0 and (periodicity_candidates is None or reaper_info["backendStatus"][3]==0)):
            raise ValueError("REAPER clipping/frame metadata mismatch")
        raw_f0 = load(directory/(name+"-reaper-f0.raw"),"<f4",source_count)
        mapped = np.zeros(count)
        common = min(count,source_count)
        mapped[:common] = np.maximum(raw_f0[:common],0)
        if not np.array_equal(mapped,f0 if periodicity_candidates is None else periodicity_candidates[:,3]):
            raise ValueError("REAPER index mapping mismatch")
        reaper_info.update(pcmClips=clips,reaperSourceFrames=source_count,
                           worldFrames=count,missingFrames=max(count-source_count,0),
                           truncatedFrames=max(source_count-count,0))
    f0 = f0[(time >= seconds*.5) & (time < seconds)]
    voiced = f0[f0 > 0]
    coverage = len(voiced)/len(f0)
    cents = float(np.median(1200*np.log2(voiced/fundamental))) if len(voiced) else 1e30
    metadata = (directory/(name+".txt")).read_text().split()
    if len(metadata) != 4 or metadata[0] != "voicedCoverage" or metadata[2] != "medianCents":
        raise ValueError("invalid metadata")
    observed_metadata = np.array([float(metadata[1]), float(metadata[3])])
    if not np.isfinite(observed_metadata).all() or abs(observed_metadata[0]-coverage) > 1e-12 or abs(observed_metadata[1]-cents) > 1e-9:
        raise ValueError("F0 metadata disagrees with contour")
    target = fundamental*2**(pitch/12)
    scale = 2**(formant/12)
    envelope = center*scale if center else (target if kind=="sine" else 600*scale)
    tail = output[frames//2:]
    row = metrics(tail,target,envelope,rate,broad_hann=kind=="matrix")
    row.update(reaper_info)
    row.update(rate=rate,kind=kind,sourceF0=fundamental,pitch=pitch,formant=formant,
               inputVoicedCoverage=coverage,inputMedianCents=cents,
               inputEstimatePass=coverage >= .9 and abs(cents) <= 20)
    if kind=="matrix":
        measured=peak_on_grid(tail,rate,target,envelope*.5,envelope*1.5)
        resolvable=abs(np.floor(envelope/target+.5)*target-envelope)<=envelope*.2
        row.update(matrixGridPeakHz=measured,matrixGridEnvelopeError=abs(measured/envelope-1),
                   matrixSearchLowHz=envelope*.5,matrixSearchHighHz=envelope*1.5,
                   sourceCenterHz=center,targetEnvelopeHz=envelope,targetF0Hz=target,
                   fullHannSearchLowHz=0.,fullHannSearchHighHz=rate/2,
                   nearestGridResolvable=bool(resolvable),retainedPass=bool(row["periodicityPass"]))
    elif kind=="sine":
        hz = crossing_frequency(tail,rate)
        cents_error = float(1200*np.log2(hz/target)) if hz>0 else 1e30
        row.update(crossingFrequency=hz,sineCents=cents_error,
                   retainedPass=abs(cents_error)<20)
    elif kind=="multi":
        boundary = np.sqrt(600*1200)*scale
        upper = np.sqrt(1200*2500)*scale
        first = peak_on_grid(tail,rate,target,200,boundary)
        second = peak_on_grid(tail,rate,target,boundary,upper)
        row.update(firstEnvelopePeak=first,secondEnvelopePeak=second,
                   firstEnvelopeError=abs(first/(600*scale)-1),
                   secondEnvelopeError=abs(second/(1200*scale)-1))
        row["retainedPass"] = bool(row["periodicityPass"] and row["firstEnvelopeError"]<.2 and row["secondEnvelopeError"]<.2)
    else:
        row["retainedPass"] = bool(row["periodicityPass"] and row["envelopePass"])
    row["retainedSubsetPass"] = bool(row["inputEstimatePass"] and row["retainedPass"])
    return row


def matrix_cache_metadata(directory,name,index,rate,frames,first_sources):
    tokens=(directory/(name+"-cache.txt")).read_text().split()
    if len(tokens)!=8 or tokens[::2]!=["cacheHit","sourceName","sampleRate","inputBytes"]:
        raise ValueError("invalid trajectory cache metadata")
    hit,source,observed_rate,observed_bytes=tokens[1::2]
    if hit not in ("0","1") or int(observed_rate)!=rate or int(observed_bytes)!=frames*4:
        raise ValueError("trajectory cache metadata values mismatch")
    original=(directory/(name+"-input.raw")).read_bytes()
    key=(rate,original)
    expected_source=first_sources.setdefault(key,name)
    if source!=expected_source or int(hit)!=int(expected_source!=name):
        raise ValueError("trajectory cache source/hit mismatch")
    return dict(trajectoryCacheHit=bool(int(hit)),trajectoryCacheSource=source,
                trajectoryCacheInputBytes=len(original))


def analyse(directory, suite="narrow"):
    if suite=="periodicity-controls":
        return analyse_periodicity_controls(directory)
    is_matrix=suite=="matrix-periodicity"
    rows=matrix_manifest() if is_matrix else (manifest() if suite.startswith("retained") else [(48000,*c,"combined") for c in CASES])
    if is_matrix or suite.startswith("retained"):
        methods=(suite,)
    else:
        methods={"narrow-yin":("yin",),"narrow-reaper":("reaper",),
                 "narrow-periodicity":("periodicity",)}.get(suite,("dio","harvest"))
    result={};verification_cache={} if is_matrix else None;first_sources={}
    for method in methods:
        for index,case in enumerate(rows):
            name=f"{method}-{index}"
            try:
                row=analyse_row(directory,method,index,case,verification_cache)
                if is_matrix:
                    row.update(matrix_cache_metadata(directory,name,index,case[0],case[0]*2,first_sources))
                    row["hardPass"]=bool(row["retainedSubsetPass"])
                result[name]=row
            except (ValueError,OSError) as error:
                if not is_matrix:
                    raise
                result[name]=dict(hardPass=False,measurementError=str(error))
    output=dict(suite=suite,rows=result,controls=controls())
    if is_matrix:
        passed=sum(row["hardPass"] for row in result.values())
        output["summary"]=dict(expectedCases=180,measuredCases=len(result),hardPasses=passed,
                               hardFailures=180-passed,
                               trajectoryCacheHits=sum(row.get("trajectoryCacheHit",False) for row in result.values()),
                               independentDecisionVerifications=len(verification_cache),
                               reusedDecisionVerifications=sum(row.get("decisionVerificationReused",False) for row in result.values()))
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--suite", choices=("narrow","retained","retained-harvest","retained-yin","narrow-yin","retained-reaper","narrow-reaper","retained-periodicity","narrow-periodicity","periodicity-controls","matrix-periodicity"), default="narrow")
    args = parser.parse_args()
    result=analyse(args.directory,args.suite)
    print(json.dumps(result, indent=2, allow_nan=False))
    if args.suite=="matrix-periodicity" and result["summary"]["hardFailures"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
