#include "audio_player.hpp"
#include "audio_analysis.hpp"
#include "common.hpp"
#include "image_analysis.hpp"
#include "job_controller.hpp"
#include "lyrics_document.hpp"
#include "mini_json.hpp"
#include "video_export_controller.hpp"
#include "resource.h"
#include "production_planner.hpp"
#include "song_structure.hpp"
#include "video_renderer.hpp"
#include "visual_timeline.hpp"
#include "tatarus_visual_brain.hpp"

#include <commdlg.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <cmath>
#include <deque>
#include <memory>
#include <iterator>
#include <set>

using namespace kg;

namespace {

constexpr wchar_t kClassName[] = L"KlanggeistLyricsStudioWindow";
constexpr int kEditId = 1001;

enum class Page { Transcribe, Editor, VideoExport, Settings };
enum class JobStatus { Queued, Running, Done, Skipped, Error };
struct Job { fs::path path; fs::path inputRoot; JobStatus status{JobStatus::Queued}; float progress{}; std::wstring message; };

struct Theme {
    D2D1_COLOR_F bg{0.025f,0.028f,0.034f,1};
    D2D1_COLOR_F panel{0.055f,0.060f,0.071f,0.96f};
    D2D1_COLOR_F panel2{0.080f,0.086f,0.100f,1};
    D2D1_COLOR_F line{0.18f,0.20f,0.24f,1};
    D2D1_COLOR_F text{0.91f,0.93f,0.96f,1};
    D2D1_COLOR_F muted{0.52f,0.56f,0.62f,1};
    D2D1_COLOR_F cyan{0.12f,0.78f,0.92f,1};
    D2D1_COLOR_F violet{0.55f,0.30f,0.95f,1};
    D2D1_COLOR_F danger{0.95f,0.26f,0.36f,1};
    D2D1_COLOR_F warn{0.95f,0.69f,0.20f,1};
    D2D1_COLOR_F ok{0.24f,0.82f,0.54f,1};
};

class App {
public:
    explicit App(HINSTANCE inst) : inst_(inst), worker_(nullptr) {}
    int Run();
private:
    static LRESULT CALLBACK WndProc(HWND,UINT,WPARAM,LPARAM);
    LRESULT Handle(UINT,WPARAM,LPARAM);
    bool Create(); void CreateDevice(); void DiscardDevice(); void Draw(); void Resize();
    void DrawHeader(); void DrawNav(); void DrawTranscribe(); void DrawEditor(); void DrawVideoExport(); void DrawSettings(); void DrawFooter();
    void Fill(FRect,D2D1_COLOR_F,float radius=8); void Stroke(FRect,D2D1_COLOR_F,float width=1,float radius=8); void Text(std::wstring_view,FRect,float,D2D1_COLOR_F,DWRITE_TEXT_ALIGNMENT=DWRITE_TEXT_ALIGNMENT_LEADING,DWRITE_FONT_WEIGHT=DWRITE_FONT_WEIGHT_NORMAL);
    void Button(std::wstring_view,FRect,bool accent=false,bool danger=false); void Chip(std::wstring_view,FRect,bool active=false); void Progress(FRect,float,D2D1_COLOR_F);
    void OnClick(float x,float y); void OnWheel(short delta,float x,float y); void OnKey(WPARAM key);
    std::optional<fs::path> PickAudioFile(); std::optional<fs::path> PickCoverFile(); std::vector<fs::path> PickImageFiles(); std::optional<fs::path> PickFolder(std::wstring_view title);
    void AddFile(); void AddFolder(); void ScanFolder(const fs::path& root); void ChooseOutput(); void StartQueue(); void StopQueue(); void DispatchNext(); void OnWorker(WorkerEvent*); void AddLog(std::wstring);
    void RefreshLyrics(bool revealNewest=false); void SelectSong(size_t); void SelectSegment(size_t); void CommitEdit(); void SaveDocument(); void PlaySelected(bool before=false); void PlayCursor(); void StopAudio(); void UpdateEditPlacement(); void NextSuspicious();
    void OpenVideoExportForCurrentSong(); void ChooseVideoCover(); void ChooseVideoAlbumCover(); void ChooseVideoOutput(); void StartVideoExport(); void CancelVideoExport(); void OnExport(ExportProgress*);
    void StartVideoPreview(); void StopVideoPreview(); void UpdateVideoPreview(); void AnalyzeVideoAudio(); void AnalyzeVideoImages(); void ApplySmartVisualTimeline(); void ApplyTatarusVisualTimeline(); void ApplyTatarusProduction(); void TrainTatarusBoundary(double seconds); void TrainTatarusStyle(double seconds, bool toggleTransition); void TrainTatarusImage(double seconds); fs::path DiscoverCover(const fs::path& audio,const fs::path& lyrics) const;
    bool PersistTatarusBrain(std::wstring_view context);
    void LoadSettings(); void SaveSettings();

    HINSTANCE inst_{}; HWND hwnd_{},edit_{}; HFONT editFont_{}; float dpiScale_{1};
    ComPtr<ID2D1Factory> d2dFactory_; ComPtr<IDWriteFactory> dwFactory_; ComPtr<ID2D1HwndRenderTarget> rt_; ComPtr<ID2D1SolidColorBrush> brush_; Theme th_;
    Page page_{Page::Transcribe};
    FRect navTrans_,navEdit_,navVideo_,navSettings_,btnFile_,btnFolder_,btnOutput_,btnStart_,btnStop_,btnClear_,modelChip_,deviceChip_,computeChip_,languageChip_,watchChip_,overwriteChip_;
    FRect editorVideo_,editorRefresh_,editorSave_,editorApply_,editorApplyNext_,editorPlay_,editorBefore_,editorStop_,editorNext_,editorSuspicious_,timelineRect_;
    FRect videoPreviewRect_,videoChooseCover_,videoChooseAlbumCover_,videoUseAllImagesChip_,videoChooseOutput_,videoSmartCuts_,videoTatarusCuts_,videoTatarusProduce_,videoExport_,videoCancel_,videoPreviewPlay_,videoPreviewStop_,videoTimeline_;
    std::deque<Job> jobs_; int activeJob_{-1}; int queueScroll_{}; std::wstring outputRoot_; std::wstring model_{L"large-v3"},device_{L"cpu"},compute_{L"int8"},language_{L"auto"}; bool watchEnabled_{true}, overwrite_{}; std::optional<fs::path> watchedRoot_;
    std::vector<std::wstring> log_; std::unique_ptr<JobController> worker_; bool backendReady_{};
    std::vector<fs::path> lyricFiles_; std::unique_ptr<LyricsDocument> doc_; int songIndex_{-1},segmentIndex_{-1},songScroll_{},segmentScroll_{}; bool onlySuspicious_{}; double cursorSeconds_{}; AudioPlayer audio_;
    AudioAnalysis videoAudioAnalysis_;
    SongStructure videoSongStructure_;
    TatarusVisualBrain tatarusVisualBrain_;
    std::wstring coverRoot_,videoOutputRoot_; fs::path videoAudioPath_,videoLyricsPath_,videoCoverPath_,videoAlbumCoverPath_,videoOutputPath_; std::vector<fs::path> videoImages_; std::vector<VisualImageProfile> videoImageProfiles_; VisualTimeline videoVisualTimeline_; bool videoUseAllImages_{true}; double videoDuration_{},videoPreviewSeconds_{};
    VideoExportPreset videoPreset_{KlanggeistDefaultPreset()}; std::unique_ptr<VideoExportController> videoExporter_; ExportState videoExportState_{ExportState::Idle}; double videoExportProgress_{}; std::wstring videoExportMessage_{L"Bereit"};
    std::unique_ptr<VideoRenderer> previewRenderer_; ComPtr<ID2D1Bitmap> videoPreviewBitmap_; fs::path previewLoadedCover_; std::uint32_t previewWidth_{},previewHeight_{}; bool videoPreviewDirty_{true},videoPreviewPlaying_{}; double videoPreviewStartSeconds_{}; std::chrono::steady_clock::time_point videoPreviewStarted_{};
};

void App::Fill(FRect r,D2D1_COLOR_F c,float radius){brush_->SetColor(c);rt_->FillRoundedRectangle(D2D1::RoundedRect(r.D2D(),radius,radius),brush_.Get());}
void App::Stroke(FRect r,D2D1_COLOR_F c,float width,float radius){brush_->SetColor(c);rt_->DrawRoundedRectangle(D2D1::RoundedRect(r.D2D(),radius,radius),brush_.Get(),width);}
void App::Text(std::wstring_view s,FRect r,float size,D2D1_COLOR_F c,DWRITE_TEXT_ALIGNMENT align,DWRITE_FONT_WEIGHT weight){ComPtr<IDWriteTextFormat> f;dwFactory_->CreateTextFormat(L"Segoe UI",nullptr,weight,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"de-de",f.GetAddressOf());f->SetTextAlignment(align);f->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);brush_->SetColor(c);rt_->DrawTextW(s.data(),static_cast<UINT32>(s.size()),f.Get(),r.D2D(),brush_.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);}
void App::Button(std::wstring_view s,FRect r,bool accent,bool danger){auto c=danger?D2D1::ColorF(0.28f,0.07f,0.10f,1):accent?D2D1::ColorF(0.05f,0.22f,0.27f,1):th_.panel2;Fill(r,c,7);Stroke(r,accent?th_.cyan:danger?th_.danger:th_.line,1,7);Text(s,r,13,accent?th_.cyan:danger?th_.danger:th_.text,DWRITE_TEXT_ALIGNMENT_CENTER,DWRITE_FONT_WEIGHT_SEMI_BOLD);}
void App::Chip(std::wstring_view s,FRect r,bool active){Fill(r,active?D2D1::ColorF(0.11f,0.17f,0.20f,1):th_.panel2,12);Stroke(r,active?th_.cyan:th_.line,1,12);Text(s,r,12,active?th_.cyan:th_.muted,DWRITE_TEXT_ALIGNMENT_CENTER,DWRITE_FONT_WEIGHT_SEMI_BOLD);}
void App::Progress(FRect r,float p,D2D1_COLOR_F c){Fill(r,th_.panel2,3);FRect f=r;f.r=f.l+(f.r-f.l)*Clamp01(p);Fill(f,c,3);}

bool App::Create(){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);OleInitialize(nullptr);
    WNDCLASSEXW wc{sizeof(wc)};wc.lpfnWndProc=WndProc;wc.hInstance=inst_;wc.lpszClassName=kClassName;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(inst_,MAKEINTRESOURCEW(IDI_APP_ICON));wc.hIconSm=static_cast<HICON>(LoadImageW(inst_,MAKEINTRESOURCEW(IDI_APP_ICON),IMAGE_ICON,16,16,LR_DEFAULTCOLOR));if(!wc.hIcon)wc.hIcon=LoadIcon(nullptr,IDI_APPLICATION);if(!wc.hIconSm)wc.hIconSm=wc.hIcon;RegisterClassExW(&wc);
    hwnd_=CreateWindowExW(0,kClassName,L"Klanggeist Lyrics Studio",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1500,900,nullptr,nullptr,inst_,this);if(!hwnd_)return false;
    edit_=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN|WS_VSCROLL,0,0,0,0,hwnd_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(kEditId)),inst_,nullptr);
    editFont_=CreateFontW(-19,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");SendMessageW(edit_,WM_SETFONT,reinterpret_cast<WPARAM>(editFont_),TRUE);
    worker_=std::make_unique<JobController>(hwnd_);videoExporter_=std::make_unique<VideoExportController>(hwnd_);LoadSettings();{std::wstring brainError;if(!tatarusVisualBrain_.Load(fs::path(ExeDirectory())/L"tatarus_visual_brain.json",brainError)){AddLog(brainError.empty()?L"TATARUS Visual Brain konnte nicht geladen werden.":brainError);}}SetTimer(hwnd_,42,10'000,nullptr);SetTimer(hwnd_,43,33,nullptr);ShowWindow(hwnd_,SW_SHOW);UpdateWindow(hwnd_);return true;
}
int App::Run(){if(!Create())return 1;MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return static_cast<int>(msg.wParam);}
LRESULT CALLBACK App::WndProc(HWND h,UINT m,WPARAM w,LPARAM l){App*self=reinterpret_cast<App*>(GetWindowLongPtrW(h,GWLP_USERDATA));if(m==WM_NCCREATE){self=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);self->hwnd_=h;SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}return self?self->Handle(m,w,l):DefWindowProcW(h,m,w,l);}

