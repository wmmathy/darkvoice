#!/usr/bin/env python3
"""Verify the original, attributed preview recordings and their cue metadata."""

import argparse
import array
import copy
import hashlib
import json
import math
from pathlib import Path, PurePosixPath
import shutil
import sys
import tempfile
import wave


class AssetError(ValueError):
    pass


SOURCES = {
    "male_speech": (
        "male", "speech", "bdl", "JimLiu/baocut",
        "bc57be6f9afde8d738e25d3c8438da0377efd041",
        "packages/models/assets/tts-voices/en-male.wav",
        "31b4d6166009ea6c7367a01f5441d72c1f4bbeb9784781e3c5462f46a66651d3",
    ),
    "female_speech": (
        "female", "speech", "slt", "JimLiu/baocut",
        "bc57be6f9afde8d738e25d3c8438da0377efd041",
        "packages/models/assets/tts-voices/en-female.wav",
        "b152266467379260650c9c9959956efda37f3dc1e4bb3eeb4c5aad672b71501b",
    ),
    "male_singing": (
        "male", "singing", "m8", "mblackman/vocalset-vowel-voicebanks",
        "c48ab544e94e9788fec11be52b365026bb4a017c",
        "source/vocalset/m8_long_straight_a.wav",
        "9adce8c9c6d3bc93c1c8f3d4604d2007e5a9f0c83b0670e4be29570ef1d35628",
    ),
    "female_singing": (
        "female", "singing", "f1", "mblackman/vocalset-vowel-voicebanks",
        "c48ab544e94e9788fec11be52b365026bb4a017c",
        "source/vocalset/f1_long_straight_a.wav",
        "e1ff90d89de7e30a4dd717dd642173e57b083832f38e74eb8033d349c02bc340",
    ),
}
NOTICES = {
    "licenses/CMU_ARCTIC_COPYING.txt":
        "1e6cc6149cb8b08f9ad429973af9e4924e3706e32089910150be457dba266bf3",
    "licenses/VOCALSET_LICENSE.txt":
        "9cf1345fa81eecfcaf9aba014923c02f87dcf9e57bdd91e7458454c4ef614735",
    "licenses/VOCALSET_README.txt":
        "92e2632c2ecb4785283cb05d0fd4d7e4d51a6425b71e6a8a9184dab048ec8a4a",
    "licenses/VOCALSET_ATTRIBUTION.md":
        "883ed5d97b22c8bd20c30b77dcf8b4b9896abfcf93393584ce8a13ed1dfb4c1c",
}


def require(condition, message):
    if not condition:
        raise AssetError(message)


def safe_file(root, relative):
    require(isinstance(relative, str) and bool(relative), "Missing resource path")
    path = PurePosixPath(relative)
    require(not path.is_absolute() and ".." not in path.parts and "\\" not in relative,
            "Unsafe resource path")
    candidate = root.joinpath(*path.parts)
    require(candidate.resolve().is_relative_to(root.resolve()), "Resource escapes root")
    require(candidate.is_file(), "Missing resource file")
    return candidate


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def integer(value, minimum, maximum, name):
    require(type(value) is int and minimum <= value <= maximum, f"Invalid {name}")
    return value


def pcm_info(path):
    require(44 <= path.stat().st_size <= 5 * 1024 * 1024, "Invalid WAV size")
    try:
        with wave.open(str(path), "rb") as reader:
            require(reader.getcomptype() == "NONE", "Preview WAV must be PCM")
            require(reader.getnchannels() == 1 and reader.getsampwidth() == 2,
                    "Preview WAV must be mono PCM16")
            rate = integer(reader.getframerate(), 16000, 192000, "WAV sample rate")
            frames = integer(reader.getnframes(), 1, rate * 30, "WAV frame count")
            data = reader.readframes(frames)
    except (EOFError, wave.Error) as error:
        raise AssetError("Malformed WAV") from error
    require(len(data) == frames * 2, "Truncated PCM data")
    samples = array.array("h", data)
    if sys.byteorder != "little":
        samples.byteswap()
    require(not any(value <= -32768 or value >= 32767 for value in samples),
            "Full-scale/clipped PCM sample")
    energy = sum(float(value) * value for value in samples)
    require(energy > 0 and math.isfinite(energy), "Silent or invalid PCM")
    return rate, frames


