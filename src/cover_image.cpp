#include "cover_image.hpp"

namespace kg {

bool CoverImage::Load(IWICImagingFactory* wicFactory, ID2D1RenderTarget* renderTarget,
                      const fs::path& path, std::wstring& error) {
    bitmap_.Reset();
    size_ = {};
    if (!wicFactory || !renderTarget || path.empty() || !fs::exists(path)) {
        error = L"Cover-Datei wurde nicht gefunden.";
        return false;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = wicFactory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf());
    if (FAILED(hr)) {
        error = L"Das Cover kann von Windows Imaging Component nicht geöffnet werden.";
        return false;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr)) {
        error = L"Das erste Bild des Covers kann nicht gelesen werden.";
        return false;
    }

    ComPtr<IWICFormatConverter> converter;
    hr = wicFactory->CreateFormatConverter(converter.GetAddressOf());
    if (SUCCEEDED(hr)) {
        hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeMedianCut);
    }
    if (FAILED(hr)) {
        error = L"Das Cover konnte nicht in das BGRA-Renderformat konvertiert werden.";
        return false;
    }

    hr = renderTarget->CreateBitmapFromWicBitmap(converter.Get(), nullptr, bitmap_.GetAddressOf());
    if (FAILED(hr) || !bitmap_) {
        error = L"Das Direct2D-Coverbitmap konnte nicht erzeugt werden.";
        return false;
    }
    size_ = bitmap_->GetSize();
    return size_.width > 0.0f && size_.height > 0.0f;
}

} // namespace kg
