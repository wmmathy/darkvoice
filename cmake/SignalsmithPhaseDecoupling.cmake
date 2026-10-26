# Developer-only private-copy experiment. Pinned sources and licenses stay unchanged.
# The caller verified the original private header before applying this finite patch.
function(darkvoice_phase_replace before after)
    string(FIND "${darkvoice_stretch_header}" "${before}" first)
    if(first LESS 0)
        message(FATAL_ERROR "Phase experiment anchor missing")
    endif()
    string(LENGTH "${before}" length)
    math(EXPR after_first "${first} + ${length}")
    string(SUBSTRING "${darkvoice_stretch_header}" ${after_first} -1 remaining)
    string(FIND "${remaining}" "${before}" second)
    if(NOT second LESS 0)
        message(FATAL_ERROR "Phase experiment anchor is not unique")
    endif()
    string(REPLACE "${before}" "${after}" changed "${darkvoice_stretch_header}")
    set(darkvoice_stretch_header "${changed}" PARENT_SCOPE)
endfunction()
darkvoice_phase_replace([==[Sample inputEnergy;]==] [==[Sample inputEnergy;
		Sample synthesisInputEnergy = 0;]==])
darkvoice_phase_replace([==[_channelBands.assign(_channelBands.size(), Band());]==] [==[_channelBands.assign(_channelBands.size(), Band());
		channelPredictions.assign(channelPredictions.size(), Prediction());]==])
darkvoice_phase_replace([==[channelPredictions.resize(channels*bands);]==] [==[channelPredictions.assign(channels*bands, Prediction());]==])
darkvoice_phase_replace([==[
			return;
		}
		if (blockProcess.processFormants) {
			if (step < 3)]==] [==[
			for (auto &bin : _channelBands) bin.synthesisInputEnergy = bin.inputEnergy;
			return;
		}
		if (blockProcess.processFormants) {
			if (step < 3)]==])
darkvoice_phase_replace([==[bins[b].inputEnergy *= energyRatio;]==] [==[#ifdef DARKVOICE_PHASE_GAIN
                    energyRatio = DARKVOICE_PHASE_GAIN(energyRatio, c, b, inputF);
#endif
                    bins[b].synthesisInputEnergy *= energyRatio;]==])
darkvoice_phase_replace([==[spectrumBands[b] = channelBands[b].output;]==] [==[// Reference history is never replaced by the emitted spectrum.
                            const auto map = outputMap[b];
                            Sample desired = getFractional<&Band::synthesisInputEnergy>(c, map.inputBin);
                            desired *= std::max<Sample>(0, map.freqGrad);
                            const Sample reference = predictionsForChannel(c)[b].energy;
                            Sample gain = reference > 0 ? (desired == reference ? Sample(1) : std::sqrt(desired/reference)) : Sample(0);
                            spectrumBands[b] = gain == 1 ? channelBands[b].output : channelBands[b].output*gain;
#ifdef DARKVOICE_PHASE_EMIT
                            DARKVOICE_PHASE_EMIT(reference, desired, channelBands[b].output, spectrumBands[b]);
#endif]==])
darkvoice_phase_replace([==[void processSpectrum(size_t step) {]==] [==[void processSpectrum(size_t step) {
#ifdef DARKVOICE_PHASE_TRACE
        struct SnapshotOnReturn {
            SignalsmithStretch *owner; size_t step;
            ~SnapshotOnReturn() { owner->phaseSnapshot(step); }
        } snapshot{this, step};
#endif]==])
darkvoice_phase_replace([==[Sample phaseNorm = _impl::norm(phase);]==] [==[Sample phaseNorm = _impl::norm(phase);
#ifdef DARKVOICE_PHASE_TRACE
            DARKVOICE_PHASE_TRACE(30, energy);
            DARKVOICE_PHASE_TRACE(31, phase.real());
            DARKVOICE_PHASE_TRACE(32, phase.imag());
            DARKVOICE_PHASE_TRACE(33, phaseNorm <= noiseFloor);
#endif]==])
darkvoice_phase_replace([==[auto *predictions = predictionsForChannel(maxChannel);]==] [==[#ifdef DARKVOICE_PHASE_TRACE
                DARKVOICE_PHASE_TRACE(34, maxChannel);
#endif
                auto *predictions = predictionsForChannel(maxChannel);]==])
darkvoice_phase_replace([==[std::vector<Prediction> channelPredictions;]==] [==[std::vector<Prediction> channelPredictions;
#ifdef DARKVOICE_PHASE_TRACE
    void phaseSnapshot(size_t step) {
        DARKVOICE_PHASE_TRACE(1, step);
        DARKVOICE_PHASE_TRACE(2, blockProcess.newSpectrum);
        DARKVOICE_PHASE_TRACE(3, blockProcess.timeFactor);
        DARKVOICE_PHASE_TRACE(42, smoothEnergyState);
        for (int c=0; c<channels; ++c) for (int b=0; b<bands; ++b) {
            const auto &bin = bandsForChannel(c)[b];
            const auto &p = predictionsForChannel(c)[b];
            DARKVOICE_PHASE_TRACE(4, bin.input.real());
            DARKVOICE_PHASE_TRACE(5, bin.input.imag());
            DARKVOICE_PHASE_TRACE(6, bin.prevInput.real());
            DARKVOICE_PHASE_TRACE(7, bin.prevInput.imag());
            DARKVOICE_PHASE_TRACE(8, bin.output.real());
            DARKVOICE_PHASE_TRACE(9, bin.output.imag());
            DARKVOICE_PHASE_TRACE(10, bin.inputEnergy);
            DARKVOICE_PHASE_TRACE(11, p.energy);
            DARKVOICE_PHASE_TRACE(12, p.input.real());
            DARKVOICE_PHASE_TRACE(13, p.input.imag());
        }
        for (int b=0; b<bands; ++b) {
            DARKVOICE_PHASE_TRACE(14, outputMap[b].inputBin);
            DARKVOICE_PHASE_TRACE(15, outputMap[b].freqGrad);
            DARKVOICE_PHASE_TRACE(16, energy[b]);
            DARKVOICE_PHASE_TRACE(17, smoothedEnergy[b]);
        }
        DARKVOICE_PHASE_TRACE(18, peaks.size());
        for (auto &p : peaks) {
            DARKVOICE_PHASE_TRACE(19, p.input);
            DARKVOICE_PHASE_TRACE(20, p.output);
        }
#ifdef DARKVOICE_PHASE_RNG
        DARKVOICE_PHASE_RNG(randomEngine);
#endif
    }
#endif]==])
file(WRITE "${darkvoice_stretch_private}/signalsmith-stretch.h" "${darkvoice_stretch_header}")

darkvoice_verify_file("${darkvoice_stretch_private}/signalsmith-stretch.h" 86bcf041a086f2dd7c1c8a94e91a16c522416bd6c62de003524dfe840d4184ae)
