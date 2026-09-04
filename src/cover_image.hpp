#pragma once

#include "common.hpp"

#include <wincodec.h>

namespace kg {

class CoverImage {
public:
    bool Load(IWICImagingFactory* wicFactory, ID2D1RenderTarget* renderTarget,
              const fs::path& path, std::wstring& error);

    [[nodiscard]] ID2D1Bitmap* Bitmap() const noexcept { return bitmap_.Get(); }
    [[nodiscard]] D2D1_SIZE_F Size() const noexcept { return size_; }

private:
    ComPtr<ID2D1Bitmap> bitmap_;
    D2D1_SIZE_F size_{};
};

} // namespace kg
