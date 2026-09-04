#include "lyrics_document.hpp"
#include <cmath>

namespace kg {

static std::optional<double> OptNumber(const json::Value* v) {
    if (!v || !v->IsNumber()) return std::nullopt;
    const double n=v->AsNumber(); return std::isfinite(n)?std::optional<double>(n):std::nullopt;
}

bool LyricSegment::Suspicious() const {
    if (avgLogprob && *avgLogprob < -0.75) return true;
    if (noSpeechProb && avgLogprob && *noSpeechProb > 0.45 && *avgLogprob < -0.45) return true;
    if (compressionRatio && *compressionRatio > 2.4) return true;
    return false;
}
std::wstring LyricSegment::SuspicionReason() const {
    std::wstring r;
    auto add=[&](std::wstring_view x){if(!r.empty())r+=L", ";r+=x;};
    if(avgLogprob&&*avgLogprob<-0.75)add(L"niedrige Modell-Sicherheit");
    if(noSpeechProb&&avgLogprob&&*noSpeechProb>0.45&&*avgLogprob<-0.45)add(L"wenig Vocal-Anteil");
    if(compressionRatio&&*compressionRatio>2.4)add(L"ungewöhnliche Wiederholung");
    return r;
}

bool LyricsDocument::Load(const fs::path& jsonPath,const fs::path& inputRoot,std::wstring& error) {
    try {
        jsonPath_=jsonPath; root_=json::Parse(ReadUtf8File(jsonPath));
        const auto* source=root_.Find("source"); const auto* trans=root_.Find("transcription");
        if(!source||!trans||!source->IsObject()||!trans->IsObject()) throw std::runtime_error("Lyrics-JSON ist unvollständig.");
        std::string sourceFile; if(auto* v=source->Find("file"))sourceFile=v->AsString();
        std::string relative; if(auto* v=source->Find("relative_file"))relative=v->AsString();
        if(!sourceFile.empty()) audioPath_=Utf8ToWide(sourceFile);
        if((audioPath_.empty()||!fs::exists(audioPath_))&&!relative.empty())audioPath_=inputRoot/Utf8ToWide(relative);
        relativeName_=!relative.empty()?Utf8ToWide(relative):audioPath_.filename().wstring();
        duration_=trans->Find("duration_seconds")?trans->Find("duration_seconds")->AsNumber():0.0;
        segments_.clear();
        if(auto* arr=trans->Find("segments");arr&&arr->IsArray()){
            for(const auto& item:arr->AsArray()){
                if(!item.IsObject())continue; LyricSegment s;
                if(auto*v=item.Find("start"))s.start=v->AsNumber(); if(auto*v=item.Find("end"))s.end=v->AsNumber(); if(auto*v=item.Find("text"))s.text=Utf8ToWide(v->AsString());
                s.avgLogprob=OptNumber(item.Find("avg_logprob"));s.noSpeechProb=OptNumber(item.Find("no_speech_prob"));s.compressionRatio=OptNumber(item.Find("compression_ratio"));
                segments_.push_back(std::move(s));
            }
        }
        auto name=jsonPath_.filename().wstring(); const std::wstring suffix=L".lyrics.json";
        if(name.size()<suffix.size()||name.substr(name.size()-suffix.size())!=suffix)throw std::runtime_error("Dateiname ist keine .lyrics.json-Datei.");
        const auto stem=name.substr(0,name.size()-suffix.size()); const auto dir=jsonPath_.parent_path();
        txtPath_=dir/(stem+L".lyrics.txt"); lrcPath_=dir/(stem+L".lyrics.lrc"); srtPath_=dir/(stem+L".lyrics.srt");
        return true;
    } catch(const std::exception& e){error=Utf8ToWide(e.what());return false;}
}

static std::string LrcTime(double s){long long cs=static_cast<long long>(std::max(0.0,s)*100+0.5);long long m=cs/6000,sec=(cs/100)%60,c=cs%100;char b[32];sprintf_s(b,"%02lld:%02lld.%02lld",m,sec,c);return b;}
static std::string SrtTime(double s){long long ms=static_cast<long long>(std::max(0.0,s)*1000+0.5);long long h=ms/3600000,m=(ms/60000)%60,sec=(ms/1000)%60,x=ms%1000;char b[40];sprintf_s(b,"%02lld:%02lld:%02lld,%03lld",h,m,sec,x);return b;}

bool LyricsDocument::Save(std::wstring& error){
    try{
        std::string txt,lrc,srt;
        for(size_t i=0;i<segments_.size();++i){const auto&t=WideToUtf8(segments_[i].text);txt+=t+"\n";lrc+="["+LrcTime(segments_[i].start)+"]"+t+"\n";
            srt+=std::to_string(i+1)+"\n"+SrtTime(segments_[i].start)+" --> "+SrtTime(segments_[i].end)+"\n"+t+"\n\n";}
        AtomicWriteUtf8(txtPath_,txt);AtomicWriteUtf8(lrcPath_,lrc);AtomicWriteUtf8(srtPath_,srt);
        auto& trans=root_["transcription"];trans["text"]=txt.empty()?std::string{}:txt.substr(0,txt.size()-1);json::Value::Array arr;
        for(const auto&s:segments_){json::Value::Object o;o["start"]=s.start;o["end"]=s.end;o["text"]=WideToUtf8(s.text);if(s.avgLogprob)o["avg_logprob"]=*s.avgLogprob;if(s.noSpeechProb)o["no_speech_prob"]=*s.noSpeechProb;if(s.compressionRatio)o["compression_ratio"]=*s.compressionRatio;arr.emplace_back(std::move(o));}
        trans["segments"]=json::Value(std::move(arr));root_["editor"]["manually_reviewed"]=true;
        const auto now=std::chrono::system_clock::now(); const auto tt=std::chrono::system_clock::to_time_t(now); std::tm tm{};localtime_s(&tm,&tt);char b[64];strftime(b,sizeof(b),"%Y-%m-%dT%H:%M:%S",&tm);root_["editor"]["last_saved_local"]=std::string(b);
        AtomicWriteUtf8(jsonPath_,json::Dump(root_,2)+"\n");for(auto&s:segments_)s.edited=false;return true;
    }catch(const std::exception&e){error=Utf8ToWide(e.what());return false;}
}
bool LyricsDocument::Dirty()const noexcept{return std::any_of(segments_.begin(),segments_.end(),[](const auto&s){return s.edited;});}
std::vector<fs::path> DiscoverLyricsJson(const fs::path& root){std::vector<fs::path> out;std::error_code ec;if(!fs::exists(root,ec))return out;for(fs::recursive_directory_iterator it(root,fs::directory_options::skip_permission_denied,ec),end;it!=end;it.increment(ec)){if(ec){ec.clear();continue;}if(!it->is_regular_file(ec))continue;auto n=it->path().filename().wstring();if(n.size()>=12&&n.ends_with(L".lyrics.json"))out.push_back(it->path());}std::sort(out.begin(),out.end());return out;}

} // namespace kg
