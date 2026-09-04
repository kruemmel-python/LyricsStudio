#pragma once

#include <windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace kg {

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

inline std::wstring Utf8ToWide(std::string_view text) {
    if (text.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (count <= 0) return {};
    std::wstring out(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        text.data(), static_cast<int>(text.size()), out.data(), count);
    return out;
}

inline std::string WideToUtf8(std::wstring_view text) {
    if (text.empty()) return {};
    const int count = WideCharToMultiByte(CP_UTF8, 0,
        text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) return {};
    std::string out(static_cast<size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0,
        text.data(), static_cast<int>(text.size()), out.data(), count, nullptr, nullptr);
    return out;
}

inline std::string ReadUtf8File(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Datei kann nicht gelesen werden: " + path.string());
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

inline void AtomicWriteUtf8(const fs::path& path, std::string_view content) {
    fs::create_directories(path.parent_path());
    auto temp = path;
    temp += L".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("Temporäre Datei kann nicht geschrieben werden.");
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
    }
    std::error_code ec;
    fs::rename(temp, path, ec);
    if (ec) {
        fs::remove(path, ec);
        ec.clear();
        fs::rename(temp, path, ec);
        if (ec) throw std::runtime_error("Datei konnte nicht atomar ersetzt werden.");
    }
}

inline std::wstring FormatTime(double seconds, bool millis = false) {
    seconds = std::max(0.0, seconds);
    const auto totalMs = static_cast<long long>(seconds * 1000.0 + 0.5);
    const long long hours = totalMs / 3'600'000;
    const long long minutes = (totalMs / 60'000) % 60;
    const long long secs = (totalMs / 1000) % 60;
    const long long ms = totalMs % 1000;
    std::wostringstream ss;
    ss << std::setfill(L'0');
    if (hours > 0) ss << std::setw(2) << hours << L":";
    ss << std::setw(2) << minutes << L":" << std::setw(2) << secs;
    if (millis) ss << L"." << std::setw(3) << ms;
    return ss.str();
}

struct FRect {
    float l{}, t{}, r{}, b{};
    [[nodiscard]] bool Contains(float x, float y) const noexcept {
        return x >= l && x <= r && y >= t && y <= b;
    }
    [[nodiscard]] D2D1_RECT_F D2D() const noexcept { return D2D1::RectF(l, t, r, b); }
};

inline bool IsAudioExtension(const fs::path& p) {
    auto e = p.extension().wstring();
    std::transform(e.begin(), e.end(), e.begin(), ::towlower);
    static constexpr std::wstring_view exts[] = {
        L".mp3", L".wav", L".flac", L".m4a", L".aac", L".ogg",
        L".opus", L".wma", L".mp4", L".mkv", L".webm"
    };
    return std::find(std::begin(exts), std::end(exts), e) != std::end(exts);
}

inline std::wstring ExeDirectory() {
    std::wstring buffer(32768, L'\0');
    const DWORD n = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    buffer.resize(n);
    return fs::path(buffer).parent_path().wstring();
}

inline float Clamp01(float v) noexcept { return std::clamp(v, 0.0f, 1.0f); }

} // namespace kg
