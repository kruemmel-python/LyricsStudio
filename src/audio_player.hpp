#pragma once
#include "common.hpp"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <xaudio2.h>

namespace kg {

class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();
    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    bool Load(const fs::path& path, std::wstring& error);
    bool Play(double startSeconds, double endSeconds, std::wstring& error);
    void Stop();
    [[nodiscard]] double Duration() const noexcept { return duration_; }
    [[nodiscard]] const fs::path& Path() const noexcept { return path_; }

private:
    ComPtr<IXAudio2> xaudio_;
    IXAudio2MasteringVoice* mastering_{};
    IXAudio2SourceVoice* source_{};
    WAVEFORMATEX format_{};
    std::vector<std::byte> pcm_;
    fs::path path_;
    double duration_{};
};

} // namespace kg
