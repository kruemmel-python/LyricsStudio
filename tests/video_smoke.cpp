#include "mp4_encoder.hpp"
#include "video_renderer.hpp"

#include <mfapi.h>

#include <iostream>

using namespace kg;

int wmain(int argc, wchar_t** argv) {
    if (argc < 4 || argc > 5) {
        std::wcerr << L"Usage: KlanggeistVideoSmoke <audio> <cover> <output.mp4> [1080p]\n";
        return 2;
    }
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) return 3;
    if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_LITE))) {
        CoUninitialize();
        return 4;
    }

    VideoExportPreset preset = KlanggeistDefaultPreset();
    preset.size = {320, 180};
    preset.fps = 10;
    preset.videoBitrate = 700'000;
    preset.audioBitrate = 128'000;
    preset.driftPixelsX = 3.0f;
    preset.driftPixelsY = 2.0f;
    const bool fullHd = argc == 5 && std::wstring_view(argv[4]) == L"1080p";
    if (fullHd) {
        preset.size = {1920, 1080};
        preset.fps = 30;
        preset.videoBitrate = 12'000'000;
        preset.driftPixelsX = 16.0f;
        preset.driftPixelsY = 9.0f;
    }

    std::vector<LyricSegment> lyrics{
        {0.0, 0.75, L"Klanggeist renderer smoke test"},
        {0.75, 1.45, L"Cover, motion and timed lyrics"},
        {1.45, 2.0, L"Native H.264 and AAC export"}
    };
    std::wstring error;
    VideoRenderer renderer;
    Mp4Encoder encoder;
    int result = 0;
    if (!renderer.Initialize(preset.size.width, preset.size.height, error) ||
        !renderer.LoadCover(argv[2], error) ||
        !encoder.Open(argv[3], argv[1], preset.size.width, preset.size.height, preset.fps,
                      preset.videoBitrate, preset.audioBitrate, error)) {
        std::wcerr << error << L"\n";
        result = 5;
    } else {
        std::vector<std::byte> pixels;
        const std::uint64_t frameCount = static_cast<std::uint64_t>(preset.fps) * 2;
        for (std::uint64_t frame = 0; frame < frameCount; ++frame) {
            const auto timestamp = static_cast<std::int64_t>(frame * 10'000'000ULL / preset.fps);
            const auto next = static_cast<std::int64_t>((frame + 1) * 10'000'000ULL / preset.fps);
            if (!renderer.RenderFrame(static_cast<double>(frame) / preset.fps, lyrics, preset,
                                      pixels, error) ||
                !encoder.WriteVideoFrame(pixels, timestamp, next - timestamp, error)) {
                std::wcerr << error << L"\n";
                result = 6;
                break;
            }
        }
        if (result == 0 && !encoder.Finalize(error)) {
            std::wcerr << error << L"\n";
            result = 7;
        }
    }
    MFShutdown();
    CoUninitialize();
    return result;
}
