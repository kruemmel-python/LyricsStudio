# Klanggeist Lyrics Studio 2.1.1

Version 2.1.1 ist ein Stabilitäts- und Veröffentlichungsrelease für Windows 10/11 x64.

## Wichtigste Änderungen

- Mehrbild-Videoexport massiv beschleunigt: Standbilder werden nicht mehr für jedes einzelne Frame erneut dekodiert.
- FFmpeg-Zeitbasis für Misch-Timelines aus Crossfade und hartem Schnitt korrigiert.
- Der unter **VIDEO-ORDNER** gewählte Zielordner wird nun zuverlässig verwendet.
- Vollständiges deutsches UI-Handbuch und ausführliche TATARUS-Dokumentation ergänzt.
- Saubere Portable- und MSI-Pakete inklusive Drittanbieter- und Laufzeitlizenzen.
- Vollständiger Quellcode des mitgelieferten gepatchten FFmpeg-Forks als separates Release-Asset.

## Download

- **Portable ZIP:** entpacken und `KlanggeistLyricsStudio.exe` starten.
- **MSI:** reguläre Installation mit Startmenü- und Desktop-Verknüpfung.
- **FFmpeg Source:** korrespondierender Quellcode der im Paket enthaltenen GPL-FFmpeg-Ausgabe.
- **SHA256SUMS.txt:** Prüfsummen aller Release-Pakete.

Für Transkription muss einmal `INSTALL_BACKEND.bat` ausgeführt werden. Videoexport und Bearbeitung vorhandener `.lyrics.json`-Dateien funktionieren ohne Python-Backend.

Die Binärdateien sind nicht kommerziell codesigniert; Windows SmartScreen kann deshalb beim ersten Start nachfragen.

- Vollständige Bedienung: [UI-Handbuch](https://github.com/kruemmel-python/LyricsStudio/blob/v2.1.1/docs/HANDBUCH.md)
- TATARUS erklärt: [TATARUS im Detail](https://github.com/kruemmel-python/LyricsStudio/blob/v2.1.1/docs/TATARUS.md)
- Änderungsverlauf: [CHANGELOG](https://github.com/kruemmel-python/LyricsStudio/blob/v2.1.1/docs/CHANGELOG.md)