LRESULT App::Handle(UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(hwnd_,&ps);Draw();EndPaint(hwnd_,&ps);return 0;}
    case WM_SIZE:Resize();UpdateEditPlacement();videoPreviewDirty_=true;return 0;
    case WM_GETMINMAXINFO:{auto*mm=reinterpret_cast<MINMAXINFO*>(l);mm->ptMinTrackSize={1200,820};return 0;}
    case WM_DPICHANGED:dpiScale_=HIWORD(w)/96.0f;videoPreviewDirty_=true;return 0;
    case WM_LBUTTONDOWN:OnClick(GET_X_LPARAM(l)/dpiScale_,GET_Y_LPARAM(l)/dpiScale_);return 0;
    case WM_MOUSEWHEEL:{POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(hwnd_,&p);OnWheel(GET_WHEEL_DELTA_WPARAM(w),p.x/dpiScale_,p.y/dpiScale_);return 0;}
    case WM_RBUTTONDOWN:{const float x=GET_X_LPARAM(l)/dpiScale_,y=GET_Y_LPARAM(l)/dpiScale_;if(page_==Page::VideoExport&&videoTimeline_.Contains(x,y)&&videoDuration_>0){const double clicked=videoDuration_*std::clamp((x-videoTimeline_.l)/(videoTimeline_.r-videoTimeline_.l),0.f,1.f);TrainTatarusImage(clicked);}return 0;}
    case WM_KEYDOWN:OnKey(w);return 0;
    case WM_TIMER:
        if(w==42&&watchEnabled_&&watchedRoot_){const auto before=jobs_.size();ScanFolder(*watchedRoot_);if(jobs_.size()>before){AddLog(L"Watch: neue Audiodatei(en) erkannt.");if(worker_&&worker_->Running()&&backendReady_&&activeJob_<0)DispatchNext();else if(!worker_||!worker_->Running())StartQueue();}}
        if(w==43&&videoPreviewPlaying_){const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-videoPreviewStarted_).count();videoPreviewSeconds_=videoPreviewStartSeconds_+elapsed;if(videoPreviewSeconds_>=videoDuration_){videoPreviewSeconds_=videoDuration_;StopVideoPreview();}videoPreviewDirty_=true;InvalidateRect(hwnd_,nullptr,FALSE);}
        return 0;
    case WM_KG_WORKER_EVENT:OnWorker(reinterpret_cast<WorkerEvent*>(l));return 0;
    case WM_KG_EXPORT_EVENT:OnExport(reinterpret_cast<ExportProgress*>(l));return 0;
    case WM_CTLCOLOREDIT:{HDC dc=reinterpret_cast<HDC>(w);SetTextColor(dc,RGB(235,238,244));SetBkColor(dc,RGB(21,23,28));static HBRUSH b=CreateSolidBrush(RGB(21,23,28));return reinterpret_cast<LRESULT>(b);}
    case WM_DESTROY:SaveSettings();PersistTatarusBrain(L"Programmende");if(worker_)worker_->Stop();if(videoExporter_)videoExporter_->Cancel();audio_.Stop();if(editFont_)DeleteObject(editFont_);DiscardDevice();OleUninitialize();PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(hwnd_,m,w,l);
}
void App::CreateDevice(){if(!d2dFactory_)D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,d2dFactory_.GetAddressOf());if(!dwFactory_)DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(dwFactory_.GetAddressOf()));if(!rt_){RECT rc;GetClientRect(hwnd_,&rc);D2D1_SIZE_U sz{static_cast<UINT32>(rc.right),static_cast<UINT32>(rc.bottom)};d2dFactory_->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),D2D1::HwndRenderTargetProperties(hwnd_,sz),rt_.GetAddressOf());if(rt_)rt_->CreateSolidColorBrush(th_.text,brush_.GetAddressOf());}}
void App::DiscardDevice(){videoPreviewBitmap_.Reset();videoPreviewDirty_=true;brush_.Reset();rt_.Reset();}
void App::Resize(){if(rt_){RECT rc;GetClientRect(hwnd_,&rc);rt_->Resize(D2D1::SizeU(rc.right,rc.bottom));}InvalidateRect(hwnd_,nullptr,FALSE);}

void App::Draw(){CreateDevice();if(!rt_)return;RECT rc;GetClientRect(hwnd_,&rc);dpiScale_=GetDpiForWindow(hwnd_)/96.0f;rt_->SetDpi(96*dpiScale_,96*dpiScale_);const float W=rc.right/dpiScale_;rt_->BeginDraw();rt_->Clear(th_.bg);
    // dezente Klangwelle im Hintergrund
    brush_->SetColor(D2D1::ColorF(0.07f,0.14f,0.17f,0.45f));for(int i=0;i<80;++i){float x=210.f+i*(W-230.f)/80.f;float amp=8.f+18.f*std::abs(std::sin(i*0.37f));rt_->DrawLine(D2D1::Point2F(x,93-amp),D2D1::Point2F(x,93+amp),brush_.Get(),1);}
    DrawHeader();DrawNav();if(page_==Page::Transcribe)DrawTranscribe();else if(page_==Page::Editor)DrawEditor();else if(page_==Page::VideoExport)DrawVideoExport();else DrawSettings();DrawFooter();HRESULT hr=rt_->EndDraw();if(hr==D2DERR_RECREATE_TARGET)DiscardDevice();}
