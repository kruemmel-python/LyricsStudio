# Validierung von Version 2.1.2

Dieses Dokument hält die für das öffentliche Release am 4. September 2026 ausgeführten Prüfungen fest.

## Prüfumgebung

- Windows 11 x64
- Visual Studio 2022, x64-Toolchain
- CMake mit Visual-Studio-2022-Generator
- WiX Toolset 7.0.0 lokal und 6.0.2 im öffentlichen CI-Build
- Python 3.12
- Klanggeist-FFmpeg `8.0.git-klanggeist`, Basiscommit `b397eba2f0d3d86daf1098d0f27daffccc74fea5`

## Quellcodeprüfungen

- C++-Hauptprogramm mit `/W4`, `/WX`, `/permissive-`, C++20: **bestanden**
- `KlanggeistVideoSmoke`: **kompiliert**
- `KlanggeistExportControllerSmoke`: **kompiliert und vollständiger Export ausgeführt**
- `KlanggeistLyricsRefreshSmoke`: **kompiliert und ausgeführt**; eine am selben Pfad ersetzte JSON-Datei liefert nach dem Neuladen ausschließlich den neuen Segmenttext
- Python-Syntaxprüfung von `backend/whisper_worker.py`: **bestanden**
- Zwei Python-Regressionstests für den gewählten Ausgabeordner und die Quellidentität gleichnamiger Dateien: **bestanden**
- FFmpeg-Patch gegen den dokumentierten Basiscommit: **bestanden**; der Rückwärtstest entspricht exakt der einzigen Änderung `libavfilter/vf_subtitles.c`
- Prüfung auf versehentlich enthaltene lokale Pfade, Testtitel, Zugangsdaten und Geheimnismuster: **bestanden**

## Funktionsprüfung Videoexport

Referenzfall:

- Audio: 1:54,88 Minuten
- Bilder: sechs Motive aus einem lokalen Viking-Testset
- Cover: separates ausgewähltes Albumcover
- Ausgabe: H.264/AAC, 1920 × 1080, 30 FPS
- Timeline: Mischung aus Crossfades und harten Schnitten

Ergebnis:

- Exportdauer: **20,88 Sekunden**
- Videodauer: **114,88 Sekunden**
- Verhältnis: rund **5,5-mal schneller als Echtzeit**
- MP4 vollständig erstellt und dekodierbar: **bestanden**

Damit wurde die zuvor beobachtete Laufzeit von ungefähr 20 Minuten beseitigt. Der Test nutzt die neue einmalige Bilddekodierung im Filtergraphen und die gemeinsame AVTB-Zeitbasis für Cut-/Crossfade-Mischungen.

## Zielordnerprüfung

Der mit `VIDEO-ORDNER` gewählte Pfad muss sowohl vor dem Start angezeigt als auch tatsächlich als Elternordner der fertigen MP4 verwendet werden.

Zusätzlich zum UI-Pfadtest wurde der vollständige Exportcontroller mit einem ausdrücklich gewählten, verschachtelten Testordner ausgeführt. Die atomar veröffentlichte MP4 lag genau dort, hatte 7.260.210 Bytes und wurde anschließend ohne Dekodierfehler vollständig durch FFmpeg gelesen. Ergebnis: **bestanden**.

## Lyrics-Ausgabe und Aktualisierung

- Der gewählte Lyrics-Ausgabeordner wird vor dem Start angelegt bzw. auf Schreibbarkeit geprüft: **bestanden**
- Jeder wartende Auftrag erhält beim Queue-Start eine feste Kopie dieses Zielordners: **bestanden**
- Der Whisper-Worker schreibt TXT, LRC, SRT und JSON unterhalb genau dieses Ordners: **bestanden**
- Eine vorhandene gleichnamige JSON-Datei wird nur übersprungen, wenn vollständiger Quellpfad, Größe, Änderungszeit und Whisper-Modell übereinstimmen: **bestanden**
- Der vom Worker gemeldete vollständige `json_path` wird nach Abschluss automatisch ausgewählt: **bestanden**
- Ein zuvor geladenes Dokument am selben Pfad wird ohne ungespeicherte Änderungen neu von der Festplatte gelesen: **bestanden**
- Ungespeicherte manuelle Editoränderungen werden durch eine Aktualisierung nicht verworfen: **bestanden**

## Paketprüfung

- Portable-ZIP ist lesbar und enthält ausschließlich die feste Freigabeliste sowie die benötigten FFmpeg-Laufzeitdateien: **bestanden**
- Portable-ZIP enthält EXE, Backend, Preset, FFmpeg, Dokumentation und Lizenztexte: **bestanden**
- Portable-ZIP enthält keine `.hf`, Einstellungen, Lerndaten, Lyrics, Videos, Test-EXEs oder Debugsymbole: **bestanden**
- FFmpeg-Quell-ZIP ist lesbar, enthält 10.714 Archiveinträge, Patchinformation und den geänderten Quelltext: **bestanden**
- MSI besteht die WiX-Validierung: **bestanden**
- MSI lässt sich unbeaufsichtigt installieren und wieder entfernen: **bestanden**
- MSI-Paketinhalt einschließlich Produktversion 2.1.2, Handbuch, TATARUS-Dokument und Laufzeitlizenzen: **bestanden**
- Upgrade-Code entspricht Version 2.1.1; WiX `MajorUpgrade` erkennt 2.1.2 als Aktualisierung: **bestanden**
- SHA-256-Datei umfasst alle drei veröffentlichten Pakete und wurde unabhängig nachgerechnet: **bestanden**

## Release-Dateien

Geprüft werden `KlanggeistLyricsStudio-2.1.2-portable.zip`, `KlanggeistLyricsStudio-2.1.2-x64.msi` und `Klanggeist-FFmpeg-8.0-git-b397eba2-source.zip`. Die verbindlichen SHA-256-Werte stehen in der zusammen mit diesen Dateien erzeugten `SHA256SUMS.txt`.

Die Prüfsummen werden bewusst nicht hier wiederholt: Dieses Dokument ist selbst Bestandteil von Portable-ZIP und MSI. Eine darin eingebettete Prüfsumme des eigenen Pakets würde den Paketinhalt und damit die Prüfsumme erneut verändern.

## Bekannte Veröffentlichungseigenschaft

Die Release-Dateien sind nicht mit einem kommerziellen Windows-Code-Signing-Zertifikat signiert. Eine SmartScreen-Rückfrage ist daher möglich und stellt für sich allein keinen Paketfehler dar.
