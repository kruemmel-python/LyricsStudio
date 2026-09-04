#include "mp4_encoder.hpp"

#include <mferror.h>

#include <cstring>
#include <limits>

namespace kg {

namespace {

constexpr DWORD kFirstAudioStream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM);
constexpr DWORD kAllStreams = static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS);

std::wstring HrError(std::wstring_view message, HRESULT hr) {
    wchar_t code[24]{};
    swprintf_s(code, L" (0x%08X)", static_cast<unsigned>(hr));
    return std::wstring(message) + code;
}

bool SetVideoGeometry(IMFMediaType* type, std::uint32_t width, std::uint32_t height,
                      std::uint32_t fps) {
    return SUCCEEDED(MFSetAttributeSize(type, MF_MT_FRAME_SIZE, width, height)) &&
           SUCCEEDED(MFSetAttributeRatio(type, MF_MT_FRAME_RATE, fps, 1)) &&
           SUCCEEDED(MFSetAttributeRatio(type, MF_MT_PIXEL_ASPECT_RATIO, 1, 1)) &&
           SUCCEEDED(type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));
}

} // namespace

bool Mp4Encoder::Open(const fs::path& output, const fs::path& audio, std::uint32_t width,
                      std::uint32_t height, std::uint32_t fps, std::uint32_t videoBitrate,
                      std::uint32_t audioBitrate, std::wstring& error) {
    Cancel();
    width_ = width;
    height_ = height;
    audioEos_ = false;
    audioNextTime_ = 0;
    videoEndTime_ = 0;

    HRESULT hr = MFCreateSinkWriterFromURL(output.c_str(), nullptr, nullptr, writer_.GetAddressOf());
    if (FAILED(hr)) {
        error = HrError(L"MP4-Sink-Writer konnte nicht geöffnet werden.", hr);
        Cancel();
        return false;
    }

    ComPtr<IMFMediaType> videoOutput;
    hr = MFCreateMediaType(videoOutput.GetAddressOf());
    if (SUCCEEDED(hr)) hr = videoOutput->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (SUCCEEDED(hr)) hr = videoOutput->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    if (SUCCEEDED(hr)) hr = videoOutput->SetUINT32(MF_MT_AVG_BITRATE, videoBitrate);
    if (SUCCEEDED(hr) && !SetVideoGeometry(videoOutput.Get(), width, height, fps)) hr = E_FAIL;
    if (SUCCEEDED(hr)) hr = writer_->AddStream(videoOutput.Get(), &videoStream_);
    if (FAILED(hr)) {
        error = HrError(L"H.264-Ausgabestream konnte nicht erstellt werden.", hr);
        Cancel();
        return false;
    }

    ComPtr<IMFMediaType> videoInput;
    hr = MFCreateMediaType(videoInput.GetAddressOf());
    if (SUCCEEDED(hr)) hr = videoInput->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (SUCCEEDED(hr)) hr = videoInput->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    if (SUCCEEDED(hr) && !SetVideoGeometry(videoInput.Get(), width, height, fps)) hr = E_FAIL;
    if (SUCCEEDED(hr)) hr = videoInput->SetUINT32(MF_MT_DEFAULT_STRIDE, width * 4);
    if (SUCCEEDED(hr)) hr = writer_->SetInputMediaType(videoStream_, videoInput.Get(), nullptr);
    if (FAILED(hr)) {
        error = HrError(L"BGRA-Eingabe konnte nicht mit dem H.264-Encoder verbunden werden.", hr);
        Cancel();
        return false;
    }

    ComPtr<IMFAttributes> readerAttributes;
    hr = MFCreateAttributes(readerAttributes.GetAddressOf(), 1);
    if (SUCCEEDED(hr)) hr = MFCreateSourceReaderFromURL(audio.c_str(), readerAttributes.Get(),
                                                        audioReader_.GetAddressOf());
    if (FAILED(hr)) {
        error = HrError(L"Audioquelle konnte für den MP4-Export nicht geöffnet werden.", hr);
        Cancel();
        return false;
    }
    audioReader_->SetStreamSelection(kAllStreams, FALSE);
    audioReader_->SetStreamSelection(kFirstAudioStream, TRUE);

    ComPtr<IMFMediaType> requestedAudio;
    hr = MFCreateMediaType(requestedAudio.GetAddressOf());
    if (SUCCEEDED(hr)) hr = requestedAudio->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    if (SUCCEEDED(hr)) hr = requestedAudio->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    if (SUCCEEDED(hr)) hr = requestedAudio->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48'000);
    if (SUCCEEDED(hr)) hr = requestedAudio->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
    if (SUCCEEDED(hr)) hr = requestedAudio->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    if (SUCCEEDED(hr)) hr = requestedAudio->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 4);
    if (SUCCEEDED(hr)) hr = requestedAudio->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 192'000);
    if (SUCCEEDED(hr)) hr = audioReader_->SetCurrentMediaType(kFirstAudioStream,
                                                              nullptr, requestedAudio.Get());
    if (FAILED(hr)) {
        error = HrError(L"Audio konnte nicht auf PCM 48 kHz Stereo konvertiert werden.", hr);
        Cancel();
        return false;
    }

    ComPtr<IMFMediaType> audioInput;
    hr = audioReader_->GetCurrentMediaType(kFirstAudioStream,
                                           audioInput.GetAddressOf());
    UINT32 sampleRate = 48'000, channels = 2, bits = 16, blockAlign = 4;
    if (SUCCEEDED(hr)) audioInput->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);
    if (SUCCEEDED(hr)) audioInput->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
    if (SUCCEEDED(hr)) audioInput->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits);
    if (SUCCEEDED(hr)) audioInput->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &blockAlign);
    audioBytesPerSecond_ = sampleRate * blockAlign;

    ComPtr<IMFMediaType> audioOutput;
    if (SUCCEEDED(hr)) hr = MFCreateMediaType(audioOutput.GetAddressOf());
    if (SUCCEEDED(hr)) hr = audioOutput->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    if (SUCCEEDED(hr)) hr = audioOutput->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
    if (SUCCEEDED(hr)) hr = audioOutput->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sampleRate);
    if (SUCCEEDED(hr)) hr = audioOutput->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, channels);
    if (SUCCEEDED(hr)) hr = audioOutput->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, bits);
    if (SUCCEEDED(hr)) hr = audioOutput->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, audioBitrate / 8);
    if (SUCCEEDED(hr)) hr = audioOutput->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0);
    if (SUCCEEDED(hr)) hr = audioOutput->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0x29);
    if (SUCCEEDED(hr)) hr = writer_->AddStream(audioOutput.Get(), &audioStream_);
    if (SUCCEEDED(hr)) hr = writer_->SetInputMediaType(audioStream_, audioInput.Get(), nullptr);
    if (SUCCEEDED(hr)) hr = writer_->BeginWriting();
    if (FAILED(hr)) {
        error = HrError(L"AAC-Audiostream konnte nicht gestartet werden.", hr);
        Cancel();
        return false;
    }
    return true;
}

