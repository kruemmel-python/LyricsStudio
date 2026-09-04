#include "lyrics_document.hpp"

#include <iostream>

using namespace kg;

namespace {

std::string Document(std::string_view text) {
    return "{\n"
           "  \"source\": {\"file\": \"C:/missing/song.mp3\", \"relative_file\": \"song.mp3\"},\n"
           "  \"transcription\": {\"duration_seconds\": 10.0, \"segments\": ["
           "{\"start\": 0.0, \"end\": 2.0, \"text\": \"" + std::string(text) + "\"}]}\n"
           "}\n";
}

} // namespace

int wmain() {
    const auto root=fs::temp_directory_path()/(L"KlanggeistLyricsRefreshSmoke-"+std::to_wstring(GetCurrentProcessId()));
    const auto jsonPath=root/L"song.lyrics.json";
    std::error_code cleanupError;
    try {
        AtomicWriteUtf8(jsonPath,Document("ALTER TEXT"));
        LyricsDocument first;std::wstring error;
        if(!first.Load(jsonPath,root,error)||first.Segments().size()!=1||first.Segments()[0].text!=L"ALTER TEXT")return 2;

        AtomicWriteUtf8(jsonPath,Document("NEUER TEXT"));
        LyricsDocument refreshed;
        if(!refreshed.Load(jsonPath,root,error)||refreshed.Segments().size()!=1||refreshed.Segments()[0].text!=L"NEUER TEXT")return 3;

        const auto discovered=DiscoverLyricsJson(root);
        if(discovered.size()!=1||discovered[0].filename()!=jsonPath.filename())return 4;
        fs::remove_all(root,cleanupError);
        std::wcout<<L"LYRICS_REFRESH_OK\n";return 0;
    } catch(const std::exception& ex) {
        fs::remove_all(root,cleanupError);std::cerr<<ex.what()<<'\n';return 5;
    }
}
