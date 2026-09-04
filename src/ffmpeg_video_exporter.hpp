#pragma once

#include "lyrics_document.hpp"
#include "video_export_types.hpp"

#include <atomic>
#include <functional>

namespace kg {

class FfmpegVideoExporter {
public:
    using ProgressCallback = std::function<void(double, std::wstring)>;

    [[nodiscard]] static fs::path FindExecutable(const fs::path& appDirectory);

    bool Export(const fs::path& executable, const VideoExportJob& job,
                const LyricsDocument& document, std::atomic_bool& cancelRequested,
                ProgressCallback progress, std::wstring& error);
};

} // namespace kg
