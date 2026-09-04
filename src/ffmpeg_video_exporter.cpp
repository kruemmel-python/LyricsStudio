#include "ffmpeg_video_exporter.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace kg {

namespace {

std::wstring QuoteArgument(std::wstring_view value) {
    std::wstring result = L"\"";
    std::size_t backslashes = 0;
    for (const wchar_t c : value) {
        if (c == L'\\') {
            ++backslashes;
            continue;
        }
        if (c == L'\"') {
            result.append(backslashes * 2 + 1, L'\\');
            result += L'\"';
            backslashes = 0;
            continue;
        }
        result.append(backslashes, L'\\');
        backslashes = 0;
        result += c;
    }
    result.append(backslashes * 2, L'\\');
    result += L'\"';
    return result;
}

std::wstring Number(double value, int precision = 6) {
    std::wostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(precision);
    stream << value;
    return stream.str();
}

std::string AssTime(double seconds) {
    const auto centiseconds = static_cast<long long>(std::max(0.0, seconds) * 100.0 + 0.5);
    const auto hours = centiseconds / 360'000;
    const auto minutes = (centiseconds / 6'000) % 60;
    const auto secs = (centiseconds / 100) % 60;
    const auto cs = centiseconds % 100;
    char buffer[32]{};
    sprintf_s(buffer, "%lld:%02lld:%02lld.%02lld", hours, minutes, secs, cs);
    return buffer;
}

std::string EscapeAss(std::wstring_view text) {
    std::string result;
    for (const wchar_t c : text) {
        if (c == L'\r') continue;
        if (c == L'\n') {
            result += "\\N";
            continue;
        }
        if (c == L'{') {
            result += "\\{";
            continue;
        }
        if (c == L'}') {
            result += "\\}";
            continue;
        }
        result += WideToUtf8(std::wstring_view(&c, 1));
    }
    return result;
}

float LyricsScaleAt(const VisualTimeline& timeline, double seconds, const VideoExportPreset& preset) {
    float scale = preset.lyricsScale;
    if (const auto* clip = timeline.ClipAt(seconds)) scale *= clip->style.lyricsScale;
    return std::clamp(scale, 0.70f, 1.45f);
}

bool WriteAss(const fs::path& path, const LyricsDocument& document, const VideoExportJob& job,
              std::wstring& error) {
    try {
        const auto& segments = document.Segments();
        const auto& preset = job.preset;
        const int fadeMs = static_cast<int>(std::max(0.0f, preset.fadeDuration) * 1000.0f + 0.5f);
        std::string content =
            "[Script Info]\nScriptType: v4.00+\nPlayResX: " + std::to_string(preset.size.width) +
            "\nPlayResY: " + std::to_string(preset.size.height) +
            "\nScaledBorderAndShadow: yes\nWrapStyle: 0\n\n"
            "[V4+ Styles]\n"
            "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, "
            "Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, "
            "Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n"
            "Style: Current,Segoe UI Semibold,64,&H00F0E03D,&H00F0E03D,&HAA000000,&H80000000,-1,0,0,0,100,100,0,0,1,2.2,1.5,5,170,170,40,1\n"
            "Style: Neighbor,Segoe UI,42,&H88FFFFFF,&H88FFFFFF,&HAA000000,&H80000000,0,0,0,0,100,100,0,0,1,1.5,1,5,190,190,40,1\n\n"
            "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";

        auto add = [&](double start, double end, std::string_view style, int y,
                       std::wstring_view text, bool fade, float scale) {
            const int baseFont = style == "Current" ? 64 : 42;
            const int fontSize = static_cast<int>(std::lround(static_cast<double>(baseFont) * scale));
            content += "Dialogue: 0," + AssTime(start) + "," + AssTime(end) + ",";
            content += style;
            content += ",,0,0,0,,{\\an5\\pos(" + std::to_string(preset.size.width / 2U) + "," +
                       std::to_string(y) + ")\\fs" + std::to_string(fontSize);
            if (fade) content += "\\fad(" + std::to_string(fadeMs) + "," + std::to_string(fadeMs) + ")";
            content += "}" + EscapeAss(text) + "\n";
        };

        const double yScale = static_cast<double>(preset.size.height) / 1080.0;
        const int previousY = static_cast<int>(std::lround(410.0 * yScale));
        const int currentY = static_cast<int>(std::lround(540.0 * yScale));
        const int nextY = static_cast<int>(std::lround(670.0 * yScale));

        for (std::size_t i = 0; i < segments.size(); ++i) {
            const auto& current = segments[i];
            const float scale = LyricsScaleAt(job.visualTimeline, (current.start + current.end) * 0.5, preset);
            if (preset.showPreviousLine && i > 0)
                add(current.start, current.end, "Neighbor", previousY, segments[i - 1].text, false, scale);
            add(current.start, current.end, "Current", currentY, current.text, true, scale);
            if (preset.showNextLine && i + 1 < segments.size())
                add(current.start, current.end, "Neighbor", nextY, segments[i + 1].text, false, scale);
        }
        AtomicWriteUtf8(path, content);
        return true;
    } catch (const std::exception& exception) {
        error = L"FFmpeg-Lyrics-Timeline konnte nicht geschrieben werden: " + Utf8ToWide(exception.what());
        return false;
    }
}

std::wstring EscapeFilterPath(const fs::path& path) {
    std::wstring value = path.wstring();
    std::replace(value.begin(), value.end(), L'\\', L'/');
    std::wstring result;
    for (const wchar_t c : value) {
        if (c == L':') result += L"\\:";
        else if (c == L'\'') result += L"\\'";
        else result += c;
    }
    return result;
}

double SafeFade(const VisualTimeline& timeline, std::size_t index) {
    if (index + 1 >= timeline.clips.size()) return 0.0;
    const auto& current = timeline.clips[index];
    const auto& next = timeline.clips[index + 1];
    if (current.transition != VisualTransition::CrossFade) return 0.0;
    const double currentDuration = std::max(0.0, current.end - current.start);
    const double nextDuration = std::max(0.0, next.end - next.start);
    return std::clamp(current.transitionSeconds, 0.0,
                      std::min(currentDuration * 0.45, nextDuration * 0.45));
}

std::wstring BuildSingleImageFilter(const fs::path& assPath, const VideoExportJob& job, bool reuseUnchanged) {
    const auto width = job.preset.size.width;
    const auto height = job.preset.size.height;
    const auto fps = job.preset.fps;
    const auto oversizedWidth = static_cast<unsigned>(std::ceil(width * 1.05 / 2.0) * 2.0);
    const auto oversizedHeight = static_cast<unsigned>(std::ceil(height * 1.05 / 2.0) * 2.0);
    std::wostringstream filter;
    filter << L"scale=" << oversizedWidth << L":" << oversizedHeight
           << L":force_original_aspect_ratio=increase,crop=" << oversizedWidth << L":" << oversizedHeight
           << L",loop=loop=-1:size=1:start=0"
           << L",crop=" << width << L":" << height
           << L":x='(iw-ow)/2+16*sin(2*PI*n/(" << fps << L"*28))'"
           << L":y='(ih-oh)/2+9*sin(2*PI*n/(" << fps << L"*37)+1.7)'"
           << L",subtitles='" << EscapeFilterPath(assPath) << L"'";
    if (reuseUnchanged)
        filter << L":reuse_unchanged=1";
    else
        filter << L",mpdecimate=hi=0:lo=0:frac=0";
    return filter.str();
}

std::wstring BuildMultiImageFilter(const fs::path& assPath, const VideoExportJob& job, bool reuseUnchanged) {
    const auto& timeline = job.visualTimeline;
    const auto width = job.preset.size.width;
    const auto height = job.preset.size.height;
    const auto fps = job.preset.fps;
    std::wostringstream filter;

    for (std::size_t i = 0; i < timeline.clips.size(); ++i) {
        const auto& clip = timeline.clips[i];
        const double duration = std::max(0.05, clip.end - clip.start);
        const double incomingFade = i == 0 ? 0.0 : SafeFade(timeline, i - 1);
        const float zoomGain = std::clamp(clip.style.zoomGain, 0.60f, 1.80f);
        const float driftGain = std::clamp(clip.style.driftGain, 0.40f, 1.80f);
        const double zoomRange = std::max(0.0f, job.preset.zoomMax - 1.0f) * zoomGain;
        const double overscan = std::max(1.025, 1.025 + zoomRange);
        const auto scaledWidth = static_cast<unsigned>(std::ceil(width * overscan / 2.0) * 2.0);
        const auto scaledHeight = static_cast<unsigned>(std::ceil(height * overscan / 2.0) * 2.0);
        const double driftX = static_cast<double>(job.preset.driftPixelsX) * driftGain;
        const double driftY = static_cast<double>(job.preset.driftPixelsY) * driftGain;

        filter << L"[" << i << L":v]scale=" << scaledWidth << L":" << scaledHeight
               << L":force_original_aspect_ratio=increase"
               << L",crop=" << scaledWidth << L":" << scaledHeight
               << L",loop=loop=-1:size=1:start=0"
               << L",crop=" << width << L":" << height
               << L":x='(iw-ow)/2+" << Number(driftX, 3) << L"*sin(2*PI*t/28)'"
               << L":y='(ih-oh)/2+" << Number(driftY, 3) << L"*sin(2*PI*t/37+1.7)'"
               << L",setsar=1,format=yuv420p,trim=duration=" << Number(duration);
        if (incomingFade > 0.0001)
            filter << L",tpad=start_mode=clone:start_duration=" << Number(incomingFade);
        // concat outputs AVTB while image streams normally use 1/fps. A later xfade
        // rejects that mixed time base and would silently send the app into the slow
        // native frame renderer. Normalize every clip so any cut/fade order is valid.
        filter << L",setpts=N/(" << fps << L"*TB),fps=" << fps
               << L",settb=AVTB[v" << i << L"];";
    }

    std::wstring current = L"v0";
    double currentDuration = std::max(0.05, timeline.clips.front().end - timeline.clips.front().start);
    for (std::size_t i = 1; i < timeline.clips.size(); ++i) {
        const double baseDuration = std::max(0.05, timeline.clips[i].end - timeline.clips[i].start);
        const double fade = SafeFade(timeline, i - 1);
        const std::wstring nextLabel = L"chain" + std::to_wstring(i);
        if (fade > 0.0001) {
            const double offset = std::max(0.0, currentDuration - fade);
            filter << L"[" << current << L"][v" << i << L"]xfade=transition=fade:duration="
                   << Number(fade) << L":offset=" << Number(offset) << L"[" << nextLabel << L"];";
        } else {
            filter << L"[" << current << L"][v" << i << L"]concat=n=2:v=1:a=0[" << nextLabel << L"];";
        }
        current = nextLabel;
        currentDuration += baseDuration;
    }

    filter << L"[" << current << L"]subtitles='" << EscapeFilterPath(assPath) << L"'";
    if (reuseUnchanged)
        filter << L":reuse_unchanged=1";
    else
        filter << L",mpdecimate=hi=0:lo=0:frac=0";
    filter << L"[outv]";
    return filter.str();
}

bool RunProcess(const fs::path& executable, const std::vector<std::wstring>& arguments,
                double duration, std::atomic_bool& cancelRequested,
                FfmpegVideoExporter::ProgressCallback progress, std::wstring& error) {
    std::wstring command;
    for (const auto& argument : arguments) {
        if (!command.empty()) command += L' ';
        command += QuoteArgument(argument);
    }

    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &security, 0)) {
        error = L"FFmpeg-Fortschrittspipe konnte nicht erstellt werden.";
        return false;
    }
    if (!SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        error = L"FFmpeg-Fortschrittspipe konnte nicht konfiguriert werden.";
        return false;
    }

