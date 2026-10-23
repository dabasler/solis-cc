# qSOLIS v1.1.0 Release Notes

qSOLIS v1.1.0 adds quantitative canopy-exposure analysis while preserving the
v1.0 Direct/Diffuse/Total calculation model and result naming.

## Canopy Exposure

A new **SOLIS Exposure...** GUI action and `-SOLIS_EXPOSURE` CLI command provide:

- **Sky Exposure Factor (SEF)** — geometric fraction of the global upper
  hemisphere visible from each point/vertex.
- **Sun Exposure Fraction (SuEF)** — fraction of sampled daylight time with an
  unobstructed line of sight to the sun.
- **Radiative Exposure Index (REI)** — received clear-sky direct + diffuse
  horizontal radiation divided by the unobstructed open-site potential.
- **Crown Exposure Index (CEI)** — entity-level spatially weighted mean REI,
  with distribution statistics.

Point-cloud crown summaries use equal weighting of occupied voxels to reduce
local scan-density bias. Mesh summaries use triangle surface-area weighting.


## Target and neighboring geometry

Canopy exposure now separates the entity receiving result fields from the geometry used for occlusion. The GUI offers **Target only**, **Selected entities**, and **All visible entities**. Selecting one tree and choosing All visible entities computes that tree point-by-point while neighboring visible trees, terrain, or other geometry block sky and solar rays. Selected entities mutually occlude and each receive their own result fields/CEI.

CLI exposure supports `-CONTEXT TARGET|LOADED`; LOADED is the default and makes all loaded entities mutually occlude. Context mode and occluder provenance are stored in metadata.

## Rendering architecture

The exposure workflow reuses qSOLIS's existing offscreen FBO renderer. One sky
direction visibility pass feeds both SEF and diffuse REI; one solar-direction
pass feeds both SuEF and direct REI. The existing Direct/Diffuse/Total path
continues to use the same visibility semantics.

## Metadata and cleanup

Result metadata schema advances to 3. v1.0/schema-2 result records remain
readable and removable by the cleanup tools. Exposure scalar fields use the
normal result metadata namespace; crown summaries are stored under
`qSOLIS/crownExposure/<runId>` and are removed with Last Run / All cleanup.

## Compatibility

qSOLIS v1.1.0 continues to target the CloudCompare 2.14 / Qt 6 code line.
Platform binaries should be built and tested against a specific CloudCompare
release/tag/commit.

## Validation

The source includes:

- pure exposure-fraction/weighted-statistics tests;
- occupied-voxel point-density-invariance tests;
- the unchanged v1.0 irradiance/surface/FBO regression suite;
- a new exposure regression script covering open-sky SEF/SuEF/REI,
  no-daylight rejection, wall-shadow sun access, wall sky openness, and wall REI.

## Crown Exposure table export

Select multiple tree entities and use **SOLIS Export Crown Exposure Summary...** to create one CSV row per tree from the latest stored CEI run. CLI automation is available via `-SOLIS_EXPORT_CROWN <file.csv> [LATEST|ALL_RUNS]`.