void App::DrawHeader(){RECT rc;GetClientRect(hwnd_,&rc);float W=rc.right/dpiScale_;Fill({18,16,W-18,78},D2D1::ColorF(0.04f,0.045f,0.054f,0.98f),12);Stroke({18,16,W-18,78},th_.line,1,12);Text(L"KG",{35,23,92,69},26,th_.cyan,DWRITE_TEXT_ALIGNMENT_CENTER,DWRITE_FONT_WEIGHT_BOLD);Text(L"KLANGGEIST",{100,22,300,49},21,th_.text,DWRITE_TEXT_ALIGNMENT_LEADING,DWRITE_FONT_WEIGHT_BOLD);Text(L"LYRICS STUDIO // LOCAL WHISPER LAB",{100,48,420,68},10,th_.muted);Text(L"Gedanke → Klang → Text",{W-330,25,W-40,65},13,th_.muted,DWRITE_TEXT_ALIGNMENT_TRAILING,DWRITE_FONT_WEIGHT_SEMI_BOLD);}
void App::DrawNav(){RECT rc;GetClientRect(hwnd_,&rc);float H=rc.bottom/dpiScale_;Fill({18,92,186,H-42},th_.panel,12);navTrans_={30,112,174,154};navEdit_={30,163,174,205};navVideo_={30,214,174,256};navSettings_={30,265,174,307};auto nav=[&](std::wstring_view t,FRect r,Page p){if(page_==p){Fill(r,D2D1::ColorF(0.06f,0.20f,0.24f,1),8);Stroke(r,th_.cyan,1,8);}Text(t,r,13,page_==p?th_.cyan:th_.muted,DWRITE_TEXT_ALIGNMENT_CENTER,DWRITE_FONT_WEIGHT_SEMI_BOLD);};nav(L"TRANSKRIBIEREN",navTrans_,Page::Transcribe);nav(L"LYRICS EDITOR",navEdit_,Page::Editor);nav(L"VIDEO EXPORT",navVideo_,Page::VideoExport);nav(L"SETTINGS",navSettings_,Page::Settings);std::wstring backend=L"OFFLINE";D2D1_COLOR_F statusColor=th_.muted;if(backendReady_){backend=L"WHISPER BEREIT";statusColor=th_.ok;}else if(worker_&&worker_->Running()){backend=L"MODELL LÄDT";statusColor=th_.warn;}Text(backend,{30,H-105,174,H-80},11,statusColor,DWRITE_TEXT_ALIGNMENT_CENTER,DWRITE_FONT_WEIGHT_BOLD);Text(L"Native Win32 UI",{30,H-78,174,H-55},10,th_.muted,DWRITE_TEXT_ALIGNMENT_CENTER);}
void App::DrawTranscribe(){RECT rc;GetClientRect(hwnd_,&rc);float W=rc.right/dpiScale_,H=rc.bottom/dpiScale_;float x=204;
    Text(L"WHISPER TRANSCRIPTION",{x,96,W-30,126},18,th_.text,DWRITE_TEXT_ALIGNMENT_LEADING,DWRITE_FONT_WEIGHT_BOLD);Text(L"Einzeldatei oder kompletter Ordner – rekursiv, automatisch, wiederaufnehmbar.",{x,124,W-30,150},11,th_.muted);
    btnFile_={x,162,x+145,202};btnFolder_={x+157,162,x+315,202};btnOutput_={x+327,162,x+495,202};btnStart_={W-300,162,W-180,202};btnStop_={W-168,162,W-50,202};Button(L"+ DATEI",btnFile_);Button(L"+ ORDNER",btnFolder_);Button(L"AUSGABE",btnOutput_);Button(L"START",btnStart_,true);Button(L"STOP",btnStop_,false,true);
    modelChip_={x,215,x+150,244};deviceChip_={x+160,215,x+275,244};computeChip_={x+285,215,x+405,244};languageChip_={x+415,215,x+535,244};watchChip_={x+545,215,x+660,244};overwriteChip_={x+670,215,x+820,244};Chip(L"MODEL  "+model_,modelChip_,true);Chip(L"DEVICE  "+device_,deviceChip_);Chip(L"PRECISION  "+compute_,computeChip_);Chip(L"LANG  "+language_,languageChip_);Chip(watchEnabled_?L"WATCH  ON":L"WATCH  OFF",watchChip_,watchEnabled_);Chip(overwrite_?L"OVERWRITE":L"SKIP AKTUELL",overwriteChip_,overwrite_);
    Text(L"Ausgabe: "+outputRoot_,{x+830,210,W-40,248},10,th_.muted,DWRITE_TEXT_ALIGNMENT_TRAILING);
    Fill({x,260,W-30,H-122},th_.panel,10);Stroke({x,260,W-30,H-122},th_.line,1,10);Text(L"QUEUE",{x+16,270,x+100,296},11,th_.muted,DWRITE_TEXT_ALIGNMENT_LEADING,DWRITE_FONT_WEIGHT_BOLD);Text(L"STATUS",{W-330,270,W-250,296},10,th_.muted,DWRITE_TEXT_ALIGNMENT_CENTER);Text(L"PROGRESS",{W-225,270,W-55,296},10,th_.muted,DWRITE_TEXT_ALIGNMENT_CENTER);
    const float rowH=46;int visible=std::max(1,int((H-430)/rowH));int start=std::clamp(queueScroll_,0,std::max(0,int(jobs_.size())-visible));for(int n=0;n<visible&&start+n<int(jobs_.size());++n){int i=start+n;const auto&j=jobs_[i];float y=302+n*rowH;Fill({x+10,y,W-40,y+39},i==activeJob_?D2D1::ColorF(0.07f,0.12f,0.14f,1):th_.panel2,6);Text(j.path.filename().wstring(),{x+24,y,W-350,y+39},12,th_.text);std::wstring st=L"WARTET";D2D1_COLOR_F sc=th_.muted;if(j.status==JobStatus::Running){st=L"LÄUFT";sc=th_.cyan;}else if(j.status==JobStatus::Done){st=L"FERTIG";sc=th_.ok;}else if(j.status==JobStatus::Skipped){st=L"SKIP";sc=th_.warn;}else if(j.status==JobStatus::Error){st=L"FEHLER";sc=th_.danger;}Text(st,{W-335,y,W-245,y+39},10,sc,DWRITE_TEXT_ALIGNMENT_CENTER,DWRITE_FONT_WEIGHT_BOLD);Progress({W-225,y+16,W-55,y+23},j.progress,j.status==JobStatus::Error?th_.danger:th_.cyan);}
    btnClear_={x,H-108,x+120,H-73};Button(L"QUEUE LEEREN",btnClear_);std::wstring summary=L"Jobs: "+std::to_wstring(jobs_.size());if(activeJob_>=0)summary+=L"   //   aktiv: "+jobs_[activeJob_].path.filename().wstring();Text(summary,{x+140,H-110,W-40,H-72},11,th_.muted,DWRITE_TEXT_ALIGNMENT_TRAILING);
}
void App::DrawEditor(){RECT rc;GetClientRect(hwnd_,&rc);float W=rc.right/dpiScale_,H=rc.bottom/dpiScale_;float x=204;Text(L"LYRICS REVIEW",{x,96,W-30,126},18,th_.text,DWRITE_TEXT_ALIGNMENT_LEADING,DWRITE_FONT_WEIGHT_BOLD);Text(L"Segment anklicken → Stelle hören → Text korrigieren → TXT/LRC/SRT/JSON gemeinsam speichern.",{x,124,W-30,150},11,th_.muted);
    editorVideo_={W-466,105,W-306,140};editorRefresh_={W-296,105,W-176,140};editorSave_={W-166,105,W-46,140};Button(L"VIDEO ERSTELLEN",editorVideo_);Button(L"REFRESH",editorRefresh_);Button(L"SPEICHERN",editorSave_,true);
    float listW=250,segW=std::max(400.f,(W-x-listW-390));FRect songs{x,162,x+listW,H-78};FRect segs{x+listW+12,162,x+listW+12+segW,H-78};FRect editp{segs.r+12,162,W-30,H-78};Fill(songs,th_.panel,10);Fill(segs,th_.panel,10);Fill(editp,th_.panel,10);Stroke(songs,th_.line,1,10);Stroke(segs,th_.line,1,10);Stroke(editp,th_.line,1,10);Text(L"SONGS ("+std::to_wstring(lyricFiles_.size())+L")",{songs.l+12,songs.t+8,songs.r-12,songs.t+34},10,th_.muted);Text(L"SEGMENTS",{segs.l+12,segs.t+8,segs.r-12,segs.t+34},10,th_.muted);
    int songVisible=std::max(1,int((songs.b-songs.t-48)/38));int ss=std::clamp(songScroll_,0,std::max(0,int(lyricFiles_.size())-songVisible));for(int n=0;n<songVisible&&ss+n<int(lyricFiles_.size());++n){int i=ss+n;float y=songs.t+38+n*38;FRect r{songs.l+8,y,songs.r-8,y+32};if(i==songIndex_)Fill(r,D2D1::ColorF(0.06f,0.20f,0.24f,1),6);auto name=lyricFiles_[i].filename().wstring();if(name.ends_with(L".lyrics.json"))name.resize(name.size()-12);Text(name,{r.l+8,r.t,r.r-6,r.b},11,i==songIndex_?th_.cyan:th_.text);}
    if(doc_){const auto&v=doc_->Segments();std::vector<int> indices;indices.reserve(v.size());for(int i=0;i<int(v.size());++i)if(!onlySuspicious_||v[i].Suspicious())indices.push_back(i);int visible=std::max(1,int((segs.b-segs.t-48)/42));int start=std::clamp(segmentScroll_,0,std::max(0,int(indices.size())-visible));for(int n=0;n<visible&&start+n<int(indices.size());++n){int i=indices[start+n];const auto&s=v[i];float y=segs.t+38+n*42;FRect r{segs.l+8,y,segs.r-8,y+36};if(i==segmentIndex_)Fill(r,D2D1::ColorF(0.07f,0.13f,0.16f,1),5);Text(FormatTime(s.start),{r.l+8,r.t,r.l+73,r.b},9,s.Suspicious()?th_.warn:th_.muted);Text(s.text,{r.l+80,r.t,r.r-8,r.b},10,i==segmentIndex_?th_.cyan:th_.text);}
        Text(doc_->RelativeName(),{editp.l+14,editp.t+10,editp.r-14,editp.t+38},12,th_.text,DWRITE_TEXT_ALIGNMENT_LEADING,DWRITE_FONT_WEIGHT_SEMI_BOLD);if(segmentIndex_>=0&&segmentIndex_<int(v.size())){const auto&s=v[segmentIndex_];Text(FormatTime(s.start,true)+L"  →  "+FormatTime(s.end,true),{editp.l+14,editp.t+38,editp.r-14,editp.t+64},10,th_.muted);if(s.Suspicious())Text(L"PRÜFEN: "+s.SuspicionReason(),{editp.l+14,editp.t+62,editp.r-14,editp.t+88},10,th_.warn);}
    } else Text(L"Noch keine Lyrics geladen.",{segs.l+20,segs.t+60,segs.r-20,segs.t+100},11,th_.muted);
    float bx=editp.l+14,by=editp.t+290;editorApply_={bx,by,bx+120,by+34};editorApplyNext_={bx+128,by,editp.r-14,by+34};Button(L"ÜBERNEHMEN",editorApply_);Button(L"ÜBERNEHMEN + NÄCHSTE",editorApplyNext_,true);
    by+=46;editorPlay_={bx,by,bx+105,by+34};editorBefore_={bx+113,by,bx+235,by+34};editorStop_={bx+243,by,bx+330,by+34};Button(L"▶ SEGMENT",editorPlay_,true);Button(L"▶ -5 SEK",editorBefore_);Button(L"■ STOP",editorStop_);
    by+=48;timelineRect_={bx,by,editp.r-14,by+18};Fill(timelineRect_,th_.panel2,5);if(doc_&&doc_->Duration()>0){float p=Clamp01(static_cast<float>(cursorSeconds_/doc_->Duration()));FRect f=timelineRect_;f.r=f.l+(f.r-f.l)*p;Fill(f,th_.violet,5);float px=timelineRect_.l+(timelineRect_.r-timelineRect_.l)*p;Fill({px-3,timelineRect_.t-5,px+3,timelineRect_.b+5},th_.cyan,3);}Text(doc_?FormatTime(cursorSeconds_):L"00:00",{timelineRect_.l,timelineRect_.b+5,timelineRect_.r,timelineRect_.b+29},10,th_.muted,DWRITE_TEXT_ALIGNMENT_TRAILING);
    by+=54;editorNext_={bx,by,bx+175,by+34};editorSuspicious_={bx+185,by,editp.r-14,by+34};Button(L"NÄCHSTE PRÜFSTELLE",editorNext_);Chip(onlySuspicious_?L"✓ NUR PRÜFSTELLEN":L"ALLE SEGMENTE",editorSuspicious_,onlySuspicious_);
    UpdateEditPlacement();
}
void App::DrawVideoExport(){
    RECT rc;GetClientRect(hwnd_,&rc);const float W=rc.right/dpiScale_,H=rc.bottom/dpiScale_;const float x=204;
    Text(L"VIDEO EXPORT",{x,96,W-30,126},18,th_.text,DWRITE_TEXT_ALIGNMENT_LEADING,DWRITE_FONT_WEIGHT_BOLD);
    Text(L"TATARUS v2.1.1 · FAST Render Engine · Diversity Planner · Visual Intelligence",{x,124,W-30,150},11,th_.muted);
    videoCancel_={W-328,105,W-208,140};videoExport_={W-198,105,W-46,140};Button(L"ABBRECHEN",videoCancel_,false,true);Button(L"EXPORTIEREN",videoExport_,true);

    const float sideWidth=420.0f;const float rightX=W-sideWidth-30.0f;const float availableW=rightX-x-18.0f;const float availableH=H-330.0f;
    const float previewW=std::max(320.0f,std::min(availableW,availableH*16.0f/9.0f));const float previewH=previewW*9.0f/16.0f;const float previewLeft=x+(availableW-previewW)*0.5f;
    videoPreviewRect_={previewLeft,162,previewLeft+previewW,162+previewH};Fill(videoPreviewRect_,D2D1::ColorF(0.015f,0.018f,0.023f,1),8);Stroke(videoPreviewRect_,th_.line,1,8);
    if(videoPreviewDirty_)UpdateVideoPreview();if(videoPreviewBitmap_)rt_->DrawBitmap(videoPreviewBitmap_.Get(),videoPreviewRect_.D2D(),1.0f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);else Text(videoImages_.empty()?L"BILDER WÄHLEN":L"VORSCHAU WIRD VORBEREITET",videoPreviewRect_,14,th_.muted,DWRITE_TEXT_ALIGNMENT_CENTER,DWRITE_FONT_WEIGHT_SEMI_BOLD);

    const float controlsY=videoPreviewRect_.b+18;videoPreviewPlay_={videoPreviewRect_.l,controlsY,videoPreviewRect_.l+105,controlsY+34};videoPreviewStop_={videoPreviewRect_.l+113,controlsY,videoPreviewRect_.l+205,controlsY+34};Button(videoPreviewPlaying_?L"▶ LÄUFT":L"▶ PLAY",videoPreviewPlay_,videoPreviewPlaying_);Button(L"■ STOP",videoPreviewStop_);
    videoTimeline_={videoPreviewRect_.l+220,controlsY+4,videoPreviewRect_.r,controlsY+29};Fill(videoTimeline_,th_.panel2,5);
    if(videoDuration_>0&&!videoAudioAnalysis_.Empty()){const float mid=(videoTimeline_.t+videoTimeline_.b)*0.5f;const float half=(videoTimeline_.b-videoTimeline_.t)*0.43f;const size_t bars=std::max<size_t>(1,static_cast<size_t>(videoTimeline_.r-videoTimeline_.l));for(size_t bx=0;bx<bars;++bx){const size_t idx=std::min(videoAudioAnalysis_.envelope.size()-1,bx*videoAudioAnalysis_.envelope.size()/bars);const float a=Clamp01(videoAudioAnalysis_.envelope[idx].rms*5.5f);const float px=videoTimeline_.l+static_cast<float>(bx);Fill({px,mid-half*a,px+1.0f,mid+half*a},D2D1::ColorF(th_.muted.r,th_.muted.g,th_.muted.b,0.55f),0);}}
    if(videoDuration_>0&&!videoSongStructure_.Empty()){for(const auto&section:videoSongStructure_.sections){const float sl=videoTimeline_.l+(videoTimeline_.r-videoTimeline_.l)*Clamp01(static_cast<float>(section.start/videoDuration_));const float sr=videoTimeline_.l+(videoTimeline_.r-videoTimeline_.l)*Clamp01(static_cast<float>(section.end/videoDuration_));const auto c=section.type==SongSectionType::Chorus?th_.violet:section.type==SongSectionType::Bridge?th_.warn:section.type==SongSectionType::Intro||section.type==SongSectionType::Outro?th_.muted:th_.cyan;Fill({sl,videoTimeline_.t,sr,videoTimeline_.t+2},c,0);}}
    if(videoDuration_>0&&!videoVisualTimeline_.Empty()){for(size_t i=0;i<videoVisualTimeline_.clips.size();++i){const auto&c=videoVisualTimeline_.clips[i];const float l=videoTimeline_.l+(videoTimeline_.r-videoTimeline_.l)*Clamp01(static_cast<float>(c.start/videoDuration_));const float r=videoTimeline_.l+(videoTimeline_.r-videoTimeline_.l)*Clamp01(static_cast<float>(c.end/videoDuration_));Fill({l+1,videoTimeline_.b-5,r-1,videoTimeline_.b-1},i%2==0?th_.cyan:th_.violet,2);}}
    if(videoDuration_>0&&!videoAudioAnalysis_.onsets.empty()){for(double t:videoAudioAnalysis_.onsets){const float px=videoTimeline_.l+(videoTimeline_.r-videoTimeline_.l)*Clamp01(static_cast<float>(t/videoDuration_));Fill({px,videoTimeline_.t+1,px+1,videoTimeline_.t+5},th_.warn,0);}}
    if(videoDuration_>0){const float p=Clamp01(static_cast<float>(videoPreviewSeconds_/videoDuration_));const float px=videoTimeline_.l+(videoTimeline_.r-videoTimeline_.l)*p;Fill({px-2,videoTimeline_.t-5,px+2,videoTimeline_.b+5},th_.text,2);}Text(FormatTime(videoPreviewSeconds_)+L" / "+FormatTime(videoDuration_),{videoTimeline_.l,controlsY+28,videoTimeline_.r,controlsY+52},10,th_.muted,DWRITE_TEXT_ALIGNMENT_TRAILING);

    FRect side{rightX,162,W-30,H-78};Fill(side,th_.panel,10);Stroke(side,th_.line,1,10);const float l=side.l+18,r=side.r-18;float sy=side.t+12;
    Text(L"SOURCE",{l,sy,r,sy+20},10,th_.muted,DWRITE_TEXT_ALIGNMENT_LEADING,DWRITE_FONT_WEIGHT_BOLD);sy+=24;
    Text(L"AUDIO",{l,sy,l+60,sy+20},9,th_.muted);Text(videoAudioPath_.empty()?L"—":videoAudioPath_.filename().wstring(),{l+64,sy,r,sy+20},10,th_.text);sy+=24;
    Text(L"LYRICS",{l,sy,l+60,sy+20},9,th_.muted);Text(videoLyricsPath_.empty()?L"—":videoLyricsPath_.filename().wstring(),{l+64,sy,r,sy+20},10,th_.text);sy+=28;
    const std::wstring coverName=videoAlbumCoverPath_.empty()?L"noch nicht festgelegt":videoAlbumCoverPath_.filename().wstring();
    Text(L"VISUALS",{l,sy,l+60,sy+20},9,th_.muted);Text(std::to_wstring(videoImages_.size())+L" Bilder",{l+64,sy,l+138,sy+20},10,videoImages_.empty()?th_.warn:th_.text);Text(L"★ "+coverName,{l+142,sy,r,sy+20},9,videoAlbumCoverPath_.empty()?th_.warn:th_.cyan);sy+=28;

    videoChooseCover_={l,sy,(l+r)*0.5f-5,sy+34};videoChooseAlbumCover_={(l+r)*0.5f+5,sy,r,sy+34};Button(L"BILDER WÄHLEN",videoChooseCover_);Button(L"★ ALBUMCOVER",videoChooseAlbumCover_,!videoAlbumCoverPath_.empty());sy+=40;
    videoUseAllImagesChip_={l,sy,r,sy+30};Chip(videoUseAllImages_?L"✓ ALLE BILDER MINDESTENS 1×":L"FREIE TATARUS-BILDWAHL",videoUseAllImagesChip_,videoUseAllImages_);sy+=36;
    const float mid=(l+r)*0.5f;videoSmartCuts_={l,sy,mid-5,sy+32};videoTatarusCuts_={mid+5,sy,r,sy+32};Button(L"⚡ SMART",videoSmartCuts_,!videoAudioAnalysis_.Empty());Button(L"🧠 TATARUS",videoTatarusCuts_,!videoAudioAnalysis_.Empty());sy+=38;
    videoTatarusProduce_={l,sy,r,sy+36};Button(L"TATARUS PRODUCE",videoTatarusProduce_,!videoSongStructure_.Empty());sy+=42;

    Text(L"ANALYSE",{l,sy,r,sy+20},9,th_.muted,DWRITE_TEXT_ALIGNMENT_LEADING,DWRITE_FONT_WEIGHT_BOLD);sy+=20;
    Text(videoAudioAnalysis_.Empty()?L"Audio Intelligence nicht verfügbar":L"Audio: "+std::to_wstring(videoAudioAnalysis_.onsets.size())+L" Onsets · "+std::to_wstring(videoAudioAnalysis_.pauses.size())+L" Pausen · "+std::to_wstring(videoSongStructure_.sections.size())+L" Bereiche",{l,sy,r,sy+20},9,videoAudioAnalysis_.Empty()?th_.warn:th_.ok);sy+=20;
    Text(L"Training: "+std::to_wstring(tatarusVisualBrain_.TrainingEvents())+L" Cut · "+std::to_wstring(tatarusVisualBrain_.StyleTrainingEvents())+L" Stil · "+std::to_wstring(tatarusVisualBrain_.ImageTrainingEvents())+L" Bild",{l,sy,r,sy+20},9,tatarusVisualBrain_.Trained()?th_.cyan:th_.muted);sy+=28;

    videoChooseOutput_={l,sy,r,sy+34};Button(L"VIDEO-ORDNER",videoChooseOutput_);sy+=38;
    Text(videoOutputPath_.empty()?L"Ziel: —":L"Ziel: "+videoOutputPath_.wstring(),{l,sy,r,sy+20},9,th_.muted);sy+=26;
    Text(L"PRESET",{l,sy,l+60,sy+20},9,th_.muted);Text(videoPreset_.name,{l+64,sy,r,sy+20},11,th_.cyan,DWRITE_TEXT_ALIGNMENT_LEADING,DWRITE_FONT_WEIGHT_SEMI_BOLD);sy+=26;
    Chip(L"MP4 · 1080P · 30 FPS",{l,sy,r,sy+30},true);sy+=36;
    Text(L"Timeline: Shift=Cut · Ctrl=Transition · Alt=Motion · Rechts=Bild",{l,sy,r,sy+20},8.5f,th_.muted);sy+=24;

    const float statusTop=std::max(sy+4,side.b-82);if(statusTop>sy+4)Stroke({l,sy,r,sy+1},th_.line,1,0);
    Text(videoExportMessage_,{l,statusTop,r,statusTop+36},9,videoExportState_==ExportState::Error?th_.danger:videoExportState_==ExportState::Done?th_.ok:th_.muted);
    Progress({l,side.b-36,r,side.b-28},static_cast<float>(videoExportProgress_),videoExportState_==ExportState::Error?th_.danger:th_.cyan);Text(std::to_wstring(static_cast<int>(videoExportProgress_*100.0))+L" %",{l,side.b-27,r,side.b-7},9,th_.muted,DWRITE_TEXT_ALIGNMENT_TRAILING);
}
void App::DrawSettings(){RECT rc;GetClientRect(hwnd_,&rc);float W=rc.right/dpiScale_;float x=204;Text(L"SETTINGS",{x,96,W-30,126},18,th_.text,DWRITE_TEXT_ALIGNMENT_LEADING,DWRITE_FONT_WEIGHT_BOLD);Fill({x,160,W-30,600},th_.panel,10);Stroke({x,160,W-30,600},th_.line,1,10);Text(L"WHISPER BACKEND",{x+24,180,x+250,210},11,th_.muted,DWRITE_TEXT_ALIGNMENT_LEADING,DWRITE_FONT_WEIGHT_BOLD);Text(L"Inference: faster-whisper / CTranslate2. UI und Audio sind rein native Windows-C++-Komponenten.",{x+24,218,W-60,248},12,th_.text);Text(L"Model",{x+24,280,x+130,310},11,th_.muted);Text(model_,{x+160,280,x+340,310},12,th_.cyan);Text(L"Device",{x+24,320,x+130,350},11,th_.muted);Text(device_,{x+160,320,x+340,350},12,th_.text);Text(L"Compute",{x+24,360,x+130,390},11,th_.muted);Text(compute_,{x+160,360,x+340,390},12,th_.text);Text(L"Language",{x+24,400,x+130,430},11,th_.muted);Text(language_,{x+160,400,x+340,430},12,th_.text);Text(L"Cache",{x+24,440,x+130,470},11,th_.muted);Text(fs::path(ExeDirectory()).append(L".hf\\hub").wstring(),{x+160,440,W-60,470},11,th_.muted);Text(L"Videoausgabe",{x+24,490,x+140,520},11,th_.muted);Text(videoOutputRoot_,{x+160,490,W-60,520},11,th_.text);Text(L"Cover-Suche",{x+24,530,x+140,560},11,th_.muted);Text(coverRoot_.empty()?L"Noch kein Cover-Ordner gespeichert":coverRoot_,{x+160,530,W-60,560},11,coverRoot_.empty()?th_.muted:th_.text);}
void App::DrawFooter(){RECT rc;GetClientRect(hwnd_,&rc);float W=rc.right/dpiScale_,H=rc.bottom/dpiScale_;Text(L"KLANGGEIST 2.1.1 // native C++20 // TATARUS FAST Render Engine // Whisper large-v3",{204,H-37,W-30,H-14},9,th_.muted,DWRITE_TEXT_ALIGNMENT_TRAILING);}

