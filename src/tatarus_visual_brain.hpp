#pragma once

#include "audio_analysis.hpp"
#include "common.hpp"
#include "image_analysis.hpp"
#include "visual_timeline.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace kg {

// Kompakter, persistenter Online-Lerner für visuelle Schnittentscheidungen.
// Kein LLM, keine Cloud: nur numerische Sensorik -> Gewichtung -> Entscheidung.
class TatarusVisualBrain {
public:
    static constexpr std::size_t FeatureCount = 8;
    static constexpr std::size_t ImageMatchFeatureCount = 12;
    using Features = std::array<float, FeatureCount>;
    using ImageMatchFeatures = std::array<float, ImageMatchFeatureCount>;

    [[nodiscard]] bool Load(const fs::path& path, std::wstring& error);
    [[nodiscard]] bool Save(const fs::path& path, std::wstring& error) const;

    [[nodiscard]] Features Sense(double time, double duration, const AudioAnalysis& analysis) const noexcept;
    [[nodiscard]] float Score(const Features& features) const noexcept;
    [[nodiscard]] float ScoreAt(double time, double duration, const AudioAnalysis& analysis) const noexcept;

    // pairwise preference: preferred soll künftig höher bewertet werden als rejected.
    void LearnPreference(const Features& preferred, const Features& rejected, float strength = 1.0f) noexcept;

    [[nodiscard]] VisualStyle StyleFor(const Features& features) const noexcept;
    [[nodiscard]] VisualTransition TransitionFor(const Features& features) const noexcept;
    void LearnStyle(const Features& features, VisualStyle preferred, VisualTransition transition) noexcept;

    [[nodiscard]] ImageMatchFeatures SenseImageMatch(const Features& context,
                                                     const VisualImageFeatures& image) const noexcept;
    [[nodiscard]] float ScoreImage(const Features& context, const VisualImageFeatures& image) const noexcept;
    void LearnImagePreference(const Features& context, const VisualImageFeatures& preferred,
                              const VisualImageFeatures& rejected, float strength = 1.0f) noexcept;

    [[nodiscard]] std::uint64_t TrainingEvents() const noexcept { return trainingEvents_; }
    [[nodiscard]] std::uint64_t StyleTrainingEvents() const noexcept { return styleTrainingEvents_; }
    [[nodiscard]] std::uint64_t ImageTrainingEvents() const noexcept { return imageTrainingEvents_; }
    [[nodiscard]] bool Trained() const noexcept { return trainingEvents_ > 0 || styleTrainingEvents_ > 0 || imageTrainingEvents_ > 0; }

private:
    std::array<float, FeatureCount> weights_{0.24f, 0.36f, 0.26f, 0.18f, 0.12f, 0.08f, -0.06f, 0.05f};
    float bias_{-0.10f};
    std::uint64_t trainingEvents_{};
    std::uint64_t styleTrainingEvents_{};
    float fadePreference_{0.58f};
    float zoomPreference_{1.0f};
    float driftPreference_{1.0f};
    float lyricsPreference_{1.0f};
    std::array<float, ImageMatchFeatureCount> imageWeights_{
        0.20f, 0.18f, 0.14f, 0.08f, 0.22f, 0.10f,
        0.12f, 0.18f, 0.10f, 0.08f, 0.05f, 0.06f
    };
    float imageBias_{};
    std::uint64_t imageTrainingEvents_{};
};

[[nodiscard]] VisualTimeline BuildTatarusVisualTimeline(const std::vector<VisualImageProfile>& images,
                                                        double duration,
                                                        const AudioAnalysis& analysis,
                                                        const TatarusVisualBrain& brain,
                                                        double crossFadeSeconds = 0.65);

} // namespace kg
