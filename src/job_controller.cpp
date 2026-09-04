#include "job_controller.hpp"

namespace kg {

static std::wstring QuoteArg(std::wstring_view s){std::wstring out=L"\"";for(wchar_t c:s){if(c==L'\"')out+=L"\\\"";else out+=c;}out+=L"\"";return out;}
JobController::~JobController(){Stop();}

bool JobController::Start(const fs::path& appDir,const std::wstring& model,const std::wstring& device,const std::wstring& compute,const std::wstring& language,std::wstring& error){
    Stop();SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE outRead=nullptr,outWrite=nullptr,inRead=nullptr,inWrite=nullptr;
    if(!CreatePipe(&outRead,&outWrite,&sa,0)||!CreatePipe(&inRead,&inWrite,&sa,0)){error=L"Pipes konnten nicht erstellt werden.";return false;}SetHandleInformation(outRead,HANDLE_FLAG_INHERIT,0);SetHandleInformation(inWrite,HANDLE_FLAG_INHERIT,0);
    const auto script=appDir/L"backend"/L"whisper_worker.py";fs::path cache=appDir/L".hf"/L"hub";for(fs::path probe=appDir;!probe.empty();probe=probe.parent_path()){const auto candidate=probe/L".hf"/L"hub";if(fs::exists(candidate)){cache=candidate;break;}if(probe==probe.root_path())break;}
    std::wstring cmd=L"py.exe -3.12 "+QuoteArg(script.wstring())+L" --server --model "+QuoteArg(model)+L" --device "+QuoteArg(device)+L" --compute-type "+QuoteArg(compute)+L" --language "+QuoteArg(language)+L" --cache-dir "+QuoteArg(cache.wstring());
    STARTUPINFOW si{sizeof(si)};si.dwFlags=STARTF_USESTDHANDLES;si.hStdOutput=outWrite;si.hStdError=outWrite;si.hStdInput=inRead;PROCESS_INFORMATION pi{};
    std::vector<wchar_t> mutableCmd(cmd.begin(),cmd.end());mutableCmd.push_back(L'\0');
    const BOOL ok=CreateProcessW(nullptr,mutableCmd.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,appDir.c_str(),&si,&pi);CloseHandle(outWrite);CloseHandle(inRead);
    if(!ok){CloseHandle(outRead);CloseHandle(inWrite);error=L"Python-Backend konnte nicht gestartet werden. Prüfe Python 3.12 und INSTALL_BACKEND.bat.";return false;}
    process_=pi.hProcess;threadHandle_=pi.hThread;childStdin_=inWrite;childStdout_=outRead;running_=true;readerThread_=std::thread(&JobController::ReaderLoop,this);return true;
}

bool JobController::Send(const json::Value& command,std::wstring& error){if(!running_||!childStdin_){error=L"Whisper-Backend läuft nicht.";return false;}std::string line=json::Dump(command,0)+"\n";DWORD written=0;if(!WriteFile(childStdin_,line.data(),static_cast<DWORD>(line.size()),&written,nullptr)||written!=line.size()){error=L"Kommando konnte nicht an Whisper gesendet werden.";return false;}return true;}
void JobController::ReaderLoop(){std::string pending;char buf[4096];while(running_){DWORD got=0;if(!ReadFile(childStdout_,buf,sizeof(buf),&got,nullptr)||got==0)break;pending.append(buf,buf+got);for(;;){auto pos=pending.find('\n');if(pos==std::string::npos)break;std::string line=pending.substr(0,pos);pending.erase(0,pos+1);if(line.empty())continue;try{auto payload=json::Parse(line);std::string type="log";if(auto*t=payload.Find("type"))type=t->AsString();auto* ev=new WorkerEvent{type,std::move(payload)};PostMessageW(notifyWindow_,WM_KG_WORKER_EVENT,0,reinterpret_cast<LPARAM>(ev));}catch(...){json::Value::Object o;o["type"]="log";o["message"]=line;auto*ev=new WorkerEvent{"log",json::Value(std::move(o))};PostMessageW(notifyWindow_,WM_KG_WORKER_EVENT,0,reinterpret_cast<LPARAM>(ev));}}}running_=false;}
void JobController::Stop(){running_=false;if(childStdin_){CloseHandle(childStdin_);childStdin_=nullptr;}if(process_){TerminateProcess(process_,0);WaitForSingleObject(process_,2000);}if(readerThread_.joinable())readerThread_.join();if(childStdout_){CloseHandle(childStdout_);childStdout_=nullptr;}if(threadHandle_){CloseHandle(threadHandle_);threadHandle_=nullptr;}if(process_){CloseHandle(process_);process_=nullptr;}}

} // namespace kg
