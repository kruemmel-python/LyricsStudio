#pragma once

#include "audio_analysis.hpp"
#include "lyrics_document.hpp"

#include <vector>

namespace kg {

enum class SongSectionType {
    Intro,
    Verse,
    PreChorus,
    Chorus,
    Bridge,
    Instrumental,
    Outro
};

struct SongStructureSection {
    double start{};
    double end{};
    SongSectionType type{SongSectionType::Verse};
    float energy{};
    float lyricDensity{};
    float repetition{};
};

struct SongStructure {
    std::vector<SongStructureSection> sections;

    [[nodiscard]] bool Empty() const noexcept { return sections.empty(); }
    [[nodiscard]] const SongStructureSection* SectionAt(double seconds) const noexcept;
};

[[nodiscard]] SongStructure AnalyzeSongStructure(const AudioAnalysis& audio,
                                                 const std::vector<LyricSegment>& lyrics,
                                                 double duration);
[[nodiscard]] std::wstring_view SongSectionName(SongSectionType type) noexcept;

} // namespace kg
