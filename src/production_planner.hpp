#pragma once

#include "image_analysis.hpp"
#include "song_structure.hpp"
#include "tatarus_visual_brain.hpp"
#include "visual_timeline.hpp"

namespace kg {

struct ProductionPlanOptions {
    fs::path albumCoverPath;
    bool requireAllImages{true};
    std::size_t reuseCooldown{4};
    float reusePenalty{0.16f};
};

[[nodiscard]] VisualTimeline BuildTatarusProductionTimeline(
    const std::vector<VisualImageProfile>& images,
    double duration,
    const AudioAnalysis& audio,
    const SongStructure& structure,
    const TatarusVisualBrain& brain,
    const ProductionPlanOptions& options = {},
    double crossFadeSeconds = 0.65);

} // namespace kg
