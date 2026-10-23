# Distributing qSOLIS Binaries

qSOLIS is a C++ CloudCompare plugin, so binary compatibility matters. A plugin
built for one CloudCompare/Qt/compiler combination should not be presented as a
universal binary for arbitrary CloudCompare installations.


## CloudCompare release status at qSOLIS 1.1.0

At the qSOLIS 1.1.0 release date (2026-10-15), CloudCompare's latest stable
release is still 2.13.2 (Qt 5), while the 2.14 line is available as Qt 6
pre-releases (including v2.14.beta2) and current master. qSOLIS 1.1.0 targets
the Qt 6 / 2.14 line, so do **not** publish its binaries as compatible with the
2.13.2 stable binary. Build against an exact 2.14 tag/commit and name the asset
accordingly.

## Recommended release matrix

Build and publish separate artifacts identified by:

```text
qSOLIS-1.1.0-CloudCompare-<CC_VERSION>-<OS>-<ARCH>-<TOOLCHAIN>.zip
```

Examples:

```text
qSOLIS-1.1.0-CloudCompare-2.14.beta2-Windows-x64-MSVC.zip
qSOLIS-1.1.0-CloudCompare-2.14.beta2-macOS-arm64-Clang.zip
qSOLIS-1.1.0-CloudCompare-2.14.beta2-macOS-x86_64-Clang.zip
```

Use the exact CloudCompare release/tag (or clearly identified commit) and the same
Qt major/toolchain expected by that CloudCompare binary.

## Windows

The safest workflow is to build qSOLIS as part of the exact CloudCompare source
version you want to support and run `cmake --install`. Package the installed
qSOLIS plugin DLL from the resulting CloudCompare installation layout rather than
copying an arbitrary build-tree DLL.

Because qSOLIS itself adds no third-party runtime library beyond what the
CloudCompare/Qt installation already uses, a plugin-only package will often be
small. Still test it by copying the package into a clean matching CloudCompare
installation on a machine without your development environment.

Recommended release notes should state:

- CloudCompare version/tag/commit;
- Qt version;
- MSVC toolset/runtime and architecture;
- exact destination inside the CloudCompare installation;
- SHA-256 checksum.

If you want a low-friction user experience, a ZIP that mirrors the CloudCompare
installation tree is preferable to asking users to locate an internal DLL by
name.

## macOS

macOS plugin binaries are especially sensitive to architecture, deployment
minimum, rpaths, code signing, and the CloudCompare application bundle. Build
against the exact CloudCompare code line and use CloudCompare's installation and
bundling workflow. The current CloudCompare build system provides a `pixi`
workflow and a bundling step for relocatable `.app` output.

For public distribution, consider two approaches:

1. **Full CloudCompare.app bundle including qSOLIS** - easiest for nontechnical
   users, but you are effectively redistributing CloudCompare and must respect
   its licenses and signing/notarization requirements.
2. **Plugin-only binary** - smaller, but must be documented for a very specific
   CloudCompare version/architecture. On Apple Silicon, publish an arm64 build;
   publish x86_64 separately unless you deliberately build/test a universal
   binary.

For Gatekeeper-friendly public downloads, code signing/notarization should be
handled as part of the final application/plugin distribution process.

## Linux

Linux binary portability is affected by distribution/glibc and library versions.
Source releases are the most robust Linux distribution method. If publishing
binaries, label the distro/base image used and package from a clean installed
CloudCompare tree.

## Continuous integration

Once the source is in GitHub, a sensible next step is a release workflow that:

1. checks out the exact CloudCompare tag/commit;
2. integrates qSOLIS under `plugins/core/Standard/qSolis`;
3. enables `PLUGIN_STANDARD_QSOLIS`;
4. builds and installs Release configuration;
5. runs the qSOLIS regression suite;
6. extracts the platform plugin artifact from the install tree;
7. calculates SHA-256 checksums;
8. uploads artifacts to the matching GitHub Release.

Do not call a platform binary "2.14 compatible" until it has been tested against
the exact binary line you intend users to run.

## Licenses

The qSOLIS source release includes GPLv2-or-later licensing inherited from the
original plugin. CloudCompare is also GPL-licensed. When distributing a bundled
CloudCompare build, make corresponding source and license information available
as required by the applicable licenses.
