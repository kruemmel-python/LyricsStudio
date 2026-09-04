# UI-Handbuch – Klanggeist Lyrics Studio 2.1.1

Dieses Handbuch beschreibt die Oberfläche, jeden Button und einen vollständigen Arbeitsablauf vom Song bis zum fertigen Lyrics-Video.

## 1. Programmaufbau

Links befindet sich die Navigation mit vier Bereichen:

| Bereich | Aufgabe |
|---|---|
| `TRANSKRIBIEREN` | Audio-/Videodateien mit Whisper in Lyrics-Dateien umwandeln |
| `LYRICS EDITOR` | Segmente anhören, prüfen, korrigieren und speichern |
| `VIDEO EXPORT` | Bilder planen, TATARUS anwenden, Vorschau ansehen und MP4 exportieren |
| `SETTINGS` | Aktive Backend-, Cache- und Ausgabeinformationen anzeigen |

Unten links zeigt der Backend-Status:

- `OFFLINE`: Der Whisper-Worker läuft nicht. Das ist normal, solange keine Transkription gestartet wurde.
- `MODELL LÄDT`: Python und das gewählte Whisper-Modell werden gestartet.
- `WHISPER BEREIT`: Das Modell ist geladen und kann Queue-Einträge verarbeiten.

## 2. Erster Start

### Transkription vorbereiten

1. Python 3.12 installieren.
2. Im Programmordner `INSTALL_BACKEND.bat` einmal ausführen.
3. Klanggeist starten.
4. Im Bereich `TRANSKRIBIEREN` den gewünschten Ausgabeordner festlegen.

Der erste Start eines Whisper-Modells kann länger dauern, weil mehrere Gigabyte in den lokalen `.hf`-Ordner geladen werden. Spätere Starts verwenden diesen Cache.

### Ohne Transkription arbeiten

Wer bereits passende `.lyrics.json`-Dateien besitzt, kann diese über den Lyrics-Ausgabeordner laden und Editor sowie Videoexport ohne laufendes Python-Backend verwenden.

## 3. TRANSKRIBIEREN

### `+ DATEI`

Öffnet eine Dateiauswahl und fügt genau eine Audio- oder Videodatei zur Queue hinzu.

Unterstützte Eingaben:

- MP3, WAV, FLAC
- M4A, AAC
- OGG, OPUS, WMA
- MP4, MKV, WEBM

Die Datei wird noch nicht sofort transkribiert. Erst `START` verarbeitet die Queue.

### `+ ORDNER`

Wählt einen Musikordner. Klanggeist durchsucht ihn einschließlich aller Unterordner nach unterstützten Medien und fügt noch nicht bekannte Dateien zur Queue hinzu.

Der gewählte Ordner wird zugleich als Watch-Ordner gespeichert.

### `AUSGABE`

Legt den Basisordner für TXT, LRC, SRT und JSON fest. Bei einem importierten Ordner bleibt dessen Unterordnerstruktur in der Ausgabe erhalten.

Beispiel:

```text
Quelle:  D:\Musik\Album\Titel.wav
Ausgabe: D:\Lyrics

D:\Lyrics\Album\Titel.lyrics.txt
D:\Lyrics\Album\Titel.lyrics.lrc
D:\Lyrics\Album\Titel.lyrics.srt
D:\Lyrics\Album\Titel.lyrics.json
```

### `START`

Startet den persistenten Python-Worker, lädt das ausgewählte Whisper-Modell einmal und verarbeitet anschließend alle wartenden Queue-Einträge der Reihe nach.

Der Worker bleibt für die gesamte Queue aktiv. Dadurch wird ein großes Modell nicht für jeden Song erneut geladen.

### `STOP`

Beendet den Whisper-Worker und markiert einen gerade laufenden Job als abgebrochen. Bereits vollständig gespeicherte Ergebnisse bleiben erhalten.

### `QUEUE LEEREN`

Entfernt die angezeigten Queue-Einträge, solange kein Job aktiv ist. Bereits erzeugte Dateien werden nicht gelöscht.

### Einstellungs-Chips

Ein Klick wechselt jeweils zyklisch zur nächsten Option.

#### `MODEL`

| Wert | Charakteristik |
|---|---|
| `large-v3` | Höchste der angebotenen Genauigkeiten, größter Speicher- und Zeitbedarf |
| `turbo` | Schneller Whisper-Ableger mit gutem Verhältnis aus Tempo und Genauigkeit |
| `medium` | Kleineres Modell, weniger Ressourcenbedarf |
| `small` | Schnellstes und kompaktestes angebotenes Modell, geringere Genauigkeit möglich |

#### `DEVICE`

- `cpu`: Inferenz auf dem Prozessor.
- `cuda`: Inferenz auf einer kompatiblen NVIDIA-GPU.

