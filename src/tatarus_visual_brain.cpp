#include "tatarus_visual_brain.hpp"

#include "mini_json.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace kg {
namespace {

float Clamp01f(float value) noexcept { return std::clamp(value, 0.0f, 1.0f); }

float LocalRms(double time, const AudioAnalysis& analysis) noexcept {
    if (analysis.envelope.empty()) return 0.0f;
    const AudioEnvelopePoint* best = &analysis.envelope.front();
    double distance = std::abs(best->time - time);
    for (const auto& point : analysis.envelope) {
        const double d = std::abs(point.time - time);
        if (d < distance) { distance = d; best = &point; }
        if (point.time > time && d > distance) break;
    }
    return best->rms;
}

float OnsetProximity(double time, const AudioAnalysis& analysis) noexcept {
    double best = 1.0;
    for (const double onset : analysis.onsets) best = std::min(best, std::abs(onset - time));
    return Clamp01f(static_cast<float>(1.0 - best / 0.75));
}

float PauseEndProximity(double time, const AudioAnalysis& analysis) noexcept {
    double best = 1.25;
    for (const auto& pause : analysis.pauses) best = std::min(best, std::abs(pause.end - time));
    return Clamp01f(static_cast<float>(1.0 - best / 1.0));
}

float SectionEnergy(double time, const AudioAnalysis& analysis) noexcept {
    for (const auto& section : analysis.sections) {
        if (time >= section.start && time < section.end) {
            switch (section.energy) {
            case AudioEnergyBand::Low: return 0.20f;
            case AudioEnergyBand::Medium: return 0.55f;
            case AudioEnergyBand::High: return 0.90f;
            }
        }
    }
    return 0.5f;
}

float Sigmoid(float value) noexcept {
    if (value >= 0.0f) {
        const float z = std::exp(-value);
        return 1.0f / (1.0f + z);
    }
    const float z = std::exp(value);
    return z / (1.0f + z);
}

} // namespace

TatarusVisualBrain::Features TatarusVisualBrain::Sense(double time, double duration,
                                                       const AudioAnalysis& analysis) const noexcept {
    const float rms = LocalRms(time, analysis);
    const float before = LocalRms(std::max(0.0, time - 0.18), analysis);
    const float after = LocalRms(std::min(duration, time + 0.18), analysis);
    const float rise = Clamp01f((after - before) * 7.0f + 0.5f);
    const float fall = Clamp01f((before - after) * 7.0f + 0.5f);
    const float normalizedTime = duration > 0.0 ? Clamp01f(static_cast<float>(time / duration)) : 0.0f;
    return {
        Clamp01f(rms * 5.5f),             // lokale Energie
        OnsetProximity(time, analysis),    // Nähe zu Transiente/Beat
        PauseEndProximity(time, analysis), // Nähe zu Pausenende
        SectionEnergy(time, analysis),     // Energie des Songabschnitts
        rise,                              // Energieanstieg
        fall,                              // Energieabfall
        std::abs(normalizedTime - 0.5f) * 2.0f, // Randnähe des Songs
        1.0f - std::abs(normalizedTime - 0.5f) * 2.0f // Mittellage
    };
}

float TatarusVisualBrain::Score(const Features& features) const noexcept {
    float value = bias_;
    for (std::size_t i = 0; i < FeatureCount; ++i) value += weights_[i] * features[i];
    return Sigmoid(value);
}

float TatarusVisualBrain::ScoreAt(double time, double duration, const AudioAnalysis& analysis) const noexcept {
    return Score(Sense(time, duration, analysis));
}

VisualStyle TatarusVisualBrain::StyleFor(const Features& features) const noexcept {
    const float energy = features[0];
    const float onset = features[1];
    const float section = features[3];
    VisualStyle style;
    style.zoomGain = std::clamp(zoomPreference_ * (0.78f + 0.42f * section + 0.18f * onset), 0.55f, 1.65f);
    style.driftGain = std::clamp(driftPreference_ * (1.18f - 0.45f * energy + 0.20f * features[5]), 0.45f, 1.55f);
    style.lyricsScale = std::clamp(lyricsPreference_ * (0.94f + 0.16f * section + 0.10f * onset), 0.80f, 1.35f);
    return style;
}

VisualTransition TatarusVisualBrain::TransitionFor(const Features& features) const noexcept {
    const float hardCutEvidence = 0.55f * features[1] + 0.30f * features[4] + 0.15f * features[3];
    return hardCutEvidence > fadePreference_ ? VisualTransition::Cut : VisualTransition::CrossFade;
}

void TatarusVisualBrain::LearnStyle(const Features& features, VisualStyle preferred, VisualTransition transition) noexcept {
    const float lr = 0.10f / std::sqrt(1.0f + static_cast<float>(styleTrainingEvents_) * 0.03f);
    const auto current = StyleFor(features);
    zoomPreference_ = std::clamp(zoomPreference_ + lr * (preferred.zoomGain - current.zoomGain), 0.45f, 1.8f);
    driftPreference_ = std::clamp(driftPreference_ + lr * (preferred.driftGain - current.driftGain), 0.35f, 1.8f);
    lyricsPreference_ = std::clamp(lyricsPreference_ + lr * (preferred.lyricsScale - current.lyricsScale), 0.75f, 1.45f);
    const float targetFade = transition == VisualTransition::CrossFade ? 0.78f : 0.38f;
    fadePreference_ = std::clamp(fadePreference_ + lr * (targetFade - fadePreference_), 0.20f, 0.90f);
    ++styleTrainingEvents_;
}


