#pragma once

#include "common.hpp"
#include "audio_analysis.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>
#include <limits>
#include <cmath>

namespace kg {

enum class VisualTransition { Cut, CrossFade };

struct VisualStyle {
    float zoomGain{1.0f};
    float driftGain{1.0f};
    float lyricsScale{1.0f};
};

struct VisualClip {
    fs::path imagePath;
    double start{};
    double end{};
    VisualTransition transition{VisualTransition::CrossFade};
    double transitionSeconds{0.65};
    VisualStyle style{};
};

struct VisualTimeline {
    std::vector<VisualClip> clips;

    [[nodiscard]] bool Empty() const noexcept { return clips.empty(); }
    [[nodiscard]] std::size_t Size() const noexcept { return clips.size(); }

    [[nodiscard]] const VisualClip* ClipAt(double seconds) const noexcept {
        if (clips.empty()) return nullptr;
        if (seconds <= clips.front().start) return &clips.front();
        for (const auto& clip : clips) {
            if (seconds >= clip.start && seconds < clip.end) return &clip;
        }
        return &clips.back();
    }
};

inline VisualTimeline BuildEvenVisualTimeline(const std::vector<fs::path>& images, double duration,
                                              double crossFadeSeconds = 0.65) {
    VisualTimeline timeline;
    if (images.empty() || duration <= 0.0) return timeline;
    const double clipDuration = duration / static_cast<double>(images.size());
    timeline.clips.reserve(images.size());
    for (std::size_t i = 0; i < images.size(); ++i) {
        const double start = clipDuration * static_cast<double>(i);
        const double end = i + 1 == images.size() ? duration : clipDuration * static_cast<double>(i + 1);
        const double safeFade = std::min({crossFadeSeconds, clipDuration * 0.35, std::max(0.0, end - start)});
        timeline.clips.push_back(VisualClip{images[i], start, end,
            i + 1 < images.size() ? VisualTransition::CrossFade : VisualTransition::Cut, safeFade, {}});
    }
    return timeline;
}

inline VisualTimeline BuildSmartVisualTimeline(const std::vector<fs::path>& images, double duration,
                                               const AudioAnalysis& analysis, double crossFadeSeconds = 0.65) {
    if (images.empty() || duration <= 0.0 || images.size() == 1 || analysis.onsets.empty())
        return BuildEvenVisualTimeline(images, duration, crossFadeSeconds);

    const std::size_t boundaryCount = images.size() - 1;
    std::vector<double> boundaries;
    boundaries.reserve(boundaryCount);
    double previous = 0.0;
    const double idealSpacing = duration / static_cast<double>(images.size());
    const double minSpacing = std::max(1.25, idealSpacing * 0.38);
    const double searchRadius = std::max(1.5, idealSpacing * 0.45);

    for (std::size_t i = 1; i <= boundaryCount; ++i) {
        const double ideal = idealSpacing * static_cast<double>(i);
        double best = ideal;
        double bestScore = std::numeric_limits<double>::max();
        for (const double onset : analysis.onsets) {
            if (onset <= previous + minSpacing || onset >= duration - minSpacing) continue;
            const double distance = std::abs(onset - ideal);
            if (distance <= searchRadius && distance < bestScore) { best = onset; bestScore = distance; }
        }
        // Pausenenden sind ebenfalls sehr gute Schnittpunkte und werden leicht bevorzugt.
        for (const auto& pause : analysis.pauses) {
            const double candidate = pause.end;
            if (candidate <= previous + minSpacing || candidate >= duration - minSpacing) continue;
            const double distance = std::abs(candidate - ideal) * 0.82;
            if (std::abs(candidate - ideal) <= searchRadius && distance < bestScore) { best = candidate; bestScore = distance; }
        }
        best = std::clamp(best, previous + minSpacing, duration - minSpacing * static_cast<double>(boundaryCount - i + 1));
        boundaries.push_back(best);
        previous = best;
    }

    VisualTimeline timeline;
    timeline.clips.reserve(images.size());
    for (std::size_t i = 0; i < images.size(); ++i) {
        const double start = i == 0 ? 0.0 : boundaries[i - 1];
        const double end = i + 1 == images.size() ? duration : boundaries[i];
        const double clipDuration = std::max(0.0, end - start);
        const double safeFade = std::min({crossFadeSeconds, clipDuration * 0.35, clipDuration});
        timeline.clips.push_back(VisualClip{images[i], start, end,
            i + 1 < images.size() ? VisualTransition::CrossFade : VisualTransition::Cut, safeFade, {}});
    }
    return timeline;
}

} // namespace kg
