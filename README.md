# qSOLIS v1.1.0

qSOLIS is a CloudCompare plugin for solar irradiance, visibility, and canopy-exposure analysis.

## Release compatibility

| Platform | Architecture | Release target |
|---|---|---|
| Windows | x86_64 / x64 | CloudCompare v2.14.beta2 |
| macOS | Apple Silicon / arm64 | CloudCompare v2.14.beta2 |
| Linux | x86_64 | Ubuntu 24.04, CloudCompare v2.14.beta2 |

> **Important:** qSOLIS release binaries are built and tested with **CloudCompare v2.14.beta2**. Keep the release bundle together. Do not copy only the qSOLIS plugin binary into a different CloudCompare version unless you rebuild qSOLIS against that version.

## Installation from a release

The normal installation path is the **precompiled release package**. Building from source is only needed for development or unsupported platforms.

1. Open the qSOLIS **GitHub Releases** page.
2. Download the archive for your operating system and architecture.
3. Extract the complete archive.
4. Keep the extracted directory structure intact.
5. Start the CloudCompare executable contained in that release bundle.
6. In CloudCompare, confirm that **SOLIS** is available in the plugin menu.

### Windows x64

1. Download the **Windows x64** release ZIP.
2. Extract it to a permanent folder, for example:

   ```text
   C:\Apps\qSOLIS-1.1.0\
   ```

3. Do **not** move only the qSOLIS DLL out of the extracted package.
4. Launch the bundled `CloudCompare.exe`.
5. Confirm that **SOLIS** appears in CloudCompare's plugin menu.
6. Optionally create a desktop or Start-menu shortcut to that executable.

If Windows displays a download/security warning, verify that you downloaded the archive from the official qSOLIS release and, if provided, verify its checksum before running it.

### macOS Apple Silicon

The v1.1.0 macOS build targets **Apple Silicon (arm64)**. It is not an Intel/x86_64 build.

1. Download the **macOS arm64** release archive.
2. Extract the complete archive.
3. Move the extracted application/bundle to a permanent location if desired.
4. Launch the bundled CloudCompare application.
5. Confirm that **SOLIS** appears in CloudCompare's plugin menu.

The macOS release is currently **unsigned**. On first launch, macOS may block it. If that happens:

1. In Finder, Control-click the CloudCompare application and choose **Open**.
2. If it is still blocked, open **System Settings → Privacy & Security**.
3. Use **Open Anyway** for the downloaded CloudCompare/qSOLIS application.
4. Only bypass the warning after confirming that the archive came from the official qSOLIS release.

### Linux x86_64

The Linux release is tested on **Ubuntu 24.04 x86_64**.

1. Download the Linux release archive.
2. Extract it, for example:

   ```bash
   mkdir -p qSOLIS-1.1.0
   unzip <linux-release>.zip -d qSOLIS-1.1.0
   cd qSOLIS-1.1.0
   ```

3. Start the bundled CloudCompare executable:

   ```bash
   ./bin/CloudCompare
   ```

4. Confirm that **SOLIS** appears in CloudCompare's plugin menu.

Other Linux distributions may work, but Ubuntu 24.04 is the release target used for CI and regression testing.

## Quick start

For a short walkthrough, see:

**[qSOLIS 5-minute tutorial](TUTORIAL.md)**

The tutorial covers:

- loading a cloud or mesh;
- selecting the analysis target;
- Surface vs Horizontal analysis;
- time/location settings;
- occlusion context;
- canopy-exposure metrics;
- interpreting SEF, SuEF, REI, and CEI results.

## qSOLIS v1.1 canopy exposure

qSOLIS v1.1 adds exposure metrics intended for canopy, crown, and obstruction analysis:

- **SEF — Sky Exposure Factor:** weighted geometric visibility of the upper hemisphere, from 0 to 1.
- **SuEF — Sun Exposure Fraction:** fraction of daylight time with line of sight to the sun, from 0 to 1.
- **REI — Radiative Exposure Index:** received direct + diffuse radiation relative to an unobstructed horizontal reference, from 0 to 1.
- **CEI — Crown Exposure Index:** entity-level spatially weighted summary of REI.

### Occlusion context

The GUI provides three context modes:

- **Target only** — only the target geometry can occlude itself.
- **Selected entities** — selected neighboring entities also block rays. This is the recommended/default multi-entity workflow.
- **All visible entities** — all visible geometry can participate as occlusion context.

Only the target receives result scalar fields. Context entities act as blockers.

## Surface-analysis notes

qSOLIS uses the coordinate convention:

```text
X = east
Y = north
Z = up
```

**Surface** irradiance requires valid normals on the target.

**Horizontal** irradiance and canopy-exposure metrics do not require target normals.

## Command-line regression suites

Run the installed release candidate rather than a build-tree CloudCompare executable.

### Frozen v1.0 solar regression

```bash
./run_qsolis_regression.sh /path/to/CloudCompare
```

The suite verifies:

1. Direct instantaneous **surface irradiance** with known normals.
2. Explicit rejection of Surface mode when normals are absent.
3. Direct instantaneous **horizontal irradiance** behind a 5 m wall, including the offscreen OpenGL/FBO visibility path.

The frozen expected values are in `expected_results.json`. If the scientific model is deliberately changed in a future release, update the baseline as an explicit model change; do not simply loosen tolerances.

The original v1.0.0 acceptance run produced:

```text
surface HORIZONTAL  : median=262.5244 W/m2  PASS
surface SOUTH       : median=665.6770 W/m2  PASS
surface NORTH       : median=0.0000 W/m2    PASS
surface TILT45_SOUTH: median=656.6156 W/m2  PASS
wall sunlit median  : 834.6073 W/m2         PASS
wall shadow median  : approximately 0 W/m2 PASS
expected shadow length: 1.9746 m
```

### v1.1 canopy-exposure regression

```bash
./run_qsolis_exposure_regression.sh /path/to/CloudCompare
```

This suite covers:

- open-reference Sky Exposure;
- open-reference Sun Exposure;
- open-reference Radiative Exposure and Crown Exposure;
- rejection of Sun Exposure intervals containing no daylight;
- wall direct-sun accessibility;
- wall sky openness;
- wall REI behavior;
- separate target/neighbor geometry with **Target only** context;
- separate target/neighbor geometry with **Loaded/selected** context.

For the multi-entity exposure tests, `context_target_ground.ply` and `context_neighbor_wall.ply` are loaded as separate entities. The regression verifies that target-only context leaves the ground target exposed while loaded context allows the separately loaded wall to occlude it.

## Building from source

Source builds are intended for developers, unsupported platforms, or users who need to build qSOLIS against a different CloudCompare revision.

When building from source, use the CloudCompare revision that matches your intended deployment and enable qSOLIS with:

```text
-DPLUGIN_STANDARD_QSOLIS=ON
```

For normal users, the precompiled release package is recommended.