Beim Gerätewechsel setzt Klanggeist automatisch einen passenden Standard für `PRECISION`.

#### `PRECISION`

CPU:

- `int8`: Speicher- und meist zeitsparend; Standard.
- `float32`: Höhere Rechengenauigkeit, deutlich mehr Ressourcenbedarf.

CUDA:

- `float16`: Typischer GPU-Standard.
- `int8_float16`: Gemischte Quantisierung zur Reduktion des Ressourcenbedarfs.

#### `LANG`

- `auto`: Whisper erkennt die Sprache selbst.
- `de`: Deutsch erzwingen.
- `en`: Englisch erzwingen.

Eine fest vorgegebene Sprache kann Fehlentscheidungen der automatischen Erkennung vermeiden, ist bei mehrsprachigen Songs aber möglicherweise ungeeignet.

#### `WATCH ON` / `WATCH OFF`

Bei `WATCH ON` scannt Klanggeist den zuletzt gewählten Musikordner alle zehn Sekunden erneut. Neue unterstützte Dateien werden der Queue hinzugefügt. Ein bereits bekannter Pfad wird nicht doppelt eingereiht.

#### `SKIP AKTUELL` / `OVERWRITE`

- `SKIP AKTUELL`: Überspringt einen Song, wenn alle vier Ausgabedateien existieren und Dateigröße, Änderungszeit sowie Whisper-Modell noch übereinstimmen.
- `OVERWRITE`: Transkribiert auch aktuelle Dateien erneut und ersetzt ihre Ausgabe.

### Queue-Status

| Status | Bedeutung |
|---|---|
| `WARTET` | Noch nicht verarbeitet |
| `LÄUFT` | Aktive Transkription |
| `FERTIG` | Erfolgreich gespeichert |
| `SKIP` | Vorhandene Ausgabe war aktuell |
| `FEHLER` | Verarbeitung oder Backend ist fehlgeschlagen |

## 4. LYRICS EDITOR

Der Editor besteht aus Songliste, Segmentliste und Bearbeitungsbereich.

### Songliste

Sie enthält alle `*.lyrics.json`-Dateien im aktuellen Lyrics-Ausgabeordner und dessen Unterordnern. Ein Klick lädt Song, Audioquelle, Zeitsegmente und Confidence-Metadaten.

### Segmentliste

Jede Zeile zeigt Startzeit und Text. Orange markierte Zeiten weisen auf eine automatische Prüfstelle hin. Eine Prüfstelle ist ein Hinweis auf unsichere Whisper-Metadaten, kein sicherer Transkriptionsfehler.

### Texteingabe

Nach Auswahl eines Segments erscheint dessen Text rechts in einem nativen Windows-Textfeld. Änderungen befinden sich zunächst nur im Eingabefeld.

### `ÜBERNEHMEN`

Überträgt den Text aus dem Eingabefeld in das geladene Dokument. Die Dateien auf der Festplatte werden erst durch `SPEICHERN` aktualisiert.

### `ÜBERNEHMEN + NÄCHSTE`

Übernimmt den Text und öffnet direkt das nächste Segment.

### `▶ SEGMENT`

Spielt das ausgewählte Segment mit ungefähr 0,65 Sekunden Kontext davor und danach.

### `▶ -5 SEK`

Startet fünf Sekunden vor dem ausgewählten Segment und endet kurz hinter dessen Ende. Das hilft bei schwer verständlichen Einsätzen.

### `■ STOP`

Beendet die aktuelle Audiowiedergabe.

### Editor-Timeline

Ein Klick auf eine freie Position startet etwa zehn Sekunden Wiedergabe ab dieser Songposition. Der helle Marker zeigt den aktuellen Cursor.

### `NÄCHSTE PRÜFSTELLE`

Springt zyklisch zur nächsten automatisch als auffällig markierten Stelle.

### `ALLE SEGMENTE` / `NUR PRÜFSTELLEN`

Filtert die Segmentliste. Die Daten selbst werden nicht verändert.

### `REFRESH`

Liest die Songliste erneut aus dem Lyrics-Ausgabeordner ein. Nicht gespeicherte Änderungen des aktuell geladenen Dokuments sollten vorher gespeichert werden.

### `SPEICHERN`

Schreibt die bearbeiteten Inhalte synchron in:

- `.lyrics.txt`
- `.lyrics.lrc`
- `.lyrics.srt`
- `.lyrics.json`

Die Schreibvorgänge verwenden temporäre Dateien und anschließendes Ersetzen, damit keine halbfertigen Dokumente zurückbleiben.

### `VIDEO ERSTELLEN`

Speichert bei Bedarf das aktuelle Dokument und übergibt Audioquelle, Lyrics-JSON und Songdauer an den Bereich `VIDEO EXPORT`.

### Tastatur im Editor