def validate(root):
    try:
        data = json.loads(safe_file(root, "manifest.json").read_text())
    except (json.JSONDecodeError, UnicodeError) as error:
        raise AssetError("Malformed asset manifest") from error
    require(isinstance(data, dict) and type(data.get("schemaVersion")) is int and
            data["schemaVersion"] == 1, "Unsupported asset manifest version")
    require(data.get("originalAssetBytesUnchanged") is True,
            "Original-byte policy is required")
    try:
        credits = safe_file(root, data.get("creditsFile")).read_text()
    except UnicodeError as error:
        raise AssetError("Malformed recording credits") from error
    require(all(word in credits for word in
                ("Carnegie Mellon", "VocalSet", "Wilkins", "CC BY 4.0", "modification")),
            "Incomplete recording credits")
    for relative, expected in NOTICES.items():
        require(sha256(safe_file(root, relative)) == expected,
                "Recording licence/metadata notice changed")
    assets = data.get("assets")
    require(isinstance(assets, list) and len(assets) == len(SOURCES),
            "Manifest must contain the four preview assets")
    seen = set()
    for asset in assets:
        require(isinstance(asset, dict), "Invalid asset entry")
        identity = asset.get("id")
        require(isinstance(identity, str) and identity in SOURCES and identity not in seen,
                "Unknown/duplicate asset ID")
        seen.add(identity)
        voice, kind, speaker, repo, commit, source_path, expected_sha = SOURCES[identity]
        require(asset.get("voice") == voice and asset.get("publishedVoiceLabel") == voice and
                asset.get("kind") == kind and asset.get("speakerId") == speaker,
                "Published voice/source metadata mismatch")
        source = asset.get("source")
        require(isinstance(source, dict), "Missing source provenance")
        require(source.get("repository") == repo and source.get("commit") == commit and
                source.get("path") == source_path and source.get("url") ==
                f"https://raw.githubusercontent.com/{repo}/{commit}/{source_path}",
                "Source pin/URL mismatch")
        require(isinstance(source.get("priorChanges"), list) and source["priorChanges"] and
                all(isinstance(change, str) and change for change in source["priorChanges"]),
                "Source modification history missing")
        require(asset.get("sourceSha256") == expected_sha and
                asset.get("destinationSha256") == expected_sha, "Original hash mismatch")
        require(asset.get("file") == identity + ".wav", "Unexpected resource filename")
        audio = safe_file(root, asset["file"])
        require(sha256(audio) == expected_sha, "WAV hash mismatch")
        rate, frames = pcm_info(audio)
        require(type(asset.get("sampleRate")) is int and asset["sampleRate"] == rate and
                type(asset.get("frames")) is int and asset["frames"] == frames and
                type(asset.get("channels")) is int and asset["channels"] == 1 and
                type(asset.get("bitsPerSample")) is int and asset["bitsPerSample"] == 16,
                "Declared PCM format does not match WAV")
        start = integer(asset.get("cueStartFrame"), 0, frames - 1, "cue start")
        count = integer(asset.get("cueFrames"), 1, min(frames, rate * 6), "cue length")
        require(start + count <= frames, "Cue exceeds recording")
        licence = asset.get("licence")
        require(isinstance(licence, dict), "Missing recording licence")
        expected_notice = ("licenses/CMU_ARCTIC_COPYING.txt" if kind == "speech" else
                           "licenses/VOCALSET_LICENSE.txt")
        expected_id = "LicenseRef-CMU-Arctic" if kind == "speech" else "CC-BY-4.0"
        require(licence.get("id") == expected_id and licence.get("file") == expected_notice and
                licence.get("sha256") == NOTICES[expected_notice], "Licence identity mismatch")
        require(sha256(safe_file(root, licence["file"])) == licence["sha256"],
                "Licence hash mismatch")
        provenance = asset.get("provenance")
        require(isinstance(provenance, dict) and
                provenance.get("primaryArchiveByteIdentityVerified") is False and
                isinstance(provenance.get("note"), str) and provenance["note"],
                "Primary archive verification limits must be preserved")
    return len(seen)


