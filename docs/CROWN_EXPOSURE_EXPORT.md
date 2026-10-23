# Crown Exposure Summary CSV export

qSOLIS v1.1.0 can export persisted tree-level Crown Exposure summaries without recalculating the exposure fields.

## GUI workflow

1. Keep each biological tree as a separate CloudCompare point cloud or mesh.
2. Run **SOLIS Exposure...** with **Crown Exposure Index** enabled.
3. Select the trees that should appear in the table.
4. Choose **SOLIS Export Crown Exposure Summary...**.
5. Save `qSOLIS_crown_exposure_summary.csv` (or another CSV filename).

The GUI writes one row per selected tree/entity using its **latest valid stored crown-exposure run**. A selected entity without Crown Exposure metadata is skipped and reported in the CloudCompare console.

## CLI

```text
-SOLIS_EXPORT_CROWN <output.csv> [LATEST|ALL_RUNS]
```

- `LATEST` is the default: one row per loaded tree/entity.
- `ALL_RUNS` exports every stored Crown Exposure run, useful for repeated dates/scenarios.
- The command reads stored metadata only; it does not run the exposure renderer.

Example:

```bash
CloudCompare -SILENT \
  -O Tree_001.bin \
  -O Tree_002.bin \
  -SOLIS_EXPORT_CROWN crown_exposure.csv
```

## CSV content

The table contains the main tree-level exposure statistics and enough provenance to reproduce/interpret them:

- entity/cloud identifiers, run ID, timestamp, plugin version, metadata schema;
- CEI, optional Sky Exposure mean and Sun Exposure mean, radiative mean;
- REI P10/P50/P90;
- exposed threshold, fraction and percent;
- weighting method, point-cloud voxel size/count or represented mesh area;
- point/vertex count;
- occlusion-context mode, occluder count and occluder entity names;
- latitude, longitude, elevation, Linke turbidity;
- time mode/UTC offset, day of year, temporal mode, duration and timestep;
- render resolution, sky-ray count and direct-sampling settings;
- source exposure result UUIDs.

Missing optional quantities (for example `sky_mean` when Sky Exposure was not requested) are exported as empty CSV cells rather than zero.

## Recommended analysis convention

Use `entity_name` as the tree identifier only when CloudCompare entity names are unique and stable in your project. For repeated measurements, retain `run_id` and `created_utc` in the analysis table. `cloud_unique_id` is useful for in-project traceability but should not be treated as an external biological tree identifier.
