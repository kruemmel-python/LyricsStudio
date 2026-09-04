#include "production_planner.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <iterator>
#include <vector>

namespace kg {
namespace {

struct Slot {
    double start{};
    double end{};
    SongSectionType type{SongSectionType::Verse};
};

double TargetClipSeconds(SongSectionType type) noexcept {
    switch (type) {
    case SongSectionType::Intro: return 12.0;
    case SongSectionType::Verse: return 10.5;
    case SongSectionType::PreChorus: return 8.0;
    case SongSectionType::Chorus: return 6.5;
    case SongSectionType::Bridge: return 9.0;
    case SongSectionType::Instrumental: return 8.5;
    case SongSectionType::Outro: return 12.0;
    }
    return 10.0;
}

VisualStyle ApplySectionStyle(VisualStyle style, SongSectionType type) noexcept {
    switch (type) {
    case SongSectionType::Intro:
        style.zoomGain *= 0.78f; style.driftGain *= 1.18f; style.lyricsScale *= 0.92f; break;
    case SongSectionType::Verse:
        style.zoomGain *= 0.92f; style.driftGain *= 1.08f; style.lyricsScale *= 0.98f; break;
    case SongSectionType::PreChorus:
        style.zoomGain *= 1.10f; style.driftGain *= 0.92f; style.lyricsScale *= 1.04f; break;
    case SongSectionType::Chorus:
        style.zoomGain *= 1.28f; style.driftGain *= 0.78f; style.lyricsScale *= 1.14f; break;
    case SongSectionType::Bridge:
        style.zoomGain *= 0.86f; style.driftGain *= 1.22f; style.lyricsScale *= 1.00f; break;
    case SongSectionType::Instrumental:
        style.zoomGain *= 1.14f; style.driftGain *= 1.12f; style.lyricsScale *= 0.88f; break;
    case SongSectionType::Outro:
        style.zoomGain *= 0.74f; style.driftGain *= 1.24f; style.lyricsScale *= 0.92f; break;
    }
    style.zoomGain = std::clamp(style.zoomGain, 0.50f, 1.75f);
    style.driftGain = std::clamp(style.driftGain, 0.40f, 1.65f);
    style.lyricsScale = std::clamp(style.lyricsScale, 0.78f, 1.38f);
    return style;
}

std::size_t ResolveCoverIndex(const std::vector<VisualImageProfile>& images, const fs::path& requested) {
    if (!requested.empty()) {
        const auto it = std::find_if(images.begin(), images.end(), [&](const VisualImageProfile& profile) {
            return profile.path == requested;
        });
        if (it != images.end()) return static_cast<std::size_t>(std::distance(images.begin(), it));
    }
    return 0;
}

double ResolveCoverEnd(double duration, const AudioAnalysis& audio, const SongStructure& structure) {
    const double upper = std::min(10.0, std::max(3.0, duration * 0.12));
    double best = std::clamp(6.0, 3.0, std::max(3.0, std::min(10.0, duration)));
    for (const auto& section : structure.sections) {
        if (section.end >= 3.0 && section.end <= upper) {
            best = section.end;
            break;
        }
    }
    double onsetBestDistance = std::numeric_limits<double>::infinity();
    for (const double onset : audio.onsets) {
        if (onset < 3.0 || onset > 10.0) continue;
        const double distance = std::abs(onset - best);
        if (distance < onsetBestDistance) {
            onsetBestDistance = distance;
            best = onset;
        }
    }
    return std::clamp(best, std::min(3.0, duration), std::min(10.0, duration));
}

bool InRecent(std::size_t index, const std::deque<std::size_t>& recent) {
    return std::find(recent.begin(), recent.end(), index) != recent.end();
}

} // namespace

VisualTimeline BuildTatarusProductionTimeline(const std::vector<VisualImageProfile>& images,
                                              double duration,
                                              const AudioAnalysis& audio,
                                              const SongStructure& structure,
                                              const TatarusVisualBrain& brain,
                                              const ProductionPlanOptions& options,
                                              double crossFadeSeconds) {
    if (images.empty() || duration <= 0.0 || audio.Empty() || structure.Empty()) {
        return BuildTatarusVisualTimeline(images, duration, audio, brain, crossFadeSeconds);
    }

    const std::size_t coverIndex = ResolveCoverIndex(images, options.albumCoverPath);
    const double coverEnd = ResolveCoverEnd(duration, audio, structure);

    std::vector<Slot> slots;
    for (const auto& section : structure.sections) {
        const double sectionStart = std::max(section.start, coverEnd);
        if (section.end <= sectionStart) continue;
        const double length = section.end - sectionStart;
        const double target = TargetClipSeconds(section.type);
        const int parts = std::max(1, static_cast<int>(std::ceil(length / target)));
        for (int part = 0; part < parts; ++part) {
            const double idealStart = sectionStart + length * static_cast<double>(part) / parts;
            const double idealEnd = sectionStart + length * static_cast<double>(part + 1) / parts;
            double start = idealStart;
            if (part > 0) {
                double bestDistance = 1.35;
                for (const double onset : audio.onsets) {
                    const double distance = std::abs(onset - idealStart);
                    if (distance < bestDistance && onset > sectionStart + 1.25 && onset < section.end - 1.25) {
                        start = onset;
                        bestDistance = distance;
                    }
                }
                if (!slots.empty()) slots.back().end = start;
            }
            slots.push_back({start, idealEnd, section.type});
        }
    }

    if (slots.empty()) {
        slots.push_back({coverEnd, duration, SongSectionType::Verse});
    }
    slots.front().start = coverEnd;
    slots.back().end = duration;

    // Wenn "alle Bilder verwenden" aktiv ist, muss es mindestens genügend Slots geben,
    // damit jedes Nicht-Cover-Bild einmal vorkommen kann. Dafür werden die längsten Slots geteilt.
    const std::size_t requiredNonCoverSlots = options.requireAllImages && images.size() > 1 ? images.size() - 1 : 0;
    while (slots.size() < requiredNonCoverSlots) {
        const auto longest = std::max_element(slots.begin(), slots.end(), [](const Slot& a, const Slot& b) {
            return (a.end - a.start) < (b.end - b.start);
        });
        if (longest == slots.end() || longest->end - longest->start <= 0.05) break;
        const double mid = (longest->start + longest->end) * 0.5;
        Slot second{mid, longest->end, longest->type};
        longest->end = mid;
        slots.insert(std::next(longest), second);
    }

    VisualTimeline timeline;
    timeline.clips.reserve(slots.size() + 1);
    const auto coverContext = brain.Sense(std::min(coverEnd * 0.5, duration), duration, audio);
    const auto coverStyle = ApplySectionStyle(brain.StyleFor(coverContext), SongSectionType::Intro);
    const double coverFade = std::min({crossFadeSeconds, coverEnd * 0.25, std::max(0.0, coverEnd)});
    timeline.clips.push_back({images[coverIndex].path, 0.0, coverEnd, VisualTransition::CrossFade, coverFade, coverStyle});

    std::vector<std::size_t> usage(images.size(), 0);
    usage[coverIndex] = 1;
    std::deque<std::size_t> recent;
    recent.push_back(coverIndex);

    for (std::size_t i = 0; i < slots.size(); ++i) {
        const auto& slot = slots[i];
        const double clipDuration = std::max(0.0, slot.end - slot.start);
        const double sampleTime = slot.start + clipDuration * 0.5;
        const auto context = brain.Sense(sampleTime, duration, audio);

        bool hasUnused = false;
        if (options.requireAllImages) {
            for (std::size_t imageIndex = 0; imageIndex < images.size(); ++imageIndex) {
                if (imageIndex != coverIndex && usage[imageIndex] == 0) {
                    hasUnused = true;
                    break;
                }
            }
        }

        std::size_t chosen = coverIndex;
        float bestScore = -std::numeric_limits<float>::infinity();
        for (std::size_t imageIndex = 0; imageIndex < images.size(); ++imageIndex) {
            if (hasUnused && (imageIndex == coverIndex || usage[imageIndex] != 0)) continue;

            float score = brain.ScoreImage(context, images[imageIndex].features);
            if (slot.type == SongSectionType::Chorus) score += 0.08f * images[imageIndex].features.vividness;
            if (slot.type == SongSectionType::Intro || slot.type == SongSectionType::Outro)
                score += 0.06f * (1.0f - images[imageIndex].features.edgeDensity);

            score -= static_cast<float>(usage[imageIndex]) * options.reusePenalty;
            if (InRecent(imageIndex, recent)) score -= 0.42f;
            if (!recent.empty() && imageIndex == recent.back() && images.size() > 1) score -= 1.25f;

            if (score > bestScore) {
                bestScore = score;
                chosen = imageIndex;
            }
        }

        ++usage[chosen];
        recent.push_back(chosen);
        const std::size_t cooldown = std::min(options.reuseCooldown, images.size() > 1 ? images.size() - 1 : std::size_t{0});
        while (recent.size() > cooldown + 1) recent.pop_front();

        auto transition = i + 1 < slots.size() ? brain.TransitionFor(context) : VisualTransition::Cut;
        if (slot.type == SongSectionType::Chorus && context[1] > 0.48f) transition = VisualTransition::Cut;
        if (slot.type == SongSectionType::Outro) transition = VisualTransition::CrossFade;
        const double safeFade = transition == VisualTransition::CrossFade
            ? std::min({crossFadeSeconds, clipDuration * 0.30, std::max(0.0, clipDuration)})
            : 0.0;
        const auto style = ApplySectionStyle(brain.StyleFor(context), slot.type);
        timeline.clips.push_back({images[chosen].path, slot.start, slot.end, transition, safeFade, style});
    }
    return timeline;
}

} // namespace kg
