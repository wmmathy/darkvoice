#!/usr/bin/env python3
"""Stream build output, retain its log and annotate errors without masking status."""
import collections
import pathlib
import subprocess
import sys


def annotation(message):
    escaped = message[:1500].replace('%', '%25').replace('\r', '%0D').replace('\n', '%0A')
    print(f'::error title=Product build::{escaped}', flush=True)


def main():
    if len(sys.argv) < 3:
        print('Usage: ci-build.py LOG COMMAND [ARG ...]', file=sys.stderr)
        return 2
    log = pathlib.Path(sys.argv[1])
    log.parent.mkdir(parents=True, exist_ok=True)
    recent = collections.deque(maxlen=5)
    errors = []
    with log.open('w') as output:
        try:
            process = subprocess.Popen(sys.argv[2:], stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, text=True,
                                       errors='replace', bufsize=1)
        except OSError as error:
            message = f'Cannot start build command: {error}'
            output.write(message + '\n')
            annotation(message)
            return 127
        with process.stdout:
            for line in process.stdout:
                output.write(line)
                output.flush()
                print(line, end='', flush=True)
                recent.append(line.rstrip())
                if any(marker in line.lower() for marker in ('error:', 'fail', 'sanitizer:', 'assertion failure', 'leaked objects')) and len(errors) < 5:
                    errors.append(line.rstrip())
        status = process.wait()
    if status:
        for line in errors or recent:
            annotation(line)
    return status if status >= 0 else 128 - status


if __name__ == '__main__':
    sys.exit(main())
