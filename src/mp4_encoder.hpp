#pragma once

#include "video_export_types.hpp"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <span>

namespace kg {

class Mp4Encoder {
public:
    bool Open(const fs::path& output, const fs::path& audio, std::uint32_t width,
              std::uint32_t height, std::uint32_t fps, std::uint32_t videoBitrate,
              std::uint32_t audioBitrate, std::wstring& error);
    bool WriteVideoFrame(std::span<const std::byte> bgra, std::int64_t timestamp100ns,
                         std::int64_t duration100ns, std::wstring& error);
    bool Finalize(std::wstring& error);
    void Cancel() noexcept;

private:
    bool WriteAudioUntil(std::int64_t target100ns, std::wstring& error);

    ComPtr<IMFSinkWriter> writer_;
    ComPtr<IMFSourceReader> audioReader_;
    DWORD videoStream_{};
    DWORD audioStream_{};
    std::uint32_t width_{};
    std::uint32_t height_{};
    std::uint32_t audioBytesPerSecond_{};
    bool audioEos_{};
    std::int64_t audioNextTime_{};
    std::int64_t videoEndTime_{};
};

} // namespace kg
