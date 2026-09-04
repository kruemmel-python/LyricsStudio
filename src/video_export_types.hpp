#pragma once

#include "common.hpp"
#include "visual_timeline.hpp"

#include <cstdint>

namespace kg {

enum class VideoContainer { Mp4, WebM };
enum class LyricsLayout { Centered, LowerThird, Subtitle };
enum class MotionMode { None, SlowZoom, Atmospheric };

struct VideoSize {
    std::uint32_t width{1920};
    std::uint32_t height{1080};
};

struct VideoExportPreset {
    std::wstring name{L"Klanggeist Lyrics Video"};
    VideoSize size{1920, 1080};
    std::uint32_t fps{30};
    VideoContainer container{VideoContainer::Mp4};
    LyricsLayout lyricsLayout{LyricsLayout::Centered};
    MotionMode motion{MotionMode::Atmospheric};
    std::uint32_t videoBitrate{12'000'000};
    std::uint32_t audioBitrate{192'000};
    float coverDarkening{0.28f};
    float vignetteStrength{0.15f};
    float zoomMin{1.000f};
    float zoomMax{1.035f};
    float driftPixelsX{16.0f};
    float driftPixelsY{9.0f};
    float fadeDuration{0.20f};
    bool showPreviousLine{true};
    bool showNextLine{true};
    bool highlightCurrentLine{true};
    float lyricsScale{1.0f};
};

struct VideoExportJob {
    fs::path audioPath;
    fs::path lyricsJsonPath;
    fs::path coverPath; // Legacy/single-image fallback.
    VisualTimeline visualTimeline;
    fs::path outputPath;
    VideoExportPreset preset;
};

inline VideoExportPreset KlanggeistDefaultPreset() { return {}; }

} // namespace kg
