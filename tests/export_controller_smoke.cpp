#include "video_export_controller.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

using namespace kg;

int wmain(int argc, wchar_t** argv) {
    if (argc < 5 || argc > 7) {
        std::wcerr << L"Usage: KlanggeistExportControllerSmoke <audio> <lyrics.json> <cover> <output.mp4> [1080p] [complete]\n";
        return 2;
    }
    const wchar_t className[] = L"KlanggeistExportControllerSmokeWindow";
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = className;
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 3;
    HWND window = CreateWindowExW(0, className, L"", 0, 0, 0, 0, 0,
                                  HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    if (!window) return 4;

    VideoExportController controller(window);
    VideoExportJob job;
    job.audioPath = argv[1];
    job.lyricsJsonPath = argv[2];
    job.coverPath = argv[3];
    job.outputPath = argv[4];
    job.preset = KlanggeistDefaultPreset();
    job.preset.size = {320, 180};
    job.preset.fps = 10;
    job.preset.videoBitrate = 700'000;
    const bool fullHd = argc >= 6 && std::wstring_view(argv[5]) == L"1080p";
    const bool complete = argc >= 7 && std::wstring_view(argv[6]) == L"complete";
    if (fullHd) {
        job.preset.size = {1920, 1080};
        job.preset.fps = 30;
        job.preset.videoBitrate = 12'000'000;
    }
    std::wstring error;
    if (!controller.Start(job, error)) {
        std::wcerr << error << L"\n";
        DestroyWindow(window);
        return 5;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(180);
    bool sawPreparing = false;
    bool sawRendering = false;
    bool requestedCancel = false;
    bool finished = false;
    int result = 0;
    while (!finished && std::chrono::steady_clock::now() < deadline) {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message != WM_KG_EXPORT_EVENT) continue;
            std::unique_ptr<ExportProgress> event(reinterpret_cast<ExportProgress*>(msg.lParam));
            std::wcout << static_cast<int>(event->state) << L" "
                       << static_cast<int>(event->progress * 100.0) << L"% "
                       << event->message << L"\n";
            sawPreparing = sawPreparing || event->state == ExportState::Preparing;
            sawRendering = sawRendering || event->state == ExportState::Rendering;
            if (!complete && event->state == ExportState::Rendering && event->progress >= 0.02 && !requestedCancel) {
                requestedCancel = true;
                controller.Cancel();
            }
            if (event->state == ExportState::Cancelled) { if (complete) result = 10; finished = true; }
            if (event->state == ExportState::Error) { result = 6; finished = true; }
            if (event->state == ExportState::Done) { if (!complete) result = 7; finished = true; }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!finished) { controller.Cancel(); result = 8; }
    if (!sawPreparing || !sawRendering) result = 9;
    DestroyWindow(window);
    return result;
}
