# qSOLIS v1.0.0 Command-Line Reference

qSOLIS registers two CloudCompare commands:

```text
-SOLIS
-SOLIS_CLEAN
```

GUI and CLI use the same calculation engine and result storage.

## `-SOLIS`

Options are parsed after `-SOLIS` until the next CloudCompare command.

```text
-COMPONENT DIRECT|DIFFUSE|TOTAL|ALL
    Legacy alias: -TYPE

-DOY <1..<367>
    Day of year. Fractional values encode clock time unless -TIME is supplied.

-TIME <HH:MM[:SS]>
    Overrides the fractional clock portion of -DOY.

-DURATION <hours>
    Calculation interval for MEAN or ENERGY.
    Legacy alias: -INT. A legacy -INT value below zero selects INSTANT.

-TIMESTEP <minutes>
    Temporal integration step. Legacy alias: -TS.

-LAT <-90..90>
-LON <-180..180>
    Longitude is east-positive.
-ELV <metres>

-OUTPUT_MODE INSTANT|MEAN|ENERGY
-SPATIAL_MODE VISIBILITY|HORIZONTAL|SURFACE
-ENERGY_UNIT WH|KWH

-TIME_MODE UTC|LOCAL
-UTC_OFFSET <-14..14>
    Used only with LOCAL; fixed offset, not a named timezone database.

-LINKE_TURBIDITY <1..12>

-DIFFUSE_RAYS <count>
    Legacy alias: -NRAYS.

-RESOLUTION <64..16384>
-DEPTH_BIAS <0..0.05>
-THREADS <0..1024>
    0 = auto, 1 = single-threaded projection, N = requested worker count.

-IS_CLOSED
    Enable closed-mesh rendering optimization for meshes known to be watertight.

-ACCEL_ANGLE <degrees>
    Enable direct-sun direction grouping using the supplied tolerance.
-NO_ACCEL

-BATCH_END_DOY <1..366>
-BATCH_END_TIME <HH:MM[:SS]>
-BATCH_STEP <minutes>
    Supplying a batch end enables time-series generation. End points are stored
    only when they fall on the step grid.
```

### Surface mode

`-SPATIAL_MODE SURFACE` requires existing point/vertex normals. qSOLIS rejects
the operation if normals are missing.

### Visibility mode

Visibility outputs are fractions. `TOTAL` is undefined for visibility;
`-COMPONENT ALL` produces Direct and Diffuse visibility fields only.

## Examples

### Direct instantaneous horizontal irradiance

```bash
CloudCompare -SILENT -AUTO_SAVE OFF -O tree.ply -SOLIS \
  -COMPONENT DIRECT \
  -OUTPUT_MODE INSTANT \
  -SPATIAL_MODE HORIZONTAL \
  -DOY 172 -TIME 12:00 \
  -LAT 45 -LON 0 -ELV 0 \
  -RESOLUTION 1024
```

### Direct + Diffuse + Total daily energy

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

### True surface irradiance

```bash
CloudCompare -SILENT -AUTO_SAVE OFF -O model_with_normals.ply -SOLIS \
  -COMPONENT DIRECT \
  -OUTPUT_MODE INSTANT \
  -SPATIAL_MODE SURFACE \
  -DOY 355 -TIME 12:00 \
  -LAT 45 -LON 0 \
  -RESOLUTION 1024
```

### Hourly stored time series

```bash
CloudCompare -SILENT -AUTO_SAVE OFF -O tree.ply -SOLIS \
  -COMPONENT DIRECT \
  -OUTPUT_MODE INSTANT \
  -SPATIAL_MODE HORIZONTAL \
  -DOY 175 -TIME 08:00 \
  -BATCH_END_TIME 18:00 -BATCH_STEP 60 \
  -LAT 45 -LON 0 \
  -RESOLUTION 1024
```

## Result names and extraction

Every persistent result prints its exact scalar-field name:

```text
SOLIS: created scalar field 'SOLIS Direct [W/m2] D175 T1200 UTC H R1024'
```

For scripts, capture the printed names instead of reconstructing duplicate
suffixes:

```bash
CloudCompare ... -SOLIS ... 2>&1 | tee solis.log
sed -n "s/.*SOLIS: created scalar field '\\(.*\\)'/\\1/p" solis.log
```

The exact naming grammar and metadata conventions are documented in
`CLI_LAYER_NAMING.md`.

## Cleanup CLI

```text
-SOLIS_CLEAN LAST_RUN
-SOLIS_CLEAN LAST_BATCH
-SOLIS_CLEAN BATCH <batch-uuid>
-SOLIS_CLEAN ALL
-SOLIS_CLEAN LEGACY
```

`ALL` is metadata-driven and leaves unrelated scalar fields untouched. `LEGACY`
is a separate explicit operation that removes old non-metadata fields whose names
begin with `SOLIS `.

## Auto-save

CloudCompare command-line auto-save is on by default. qSOLIS exports modified
entities with a SOLIS suffix when auto-save remains enabled. Use:

```text
-AUTO_SAVE OFF
```

when you want to control output later with CloudCompare save commands.

## Headless environments

`-SILENT` suppresses qSOLIS progress dialogs, but the current visibility renderer
still requires a usable Qt/OpenGL platform. A completely displayless Linux host
may need an offscreen-capable Qt/OpenGL configuration or a virtual display.
