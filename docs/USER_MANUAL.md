# qSOLIS v1.0.0 User Manual

Solar visibility, irradiance, and irradiation analysis for CloudCompare

**Target platform:** CloudCompare 2.14+ / Qt 6

## 1. Overview

qSOLIS evaluates solar exposure on CloudCompare point clouds and triangular
meshes. It combines a solar-position/clear-sky irradiance model with an
offscreen OpenGL visibility pass. Results are written as CloudCompare scalar
fields on the original point or vertex cloud.

qSOLIS can calculate:

- Direct solar visibility or irradiance.
- Diffuse sky visibility or irradiance.
- Total irradiance/irradiation as Direct + Diffuse.
- Instantaneous irradiance in W/m2.
- Mean irradiance over an interval in W/m2.
- Integrated irradiation in Wh/m2 or kWh/m2.
- Horizontal-reference values.
- True surface values using existing normals.
- Visibility fractions.

The same numerical engine is used by the GUI and CLI.

## 2. Coordinate and time conventions

qSOLIS uses the following scene axes:

- +X = east
- +Y = north
- +Z = up

Longitude is east-positive. Latitude is positive north and negative south.

Day-of-year (DOY) runs from 1 through 366. DOY 0 is invalid. A clock time is
stored internally as the fractional part of the day-of-year.

Time can be entered as UTC or as local civil time with a fixed UTC offset. The
local-time mode is a fixed offset only; it does not apply named timezone or DST
rules automatically.

## 3. Starting a calculation

1. Load a point cloud or mesh.
2. Select the point cloud itself or the mesh in the DB tree.
3. Choose **Plugins > SOLIS Calculate...**.
4. Configure the calculation.
5. Press **OK**.

The dialog displays **qSOLIS v1.0.0** at the bottom.

### 3.1 Location and time

Set latitude, longitude, elevation, DOY, time, and time reference. For UTC,
leave the UTC offset disabled. For local fixed-offset time, choose Local and
enter the offset from UTC.

### 3.2 Output quantity

**Instantaneous irradiance** evaluates one time point and writes W/m2.

**Mean irradiance over interval** averages the sampled interval and writes W/m2.

**Integrated irradiation / energy** integrates over the configured duration and
writes Wh/m2 or kWh/m2.

For Mean and Energy modes, Duration and Timestep become active.

### 3.3 Spatial mode

**Horizontal-reference irradiance** treats each visible point as a horizontal
receiver. This is useful for exposure/shadow studies where local surface
orientation should not change the irradiance magnitude.

**True surface irradiance** applies the incidence term from each point/vertex
normal. Every selected point cloud or mesh vertex cloud must already contain
normals. If any selected entity lacks normals, Surface mode is disabled.

**Visibility fraction** ignores irradiance magnitude and returns visibility
fractions. Total is undefined for Visibility; Direct + Diffuse produces two
separate visibility fields.

### 3.4 Components

Select Direct, Diffuse, or both. When Direct and Diffuse are selected in an
irradiance mode, qSOLIS also creates Total = Direct + Diffuse point-by-point.

### 3.5 Quality presets

- Fast: 512 px render resolution, 64 diffuse rays.
- Normal: 1024 px, 256 diffuse rays.
- High: 2048 px, 512 diffuse rays.
- Custom: manual resolution/ray count.

Higher resolution improves shadow-boundary detail but increases GPU memory,
framebuffer readback, and CPU projection work. The Normal preset is the
recommended starting point.

## 4. Advanced options

Expand **Advanced options** for controls that are normally left at defaults.

### Atmosphere

**Linke turbidity** controls clear-sky atmospheric attenuation. A constant value
is used for the calculation interval. qSOLIS does not ingest measured cloud or
weather time series in v1.0.0.

### Rendering and CPU

**Resolution** controls the square visibility framebuffer. Values are limited by
the GPU/OpenGL implementation.

**Depth bias** is the depth-comparison tolerance used by the visibility test.
Keep the default unless you are diagnosing systematic self-shadowing or leakage.

**CPU threads** controls parallel point projection. Auto lets qSOLIS choose from
hardware concurrency; 1 forces single-threaded projection.

**Closed mesh optimization** should only be enabled for meshes known to be
closed/watertight.

### Sampling

**Diffuse rays** controls the deterministic hemisphere sample count.

**Group nearby direct-sun directions** can accelerate long integrations by
merging adjacent solar directions within an angular tolerance. This is an
approximation and should be validated against an ungrouped reference for
accuracy-critical workflows.

## 5. Batch time series

Enable **Batch time series** to store multiple calculation start times as
separate scalar fields without duplicating geometry.

Set an end DOY/time and step in minutes. For example, 08:00 through 18:00 with a
60-minute step creates hourly result layers.

For Mean or Energy modes, each batch timestamp is the start of its own interval.
A two-hour calculation at 08:00, 09:00, and 10:00 therefore creates three
separate two-hour windows.

The GUI asks for confirmation for large batches and qSOLIS limits a batch to
1000 time points to prevent accidental creation of impractical numbers of scalar
fields.

## 6. Result layers and naming

Each persistent result is a new scalar field on the original point/vertex cloud.
Existing qSOLIS results are not silently overwritten.

Examples:

```text
SOLIS Direct [W/m2] D175 T1200 UTC H R1024
SOLIS Diffuse [W/m2] D175 T1200 UTC H R1024
SOLIS Total [Wh/m2] D175 T0000 UTC dur24h dt5m H R1024
SOLIS Direct Mean [W/m2] D175 T1200 UTC dur2h dt5m S R2048
```

