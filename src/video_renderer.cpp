#include "video_renderer.hpp"

#include <cstring>

namespace kg {

bool VideoRenderer::Initialize(std::uint32_t width, std::uint32_t height, std::wstring& error) {
    if (width == 0 || height == 0 || width > 7680 || height > 4320) {
        error = L"Ungültige Videoauflösung.";
        return false;
    }
    width_ = width;
    height_ = height;
    cover_ = {};
    timeline_ = {};
    timelineImages_.clear();
    solidBrush_.Reset();
    renderTarget_.Reset();
    targetBitmap_.Reset();

    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(wic_.ReleaseAndGetAddressOf()));
    if (FAILED(hr)) { error = L"Windows Imaging Component konnte nicht gestartet werden."; return false; }
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory_.ReleaseAndGetAddressOf());
    if (FAILED(hr)) { error = L"Direct2D konnte für den Video-Renderer nicht gestartet werden."; return false; }
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                             reinterpret_cast<IUnknown**>(writeFactory_.ReleaseAndGetAddressOf()));
    if (FAILED(hr)) { error = L"DirectWrite konnte für den Video-Renderer nicht gestartet werden."; return false; }
    hr = wic_->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad,
                            targetBitmap_.GetAddressOf());
    if (FAILED(hr)) { error = L"Offscreen-Videobitmap konnte nicht erstellt werden."; return false; }
    const auto properties = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f, D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT);
    hr = d2dFactory_->CreateWicBitmapRenderTarget(targetBitmap_.Get(), properties, renderTarget_.GetAddressOf());
    if (FAILED(hr)) { error = L"Direct2D-Offscreen-Renderziel konnte nicht erstellt werden."; return false; }
    renderTarget_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    hr = renderTarget_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), solidBrush_.GetAddressOf());
    if (FAILED(hr)) { error = L"Renderpinsel konnte nicht erstellt werden."; return false; }
    return true;
}

bool VideoRenderer::LoadCover(const fs::path& cover, std::wstring& error) {
    timeline_ = {};
    timelineImages_.clear();
    return cover_.Load(wic_.Get(), renderTarget_.Get(), cover, error);
}

bool VideoRenderer::LoadTimeline(const VisualTimeline& timeline, std::wstring& error) {
    if (timeline.clips.empty()) { error = L"Die visuelle Timeline enthält keine Bilder."; return false; }
    timeline_ = timeline;
    timelineImages_.clear();
    timelineImages_.reserve(timeline.clips.size());
    for (const auto& clip : timeline.clips) {
        CoverImage image;
        if (!image.Load(wic_.Get(), renderTarget_.Get(), clip.imagePath, error)) {
            error = L"Bild konnte nicht geladen werden: " + clip.imagePath.filename().wstring() + L" — " + error;
            timelineImages_.clear();
            timeline_ = {};
            return false;
        }
        timelineImages_.push_back(std::move(image));
    }
    return true;
}

