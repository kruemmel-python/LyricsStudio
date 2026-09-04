# TATARUS Visual Brain – technische und praktische Erklärung

## 1. Was TATARUS ist

TATARUS ist die lokale Visual-Intelligence-Schicht von Klanggeist Lyrics Studio. Sie plant für einen Song:

- sinnvolle Bildwechsel,
- die Reihenfolge der Bilder,
- Hard Cut oder Crossfade,
- Stärke von Zoom und Drift,
- relative Größe der eingebrannten Lyrics,
- einen vollständigen abschnittsbasierten Produktionsentwurf.

TATARUS ist in Version 2.1.1 **kein großes neuronales Netz, kein generatives Modell und kein Cloud-Dienst**. Die Implementierung ist ein kompakter, persistenter Online-Lerner aus normalisierten Sensorwerten, gewichteten linearen Scores, Sigmoid-Abbildung und paarweisem Präferenzlernen. Diese Form ist schnell, nachvollziehbar und vollständig lokal.

## 2. Was TATARUS nicht tut

TATARUS:

- erzeugt keine Bilder,
- verändert keine Bilddateien,
- hört Musik nicht semantisch wie ein Mensch,
- erkennt keine Personen, Gegenstände oder Bildmotive,
- bestimmt kein exaktes Tempo oder Taktraster,
- sendet keine Analyse- oder Lerndaten ins Internet,
- verändert nicht die Whisper-Transkription.

Die Entscheidungen basieren auf Audioenergie, zeitlicher Struktur, Lyrics-Wiederholungen und einfachen visuellen Bildmerkmalen.

## 3. Verarbeitungsweg

```text
Audio ──► 50-ms-Energiehüllkurve ──► Onsets, Pausen, Energiebereiche
Lyrics ────────────────────────────► Dichte und Wiederholungen
                                        │
Bilder ─► visuelle Merkmale              ├──► Songstruktur
                                        │
Persönliche Gewichte ───────────────────┘
                                        │
                                        ▼
                              TATARUS VisualTimeline
                              Bild · Start · Ende
                              Cut/Fade · Zoom · Drift
                              Lyrics-Gewichtung
                                        │
                         Vorschau und FFmpeg-Export
```

## 4. Audio Intelligence

Die native Audioanalyse dekodiert die Quelle über Windows Media Foundation möglichst als Mono-Float-Audio mit 48 kHz. Falls der Decoder diese Vorgabe nicht unterstützt, wird ein Float-Format ohne erzwungene Kanal-/Samplerate verwendet.

### 4.1 Energiehüllkurve

Das Signal wird in 50-ms-Fenster aufgeteilt. Pro Fenster speichert Klanggeist:

- RMS als mittlere Energie,
- Peak als maximalen Absolutwert,
- Mittelpunkt des Fensters als Zeitposition.

Aus allen RMS-Werten werden das 20., 50. und 80. Perzentil bestimmt. Damit passen sich weitere Schwellen an laute und leise Produktionen an.

### 4.2 Onsets

Ein Onset ist eine ausreichend große positive Energieflanke. Die Analyse glättet jeweils drei benachbarte Hüllkurvenpunkte, vergleicht die Änderung mit einer adaptiven Schwelle und erzwingt mindestens 220 ms Abstand zwischen zwei Onsets.

Das Ergebnis ist eine robuste Liste möglicher Einsätze oder Beat-naher Positionen. Es ist keine vollständige Beat- oder BPM-Erkennung.

### 4.3 Pausen

Die Stille-Schwelle ist der größere Wert aus:

```text
0,004
oder
60 % des 20-%-RMS-Perzentils
```

Ein Bereich gilt als Pause, wenn er mindestens 350 ms unter dieser Schwelle liegt. Besonders das Pausenende ist häufig ein geeigneter Bildwechselpunkt.

### 4.4 Energieabschnitte

Die Analyse bildet zunächst 8-Sekunden-Blöcke, weist ihnen anhand der RMS-Perzentile `Low`, `Medium` oder `High` zu und fasst benachbarte Blöcke mit demselben Band zusammen.

## 5. Acht Audio-/Zeitmerkmale

Für einen Zeitpunkt `t` erzeugt `TatarusVisualBrain::Sense` acht Werte zwischen 0 und 1:

