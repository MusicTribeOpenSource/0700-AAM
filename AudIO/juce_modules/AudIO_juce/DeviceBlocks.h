// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

namespace mt::detail {
// All storage is prepared before the device starts. The callback sees exactly
// one fixed graph block; the device sees its requested size. This adds one
// graph block of latency, including when the device requests a full block.
template <class Process>
void deviceBlocks(const float* const* in, int ni, float* const* out, int no, int frames,
                  juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output,
                  uint32_t& filled, Process&& process) noexcept {
    if (frames <= 0) return;
    for (int c = 0; c < no; ++c) if (out && out[c]) std::fill_n(out[c], frames, 0.f);
    const auto block = static_cast<uint32_t>(output.getNumSamples());
    if (!block || input.getNumSamples() != output.getNumSamples() || filled >= block) return;
    for (int done = 0; done < frames;) {
        const auto n = std::min<uint32_t>(block - filled, static_cast<uint32_t>(frames - done));
        for (int c = 0; c < input.getNumChannels(); ++c) {
            auto* dst = input.getWritePointer(c, static_cast<int>(filled));
            if (c < ni && in && in[c]) std::copy_n(in[c] + done, n, dst);
            else std::fill_n(dst, n, 0.f);
        }
        for (int c = 0; c < output.getNumChannels() && c < no; ++c)
            if (out && out[c]) std::copy_n(output.getReadPointer(c, static_cast<int>(filled)), n, out[c] + done);
        filled += n; done += static_cast<int>(n);
        if (filled == block) {
            output.clear(); process(input, output); filled = 0;
        }
    }
}
}