    STARTUPINFOW startup{sizeof(startup)};
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');
    const BOOL created = CreateProcessW(executable.c_str(), mutableCommand.data(), nullptr, nullptr,
        TRUE, CREATE_NO_WINDOW, nullptr, arguments.back().empty() ? nullptr : fs::path(arguments.back()).parent_path().c_str(),
        &startup, &process);
    CloseHandle(writePipe);
    if (!created) {
        CloseHandle(readPipe);
        error = L"FFmpeg-Prozess konnte nicht gestartet werden.";
        return false;
    }

    std::string pending;
    std::string diagnostic;
    double lastProgress = -1.0;
    for (;;) {
        if (cancelRequested) {
            TerminateProcess(process.hProcess, 2);
            WaitForSingleObject(process.hProcess, 5000);
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            CloseHandle(readPipe);
            error = L"CANCELLED";
            return false;
        }

        DWORD available = 0;
        if (PeekNamedPipe(readPipe, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
            char buffer[4096]{};
            DWORD received = 0;
            if (ReadFile(readPipe, buffer, std::min<DWORD>(available, static_cast<DWORD>(sizeof(buffer))), &received, nullptr) && received > 0) {
                pending.append(buffer, buffer + received);
                diagnostic.append(buffer, buffer + received);
                if (diagnostic.size() > 6000) diagnostic.erase(0, diagnostic.size() - 6000);
                for (;;) {
                    const auto newline = pending.find('\n');
                    if (newline == std::string::npos) break;
                    std::string line = pending.substr(0, newline);
                    pending.erase(0, newline + 1);
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    constexpr std::string_view prefix = "out_time_us=";
                    if (!line.starts_with(prefix)) continue;
                    try {
                        const double seconds = std::stod(line.substr(prefix.size())) / 1'000'000.0;
                        const double fraction = std::clamp(seconds / std::max(0.001, duration), 0.0, 1.0);
                        if (fraction >= lastProgress + 0.002 || fraction >= 0.999) {
                            lastProgress = fraction;
                            progress(fraction, L"FAST Render: " + FormatTime(seconds) + L" / " + FormatTime(duration));
                        }
                    } catch (...) {
                    }
                }
            }
        }
        const DWORD wait = WaitForSingleObject(process.hProcess, 20);
        if (wait == WAIT_OBJECT_0) break;
        if (wait == WAIT_FAILED) {
            TerminateProcess(process.hProcess, 3);
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            CloseHandle(readPipe);
            error = L"Warten auf FFmpeg ist fehlgeschlagen.";
            return false;
        }
    }

    DWORD exitCode = 1;
    if (!GetExitCodeProcess(process.hProcess, &exitCode)) exitCode = 1;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CloseHandle(readPipe);
    if (exitCode != 0) {
        error = L"FFmpeg-Export fehlgeschlagen";
        if (!diagnostic.empty()) error += L": " + Utf8ToWide(diagnostic);
        return false;
    }
    progress(1.0, L"FAST Render abgeschlossen.");
    return true;
}

} // namespace

