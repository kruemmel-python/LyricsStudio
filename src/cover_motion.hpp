#pragma once

#include "video_export_types.hpp"

namespace kg {

struct CoverTransform {
    float scale{1.0f};
    float offsetX{};
    float offsetY{};
};

class CoverMotion {
public:
    [[nodiscard]] CoverTransform Evaluate(double seconds, const VideoExportPreset& preset) const noexcept;
};

} // namespace kg
