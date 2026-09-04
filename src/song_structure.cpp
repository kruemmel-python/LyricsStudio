#include "song_structure.hpp"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <map>
#include <string>

namespace kg {
namespace {

std::wstring NormalizeLyrics(std::wstring_view text) {
    std::wstring normalized;
    normalized.reserve(text.size());
    bool previousSpace = true;
    for (const wchar_t ch : text) {
        if (std::iswalnum(static_cast<wint_t>(ch)) != 0) {
            normalized.push_back(static_cast<wchar_t>(std::towlower(static_cast<wint_t>(ch))));
            previousSpace = false;
        } else if (!previousSpace && !normalized.empty()) {
            normalized.push_back(L' ');
            previousSpace = true;
        }
    }
    while (!normalized.empty() && normalized.back() == L' ') normalized.pop_back();
    return normalized;
}

float EnergyValue(AudioEnergyBand band) noexcept {
    switch (band) {
    case AudioEnergyBand::Low: return 0.22f;
    case AudioEnergyBand::Medium: return 0.56f;
    case AudioEnergyBand::High: return 0.90f;
    }
    return 0.50f;
}

float SectionEnergy(double start, double end, const AudioAnalysis& audio) noexcept {
    double weighted = 0.0;
    double duration = 0.0;
    for (const auto& section : audio.sections) {
        const double overlapStart = std::max(start, section.start);
        const double overlapEnd = std::min(end, section.end);
        const double overlap = std::max(0.0, overlapEnd - overlapStart);
        if (overlap <= 0.0) continue;
        weighted += static_cast<double>(EnergyValue(section.energy)) * overlap;
        duration += overlap;
    }
    return duration > 0.0 ? static_cast<float>(weighted / duration) : 0.5f;
}

} // namespace

const SongStructureSection* SongStructure::SectionAt(double seconds) const noexcept {
    if (sections.empty()) return nullptr;
    for (const auto& section : sections) {
        if (seconds >= section.start && seconds < section.end) return &section;
    }
    return &sections.back();
}

std::wstring_view SongSectionName(SongSectionType type) noexcept {
    switch (type) {
    case SongSectionType::Intro: return L"INTRO";
    case SongSectionType::Verse: return L"VERSE";
    case SongSectionType::PreChorus: return L"PRE";
    case SongSectionType::Chorus: return L"CHORUS";
    case SongSectionType::Bridge: return L"BRIDGE";
    case SongSectionType::Instrumental: return L"INSTR";
    case SongSectionType::Outro: return L"OUTRO";
    }
    return L"VERSE";
}

SongStructure AnalyzeSongStructure(const AudioAnalysis& audio,
                                  const std::vector<LyricSegment>& lyrics,
                                  double duration) {
    SongStructure result;
    if (duration <= 0.0) return result;

    std::map<std::wstring, std::size_t> occurrences;
    for (const auto& segment : lyrics) {
        const auto normalized = NormalizeLyrics(segment.text);
        if (normalized.size() >= 4) ++occurrences[normalized];
    }

    std::vector<double> boundaries{0.0};
    for (const auto& section : audio.sections) {
        if (section.start > 0.5 && section.start < duration - 0.5) boundaries.push_back(section.start);
        if (section.end > 0.5 && section.end < duration - 0.5) boundaries.push_back(section.end);
    }
    for (const auto& pause : audio.pauses) {
        if (pause.end - pause.start < 0.45) continue;
        const double midpoint = (pause.start + pause.end) * 0.5;
        if (midpoint > 4.0 && midpoint < duration - 4.0) boundaries.push_back(midpoint);
    }
    boundaries.push_back(duration);
    std::sort(boundaries.begin(), boundaries.end());
    boundaries.erase(std::unique(boundaries.begin(), boundaries.end(), [](double a, double b) {
        return std::abs(a - b) < 2.0;
    }), boundaries.end());

    // Sehr lange Energieblöcke werden in musikalisch handhabbare Bereiche zerlegt.
    std::vector<double> expanded{boundaries.front()};
    for (std::size_t i = 1; i < boundaries.size(); ++i) {
        const double start = expanded.back();
        const double end = boundaries[i];
        constexpr double maxSection = 24.0;
        if (end - start > maxSection) {
            const int parts = std::max(2, static_cast<int>(std::ceil((end - start) / 16.0)));
            for (int part = 1; part < parts; ++part)
                expanded.push_back(start + (end - start) * static_cast<double>(part) / static_cast<double>(parts));
        }
        expanded.push_back(end);
    }

    for (std::size_t i = 0; i + 1 < expanded.size(); ++i) {
        const double start = expanded[i];
        const double end = expanded[i + 1];
        if (end - start < 2.0) continue;

        double lyricSeconds = 0.0;
        double repeatedSeconds = 0.0;
        for (const auto& segment : lyrics) {
            const double overlap = std::max(0.0, std::min(end, segment.end) - std::max(start, segment.start));
            if (overlap <= 0.0) continue;
            lyricSeconds += overlap;
            const auto normalized = NormalizeLyrics(segment.text);
            const auto found = occurrences.find(normalized);
            if (found != occurrences.end() && found->second >= 2) repeatedSeconds += overlap;
        }

        const double length = end - start;
        SongStructureSection section;
        section.start = start;
        section.end = end;
        section.energy = SectionEnergy(start, end, audio);
        section.lyricDensity = Clamp01(static_cast<float>(lyricSeconds / std::max(1.0, length)));
        section.repetition = lyricSeconds > 0.0
            ? Clamp01(static_cast<float>(repeatedSeconds / lyricSeconds))
            : 0.0f;

        const bool first = result.sections.empty();
        const bool last = end >= duration - 0.5;
        if (first && (section.lyricDensity < 0.22f || start < 0.25 && end <= 14.0)) {
            section.type = SongSectionType::Intro;
        } else if (last && section.lyricDensity < 0.35f) {
            section.type = SongSectionType::Outro;
        } else if (section.lyricDensity < 0.12f) {
            section.type = SongSectionType::Instrumental;
        } else if (section.repetition >= 0.42f && section.energy >= 0.48f) {
            section.type = SongSectionType::Chorus;
        } else {
            section.type = SongSectionType::Verse;
        }
        result.sections.push_back(section);
    }

    // Ein energiereicher Bereich unmittelbar vor einem Chorus wird als Pre-Chorus markiert.
    for (std::size_t i = 1; i < result.sections.size(); ++i) {
        if (result.sections[i].type == SongSectionType::Chorus &&
            result.sections[i - 1].type == SongSectionType::Verse &&
            result.sections[i - 1].energy >= 0.52f) {
            result.sections[i - 1].type = SongSectionType::PreChorus;
        }
    }

    // Später, nicht repetitiver Kontrastbereich nach mindestens einem Chorus => Bridge.
    bool seenChorus = false;
    for (auto& section : result.sections) {
        if (section.type == SongSectionType::Chorus) {
            seenChorus = true;
            continue;
        }
        const double center = (section.start + section.end) * 0.5;
        if (seenChorus && center > duration * 0.52 && center < duration * 0.88 &&
            section.type == SongSectionType::Verse && section.repetition < 0.18f) {
            section.type = SongSectionType::Bridge;
            break;
        }
    }

    if (!result.sections.empty() && result.sections.back().end < duration)
        result.sections.back().end = duration;
    return result;
}

} // namespace kg
