# Automated multi-platform releases

qSOLIS ships with `.github/workflows/release.yml`.

## What it does

For every tag matching `v*.*.*` the workflow:

1. verifies that the tag version matches the repository `VERSION` file;
2. creates a **draft** GitHub Release;
3. builds qSOLIS against a pinned CloudCompare reference on:
   - Ubuntu 24.04 / GCC / x86_64,
   - Windows / MSVC / x86_64,
   - macOS / Clang / arm64;
4. runs the standalone qSOLIS solar-model CTest on every platform;
5. runs both the frozen v1.0 OpenGL/FBO regression suite and the v1.1 canopy-exposure regression suite on the Linux runner under Xvfb;
6. installs CloudCompare into a private staging prefix;
7. extracts qSOLIS from that install tree while preserving its install-relative path;
8. creates a ZIP plus SHA-256 file for each platform;
9. uploads the packages to the draft release;
10. publishes the release only if **all platform jobs succeed**.

A manually dispatched workflow performs the same builds but does **not** create a GitHub Release. Its packages are available as normal Actions artifacts.

## Pinned CloudCompare version

The default is currently:

```text
v2.14.beta2
```

The workflow-dispatch form lets you test another CloudCompare tag, branch, or commit without editing the workflow. For a tagged qSOLIS release, edit `DEFAULT_CLOUDCOMPARE_REF` in the workflow before tagging if the supported CloudCompare baseline changes.

## Expected assets

For qSOLIS `1.1.0` built against CloudCompare `v2.14.beta2`, the draft release receives files similar to:

```text
qSOLIS-1.1.0-CloudCompare-v2.14.beta2-linux-x86_64-ubuntu24.04-gcc.zip
qSOLIS-1.1.0-CloudCompare-v2.14.beta2-linux-x86_64-ubuntu24.04-gcc.zip.sha256

qSOLIS-1.1.0-CloudCompare-v2.14.beta2-windows-x64-msvc.zip
qSOLIS-1.1.0-CloudCompare-v2.14.beta2-windows-x64-msvc.zip.sha256

qSOLIS-1.1.0-CloudCompare-v2.14.beta2-macos-arm64-clang-unsigned.zip
qSOLIS-1.1.0-CloudCompare-v2.14.beta2-macos-arm64-clang-unsigned.zip.sha256

# plus qSOLIS_User_Manual_v1.1.0.pdf when that optional release asset is present
```

GitHub also provides its normal source-code ZIP/tar.gz automatically for the tag.

## Creating a release

After committing the workflow and confirming a manual build works:

```bash
git tag -a v1.1.0 -m "qSOLIS 1.1.0"
git push origin v1.1.0
```

The GitHub Release stays in draft state until every build job has completed successfully.

## Why the Linux runner runs the full regression suite

Both end-to-end regression suites use the OpenGL framebuffer. Linux CI can run them consistently through Xvfb/software Mesa. Hosted Windows/macOS graphics availability can vary, so those jobs gate the release on compilation, installation, plugin packaging, and the non-OpenGL CTest. A physical-machine smoke test is still recommended before claiming full Windows/macOS runtime support.

## macOS signing

The generated macOS artifact is explicitly named `unsigned`. Apple code signing and notarization require Developer ID credentials and are intentionally not performed by the default public workflow. Do not store Apple certificates or passwords directly in the YAML; add them as GitHub repository/environment secrets when you introduce signing.

## Package layout

The packaging helper searches the CloudCompare install tree for the installed qSOLIS plugin and preserves its path relative to `CMAKE_INSTALL_PREFIX`. This avoids hard-coding platform-specific plugin destinations.

Each ZIP contains:

- the qSOLIS plugin binary in its install-relative location;
- `INSTALL.txt`;
- `BUILD_INFO.txt` containing qSOLIS and CloudCompare commit IDs;
- `LICENSE`.

Binary users should use the package only with a matching CloudCompare build. C++ plugin ABI compatibility with arbitrary CloudCompare binaries is not guaranteed.
