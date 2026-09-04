#pragma once

#include "video_export_types.hpp"

#include <atomic>
#include <thread>

namespace kg {

constexpr UINT WM_KG_EXPORT_EVENT = WM_APP + 91;

enum class ExportState { Idle, Preparing, Rendering, Finalizing, Done, Cancelled, Error };

struct ExportProgress {
    ExportState state{ExportState::Idle};
    double progress{};
    std::wstring message;
    fs::path outputPath;
};

class VideoExportController {
public:
    explicit VideoExportController(HWND notifyWindow) : notifyWindow_(notifyWindow) {}
    ~VideoExportController();
    VideoExportController(const VideoExportController&) = delete;
    VideoExportController& operator=(const VideoExportController&) = delete;

    bool Start(VideoExportJob job, std::wstring& error);
    void Cancel() noexcept;
    [[nodiscard]] bool Running() const noexcept { return running_; }

private:
    void WorkerMain(VideoExportJob job);
    void Post(ExportState state, double progress, std::wstring message,
              const fs::path& output = {}) const;

    HWND notifyWindow_{};
    std::jthread worker_;
    std::atomic_bool running_{};
    std::atomic_bool cancelRequested_{};
};

} // namespace kg
