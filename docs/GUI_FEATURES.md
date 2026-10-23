# qSOLIS GUI feature map

The **SOLIS Calculate...** dialog exposes the same calculation model used by the CLI.

## Basic section

| GUI control | Core option | CLI equivalent |
|---|---|---|
| Latitude | `site.latitudeDeg` | `-LAT` |
| Longitude | `site.longitudeDeg` | `-LON` |
| Elevation | `site.elevationM` | `-ELV` |
| UTC / Local fixed offset | `site.timeMode` | `-TIME_MODE` |
| UTC offset | `site.utcOffsetHours` | `-UTC_OFFSET` |
| DOY + time | `time.dayOfYear` | `-DOY`, `-TIME` |
| Instant / Mean / Energy | `time.mode` | `-OUTPUT_MODE` |
| Duration | `time.durationHours` | `-DURATION` |
| Timestep | `time.timestepMinutes` | `-TIMESTEP` |
| Wh / kWh | `time.energyUnit` | `-ENERGY_UNIT` |
| Horizontal / Surface / Visibility | `spatialMode` | `-SPATIAL_MODE` |
| Direct / Diffuse | component selection | `-COMPONENT` |
| Batch end / step | `BatchOptions` | `-BATCH_END_*`, `-BATCH_STEP` |

## Quality presets

| Preset | Resolution | Diffuse rays |
|---|---:|---:|
| Fast | 512 | 64 |
| Normal | 1024 | 256 |
| High | 2048 | 512 |
| Custom | user-defined | user-defined |

The preset controls only resolution and diffuse-ray count. It does not silently alter atmospheric, timestep, depth-bias, threading, or direct-ray acceleration settings.

## Advanced section

| GUI control | Core option | CLI equivalent |
|---|---|---|
| Linke turbidity | `site.linkeTurbidity` | `-LINKE_TURBIDITY` |
| Resolution | `render.resolution` | `-RESOLUTION` |
| Depth bias | `render.depthBias` | `-DEPTH_BIAS` |
| CPU threads | `render.cpuThreadCount` | `-THREADS` |
| Closed mesh | `render.meshIsClosed` | `-IS_CLOSED` |
| Diffuse rays | `sampling.diffuseRayCount` | `-DIFFUSE_RAYS` |
| Direct grouping | `sampling.acceleratedDirectSampling` | `-ACCEL_ANGLE` / `-NO_ACCEL` |
| Angular tolerance | `sampling.directAngularToleranceDeg` | `-ACCEL_ANGLE` |

## Dynamic behavior

- Instantaneous mode disables duration/timestep.
- Mean and Energy enable duration/timestep.
- Energy enables Wh/kWh selection.
- Visibility reports `fraction`, disables the energy-unit selector, and disables Linke turbidity because irradiance magnitude is not part of the visibility result.
- Direct + Diffuse creates Total only for irradiance/energy modes. Visibility produces separate Direct and Diffuse visibility fields and deliberately no Total.
- Local fixed-offset time enables the UTC-offset control.
- Surface mode is disabled if any selected entity lacks point/vertex normals.
- Direct angular grouping is enabled only for non-instantaneous Direct calculations.
- Fast/Normal/High lock resolution and diffuse-ray count to preset values. Custom unlocks them.

## Surface-normal handling

When Surface mode is selected, qSOLIS copies and normalizes the existing CloudCompare point/vertex normals into the calculation buffer and passes them to both Direct and Diffuse accumulation. No normals are generated automatically. Missing normals result in an explicit error rather than a horizontal fallback.