TatarusVisualBrain::ImageMatchFeatures TatarusVisualBrain::SenseImageMatch(
    const Features& context, const VisualImageFeatures& image) const noexcept {
    const float energy = context[0];
    const float onset = context[1];
    const float section = context[3];
    const float rise = context[4];
    const float calm = 1.0f - std::clamp(0.55f * energy + 0.45f * onset, 0.0f, 1.0f);
    return {
        image.brightness * section,
        image.contrast * onset,
        image.saturation * energy,
        image.warmth * section,
        image.edgeDensity * onset,
        image.darkness * calm,
        image.vividness * rise,
        image.landscape,
        image.brightness,
        image.contrast,
        image.saturation,
        image.edgeDensity
    };
}

float TatarusVisualBrain::ScoreImage(const Features& context, const VisualImageFeatures& image) const noexcept {
    const auto features = SenseImageMatch(context, image);
    float value = imageBias_;
    for (std::size_t i = 0; i < ImageMatchFeatureCount; ++i) value += imageWeights_[i] * features[i];
    return Sigmoid(value);
}

void TatarusVisualBrain::LearnImagePreference(const Features& context,
                                              const VisualImageFeatures& preferred,
                                              const VisualImageFeatures& rejected,
                                              float strength) noexcept {
    const auto p = SenseImageMatch(context, preferred);
    const auto r = SenseImageMatch(context, rejected);
    strength = std::clamp(strength, 0.05f, 2.0f);
    float margin = 0.0f;
    for (std::size_t i = 0; i < ImageMatchFeatureCount; ++i) margin += imageWeights_[i] * (p[i] - r[i]);
    const float probability = Sigmoid(margin);
    const float learningRate = 0.10f * strength / std::sqrt(1.0f + static_cast<float>(imageTrainingEvents_) * 0.03f);
    const float gradient = (1.0f - probability) * learningRate;
    for (std::size_t i = 0; i < ImageMatchFeatureCount; ++i)
        imageWeights_[i] = std::clamp(imageWeights_[i] + gradient * (p[i] - r[i]), -3.0f, 3.0f);
    ++imageTrainingEvents_;
}

void TatarusVisualBrain::LearnPreference(const Features& preferred, const Features& rejected, float strength) noexcept {
    strength = std::clamp(strength, 0.05f, 2.0f);
    float margin = 0.0f;
    for (std::size_t i = 0; i < FeatureCount; ++i) margin += weights_[i] * (preferred[i] - rejected[i]);
    const float probability = Sigmoid(margin);
    const float learningRate = 0.12f * strength / std::sqrt(1.0f + static_cast<float>(trainingEvents_) * 0.025f);
    const float gradient = (1.0f - probability) * learningRate;
    for (std::size_t i = 0; i < FeatureCount; ++i)
        weights_[i] = std::clamp(weights_[i] + gradient * (preferred[i] - rejected[i]), -3.0f, 3.0f);
    ++trainingEvents_;
}

bool TatarusVisualBrain::Load(const fs::path& path, std::wstring& error) {
    error.clear();
    if (!fs::exists(path)) return true;
    try {
        const auto root = json::Parse(ReadUtf8File(path));
        if (const auto* value = root.Find("training_events")) trainingEvents_ = static_cast<std::uint64_t>(value->AsNumber());
        if (const auto* value = root.Find("bias")) bias_ = static_cast<float>(value->AsNumber());
        if (const auto* value = root.Find("style_training_events")) styleTrainingEvents_ = static_cast<std::uint64_t>(value->AsNumber());
        if (const auto* value = root.Find("fade_preference")) fadePreference_ = static_cast<float>(value->AsNumber());
        if (const auto* value = root.Find("zoom_preference")) zoomPreference_ = static_cast<float>(value->AsNumber());
        if (const auto* value = root.Find("drift_preference")) driftPreference_ = static_cast<float>(value->AsNumber());
        if (const auto* value = root.Find("lyrics_preference")) lyricsPreference_ = static_cast<float>(value->AsNumber());
        if (const auto* value = root.Find("image_training_events")) imageTrainingEvents_ = static_cast<std::uint64_t>(value->AsNumber());
        if (const auto* value = root.Find("image_bias")) imageBias_ = static_cast<float>(value->AsNumber());
        if (const auto* value = root.Find("weights"); value && value->IsArray()) {
            const auto& array = value->AsArray();
            if (array.size() == FeatureCount)
                for (std::size_t i = 0; i < FeatureCount; ++i) weights_[i] = static_cast<float>(array[i].AsNumber());
        }
        if (const auto* value = root.Find("image_weights"); value && value->IsArray()) {
            const auto& array = value->AsArray();
            if (array.size() == ImageMatchFeatureCount)
                for (std::size_t i = 0; i < ImageMatchFeatureCount; ++i) imageWeights_[i] = static_cast<float>(array[i].AsNumber());
        }
        return true;
    } catch (const std::exception& ex) {
        error = L"TATARUS Visual Brain konnte nicht geladen werden: " + Utf8ToWide(ex.what());
        return false;
    }
}

