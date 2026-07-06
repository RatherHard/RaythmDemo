# RaythmDemo - Malody .mc Chart Converter
# Converts Malody-style .mc chart JSON into RaythmDemo chart JSON.
# Author: RatherHard
# Date: 2026-07-06

import argparse
import json
import math
import os
import stat
import sys
import tempfile
from fractions import Fraction
from pathlib import Path, PurePosixPath, PureWindowsPath


EXPECTED_LANE_COUNT = 4
MAX_CHART_JSON_SIZE_BYTES = 1024 * 1024
MAX_BEAT_TUPLE_COMPONENT = 1_000_000
INT32_MIN = -(2**31)
INT32_MAX = 2**31 - 1
ZERO_BEAT = Fraction(0, 1)


class ConversionError(ValueError):
    """Raised when a source .mc chart cannot be converted safely."""


class CliError(RuntimeError):
    """Raised when CLI file handling cannot complete."""


def _conversion_error(message):
    """Build a consistent conversion error message."""
    return ConversionError(f"Invalid .mc chart: {message}")


def _is_integer(value):
    """Return true when value is an integer but not a JSON boolean."""
    return isinstance(value, int) and not isinstance(value, bool)


def _require_object(value, context):
    """Validate that a JSON value is an object."""
    if not isinstance(value, dict):
        raise _conversion_error(f"{context} must be an object")
    return value


def _require_array(value, context):
    """Validate that a JSON value is an array."""
    if not isinstance(value, list):
        raise _conversion_error(f"{context} must be an array")
    return value


def _require_field(mapping, field, context):
    """Read a required object field with contextual error reporting."""
    if field not in mapping:
        raise _conversion_error(f"{context}.{field} is required")
    return mapping[field]


def _read_string(mapping, field, context):
    """Read a required string field from an object."""
    value = _require_field(mapping, field, context)
    if not isinstance(value, str):
        raise _conversion_error(f"{context}.{field} must be a string")
    if value == "":
        raise _conversion_error(f"{context}.{field} must not be empty")
    return value


def _read_optional_string(mapping, field):
    """Read an optional non-empty string field."""
    value = mapping.get(field)
    if value is None:
        return None
    if not isinstance(value, str) or value == "":
        return None
    return value


def _read_int(mapping, field, context, *, minimum=INT32_MIN, maximum=INT32_MAX):
    """Read a required bounded integer field from an object."""
    value = _require_field(mapping, field, context)
    if not _is_integer(value):
        raise _conversion_error(f"{context}.{field} must be an integer")
    if value < minimum or value > maximum:
        raise _conversion_error(f"{context}.{field} is outside the supported integer range")
    return value


def _read_beat_tuple(value, context):
    """Read and validate a RaythmDemo beat tuple."""
    if not isinstance(value, list) or len(value) != 3:
        raise _conversion_error(f"{context} must be a three-integer beat array")
    if not all(_is_integer(entry) for entry in value):
        raise _conversion_error(f"{context} entries must be integers")

    measure, numerator, denominator = value
    if any(abs(component) > MAX_BEAT_TUPLE_COMPONENT for component in value):
        raise _conversion_error(f"{context} entries are outside the supported beat component range")
    if measure < 0:
        raise _conversion_error(f"{context} measure must be greater than or equal to zero")
    if denominator <= 0:
        raise _conversion_error(f"{context} denominator must be greater than zero")
    if numerator < 0 or numerator >= denominator:
        raise _conversion_error(f"{context} numerator must satisfy 0 <= y < z")
    return [measure, numerator, denominator]


def _beat_key(beat):
    """Normalize a beat tuple into whole-beat units for comparison."""
    measure, numerator, denominator = beat
    return Fraction(measure * 4, 1) + Fraction(numerator * 4, denominator)


def _read_song_metadata(meta):
    """Read .mc song metadata from the observed nested layout."""
    song = meta.get("song")
    if song is None:
        raise _conversion_error("meta.song is required")
    _require_object(song, "meta.song")

    title = _read_optional_string(song, "title") or _read_optional_string(song, "titleorg")
    artist = _read_optional_string(song, "artist") or _read_optional_string(song, "artistorg")
    if title is None:
        raise _conversion_error("meta.song.title is required")
    if artist is None:
        raise _conversion_error("meta.song.artist is required")

    return title, artist


def _read_mode_ext(meta):
    """Read and validate the .mc mode_ext block for the current 4K runtime."""
    mode_ext = _require_object(_require_field(meta, "mode_ext", "meta"), "meta.mode_ext")
    lane_count = _read_int(mode_ext, "column", "meta.mode_ext", minimum=1, maximum=INT32_MAX)
    if lane_count != EXPECTED_LANE_COUNT:
        raise _conversion_error(
            f"meta.mode_ext.column must be {EXPECTED_LANE_COUNT} for the current RaythmDemo format"
        )
    return lane_count


