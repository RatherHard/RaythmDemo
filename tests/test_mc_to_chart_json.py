# RaythmDemo - .mc Chart Converter Tests
# Verifies conversion from Malody-style .mc JSON to RaythmDemo chart JSON.
# Author: RatherHard
# Date: 2026-07-06

import contextlib
import importlib.util
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = REPO_ROOT / "tools" / "mc_to_chart_json.py"
ASSET_ROOT = REPO_ROOT / "assets"
SAMPLE_MC_PATH = ASSET_ROOT / "charts" / "00001" / "1627802685.mc"


def load_converter_module():
    """Load the converter module directly from tools/ without requiring a package."""
    spec = importlib.util.spec_from_file_location("mc_to_chart_json", MODULE_PATH)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class McToChartJsonTests(unittest.TestCase):
    """Covers pure conversion and CLI behavior for the .mc converter."""

    @classmethod
    def setUpClass(cls):
        cls.converter = load_converter_module()

    def make_minimal_mc(self):
        """Return a minimal .mc-like chart with one tap, one hold, and one audio note."""
        return {
            "meta": {
                "creator": "Test Charter",
                "song": {
                    "title": "Test Song",
                    "artist": "Test Artist",
                },
                "mode_ext": {
                    "column": 4,
                },
            },
            "time": [
                {"beat": [0, 0, 1], "bpm": 120.0},
            ],
            "note": [
                {"beat": [2, 0, 1], "column": 2},
                {"beat": [1, 0, 1], "endbeat": [1, 1, 2], "column": 1},
                {"beat": [0, 0, 1], "sound": "music.ogg", "offset": 12, "type": 1, "vol": 100},
            ],
        }

    def test_real_fixture_conversion_matches_raythm_schema(self):
        with SAMPLE_MC_PATH.open("r", encoding="utf-8") as chart_file:
            source_data = json.load(chart_file)

        converted = self.converter.convert_mc_chart(
            source_data,
            source_path=SAMPLE_MC_PATH,
            asset_root=ASSET_ROOT,
        )

        self.assertEqual(converted["meta"]["title"], "enchanted love")
        self.assertEqual(converted["meta"]["artist"], "linear ring")
        self.assertEqual(converted["meta"]["creator"], "YuzukiY")
        self.assertEqual(converted["meta"]["path"], "charts/00001/1627802685.ogg")
        self.assertEqual(converted["meta"]["offset"], 289)
        self.assertEqual(len(converted["time"]), 1)
        self.assertAlmostEqual(converted["time"][0]["bpm"], 190.00255573050448)
        self.assertEqual(len(converted["note"]), 1166)
        self.assertTrue(all("sound" not in note for note in converted["note"]))
        self.assertTrue(all("column" in note for note in converted["note"]))

    def test_minimal_chart_maps_tap_hold_and_audio_metadata(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            asset_root = Path(temporary_directory) / "assets"
            chart_path = asset_root / "charts" / "example" / "chart.mc"
            audio_path = chart_path.parent / "music.ogg"
            audio_path.parent.mkdir(parents=True)
            audio_path.write_bytes(b"fake")

            converted = self.converter.convert_mc_chart(
                self.make_minimal_mc(),
                source_path=chart_path,
                asset_root=asset_root,
            )

        self.assertEqual(
            converted,
            {
                "meta": {
                    "title": "Test Song",
                    "artist": "Test Artist",
                    "creator": "Test Charter",
                    "path": "charts/example/music.ogg",
                    "offset": 12,
                },
                "time": [
                    {"beat": [0, 0, 1], "bpm": 120.0},
                ],
                "note": [
                    {"beat": [0, 1, 4], "endbeat": [0, 3, 8], "column": 1},
                    {"beat": [0, 1, 2], "column": 2},
                ],
            },
        )

    def test_song_metadata_can_fallback_to_original_fields(self):
        source_data = self.make_minimal_mc()
        source_data["meta"]["song"] = {
            "titleorg": "Original Title",
            "artistorg": "Original Artist",
        }

        converted = self.converter.convert_mc_chart(source_data)

        self.assertEqual(converted["meta"]["title"], "Original Title")
        self.assertEqual(converted["meta"]["artist"], "Original Artist")

    def test_missing_sound_note_is_rejected(self):
        source_data = self.make_minimal_mc()
        source_data["note"] = [note for note in source_data["note"] if "column" in note]

        with self.assertRaisesRegex(ValueError, "audio"):
            self.converter.convert_mc_chart(source_data)

    def test_duplicate_sound_notes_are_rejected(self):
        source_data = self.make_minimal_mc()
        source_data["note"].append({"beat": [0, 0, 1], "sound": "other.ogg", "offset": 0})

        with self.assertRaisesRegex(ValueError, "Multiple"):
            self.converter.convert_mc_chart(source_data)

    def test_unknown_non_playable_note_is_rejected(self):
        source_data = self.make_minimal_mc()
        source_data["note"].append({"beat": [0, 0, 1], "type": 2})

        with self.assertRaisesRegex(ValueError, "non-playable"):
            self.converter.convert_mc_chart(source_data)

    def test_non_four_key_mode_is_rejected(self):
        source_data = self.make_minimal_mc()
        source_data["meta"]["mode_ext"]["column"] = 6

        with self.assertRaisesRegex(ValueError, "mode_ext.column"):
            self.converter.convert_mc_chart(source_data)

    def test_invalid_timing_points_are_rejected(self):
        invalid_cases = [
            ([{"beat": [1, 0, 1], "bpm": 120.0}], "first BPM"),
            ([{"beat": [0, 0, 1], "bpm": 0.0}], "bpm"),
            ([{"beat": [0, 0, 1], "bpm": 120.0}, {"beat": [0, 0, 2], "bpm": 130.0}], "duplicate"),
        ]

        for time_entries, expected_message in invalid_cases:
            with self.subTest(expected_message=expected_message):
                source_data = self.make_minimal_mc()
                source_data["time"] = time_entries

                with self.assertRaisesRegex(ValueError, expected_message):
                    self.converter.convert_mc_chart(source_data)

    def test_invalid_hold_endbeat_is_rejected(self):
        source_data = self.make_minimal_mc()
        source_data["note"][1]["endbeat"] = [1, 0, 1]

        with self.assertRaisesRegex(ValueError, "endbeat"):
            self.converter.convert_mc_chart(source_data)

    def test_windows_absolute_sound_paths_are_rejected(self):
        invalid_sounds = [
            "C:/Windows/win.ini",
            "C:\\Windows\\win.ini",
            "//server/share/audio.ogg",
            "\\\\server\\share\\audio.ogg",
        ]

        for sound in invalid_sounds:
            with self.subTest(sound=sound):
                source_data = self.make_minimal_mc()
                source_data["note"][2]["sound"] = sound

                with self.assertRaisesRegex(ValueError, "relative path"):
                    self.converter.convert_mc_chart(source_data)

    def test_runtime_incompatible_integer_ranges_are_rejected(self):
        invalid_cases = [
            (lambda data: data["note"][2].update({"offset": 3_000_000_000}), "offset"),
            (lambda data: data["note"][0].update({"beat": [1_000_001, 0, 1]}), "beat component"),
        ]

        for mutate, expected_message in invalid_cases:
            with self.subTest(expected_message=expected_message):
                source_data = self.make_minimal_mc()
                mutate(source_data)

                with self.assertRaisesRegex(ValueError, expected_message):
                    self.converter.convert_mc_chart(source_data)

    def test_cli_rejects_oversized_input_before_parsing(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            input_path = Path(temporary_directory) / "large.mc"
            input_path.write_text(" " * (self.converter.MAX_CHART_JSON_SIZE_BYTES + 1), encoding="utf-8")
            stderr = io.StringIO()

            with contextlib.redirect_stderr(stderr):
                result = self.converter.main([str(input_path), "--stdout"])

            self.assertNotEqual(result, 0)
            self.assertIn("exceeds", stderr.getvalue())

    def test_cli_stdout_outputs_json_without_writing_file(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            input_path = root / "chart.mc"
            input_path.write_text(json.dumps(self.make_minimal_mc()), encoding="utf-8")
            stdout = io.StringIO()

            with contextlib.redirect_stdout(stdout):
                result = self.converter.main([str(input_path), "--stdout", "--pretty"])

            self.assertEqual(result, 0)
            output = json.loads(stdout.getvalue())
            self.assertEqual(output["meta"]["path"], "music.ogg")
            self.assertFalse(input_path.with_suffix(".json").exists())

    def test_cli_default_output_path_refuses_overwrite_without_force(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            input_path = root / "chart.mc"
            output_path = root / "chart.json"
            input_path.write_text(json.dumps(self.make_minimal_mc()), encoding="utf-8")
            output_path.write_text("{}", encoding="utf-8")
            stderr = io.StringIO()

            with contextlib.redirect_stderr(stderr):
                result = self.converter.main([str(input_path)])

            self.assertNotEqual(result, 0)
            self.assertIn("exists", stderr.getvalue())
            self.assertEqual(output_path.read_text(encoding="utf-8"), "{}")

            stdout = io.StringIO()
            with contextlib.redirect_stdout(stdout):
                result = self.converter.main([str(input_path), "--force"])

            self.assertEqual(result, 0)
            self.assertIn("Converted", stdout.getvalue())
            self.assertEqual(json.loads(output_path.read_text(encoding="utf-8"))["meta"]["title"], "Test Song")


if __name__ == "__main__":
    unittest.main(verbosity=2)