bool TatarusVisualBrain::Save(const fs::path& path, std::wstring& error) const {
    error.clear();
    try {
        json::Value::Object root;
        root["version"] = 3.0;
        root["training_events"] = static_cast<double>(trainingEvents_);
        root["bias"] = static_cast<double>(bias_);
        root["style_training_events"] = static_cast<double>(styleTrainingEvents_);
        root["fade_preference"] = static_cast<double>(fadePreference_);
        root["zoom_preference"] = static_cast<double>(zoomPreference_);
        root["drift_preference"] = static_cast<double>(driftPreference_);
        root["lyrics_preference"] = static_cast<double>(lyricsPreference_);
        root["image_training_events"] = static_cast<double>(imageTrainingEvents_);
        root["image_bias"] = static_cast<double>(imageBias_);
        json::Value::Array array;
        for (const float weight : weights_) array.emplace_back(static_cast<double>(weight));
        root["weights"] = std::move(array);
        json::Value::Array imageArray;
        for (const float weight : imageWeights_) imageArray.emplace_back(static_cast<double>(weight));
        root["image_weights"] = std::move(imageArray);
        AtomicWriteUtf8(path, json::Dump(json::Value(std::move(root)), 2) + "\n");
        return true;
    } catch (const std::exception& ex) {
        error = L"TATARUS Visual Brain konnte nicht gespeichert werden: " + Utf8ToWide(ex.what());
        return false;
    }
}

VisualTimeline BuildTatarusVisualTimeline(const std::vector<VisualImageProfile>& images, double duration,
                                         const AudioAnalysis& analysis, const TatarusVisualBrain& brain,
                                         double crossFadeSeconds) {
    if (images.empty() || duration <= 0.0 || images.size() == 1 || analysis.Empty())
        return BuildEvenVisualTimeline([&]{ std::vector<fs::path> p; p.reserve(images.size()); for (const auto& image : images) p.push_back(image.path); return p; }(), duration, crossFadeSeconds);

    const std::size_t boundaryCount = images.size() - 1;
    const double idealSpacing = duration / static_cast<double>(images.size());
    const double minSpacing = std::max(1.25, idealSpacing * 0.34);
    const double searchRadius = std::max(1.8, idealSpacing * 0.55);
    std::vector<double> candidates = analysis.onsets;
    for (const auto& pause : analysis.pauses) candidates.push_back(pause.end);
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end(), [](double a, double b){ return std::abs(a - b) < 0.08; }), candidates.end());

    std::vector<double> boundaries;
    boundaries.reserve(boundaryCount);
    double previous = 0.0;
    for (std::size_t i = 1; i <= boundaryCount; ++i) {
        const double ideal = idealSpacing * static_cast<double>(i);
        double best = ideal;
        float bestUtility = -std::numeric_limits<float>::infinity();
        for (const double candidate : candidates) {
            if (candidate <= previous + minSpacing || candidate >= duration - minSpacing) continue;
            const double distance = std::abs(candidate - ideal);
            if (distance > searchRadius) continue;
            const float neural = brain.ScoreAt(candidate, duration, analysis);
            const float temporal = 1.0f - std::min(1.0f, static_cast<float>(distance / searchRadius));
            const float utility = neural * 0.72f + temporal * 0.28f;
            if (utility > bestUtility) { bestUtility = utility; best = candidate; }
        }
        best = std::clamp(best, previous + minSpacing,
                          duration - minSpacing * static_cast<double>(boundaryCount - i + 1));
        boundaries.push_back(best);
        previous = best;
    }

    VisualTimeline timeline;
    timeline.clips.reserve(images.size());
    std::vector<bool> used(images.size(), false);
    for (std::size_t i = 0; i < images.size(); ++i) {
        const double start = i == 0 ? 0.0 : boundaries[i - 1];
        const double end = i + 1 == images.size() ? duration : boundaries[i];
        const double clipDuration = std::max(0.0, end - start);
        const double safeFade = std::min({crossFadeSeconds, clipDuration * 0.35, clipDuration});
        const auto f = brain.Sense(start + clipDuration * 0.5, duration, analysis);
        std::size_t chosen = 0;
        float bestImageScore = -std::numeric_limits<float>::infinity();
        for (std::size_t j = 0; j < images.size(); ++j) {
            if (used[j]) continue;
            const float score = brain.ScoreImage(f, images[j].features);
            if (score > bestImageScore) { bestImageScore = score; chosen = j; }
        }
        used[chosen] = true;
        auto transition = i + 1 < images.size() ? brain.TransitionFor(f) : VisualTransition::Cut;
        const auto style = brain.StyleFor(f);
        timeline.clips.push_back(VisualClip{images[chosen].path, start, end, transition, safeFade, style});
    }
    return timeline;
}

} // namespace kg
