# Drittanbieterhinweise

Stand: Klanggeist Lyrics Studio 2.1.2

Der Klanggeist-eigene Quellcode steht unter der MIT-Lizenz im Wurzelverzeichnis. Mitgelieferte oder optional installierte Drittanbieterkomponenten behalten ihre eigenen Lizenzen. Dieser Hinweis ersetzt keine Lizenztexte.

## 1. Gebündelter FFmpeg-Fork

| Feld | Wert |
|---|---|
| Projekt | FFmpeg |
| Upstream | <https://github.com/FFmpeg/FFmpeg> |
| Basiscommit | `b397eba2f0d3d86daf1098d0f27daffccc74fea5` |
| Änderung | `patches/ffmpeg-reuse-unchanged.patch` |
| Lizenz des ausgelieferten Builds | GNU GPL Version 2 oder später |

Der Build aktiviert GPL-Komponenten und libx264. Daher gilt für die ausgelieferte `ffmpeg.exe` die GPL, unabhängig von der MIT-Lizenz der Klanggeist-Anwendung.

Lizenztexte befinden sich unter `tools/licenses/ffmpeg/`. Der vollständige, bereits gepatchte Quellcode wird beim GitHub-Release als `Klanggeist-FFmpeg-8.0-git-b397eba2-source.zip` angeboten.

## 2. Gebündelte Laufzeitbibliotheken

Die FFmpeg-Ausgabe wurde mit MSYS2/UCRT64 erstellt. Folgende DLLs werden neben `ffmpeg.exe` ausgeliefert:

| Komponente | Paketversion | Lizenz laut MSYS2-Paket | Upstream |
|---|---:|---|---|
| libass | 0.17.5 | ISC | <https://github.com/libass/libass> |
| Brotli | 1.2.0 | MIT | <https://github.com/google/brotli> |
| bzip2 | 1.0.8 | bzip2-Lizenz | <https://sourceware.org/bzip2/> |
| Expat | 2.8.2 | MIT | <https://libexpat.github.io/> |
| Fontconfig | 2.18.3 | Fontconfig-Lizenz | <https://www.freedesktop.org/wiki/Software/fontconfig/> |
| FreeType | 2.14.3 | FTL oder GPL-2.0-or-later | <https://freetype.org/> |
| FriBidi | 1.0.16 | LGPL-2.1-or-later | <https://github.com/fribidi/fribidi> |
| GCC Runtime Libraries | 16.1.0 | GPL-3.0-or-later mit GCC Runtime Library Exception; Teile LGPL-2.1-or-later | <https://gcc.gnu.org/> |
| GLib | 2.88.3 | LGPL-2.1-or-later | <https://gitlab.gnome.org/GNOME/glib> |
| Graphite2 | 1.3.15 | LGPL-2.1-or-later | <https://github.com/silnrsi/graphite> |
| HarfBuzz | 14.3.0 | MIT | <https://harfbuzz.github.io/> |
| GNU libiconv | 1.19 | LGPL-2.1-or-later; Dokumentation GPL-3.0-or-later | <https://www.gnu.org/software/libiconv/> |
| GNU gettext-runtime | 1.0 | GPL-3.0-or-later und LGPL-2.1-or-later | <https://www.gnu.org/software/gettext/> |
| PCRE2 | 10.47 | BSD-3-Clause | <https://pcre.org/> |
| libpng | 1.6.58 | libpng-Lizenz | <http://www.libpng.org/pub/png/libpng.html> |
| libunibreak | 7.0 | Zlib | <https://github.com/adah1972/libunibreak> |
| MinGW-w64 winpthreads | 14.0.0.r248.g7735a1a63 | MIT und BSD-3-Clause-Clear | <https://www.mingw-w64.org/> |
| x264 | 0.165.r3222.b35605a | GPL-2.0-or-later | <https://www.videolan.org/developers/x264.html> |
| zlib | 1.3.2 | Zlib | <https://zlib.net/> |

Die zu den MSYS2-Paketen gehörenden Lizenzdateien werden unter `tools/licenses/runtime/` mitgeliefert. Für Bibliotheken mit Copyleft-Lizenz gelten deren jeweilige Quellcode- und Relinking-Bedingungen. Die Versions- und Upstream-Angaben ermöglichen den Bezug des korrespondierenden Quellstands; FFmpeg selbst wird zusätzlich vollständig als Release-Asset bereitgestellt.

## 3. Optionales Python-Backend

Python und diese Pakete werden **nicht** im Portable-ZIP oder MSI eingebettet. `INSTALL_BACKEND.bat` installiert sie auf ausdrücklichen Wunsch des Nutzers aus dem Python Package Index:

| Komponente | Zweck | Lizenz/Projekt |
|---|---|---|
| faster-whisper | Whisper-Inferenz | MIT, <https://github.com/SYSTRAN/faster-whisper> |
| CTranslate2 und transitive Python-Abhängigkeiten | Inferenzlaufzeit | Bedingungen der jeweils installierten Paketversion |

Die konkrete Abhängigkeitsauflösung erfolgt zum Installationszeitpunkt. Für eine vollständig reproduzierbare Python-Umgebung sollte ein projektspezifischer Lockfile-Workflow ergänzt werden; Version 2.1.2 verwendet bewusst die Mindestanforderung `faster-whisper>=1.1`.

Whisper-Modelle werden ebenfalls nicht verteilt. Ihre jeweiligen Model-Cards und Lizenzen sind beim Downloadanbieter zu beachten.

## 4. Windows-Komponenten

Win32, Direct2D, DirectWrite, Windows Imaging Component, Media Foundation und XAudio2 sind Bestandteile bzw. SDK-Schnittstellen von Microsoft Windows und werden nicht als separate Klanggeist-Bibliotheken ausgeliefert.

## 5. Keine Lizenzvermischung

Die MIT-Lizenz erlaubt die Nutzung des Klanggeist-Quellcodes. Sie erteilt keine zusätzlichen Rechte an FFmpeg, x264, anderen Drittanbieterbibliotheken, Musik, Lyrics, Bildern oder Whisper-Modellen.

Fragen zu einem bestimmten Drittanbieterbestandteil sollten anhand der beigefügten Lizenzdatei und des genannten Upstream-Projekts geprüft werden.
