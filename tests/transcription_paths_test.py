import json
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

from backend.whisper_worker import Segment, current, output_paths, save


class TranscriptionPathTests(unittest.TestCase):
    def test_selected_output_root_is_used_exactly(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            audio = root / "input" / "album" / "song.mp3"
            audio.parent.mkdir(parents=True)
            audio.write_bytes(b"current audio")
            output = root / "chosen lyrics"

            paths = output_paths(audio, root / "input", output)

            self.assertEqual(paths["json"], output / "album" / "song.lyrics.json")
            self.assertTrue(all(path.is_relative_to(output) for path in paths.values()))
            save(
                audio,
                root / "input",
                paths,
                [Segment(0.0, 1.5, "Aktueller Text")],
                SimpleNamespace(language="de", language_probability=0.99, duration=1.5),
                SimpleNamespace(model="large-v3", device="cpu", compute_type="int8", language="de"),
            )
            self.assertTrue(all(path.is_file() for path in paths.values()))
            document = json.loads(paths["json"].read_text(encoding="utf-8"))
            self.assertEqual(document["source"]["file"], str(audio))
            self.assertEqual(document["transcription"]["text"], "Aktueller Text")

    def test_same_name_with_different_source_is_not_skipped(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            audio = root / "new" / "same-name.mp3"
            audio.parent.mkdir(parents=True)
            audio.write_bytes(b"new audio contents")
            paths = output_paths(audio, audio.parent, root / "lyrics")
            for path in paths.values():
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("old", encoding="utf-8")
            stat = audio.stat()
            paths["json"].write_text(json.dumps({
                "source": {"file": str(root / "old" / audio.name), "size": stat.st_size, "mtime_ns": stat.st_mtime_ns},
                "pipeline": {"model": "large-v3"},
            }), encoding="utf-8")

            self.assertFalse(current(audio, paths, "large-v3", overwrite=False))


if __name__ == "__main__":
    unittest.main()
