#pragma once

#include "cover_image.hpp"
#include "cover_motion.hpp"
#include "lyrics_compositor.hpp"
#include "visual_timeline.hpp"

#include <span>
#include <vector>

namespace kg {

class VideoRenderer {
public:
    bool Initialize(std::uint32_t width, std::uint32_t height, std::wstring& error);
    bool LoadCover(const fs::path& cover, std::wstring& error);
    bool LoadTimeline(const VisualTimeline& timeline, std::wstring& error);
    bool RenderFrame(double seconds, const std::vector<LyricSegment>& lyrics,
                     const VideoExportPreset& preset, std::vector<std::byte>& destination,
                     std::wstring& error);

    [[nodiscard]] std::uint32_t Width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t Height() const noexcept { return height_; }

private:
    bool DrawScene(double seconds, const std::vector<LyricSegment>& lyrics,
                   const VideoExportPreset& preset, std::wstring& error);
    void DrawVisual(const CoverImage& image, double localSeconds, float opacity,
                    const VideoExportPreset& preset, const VisualStyle& style = {});

    std::uint32_t width_{};
    std::uint32_t height_{};
    ComPtr<IWICImagingFactory> wic_;
    ComPtr<IWICBitmap> targetBitmap_;
    ComPtr<ID2D1Factory> d2dFactory_;
    ComPtr<ID2D1RenderTarget> renderTarget_;
    ComPtr<IDWriteFactory> writeFactory_;
    ComPtr<ID2D1SolidColorBrush> solidBrush_;
    CoverImage cover_;
    VisualTimeline timeline_;
    std::vector<CoverImage> timelineImages_;
    CoverMotion motion_;
    LyricsCompositor lyricsCompositor_;
};

} // namespace kg
