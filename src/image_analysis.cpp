#include "image_analysis.hpp"

#include <wincodec.h>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace kg {

bool ImageAnalyzer::Analyze(const fs::path& path, VisualImageProfile& out, std::wstring& error) const {
    error.clear();
    out = {};
    out.path = path;
    if (path.empty() || !fs::exists(path)) {
        error = L"Bild nicht gefunden: " + path.wstring();
        return false;
    }

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(factory.GetAddressOf()));
    if (FAILED(hr)) { error = L"WIC Imaging Factory konnte nicht erstellt werden."; return false; }

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                            WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf());
    if (FAILED(hr)) { error = L"Bild konnte nicht dekodiert werden: " + path.filename().wstring(); return false; }

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr)) { error = L"Bildframe konnte nicht gelesen werden."; return false; }

    UINT sourceW = 0, sourceH = 0;
    frame->GetSize(&sourceW, &sourceH);
    if (sourceW == 0 || sourceH == 0) { error = L"Bild hat ungueltige Abmessungen."; return false; }

    constexpr UINT maxSide = 128;
    const double scale = std::min(1.0, static_cast<double>(maxSide) / static_cast<double>(std::max(sourceW, sourceH)));
    const UINT w = std::max<UINT>(1, static_cast<UINT>(std::lround(sourceW * scale)));
    const UINT h = std::max<UINT>(1, static_cast<UINT>(std::lround(sourceH * scale)));

    ComPtr<IWICBitmapSource> source = frame;
    ComPtr<IWICBitmapScaler> scaler;
    if (w != sourceW || h != sourceH) {
        hr = factory->CreateBitmapScaler(scaler.GetAddressOf());
        if (FAILED(hr) || FAILED(scaler->Initialize(frame.Get(), w, h, WICBitmapInterpolationModeFant))) {
            error = L"Bild konnte fuer die Analyse nicht skaliert werden.";
            return false;
        }
        source = scaler;
    }

    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(converter.GetAddressOf());
    if (FAILED(hr) || FAILED(converter->Initialize(source.Get(), GUID_WICPixelFormat32bppRGBA,
                                                    WICBitmapDitherTypeNone, nullptr, 0.0,
                                                    WICBitmapPaletteTypeCustom))) {
        error = L"Bild konnte nicht in Analyseformat konvertiert werden.";
        return false;
    }

    const UINT stride = w * 4;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(stride) * h);
    hr = converter->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data());
    if (FAILED(hr)) { error = L"Bildpixel konnten nicht gelesen werden."; return false; }

    const std::size_t count = static_cast<std::size_t>(w) * h;
    std::vector<float> luminance(count);
    double sumLum = 0.0, sumLumSq = 0.0, sumSat = 0.0, sumWarm = 0.0;

    for (std::size_t i = 0; i < count; ++i) {
        const float r = pixels[i * 4 + 0] / 255.0f;
        const float g = pixels[i * 4 + 1] / 255.0f;
        const float b = pixels[i * 4 + 2] / 255.0f;
        const float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        luminance[i] = lum;
        sumLum += lum;
        sumLumSq += static_cast<double>(lum) * lum;
        const float mx = std::max({r, g, b});
        const float mn = std::min({r, g, b});
        sumSat += mx > 0.001f ? (mx - mn) / mx : 0.0f;
        sumWarm += 0.5f + 0.5f * (r - b);
    }

    const float brightness = static_cast<float>(sumLum / count);
    const double variance = std::max(0.0, sumLumSq / count - static_cast<double>(brightness) * brightness);
    const float contrast = Clamp01(static_cast<float>(std::sqrt(variance) * 3.2));
    const float saturation = Clamp01(static_cast<float>(sumSat / count));
    const float warmth = Clamp01(static_cast<float>(sumWarm / count));

    double edgeSum = 0.0;
    std::size_t edgeCount = 0;
    for (UINT y = 0; y + 1 < h; ++y) {
        for (UINT x = 0; x + 1 < w; ++x) {
            const std::size_t i = static_cast<std::size_t>(y) * w + x;
            const float dx = std::abs(luminance[i] - luminance[i + 1]);
            const float dy = std::abs(luminance[i] - luminance[i + w]);
            edgeSum += std::min(1.0f, (dx + dy) * 2.4f);
            ++edgeCount;
        }
    }
    const float edgeDensity = edgeCount ? Clamp01(static_cast<float>(edgeSum / edgeCount)) : 0.0f;
    const float aspect = static_cast<float>(sourceW) / static_cast<float>(sourceH);
    const float landscape = aspect > 1.08f ? 1.0f : aspect < 0.92f ? 0.0f : 0.5f;

    out.features = {
        Clamp01(brightness), contrast, saturation, warmth, edgeDensity, landscape,
        Clamp01(1.0f - brightness), Clamp01(0.58f * saturation + 0.42f * contrast)
    };
    return true;
}

bool ImageAnalyzer::Analyze(const std::vector<fs::path>& paths, std::vector<VisualImageProfile>& out,
                            std::wstring& error) const {
    out.clear();
    error.clear();
    out.reserve(paths.size());
    for (const auto& path : paths) {
        VisualImageProfile profile;
        std::wstring localError;
        if (!Analyze(path, profile, localError)) {
            error = localError;
            out.clear();
            return false;
        }
        out.push_back(std::move(profile));
    }
    return true;
}

} // namespace kg