def self_test(root):
    validate(root)
    rejected = 0
    with tempfile.TemporaryDirectory(prefix="darkvoice-preview-assets-") as temporary:
        base = Path(temporary)
        test_root = base / "preview"
        shutil.copytree(root, test_root)
        manifest = test_root / "manifest.json"
        original = json.loads(manifest.read_text())

        def expect_rejection(change, label):
            nonlocal rejected
            trial = copy.deepcopy(original)
            change(trial)
            manifest.write_text(json.dumps(trial))
            try:
                validate(test_root)
            except (AssetError, OSError):
                rejected += 1
            else:
                raise AssetError(f"Guard accepted {label}")
            finally:
                manifest.write_text(json.dumps(original))

        expect_rejection(lambda d: d.update(schemaVersion=2), "future schema")
        expect_rejection(lambda d: d.update(schemaVersion=True), "boolean schema")
        expect_rejection(lambda d: d.update(originalAssetBytesUnchanged=False), "changed originals")
        expect_rejection(lambda d: d.update(creditsFile="../outside.md"), "unsafe credits path")
        expect_rejection(lambda d: d["assets"].pop(), "missing entry")
        expect_rejection(lambda d: d["assets"].__setitem__(1, d["assets"][0]), "duplicate entry")
        expect_rejection(lambda d: d["assets"][0].update(id=[]), "malformed asset ID")
        mutations = [
            ("file", "../escape.wav"), ("file", "/tmp/escape.wav"),
            ("file", "..\\escape.wav"), ("sourceSha256", "0" * 64),
            ("destinationSha256", "0" * 64), ("sampleRate", 44100),
            ("frames", original["assets"][0]["frames"] + 1), ("channels", 2),
            ("bitsPerSample", 24), ("cueStartFrame", -1), ("cueFrames", 0),
            ("cueFrames", True), ("cueFrames", 96001),
            ("voice", "female"), ("kind", "singing"),
        ]
        for key, value in mutations:
            expect_rejection(lambda d, k=key, v=value: d["assets"][0].update({k: v}), key)
        expect_rejection(lambda d: d["assets"][0]["source"].update(priorChanges=[123]),
                         "malformed modification history")
        expect_rejection(lambda d: d["assets"][0]["source"].update(url="https://example.com/a.wav"),
                         "untrusted source URL")
        expect_rejection(lambda d: d["assets"][0]["licence"].update(file="../licence.txt"),
                         "unsafe licence path")
        expect_rejection(lambda d: d["assets"][0]["provenance"].update(
            primaryArchiveByteIdentityVerified=True), "unsupported primary identity claim")

        audio = test_root / "male_speech.wav"
        audio_bytes = audio.read_bytes()
        for contents in (None, audio_bytes[:-1]):
            if contents is None:
                audio.unlink()
            else:
                audio.write_bytes(contents)
            try:
                validate(test_root)
            except (AssetError, OSError):
                rejected += 1
            else:
                raise AssetError("Guard accepted missing/corrupt WAV")
            finally:
                audio.write_bytes(audio_bytes)
        outside = base / "outside.wav"
        outside.write_bytes(audio_bytes)
        audio.unlink()
        audio.symlink_to(outside)
        try:
            validate(test_root)
        except AssetError:
            rejected += 1
        else:
            raise AssetError("Guard accepted symlink escape")
        finally:
            audio.unlink()
            audio.write_bytes(audio_bytes)

        notice = test_root / "licenses/CMU_ARCTIC_COPYING.txt"
        notice_bytes = notice.read_bytes()
        notice.write_bytes(b"missing required licence terms")
        try:
            validate(test_root)
        except AssetError:
            rejected += 1
        else:
            raise AssetError("Guard accepted changed licence")
        finally:
            notice.write_bytes(notice_bytes)

        for label, samples in (("clipped", [0, 32767, 0]), ("silent", [0, 0, 0])):
            probe = base / (label + ".wav")
            with wave.open(str(probe), "wb") as writer:
                writer.setnchannels(1)
                writer.setsampwidth(2)
                writer.setframerate(16000)
                pcm = array.array("h", samples)
                if sys.byteorder != "little":
                    pcm.byteswap()
                writer.writeframes(pcm.tobytes())
            try:
                pcm_info(probe)
            except AssetError:
                rejected += 1
            else:
                raise AssetError(f"Guard accepted {label} PCM")
        manifest = test_root / "manifest.json"
        manifest.write_text("{")
        try:
            validate(test_root)
        except AssetError:
            rejected += 1
        else:
            raise AssetError("Guard accepted malformed JSON")
        finally:
            manifest.write_text(json.dumps(original))
        validate(test_root)
    return rejected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path,
                        default=Path(__file__).resolve().parents[1] / "resources/preview")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    try:
        count = validate(args.root)
        print(f"PASS: {count} attributed original preview WAVs, PCM and cue metadata")
        if args.self_test:
            print(f"PASS: {self_test(args.root)} malformed/missing/corrupt asset guards")
    except (AssetError, OSError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
