#include "lyrics_compositor.hpp"

#include <cmath>

namespace kg {

std::optional<std::size_t> LyricsTimeline::ActiveIndex(double seconds) const noexcept {
    if (!segments_ || segments_->empty()) return std::nullopt;
    const auto it = std::upper_bound(segments_->begin(), segments_->end(), seconds,
        [](double time, const LyricSegment& segment) { return time < segment.start; });
    if (it == segments_->begin()) return std::nullopt;
    const auto candidate = std::prev(it);
    if (seconds < candidate->start || seconds > candidate->end) return std::nullopt;
    return static_cast<std::size_t>(std::distance(segments_->begin(), candidate));
}

const LyricSegment* LyricsTimeline::Active(double seconds) const noexcept {
    const auto index = ActiveIndex(seconds);
    return index ? &(*segments_)[*index] : nullptr;
}

namespace {

float SegmentOpacity(const LyricSegment& segment, double seconds, double fadeDuration) {
    if (seconds < segment.start || seconds > segment.end) return 0.0f;
    if (fadeDuration <= 0.0) return 1.0f;
    const double alpha = std::min({1.0, (seconds - segment.start) / fadeDuration,
                                  (segment.end - seconds) / fadeDuration});
    return static_cast<float>(std::clamp(alpha, 0.0, 1.0));
}

bool CreateFittedLayout(IDWriteFactory* factory, const std::wstring& text, float maxWidth,
                        float maxHeight, float preferredSize, float minimumSize,
                        DWRITE_FONT_WEIGHT weight, ComPtr<IDWriteTextLayout>& layout,
                        DWRITE_TEXT_METRICS& metrics) {
    for (float size = preferredSize; size >= minimumSize; size -= 2.0f) {
        ComPtr<IDWriteTextFormat> format;
        HRESULT hr = factory->CreateTextFormat(L"Segoe UI", nullptr, weight,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"de-de",
            format.GetAddressOf());
        if (FAILED(hr)) return false;
        format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);

        ComPtr<IDWriteTextLayout> candidate;
        hr = factory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()),
            format.Get(), maxWidth, maxHeight, candidate.GetAddressOf());
        if (FAILED(hr)) return false;
        DWRITE_TEXT_METRICS candidateMetrics{};
        if (FAILED(candidate->GetMetrics(&candidateMetrics))) return false;
        if (candidateMetrics.height <= maxHeight && candidateMetrics.width <= maxWidth + 0.5f) {
            layout = std::move(candidate);
            metrics = candidateMetrics;
            return true;
        }
    }
    return false;
}

} // namespace

bool LyricsCompositor::Draw(ID2D1RenderTarget* target, IDWriteFactory* writeFactory,
                            const std::vector<LyricSegment>& segments, double seconds,
                            const VideoExportPreset& preset, std::wstring& error) const {
    if (!target || !writeFactory || segments.empty()) return true;
    LyricsTimeline timeline(segments);
    const auto activeIndex = timeline.ActiveIndex(seconds);
    if (!activeIndex) return true;

    const auto targetSize = target->GetSize();
    const float scale = targetSize.height / 1080.0f;
    const float lyricsScale = std::clamp(preset.lyricsScale, 0.75f, 1.45f);
    const float maxWidth = targetSize.width * 0.82f;
    const float currentMaxHeight = targetSize.height * 0.22f;
    float centerY = targetSize.height * 0.50f;
    if (preset.lyricsLayout == LyricsLayout::LowerThird) centerY = targetSize.height * 0.69f;
    if (preset.lyricsLayout == LyricsLayout::Subtitle) centerY = targetSize.height * 0.82f;

    ComPtr<ID2D1SolidColorBrush> brush;
    if (FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), brush.GetAddressOf()))) {
        error = L"Textpinsel für den Video-Renderer konnte nicht erstellt werden.";
        return false;
    }

    auto drawLine = [&](const LyricSegment& segment, float y, float preferredSize,
                        float minimumSize, float alpha, DWRITE_FONT_WEIGHT weight,
                        bool current) -> bool {
        if (segment.text.empty() || alpha <= 0.001f) return true;
        ComPtr<IDWriteTextLayout> layout;
        DWRITE_TEXT_METRICS metrics{};
        if (!CreateFittedLayout(writeFactory, segment.text, maxWidth, currentMaxHeight,
                                preferredSize * scale * lyricsScale, minimumSize * scale * lyricsScale, weight,
                                layout, metrics)) {
            error = L"Lyrics-Textlayout konnte nicht erstellt werden.";
            return false;
        }
        const D2D1_POINT_2F origin{(targetSize.width - maxWidth) * 0.5f,
                                   y - metrics.height * 0.5f};
        brush->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, alpha * 0.72f));
        target->DrawTextLayout(D2D1::Point2F(origin.x, origin.y + 4.0f * scale),
                               layout.Get(), brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        if (current && preset.highlightCurrentLine) {
            brush->SetColor(D2D1::ColorF(0.24f, 0.88f, 1.0f, alpha));
        } else {
            brush->SetColor(D2D1::ColorF(0.94f, 0.96f, 1.0f, alpha));
        }
        target->DrawTextLayout(origin, layout.Get(), brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        return true;
    };

    const std::size_t index = *activeIndex;
    const float currentAlpha = SegmentOpacity(segments[index], seconds, preset.fadeDuration);
    const float neighborDistance = 128.0f * scale * lyricsScale;
    if (preset.showPreviousLine && index > 0 &&
        !drawLine(segments[index - 1], centerY - neighborDistance, 42.0f, 30.0f,
                  0.30f * std::max(0.35f, currentAlpha), DWRITE_FONT_WEIGHT_NORMAL, false)) return false;
    if (!drawLine(segments[index], centerY, 64.0f, 38.0f, currentAlpha,
                  DWRITE_FONT_WEIGHT_SEMI_BOLD, true)) return false;
    if (preset.showNextLine && index + 1 < segments.size() &&
        !drawLine(segments[index + 1], centerY + neighborDistance, 42.0f, 30.0f,
                  0.22f * std::max(0.35f, currentAlpha), DWRITE_FONT_WEIGHT_NORMAL, false)) return false;
    return true;
}

} // namespace kg