| Index | Merkmal | Bedeutung |
|---:|---|---|
| 0 | Lokale Energie | RMS am Zeitpunkt, verstärkt und begrenzt |
| 1 | Onset-Nähe | Hoch, wenn `t` innerhalb von etwa 750 ms eines Onsets liegt |
| 2 | Nähe zum Pausenende | Hoch, wenn `t` nahe am Ende einer Pause liegt |
| 3 | Abschnittsenergie | Fester Repräsentant für Low, Medium oder High |
| 4 | Energieanstieg | Vergleich von ungefähr 180 ms vor und nach `t` |
| 5 | Energieabfall | Umgekehrter Vergleich derselben Umgebung |
| 6 | Songrandnähe | Hoch an Anfang und Ende, niedrig in der Mitte |
| 7 | Songmittellage | Hoch in der Mitte, niedrig an den Rändern |

Diese Werte sind die gemeinsame Kontextbeschreibung für Schnitte, Übergänge, Stile und Bild-Matching.

## 6. Schnittbewertung

Der Basisscore ist:

```text
score = sigmoid(bias + Summe(gewicht[i] × merkmal[i]))
```

Die mitgelieferten Bootstrap-Gewichte bevorzugen unter anderem Onset-Nähe, lokale Energie und Pausenenden. Alle Gewichte sind begrenzt, damit einzelne Trainingsaktionen das Verhalten nicht unkontrolliert verändern.

### TATARUS-Timeline

Für eine normale TATARUS-Timeline gilt:

1. Es wird ungefähr ein Clip pro geladenem Bild geplant.
2. Ideale Grenzen entstehen zunächst durch Gleichverteilung.
3. Onsets und Pausenenden in einem Suchradius werden als Kandidaten gesammelt.
4. Kandidaten mit weniger als 80 ms Abstand werden zusammengefasst.
5. Jeder Kandidat erhält eine Kombination aus TATARUS-Score und Nähe zur idealen Grenze:

```text
Nutzen = 72 % TATARUS-Score + 28 % zeitliche Nähe
```

6. Mindestabstände verhindern extrem kurze Clips.

SMART verwendet dagegen keine gelernten Gewichte und wählt nur anhand zeitlicher Nähe.

## 7. Übergänge und visueller Stil

### 7.1 Hard Cut oder Crossfade

TATARUS bildet eine Hard-Cut-Evidenz aus:

- 55 % Onset-Nähe,
- 30 % Energieanstieg,
- 15 % Abschnittsenergie.

Ist diese Evidenz größer als die persönliche `fade_preference`, wird ein Hard Cut gewählt; andernfalls ein Crossfade. Die Standardpräferenz liegt bei 0,58.

Crossfades dauern standardmäßig höchstens 0,65 Sekunden und werden für kurze Clips zusätzlich begrenzt.

### 7.2 Zoom

Zoom wird mit Abschnittsenergie und Onset-Nähe verstärkt. Ein energiereicher Chorus kann dadurch dynamischer wirken als ein ruhiges Intro.

### 7.3 Drift

Drift ist bei geringerer lokaler Energie tendenziell stärker und reagiert zusätzlich auf Energieabfälle. Das erzeugt in ruhigen Bereichen eine schwebende Bewegung, während starke Passagen weniger seitlich wandern können.

### 7.4 Lyrics-Größe

Die relative Lyrics-Größe steigt moderat mit Abschnittsenergie und Onset-Nähe. Sie wird anschließend auf einen sicheren Bereich begrenzt.

Die Werte sind Multiplikatoren des Video-Presets und keine absoluten Pixelgrößen.

## 8. Bildanalyse und Bild-Matching

Jedes geladene Bild wird über Windows Imaging Component dekodiert und für die Analyse auf höchstens 128 Pixel an der längsten Seite verkleinert. Die Originaldatei bleibt unverändert.

### Acht Bildmerkmale

| Merkmal | Ermittlung |
|---|---|
| Helligkeit | Mittlere wahrnehmungsgewichtete RGB-Luminanz |
| Kontrast | Normalisierte Standardabweichung der Luminanz |
| Sättigung | Mittlere relative RGB-Sättigung |
| Wärme | Verhältnis von Rot- zu Blauanteil |
| Kantendichte | Lokale horizontale und vertikale Luminanzunterschiede |
| Format | Querformat = 1, Hochformat = 0, annähernd quadratisch = 0,5 |
| Dunkelheit | `1 − Helligkeit` |
| Lebendigkeit | Kombination aus 58 % Sättigung und 42 % Kontrast |

