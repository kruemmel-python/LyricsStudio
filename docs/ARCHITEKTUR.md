# Architektur – Klanggeist Lyrics Studio 2.1.2

## 1. Zielbild

Klanggeist trennt interaktive Windows-Funktionen, lokale Whisper-Inferenz und Video-Encoding in klar abgegrenzte Komponenten:

```text
┌──────────────────────────────────────────────────────────────┐
│ KlanggeistLyricsStudio.exe · C++20 / Win32                  │
│                                                              │
│ Direct2D/DirectWrite UI                                      │
│  ├─ Queue und Einstellungen                                  │
│  ├─ LyricsDocument und Editor                                │
│  ├─ Audio Intelligence / Image Intelligence                  │
│  ├─ SongStructure / TATARUS / VisualTimeline                 │
│  └─ Vorschau und Exportsteuerung                             │
│          │                                   │               │
│          ▼                                   ▼               │
│ Media Foundation + XAudio2        FFmpeg FAST Render Engine  │
│                                      │ Fallback               │
│                                      ▼                        │
│                         Direct2D + Media Foundation Encoder   │
└──────────────────────┬───────────────────────────────────────┘
                       │ UTF-8 JSON Lines über Pipes
                       ▼
┌──────────────────────────────────────────────────────────────┐
│ backend/whisper_worker.py                                    │
│ persistentes faster-whisper-/CTranslate2-Modell              │
└──────────────────────────────────────────────────────────────┘
```

## 2. Native Oberfläche

`src/main.cpp` enthält Fenster, Zustandsmodell, Zeichnung und Eingabebehandlung. Verwendet werden:

- Win32 für Fenster, Nachrichten, Tastatur, Maus und Dialoge,
- Direct2D für Panels, Buttons, Chips, Timeline und Fortschritt,
- DirectWrite für alle gezeichneten Texte,
- ein natives Win32-`EDIT`-Control für die Lyrics-Eingabe.

Es gibt keine Qt-, ImGui-, wxWidgets-, GTK-, SDL-, Electron- oder WebView-Abhängigkeit.

Die Anwendung ist Per-Monitor-DPI-aware und besitzt eine Mindestfenstergröße von 1200 × 820 logischen Pixeln.

## 3. Transkriptionsprozess

### JobController

`src/job_controller.*` startet Python mit `CreateProcessW` und verbindet stdin/stdout über Pipes. Die UI bleibt dadurch unabhängig vom Python-Prozess und wird beim Laden oder Transkribieren nicht blockiert.

### Persistenter Worker

`backend/whisper_worker.py` lädt genau ein Whisper-Modell und verarbeitet danach mehrere `transcribe`-Kommandos. Das verhindert wiederholtes Laden großer Modelldateien.

### Protokoll

C++ sendet eine JSON-Zeile:

```json
{"cmd":"transcribe","path":"D:\\Songs\\Titel.wav","input_root":"D:\\Songs","output_root":"D:\\Lyrics","overwrite":false}
```

Der Worker meldet Ereignisse wie:

```json
{"type":"progress","path":"D:\\Songs\\Titel.wav","progress":0.64,"segment":"..."}
```

Unterstützte Ereignisse:

- `loading`
- `ready`
- `started`
- `progress`
- `done`
- `skipped`
- `error`
- `fatal`
- `bye`

### Skip-/Resume-Prüfung

Eine vorhandene Transkription gilt als aktuell, wenn:

- TXT, LRC, SRT und JSON existieren,
- Dateigröße der Quelle übereinstimmt,
- Änderungszeit der Quelle übereinstimmt,
- verwendetes Whisper-Modell übereinstimmt,
- Overwrite nicht aktiviert ist.

## 4. Lyrics-Datenmodell

`src/lyrics_document.*` lädt und speichert die vier zusammengehörigen Formate. Ein Segment enthält:

```text
start
end
text
avg_logprob
no_speech_prob
compression_ratio
edited
```

Die Prüfstelle-Heuristik nutzt niedrige Log-Wahrscheinlichkeit, hohe No-Speech-Wahrscheinlichkeit und auffällige Kompressionswerte als Review-Hinweis.

Schreibvorgänge erfolgen über temporäre Dateien und anschließendes Umbenennen. So bleiben zuvor gültige Dateien erhalten, falls der Schreibvorgang fehlschlägt.

## 5. Eigener JSON-Layer

`src/mini_json.*` implementiert den benötigten JSON-Teilumfang:

- Object und Array,
- String einschließlich `\uXXXX`,
- Number,
- Boolean,
- Null,
- Parser und formatierter Serializer.

Dadurch benötigt die C++-Anwendung keine zusätzliche JSON-Bibliothek.

## 6. Audio

### Wiedergabe

`src/audio_player.*` dekodiert Audio über Media Foundation in PCM, hält es für schnelle Segmentzugriffe im Speicher und spielt Bereiche mit XAudio2 ab.

### Analyse

`src/audio_analysis.*` erzeugt aus 50-ms-Fenstern eine Energiehüllkurve, adaptive Onsets, Pausen und Energieabschnitte. Die Datenmenge ist klein und für UI sowie TATARUS direkt nutzbar.

