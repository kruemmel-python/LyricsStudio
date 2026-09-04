#pragma once

#include "common.hpp"

#include <cstdint>
#include <vector>

namespace kg {

struct AudioEnvelopePoint {
    double time{};
    float rms{};
    float peak{};
};

struct AudioRange {
    double start{};
    double end{};
};

enum class AudioEnergyBand { Low, Medium, High };

struct AudioSection {
    double start{};
    double end{};
    AudioEnergyBand energy{AudioEnergyBand::Medium};
    float meanRms{};
};

struct AudioAnalysis {
    double duration{};
    std::uint32_t sampleRate{};
    std::vector<AudioEnvelopePoint> envelope;
    std::vector<double> onsets;
    std::vector<AudioRange> pauses;
    std::vector<AudioSection> sections;

    [[nodiscard]] bool Empty() const noexcept { return envelope.empty(); }
};

class AudioAnalyzer {
public:
    [[nodiscard]] bool Analyze(const fs::path& path, AudioAnalysis& result, std::wstring& error) const;
};

} // namespace kg
