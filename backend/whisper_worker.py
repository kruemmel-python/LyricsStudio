from __future__ import annotations

import argparse
import json
import os
import re
import sys
import time
from dataclasses import asdict, dataclass
from datetime import datetime
from pathlib import Path
from tempfile import NamedTemporaryFile
from typing import Any, Callable


def emit(event_type: str, **payload: Any) -> None:
    print(json.dumps({"type": event_type, **payload}, ensure_ascii=False), flush=True)


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser()
    p.add_argument("--server", action="store_true")
    p.add_argument("--model", default="large-v3")
    p.add_argument("--device", default="cpu")
    p.add_argument("--compute-type", default="int8")
    p.add_argument("--language", default="auto")
    p.add_argument("--cache-dir", type=Path, required=True)
    return p.parse_args()


@dataclass(slots=True)
class Segment:
    start: float
    end: float
    text: str
    avg_logprob: float | None = None
    no_speech_prob: float | None = None
    compression_ratio: float | None = None


def clean(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


def atomic_write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with NamedTemporaryFile("w", encoding="utf-8", newline="\n", dir=path.parent, delete=False, prefix=".", suffix=".tmp") as f:
        f.write(content)
        tmp = Path(f.name)
    tmp.replace(path)


def lrc_time(seconds: float) -> str:
    cs = max(0, round(seconds * 100)); minutes, rem = divmod(cs, 6000); sec, cs = divmod(rem, 100)
    return f"{minutes:02d}:{sec:02d}.{cs:02d}"


def srt_time(seconds: float) -> str:
    ms = max(0, round(seconds * 1000)); h, rem = divmod(ms, 3_600_000); m, rem = divmod(rem, 60_000); s, ms = divmod(rem, 1000)
    return f"{h:02d}:{m:02d}:{s:02d},{ms:03d}"


def output_paths(audio: Path, input_root: Path, output_root: Path) -> dict[str, Path]:
    try:
        relative = audio.relative_to(input_root)
    except ValueError:
        relative = Path(audio.name)
    base = (output_root / relative).with_suffix("")
    return {
        "txt": base.parent / f"{base.name}.lyrics.txt",
        "lrc": base.parent / f"{base.name}.lyrics.lrc",
        "srt": base.parent / f"{base.name}.lyrics.srt",
        "json": base.parent / f"{base.name}.lyrics.json",
    }


def current(audio: Path, paths: dict[str, Path], model: str, overwrite: bool) -> bool:
    if overwrite or not all(p.exists() for p in paths.values()):
        return False
    try:
        doc = json.loads(paths["json"].read_text(encoding="utf-8"))
        st = audio.stat(); source = doc.get("source", {}); pipeline = doc.get("pipeline", {})
        stored_file = source.get("file")
        if not isinstance(stored_file, str) or not stored_file:
            return False
        same_source = os.path.normcase(str(Path(stored_file).resolve(strict=False))) == os.path.normcase(str(audio.resolve(strict=False)))
        return same_source and source.get("size") == st.st_size and source.get("mtime_ns") == st.st_mtime_ns and pipeline.get("model") == model
    except Exception:
        return False


def save(audio: Path, input_root: Path, paths: dict[str, Path], segments: list[Segment], info: Any, args: argparse.Namespace) -> None:
    text = "\n".join(s.text for s in segments)
    atomic_write(paths["txt"], text + ("\n" if text else ""))
    atomic_write(paths["lrc"], "\n".join(f"[{lrc_time(s.start)}]{s.text}" for s in segments) + ("\n" if segments else ""))
    blocks = [f"{i}\n{srt_time(s.start)} --> {srt_time(s.end)}\n{s.text}" for i, s in enumerate(segments, 1)]
    atomic_write(paths["srt"], "\n\n".join(blocks) + ("\n" if blocks else ""))
    st = audio.stat()
    try: relative = str(audio.relative_to(input_root))
    except ValueError: relative = audio.name
    doc = {
        "source": {"file": str(audio), "relative_file": relative, "size": st.st_size, "mtime_ns": st.st_mtime_ns},
        "pipeline": {
            "app": "Klanggeist Lyrics Studio", "app_version": "2.1.2", "backend": "faster-whisper",
            "model": args.model, "device": args.device, "compute_type": args.compute_type,
            "language_requested": None if args.language == "auto" else args.language,
            "created_at": datetime.now().astimezone().isoformat(timespec="seconds"),
        },
        "transcription": {
            "language_detected": str(info.language), "language_probability": float(info.language_probability),
            "duration_seconds": float(info.duration), "text": text, "segments": [asdict(s) for s in segments],
        },
    }
    atomic_write(paths["json"], json.dumps(doc, ensure_ascii=False, indent=2) + "\n")


def main() -> int:
    args = parse_args()
    args.cache_dir.mkdir(parents=True, exist_ok=True)
    hf_home = args.cache_dir.parent
    os.environ["HF_HOME"] = str(hf_home)
    os.environ["HF_HUB_CACHE"] = str(args.cache_dir)
    os.environ["HUGGINGFACE_HUB_CACHE"] = str(args.cache_dir)

    try:
        from faster_whisper import WhisperModel
    except Exception as exc:
        emit("fatal", message=f"faster-whisper Import fehlgeschlagen: {exc}")
        return 2

    try:
        emit("loading", model=args.model, device=args.device, compute_type=args.compute_type)
        model = WhisperModel(args.model, device=args.device, compute_type=args.compute_type, download_root=str(args.cache_dir))
    except Exception as exc:
        emit("fatal", message=f"Modell konnte nicht geladen werden: {type(exc).__name__}: {exc}")
        return 3

    emit("ready", model=args.model)

    for raw in sys.stdin:
        raw = raw.strip()
        if not raw: continue
        try:
            cmd = json.loads(raw)
        except Exception as exc:
            emit("error", message=f"Ungültiges Kommando: {exc}")
            continue
        if cmd.get("cmd") == "quit":
            emit("bye"); return 0
        if cmd.get("cmd") != "transcribe":
            emit("error", message="Unbekanntes Kommando")
            continue

        audio = Path(cmd["path"]); input_root = Path(cmd.get("input_root") or audio.parent); output_root = Path(cmd["output_root"]); overwrite = bool(cmd.get("overwrite", False))
        paths = output_paths(audio, input_root, output_root)
        if current(audio, paths, args.model, overwrite):
            emit("skipped", path=str(audio), json_path=str(paths["json"])); continue
        started = time.perf_counter(); emit("started", path=str(audio))
        try:
            seg_iter, info = model.transcribe(
                str(audio), task="transcribe", language=None if args.language == "auto" else args.language,
                beam_size=5, temperature=0.0, vad_filter=False, word_timestamps=False,
                condition_on_previous_text=False,
                initial_prompt="Song lyrics. Transcribe the sung vocals accurately. Preserve the original language and repeated chorus lines.",
            )
            segments: list[Segment] = []; last_pct = -1
            duration = max(float(info.duration), 0.001)
            for seg in seg_iter:
                text = clean(seg.text)
                if text:
                    segments.append(Segment(float(seg.start), float(seg.end), text,
                        float(seg.avg_logprob) if seg.avg_logprob is not None else None,
                        float(seg.no_speech_prob) if seg.no_speech_prob is not None else None,
                        float(seg.compression_ratio) if seg.compression_ratio is not None else None))
                pct = min(99, int((float(seg.end) / duration) * 100))
                if pct >= last_pct + 2:
                    last_pct = pct; emit("progress", path=str(audio), progress=pct / 100.0, segment=text)
            save(audio, input_root, paths, segments, info, args)
            emit("done", path=str(audio), json_path=str(paths["json"]), segments=len(segments), language=str(info.language), elapsed=time.perf_counter()-started)
        except Exception as exc:
            emit("error", path=str(audio), message=f"{type(exc).__name__}: {exc}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
