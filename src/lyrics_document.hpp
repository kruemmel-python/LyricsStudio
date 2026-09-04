#pragma once
#include "common.hpp"
#include "mini_json.hpp"

namespace kg {

struct LyricSegment {
    double start{};
    double end{};
    std::wstring text;
    std::optional<double> avgLogprob;
    std::optional<double> noSpeechProb;
    std::optional<double> compressionRatio;
    bool edited{};

    [[nodiscard]] bool Suspicious() const;
    [[nodiscard]] std::wstring SuspicionReason() const;
};

class LyricsDocument {
public:
    bool Load(const fs::path& jsonPath, const fs::path& inputRoot, std::wstring& error);
    bool Save(std::wstring& error);

    [[nodiscard]] const fs::path& JsonPath() const noexcept { return jsonPath_; }
    [[nodiscard]] const fs::path& AudioPath() const noexcept { return audioPath_; }
    [[nodiscard]] const std::wstring& RelativeName() const noexcept { return relativeName_; }
    [[nodiscard]] double Duration() const noexcept { return duration_; }
    [[nodiscard]] std::vector<LyricSegment>& Segments() noexcept { return segments_; }
    [[nodiscard]] const std::vector<LyricSegment>& Segments() const noexcept { return segments_; }
    [[nodiscard]] bool Dirty() const noexcept;

private:
    fs::path jsonPath_, audioPath_, txtPath_, lrcPath_, srtPath_;
    std::wstring relativeName_;
    double duration_{};
    json::Value root_;
    std::vector<LyricSegment> segments_;
};

std::vector<fs::path> DiscoverLyricsJson(const fs::path& outputRoot);

} // namespace kg
