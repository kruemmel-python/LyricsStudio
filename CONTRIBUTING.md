# Mitwirken

Fehlerberichte und Pull Requests sind willkommen.

## Fehler melden

Bitte angeben:

- Klanggeist-Version und Windows-Version
- betroffener Bereich: Transkription, Editor oder Videoexport
- genaue Schritte bis zum Fehler
- erwartetes und tatsächliches Ergebnis
- relevante Textausgabe ohne persönliche Pfade, Songtexte oder Zugangsdaten

Keine urheberrechtlich geschützten Songs, privaten Lyrics, `.hf`-Modelldateien, `klanggeist_studio.json` oder `tatarus_visual_brain.json` hochladen.

## Änderung einreichen

1. Repository forken und einen eigenen Branch erstellen.
2. Änderungen klein und nachvollziehbar halten.
3. Native Targets mit `BUILD_VS2022.bat` bauen.
4. Python-Datei mit `py -3.12 -m py_compile backend\whisper_worker.py` prüfen.
5. Bei UI- oder Verhaltensänderungen Handbuch und Changelog aktualisieren.
6. Pull Request mit Testbeschreibung eröffnen.

Alle C++-Warnungen gelten im Projekt als Fehler. Beiträge müssen unter der MIT-Lizenz des Projekts bereitgestellt werden können.