fs::path FfmpegVideoExporter::FindExecutable(const fs::path& appDirectory) {
    const fs::path bundled = appDirectory / L"tools" / L"ffmpeg.exe";
    if (fs::is_regular_file(bundled)) return bundled;
    std::wstring buffer(32768, L'\0');
    const DWORD size = SearchPathW(nullptr, L"ffmpeg.exe", nullptr,
                                   static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
    if (size > 0 && size < buffer.size()) {
        buffer.resize(size);
        return fs::path(buffer);
    }
    const fs::path common = L"C:\\ffmpeg\\bin\\ffmpeg.exe";
    if (fs::is_regular_file(common)) return common;
    return {};
}

bool FfmpegVideoExporter::Export(const fs::path& executable, const VideoExportJob& job,
                                 const LyricsDocument& document,
                                 std::atomic_bool& cancelRequested,
                                 ProgressCallback progress, std::wstring& error) {
    if (executable.empty() || !fs::is_regular_file(executable)) {
        error = L"FFmpeg wurde nicht gefunden.";
        return false;
    }
    if (document.Duration() <= 0.0) {
        error = L"Ungültige Songdauer für FFmpeg.";
        return false;
    }

    const fs::path assPath = job.outputPath.parent_path() /
        (job.outputPath.stem().wstring() + L".lyrics.tmp.ass");
    if (!WriteAss(assPath, document, job, error)) return false;
    auto cleanupAss = [&] {
        std::error_code ec;
        fs::remove(assPath, ec);
    };

    const bool multiImage = job.visualTimeline.Size() > 1;
    const bool reuseUnchanged = fs::is_regular_file(executable.parent_path() / L"klanggeist-ffmpeg.reuse");
    const fs::path singleImage = !job.visualTimeline.Empty()
        ? job.visualTimeline.clips.front().imagePath
        : job.coverPath;
    if (!multiImage && (singleImage.empty() || !fs::is_regular_file(singleImage))) {
        cleanupAss();
        error = L"Kein gültiges Bild für den schnellen FFmpeg-Export vorhanden.";
        return false;
    }

    std::vector<std::wstring> arguments{
        executable.wstring(), L"-hide_banner", L"-loglevel", L"error", L"-nostdin", L"-y"
    };

    if (multiImage) {
        for (const auto& clip : job.visualTimeline.clips) {
            arguments.emplace_back(L"-framerate");
            arguments.emplace_back(std::to_wstring(job.preset.fps));
            arguments.emplace_back(L"-i");
            arguments.emplace_back(clip.imagePath.wstring());
        }
        arguments.emplace_back(L"-i");
        arguments.emplace_back(job.audioPath.wstring());
        const std::size_t audioIndex = job.visualTimeline.Size();
        arguments.emplace_back(L"-filter_complex");
        arguments.emplace_back(BuildMultiImageFilter(assPath, job, reuseUnchanged));
        arguments.emplace_back(L"-map");
        arguments.emplace_back(L"[outv]");
        arguments.emplace_back(L"-map");
        arguments.emplace_back(std::to_wstring(audioIndex) + L":a:0");
    } else {
        arguments.emplace_back(L"-framerate");
        arguments.emplace_back(std::to_wstring(job.preset.fps));
        arguments.emplace_back(L"-i");
        arguments.emplace_back(singleImage.wstring());
        arguments.emplace_back(L"-i");
        arguments.emplace_back(job.audioPath.wstring());
        arguments.emplace_back(L"-map");
        arguments.emplace_back(L"0:v:0");
        arguments.emplace_back(L"-map");
        arguments.emplace_back(L"1:a:0");
        arguments.emplace_back(L"-vf");
        arguments.emplace_back(BuildSingleImageFilter(assPath, job, reuseUnchanged));
    }

    arguments.emplace_back(L"-c:v");
    arguments.emplace_back(L"libx264");
    arguments.emplace_back(L"-preset");
    arguments.emplace_back(L"veryfast");
    arguments.emplace_back(L"-crf");
    arguments.emplace_back(L"20");
    arguments.emplace_back(L"-pix_fmt");
    arguments.emplace_back(L"yuv420p");
    arguments.emplace_back(L"-fps_mode");
    arguments.emplace_back(L"vfr");
    arguments.emplace_back(L"-c:a");
    arguments.emplace_back(L"aac");
    arguments.emplace_back(L"-b:a");
    arguments.emplace_back(std::to_wstring(job.preset.audioBitrate));
    arguments.emplace_back(L"-t");
    arguments.emplace_back(Number(document.Duration()));
    arguments.emplace_back(L"-movflags");
    arguments.emplace_back(L"+faststart");
    arguments.emplace_back(L"-progress");
    arguments.emplace_back(L"pipe:1");
    arguments.emplace_back(L"-stats_period");
    arguments.emplace_back(L"0.25");
    arguments.emplace_back(job.outputPath.wstring());

    const bool success = RunProcess(executable, arguments, document.Duration(), cancelRequested,
                                    std::move(progress), error);
    cleanupAss();
    return success;
}

} // namespace kg