Tokens include component, unit, day, clock, time convention, optional duration
and timestep, spatial mode, and resolution. Duplicate names receive `#2`, `#3`,
and so on.

Full calculation provenance is stored as qSOLIS JSON metadata on the point
cloud. The metadata includes qSOLIS version, result/run/batch identifiers,
location, time convention, atmospheric settings, render settings, sampling
settings, and creation time.

For machine parsing, see `CLI_LAYER_NAMING.md`.

## 7. Cleanup

Select one or more clouds/meshes and choose **Plugins > SOLIS Cleanup...**.

Available cleanup operations include:

- Last run.
- Last batch.
- Selected batch.
- All metadata-backed qSOLIS results.
- Explicit legacy SOLIS-name cleanup.

Normal cleanup is metadata-driven and does not remove unrelated scalar fields.
Legacy cleanup is deliberately separate because it relies on field-name matching.

Deletion should be considered permanent once the modified project is saved.

## 8. Command-line use

qSOLIS registers `-SOLIS` and `-SOLIS_CLEAN`.

Example: one direct instantaneous horizontal calculation:

```bash
CloudCompare -SILENT -AUTO_SAVE OFF -O tree.ply -SOLIS \
  -COMPONENT DIRECT \
  -OUTPUT_MODE INSTANT \
  -SPATIAL_MODE HORIZONTAL \
  -DOY 172 -TIME 12:00 \
  -LAT 45 -LON 0 -ELV 0 \
  -RESOLUTION 1024
```

Example: daily Direct + Diffuse + Total energy:

```bash
CloudCompare -SILENT -AUTO_SAVE OFF -O tree.ply -SOLIS \
  -COMPONENT ALL \
  -OUTPUT_MODE ENERGY -ENERGY_UNIT WH \
  -SPATIAL_MODE HORIZONTAL \
  -DOY 172 -TIME 00:00 \
  -DURATION 24 -TIMESTEP 5 \
  -LAT 45 -LON 0 -ELV 0 \
  -LINKE_TURBIDITY 3 \
  -DIFFUSE_RAYS 256 \
  -RESOLUTION 1024
```

Every created field is printed to the CLI log. Capture these names in automation
rather than guessing duplicate suffixes. See `CLI.md` and `CLI_LAYER_NAMING.md`
for the complete grammar and option reference.

## 9. Interpretation of results

### Direct

Direct irradiance includes clear-sky direct normal irradiance, visibility, and
an incidence term for Horizontal or Surface mode. Occluded points receive zero
direct contribution for that sample.

### Diffuse

Diffuse irradiance uses an isotropic sky-radiance assumption and deterministic
hemisphere visibility sampling. Surface mode applies normal incidence to each
sky direction.

### Total

Total is Direct + Diffuse for irradiance/energy modes. The maximum Total value
need not equal max(Direct) + max(Diffuse), because maxima can occur at different
points.

### Visibility

Direct visibility over an interval is the fraction of sampled daylight time for
which the point/vertex is visible to the sun. Diffuse visibility is the fraction
of sampled sky solid angle visible from the point/vertex.

## 10. Performance guidance

The dominant cost is repeated visibility evaluation over the entity. For large
clouds or long integrations:

1. Start at Normal quality (1024 / 256 rays).
2. Validate whether 2048 materially changes results before adopting High.
3. Use a coarser temporal timestep when scientifically acceptable.
4. Consider direct angular grouping for long integrations after comparing it to
   an ungrouped reference.
5. Keep CPU threads on Auto unless benchmarking shows a reason to override.

qSOLIS reuses uploaded geometry and the rendering backend across Direct/Diffuse
and batch points, avoiding repeated geometry setup.

## 11. Regression tests

The release contains `qSolis/tests/regression` with three acceptance cases:

1. Surface irradiance with known normals.
2. Surface mode without normals (must be rejected).
3. A 5 m wall casting a deterministic noon shadow.

Run:

```bash
cd qSolis/tests/regression
./run_qsolis_regression.sh /path/to/CloudCompare
```

The v1.0.0 accepted wall test has a sunlit horizontal median of about
834.6073 W/m2 and an expected geometric shadow length of about 1.9746 m.

## 12. Troubleshooting

### SOLIS action is disabled

Select the actual point cloud or mesh entity, not only its parent container.

### Surface mode is disabled

Compute/import normals for every selected point cloud or mesh vertex cloud.

### Result is all zero

Confirm the sun is above the horizon for the selected location/time. If the same
case works at one render resolution but not another, verify that you are using a
v1.0.0 build with the explicit FBO renderer and check OpenGL limits/errors.

### CLI fails on a headless server

`-SILENT` removes dialogs but does not remove the OpenGL requirement. Use an
offscreen-capable Qt/OpenGL platform or a virtual display.

### Very large number of result fields

Use **SOLIS Cleanup... > Last batch** to remove an accidental batch without
removing unrelated scalar fields.

## 13. Scientific limitations

qSOLIS v1.0.0 is a clear-sky model, not a complete weather simulator. Major
limitations include:

- No measured cloud cover or meteorological time-series input.
- Isotropic diffuse sky model.
- Constant Linke turbidity over one calculation.
- Surface results depend on normal quality/orientation.
- Shadow boundaries depend on finite render resolution and depth bias.
- Day-of-year solar approximations do not carry an explicit calendar year.

These assumptions should be documented when qSOLIS results are used in research
or engineering reports.

## 14. Version and license

This manual describes **qSOLIS v1.0.0**, released 2026-10-15.

qSOLIS is distributed under the GNU General Public License, version 2 or later.
See the release `LICENSE` file.
