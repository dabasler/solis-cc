#!/usr/bin/env python3
"""Install qSOLIS source into a CloudCompare source checkout.

This is a source integration helper, not a binary plugin installer.
"""
from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("cloudcompare", type=Path, help="CloudCompare source-tree root")
    parser.add_argument("--force", action="store_true", help="replace an existing plugins/core/Standard/qSolis directory")
    parser.add_argument("--symlink", action="store_true", help="symlink qSolis instead of copying it")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = args.cloudcompare.expanduser().resolve()
    standard = root / "plugins" / "core" / "Standard"
    parent_cmake = standard / "CMakeLists.txt"
    source = Path(__file__).resolve().parents[1] / "qSolis"
    target = standard / "qSolis"

    if not parent_cmake.is_file():
        print(f"error: {root} does not look like a current CloudCompare source tree", file=sys.stderr)
        return 2
    if not source.is_dir():
        print(f"error: source plugin directory not found: {source}", file=sys.stderr)
        return 2

    if target.exists() or target.is_symlink():
        if not args.force:
            print(f"error: {target} already exists (use --force to replace it)", file=sys.stderr)
            return 2
        if target.is_symlink() or target.is_file():
            target.unlink()
        else:
            shutil.rmtree(target)

    if args.symlink:
        target.symlink_to(source, target_is_directory=True)
    else:
        shutil.copytree(source, target, ignore=shutil.ignore_patterns("legacy"))

    text = parent_cmake.read_text(encoding="utf-8")
    cmake_line = "add_subdirectory( qSolis )"
    if "add_subdirectory( qSolis" not in text and "add_subdirectory(qSolis" not in text:
        if text and not text.endswith("\n"):
            text += "\n"
        text += f"{cmake_line}\n"
        parent_cmake.write_text(text, encoding="utf-8")
        print(f"updated {parent_cmake}")

    print(f"installed source at {target}")
    print("configure CloudCompare with -DPLUGIN_STANDARD_QSOLIS=ON")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
