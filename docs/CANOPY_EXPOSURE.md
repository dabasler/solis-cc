# qSOLIS Canopy Exposure metrics (v1.1.0)

qSOLIS v1.1.0 adds a dedicated **SOLIS Exposure...** workflow for quantifying how open a canopy point/vertex is to the sky and solar path, plus an entity-level Crown Exposure Index.

## Metrics

### Sky Exposure Factor (SEF)

For point/vertex `i`:

```text
SEF_i = sum_j(w_j * V_ij) / sum_j(w_j)
```

`j` samples the global upper hemisphere (`+Z` is up), `V` is geometric visibility (0/1), and `w` is solid-angle weight. qSOLIS uses deterministic equal-solid-angle Fibonacci samples, so every sample has equal weight.

- Range: `0..1`
- No site, date, time, atmosphere, or normals required.
- Not cosine weighted.
- This is geometric sky openness, not wind exposure or atmospheric transport.

### Sun Exposure Fraction (SuEF)

```text
SuEF_i = sum_k(dt_k * V_ik) / sum_k(dt_k)
```

Only samples with the sun above the horizon enter the denominator. The metric is **time weighted**, not irradiance weighted.

- `0`: never sees the sun during sampled daylight.
- `1`: sees the sun for all sampled daylight.
- An instantaneous daylight calculation returns 0 or 1.
- A period with no daylight is rejected because the fraction is undefined.

### Radiative Exposure Index (REI)

```text
REI_i = (received direct + received diffuse) / (open direct + open diffuse)
```

The denominator is the radiation the same **horizontal-reference receiver** would receive with all directions unobstructed. v1.1.0 intentionally does not use point normals for REI.

Direct open potential uses the same qSOLIS clear-sky DNI/time samples as the numerator. Diffuse open potential uses the exact same deterministic sky quadrature as the received diffuse numerator with visibility forced to one. This keeps the normalization internally consistent.

- Range: `0..1` apart from tiny floating-point tolerance.
- Requires site/time and Linke turbidity.
- A zero open-sky radiation denominator is rejected.

### Crown Exposure Index (CEI)

CEI is an **entity-level** summary, not another point scalar field:

```text
CEI = spatially weighted mean(REI)
```

It means: **mean fraction of open-site clear-sky radiation available across the represented crown**.

CEI is not the traditional ordinal Crown Illumination Index and no 1–5 mapping is implied.


## Target geometry versus occlusion context

Exposure values are always calculated **per point/vertex of a target entity**. The geometry used to block sky/sun rays is configured separately.

GUI context modes:

```text
Target only
    Each target is rendered against itself only. This measures self-exposure.

Selected entities   [default]
    All selected target clouds/meshes mutually occlude one another.
    Every selected target still receives its own result fields and crown summary.
    With only one selected entity this is equivalent to Target only.

All visible entities
    Results are written only to the selected target(s), but every currently
    visible/enabled point cloud or mesh in the CloudCompare DB tree can block rays.
    This is the recommended mode for one-tree-at-a-time ecological crown exposure.
```

Example: select `Tree_A` only, leave `Tree_B`, `Tree_C`, and terrain visible, and choose **All visible entities**. qSOLIS writes SEF/SuEF/REI/CEI only to `Tree_A`; the neighboring trees and terrain are context-only occluders.

The context geometry is uploaded once into the exposure renderer and reused for all sky and solar directions. Target points remain a separate projection/output array, so neighboring geometry never becomes part of the target crown statistics.

The selected context mode and the names/IDs of the occluding entities are stored in exposure and crown metadata for reproducibility.

### CLI context

Command-line mode has no GUI selection/visibility concept, so it provides:

```text
-CONTEXT TARGET   # each loaded target sees only itself
-CONTEXT LOADED   # all loaded clouds/meshes mutually occlude (default)
```

CLI currently treats loaded entities as both targets and, in `LOADED` mode, occluders. For a context-only neighbor without an output field, use the GUI **All visible entities** workflow.

## Crown spatial weighting

### Point clouds

qSOLIS uses occupied-voxel weighting to reduce sensitivity to local scan density:

1. points are assigned to a regular voxel grid;
2. REI is averaged inside each occupied voxel;
3. every occupied voxel gets equal crown weight;
4. crown statistics are calculated over occupied voxels.

Automatic voxel size is:

```text
bounding-box diagonal / 100
```

An explicit voxel size can be supplied.

### Meshes

Meshes use surface-area weighting. Each triangle of area `A` contributes `A/3` to each of its three vertices. CEI and weighted REI percentiles then use these vertex weights.

## Crown summary

The stored/reportable summary contains:

- CEI / weighted mean REI;
- weighted mean SEF if calculated;
- weighted mean SuEF if calculated;
- weighted REI P10, median/P50 and P90;
- weighted fraction of crown representation with REI >= threshold;
- default threshold `0.75`;
- weighting method;
- actual voxel size for point clouds;
- occupied voxel count or represented mesh surface area;
- source result IDs.

## Shared rendering

The exposure implementation uses the existing qSOLIS FBO visibility backend through a reusable per-direction visibility mask.