### Zwölf Matching-Merkmale

TATARUS verknüpft Audio- und Bildwerte unter anderem so:

- Helligkeit × Abschnittsenergie,
- Kontrast × Onset-Nähe,
- Sättigung × lokale Energie,
- Wärme × Abschnittsenergie,
- Kantendichte × Onset-Nähe,
- Dunkelheit × Ruhe,
- Lebendigkeit × Energieanstieg,
- Querformatbonus,
- zusätzlich die vier direkten Werte Helligkeit, Kontrast, Sättigung und Kantendichte.

Diese zwölf Werte werden wie die Schnittmerkmale gewichtet und durch eine Sigmoid-Funktion auf einen Bildscore abgebildet.

Wichtig: Das ist ästhetisches Low-Level-Matching. TATARUS weiß nicht, ob auf einem Bild beispielsweise ein Wikinger, eine Landschaft oder eine Person zu sehen ist.

## 9. Songstruktur

`TATARUS PRODUCE` kombiniert Energiegrenzen, längere Pausen und Lyrics-Wiederholungen.

### Grenzen

- Grenzen der Audioenergiebereiche werden übernommen.
- Pausen ab 450 ms können zusätzliche Grenzen erzeugen.
- Grenzen mit weniger als zwei Sekunden Abstand werden zusammengeführt.
- Sehr lange Abschnitte werden in handhabbare Teile von ungefähr 16 Sekunden zerlegt.

### Lyrics-Merkmale

- `lyricDensity`: Anteil eines Bereichs, der durch Lyrics-Segmente belegt ist.
- `repetition`: Anteil der Lyrics-Zeit, deren normalisierte Zeilen mehrfach im Song vorkommen.

### Heuristische Abschnittstypen

- `Intro`: erster Bereich mit niedriger Lyrics-Dichte oder kurzer Anfangsbereich.
- `Outro`: letzter Bereich mit niedriger Lyrics-Dichte.
- `Instrumental`: Lyrics-Dichte unter 0,12.
- `Chorus`: genügend Wiederholung und Energie.
- `Pre-Chorus`: energiereicher Verse unmittelbar vor einem erkannten Chorus.
- `Bridge`: später, wenig repetitiver Kontrastbereich nach einem Chorus.
- `Verse`: Standardtyp, wenn keine speziellere Regel passt.

Die Erkennung ist bewusst heuristisch. Sie kann bei instrumentaler, frei strukturierter oder stark wiederholender Musik von der musikalischen Fachanalyse abweichen.

## 10. TATARUS PRODUCE

### Ziel-Clipdauer

| Abschnitt | Zielwert |
|---|---:|
| Intro | 12,0 s |
| Verse | 10,5 s |
| Pre-Chorus | 8,0 s |
| Chorus | 6,5 s |
| Bridge | 9,0 s |
| Instrumental | 8,5 s |
| Outro | 12,0 s |

Jeder Songbereich wird anhand seiner Länge in einen oder mehrere Produktionsslots geteilt. Innere Grenzen dürfen sich um bis zu 1,35 Sekunden zu einem geeigneten Onset verschieben.

### Albumcover-Lock

Das Albumcover bildet den ersten Clip. Sein Ende liegt zwischen drei und zehn Sekunden und wird möglichst an einen geeigneten frühen Songbereich oder Onset angelegt.

### Diversity Planner

Mit aktivem `ALLE BILDER MINDESTENS 1×` gelten folgende Regeln:

- Jedes Nicht-Cover-Bild muss vor einer freien Wiederholung einmal vorkommen.
- Bei zu wenigen Slots werden die längsten Slots geteilt.
- Bereits häufig verwendete Bilder erhalten eine Nutzungskostenstrafe.
- Bilder im jüngsten Verlauf erhalten eine Cooldown-Strafe.
- Eine direkte Wiederholung wird besonders stark bestraft.
- Nach vollständiger Pool-Abdeckung sind Wiederholungen wieder erlaubt.

In Version 2.1.1 beträgt der Reuse-Cooldown vier Bilder und die Basiskostenstrafe 0,16 pro vorheriger Verwendung.

### Abschnittsstil

Der persönliche TATARUS-Stil wird je nach Songtyp weiter moduliert:

