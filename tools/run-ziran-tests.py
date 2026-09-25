#!/usr/bin/env python3
"""Run the independent Ziran behavior scripts with bounded parallelism."""

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import os
from pathlib import Path
import subprocess
import sys
import time


def positive_int(value):
    number = int(value)
    if number < 1:
        raise argparse.ArgumentTypeError("jobs must be at least 1")
    return number


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jobs", type=positive_int, default=4)
    parser.add_argument("--filter", default="", help="part of a test script name")
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args()

    repo = Path(__file__).resolve().parent.parent
    tests = sorted(repo.glob("tests/ziran_*_test.sh"))
    if args.filter:
        tests = [test for test in tests if args.filter in test.stem]
    if not tests:
        parser.error(f"no Ziran tests match {args.filter!r}")
    if args.list:
        for test in tests:
            print(test.relative_to(repo))
        return 0

    env = os.environ.copy()
    env.pop("DISPLAY", None)
    env.pop("WAYLAND_DISPLAY", None)

    def run(test):
        started = time.monotonic()
        result = subprocess.run(
            ["sh", str(test)], cwd=repo, env=env,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
            check=False,
        )
        return test, result.returncode, time.monotonic() - started, result.stdout

    print(f"Running {len(tests)} Ziran tests with {min(args.jobs, len(tests))} jobs", flush=True)
    failures = 0
    started = time.monotonic()
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [pool.submit(run, test) for test in tests]
        for future in as_completed(futures):
            test, code, elapsed, output = future.result()
            status = "PASS" if code == 0 else "FAIL"
            print(f"{status} {test.relative_to(repo)} ({elapsed:.1f}s)", flush=True)
            if code:
                failures += 1
                print(output, end="" if output.endswith("\n") else "\n", flush=True)

    elapsed = time.monotonic() - started
    print(f"{len(tests) - failures}/{len(tests)} passed in {elapsed:.1f}s", flush=True)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