One sky render contributes to both:

```text
Sky Exposure
Diffuse part of REI
```

One solar render contributes to both:

```text
Sun Exposure
Direct part of REI
```

Therefore selecting all metrics requires approximately `sky passes + solar passes`, not one complete render campaign per metric.

## GUI

Select one or more point clouds/meshes and choose:

```text
Plugins > SOLIS Exposure...
```

Duration `0` means an instantaneous calculation. Positive duration uses the configured timestep.

Quality presets:

```text
Fast    512 px / 64 sky rays
Normal 1024 px / 256 sky rays
High   2048 px / 512 sky rays
Custom user selected
```

## CLI

Example:

```bash
CloudCompare -SILENT -AUTO_SAVE OFF -O tree.ply -SOLIS_EXPOSURE \
  -METRICS ALL -CONTEXT LOADED \
  -DOY 172 -TIME 00:00 \
  -DURATION 24 -TIMESTEP 10 \
  -LAT 45 -LON 0 -ELV 0 \
  -LINKE_TURBIDITY 3 \
  -SKY_RAYS 256 -RESOLUTION 1024 \
  -CROWN_SUMMARY -CROWN_VOXEL_AUTO
```

Options:

```text
-METRICS SKY|SUN|RADIATIVE|ALL
-CONTEXT TARGET|LOADED
-DOY <value>
-TIME <HH:MM[:SS]>
-DURATION <hours>       (0 = instantaneous)
-TIMESTEP <minutes>
-LAT <degrees>
-LON <degrees>
-ELV <metres>
-TIME_MODE UTC|LOCAL
-UTC_OFFSET <hours>
-LINKE_TURBIDITY <value>
-SKY_RAYS <count>
-RESOLUTION <pixels>
-DEPTH_BIAS <value>
-THREADS <count>
-IS_CLOSED
-ACCEL_ANGLE <degrees>
-NO_ACCEL
-CROWN_SUMMARY
-CROWN_VOXEL_AUTO
-CROWN_VOXEL_SIZE <metres>
-CROWN_THRESHOLD <0..1>
```

`-CROWN_SUMMARY` requires `RADIATIVE` or `ALL`.

## Scalar-field names

```text
SOLIS Sky Exposure [fraction] rays256 R1024
SOLIS Sun Exposure [fraction] D172 T1200 UTC R1024
SOLIS Sun Exposure [fraction] D172 T0000 UTC dur24h dt10m R1024
SOLIS Radiative Exposure [fraction] D172 T0000 UTC dur24h dt10m H R1024
```

Duplicate names receive the existing `#2`, `#3`, ... suffix.


## Export Crown Exposure Summary

Crown Exposure summaries are persistent entity metadata rather than scalar fields. To create a tree-level table:

1. Calculate exposure with **Crown Exposure Index** enabled.
2. Keep each biological tree as a separate CloudCompare point-cloud or mesh entity.
3. Select the trees to export.
4. Choose **Plugins > SOLIS Export Crown Exposure Summary...**.
5. Save the CSV.

The GUI exports the **latest stored crown run per selected entity**, producing one row per tree. This is intentionally separate from the point-level SEF/SuEF/REI scalar fields.

Key CSV columns include:

```text
entity_name
run_id
created_utc
cei
sky_mean
sun_mean
radiative_mean
rei_p10
rei_p50
rei_p90
exposure_threshold
exposed_fraction
exposed_percent
weighting_method
voxel_size_m
occupied_voxel_count
represented_surface_area_m2
point_or_vertex_count
occlusion_context
occluder_count
occluder_entities
latitude_deg / longitude_deg / elevation_m
linke_turbidity
day_of_year / duration_hours / timestep_minutes
resolution / sky_ray_count
run/plugin/schema provenance
```

For automated workflows, use:

```text
-SOLIS_EXPORT_CROWN <output.csv> [LATEST|ALL_RUNS]
```

`LATEST` (default) gives one row per loaded tree/entity. `ALL_RUNS` exports longitudinal/repeated crown-exposure calculations as separate rows.

## Metadata and cleanup

Exposure result fields use the normal `qSOLIS/result/<UUID>` metadata namespace and share one run ID. Schema version is 3. Exposure metadata records `occlusionContext`, `occluderCount`, and the context entity names/IDs. Crown summaries are stored under:

```text
qSOLIS/crownExposure/<runId>
```

`SOLIS Cleanup...`, `-SOLIS_CLEAN LAST_RUN`, and `-SOLIS_CLEAN ALL` remove exposure fields and associated crown metadata without touching unrelated scalar fields. Schema-2 v1.0 result records remain readable/cleanable.

## Limitations

- SEF is geometric visibility, not aerodynamic wind exposure.
- SuEF is geometric direct-sun access, not a heat-load metric.
- REI uses qSOLIS clear-sky radiation and constant Linke turbidity for a run.
- REI v1.1 is horizontal-reference only.
- The diffuse model remains isotropic.
- Visibility near shadow boundaries depends on render resolution/depth bias.
- Point-cloud CEI depends on voxel scale; report the selected/automatic voxel size with results.
