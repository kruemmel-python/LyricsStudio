#include "audio_analysis.hpp"

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propvarutil.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <optional>

namespace kg {
namespace {

constexpr DWORD kAudioStream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM);
constexpr DWORD kMediaSource = static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE);

float ClampUnit(float v) noexcept { return std::clamp(v, 0.0f, 1.0f); }

float Percentile(std::vector<float> values, float q) {
    if (values.empty()) return 0.0f;
    const auto index = static_cast<std::size_t>(std::clamp(q, 0.0f, 1.0f) * static_cast<float>(values.size() - 1));
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(index), values.end());
    return values[index];
}

} // namespace

bool AudioAnalyzer::Analyze(const fs::path& path, AudioAnalysis& result, std::wstring& error) const {
    result = {};
    if (path.empty() || !fs::exists(path)) {
        error = L"Audiodatei für die Analyse wurde nicht gefunden.";
        return false;
    }

    const HRESULT startup = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    if (FAILED(startup)) {
        error = L"Media Foundation konnte für die Audioanalyse nicht gestartet werden.";
        return false;
    }
    struct ShutdownGuard { ~ShutdownGuard(){ MFShutdown(); } } shutdown;

    ComPtr<IMFSourceReader> reader;
    HRESULT hr = MFCreateSourceReaderFromURL(path.c_str(), nullptr, reader.GetAddressOf());
    if (FAILED(hr)) {
        error = L"Audioanalyse: Datei kann nicht geöffnet werden.";
        return false;
    }

    ComPtr<IMFMediaType> requested;
    MFCreateMediaType(requested.GetAddressOf());
    requested->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    requested->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
    requested->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 1);
    requested->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000);
    hr = reader->SetCurrentMediaType(kAudioStream, nullptr, requested.Get());
    if (FAILED(hr)) {
        // Manche Decoder akzeptieren die Mono/48-kHz-Vorgabe nicht. Float ohne Zwangswerte ist der robuste Fallback.
        requested.Reset();
        MFCreateMediaType(requested.GetAddressOf());
        requested->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        requested->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
        hr = reader->SetCurrentMediaType(kAudioStream, nullptr, requested.Get());
    }
    if (FAILED(hr)) {
        error = L"Audioanalyse: PCM-Float-Decodierung wird für diese Datei nicht unterstützt.";
        return false;
    }

    ComPtr<IMFMediaType> actual;
    reader->GetCurrentMediaType(kAudioStream, actual.GetAddressOf());
    UINT32 sampleRate = 0, channels = 0;
    actual->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);
    actual->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
    if (sampleRate == 0 || channels == 0) {
        error = L"Audioanalyse: ungültiges dekodiertes Audioformat.";
        return false;
    }
    result.sampleRate = sampleRate;

    PROPVARIANT duration;
    PropVariantInit(&duration);
    if (SUCCEEDED(reader->GetPresentationAttribute(kMediaSource, MF_PD_DURATION, &duration)) && duration.vt == VT_UI8)
        result.duration = static_cast<double>(duration.uhVal.QuadPart) / 10'000'000.0;
    PropVariantClear(&duration);

    constexpr double kWindowSeconds = 0.050; // 50 ms: genug Detail für Onsets, klein genug für UI/Planer.
    const std::size_t windowFrames = std::max<std::size_t>(1, static_cast<std::size_t>(sampleRate * kWindowSeconds));
    std::size_t framesInWindow = 0;
    double sumSquares = 0.0;
    float peak = 0.0f;
    std::uint64_t totalFrames = 0;

    auto flushWindow = [&]() {
        if (framesInWindow == 0) return;
        const float rms = static_cast<float>(std::sqrt(sumSquares / static_cast<double>(framesInWindow)));
        const double time = static_cast<double>(totalFrames - framesInWindow / 2) / static_cast<double>(sampleRate);
        result.envelope.push_back({time, rms, peak});
        framesInWindow = 0;
        sumSquares = 0.0;
        peak = 0.0f;
    };

    for (;;) {
        DWORD flags = 0;
        ComPtr<IMFSample> sample;
        hr = reader->ReadSample(kAudioStream, 0, nullptr, &flags, nullptr, sample.GetAddressOf());
        if (FAILED(hr)) {
            error = L"Audioanalyse: Fehler beim Dekodieren.";
            return false;
        }
        if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0) break;
        if (!sample) continue;

        ComPtr<IMFMediaBuffer> buffer;
        if (FAILED(sample->ConvertToContiguousBuffer(buffer.GetAddressOf()))) continue;
        BYTE* data = nullptr; DWORD maxLength = 0, length = 0;
        if (FAILED(buffer->Lock(&data, &maxLength, &length))) continue;
        const auto* values = reinterpret_cast<const float*>(data);
        const std::size_t valueCount = length / sizeof(float);
        const std::size_t frameCount = valueCount / channels;
        for (std::size_t frame = 0; frame < frameCount; ++frame) {
            double mixed = 0.0;
            for (UINT32 channel = 0; channel < channels; ++channel)
                mixed += values[frame * channels + channel];
            const float mono = static_cast<float>(mixed / static_cast<double>(channels));
            sumSquares += static_cast<double>(mono) * mono;
            peak = std::max(peak, std::abs(mono));
            ++framesInWindow;
            ++totalFrames;
            if (framesInWindow >= windowFrames) flushWindow();
        }
        buffer->Unlock();
    }
    flushWindow();

    if (result.envelope.empty()) {
        error = L"Audioanalyse: Datei enthielt keine auswertbaren Audiodaten.";
        return false;
    }
    if (result.duration <= 0.0) result.duration = static_cast<double>(totalFrames) / sampleRate;

    std::vector<float> rmsValues;
    rmsValues.reserve(result.envelope.size());
    for (const auto& point : result.envelope) rmsValues.push_back(point.rms);
    const float p20 = Percentile(rmsValues, 0.20f);
    const float p50 = Percentile(rmsValues, 0.50f);
    const float p80 = Percentile(rmsValues, 0.80f);
    const float silenceThreshold = std::max(0.004f, p20 * 0.60f);

    // Positive Energie-Flanken mit lokaler Refraktärzeit liefern robuste, deterministische Onset-Marker.
    double lastOnset = -10.0;
    float previousSmooth = result.envelope.front().rms;
    for (std::size_t i = 1; i + 1 < result.envelope.size(); ++i) {
        const float current = (result.envelope[i - 1].rms + 2.0f * result.envelope[i].rms + result.envelope[i + 1].rms) * 0.25f;
        const float delta = current - previousSmooth;
        const float adaptive = std::max(0.012f, p50 * 0.35f);
        if (delta > adaptive && current > p50 * 0.75f && result.envelope[i].time - lastOnset >= 0.22) {
            result.onsets.push_back(result.envelope[i].time);
            lastOnset = result.envelope[i].time;
        }
        previousSmooth = current;
    }

    // Pausen: mindestens 350 ms unter adaptiver Energieschwelle.
    std::optional<double> pauseStart;
    for (const auto& point : result.envelope) {
        if (point.rms <= silenceThreshold) {
            if (!pauseStart) pauseStart = point.time;
        } else if (pauseStart) {
            if (point.time - *pauseStart >= 0.35) result.pauses.push_back({*pauseStart, point.time});
            pauseStart.reset();
        }
    }
    if (pauseStart && result.duration - *pauseStart >= 0.35) result.pauses.push_back({*pauseStart, result.duration});

    // Grobe Song-Sektionen in 8-s-Blöcken, danach benachbarte gleiche Energiebänder zusammenziehen.
    constexpr double kSectionSeconds = 8.0;
    std::vector<AudioSection> rawSections;
    for (double start = 0.0; start < result.duration; start += kSectionSeconds) {
        const double end = std::min(result.duration, start + kSectionSeconds);
        double sum = 0.0; std::size_t count = 0;
        for (const auto& point : result.envelope) {
            if (point.time >= start && point.time < end) { sum += point.rms; ++count; }
        }
        const float mean = count ? static_cast<float>(sum / count) : 0.0f;
        AudioEnergyBand band = AudioEnergyBand::Medium;
        if (mean <= p20 + (p50 - p20) * 0.55f) band = AudioEnergyBand::Low;
        else if (mean >= p50 + (p80 - p50) * 0.55f) band = AudioEnergyBand::High;
        rawSections.push_back({start, end, band, mean});
    }
    for (const auto& section : rawSections) {
        if (!result.sections.empty() && result.sections.back().energy == section.energy) {
            auto& back = result.sections.back();
            const double oldDuration = back.end - back.start;
            const double addDuration = section.end - section.start;
            back.meanRms = static_cast<float>((back.meanRms * oldDuration + section.meanRms * addDuration) / (oldDuration + addDuration));
            back.end = section.end;
        } else result.sections.push_back(section);
    }

    return true;
}

} // namespace kg