| Abschnitt | Tendenz |
|---|---|
| Intro | weniger Zoom, mehr Drift, etwas kleinere Lyrics |
| Verse | leicht ruhiger Zoom, etwas mehr Drift |
| Pre-Chorus | ansteigender Zoom, weniger Drift, größere Lyrics |
| Chorus | deutlich mehr Zoom, weniger Drift, größere Lyrics |
| Bridge | weniger Zoom, mehr Drift |
| Instrumental | mehr Zoom und Drift, kleinere Lyrics |
| Outro | weniger Zoom, mehr Drift, kleinere Lyrics |

Ein energiereicher Chorus bevorzugt zusätzlich Hard Cuts; Outro-Bereiche bevorzugen Crossfades.

## 11. Lernen durch die Timeline

TATARUS lernt ausschließlich aus ausdrücklichen Nutzeraktionen.

### `Shift` + Linksklick: Schnittzeit lernen

Die nächstgelegene bestehende Clipgrenze wird zur Klickposition verschoben. Der Kontext am neuen Zeitpunkt gilt als bevorzugt, der Kontext am alten Zeitpunkt als abgelehnt.

Das paarweise Update erhöht künftig die Bewertung von Merkmalen, die am bevorzugten Punkt stärker vorhanden waren. Die Lernrate startet bei 0,12 und sinkt mit der Zahl der Trainingsereignisse.

### `Ctrl` + Linksklick: Übergang lernen

Der Übergang des betroffenen Clips wird zwischen Hard Cut und Crossfade umgeschaltet. TATARUS nähert die persönliche Fade-Schwelle dem gewünschten Ergebnis an und speichert zugleich den Stilkontext.

### `Alt` + Linksklick: Bewegungsstil lernen

Der Clip wechselt zwischen zwei bewusst deutlich unterschiedlichen Profilen:

- intensiv: mehr Zoom, weniger Drift, größere Lyrics,
- ruhig: weniger Zoom, mehr Drift, kleinere Lyrics.

Die globalen persönlichen Zoom-, Drift- und Lyrics-Präferenzen werden schrittweise in diese Richtung bewegt.

### Rechtsklick: Bildpräferenz lernen

Das gewählte Bild ersetzt den bisherigen Clip. Seine Merkmale gelten in diesem Audio-/Songkontext als bevorzugt, die des ersetzten Bildes als abgelehnt. Das Bild wird bei Bedarf dem aktuellen Pool hinzugefügt.

Die Bild-Lernrate startet bei 0,10 und sinkt ebenfalls mit der Anzahl der Ereignisse.

## 12. Persistenz

Die Gewichte werden neben der EXE in `tatarus_visual_brain.json` gespeichert. Aktuelles Format: Version 3.

Enthalten sind:

- acht Schnittgewichte und Bias,
- Schnitt-Trainingszähler,
- Fade-, Zoom-, Drift- und Lyrics-Präferenzen,
- zwölf Bild-Matching-Gewichte und Bild-Bias,
- Stil- und Bild-Trainingszähler.

Die Datei enthält keine Audiodaten, Bilder, Lyrics oder Dateilisten.

### TATARUS zurücksetzen

1. Klanggeist schließen.
2. `tatarus_visual_brain.json` neben der verwendeten EXE sichern oder löschen.
3. Klanggeist neu starten.

Ohne Datei verwendet TATARUS wieder die eingebauten Bootstrap-Gewichte.

### Portable und MSI getrennt

Da die Lerndatei neben der jeweiligen EXE liegt, besitzen eine Portable- und eine MSI-Installation getrennte TATARUS-Persönlichkeiten. Wer dieselben Präferenzen verwenden möchte, kann die JSON-Datei bei geschlossenem Programm zwischen beiden Installationsordnern kopieren.

## 13. Reproduzierbarkeit und Grenzen

Bei identischen Eingaben und identischer `tatarus_visual_brain.json` arbeitet TATARUS deterministisch. Es gibt keine Zufallsentscheidung.

Die wichtigsten Grenzen der aktuellen Version:

- keine semantische Bildklassifikation,
- keine echte Takt-/Tempo-/Downbeat-Erkennung,
- heuristische Songstruktur,
- globale persönliche Stilwerte statt getrennten Profilen pro Genre,
- keine Undo-Historie für einzelne Lernaktionen,
- nur ein persistentes Profil pro Programmordner.

Diese Grenzen sind absichtlich dokumentiert: TATARUS ist ein schneller, lokaler Assistenzplaner und kein Ersatz für eine vollständige manuelle Videoredaktion.
