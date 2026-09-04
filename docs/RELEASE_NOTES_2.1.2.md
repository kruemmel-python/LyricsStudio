# Klanggeist Lyrics Studio 2.1.2

Version 2.1.2 behebt die Zuordnung und Anzeige neu erzeugter Lyrics.

## Behoben

- Der unter **AUSGABE** gewählte Lyrics-Ordner wird geprüft, sofort gespeichert und fest an die gestartete Queue gebunden.
- Nach jeder Transkription übernimmt die Oberfläche den exakten, vom Whisper-Worker gemeldeten JSON-Pfad.
- Wird ein Song mit demselben Dateinamen erneut transkribiert, lädt der Editor die neue Datei von der Festplatte und zeigt nicht länger den alten Inhalt aus dem Arbeitsspeicher.
- Eine vorhandene gleichnamige Ausgabe wird nur als aktuell erkannt, wenn sie auch zum identischen Quellpfad gehört.
- Der vollständige Zielpfad wird unter der Queue als `Schreibe nach`, `Gespeichert` oder `Vorhanden` angezeigt.
- Gleichnamige Songs aus verschiedenen Unterordnern erscheinen mit ihrem relativen Pfad in der Editorliste.
- Neue Queue-Einträge können mit `START` verarbeitet werden, während das bereits geladene Whisper-Modell weiterläuft.
- Die Portable-Paketierung nimmt nur freigegebene Projektdateien auf und schließt lokale Zusatzdateien zuverlässig aus.

## Bereits in 2.1.1 enthalten

- Beschleunigter Mehrbild-Videoexport.
- Korrigierte FFmpeg-Zeitbasis für gemischte Cut-/Crossfade-Timelines.
- Zuverlässige Verwendung des gewählten Video-Ausgabeordners.
- Ausführliches UI-Handbuch und vollständige TATARUS-Dokumentation.

## Installation

Die MSI-Version 2.1.2 aktualisiert eine vorhandene 2.1.1-Installation. Klanggeist vor dem Start des Installers schließen. Bereits erzeugte Lyrics und der Whisper-Modellcache werden nicht als Bestandteil des MSI entfernt.

Die Binärdateien sind nicht kommerziell codesigniert; Windows SmartScreen kann deshalb beim ersten Start nachfragen.

- [UI-Handbuch](https://github.com/kruemmel-python/LyricsStudio/blob/v2.1.2/docs/HANDBUCH.md)
- [TATARUS im Detail](https://github.com/kruemmel-python/LyricsStudio/blob/v2.1.2/docs/TATARUS.md)
- [Änderungsverlauf](https://github.com/kruemmel-python/LyricsStudio/blob/v2.1.2/docs/CHANGELOG.md)