| Taste | Funktion |
|---|---|
| `Ctrl+Enter` im Textfeld | Text übernehmen und nächstes Segment öffnen |
| `F5` | Songliste aktualisieren |
| `Leertaste` außerhalb des Textfelds | Ausgewähltes Segment abspielen |
| Mausrad über Songliste | Songliste scrollen |
| Mausrad über Segmentliste | Segmentliste scrollen |

## 5. VIDEO EXPORT

Der aktuelle Song muss zuvor im Lyrics Editor geöffnet worden sein.

### Automatische Cover-Suche

Beim Öffnen sucht Klanggeist in dieser Reihenfolge:

1. neben der Audiodatei,
2. neben der Lyrics-Datei,
3. im zuletzt verwendeten Bilderordner.

Gesucht werden zuerst Dateien mit demselben Basisnamen wie der Song, danach Dateien namens `cover`. Unterstützt werden PNG, JPEG, BMP, TIFF und WebP.

### `BILDER WÄHLEN`

Öffnet eine Mehrfachauswahl. Alle ausgewählten Bilder werden analysiert und bilden den Pool für die visuelle Timeline.

Analysierte Bildeigenschaften sind Helligkeit, Kontrast, Sättigung, Wärme, Kantendichte, Format, Dunkelheit und Lebendigkeit. Die Originalbilder werden nicht verändert.

### `★ ALBUMCOVER`

Wählt eines der bereits geladenen Bilder als verbindliches Albumcover. Es wird im Bildpool nach vorne gestellt und bei `TATARUS PRODUCE` als erster Clip gesperrt.

### `✓ ALLE BILDER MINDESTENS 1×`

Aktiviert den Diversity-Lock. `TATARUS PRODUCE` muss jedes Nicht-Cover-Bild mindestens einmal verwenden, bevor Wiederholungen erlaubt sind. Falls die automatisch erzeugte Timeline zu wenige Slots enthält, werden die längsten Slots geteilt.

Ein Klick schaltet auf `FREIE TATARUS-BILDWAHL`. Dann darf TATARUS sofort ausschließlich nach Kontextscore, Wiederverwendungskosten und Cooldown auswählen.

### `⚡ SMART`

Erzeugt eine deterministische Timeline mit genau einem Clip pro geladenem Bild. Die ungefähre Gleichverteilung wird auf geeignete Onsets und Pausenenden verschoben. Bildreihenfolge und Standardstil bleiben erhalten.

SMART lernt nicht und verändert `tatarus_visual_brain.json` nicht.

### `🧠 TATARUS`

Erzeugt ebenfalls ungefähr einen Clip pro Bild, bewertet mögliche Schnittpunkte aber mit dem TATARUS Visual Brain. Zusätzlich werden Bilder, Cut/Crossfade sowie Zoom-, Drift- und Lyrics-Gewichtung kontextabhängig bestimmt.

Gelernte persönliche Präferenzen werden berücksichtigt. Das explizit gewählte Albumcover bleibt im ersten Clip.

### `TATARUS PRODUCE`

Erstellt einen vollständigen Produktionsentwurf statt nur einer Bildverteilung:

1. Song in Intro, Verse, Pre-Chorus, Chorus, Bridge, Instrumental und Outro gliedern.
2. Abschnittsabhängig unterschiedlich lange Clips anlegen.
3. Clipgrenzen an nahe Onsets rücken.
4. Albumcover als Intro sichern.
5. Bilder bewerten und unter Diversity-/Cooldown-Regeln verteilen.
6. Übergänge, Zoom, Drift und Lyrics-Größe pro Abschnitt festlegen.

Nach dem Klick zeigt der Status Anzahl der Clips sowie verwendete und geladene Bilder.

### Analyseanzeige

- `Onsets`: erkannte positive Energieflanken bzw. mögliche Beat-/Einsatzpunkte.
- `Pausen`: mindestens 350 ms unter der adaptiven Energieschwelle.
- `Bereiche`: heuristisch erkannte Songabschnitte.
- `Training`: Zähler für Cut-, Stil- und Bild-Lernsignale.

### Vorschau und Timeline

`▶ PLAY` startet die Audio-/Bildvorschau an der weißen Cursorposition. `■ STOP` beendet sie. Die Leertaste schaltet Wiedergabe ebenfalls ein oder aus.

Die Timeline enthält:

- graue Wellenform: lokale Audioenergie,
- kleine orange Marker: Onsets,
- obere Farblinie: Songbereiche,
- untere cyan/violette Blöcke: visuelle Clips,
- weißen Marker: Vorschauposition.

#### Timeline-Bedienung und TATARUS-Training