std::optional<fs::path> App::PickAudioFile(){ComPtr<IFileOpenDialog>d;if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(d.GetAddressOf()))))return{};COMDLG_FILTERSPEC f[]={{L"Audio / Video",L"*.mp3;*.wav;*.flac;*.m4a;*.aac;*.ogg;*.opus;*.wma;*.mp4;*.mkv;*.webm"},{L"Alle Dateien",L"*.*"}};d->SetFileTypes(2,f);if(FAILED(d->Show(hwnd_)))return{};ComPtr<IShellItem>item;d->GetResult(item.GetAddressOf());PWSTR p=nullptr;item->GetDisplayName(SIGDN_FILESYSPATH,&p);fs::path out=p;CoTaskMemFree(p);return out;}
std::optional<fs::path> App::PickCoverFile(){ComPtr<IFileOpenDialog>d;if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(d.GetAddressOf()))))return{};COMDLG_FILTERSPEC f[]={{L"Cover-Bilder",L"*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff;*.webp"},{L"Alle Dateien",L"*.*"}};d->SetFileTypes(2,f);d->SetTitle(L"Cover für das Lyrics-Video auswählen");if(FAILED(d->Show(hwnd_)))return{};ComPtr<IShellItem>item;d->GetResult(item.GetAddressOf());PWSTR p=nullptr;item->GetDisplayName(SIGDN_FILESYSPATH,&p);fs::path out=p;CoTaskMemFree(p);return out;}
std::vector<fs::path> App::PickImageFiles(){
    std::vector<fs::path> result;ComPtr<IFileOpenDialog>d;
    if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(d.GetAddressOf()))))return result;
    COMDLG_FILTERSPEC f[]={{L"Video-Bilder",L"*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff;*.webp"},{L"Alle Dateien",L"*.*"}};d->SetFileTypes(2,f);
    DWORD options{};d->GetOptions(&options);d->SetOptions(options|FOS_ALLOWMULTISELECT|FOS_FORCEFILESYSTEM);d->SetTitle(L"Bilder für die visuelle Timeline auswählen");
    if(FAILED(d->Show(hwnd_)))return result;ComPtr<IShellItemArray>items;if(FAILED(d->GetResults(items.GetAddressOf())))return result;
    DWORD count{};items->GetCount(&count);result.reserve(count);for(DWORD i=0;i<count;++i){ComPtr<IShellItem>item;if(FAILED(items->GetItemAt(i,item.GetAddressOf())))continue;PWSTR raw=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&raw))&&raw){result.emplace_back(raw);CoTaskMemFree(raw);}}
    return result;
}
std::optional<fs::path> App::PickFolder(std::wstring_view title){ComPtr<IFileOpenDialog>d;if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(d.GetAddressOf()))))return{};DWORD opt{};d->GetOptions(&opt);d->SetOptions(opt|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM);d->SetTitle(std::wstring(title).c_str());if(FAILED(d->Show(hwnd_)))return{};ComPtr<IShellItem>item;d->GetResult(item.GetAddressOf());PWSTR p=nullptr;item->GetDisplayName(SIGDN_FILESYSPATH,&p);fs::path out=p;CoTaskMemFree(p);return out;}
void App::AddFile(){if(auto p=PickAudioFile()){jobs_.push_back(Job{*p,p->parent_path()});InvalidateRect(hwnd_,nullptr,FALSE);}}
void App::AddFolder(){if(auto root=PickFolder(L"Musikordner auswählen")){watchedRoot_=*root;ScanFolder(*root);InvalidateRect(hwnd_,nullptr,FALSE);}}
void App::ScanFolder(const fs::path& root){std::error_code ec;for(fs::recursive_directory_iterator it(root,fs::directory_options::skip_permission_denied,ec),end;it!=end;it.increment(ec)){if(ec){ec.clear();continue;}if(!it->is_regular_file(ec)||!IsAudioExtension(it->path()))continue;const auto candidate=it->path();const bool known=std::any_of(jobs_.begin(),jobs_.end(),[&](const Job&j){std::error_code e;return fs::equivalent(j.path,candidate,e)||j.path==candidate;});if(!known)jobs_.push_back(Job{candidate,root});}}
void App::ChooseOutput(){if(auto p=PickFolder(L"Lyrics-Ausgabeordner auswählen")){outputRoot_=p->wstring();RefreshLyrics(true);InvalidateRect(hwnd_,nullptr,FALSE);}}
void App::StartQueue(){if(jobs_.empty())return;if(worker_&&worker_->Running())return;for(auto&j:jobs_)if(j.status==JobStatus::Error)j.status=JobStatus::Queued;std::wstring err;backendReady_=false;activeJob_=-1;if(!worker_->Start(ExeDirectory(),model_,device_,compute_,language_,err)){MessageBoxW(hwnd_,err.c_str(),L"Whisper Backend",MB_ICONERROR);return;}AddLog(L"Backend startet: "+model_+L" / "+device_+L" / "+compute_);InvalidateRect(hwnd_,nullptr,FALSE);}
void App::StopQueue(){if(worker_)worker_->Stop();backendReady_=false;if(activeJob_>=0&&activeJob_<int(jobs_.size())){jobs_[activeJob_].status=JobStatus::Error;jobs_[activeJob_].message=L"Abgebrochen";}activeJob_=-1;InvalidateRect(hwnd_,nullptr,FALSE);}
void App::DispatchNext(){if(!backendReady_||!worker_||!worker_->Running())return;for(int i=0;i<int(jobs_.size());++i){if(jobs_[i].status!=JobStatus::Queued)continue;activeJob_=i;jobs_[i].status=JobStatus::Running;jobs_[i].progress=0;json::Value::Object o;o["cmd"]="transcribe";o["path"]=WideToUtf8(jobs_[i].path.wstring());o["input_root"]=WideToUtf8(jobs_[i].inputRoot.wstring());o["output_root"]=WideToUtf8(outputRoot_);o["overwrite"]=overwrite_;std::wstring err;if(!worker_->Send(json::Value(std::move(o)),err)){jobs_[i].status=JobStatus::Error;jobs_[i].message=err;activeJob_=-1;}InvalidateRect(hwnd_,nullptr,FALSE);return;}AddLog(L"Queue abgeschlossen.");RefreshLyrics(true);}
void App::OnWorker(WorkerEvent* raw){std::unique_ptr<WorkerEvent>ev(raw);auto getS=[&](const char*k){auto*v=ev->payload.Find(k);return v?Utf8ToWide(v->AsString()):L"";};if(ev->type=="ready"){backendReady_=true;AddLog(L"Whisper bereit.");DispatchNext();}else if(ev->type=="loading")AddLog(L"Lade Modell …");else if(ev->type=="progress"&&activeJob_>=0){if(auto*v=ev->payload.Find("progress"))jobs_[activeJob_].progress=static_cast<float>(v->AsNumber());}else if((ev->type=="done"||ev->type=="skipped"||ev->type=="error")&&activeJob_>=0){auto&j=jobs_[activeJob_];if(ev->type=="done"){j.status=JobStatus::Done;j.progress=1;AddLog(L"Fertig: "+j.path.filename().wstring());}else if(ev->type=="skipped"){j.status=JobStatus::Skipped;j.progress=1;}else{j.status=JobStatus::Error;j.message=getS("message");AddLog(L"Fehler: "+j.message);}activeJob_=-1;DispatchNext();}else if(ev->type=="fatal"){AddLog(L"FATAL: "+getS("message"));MessageBoxW(hwnd_,getS("message").c_str(),L"Whisper",MB_ICONERROR);}else if(ev->type=="log")AddLog(getS("message"));InvalidateRect(hwnd_,nullptr,FALSE);}
void App::AddLog(std::wstring s){log_.push_back(std::move(s));if(log_.size()>100)log_.erase(log_.begin());}

