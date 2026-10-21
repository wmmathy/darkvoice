#!/usr/bin/env python3
"""Run a build, retain its log and expose errors through GitHub annotations."""
import pathlib
import subprocess
import sys


def main():
    if len(sys.argv) < 3:
        print('Usage: ci-build.py LOG COMMAND [ARG ...]', file=sys.stderr)
        return 2
    log = pathlib.Path(sys.argv[1])
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open('w') as output:
        result = subprocess.run(sys.argv[2:], stdout=output, stderr=subprocess.STDOUT, check=False)
    content = log.read_text(errors='replace')
    print(content, end='')
    if result.returncode:
        errors = [line for line in content.splitlines() if 'error:' in line.lower()]
        for line in (errors or content.splitlines()[-5:])[:5]:
            escaped = line[:1500].replace('%', '%25').replace('\r', '%0D').replace('\n', '%0A')
            print(f'::error title=Product build::{escaped}')
    return result.returncode if result.returncode >= 0 else 128 - result.returncode


if __name__ == '__main__':
    sys.exit(main())
