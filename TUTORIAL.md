# qSOLIS 5-minute tutorial

This tutorial assumes you installed the precompiled qSOLIS v1.1.0 release and can see **SOLIS** in CloudCompare.

qSOLIS uses:

```text
X = east
Y = north
Z = up
```

## 1. Load your geometry

Open a point cloud or mesh in CloudCompare.

For a simple first test, use one target entity. For a canopy or obstruction study, you can load several entities, such as:

- a crown or canopy target;
- neighboring crowns;
- buildings;
- terrain;
- walls or other obstructions.

## 2. Select the target

Select the entity that should receive the qSOLIS result scalar fields.

For multi-entity work, keep the target selected together with any neighboring entities you want to use as blockers.

## 3. Open SOLIS

Open the **SOLIS** plugin from CloudCompare.

Enter the site and time information appropriate for your analysis:

- latitude;
- longitude;
- elevation;
- date or day-of-year;
- time;
- UTC/fixed time offset;
- duration and time step when running an interval.

## 4. Choose the analysis type

### Surface irradiance

Use **Surface** mode when you want irradiance on the actual orientation of the geometry.

The target must have valid normals.

Typical outputs include direct, diffuse, or total irradiance and can be evaluated as instantaneous, mean, or accumulated energy values.

### Horizontal irradiance

Use **Horizontal** mode when you want irradiance referenced to a horizontal receiver rather than the target normal.

This mode does not require target normals.

## 5. Choose the occlusion context

For obstruction-aware calculations, choose how qSOLIS should treat neighboring geometry:

- **Target only** — the target can only occlude itself.
- **Selected entities** — the selected target and neighboring selected geometry are used for visibility/occlusion. This is the recommended multi-entity choice.
- **All visible entities** — all visible geometry can participate as blockers.

Only the target receives output scalar fields. Context entities are used as occluders.

## 6. Run a canopy-exposure analysis

For crown/canopy work, calculate one or more of the v1.1 exposure metrics.

### SEF — Sky Exposure Factor

Range:

```text
0 … 1
```

Interpretation:

- `1` = upper hemisphere is essentially unobstructed;
- `0` = upper hemisphere is fully obstructed;
- intermediate values = partial sky openness.

SEF is geometric and does not require target normals.

### SuEF — Sun Exposure Fraction

Range:

```text
0 … 1
```

Interpretation:

- `1` = line of sight to the sun for the full daylight interval;
- `0` = sun is blocked for the full daylight interval;
- intermediate values = sun is visible for only part of the daylight period.

SuEF is weighted by daylight time, not by DNI.

### REI — Radiative Exposure Index

Range:

```text
0 … 1
```

REI compares received direct + diffuse radiation with an unobstructed horizontal reference receiver.

Interpretation:

- `1` = radiatively equivalent to an open reference;
- values below `1` = radiation has been reduced by occlusion.

### CEI — Crown Exposure Index

CEI is an entity-level spatially weighted summary of REI.

It is useful when you want one crown-level exposure number in addition to the per-point/per-vertex REI scalar field.

CEI here is a qSOLIS radiative exposure summary; it is **not** the Dawkins ordinal crown-illumination class.

## 7. Inspect the results

After a successful run, qSOLIS adds result scalar fields to the target.

Use CloudCompare's scalar-field display tools to:

- select the qSOLIS field;
- inspect its minimum/maximum range;
- adjust the color scale;
- query individual points;
- compare exposed and shaded regions.

For a canopy study, a useful combination is:

- **SEF** for geometric sky openness;
- **SuEF** for direct-sun availability through time;
- **REI** for total radiative exposure;
- **CEI** for a crown-level summary.

## 8. Export a crown summary

Use:

**SOLIS → Export Crown Exposure Summary…**

to write the persisted crown-exposure metadata to CSV.

The command-line equivalent is:

```bash
CloudCompare \
  -O your_result_file \
  -SOLIS_EXPORT_CROWN crown_exposure.csv LATEST
```

Use `ALL_RUNS` instead of `LATEST` when you want all persisted exposure runs.

## Practical tips

- Use **Surface** mode only when target normals are valid.
- Use **Selected entities** for most canopy/crown analyses involving neighboring geometry.
- Use a mesh for solid walls/buildings when available; point-cloud occlusion can depend on point density and rendering resolution.
- Keep the coordinate convention X=east, Y=north, Z=up.
- For release validation and automated testing, run the installed CloudCompare executable, not a build-tree executable.