def _read_timing_points(time_array):
    """Convert and validate source timing points."""
    _require_array(time_array, "time")
    if not time_array:
        raise _conversion_error("time must contain at least one BPM entry")

    timing_points = []
    seen_beats = set()
    for index, timing_object in enumerate(time_array):
        context = f"time[{index}]"
        _require_object(timing_object, context)
        beat = _read_beat_tuple(_require_field(timing_object, "beat", context), f"{context}.beat")
        bpm = _require_field(timing_object, "bpm", context)
        if not isinstance(bpm, (int, float)) or isinstance(bpm, bool) or not math.isfinite(bpm) or bpm <= 0:
            raise _conversion_error(f"{context}.bpm must be finite and greater than zero")

        normalized_beat = _beat_key(beat)
        if normalized_beat in seen_beats:
            raise _conversion_error("duplicate BPM entries at the same normalized beat are not allowed")
        seen_beats.add(normalized_beat)
        timing_points.append({"beat": beat, "bpm": bpm})

    timing_points.sort(key=lambda point: _beat_key(point["beat"]))
    if _beat_key(timing_points[0]["beat"]) != ZERO_BEAT:
        raise _conversion_error("first BPM entry must start at beat [0, 0, 1]")
    return timing_points


def _is_audio_note(note_object):
    """Return true when an .mc note object carries audio metadata."""
    return "sound" in note_object or "offset" in note_object


def _read_audio_note(note_object, context):
    """Read the non-playable .mc note that carries audio metadata."""
    sound = _read_string(note_object, "sound", context)
    offset = _read_int(note_object, "offset", context)
    if "column" in note_object:
        raise _conversion_error(f"{context} audio note must not contain column")
    return {"sound": sound, "offset": offset}


def _read_playable_note(note_object, context, lane_count):
    """Read one playable tap or hold note from the .mc note array."""
    beat = _read_beat_tuple(_require_field(note_object, "beat", context), f"{context}.beat")
    column = _read_int(note_object, "column", context)
    if column < 0 or column >= lane_count:
        raise _conversion_error(f"{context}.column must be in range [0, {lane_count - 1}]")

    converted = {"beat": beat, "column": column}
    if "endbeat" in note_object:
        endbeat = _read_beat_tuple(note_object["endbeat"], f"{context}.endbeat")
        if _beat_key(endbeat) <= _beat_key(beat):
            raise _conversion_error(f"{context}.endbeat must be greater than beat")
        converted["endbeat"] = endbeat
    return converted


def _read_notes(note_array, lane_count):
    """Convert playable notes and extract the single audio metadata note."""
    _require_array(note_array, "note")
    converted_notes = []
    audio_note = None

    for index, note_object in enumerate(note_array):
        context = f"note[{index}]"
        _require_object(note_object, context)
        if _is_audio_note(note_object):
            if audio_note is not None:
                raise _conversion_error("Multiple audio metadata notes are not allowed")
            audio_note = _read_audio_note(note_object, context)
            continue

        if "column" not in note_object:
            raise _conversion_error(f"{context} has an unknown non-playable note shape")
        converted_notes.append(_read_playable_note(note_object, context, lane_count))

    if audio_note is None:
        raise _conversion_error("one audio metadata note with sound and offset is required")

    converted_notes.sort(
        key=lambda note: (
            _beat_key(note["beat"]),
            note["column"],
            _beat_key(note["endbeat"]) if "endbeat" in note else Fraction(-1, 1),
        )
    )
    return converted_notes, audio_note


def _validate_relative_resource_path(path_text, context):
    """Validate a resource path before it is emitted into RaythmDemo metadata."""
    path = PurePosixPath(path_text.replace("\\", "/"))
    windows_path = PureWindowsPath(path_text)
    if path_text == "" or path.is_absolute() or windows_path.is_absolute() or windows_path.drive:
        raise _conversion_error(f"{context} must be a non-empty relative path")
    if any(part in ("", ".", "..") for part in path.parts):
        raise _conversion_error(f"{context} must not contain empty, current, or parent path segments")
    return path


def _is_relative_to(candidate, root):
    """Return true when candidate is equal to or inside root."""
    try:
        candidate.relative_to(root)
        return True
    except ValueError:
        return False


def _resolve_audio_path(sound, source_path=None, asset_root=None):
    """Resolve the .mc sound entry into a RaythmDemo relative resource path."""
    relative_sound = _validate_relative_resource_path(sound, "note audio sound")
    if source_path is None:
        return relative_sound.as_posix()

    candidate = (Path(source_path).resolve().parent / Path(*relative_sound.parts)).resolve()
    if asset_root is None:
        return relative_sound.as_posix()

    root = Path(asset_root).resolve()
    if not _is_relative_to(candidate, root):
        raise _conversion_error("audio file resolved from sound escapes the configured asset root")
    return candidate.relative_to(root).as_posix()


