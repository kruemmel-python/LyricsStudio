#include "audio_player.hpp"
#include <mferror.h>
#include <propvarutil.h>

namespace kg {

constexpr DWORD kFirstAudioStream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM);
constexpr DWORD kMediaSource = static_cast<DWORD>(MF_SOURCE_READER_MEDIASOURCE);

AudioPlayer::AudioPlayer(){
    MFStartup(MF_VERSION,MFSTARTUP_LITE);
    if(SUCCEEDED(XAudio2Create(xaudio_.GetAddressOf(),0,XAUDIO2_DEFAULT_PROCESSOR))&&xaudio_)xaudio_->CreateMasteringVoice(&mastering_);
}
AudioPlayer::~AudioPlayer(){Stop();if(mastering_){mastering_->DestroyVoice();mastering_=nullptr;}xaudio_.Reset();MFShutdown();}

bool AudioPlayer::Load(const fs::path& path,std::wstring& error){
    Stop();pcm_.clear();duration_=0;path_.clear();
    ComPtr<IMFSourceReader> reader;HRESULT hr=MFCreateSourceReaderFromURL(path.c_str(),nullptr,reader.GetAddressOf());if(FAILED(hr)){error=L"Media Foundation kann die Datei nicht öffnen.";return false;}
    ComPtr<IMFMediaType> requested;MFCreateMediaType(requested.GetAddressOf());requested->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Audio);requested->SetGUID(MF_MT_SUBTYPE,MFAudioFormat_PCM);
    hr=reader->SetCurrentMediaType(kFirstAudioStream,nullptr,requested.Get());if(FAILED(hr)){error=L"Audio kann nicht als PCM dekodiert werden.";return false;}
    ComPtr<IMFMediaType> actual;reader->GetCurrentMediaType(kFirstAudioStream,actual.GetAddressOf());
    WAVEFORMATEX* wf=nullptr;UINT32 wfSize=0;hr=MFCreateWaveFormatExFromMFMediaType(actual.Get(),&wf,&wfSize);if(FAILED(hr)||!wf){error=L"PCM-Format konnte nicht ermittelt werden.";return false;}
    format_=*wf;CoTaskMemFree(wf);if(format_.wFormatTag!=WAVE_FORMAT_PCM){error=L"Unerwartetes Audioformat.";return false;}
    PROPVARIANT dur;PropVariantInit(&dur);if(SUCCEEDED(reader->GetPresentationAttribute(kMediaSource,MF_PD_DURATION,&dur))&&dur.vt==VT_UI8)duration_=static_cast<double>(dur.uhVal.QuadPart)/10'000'000.0;PropVariantClear(&dur);
    for(;;){DWORD flags=0;ComPtr<IMFSample> sample;hr=reader->ReadSample(kFirstAudioStream,0,nullptr,&flags,nullptr,sample.GetAddressOf());if(FAILED(hr)){error=L"Fehler beim Dekodieren.";return false;}if(flags&MF_SOURCE_READERF_ENDOFSTREAM)break;if(!sample)continue;
        ComPtr<IMFMediaBuffer> buffer;sample->ConvertToContiguousBuffer(buffer.GetAddressOf());BYTE* data=nullptr;DWORD maxLen=0,len=0;if(SUCCEEDED(buffer->Lock(&data,&maxLen,&len))){const auto old=pcm_.size();pcm_.resize(old+len);memcpy(pcm_.data()+old,data,len);buffer->Unlock();}}
    if(format_.nAvgBytesPerSec&&duration_<=0)duration_=static_cast<double>(pcm_.size())/format_.nAvgBytesPerSec;path_=path;return !pcm_.empty();
}

bool AudioPlayer::Play(double start,double end,std::wstring& error){
    Stop();if(!xaudio_||!mastering_||pcm_.empty()){error=L"Kein Audio geladen.";return false;}start=std::clamp(start,0.0,duration_);end=std::clamp(end,start,duration_);
    HRESULT hr=xaudio_->CreateSourceVoice(&source_,&format_);if(FAILED(hr)){error=L"XAudio2 SourceVoice konnte nicht erstellt werden.";return false;}
    const UINT32 totalFrames=static_cast<UINT32>(pcm_.size()/format_.nBlockAlign);const UINT32 begin=static_cast<UINT32>(start*format_.nSamplesPerSec);const UINT32 finish=static_cast<UINT32>(end*format_.nSamplesPerSec);const UINT32 length=std::min(totalFrames,finish)>begin?std::min(totalFrames,finish)-begin:0;
    XAUDIO2_BUFFER b{};b.AudioBytes=static_cast<UINT32>(pcm_.size());b.pAudioData=reinterpret_cast<const BYTE*>(pcm_.data());b.PlayBegin=begin;b.PlayLength=length;b.Flags=XAUDIO2_END_OF_STREAM;
    if(FAILED(source_->SubmitSourceBuffer(&b))||FAILED(source_->Start())){error=L"Audio konnte nicht gestartet werden.";Stop();return false;}return true;
}
void AudioPlayer::Stop(){if(source_){source_->Stop(0);source_->FlushSourceBuffers();source_->DestroyVoice();source_=nullptr;}}

} // namespace kg
