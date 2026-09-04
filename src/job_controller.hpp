#pragma once
#include "common.hpp"
#include "mini_json.hpp"
#include <atomic>
#include <deque>
#include <mutex>
#include <thread>

namespace kg {

constexpr UINT WM_KG_WORKER_EVENT = WM_APP + 77;

struct WorkerEvent { std::string type; json::Value payload; };

class JobController {
public:
    explicit JobController(HWND notifyWindow) : notifyWindow_(notifyWindow) {}
    ~JobController();
    bool Start(const fs::path& appDir,const std::wstring& model,const std::wstring& device,const std::wstring& compute,const std::wstring& language,std::wstring& error);
    bool Send(const json::Value& command,std::wstring& error);
    void Stop();
    [[nodiscard]] bool Running() const noexcept { return running_; }
private:
    void ReaderLoop();
    HWND notifyWindow_{};HANDLE process_{},threadHandle_{},childStdin_{},childStdout_{};std::thread readerThread_;std::atomic_bool running_{};
};

} // namespace kg