bool Mp4Encoder::WriteAudioUntil(std::int64_t target100ns, std::wstring& error) {
    while (!audioEos_) {
        DWORD flags = 0;
        LONGLONG sourceTime = 0;
        ComPtr<IMFSample> sample;
        HRESULT hr = audioReader_->ReadSample(kFirstAudioStream, 0, nullptr,
                                               &flags, &sourceTime, sample.GetAddressOf());
        if (FAILED(hr)) {
            error = HrError(L"Audio konnte während des Exports nicht dekodiert werden.", hr);
            return false;
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            audioEos_ = true;
            break;
        }
        if (!sample) continue;
        (void)sourceTime;
        const LONGLONG time = audioNextTime_;
        LONGLONG duration = 0;
        if (FAILED(sample->GetSampleDuration(&duration)) || duration <= 0) {
            ComPtr<IMFMediaBuffer> buffer;
            DWORD bytes = 0;
            if (SUCCEEDED(sample->ConvertToContiguousBuffer(buffer.GetAddressOf()))) buffer->GetCurrentLength(&bytes);
            duration = audioBytesPerSecond_ ? static_cast<LONGLONG>(bytes) * 10'000'000 / audioBytesPerSecond_ : 0;
        }
        sample->SetSampleTime(time);
        if (duration > 0) sample->SetSampleDuration(duration);
        hr = writer_->WriteSample(audioStream_, sample.Get());
        if (FAILED(hr)) {
            error = HrError(L"AAC-Audiosample konnte nicht geschrieben werden.", hr);
            return false;
        }
        audioNextTime_ = time + std::max<LONGLONG>(duration, 1);
        if (audioNextTime_ >= target100ns) break;
    }
    return true;
}

bool Mp4Encoder::WriteVideoFrame(std::span<const std::byte> bgra, std::int64_t timestamp100ns,
                                 std::int64_t duration100ns, std::wstring& error) {
    const std::size_t stride = static_cast<std::size_t>(width_) * 4;
    const std::size_t bytes = stride * height_;
    if (!writer_ || bgra.size() < bytes) {
        error = L"Ungültiger BGRA-Videoframe.";
        return false;
    }

    ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = MFCreateMemoryBuffer(static_cast<DWORD>(bytes), buffer.GetAddressOf());
    BYTE* destination = nullptr;
    DWORD maximum = 0;
    if (SUCCEEDED(hr)) hr = buffer->Lock(&destination, &maximum, nullptr);
    if (SUCCEEDED(hr)) {
        std::memcpy(destination, bgra.data(), bytes);
        buffer->Unlock();
        hr = buffer->SetCurrentLength(static_cast<DWORD>(bytes));
    }
    ComPtr<IMFSample> sample;
    if (SUCCEEDED(hr)) hr = MFCreateSample(sample.GetAddressOf());
    if (SUCCEEDED(hr)) hr = sample->AddBuffer(buffer.Get());
    if (SUCCEEDED(hr)) hr = sample->SetSampleTime(timestamp100ns);
    if (SUCCEEDED(hr)) hr = sample->SetSampleDuration(duration100ns);
    if (SUCCEEDED(hr)) hr = writer_->WriteSample(videoStream_, sample.Get());
    if (FAILED(hr)) {
        error = HrError(L"H.264-Videoframe konnte nicht geschrieben werden.", hr);
        return false;
    }
    videoEndTime_ = std::max(videoEndTime_, timestamp100ns + duration100ns);
    return WriteAudioUntil(timestamp100ns + duration100ns, error);
}

bool Mp4Encoder::Finalize(std::wstring& error) {
    if (!writer_) return true;
    if (!WriteAudioUntil(videoEndTime_, error)) {
        Cancel();
        return false;
    }
    const HRESULT hr = writer_->Finalize();
    if (FAILED(hr)) {
        error = HrError(L"MP4-Datei konnte nicht finalisiert werden.", hr);
        Cancel();
        return false;
    }
    writer_.Reset();
    audioReader_.Reset();
    return true;
}

void Mp4Encoder::Cancel() noexcept {
    audioReader_.Reset();
    writer_.Reset();
    audioEos_ = true;
}

} // namespace kg