void App::RefreshLyrics(bool revealNewest){
    std::optional<fs::path> selectedPath;
    if(songIndex_>=0&&songIndex_<int(lyricFiles_.size()))selectedPath=lyricFiles_[songIndex_];

    lyricFiles_=DiscoverLyricsJson(outputRoot_);
    songIndex_=-1;
    if(selectedPath){
        const auto selected=std::find(lyricFiles_.begin(),lyricFiles_.end(),*selectedPath);
        if(selected!=lyricFiles_.end())songIndex_=static_cast<int>(std::distance(lyricFiles_.begin(),selected));
        else{doc_.reset();segmentIndex_=-1;segmentScroll_=0;audio_.Stop();ShowWindow(edit_,SW_HIDE);}
    }

    if(revealNewest&&!lyricFiles_.empty()){
        auto newest=lyricFiles_.begin();std::error_code newestEc;auto newestTime=fs::last_write_time(*newest,newestEc);
        for(auto it=std::next(lyricFiles_.begin());it!=lyricFiles_.end();++it){
            std::error_code ec;const auto time=fs::last_write_time(*it,ec);
            if(!ec&&(newestEc||time>newestTime)){newest=it;newestTime=time;newestEc.clear();}
        }
        songScroll_=static_cast<int>(std::distance(lyricFiles_.begin(),newest));
    }
    songScroll_=std::clamp(songScroll_,0,std::max(0,int(lyricFiles_.size())-1));
    InvalidateRect(hwnd_,nullptr,FALSE);
}
void App::SelectSong(size_t idx){if(idx>=lyricFiles_.size())return;if(doc_&&doc_->Dirty()){if(MessageBoxW(hwnd_,L"Ungespeicherte Änderungen verwerfen?",L"Lyrics Editor",MB_YESNO|MB_ICONQUESTION)!=IDYES)return;}auto d=std::make_unique<LyricsDocument>();std::wstring err;fs::path inputRoot=lyricFiles_[idx].parent_path(); // source.file aus JSON hat Vorrang
    if(!d->Load(lyricFiles_[idx],inputRoot,err)){MessageBoxW(hwnd_,err.c_str(),L"Lyrics laden",MB_ICONERROR);return;}doc_=std::move(d);songIndex_=static_cast<int>(idx);segmentIndex_=-1;segmentScroll_=0;cursorSeconds_=0;audio_.Stop();ShowWindow(edit_,SW_HIDE);InvalidateRect(hwnd_,nullptr,FALSE);}
