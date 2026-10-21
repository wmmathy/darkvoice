#!/usr/bin/env python3
"""Small, offline repository checks; not a comprehensive security audit.

Default: scan tracked UTF-8 files for selected known credential/private-key formats.
Never print matched contents. History, ignored/untracked files and arbitrary custom
credentials are outside this check. Dependency mode requires immutable references
for simple direct FetchContent declarations; it does not evaluate licenses/CVEs.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

PATTERNS = (
    ("private-key", re.compile(r"-----BEGIN (?:RSA |EC |DSA |OPENSSH |ENCRYPTED )?PRIVATE KEY-----")),
    ("github-token", re.compile(r"\bgh[pousr]_[A-Za-z0-9]{20,}\b")),
    ("github-fine-grained-token", re.compile(r"\bgithub_pat_[A-Za-z0-9_]{40,}\b")),
    ("aws-access-key", re.compile(r"\b(?:AKIA|ASIA)[A-Z0-9]{16}\b")),
    ("openai-key", re.compile(r"\bsk-(?:(?:proj|svcacct)-)?[A-Za-z0-9_-]{24,}\b")),
    ("slack-token", re.compile(r"\bxox[baprs]-[A-Za-z0-9-]{20,}\b")),
)


def scan_text(text: str) -> list[tuple[int, str]]:
    findings = []
    for number, line in enumerate(text.splitlines(), start=1):
        for name, pattern in PATTERNS:
            if pattern.search(line):
                findings.append((number, name))
    return findings


def dependency_findings(text: str) -> list[str]:
    # Deliberately limited to direct declarations, not general CMake evaluation.
    # Reject substitutions/indirection for remote refs rather than guessing them.
    uncommented = re.sub(r"(?m)#.*$", "", text)
    calls = re.findall(r"\bFetchContent_Declare\s*\((.*?)\)", uncommented, re.I | re.S)
    findings = []
    for call in calls:
        name = call.split()[0] if call.split() else "unnamed"
        git = re.search(r'\bGIT_REPOSITORY\s+([^\s)]+)', call, re.I)
        url = re.search(r'(?<![A-Za-z_])URL\s+([^\s)]+)', call, re.I)
        if git:
            location = git.group(1).strip('"')
            pin = re.search(r'\bGIT_TAG\s+"?([0-9a-fA-F]{40})"?(?=\s|$)', call)
            if not location.startswith("https://") or "$" in location or not pin:
                findings.append(f"{name}: remote Git dependency requires HTTPS and a full commit SHA")
        if url:
            location = url.group(1).strip('"')
            checksum = re.search(r'\bURL_HASH\s+"?SHA256=[0-9a-fA-F]{64}"?(?=\s|$)', call)
            if not location.startswith("https://") or "$" in location or not checksum:
                findings.append(f"{name}: remote archive requires HTTPS and a SHA256 hash")
        if not git and not url:
            findings.append(f"{name}: dependency source is not a directly inspectable Git/URL declaration")
    return findings


def tracked_files() -> tuple[Path, list[Path]]:
    root = Path(subprocess.check_output(["git", "rev-parse", "--show-toplevel"], text=True).strip())
    names = subprocess.check_output(["git", "-C", str(root), "ls-files", "-z"])
    return root, [Path(name.decode("utf-8", "surrogateescape")) for name in names.split(b"\0") if name]


def self_test() -> None:
    # Construct fake fixtures at runtime so no token-shaped strings enter Git.
    planted = [
        "-----BEGIN " + "PRIVATE KEY-----",
        "ghp_" + "A" * 36,
        "github_pat_" + "A" * 60,
        "AKIA" + "A" * 16,
        "sk-proj-" + "A" * 30,
        "xoxb-" + "1" * 30,
    ]
    for fixture in planted:
        if not scan_text("fixture\n" + fixture):
            raise RuntimeError("Planted credential fixture was not detected")
    if scan_text("getRawParameterValue(id);\nsecret = os.environ.get('TOKEN')"):
        raise RuntimeError("Benign fixture was rejected")
    sha = "a" * 40
    hashed = "b" * 64
    for valid in (
        f"FetchContent_Declare(juce GIT_REPOSITORY https://example.org/juce.git GIT_TAG {sha})",
        f"FetchContent_Declare(lib URL https://example.org/lib.tar.gz URL_HASH SHA256={hashed})",
    ):
        if dependency_findings(valid):
            raise RuntimeError("Immutable dependency fixture was rejected")
    for invalid in (
        "FetchContent_Declare(lib GIT_REPOSITORY https://example.org/lib.git GIT_TAG main)",
        "FetchContent_Declare(lib GIT_REPOSITORY https://example.org/lib.git GIT_TAG v1.0)",
        "FetchContent_Declare(lib GIT_REPOSITORY https://example.org/lib.git GIT_TAG ${PIN})",
        "FetchContent_Declare(lib URL https://example.org/lib.tar.gz)",
        f"FetchContent_Declare(lib URL http://example.org/lib.tar.gz URL_HASH SHA256={hashed})",
    ):
        if not dependency_findings(invalid):
            raise RuntimeError("Mutable/unverified dependency fixture was not rejected")
    with tempfile.TemporaryDirectory(prefix="darkvoice-scanner-fixture-") as directory:
        fixture_root = Path(directory)
        subprocess.run(["git", "init", "-q", directory], check=True, capture_output=True)
        (fixture_root / "planted.txt").write_text("\n".join(planted), encoding="utf-8")
        subprocess.run(["git", "add", "planted.txt"], cwd=directory, check=True, capture_output=True)
        result = subprocess.run([sys.executable, str(Path(__file__).resolve())], cwd=directory,
                                capture_output=True, text=True)
        report = result.stdout + result.stderr
        if result.returncode != 1 or any(fixture in report for fixture in planted):
            raise RuntimeError("Planted-file scan failed to reject safely")
    print("PASS: planted credential and dependency fixtures; matched contents withheld")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--self-test", action="store_true")
    mode.add_argument("--check-dependencies", action="store_true")
    arguments = parser.parse_args()
    if arguments.self_test:
        self_test()
        return 0
    root, paths = tracked_files()
    failures = 0
    scanned = 0
    for relative in paths:
        if arguments.check_dependencies and relative.name != "CMakeLists.txt" and relative.suffix != ".cmake":
            continue
        data = (root / relative).read_bytes()
        if b"\0" in data:
            continue
        try:
            text = data.decode("utf-8")
        except UnicodeDecodeError:
            continue
        scanned += 1
        if arguments.check_dependencies:
            for finding in dependency_findings(text):
                print(f"FAIL {json.dumps(str(relative))}: {finding}", file=sys.stderr)
                failures += 1
        else:
            for line, rule in scan_text(text):
                print(f"FAIL {json.dumps(str(relative))}:{line}: {rule} (contents withheld)", file=sys.stderr)
                failures += 1
    if scanned == 0:
        print("FAIL: no applicable tracked text files scanned", file=sys.stderr)
        return 1
    print(f"Scanned {scanned} tracked text files; {failures} finding(s)")
    return 1 if failures else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        # Do not dump command output or file contents on failure.
        print(f"FAIL: checker could not complete ({type(error).__name__})", file=sys.stderr)
        sys.exit(2)