def convert_mc_chart(data, *, source_path=None, asset_root=None):
    """Convert a parsed .mc chart object into RaythmDemo chart JSON data."""
    root = _require_object(data, "root")
    meta = _require_object(_require_field(root, "meta", "root"), "meta")
    title, artist = _read_song_metadata(meta)
    creator = _read_string(meta, "creator", "meta")
    lane_count = _read_mode_ext(meta)
    timing_points = _read_timing_points(_require_field(root, "time", "root"))
    notes, audio_note = _read_notes(_require_field(root, "note", "root"), lane_count)

    return {
        "meta": {
            "title": title,
            "artist": artist,
            "creator": creator,
            "path": _resolve_audio_path(audio_note["sound"], source_path=source_path, asset_root=asset_root),
            "offset": audio_note["offset"],
        },
        "time": timing_points,
        "note": notes,
    }


def _default_asset_root_for(source_path):
    """Infer the repository assets root when the input chart is inside assets/."""
    repo_asset_root = Path(__file__).resolve().parents[1] / "assets"
    try:
        Path(source_path).resolve().relative_to(repo_asset_root.resolve())
    except ValueError:
        return None
    return repo_asset_root


def _json_text(data, pretty):
    """Serialize converted chart data with stable UTF-8 friendly formatting."""
    if pretty:
        return json.dumps(data, ensure_ascii=False, indent=4) + "\n"
    return json.dumps(data, ensure_ascii=False, separators=(",", ":")) + "\n"


def _parse_args(argv):
    """Parse converter CLI arguments."""
    parser = argparse.ArgumentParser(
        description="Convert a Malody-style .mc chart into RaythmDemo chart JSON."
    )
    parser.add_argument("input", type=Path, help="source .mc chart path")
    parser.add_argument("output", nargs="?", type=Path, help="target RaythmDemo .json path")
    parser.add_argument("--asset-root", type=Path, help="asset root used for output meta.path rewriting")
    parser.add_argument("--stdout", action="store_true", help="write converted JSON to stdout instead of a file")
    parser.add_argument("--pretty", action="store_true", help="pretty-print the converted JSON")
    parser.add_argument("--force", action="store_true", help="overwrite an existing output file")
    return parser.parse_args(argv)


def _load_json_file(path):
    """Read and parse a bounded UTF-8 JSON file."""
    try:
        file_size = path.stat().st_size
        if file_size > MAX_CHART_JSON_SIZE_BYTES:
            raise CliError(f"input file exceeds {MAX_CHART_JSON_SIZE_BYTES} bytes: {path}")
        with path.open("r", encoding="utf-8") as input_file:
            return json.load(input_file)
    except OSError as error:
        raise CliError(f"failed to read {path}: {error}") from error
    except json.JSONDecodeError as error:
        raise CliError(f"failed to parse {path} as JSON: {error}") from error


def _path_is_link_or_reparse_point(path):
    """Return true when a path is a symlink or Windows reparse point."""
    try:
        metadata = path.lstat()
    except FileNotFoundError:
        return False
    if stat.S_ISLNK(metadata.st_mode):
        return True
    attributes = getattr(metadata, "st_file_attributes", 0)
    reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0)
    return reparse_flag != 0 and (attributes & reparse_flag) != 0


def _write_new_file_exclusively(path, text):
    """Create a new text file without following a final symlink path."""
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    descriptor = os.open(path, flags, 0o644)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as output_file:
            descriptor = None
            output_file.write(text)
    finally:
        if descriptor is not None:
            os.close(descriptor)


def _replace_regular_file(path, text):
    """Atomically replace a regular file without following an existing link target."""
    if path.exists() and (_path_is_link_or_reparse_point(path) or not path.is_file()):
        raise CliError(f"output path is not a regular file: {path}")

    temporary_name = None
    try:
        with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=path.parent, delete=False) as temporary_file:
            temporary_name = Path(temporary_file.name)
            temporary_file.write(text)
        os.replace(temporary_name, path)
    except OSError as error:
        raise CliError(f"failed to write {path}: {error}") from error
    finally:
        if temporary_name is not None and temporary_name.exists():
            temporary_name.unlink()


def _write_json_file(path, text, force):
    """Write converted JSON to disk without clobbering by default."""
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        if force:
            _replace_regular_file(path, text)
            return
        _write_new_file_exclusively(path, text)
    except FileExistsError as error:
        raise CliError(f"output file already exists: {path}; use --force to overwrite") from error
    except OSError as error:
        raise CliError(f"failed to write {path}: {error}") from error


def main(argv=None):
    """Run the converter command-line interface."""
    args = _parse_args(sys.argv[1:] if argv is None else argv)
    try:
        input_path = args.input
        output_path = args.output or input_path.with_suffix(".json")
        if args.stdout and args.output is not None:
            raise CliError("output path cannot be combined with --stdout")

        asset_root = args.asset_root if args.asset_root is not None else _default_asset_root_for(input_path)
        converted = convert_mc_chart(_load_json_file(input_path), source_path=input_path, asset_root=asset_root)
        text = _json_text(converted, args.pretty)
        if args.stdout:
            print(text, end="")
            return 0

        _write_json_file(output_path, text, args.force)
        print(f"Converted {input_path} -> {output_path}")
        return 0
    except (CliError, ConversionError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
