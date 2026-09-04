#pragma once

#include "lyrics_document.hpp"
#include "video_export_types.hpp"

#include <dwrite.h>
#include <optional>

namespace kg {

class LyricsTimeline {
public:
    explicit LyricsTimeline(const std::vector<LyricSegment>& segments) : segments_(&segments) {}

    [[nodiscard]] std::optional<std::size_t> ActiveIndex(double seconds) const noexcept;
    [[nodiscard]] const LyricSegment* Active(double seconds) const noexcept;

private:
    const std::vector<LyricSegment>* segments_{};
};

class LyricsCompositor {
public:
    bool Draw(ID2D1RenderTarget* target, IDWriteFactory* writeFactory,
              const std::vector<LyricSegment>& segments, double seconds,
              const VideoExportPreset& preset, std::wstring& error) const;
};

} // namespace kg