| Eingabe | Wirkung |
|---|---|
| Einfacher Linksklick | Vorschauposition setzen |
| `Shift` + Linksklick | Nächste Clipgrenze an die Klickposition verschieben und diese Cut-Präferenz lernen |
| `Ctrl` + Linksklick | Übergang des betroffenen Clips zwischen Hard Cut und Crossfade umschalten und lernen |
| `Alt` + Linksklick | Motion-/Zoom-/Lyrics-Profil zwischen intensiv und ruhig umschalten und lernen |
| Rechtsklick | Bevorzugtes Bild für diesen Songkontext auswählen, einsetzen und als Bildpräferenz lernen |

Die Cut-Korrektur lässt links und rechts mindestens eine Sekunde Platz. Aktionen ohne ausreichende Timeline- oder Audiodaten haben keine Wirkung.

### `VIDEO-ORDNER`

Legt den Zielordner fest. Der Dateiname wird automatisch aus dem Namen der Audioquelle erzeugt. Der vollständige Zielpfad wird direkt darunter angezeigt und beim Start des Exports erneut aus dem aktuell gewählten Ordner aufgebaut.

### Preset

Version 2.1.1 verwendet das feste Preset `Klanggeist Lyrics Video`:

- MP4
- 1920 × 1080
- 30 FPS
- H.264, CRF 20, Preset `veryfast`
- AAC, 192 kbit/s
- zentrierte vorherige, aktuelle und nächste Lyrics-Zeile
- atmosphärischer Zoom und Drift

### `EXPORTIEREN`

Startet den Export im Hintergrund. Die bevorzugte FAST Render Engine erstellt einen FFmpeg-Filtergraph aus Bildern, Schnitten, Crossfades, Bewegung und ASS-Lyrics. Die MP4 wird zunächst unter einem temporären Namen geschrieben und nach erfolgreichem Abschluss atomar auf den angezeigten Zielpfad verschoben.

Falls der gebündelte FFmpeg-Pfad fehlschlägt oder fehlt, verwendet Klanggeist den nativen Direct2D-/Media-Foundation-Renderer als langsameren Fallback.

### `ABBRECHEN`

Beendet einen laufenden FFmpeg- oder nativen Export. Die unvollständige temporäre MP4 wird entfernt; eine bereits vorhandene fertige Zieldatei bleibt bis zur erfolgreichen Veröffentlichung unangetastet.

## 6. SETTINGS

Diese Seite ist eine Statusübersicht, kein eigener Einstellungsdialog. Angezeigt werden:

- aktives Whisper-Modell,
- CPU/CUDA-Gerät,
- Compute-Typ,
- Sprache,
- erwarteter Modellcache,
- Video-Ausgabeordner,
- zuletzt verwendeter Bilder-/Coverordner.

Die Werte werden auf den jeweiligen Arbeitsseiten geändert.

## 7. Lokale Konfigurationsdateien

Neben der EXE können entstehen:

| Pfad | Zweck |
|---|---|
| `klanggeist_studio.json` | UI-Einstellungen und zuletzt verwendete Ordner |
| `tatarus_visual_brain.json` | Persönliche TATARUS-Gewichte und Trainingszähler |
| `.hf/` | Whisper-Modellcache |
| `lyrics/` | Standardausgabe, falls kein anderer Lyrics-Ordner gewählt wurde |
| `videos/` | Standardvideoausgabe, falls kein anderer Video-Ordner gewählt wurde |

Portable Installationen sollten deshalb in einem beschreibbaren Ordner liegen.

## 8. Häufige Probleme

### `OFFLINE` bleibt sichtbar

Das ist außerhalb einer Transkriptionsqueue normal. Falls `START` sofort fehlschlägt, Python 3.12 prüfen und `INSTALL_BACKEND.bat` erneut ausführen.

### CUDA-Start schlägt fehl

Vorübergehend auf `cpu` und `int8` wechseln. Danach CUDA-, Treiber- und CTranslate2-Kompatibilität getrennt prüfen.

### Song erscheint nicht im Editor

- Richtigen Lyrics-Ausgabeordner wählen.
- Prüfen, ob eine `*.lyrics.json` existiert.
- `REFRESH` oder `F5` verwenden.

### Video wird langsam gerendert

Der Status sollte `FAST Render` melden. Ein nativer Fallback ist deutlich langsamer. Prüfen, ob `tools/ffmpeg.exe`, seine DLLs und `klanggeist-ffmpeg.reuse` vollständig neben der Anwendung liegen.

### Video ist nicht im erwarteten Ordner

Den vollständigen Pfad unter `VIDEO-ORDNER` kontrollieren. Version 2.1.1 berechnet das Ziel beim Klick auf `EXPORTIEREN` nochmals aus diesem Ordner und zeigt nach Abschluss den vollständigen Pfad.

### Windows warnt beim Download

Die veröffentlichten Binärdateien sind nicht kommerziell codesigniert. SHA-256 mit `SHA256SUMS.txt` vergleichen und die Datei ausschließlich vom offiziellen GitHub-Release beziehen.
