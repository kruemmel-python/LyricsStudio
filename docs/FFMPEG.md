# Klanggeist-FFmpeg

## Zweck

Klanggeist Lyrics Studio bündelt einen eigenen FFmpeg-Build für schnellen und reproduzierbaren H.264/AAC-Export. Die Anwendung funktioniert bei fehlendem Fork weiterhin mit einem kompatiblen Standard-FFmpeg oder dem nativen Windows-Fallback, der optimierte Untertitelpfad ist dann jedoch nicht verfügbar.

## Basis

| Eigenschaft | Wert |
|---|---|
| Upstream | <https://github.com/FFmpeg/FFmpeg> |
| Commit | `b397eba2f0d3d86daf1098d0f27daffccc74fea5` |
| Versionskennung | `8.0.git-klanggeist` |
| Patch | `patches/ffmpeg-reuse-unchanged.patch` |
| Lizenz des gebündelten Builds | GPLv2 oder später |

Die GPL-Einstufung entsteht durch `--enable-gpl` und die Verwendung von libx264.

## Konfiguration

Der veröffentlichte Windows-Build wurde in MSYS2/UCRT64 mit einer Konfiguration dieses Umfangs erstellt:

```sh
./configure \
  --target-os=mingw32 \
  --arch=x86_64 \
  --enable-gpl \
  --enable-libass \
  --enable-libx264 \
  --enable-zlib \
  --disable-autodetect \
  --disable-debug \
  --disable-doc \
  --disable-ffplay \
  --disable-ffprobe \
  --disable-network \
  --disable-avdevice \
  --extra-version=klanggeist

make -j12 ffmpeg.exe
```

Die Release-Datei `Klanggeist-FFmpeg-8.0-git-b397eba2-source.zip` enthält den vollständigen Quellbaum mit bereits angewendetem Patch.

## `reuse_unchanged`

Der Patch ergänzt den `subtitles`-Filter um die standardmäßig deaktivierte Option:

```text
reuse_unchanged=1
```

Sie nutzt zwei Signale:

1. libass meldet, ob sich die gerenderte Untertitelgrafik geändert hat.
2. Datenzeiger und Strides zeigen, ob die aktuelle Crop-Ansicht auf dieselben unveränderten Bildpixel verweist.

Nur wenn beide Zustände unverändert sind, wird das Frame vor Vollbildkopie und Untertitel-Blending verworfen. Mit VFR hält der Player das vorherige Frame bis zur nächsten tatsächlichen Änderung.

Die Prüfung vergleicht absichtlich keine kompletten Pixelpuffer. Sie ist auf den Klanggeist-Aufbau zugeschnitten, bei dem ein einmal skaliertes Standbild innerhalb des Filtergraphs wiederholt und als Integer-Crop bewegt wird.

Der Marker `tools/klanggeist-ffmpeg.reuse` signalisiert der Anwendung, dass die lokale FFmpeg-Binärdatei diese Option unterstützt. Ohne Marker wird keine unbekannte Filteroption an einen Standard-Build übergeben.

## Single-Image-Graph

Der Einbildpfad:

1. dekodiert das Bild einmal,
2. skaliert mit Overscan,
3. wiederholt das dekodierte Frame,
4. erzeugt eine langsame Crop-Bewegung,
5. rendert ASS-Lyrics,
6. verwirft unveränderte Ausgaben über den Patch oder `mpdecimate`.

## Multi-Image-Graph

Für jeden `VisualClip` entsteht ein eigener Stream:

- einmaliges Dekodieren und Skalieren,
- Frame-Wiederholung im Filtergraph,
- clipabhängiger Crop/Drift,
- Trim auf Clipdauer,
- optionales Start-Padding für Crossfades,
- feste Ausgabe-FPS,
- Normalisierung auf `AVTB`.

Anschließend werden:

- Hard Cuts mit `concat`,
- Crossfades mit `xfade`,
- Lyrics nach der fertigen Bildkette mit `subtitles`

kombiniert.

### Warum `settb=AVTB` nötig ist

Der `concat`-Filter liefert eine Mikrosekunden-Zeitbasis, während ein Bildstream gewöhnlich `1/fps` verwendet. Wenn nach einem Hard Cut später ein Crossfade folgt, verweigert `xfade` unterschiedliche Zeitbasen. Version 2.1.1 normalisiert deshalb jeden Clip vor der Verkettung auf `AVTB`.

Ohne diese Korrektur scheitert der schnelle Graph und Klanggeist fällt auf den deutlich langsameren nativen Frame-Renderer zurück.

## Ausgabeparameter

```text
Video: libx264, preset veryfast, CRF 20, yuv420p
Audio: AAC, 192 kbit/s
Modus: VFR-kompatibel
Container: MP4 mit +faststart
```

Die nominelle Timeline basiert weiterhin auf 30 FPS; VFR reduziert nur physisch redundante Ausgaben.

## Fortschritt und Abbruch

FFmpeg schreibt maschinenlesbare Werte über:

```text
-progress pipe:1
-stats_period 0.25
```

Der C++-Controller wandelt `out_time_us` in UI-Fortschritt um. Bei Abbruch wird der Prozess beendet und die temporäre MP4 entfernt.

## Lizenz und Quellcode

Die Anwendung unter MIT und der gebündelte FFmpeg-Build besitzen unterschiedliche Lizenzen. Die MIT-Lizenz ändert die Bedingungen des FFmpeg-Builds nicht.

Im GitHub-Release werden angeboten:

- vollständiger gepatchter FFmpeg-Quellcode,
- GPL-/LGPL-Lizenztexte,
- Patch im Hauptrepository,
- Buildkonfiguration,
- Drittanbieterhinweise mit Versionen und Herkunft.

Siehe [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).
