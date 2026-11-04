#!/usr/bin/env python3
"""Build a software-console-enabled CEmu runner in an isolated directory."""

import argparse
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True, help="CEmu source checkout")
    parser.add_argument("--out", type=Path, required=True, help="Disposable build directory")
    args = parser.parse_args()
    source, out = args.source.resolve(), args.out.resolve()
    if out == source or source in out.parents:
        parser.error("--out must be outside the CEmu checkout")
    shutil.copytree(source / "core", out / "core", dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns(".git", "*.o", "*.a", "*.so", "*.dylib"))
    runner = out / "tests/autotester"
    runner.mkdir(parents=True, exist_ok=True)
    for name in ("Makefile", "autotester.cpp", "autotester.h", "headless_cli.cpp", "crc32.hpp", "json11.hpp"):
        shutil.copy2(source / "tests/autotester" / name, runner / name)
    subprocess.run(["make", "-C", str(runner), "-B", "-j4", "cemu-headless", "DEBUG_SUPPORT=1"], check=True)
    print(runner / "cemu-headless")


if __name__ == "__main__":
    main()