void App::SelectSegment(size_t idx){if(!doc_||idx>=doc_->Segments().size())return;CommitEdit();segmentIndex_=static_cast<int>(idx);const auto&s=doc_->Segments()[idx];cursorSeconds_=s.start;SetWindowTextW(edit_,s.text.c_str());ShowWindow(edit_,SW_SHOW);UpdateEditPlacement();SetFocus(edit_);InvalidateRect(hwnd_,nullptr,FALSE);}
void App::CommitEdit(){if(!doc_||segmentIndex_<0||segmentIndex_>=int(doc_->Segments().size()))return;int len=GetWindowTextLengthW(edit_);std::wstring t(static_cast<size_t>(len)+1,L'\0');GetWindowTextW(edit_,t.data(),len+1);t.resize(static_cast<size_t>(len));while(!t.empty()&&(t.back()==L'\r'||t.back()==L'\n'||t.back()==L' '))t.pop_back();auto&s=doc_->Segments()[segmentIndex_];if(t!=s.text){s.text=std::move(t);s.edited=true;}}
void App::SaveDocument(){if(!doc_)return;CommitEdit();std::wstring err;if(!doc_->Save(err))MessageBoxW(hwnd_,err.c_str(),L"Speichern",MB_ICONERROR);else AddLog(L"Lyrics gespeichert: "+doc_->RelativeName());InvalidateRect(hwnd_,nullptr,FALSE);}
void App::PlaySelected(bool before){if(!doc_||segmentIndex_<0)return;CommitEdit();std::wstring err;if(audio_.Path()!=doc_->AudioPath()){if(!audio_.Load(doc_->AudioPath(),err)){MessageBoxW(hwnd_,err.c_str(),L"Audio",MB_ICONERROR);return;}}const auto&s=doc_->Segments()[segmentIndex_];double start=before?std::max(0.0,s.start-5.0):std::max(0.0,s.start-0.65);double end=std::min(audio_.Duration(),s.end+0.65);audio_.Play(start,end,err);cursorSeconds_=start;InvalidateRect(hwnd_,nullptr,FALSE);}
void App::PlayCursor(){if(!doc_)return;std::wstring err;if(audio_.Path()!=doc_->AudioPath()&&!audio_.Load(doc_->AudioPath(),err)){MessageBoxW(hwnd_,err.c_str(),L"Audio",MB_ICONERROR);return;}audio_.Play(cursorSeconds_,std::min(audio_.Duration(),cursorSeconds_+10.0),err);}
void App::StopAudio(){audio_.Stop();}
void App::NextSuspicious(){if(!doc_)return;const auto&v=doc_->Segments();int start=segmentIndex_+1;for(int n=0;n<int(v.size());++n){int i=(start+n)%int(v.size());if(v[i].Suspicious()){SelectSegment(i);return;}}MessageBoxW(hwnd_,L"Keine automatisch markierte Prüfstelle gefunden.",L"Lyrics Editor",MB_OK|MB_ICONINFORMATION);}
fs::path App::DiscoverCover(const fs::path& audio,const fs::path& lyrics)const{
    std::vector<fs::path> dirs;auto add=[&](const fs::path&p){if(!p.empty()&&std::find(dirs.begin(),dirs.end(),p)==dirs.end())dirs.push_back(p);};add(audio.parent_path());add(lyrics.parent_path());if(!coverRoot_.empty())add(fs::path(coverRoot_));
    const std::wstring stem=audio.stem().wstring();static constexpr std::wstring_view extensions[]={L".jpg",L".jpeg",L".png",L".bmp",L".tif",L".tiff",L".webp"};std::error_code ec;
    for(const auto&dir:dirs)for(const auto ext:extensions){const fs::path candidate=dir/(stem+std::wstring(ext));if(fs::is_regular_file(candidate,ec))return candidate;ec.clear();}
    for(const auto&dir:dirs)for(const auto ext:extensions){const fs::path candidate=dir/(L"cover"+std::wstring(ext));if(fs::is_regular_file(candidate,ec))return candidate;ec.clear();}
    return{};
}
void App::OpenVideoExportForCurrentSong(){if(!doc_){MessageBoxW(hwnd_,L"Bitte zuerst im Lyrics Editor einen Song auswählen.",L"Video Export",MB_OK|MB_ICONINFORMATION);return;}CommitEdit();if(doc_->Dirty()){std::wstring error;if(!doc_->Save(error)){MessageBoxW(hwnd_,error.c_str(),L"Lyrics speichern",MB_OK|MB_ICONERROR);return;}}
    videoAudioPath_=doc_->AudioPath();videoLyricsPath_=doc_->JsonPath();videoDuration_=doc_->Duration();videoAudioAnalysis_={};videoSongStructure_={};videoPreviewSeconds_=doc_->Segments().empty()?0.0:std::min(videoDuration_,doc_->Segments().front().start+0.35);videoCoverPath_=DiscoverCover(videoAudioPath_,videoLyricsPath_);videoAlbumCoverPath_=videoCoverPath_;videoImages_.clear();videoImageProfiles_.clear();if(!videoCoverPath_.empty())videoImages_.push_back(videoCoverPath_);AnalyzeVideoImages();videoVisualTimeline_=BuildEvenVisualTimeline(videoImages_,videoDuration_);AnalyzeVideoAudio();if(videoOutputRoot_.empty())videoOutputRoot_=(fs::path(outputRoot_).parent_path()/L"videos").wstring();videoOutputPath_=fs::path(videoOutputRoot_)/(videoAudioPath_.stem().wstring()+L".mp4");videoExportState_=ExportState::Idle;videoExportProgress_=0;videoExportMessage_=videoImages_.empty()?L"Bitte Bilder wählen.":L"Startbild automatisch gefunden. Bereit zum Export.";previewRenderer_.reset();previewLoadedCover_.clear();videoPreviewBitmap_.Reset();videoPreviewDirty_=true;StopAudio();page_=Page::VideoExport;ShowWindow(edit_,SW_HIDE);SaveSettings();InvalidateRect(hwnd_,nullptr,FALSE);
}
void App::ChooseVideoCover(){
    auto images=PickImageFiles();if(images.empty())return;
    videoImages_=std::move(images);
    if(videoAlbumCoverPath_.empty()||std::find(videoImages_.begin(),videoImages_.end(),videoAlbumCoverPath_)==videoImages_.end())videoAlbumCoverPath_=videoImages_.front();
    const auto coverIt=std::find(videoImages_.begin(),videoImages_.end(),videoAlbumCoverPath_);
    if(coverIt!=videoImages_.end()&&coverIt!=videoImages_.begin())std::rotate(videoImages_.begin(),coverIt,std::next(coverIt));
    videoCoverPath_=videoAlbumCoverPath_;
    coverRoot_=videoCoverPath_.parent_path().wstring();
    AnalyzeVideoImages();
    videoVisualTimeline_=videoAudioAnalysis_.Empty()?BuildEvenVisualTimeline(videoImages_,videoDuration_):BuildSmartVisualTimeline(videoImages_,videoDuration_,videoAudioAnalysis_);
    if(!videoVisualTimeline_.clips.empty())videoVisualTimeline_.clips.front().imagePath=videoAlbumCoverPath_;
    previewRenderer_.reset();previewLoadedCover_.clear();videoPreviewBitmap_.Reset();videoPreviewDirty_=true;
    videoExportMessage_=std::to_wstring(videoImages_.size())+(videoImageProfiles_.size()==videoImages_.size()?L" Bilder analysiert · Albumcover: "+videoAlbumCoverPath_.filename().wstring():L" Bilder geladen · Analyse teilweise fehlgeschlagen.");
    SaveSettings();InvalidateRect(hwnd_,nullptr,FALSE);
}
void App::ChooseVideoAlbumCover(){
    if(videoImages_.empty()){MessageBoxW(hwnd_,L"Bitte zuerst Bilder laden.",L"Albumcover",MB_OK|MB_ICONINFORMATION);return;}
    const auto selected=PickCoverFile();if(!selected)return;
    const auto it=std::find(videoImages_.begin(),videoImages_.end(),*selected);
    if(it==videoImages_.end()){MessageBoxW(hwnd_,L"Das Albumcover muss eines der bereits geladenen Bilder sein.",L"Albumcover",MB_OK|MB_ICONWARNING);return;}
    videoAlbumCoverPath_=*selected;videoCoverPath_=*selected;
    if(it!=videoImages_.begin())std::rotate(videoImages_.begin(),it,std::next(it));
    AnalyzeVideoImages();
    if(!videoVisualTimeline_.clips.empty())videoVisualTimeline_.clips.front().imagePath=videoAlbumCoverPath_;
    previewRenderer_.reset();videoPreviewBitmap_.Reset();videoPreviewSeconds_=0.0;videoPreviewDirty_=true;
    videoExportMessage_=L"Albumcover festgelegt: "+videoAlbumCoverPath_.filename().wstring()+L" · wird immer als erster Clip verwendet.";
    InvalidateRect(hwnd_,nullptr,FALSE);
}
void App::ChooseVideoOutput(){if(auto path=PickFolder(L"Video-Ausgabeordner auswählen")){const fs::path selected=path->lexically_normal();videoOutputRoot_=selected.wstring();if(!videoAudioPath_.empty())videoOutputPath_=selected/(videoAudioPath_.stem().wstring()+L".mp4");videoExportMessage_=L"Video-Ziel festgelegt: "+videoOutputPath_.wstring();SaveSettings();InvalidateRect(hwnd_,nullptr,FALSE);}}
void App::StartVideoExport(){if(!videoExporter_||videoExporter_->Running())return;if(videoAudioPath_.empty()||videoLyricsPath_.empty()){MessageBoxW(hwnd_,L"Bitte den Videoexport aus einem geöffneten Song im Lyrics Editor starten.",L"Video Export",MB_OK|MB_ICONINFORMATION);return;}if(videoImages_.empty()){MessageBoxW(hwnd_,L"Bitte zuerst mindestens ein Bild auswählen.",L"Video Export",MB_OK|MB_ICONINFORMATION);return;}if(videoOutputRoot_.empty()){MessageBoxW(hwnd_,L"Bitte zuerst einen Video-Ausgabeordner auswählen.",L"Video Export",MB_OK|MB_ICONINFORMATION);return;}videoOutputPath_=fs::path(videoOutputRoot_).lexically_normal()/(videoAudioPath_.stem().wstring()+L".mp4");if(videoVisualTimeline_.Empty())videoVisualTimeline_=BuildEvenVisualTimeline(videoImages_,videoDuration_);StopVideoPreview();VideoExportJob job;job.audioPath=videoAudioPath_;job.lyricsJsonPath=videoLyricsPath_;job.coverPath=videoCoverPath_;job.visualTimeline=videoVisualTimeline_;job.outputPath=videoOutputPath_;job.preset=videoPreset_;std::wstring error;if(!videoExporter_->Start(std::move(job),error)){MessageBoxW(hwnd_,error.c_str(),L"Video Export",MB_OK|MB_ICONERROR);return;}videoExportState_=ExportState::Preparing;videoExportProgress_=0;videoExportMessage_=L"Export nach: "+videoOutputPath_.wstring();InvalidateRect(hwnd_,nullptr,FALSE);}
void App::CancelVideoExport(){if(videoExporter_&&videoExporter_->Running()){videoExportMessage_=L"Abbruch wird ausgeführt …";videoExporter_->Cancel();InvalidateRect(hwnd_,nullptr,FALSE);}}
void App::OnExport(ExportProgress*raw){std::unique_ptr<ExportProgress>event(raw);videoExportState_=event->state;videoExportProgress_=event->progress;videoExportMessage_=event->message;if(event->state==ExportState::Done)AddLog(L"Video fertig: "+event->outputPath.wstring());else if(event->state==ExportState::Error){AddLog(L"Videoexport fehlgeschlagen: "+event->message);MessageBoxW(hwnd_,event->message.c_str(),L"Video Export",MB_OK|MB_ICONERROR);}InvalidateRect(hwnd_,nullptr,FALSE);}
void App::AnalyzeVideoImages(){
    videoImageProfiles_.clear();
    if(videoImages_.empty())return;
    ImageAnalyzer analyzer;std::wstring error;
    if(!analyzer.Analyze(videoImages_,videoImageProfiles_,error)){
        videoImageProfiles_.clear();
        videoExportMessage_=L"Visual Intelligence: "+error;
        return;
    }
}
void App::AnalyzeVideoAudio(){videoAudioAnalysis_={};videoSongStructure_={};if(videoAudioPath_.empty())return;AudioAnalyzer analyzer;std::wstring error;if(!analyzer.Analyze(videoAudioPath_,videoAudioAnalysis_,error)){videoExportMessage_=L"Audio Intelligence: "+error;return;}if(videoDuration_<=0.0)videoDuration_=videoAudioAnalysis_.duration;const std::vector<LyricSegment> emptyLyrics;const auto& lyrics=doc_?doc_->Segments():emptyLyrics;videoSongStructure_=kg::AnalyzeSongStructure(videoAudioAnalysis_,lyrics,videoDuration_);videoExportMessage_=L"Audio analysiert: "+std::to_wstring(videoAudioAnalysis_.onsets.size())+L" Onsets, "+std::to_wstring(videoAudioAnalysis_.pauses.size())+L" Pausen, "+std::to_wstring(videoSongStructure_.sections.size())+L" Songbereiche.";if(videoImages_.size()>1)videoVisualTimeline_=BuildSmartVisualTimeline(videoImages_,videoDuration_,videoAudioAnalysis_);videoPreviewDirty_=true;}
void App::ApplySmartVisualTimeline(){if(videoImages_.empty())return;if(videoAudioAnalysis_.Empty())AnalyzeVideoAudio();if(videoAudioAnalysis_.Empty())return;StopVideoPreview();videoVisualTimeline_=BuildSmartVisualTimeline(videoImages_,videoDuration_,videoAudioAnalysis_);previewRenderer_.reset();videoPreviewBitmap_.Reset();videoPreviewDirty_=true;videoExportMessage_=L"Smart Cuts: Bildgrenzen wurden auf musikalische Onsets und Pausen ausgerichtet.";InvalidateRect(hwnd_,nullptr,FALSE);}
void App::ApplyTatarusVisualTimeline(){if(videoImages_.empty())return;if(videoAudioAnalysis_.Empty())AnalyzeVideoAudio();if(videoAudioAnalysis_.Empty())return;StopVideoPreview();if(videoImageProfiles_.size()!=videoImages_.size())AnalyzeVideoImages();if(videoImageProfiles_.size()==videoImages_.size())videoVisualTimeline_=BuildTatarusVisualTimeline(videoImageProfiles_,videoDuration_,videoAudioAnalysis_,tatarusVisualBrain_);else videoVisualTimeline_=BuildSmartVisualTimeline(videoImages_,videoDuration_,videoAudioAnalysis_);if(!videoVisualTimeline_.clips.empty()&&!videoAlbumCoverPath_.empty())videoVisualTimeline_.clips.front().imagePath=videoAlbumCoverPath_;previewRenderer_.reset();videoPreviewBitmap_.Reset();videoPreviewDirty_=true;videoExportMessage_=tatarusVisualBrain_.Trained()?L"TATARUS Visual Intelligence: gelernte Bildwahl-, Cut-, Transition-, Zoom-, Drift- und Lyrics-Präferenzen wurden angewendet.":L"TATARUS Visual Intelligence: Bootstrap aktiv. Rechtsklick auf Timeline = Wunschbild auswählen und lernen.";InvalidateRect(hwnd_,nullptr,FALSE);}
void App::ApplyTatarusProduction(){
    if(videoImages_.empty())return;
    if(videoAlbumCoverPath_.empty())videoAlbumCoverPath_=videoImages_.front();
    if(videoAudioAnalysis_.Empty())AnalyzeVideoAudio();if(videoAudioAnalysis_.Empty())return;
    if(videoSongStructure_.Empty()){const std::vector<LyricSegment> emptyLyrics;const auto& lyrics=doc_?doc_->Segments():emptyLyrics;videoSongStructure_=kg::AnalyzeSongStructure(videoAudioAnalysis_,lyrics,videoDuration_);}
    StopVideoPreview();if(videoImageProfiles_.size()!=videoImages_.size())AnalyzeVideoImages();
    if(videoImageProfiles_.size()==videoImages_.size()&&!videoSongStructure_.Empty()){
        ProductionPlanOptions options;options.albumCoverPath=videoAlbumCoverPath_;options.requireAllImages=videoUseAllImages_;options.reuseCooldown=4;options.reusePenalty=0.16f;
        videoVisualTimeline_=BuildTatarusProductionTimeline(videoImageProfiles_,videoDuration_,videoAudioAnalysis_,videoSongStructure_,tatarusVisualBrain_,options);
    }else if(videoImageProfiles_.size()==videoImages_.size()){
        videoVisualTimeline_=BuildTatarusVisualTimeline(videoImageProfiles_,videoDuration_,videoAudioAnalysis_,tatarusVisualBrain_);
        if(!videoVisualTimeline_.clips.empty())videoVisualTimeline_.clips.front().imagePath=videoAlbumCoverPath_;
    }else{
        videoVisualTimeline_=BuildSmartVisualTimeline(videoImages_,videoDuration_,videoAudioAnalysis_);
        if(!videoVisualTimeline_.clips.empty())videoVisualTimeline_.clips.front().imagePath=videoAlbumCoverPath_;
    }
    std::set<fs::path> usedImages;for(const auto& clip:videoVisualTimeline_.clips)usedImages.insert(clip.imagePath);
    previewRenderer_.reset();videoPreviewBitmap_.Reset();videoPreviewSeconds_=0.0;videoPreviewDirty_=true;
    videoExportMessage_=L"TATARUS PRODUCE: "+std::to_wstring(videoVisualTimeline_.clips.size())+L" Clips · "+std::to_wstring(usedImages.size())+L"/"+std::to_wstring(videoImages_.size())+L" Bilder verwendet"+(videoUseAllImages_?L" · Diversity-Lock aktiv.":L" · freie Bildwahl.");
    InvalidateRect(hwnd_,nullptr,FALSE);
}
bool App::PersistTatarusBrain(std::wstring_view context){std::wstring brainError;if(tatarusVisualBrain_.Save(fs::path(ExeDirectory())/L"tatarus_visual_brain.json",brainError))return true;const std::wstring message=L"TATARUS Brain speichern fehlgeschlagen ["+std::wstring(context)+L"]: "+(brainError.empty()?L"unbekannter Fehler":brainError);AddLog(message);videoExportMessage_=message;return false;}
void App::TrainTatarusBoundary(double seconds){if(videoVisualTimeline_.clips.size()<2||videoDuration_<=0.0||videoAudioAnalysis_.Empty())return;std::size_t boundary=1;double oldTime=videoVisualTimeline_.clips[0].end;double best=std::abs(oldTime-seconds);for(std::size_t i=2;i<videoVisualTimeline_.clips.size();++i){const double t=videoVisualTimeline_.clips[i-1].end;const double d=std::abs(t-seconds);if(d<best){best=d;boundary=i;oldTime=t;}}const double left=videoVisualTimeline_.clips[boundary-1].start+1.0;const double right=videoVisualTimeline_.clips[boundary].end-1.0;if(right<=left)return;const double chosen=std::clamp(seconds,left,right);const auto preferred=tatarusVisualBrain_.Sense(chosen,videoDuration_,videoAudioAnalysis_);const auto rejected=tatarusVisualBrain_.Sense(oldTime,videoDuration_,videoAudioAnalysis_);tatarusVisualBrain_.LearnPreference(preferred,rejected);videoVisualTimeline_.clips[boundary-1].end=chosen;videoVisualTimeline_.clips[boundary].start=chosen;if(!PersistTatarusBrain(L"Training"))return;previewRenderer_.reset();videoPreviewBitmap_.Reset();videoPreviewSeconds_=chosen;videoPreviewDirty_=true;videoExportMessage_=L"TATARUS gelernt: Schnitt "+FormatTime(oldTime)+L" → "+FormatTime(chosen)+L" · Lernsignal #"+std::to_wstring(tatarusVisualBrain_.TrainingEvents());InvalidateRect(hwnd_,nullptr,FALSE);}
void App::TrainTatarusStyle(double seconds, bool toggleTransition){
    if(videoVisualTimeline_.clips.empty()||videoDuration_<=0.0||videoAudioAnalysis_.Empty())return;
    std::size_t index=0;
    for(std::size_t i=0;i<videoVisualTimeline_.clips.size();++i){if(seconds>=videoVisualTimeline_.clips[i].start&&seconds<videoVisualTimeline_.clips[i].end){index=i;break;}}
    auto& clip=videoVisualTimeline_.clips[index];
    const double sample=clip.start+std::max(0.0,clip.end-clip.start)*0.5;
    const auto features=tatarusVisualBrain_.Sense(sample,videoDuration_,videoAudioAnalysis_);
    if(toggleTransition&&index+1<videoVisualTimeline_.clips.size()){
        clip.transition=clip.transition==VisualTransition::CrossFade?VisualTransition::Cut:VisualTransition::CrossFade;
    }else{
        const bool intense=clip.style.zoomGain<1.15f;
        clip.style.zoomGain=intense?1.40f:0.82f;
        clip.style.driftGain=intense?0.70f:1.28f;
        clip.style.lyricsScale=intense?1.15f:0.95f;
    }
    tatarusVisualBrain_.LearnStyle(features,clip.style,clip.transition);
    if(!PersistTatarusBrain(L"Training"))return;
    previewRenderer_.reset();videoPreviewBitmap_.Reset();videoPreviewSeconds_=seconds;videoPreviewDirty_=true;
    videoExportMessage_=toggleTransition?L"TATARUS Stil gelernt: Übergang wurde als Präferenz gespeichert.":L"TATARUS Stil gelernt: Motion/Zoom/Lyrics-Gewichtung wurde gespeichert.";
    InvalidateRect(hwnd_,nullptr,FALSE);
}

