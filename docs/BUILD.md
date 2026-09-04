# Build und Release

## 1. Unterstützte Build-Umgebung

Klanggeist Lyrics Studio ist eine native Windows-Anwendung. Der vollständige Build wird deshalb unter Windows ausgeführt.

Erforderlich:

- Windows 10/11 x64
- Visual Studio 2022 mit Workload „Desktopentwicklung mit C++“
- CMake 3.24 oder neuer
- Windows SDK
- PowerShell 7 oder Windows PowerShell 5.1
- Python 3.12 für Backend-Syntaxprüfung und Laufzeit
- .NET SDK und WiX Toolset 7 für MSI

WiX kann installiert werden mit:

```powershell
dotnet tool install --global wix --version 7.0.0
```

## 2. Entwicklerbuild

```bat
BUILD_VS2022.bat
```

Das Skript konfiguriert `build/` für Visual Studio 2022 x64 und baut:

- `KlanggeistLyricsStudio`
- `KlanggeistVideoSmoke`
- `KlanggeistExportControllerSmoke`

Alle C++-Targets verwenden Warnungen als Fehler.

Ausgabe:

```text
build\Release\KlanggeistLyricsStudio.exe
```

Die CMake-Post-Build-Schritte kopieren Backend, Preset, FFmpeg-Werkzeuge, Lizenzdateien und Dokumentation neben die EXE. Lokale Einstellungen oder Modellcaches werden nicht kopiert.

## 3. Direkter CMake-Build

```powershell
$cmake = "$env:ProgramFiles\CMake\bin\cmake.exe"
& $cmake -S . -B build -G "Visual Studio 17 2022" -A x64
& $cmake --build build --config Release --target KlanggeistLyricsStudio --parallel
```

Wichtige Compileroptionen:

```text
/W4 /WX /permissive- /EHsc /utf-8 /Zc:__cplusplus
```

## 4. Python-Backend

Abhängigkeiten:

```powershell
py -3.12 -m pip install -r requirements.txt
```

Syntaxprüfung:

```powershell
py -3.12 -m py_compile backend\whisper_worker.py
```

Modelldateien gehören nicht in Build oder Repository. Sie werden bei der ersten Verwendung in `.hf/` geladen.

## 5. MSI bauen

```powershell
.\BUILD_MSI.ps1 -Configuration Release -Version 2.1.1
```

Das Skript:

1. konfiguriert CMake,
2. baut das Hauptprogramm,
3. erzeugt das per-user MSI mit WiX,
4. validiert das MSI,
5. schreibt es nach `dist/`.

Ausgabe:

```text
dist\KlanggeistLyricsStudio-2.1.1-x64.msi
```

Installationsziel:

```text
%LOCALAPPDATA%\Programs\Klanggeist Lyrics Studio
```

Das Paket legt Startmenü- und Desktop-Verknüpfungen an. Die MSI-Datei ist ohne separat konfiguriertes Zertifikat nicht codesigniert.

## 6. Vollständigen Release bauen

```powershell
.\BUILD_RELEASE.ps1 -Version 2.1.1
```

Erzeugt:

- Portable-ZIP
- MSI
- SHA-256-Prüfsummen

Wenn der vollständige gepatchte FFmpeg-Quellbaum lokal vorhanden ist:

```powershell
.\BUILD_RELEASE.ps1 `
  -Version 2.1.1 `
  -FfmpegSourceDir D:\Pfad\zu\ffmpeg-klanggeist
```

Dann entsteht zusätzlich der vollständige FFmpeg-Quellcode als Release-Asset.

## 7. Portable-Paket

Das Portable-ZIP enthält:

```text
KlanggeistLyricsStudio-2.1.1-portable\
  KlanggeistLyricsStudio.exe
  INSTALL_BACKEND.bat
  requirements.txt
  README.md
  LICENSE
  THIRD_PARTY_NOTICES.md
  assets\
  backend\
  docs\
  presets\
  tools\
```

Bewusst ausgeschlossen:

- `.hf/`
- `klanggeist_studio.json`
- `tatarus_visual_brain.json`
- persönliche Lyrics und Videos
- Testprogramme und Debugsymbole
- Build-Zwischendateien

## 8. FFmpeg-Fork reproduzieren

Basiscommit:

```text
b397eba2f0d3d86daf1098d0f27daffccc74fea5
```

```powershell
git clone https://github.com/FFmpeg/FFmpeg.git vendor\ffmpeg-klanggeist
git -C vendor\ffmpeg-klanggeist checkout b397eba2f0d3d86daf1098d0f27daffccc74fea5
git -C vendor\ffmpeg-klanggeist apply ..\..\patches\ffmpeg-reuse-unchanged.patch
```

Der eigentliche Build erfolgt in MSYS2/UCRT64. Konfiguration und Zweck des Patches stehen in [FFMPEG.md](FFMPEG.md).

## 9. Release-Checkliste

Vor einem Tag müssen folgende Punkte erfüllt sein:

- Versionsnummern in CMake, RC, UI, Backend und WiX stimmen überein.
- `git status` enthält keine persönlichen oder generierten Dateien.
- Python-Syntaxprüfung ist erfolgreich.
- Hauptprogramm und beide Smoke-Targets bauen mit `/WX`.
- Portable-ZIP enthält weder Einstellungen noch `.hf`.
- MSI-Validierung ist erfolgreich.
- FFmpeg-Mischgraph aus Cut und Crossfade ist erfolgreich.
- Vollständiger FFmpeg-Quellcode wird als Release-Asset angeboten.
- `SHA256SUMS.txt` umfasst alle hochgeladenen Binär-/Quellpakete.
- Release-Notizen nennen fehlendes Code Signing.

## 10. GitHub Actions

`.github/workflows/windows-build.yml` baut bei Push, Pull Request und manueller Ausführung auf `windows-latest`. Die Action veröffentlicht Build-Artefakte, erstellt jedoch nicht selbständig einen öffentlichen Release. Releases werden bewusst nach lokaler Prüfung erstellt.
