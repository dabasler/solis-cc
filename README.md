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

qSOLIS release archives contain the **qSOLIS plugin binary only**. They do **not** contain a complete CloudCompare installation.

Install **CloudCompare v2.14.beta2** for your platform first, then copy the qSOLIS plugin into that CloudCompare installation.

> **Important:** Use the package matching both your operating system and architecture. qSOLIS v1.1.0 release binaries are built and tested against **CloudCompare v2.14.beta2**.

### Windows x64

Download:

```text
qSOLIS-1.1.0-CloudCompare-v2.14.beta2-windows-x64-msvc.zip
```

The archive contains:

```text
CloudCompare/plugins/QSOLIS_PLUGIN.dll
```

Installation:

1. Install or extract the **64-bit CloudCompare v2.14.beta2** Windows build.
2. Close CloudCompare if it is running.
3. Extract the qSOLIS ZIP.
4. Copy:

   ```text
   CloudCompare\plugins\QSOLIS_PLUGIN.dll
   ```

   from the qSOLIS archive into the `plugins` directory of your CloudCompare installation:

   ```text
   <CloudCompare installation>\plugins\QSOLIS_PLUGIN.dll
   ```

5. Start CloudCompare.
6. Confirm that **SOLIS** appears in the plugin menu.

If Windows blocks the downloaded ZIP or DLL, verify that it came from the official qSOLIS GitHub Release and, if desired, verify the published SHA-256 checksum before unblocking it.

### macOS Apple Silicon (arm64)

Download:

```text
qSOLIS-1.1.0-CloudCompare-v2.14.beta2-macos-arm64-clang-unsigned.zip
```

The archive contains:

```text
CloudCompare/CloudCompare.app/Contents/PlugIns/ccPlugins/libQSOLIS_PLUGIN.dylib
```

Installation:

1. Install **CloudCompare v2.14.beta2 for Apple Silicon (arm64)**.
2. Quit CloudCompare.
3. Extract the qSOLIS ZIP.
4. In Finder, locate your installed `CloudCompare.app`.
5. Control-click `CloudCompare.app` and choose **Show Package Contents**.
6. Open:

   ```text
   Contents/PlugIns/ccPlugins/
   ```

7. Copy:

   ```text
   libQSOLIS_PLUGIN.dylib
   ```

   from the qSOLIS archive into that `ccPlugins` directory.
8. Start CloudCompare.
9. Confirm that **SOLIS** appears in the plugin menu.

The qSOLIS macOS plugin is currently **unsigned**. macOS may display a security warning after the plugin is added. Only allow the application/plugin after verifying that the archive came from the official qSOLIS GitHub Release.

The v1.1.0 macOS package is **arm64 only**; it is not an Intel/x86_64 build.

### Linux x86_64

Download:

```text
qSOLIS-1.1.0-CloudCompare-v2.14.beta2-linux-x86_64-ubuntu24.04-gcc.zip
```

The archive contains:

```text
lib/cloudcompare/plugins/libQSOLIS_PLUGIN.so
```

Installation:

1. Install **CloudCompare v2.14.beta2**.
2. Close CloudCompare if it is running.
3. Extract the qSOLIS ZIP.
4. Copy:

   ```text
   lib/cloudcompare/plugins/libQSOLIS_PLUGIN.so
   ```

   into the matching plugin directory of your CloudCompare installation:

   ```text
   <CloudCompare installation prefix>/lib/cloudcompare/plugins/
   ```

5. Start CloudCompare.
6. Confirm that **SOLIS** appears in the plugin menu.

The Linux release is built and tested on **Ubuntu 24.04 x86_64**. Other Linux distributions may require building qSOLIS from source because of compiler, Qt, or system-library compatibility.

### Verify the download

Each release ZIP is accompanied by a `.sha256` file.

On Linux or macOS:

```bash
sha256sum -c <package>.sha256
```

On Windows PowerShell, calculate the hash with:

```powershell
Get-FileHash <package>.zip -Algorithm SHA256
```

and compare it with the published `.sha256` value.

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
