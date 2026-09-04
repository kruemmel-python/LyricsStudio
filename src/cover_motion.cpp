#include "cover_motion.hpp"

#include <cmath>

namespace kg {

CoverTransform CoverMotion::Evaluate(double seconds, const VideoExportPreset& preset) const noexcept {
    if (preset.motion == MotionMode::None) return {};

    const float zoomPhase = 0.5f + 0.5f * std::sin(static_cast<float>(seconds * 0.0981747704 - 1.5707963268));
    const float smoothZoom = zoomPhase * zoomPhase * (3.0f - 2.0f * zoomPhase);

    CoverTransform result;
    result.scale = std::lerp(preset.zoomMin, preset.zoomMax, smoothZoom);
    if (preset.motion == MotionMode::SlowZoom) return result;

    result.offsetX = std::sin(static_cast<float>(seconds * 0.071)) * preset.driftPixelsX;
    result.offsetY = std::sin(static_cast<float>(seconds * 0.053 + 1.7)) * preset.driftPixelsY;
    return result;
}

} // namespace kg
