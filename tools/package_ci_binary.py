#!/usr/bin/env python3
"""Create a binary-only qSOLIS release package from a CloudCompare install tree."""
from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import tempfile
import zipfile
from pathlib import Path


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser()
    p.add_argument("--stage", type=Path, required=True, help="CloudCompare CMAKE_INSTALL_PREFIX")
    p.add_argument("--platform", required=True)
    p.add_argument("--cloudcompare-ref", required=True)
    p.add_argument("--cloudcompare-commit", required=True)
    p.add_argument("--qsolis-commit", required=True)
    p.add_argument("--output", type=Path, required=True)
    return p.parse_args()


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def safe_ref(value: str) -> str:
    return value.replace("/", "-").replace("\\", "-").replace(" ", "-")


def main() -> int:
    args = parse_args()
    repo = Path(__file__).resolve().parents[1]
    version = (repo / "VERSION").read_text(encoding="utf-8").strip()
    stage = args.stage.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)

    if not stage.is_dir():
        raise SystemExit(f"staging directory does not exist: {stage}")

    # CloudCompare's installed filename differs by platform. Keep this search
    # deliberately narrow so a stale development file can't be packaged.
    binaries = [
        p for p in stage.rglob("*")
        if p.is_file()
        and "QSOLIS_PLUGIN" in p.name.upper()
        and p.suffix.lower() in {".dll", ".so", ".dylib", ".bundle"}
    ]
    if not binaries:
        # Some CMake/module layouts use extension-less bundles or versioned .so names.
        binaries = [
            p for p in stage.rglob("*")
            if p.is_file() and "QSOLIS_PLUGIN" in p.name.upper()
        ]

    if not binaries:
        listing = "\n".join(str(p.relative_to(stage)) for p in stage.rglob("*") if p.is_file())
        raise SystemExit("qSOLIS plugin binary not found in install tree. Files were:\n" + listing)

    cc_ref_safe = safe_ref(args.cloudcompare_ref)
    package_name = f"qSOLIS-{version}-CloudCompare-{cc_ref_safe}-{args.platform}"

    with tempfile.TemporaryDirectory(prefix="qsolis-package-") as temp:
        root = Path(temp) / package_name
        root.mkdir(parents=True)

        copied = []
        for binary in binaries:
            rel = binary.relative_to(stage)
            target = root / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(binary, target)
            copied.append(rel)

        shutil.copy2(repo / "LICENSE", root / "LICENSE")

        build_info = f"""qSOLIS binary package
=====================

qSOLIS version:       {version}
qSOLIS commit:        {args.qsolis_commit}
CloudCompare ref:     {args.cloudcompare_ref}
CloudCompare commit:  {args.cloudcompare_commit}
Platform build:       {args.platform}
GitHub runner OS:     {os.environ.get('RUNNER_OS', 'unknown')}
GitHub runner arch:   {os.environ.get('RUNNER_ARCH', 'unknown')}

Packaged plugin files (paths are relative to the CloudCompare install prefix):
"""
        build_info += "\n".join(f"  - {p.as_posix()}" for p in copied) + "\n"
        (root / "BUILD_INFO.txt").write_text(build_info, encoding="utf-8")

        install = f"""qSOLIS {version} binary installation
===================================

This package was built against CloudCompare {args.cloudcompare_ref}
(commit {args.cloudcompare_commit}).

IMPORTANT
---------
Use it only with a matching CloudCompare build (same CloudCompare tag/ref,
architecture, compiler/runtime family, and Qt major version). C++ plugin ABI
compatibility across unrelated CloudCompare builds is not guaranteed.

Installation
------------
The directory layout in this archive is relative to CloudCompare's install
prefix. Merge the packaged paths into the matching CloudCompare installation.
Do not rename the plugin binary.

macOS
-----
The automated macOS artifact is UNSIGNED. Inserting an unsigned third-party
plugin into a signed/notarized application bundle can invalidate the bundle's
signature or trigger Gatekeeper. Treat this package as a developer/beta artifact
unless you add Apple signing/notarization to the release pipeline.

Validation
----------
After installation, start CloudCompare and confirm that the Plugins menu lists
SOLIS. For scientific validation, use the frozen regression suite distributed
with the qSOLIS source release.
"""
        (root / "INSTALL.txt").write_text(install, encoding="utf-8")

        archive = output / f"{package_name}.zip"
        with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as zf:
            for p in sorted(root.rglob("*")):
                if p.is_file():
                    zf.write(p, arcname=p.relative_to(root.parent))

    digest = sha256(archive)
    checksum = output / f"{archive.name}.sha256"
    checksum.write_text(f"{digest}  {archive.name}\n", encoding="utf-8")

    print(f"Created {archive}")
    print(f"SHA-256 {digest}")
    for p in binaries:
        print(f"Packaged: {p.relative_to(stage)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
