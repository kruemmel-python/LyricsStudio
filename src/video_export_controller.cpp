#include "video_export_controller.hpp"

#include "ffmpeg_video_exporter.hpp"
#include "lyrics_document.hpp"
#include "mp4_encoder.hpp"
#include "video_renderer.hpp"

#include <mfapi.h>

#include <cmath>

namespace kg {

VideoExportController::~VideoExportController() {
    cancelRequested_ = true;
    if (worker_.joinable()) worker_.join();
}

bool VideoExportController::Start(VideoExportJob job, std::wstring& error) {
    if (running_) {
        error = L"Es läuft bereits ein Videoexport.";
        return false;
    }
    if (worker_.joinable()) worker_.join();
    if (job.preset.container != VideoContainer::Mp4) {
        error = L"WebM ist für v1.4 vorgesehen. Bitte MP4 verwenden.";
        return false;
    }
    const bool hasVisualTimeline = !job.visualTimeline.Empty();
    const bool hasLegacyCover = !job.coverPath.empty() && fs::exists(job.coverPath);
    if (!fs::exists(job.audioPath) || !fs::exists(job.lyricsJsonPath) || (!hasVisualTimeline && !hasLegacyCover)) {
        error = L"Audio, Lyrics-JSON oder Bildquelle fehlt.";
        return false;
    }
    if (hasVisualTimeline) {
        for (const auto& clip : job.visualTimeline.clips) {
            if (clip.imagePath.empty() || !fs::exists(clip.imagePath)) {
                error = L"Ein Bild der visuellen Timeline fehlt: " + clip.imagePath.wstring();
                return false;
            }
        }
    }
    if (job.outputPath.empty()) {
        error = L"Kein Video-Ausgabepfad festgelegt.";
        return false;
    }
    cancelRequested_ = false;
    running_ = true;
    worker_ = std::jthread(&VideoExportController::WorkerMain, this, std::move(job));
    return true;
}

void VideoExportController::Cancel() noexcept {
    cancelRequested_ = true;
}

void VideoExportController::Post(ExportState state, double progress, std::wstring message,
                                 const fs::path& output) const {
    auto* event = new ExportProgress{state, progress, std::move(message), output};
    if (!PostMessageW(notifyWindow_, WM_KG_EXPORT_EVENT, 0, reinterpret_cast<LPARAM>(event))) delete event;
}

void VideoExportController::WorkerMain(VideoExportJob job) {
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool comInitialized = SUCCEEDED(comResult);
    const HRESULT mfResult = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    const bool mfStarted = SUCCEEDED(mfResult);

    fs::path temporary = job.outputPath.parent_path() /
        (job.outputPath.stem().wstring() + L".tmp" + job.outputPath.extension().wstring());
    auto finish = [&] {
        running_ = false;
        if (mfStarted) MFShutdown();
        if (comInitialized) CoUninitialize();
    };
    auto fail = [&](std::wstring message) {
        std::error_code ec;
        fs::remove(temporary, ec);
        Post(ExportState::Error, 0.0, std::move(message), job.outputPath);
        finish();
    };
    auto cancelled = [&] {
        std::error_code ec;
        fs::remove(temporary, ec);
        Post(ExportState::Cancelled, 0.0, L"Videoexport abgebrochen.", job.outputPath);
        finish();
    };

    if (!comInitialized || !mfStarted) {
        fail(L"COM oder Media Foundation konnte im Export-Thread nicht gestartet werden.");
        return;
    }

    Post(ExportState::Preparing, 0.0, L"Cover, Lyrics und Encoder werden vorbereitet …", job.outputPath);
    std::error_code ec;
    fs::create_directories(job.outputPath.parent_path(), ec);
    if (ec) {
        fail(L"Video-Ausgabeordner konnte nicht erstellt werden.");
        return;
    }
    fs::remove(temporary, ec);

    LyricsDocument document;
    std::wstring error;
    Post(ExportState::Preparing, 0.0, L"Lyrics-Timeline wird geladen …", job.outputPath);
    if (!document.Load(job.lyricsJsonPath, job.audioPath.parent_path(), error)) {
        fail(L"Lyrics konnten nicht geladen werden: " + error);
        return;
    }
    if (document.Duration() <= 0.0) {
        fail(L"Die Lyrics-Datei enthält keine gültige Songdauer.");
        return;
    }

    const fs::path ffmpeg = FfmpegVideoExporter::FindExecutable(ExeDirectory());
    if (!ffmpeg.empty()) {
        Post(ExportState::Preparing, 0.0, L"FAST FFmpeg-Timeline-Renderer wird gestartet …", job.outputPath);
        VideoExportJob fastJob = job;
        fastJob.outputPath = temporary;
        FfmpegVideoExporter fastExporter;
        if (fastExporter.Export(ffmpeg, fastJob, document, cancelRequested_,
            [&](double value, std::wstring message) {
                Post(ExportState::Rendering, value, std::move(message), job.outputPath);
            }, error)) {
            Post(ExportState::Finalizing, 1.0, L"MP4 wird atomar veröffentlicht …", job.outputPath);
            if (!MoveFileExW(temporary.c_str(), job.outputPath.c_str(),
                             MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                fail(L"Die fertige MP4-Datei konnte nicht an den Zielpfad verschoben werden.");
                return;
            }
            Post(ExportState::Done, 1.0, L"Video fertig: " + job.outputPath.wstring(),
                 job.outputPath);
            finish();
            return;
        }
        if (cancelRequested_ || error == L"CANCELLED") {
            cancelled();
            return;
        }
        std::error_code removeError;
        fs::remove(temporary, removeError);
        Post(ExportState::Preparing, 0.0,
             L"FAST Render nicht verfügbar; nativer Frame-Fallback startet …", job.outputPath);
    }

    VideoRenderer renderer;
    Post(ExportState::Preparing, 0.0, L"1080p-Renderer und visuelle Timeline werden geladen …", job.outputPath);
    if (!renderer.Initialize(job.preset.size.width, job.preset.size.height, error)) {
        fail(L"Video-Renderer: " + error);
        return;
    }
    const bool visualsLoaded = !job.visualTimeline.Empty()
        ? renderer.LoadTimeline(job.visualTimeline, error)
        : renderer.LoadCover(job.coverPath, error);
    if (!visualsLoaded) {
        fail(L"Video-Renderer: " + error);
        return;
    }

    Mp4Encoder encoder;
    Post(ExportState::Preparing, 0.0, L"H.264/AAC-Encoder wird geöffnet …", job.outputPath);
    if (!encoder.Open(temporary, job.audioPath, job.preset.size.width, job.preset.size.height,
                      job.preset.fps, job.preset.videoBitrate, job.preset.audioBitrate, error)) {
        fail(L"MP4-Encoder: " + error);
        return;
    }
    Post(ExportState::Rendering, 0.0, L"Encoder bereit. Rendering startet …", job.outputPath);

    const auto totalFrames = static_cast<std::uint64_t>(
        std::ceil(document.Duration() * static_cast<double>(job.preset.fps)));
    std::vector<std::byte> pixels;
    const std::uint64_t updateInterval = std::max<std::uint64_t>(1, job.preset.fps / 4);
    for (std::uint64_t frame = 0; frame < totalFrames; ++frame) {
        if (cancelRequested_) {
            encoder.Cancel();
            cancelled();
            return;
        }
        const double seconds = static_cast<double>(frame) / job.preset.fps;
        const std::int64_t timestamp = static_cast<std::int64_t>(frame * 10'000'000ULL / job.preset.fps);
        const std::int64_t nextTimestamp = static_cast<std::int64_t>((frame + 1) * 10'000'000ULL / job.preset.fps);
        if (!renderer.RenderFrame(seconds, document.Segments(), job.preset, pixels, error) ||
            !encoder.WriteVideoFrame(pixels, timestamp, nextTimestamp - timestamp, error)) {
            encoder.Cancel();
            fail(error);
            return;
        }
        if (frame % updateInterval == 0 || frame + 1 == totalFrames) {
            const double progress = static_cast<double>(frame + 1) / totalFrames;
            Post(ExportState::Rendering, progress,
                 L"Rendering " + FormatTime(seconds) + L" / " + FormatTime(document.Duration()),
                 job.outputPath);
        }
    }

    if (cancelRequested_) {
        encoder.Cancel();
        cancelled();
        return;
    }
    Post(ExportState::Finalizing, 1.0, L"MP4 wird finalisiert …", job.outputPath);
    if (!encoder.Finalize(error)) {
        fail(error);
        return;
    }

    if (!MoveFileExW(temporary.c_str(), job.outputPath.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        fail(L"Die fertige MP4-Datei konnte nicht an den Zielpfad verschoben werden.");
        return;
    }
    Post(ExportState::Done, 1.0, L"Video fertig: " + job.outputPath.wstring(),
         job.outputPath);
    finish();
}

} // namespace kg