void App::StartVideoPreview(){if(videoAudioPath_.empty()||videoDuration_<=0)return;if(videoPreviewSeconds_>=videoDuration_)videoPreviewSeconds_=0;std::wstring error;if(audio_.Path()!=videoAudioPath_&&!audio_.Load(videoAudioPath_,error)){MessageBoxW(hwnd_,error.c_str(),L"Video-Vorschau",MB_OK|MB_ICONERROR);return;}if(!audio_.Play(videoPreviewSeconds_,videoDuration_,error)){MessageBoxW(hwnd_,error.c_str(),L"Video-Vorschau",MB_OK|MB_ICONERROR);return;}videoPreviewStartSeconds_=videoPreviewSeconds_;videoPreviewStarted_=std::chrono::steady_clock::now();videoPreviewPlaying_=true;InvalidateRect(hwnd_,nullptr,FALSE);}
void App::StopVideoPreview(){videoPreviewPlaying_=false;audio_.Stop();InvalidateRect(hwnd_,nullptr,FALSE);}
void App::TrainTatarusImage(double seconds){
    if(videoVisualTimeline_.clips.empty()||videoDuration_<=0.0||videoAudioAnalysis_.Empty())return;
    std::size_t index=videoVisualTimeline_.clips.size()-1;
    for(std::size_t i=0;i<videoVisualTimeline_.clips.size();++i){if(seconds>=videoVisualTimeline_.clips[i].start&&seconds<videoVisualTimeline_.clips[i].end){index=i;break;}}
    const auto selected=PickCoverFile();if(!selected)return;
    ImageAnalyzer analyzer;VisualImageProfile preferred;std::wstring error;
    if(!analyzer.Analyze(*selected,preferred,error)){MessageBoxW(hwnd_,error.c_str(),L"Visual Intelligence",MB_OK|MB_ICONERROR);return;}
    VisualImageProfile rejected;bool foundRejected=false;
    for(const auto& profile:videoImageProfiles_){if(profile.path==videoVisualTimeline_.clips[index].imagePath){rejected=profile;foundRejected=true;break;}}
    if(!foundRejected&&!analyzer.Analyze(videoVisualTimeline_.clips[index].imagePath,rejected,error))return;
    const auto& clip=videoVisualTimeline_.clips[index];
    const auto context=tatarusVisualBrain_.Sense(clip.start+(clip.end-clip.start)*0.5,videoDuration_,videoAudioAnalysis_);
    tatarusVisualBrain_.LearnImagePreference(context,preferred.features,rejected.features);
    videoVisualTimeline_.clips[index].imagePath=preferred.path;
    bool exists=false;for(const auto& p:videoImages_)if(p==preferred.path){exists=true;break;}
    if(!exists){videoImages_.push_back(preferred.path);videoImageProfiles_.push_back(preferred);}
    if(!PersistTatarusBrain(L"Training"))return;
    previewRenderer_.reset();videoPreviewBitmap_.Reset();videoPreviewSeconds_=seconds;videoPreviewDirty_=true;
    videoExportMessage_=L"TATARUS gelernt: "+rejected.path.filename().wstring()+L" → "+preferred.path.filename().wstring()+L" · Bildsignal #"+std::to_wstring(tatarusVisualBrain_.ImageTrainingEvents());
    InvalidateRect(hwnd_,nullptr,FALSE);
}
void App::UpdateVideoPreview(){videoPreviewDirty_=false;if(!rt_||!doc_||videoImages_.empty()){videoPreviewBitmap_.Reset();return;}const auto width=static_cast<std::uint32_t>(std::clamp(std::lround(videoPreviewRect_.r-videoPreviewRect_.l),320L,1280L));const auto height=std::max<std::uint32_t>(180,width*9/16);std::wstring error;
    if(!previewRenderer_||previewWidth_!=width||previewHeight_!=height){auto renderer=std::make_unique<VideoRenderer>();if(!renderer->Initialize(width,height,error)){videoPreviewBitmap_.Reset();videoExportMessage_=L"Vorschau: "+error;return;}if(videoVisualTimeline_.Empty())videoVisualTimeline_=BuildEvenVisualTimeline(videoImages_,videoDuration_);if(!renderer->LoadTimeline(videoVisualTimeline_,error)){videoPreviewBitmap_.Reset();videoExportMessage_=L"Vorschau: "+error;return;}previewRenderer_=std::move(renderer);previewWidth_=width;previewHeight_=height;previewLoadedCover_=videoCoverPath_;}
    std::vector<std::byte>pixels;if(!previewRenderer_->RenderFrame(videoPreviewSeconds_,doc_->Segments(),videoPreset_,pixels,error)){videoPreviewBitmap_.Reset();videoExportMessage_=L"Vorschau: "+error;return;}videoPreviewBitmap_.Reset();const auto properties=D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96);const HRESULT hr=rt_->CreateBitmap(D2D1::SizeU(width,height),pixels.data(),width*4,properties,videoPreviewBitmap_.GetAddressOf());if(FAILED(hr)){videoExportMessage_=L"Vorschaubitmap konnte nicht angezeigt werden.";videoPreviewBitmap_.Reset();}}