void VideoRenderer::DrawVisual(const CoverImage& image, double localSeconds, float opacity,
                               const VideoExportPreset& preset, const VisualStyle& style) {
    if (!image.Bitmap() || opacity <= 0.001f) return;
    const auto size = image.Size();
    auto styledPreset = preset;
    styledPreset.zoomMax = 1.0f + (preset.zoomMax - 1.0f) * style.zoomGain;
    styledPreset.zoomMin = 1.0f + (preset.zoomMin - 1.0f) * style.zoomGain;
    styledPreset.driftPixelsX = preset.driftPixelsX * style.driftGain;
    styledPreset.driftPixelsY = preset.driftPixelsY * style.driftGain;
    const auto transform = motion_.Evaluate(localSeconds, styledPreset);
    const float baseScale = std::max(static_cast<float>(width_) / size.width,
                                     static_cast<float>(height_) / size.height);
    const float scale = baseScale * transform.scale;
    const float renderWidth = size.width * scale;
    const float renderHeight = size.height * scale;
    const float left = (static_cast<float>(width_) - renderWidth) * 0.5f + transform.offsetX;
    const float top = (static_cast<float>(height_) - renderHeight) * 0.5f + transform.offsetY;
    renderTarget_->DrawBitmap(image.Bitmap(), D2D1::RectF(left, top, left + renderWidth, top + renderHeight),
                              Clamp01(opacity), D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
}

bool VideoRenderer::DrawScene(double seconds, const std::vector<LyricSegment>& lyrics,
                              const VideoExportPreset& preset, std::wstring& error) {
    if (!renderTarget_ || (timeline_.Empty() && !cover_.Bitmap())) {
        error = L"Video-Renderer oder visuelle Quelle ist nicht bereit.";
        return false;
    }

    const D2D1_RECT_F full = D2D1::RectF(0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_));
    renderTarget_->BeginDraw();
    renderTarget_->SetTransform(D2D1::Matrix3x2F::Identity());
    renderTarget_->Clear(D2D1::ColorF(D2D1::ColorF::Black));

    if (!timeline_.Empty() && timelineImages_.size() == timeline_.clips.size()) {
        std::size_t index = timeline_.clips.size() - 1;
        for (std::size_t i = 0; i < timeline_.clips.size(); ++i) {
            if (seconds < timeline_.clips[i].end || i + 1 == timeline_.clips.size()) { index = i; break; }
        }
        const auto& clip = timeline_.clips[index];
        const double localSeconds = std::max(0.0, seconds - clip.start);
        float currentOpacity = 1.0f;
        float nextOpacity = 0.0f;
        if (clip.transition == VisualTransition::CrossFade && index + 1 < timeline_.clips.size() && clip.transitionSeconds > 0.0) {
            const double fadeStart = std::max(clip.start, clip.end - clip.transitionSeconds);
            if (seconds >= fadeStart) {
                nextOpacity = Clamp01(static_cast<float>((seconds - fadeStart) / clip.transitionSeconds));
                currentOpacity = 1.0f - nextOpacity;
            }
        }
        DrawVisual(timelineImages_[index], localSeconds, currentOpacity, preset, clip.style);
        if (nextOpacity > 0.001f && index + 1 < timelineImages_.size())
            DrawVisual(timelineImages_[index + 1], std::max(0.0, seconds - timeline_.clips[index + 1].start), nextOpacity, preset, timeline_.clips[index + 1].style);
    } else {
        DrawVisual(cover_, seconds, 1.0f, preset);
    }

    solidBrush_->SetColor(D2D1::ColorF(0.0f, 0.0f, 0.0f, Clamp01(preset.coverDarkening)));
    renderTarget_->FillRectangle(full, solidBrush_.Get());

    if (preset.vignetteStrength > 0.001f) {
        const D2D1_GRADIENT_STOP stops[] = {
            {0.0f, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f)},
            {0.68f, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f)},
            {1.0f, D2D1::ColorF(0.0f, 0.0f, 0.0f, Clamp01(preset.vignetteStrength))}
        };
        ComPtr<ID2D1GradientStopCollection> stopCollection;
        ComPtr<ID2D1RadialGradientBrush> vignette;
        HRESULT hr = renderTarget_->CreateGradientStopCollection(stops, ARRAYSIZE(stops), D2D1_GAMMA_2_2,
            D2D1_EXTEND_MODE_CLAMP, stopCollection.GetAddressOf());
        if (SUCCEEDED(hr)) hr = renderTarget_->CreateRadialGradientBrush(
            D2D1::RadialGradientBrushProperties(D2D1::Point2F(width_ * 0.5f, height_ * 0.5f), D2D1::Point2F(),
                                                width_ * 0.72f, height_ * 0.72f),
            stopCollection.Get(), vignette.GetAddressOf());
        if (SUCCEEDED(hr)) renderTarget_->FillRectangle(full, vignette.Get());
    }

    auto lyricPreset = preset;
    if (!timeline_.Empty()) {
        if (const auto* active = timeline_.ClipAt(seconds)) {
            lyricPreset.lyricsScale = active->style.lyricsScale;
            lyricPreset.fadeDuration = std::clamp(preset.fadeDuration / std::max(0.80f, active->style.lyricsScale), 0.08f, 0.40f);
        }
    }
    if (!lyricsCompositor_.Draw(renderTarget_.Get(), writeFactory_.Get(), lyrics, seconds, lyricPreset, error)) {
        renderTarget_->EndDraw();
        return false;
    }
    const HRESULT hr = renderTarget_->EndDraw();
    if (FAILED(hr)) { error = L"Ein Videoframe konnte nicht gerendert werden."; return false; }
    return true;
}

bool VideoRenderer::RenderFrame(double seconds, const std::vector<LyricSegment>& lyrics,
                                const VideoExportPreset& preset, std::vector<std::byte>& destination,
                                std::wstring& error) {
    if (!DrawScene(seconds, lyrics, preset, error)) return false;
    const std::size_t stride = static_cast<std::size_t>(width_) * 4;
    const std::size_t required = stride * height_;
    destination.resize(required);
    WICRect rect{0, 0, static_cast<INT>(width_), static_cast<INT>(height_)};
    const HRESULT hr = targetBitmap_->CopyPixels(&rect, static_cast<UINT>(stride), static_cast<UINT>(required),
                                                  reinterpret_cast<BYTE*>(destination.data()));
    if (FAILED(hr)) { error = L"Gerenderte BGRA-Pixel konnten nicht ausgelesen werden."; return false; }
    return true;
}

} // namespace kg
