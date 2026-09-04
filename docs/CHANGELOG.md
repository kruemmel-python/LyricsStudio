# Änderungsverlauf

## 2.1.1 – 2026-09-04

### Behoben

- Mehrbild-Exporte verwenden für alle Bildströme eine einheitliche FFmpeg-Zeitbasis. Mischungen aus hartem Schnitt und Crossfade laufen dadurch stabil.
- Standbilder werden im Filtergraphen wiederverwendet, statt dieselbe PNG-Datei für jedes Videobild neu zu dekodieren. Der reale Test mit 1:54 Minuten Audio und sechs Bildern sank von ungefähr 20 Minuten auf rund 21 Sekunden.
- Der unter `VIDEO-ORDNER` ausgewählte Zielordner wird unmittelbar vor jedem Export neu ausgewertet und zuverlässig verwendet.
- Fortschritts- und Abschlussmeldungen zeigen den vollständigen Zielpfad statt nur des Dateinamens.

### Veröffentlichung

- Reproduzierbares Skript für Portable-ZIP, MSI, FFmpeg-Quellpaket und SHA-256-Prüfsummen ergänzt.
- MSI-Paket auf Version 2.1.1 aktualisiert und um Handbuch, technische Dokumentation sowie sämtliche mitgelieferten Lizenztexte erweitert.
- GitHub-taugliche Ignore-Regeln verhindern die Veröffentlichung persönlicher Einstellungen, Songs, Lyrics, Videos, Whisper-Modelle und TATARUS-Lerndaten.
- Vollständiges UI-Handbuch sowie separate Dokumentationen für TATARUS, Architektur, Build, FFmpeg und Validierung ergänzt.

## 2.1.0 – 2026-08-30

- FAST Render Engine mit lokalem Klanggeist-FFmpeg eingeführt.
- Mehrbild-Timeline, harte Schnitte, Crossfades, Zoom, Drift und Albumcover-Sperre ergänzt.
- TATARUS Diversity Planner und Produktionsplanung aus Songabschnitten erweitert.
- Abbruch, Fortschrittsanzeige und atomare Veröffentlichung des fertigen Videos ergänzt.

## 2.0.0 bis 2.0.2

- Video-Exportbereich mit nativer Vorschau und Lyrics-Compositor eingeführt.
- Audioanalyse für Energie, Onsets, Pausen und Bereiche ergänzt.
- SMART-Planer und TATARUS Visual Brain integriert.
- Fehlerkorrekturen für Pfade, Zeitlinien und Media-Foundation-Verarbeitung.

## 1.9.0

- Lyrics Editor um segmentbezogene Wiedergabe, Confidence-Prüfstellen und strukturierte Korrektur erweitert.

## 1.8.0

- Native Audiowiedergabe und synchronisierte Segmentnavigation ergänzt.

## 1.7.0

- Lokalen Whisper-Worker als dauerhaften, getrennten Python-Prozess eingeführt.
- Fortschritt, Abbruch und Wiederaufnahme stabilisiert.

## 1.6.0

- Stapelverarbeitung, Ordnerüberwachung und Skip-/Resume-Metadaten ergänzt.

## 1.5.0

- Erste zusammenhängende native Windows-Oberfläche für Transkription und Lyrics-Bearbeitung.
- Ausgabe als TXT, LRC, SRT und strukturiertes JSON.