void App::UpdateEditPlacement(){if(!edit_)return;if(page_!=Page::Editor||!doc_||segmentIndex_<0){ShowWindow(edit_,SW_HIDE);return;}RECT rc;GetClientRect(hwnd_,&rc);float W=rc.right/dpiScale_;float x=204,listW=250,segW=std::max(400.f,(W-x-listW-390));float left=x+listW+12+segW+12+14,top=162+96,right=W-30-14,bottom=top+180;SetWindowPos(edit_,nullptr,int(left*dpiScale_),int(top*dpiScale_),int((right-left)*dpiScale_),int((bottom-top)*dpiScale_),SWP_NOZORDER|SWP_SHOWWINDOW);}

void App::OnClick(float x,float y){
    if(navTrans_.Contains(x,y)){StopVideoPreview();page_=Page::Transcribe;ShowWindow(edit_,SW_HIDE);}
    else if(navEdit_.Contains(x,y)){StopVideoPreview();page_=Page::Editor;RefreshLyrics(true);UpdateEditPlacement();}
    else if(navVideo_.Contains(x,y)){if(videoAudioPath_.empty()&&doc_)OpenVideoExportForCurrentSong();else{page_=Page::VideoExport;ShowWindow(edit_,SW_HIDE);videoPreviewDirty_=true;}}
    else if(navSettings_.Contains(x,y)){StopVideoPreview();page_=Page::Settings;ShowWindow(edit_,SW_HIDE);}
    else if(page_==Page::Transcribe){if(btnFile_.Contains(x,y))AddFile();else if(btnFolder_.Contains(x,y))AddFolder();else if(btnOutput_.Contains(x,y))ChooseOutput();else if(btnStart_.Contains(x,y))StartQueue();else if(btnStop_.Contains(x,y))StopQueue();else if(btnClear_.Contains(x,y)&&activeJob_<0)jobs_.clear();else if(modelChip_.Contains(x,y)){if(model_==L"large-v3")model_=L"turbo";else if(model_==L"turbo")model_=L"medium";else if(model_==L"medium")model_=L"small";else model_=L"large-v3";}else if(deviceChip_.Contains(x,y)){device_=device_==L"cpu"?L"cuda":L"cpu";compute_=device_==L"cpu"?L"int8":L"float16";}else if(computeChip_.Contains(x,y)){if(device_==L"cpu")compute_=compute_==L"int8"?L"float32":L"int8";else compute_=compute_==L"float16"?L"int8_float16":L"float16";}else if(languageChip_.Contains(x,y)){language_=language_==L"auto"?L"de":language_==L"de"?L"en":L"auto";}else if(watchChip_.Contains(x,y)){watchEnabled_=!watchEnabled_;}else if(overwriteChip_.Contains(x,y)){overwrite_=!overwrite_;}}
    else if(page_==Page::Editor){RECT rc;GetClientRect(hwnd_,&rc);float W=rc.right/dpiScale_,H=rc.bottom/dpiScale_;float sx=204,listW=250,segW=std::max(400.f,(W-sx-listW-390));FRect songs{sx,162,sx+listW,H-78};FRect segs{sx+listW+12,162,sx+listW+12+segW,H-78};if(editorVideo_.Contains(x,y))OpenVideoExportForCurrentSong();else if(editorRefresh_.Contains(x,y))RefreshLyrics(true);else if(editorSave_.Contains(x,y))SaveDocument();else if(editorApply_.Contains(x,y)){CommitEdit();}else if(editorApplyNext_.Contains(x,y)){CommitEdit();if(doc_&&segmentIndex_+1<int(doc_->Segments().size()))SelectSegment(segmentIndex_+1);}else if(editorPlay_.Contains(x,y))PlaySelected(false);else if(editorBefore_.Contains(x,y))PlaySelected(true);else if(editorStop_.Contains(x,y))StopAudio();else if(editorNext_.Contains(x,y))NextSuspicious();else if(editorSuspicious_.Contains(x,y)){onlySuspicious_=!onlySuspicious_;segmentScroll_=0;}else if(timelineRect_.Contains(x,y)&&doc_&&doc_->Duration()>0){cursorSeconds_=doc_->Duration()*std::clamp((x-timelineRect_.l)/(timelineRect_.r-timelineRect_.l),0.f,1.f);PlayCursor();}else if(songs.Contains(x,y)){int visible=std::max(1,int((songs.b-songs.t-48)/38));int start=std::clamp(songScroll_,0,std::max(0,int(lyricFiles_.size())-visible));int row=int((y-(songs.t+38))/38);if(row>=0&&row<visible&&start+row<int(lyricFiles_.size()))SelectSong(start+row);}else if(segs.Contains(x,y)&&doc_){std::vector<int>indices;for(int i=0;i<int(doc_->Segments().size());++i)if(!onlySuspicious_||doc_->Segments()[i].Suspicious())indices.push_back(i);int visible=std::max(1,int((segs.b-segs.t-48)/42));int start=std::clamp(segmentScroll_,0,std::max(0,int(indices.size())-visible));int row=int((y-(segs.t+38))/42);if(row>=0&&row<visible&&start+row<int(indices.size()))SelectSegment(indices[start+row]);}}
    else if(page_==Page::VideoExport){if(videoChooseCover_.Contains(x,y))ChooseVideoCover();else if(videoChooseAlbumCover_.Contains(x,y))ChooseVideoAlbumCover();else if(videoUseAllImagesChip_.Contains(x,y)){videoUseAllImages_=!videoUseAllImages_;videoExportMessage_=videoUseAllImages_?L"Diversity-Lock: Alle geladenen Bilder werden vor Wiederverwendung mindestens einmal genutzt.":L"Diversity-Lock aus: TATARUS darf Bilder frei nach Score wählen.";SaveSettings();}else if(videoSmartCuts_.Contains(x,y))ApplySmartVisualTimeline();else if(videoTatarusCuts_.Contains(x,y))ApplyTatarusVisualTimeline();else if(videoTatarusProduce_.Contains(x,y))ApplyTatarusProduction();else if(videoChooseOutput_.Contains(x,y))ChooseVideoOutput();else if(videoExport_.Contains(x,y))StartVideoExport();else if(videoCancel_.Contains(x,y))CancelVideoExport();else if(videoPreviewPlay_.Contains(x,y))StartVideoPreview();else if(videoPreviewStop_.Contains(x,y))StopVideoPreview();else if(videoTimeline_.Contains(x,y)&&videoDuration_>0){StopVideoPreview();const double clicked=videoDuration_*std::clamp((x-videoTimeline_.l)/(videoTimeline_.r-videoTimeline_.l),0.f,1.f);if(GetKeyState(VK_SHIFT)&0x8000)TrainTatarusBoundary(clicked);else if(GetKeyState(VK_CONTROL)&0x8000)TrainTatarusStyle(clicked,true);else if(GetKeyState(VK_MENU)&0x8000)TrainTatarusStyle(clicked,false);else{videoPreviewSeconds_=clicked;videoPreviewDirty_=true;}}}
    InvalidateRect(hwnd_,nullptr,FALSE);
}
void App::OnWheel(short d,float x,float y){int step=d>0?-3:3;if(page_==Page::Transcribe)queueScroll_=std::max(0,queueScroll_+step);else if(page_==Page::Editor){RECT rc;GetClientRect(hwnd_,&rc);float W=rc.right/dpiScale_,H=rc.bottom/dpiScale_;float sx=204,listW=250,segW=std::max(400.f,(W-sx-listW-390));if(FRect{sx,162,sx+listW,H-78}.Contains(x,y))songScroll_=std::max(0,songScroll_+step);else if(FRect{sx+listW+12,162,sx+listW+12+segW,H-78}.Contains(x,y))segmentScroll_=std::max(0,segmentScroll_+step);}InvalidateRect(hwnd_,nullptr,FALSE);}
void App::OnKey(WPARAM key){if(page_==Page::Editor){if(GetFocus()==edit_&&(GetKeyState(VK_CONTROL)&0x8000)&&key==VK_RETURN){CommitEdit();if(doc_&&segmentIndex_+1<int(doc_->Segments().size()))SelectSegment(segmentIndex_+1);return;}if(key==VK_F5)RefreshLyrics(true);if(key==VK_SPACE&&GetFocus()!=edit_)PlaySelected(false);}else if(page_==Page::VideoExport&&key==VK_SPACE){if(videoPreviewPlaying_)StopVideoPreview();else StartVideoPreview();}}

void App::LoadSettings(){outputRoot_=(fs::path(ExeDirectory())/L"lyrics").wstring();const auto p=fs::path(ExeDirectory())/L"klanggeist_studio.json";if(fs::exists(p)){try{auto j=json::Parse(ReadUtf8File(p));if(auto*v=j.Find("output_root"))outputRoot_=Utf8ToWide(v->AsString());if(auto*v=j.Find("model"))model_=Utf8ToWide(v->AsString());if(auto*v=j.Find("device"))device_=Utf8ToWide(v->AsString());if(auto*v=j.Find("compute"))compute_=Utf8ToWide(v->AsString());if(auto*v=j.Find("language"))language_=Utf8ToWide(v->AsString());if(auto*v=j.Find("watch"))watchEnabled_=v->AsBool(true);if(auto*v=j.Find("overwrite"))overwrite_=v->AsBool(false);if(auto*v=j.Find("watched_root");v&&v->IsString()&&!v->AsString().empty())watchedRoot_=fs::path(Utf8ToWide(v->AsString()));if(auto*v=j.Find("cover_root");v&&v->IsString())coverRoot_=Utf8ToWide(v->AsString());if(auto*v=j.Find("video_output_root");v&&v->IsString())videoOutputRoot_=Utf8ToWide(v->AsString());if(auto*v=j.Find("video_use_all_images"))videoUseAllImages_=v->AsBool(true);}catch(...){}}if(videoOutputRoot_.empty())videoOutputRoot_=(fs::path(outputRoot_).parent_path()/L"videos").wstring();RefreshLyrics();}
void App::SaveSettings(){json::Value::Object o;o["output_root"]=WideToUtf8(outputRoot_);o["model"]=WideToUtf8(model_);o["device"]=WideToUtf8(device_);o["compute"]=WideToUtf8(compute_);o["language"]=WideToUtf8(language_);o["watch"]=watchEnabled_;o["overwrite"]=overwrite_;o["watched_root"]=watchedRoot_?WideToUtf8(watchedRoot_->wstring()):std::string{};o["cover_root"]=WideToUtf8(coverRoot_);o["video_output_root"]=WideToUtf8(videoOutputRoot_);o["video_use_all_images"]=videoUseAllImages_;o["video_preset"]="Klanggeist Lyrics Video";o["video_container"]="mp4";try{AtomicWriteUtf8(fs::path(ExeDirectory())/L"klanggeist_studio.json",json::Dump(json::Value(std::move(o)),2)+"\n");}catch(...){} }

} // namespace

int WINAPI wWinMain(HINSTANCE h,HINSTANCE,LPWSTR,int){App app(h);return app.Run();}