## 7. Bildanalyse

`src/image_analysis.*` verwendet Windows Imaging Component. Für die Merkmalsextraktion wird ein Bild auf maximal 128 Pixel an der längsten Seite skaliert. Berechnet werden acht normalisierte Merkmale. Die Bilddatei selbst wird nicht geändert.

## 8. Songstruktur und TATARUS

- `src/song_structure.*`: heuristische Gliederung aus Energie, Pausen, Lyrics-Dichte und Wiederholungen.
- `src/tatarus_visual_brain.*`: lokaler Präferenzlerner für Cut, Stil und Bild-Matching.
- `src/production_planner.*`: vollständige abschnittsabhängige Produktionsplanung mit Albumcover-Lock und Diversity-Regeln.
- `src/visual_timeline.hpp`: gemeinsames Datenmodell für Vorschau und Export.

Ein `VisualClip` enthält Bildpfad, Start, Ende, Übergang, Übergangsdauer und `VisualStyle` mit Zoom-, Drift- und Lyrics-Multiplikator.

Die interne Funktionsweise ist in [TATARUS.md](TATARUS.md) beschrieben.

## 9. Vorschau

`src/video_renderer.*`, `src/cover_image.*`, `src/cover_motion.*` und `src/lyrics_compositor.*` erzeugen ein BGRA-Frame über Direct2D/DirectWrite. Derselbe Timeline-Zustand steuert Vorschau und nativen Exportfallback.

Die Vorschau rendert nur das aktuell benötigte Frame. Während der Wiedergabe aktualisiert ein 33-ms-Timer die Position und fordert neue Frames an.

## 10. Videoexport

### ExportController

`src/video_export_controller.*` arbeitet in einem separaten `std::jthread`, meldet Zustände per Win32-Nachricht und verwaltet:

- Vorbereitung,
- Rendering,
- Finalisierung,
- Abbruch,
- Fehler,
- atomare Veröffentlichung.

Zuerst wird in eine Datei nach dem Muster `Titel.tmp.mp4` geschrieben. Nur bei Erfolg ersetzt `MoveFileExW` die endgültige Zieldatei.

### FFmpeg FAST Render Engine

`src/ffmpeg_video_exporter.*`:

1. schreibt aus den Lyrics eine temporäre ASS-Datei,
2. bindet jedes Bild genau einmal ein,
3. wiederholt das dekodierte Frame innerhalb des Filtergraphs,
4. skaliert und bewegt jeden Clip,
5. kombiniert Hard Cuts über `concat` und Crossfades über `xfade`,
6. normalisiert alle Clip-Zeitbasen auf AVTB,
7. rendert Lyrics mit libass,
8. kodiert H.264/AAC und meldet Fortschritt über `-progress pipe:1`.

Die AVTB-Normalisierung verhindert, dass eine Folge aus `concat` und späterem `xfade` wegen unterschiedlicher Zeitbasen fehlschlägt.

### Native Rückfallebene

Wenn FFmpeg fehlt oder fehlschlägt, rendert `VideoRenderer` jedes Frame und `src/mp4_encoder.*` schreibt es mit Media Foundation als H.264/AAC-MP4. Dieser Pfad ist robust, aber bei 1080p wesentlich langsamer.

## 11. Konfiguration und lokale Daten

Alle Zustandsdateien liegen relativ zur laufenden EXE:

| Datei/Ordner | Eigentümer | Versioniert? |
|---|---|---|
| `klanggeist_studio.json` | lokale UI-Einstellungen | Nein |
| `tatarus_visual_brain.json` | persönliche Lerngewichte | Nein |
| `.hf/` | lokaler Modellcache | Nein |
| `lyrics/` | Standardausgabe | Nein |
| `videos/` | Standardausgabe | Nein |
| `presets/` | mitgeliefertes Video-Preset | Ja |
| `tools/` | FFmpeg und Laufzeitbibliotheken | Ja |

## 12. Quellstruktur

```text
assets/                 Icon und Markenassets
backend/                Python-Worker
docs/                   Handbuch und technische Dokumentation
installer/              WiX-MSI-Definition
patches/                reproduzierbarer FFmpeg-Patch
presets/                Video-Preset
src/                    native Anwendung
tests/                  Windows-Smoke-Tests
tools/                  gebündelter FFmpeg und DLLs
CMakeLists.txt          Builddefinition
BUILD_VS2022.bat        Entwicklerbuild
BUILD_MSI.ps1           MSI-Build
BUILD_RELEASE.ps1       vollständiger Release-Build
```

## 13. Qualitätsregeln

- C++20
- MSVC `/W4 /WX`
- `/permissive-`, `/utf-8`, `/Zc:__cplusplus`
- `CMAKE_COMPILE_WARNING_AS_ERROR=ON`
- keine Warnungsunterdrückung im Projekt
- Hauptprogramm und Smoke-Test-Targets müssen kompilieren
- Pakete dürfen keine lokalen Einstellungen, Modellcaches, Lyrics oder Videos enthalten
