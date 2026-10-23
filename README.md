# qSOLIS v1.1.0

qSOLIS is a CloudCompare plugin for solar visibility, irradiance, and irradiation
analysis on point clouds and triangular meshes.

Version 1.1.0 builds on the Qt 6 / current-CloudCompare port and adds quantitative canopy-exposure metrics. It includes GUI and command-line workflows, direct and diffuse solar
components, horizontal-reference and normal-aware surface irradiance, batch time
series, persistent result layers, and safe result cleanup.

## Compatibility

- CloudCompare: **2.14+ / current Qt 6 code line**
- Qt: **Qt 6**
- C++: follows CloudCompare (currently C++17)
- Platforms: source is designed for Linux, Windows, and macOS; v1.0.0 was
  acceptance-tested on a Qt 6 Linux build.
- CloudCompare 2.13.x / Qt 5 is intentionally not targeted by this release.

## Canopy Exposure (v1.1.0)

The **SOLIS Exposure...** action and `-SOLIS_EXPOSURE` CLI command add three per-point/vertex fractions plus an entity-level crown summary:

- Sky Exposure Factor: geometric upper-hemisphere openness.
- Sun Exposure Fraction: fraction of sampled daylight time with sun access.
- Radiative Exposure Index: received clear-sky direct+diffuse radiation divided by unobstructed horizontal potential.
- Crown Exposure Index: density-robust crown-wide mean REI using occupied-voxel weighting for point clouds or surface-area weighting for meshes.

Exposure separates **targets** from **occluding context**. Results are always written only to selected target entities, while the visibility renderer can use: target-only geometry, all selected targets (mutual occlusion), or every visible point cloud/mesh in the DB tree. This lets one selected tree be evaluated against neighboring visible trees/terrain without merging the scene.

After calculating Crown Exposure, select one or more tree entities and use **SOLIS Export Crown Exposure Summary...** to write a CSV with the latest CEI summary for each selected tree. The CLI equivalent is `-SOLIS_EXPORT_CROWN <file.csv> [LATEST|ALL_RUNS]`.

See `docs/CANOPY_EXPOSURE.md`.

## Main features

- Direct, Diffuse, and Total solar results.
- Instantaneous irradiance (`W/m2`), mean irradiance (`W/m2`), and integrated
  irradiation (`Wh/m2` or `kWh/m2`).
- Horizontal-reference, true surface (normal-aware), and visibility modes.
- UTC or fixed-offset local time input.
- Clear-sky irradiance using the current qSOLIS solar/atmospheric model and
  configurable Linke turbidity.
- Deterministic diffuse hemisphere sampling.
- Persistent offscreen OpenGL framebuffer rendering with reusable VBO/EBO
  geometry buffers.
- Parallel CPU point projection/visibility evaluation.
- Optional grouping of nearby direct-sun directions for long integrations.
- Batch time-series generation with one scalar field per stored time point.
- Metadata-backed result provenance and safe cleanup by run/batch.
- GUI and CLI use the same numerical engine and result layer.

## Quick source integration

From the extracted qSOLIS release directory:

```bash
python3 tools/install_into_cloudcompare.py ~/dev/CloudCompare --symlink
```

Then configure and build CloudCompare:

```bash
cd ~/dev/CloudCompare
cmake -S . -B build-qsolis \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DOPTION_BUILD_CCVIEWER=OFF \
  -DPLUGIN_STANDARD_QSOLIS=ON
cmake --build build-qsolis --parallel
```

For a clean install tree:

```bash
cmake -S . -B build-qsolis-release \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$HOME/CloudCompare-qSOLIS" \
  -DOPTION_BUILD_CCVIEWER=OFF \
  -DPLUGIN_STANDARD_QSOLIS=ON
cmake --build build-qsolis-release --parallel
cmake --install build-qsolis-release
```

See `BUILDING.md` for details.

## GUI

Select one or more point clouds or meshes, then use:

```text
Plugins > SOLIS Calculate...
```

The dialog exposes location/time, quantity, spatial mode, Direct/Diffuse
components, batch time series, quality presets, and advanced atmosphere/render
controls. Surface mode is available only when every selected point/vertex cloud
already has normals.

The lower-right corner of the dialog displays the plugin version.

Canopy-exposure calculations and tree-level export are available from the same plugin:

```text
Plugins > SOLIS Exposure...
Plugins > SOLIS Export Crown Exposure Summary...
```

The export action writes the latest stored Crown Exposure summary for each selected tree/entity as one CSV row.

Cleanup is part of the same plugin:

```text
Plugins > SOLIS Cleanup...
```

Cleanup can remove the last run, last batch, a selected batch, all
metadata-backed qSOLIS fields, or explicitly selected legacy SOLIS-named fields.

## CLI quick example

```bash
CloudCompare \
  -SILENT \
  -AUTO_SAVE OFF \
  -O tree.ply \
  -SOLIS \
  -COMPONENT ALL \
  -OUTPUT_MODE ENERGY \
  -ENERGY_UNIT WH \
  -SPATIAL_MODE HORIZONTAL \
  -DOY 172 -TIME 00:00 \
  -DURATION 24 -TIMESTEP 5 \
  -LAT 45 -LON 0 -ELV 0 \
  -DIFFUSE_RAYS 256 \
  -RESOLUTION 1024
```

Exact CLI options and examples are in `docs/CLI.md`.

## Result scalar fields

qSOLIS never silently overwrites a previous result. Each result is stored as a
new CloudCompare scalar field on the original point/vertex cloud, for example:

```text
SOLIS Direct [W/m2] D175 T1200 UTC H R1024
SOLIS Diffuse [W/m2] D175 T1200 UTC H R1024
SOLIS Total [Wh/m2] D175 T0000 UTC dur24h dt5m H R1024
```

Repeated identical names receive `#2`, `#3`, etc. Full calculation provenance,
including qSOLIS version, is stored in CloudCompare object metadata. For exact
machine-parsing rules, see `docs/CLI_LAYER_NAMING.md`.

## Testing

A frozen v1.0.0 end-to-end regression suite is shipped under:

```text
qSolis/tests/regression/
```

Run it against an installed CloudCompare build:

```bash
cd qSolis/tests/regression
./run_qsolis_regression.sh /path/to/CloudCompare
```

The accepted v1.0.0 baseline verifies surface-normal incidence, missing-normal
rejection, and a synthetic wall-shadow/FBO case. See `docs/TESTING.md`.

## Documentation

- `docs/USER_MANUAL.md` - user manual
- `docs/CLI.md` - command-line reference
- `docs/CLI_LAYER_NAMING.md` - exact result layer naming grammar
- `docs/TESTING.md` - regression and acceptance testing
- `docs/CLEANUP.md` - result cleanup behavior
- `docs/PORTING_NOTES.md` - implementation/port notes
- `docs/DISTRIBUTION.md` - guidance for binary releases

## Scientific scope and limitations

qSOLIS v1.1.0 is a clear-sky geometric/irradiance model. It does not ingest
measured cloud cover or weather time series. The diffuse model is isotropic.
Surface irradiance depends on the quality and orientation of input normals.
Results are also resolution-dependent near shadow boundaries because visibility
is evaluated through a finite-resolution depth map.

The renderer requires a usable Qt/OpenGL platform even in CLI mode.

## License

qSOLIS is distributed under the GNU General Public License, version 2 or later.
See `LICENSE`.

## Automated release builds

GitHub Actions can build and package qSOLIS for Linux, Windows, and macOS when a `v*.*.*` tag is pushed. See [`docs/GITHUB_ACTIONS_RELEASES.md`](docs/GITHUB_ACTIONS_RELEASES.md).
