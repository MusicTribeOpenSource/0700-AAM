// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
namespace mt {
class ADSR : public AudIO::ADSR<double> {
public:
    ADSR() { setEnabled(true); setSampleRate(48000); setParameters(0, 0, 0, 0, 127); }
    void setParameters(double attackSamples, double holdSamples, double decaySamples, double releaseSamples, double sustainMidi) noexcept {
        const auto rate = std::max(1., sampleRate());
        AudIO::ADSR<double>::setParameters({std::max(0., attackSamples) / rate, std::max(0., holdSamples) / rate,
            std::max(0., decaySamples) / rate, juce::jlimit(0., 1., sustainMidi / 127.), std::max(0., releaseSamples) / rate});
    }
};
enum class FilterType { LOW_PASS, HIGH_PASS, BAND_PASS };
class Filter {
public:
    inline static const juce::NormalisableRange<double> freqRange{20., 20000., 0., .25};
    void setSampleRate(double rate) { sampleRate = std::max(1., rate); filter.prepare({sampleRate, 1, 2}); update(); }
    void setFilterType(FilterType type) {
        using Type = juce::dsp::StateVariableTPTFilterType;
        filter.setType(type == FilterType::HIGH_PASS ? Type::highpass : type == FilterType::BAND_PASS ? Type::bandpass : Type::lowpass);
    }
    void setCutoff(double normalized) { cutoff = juce::jlimit(0., 1., normalized); update(); }
    void setResonance(double midi) { filter.setResonance(static_cast<float>(.707 + juce::jlimit(0., 1., midi / 127.) * 9.)); }
    void reset() { filter.reset(); }
    void process(juce::AudioBuffer<float>& audio) {
        for (int c = 0; c < std::min(2, audio.getNumChannels()); ++c)
            for (int n = 0; n < audio.getNumSamples(); ++n) audio.setSample(c, n, filter.processSample(c, audio.getSample(c, n)));
    }
private:
    void update() { filter.setCutoffFrequency(static_cast<float>(juce::jlimit(1., sampleRate * .49, freqRange.convertFrom0to1(cutoff)))); }
    juce::dsp::StateVariableTPTFilter<float> filter;
    double sampleRate = 48000, cutoff = 1;
};
}
