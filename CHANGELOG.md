# Changelog

All notable changes to qSOLIS are documented here.

## [1.1.0] - 2026-10-23

### CI / portability fixes

- Correct UTC/local fixed-offset solar equivalence: declination, equation of time, and extraterrestrial irradiance are now evaluated at the same physical UTC instant while preserving the user-facing local clock. UTC-mode numerical behavior is unchanged.
- Split Crown Exposure CSV metadata parsing/serialization from the CloudCompare DB wrapper so the headless unit test is QtCore-only and portable on Windows CI.


### Added

- `SOLIS Exposure...` GUI action and `-SOLIS_EXPOSURE` CLI command.
- Sky Exposure Factor (SEF), Sun Exposure Fraction (SuEF), and Radiative Exposure Index (REI), all in `[0,1]`.
- Crown Exposure Index (CEI) and weighted crown distribution statistics.
- Occupied-voxel crown weighting for point clouds and triangle-area weighting for meshes.
- Shared per-direction visibility masks so SEF/diffuse REI and SuEF/direct REI reuse the same FBO passes.
- Schema-v3 exposure/crown metadata with cleanup integration.
- Exposure math unit tests and open-reference/wall-shadow CLI regression scripts.
- Explicit target/context separation for canopy exposure: Target only, Selected entities, and All visible entities in the GUI; TARGET/LOADED context in the CLI.
- Multi-entity exposure renderer geometry cache so neighboring trees/terrain can occlude target points without merging point clouds.
- Context provenance in exposure/crown metadata and a separate-neighbor regression scene.
- `SOLIS Export Crown Exposure Summary...` GUI action and `-SOLIS_EXPORT_CROWN` CLI command for one-row-per-tree CSV export of persisted CEI summaries and provenance.

### Changed

- qSOLIS result metadata schema advances from 2 to 3; schema-2 records remain cleanup-compatible.
- The visibility backend now exposes a reusable geometric visibility-mask primitive while preserving existing Direct/Diffuse/Total accumulation semantics.


## [1.0.0] - 2026-10-15

First release of the modern Qt 6 / current-CloudCompare port.

### Added

- CloudCompare 2.14+/Qt 6 plugin architecture.
- Direct, Diffuse, and Total result components.
- Instantaneous, mean, and integrated-energy output modes.
- Horizontal-reference, true surface (normal-aware), and visibility modes.
- UTC and fixed-offset local time handling.
- Configurable Linke turbidity.
- Deterministic diffuse hemisphere sampling.
- Explicit Qt/OpenGL framebuffer object with depth attachment.
- Persistent VBO/EBO geometry caching during a calculation.
- Parallel CPU projection and depth visibility evaluation.
- Optional direct-sun angular grouping for accelerated long integrations.
- GUI quality presets and advanced rendering/sampling controls.
- GUI batch time-series generation.
- CLI `-SOLIS` implementation with legacy aliases where practical.
- Persistent result scalar fields with deterministic names and full metadata.
- Direct/Diffuse/Total results from one run linked by a shared run identifier.
- Batch result metadata linked by a shared batch identifier.
- Integrated cleanup UI and `-SOLIS_CLEAN` CLI command.
- Frozen regression scenes and automated release test script.
- Plugin version shown in the GUI and stored with result metadata.

### Changed from the legacy plugin

- Corrected solar direction vector geometry.
- Corrected UTC/longitude interpretation.
- Corrected instantaneous versus integrated scaling semantics.
- Southern latitudes are supported in the GUI.
- Day-of-year 0 is rejected; valid input is 1 through 366 (with fractional time).
- Surface irradiance uses the point/vertex normal incidence term.
- Diffuse sampling was redesigned rather than preserving the old ray pattern.
- Legacy Qt `QGLPixelBuffer` and GLU dependencies were removed.
- Result correctness takes priority over reproducing known legacy numerical bugs.

### Performance

- Reuses one rendering backend across Direct/Diffuse passes for an entity.
- Reuses uploaded geometry across solar directions and batch points.
- Uses pixel buffer objects for framebuffer readback.
- Uses multi-threaded CPU projection/visibility work for sufficiently large clouds.

### Validation baseline

The v1.0.0 release regression suite passed the following frozen checks on the
acceptance system:

```text
surface HORIZONTAL  : 262.5244 W/m2
surface SOUTH       : 665.6770 W/m2
surface NORTH       : 0.0000 W/m2
surface TILT45_SOUTH: 656.6156 W/m2
wall sunlit median  : 834.6073 W/m2
wall shadow median  : approximately 0 W/m2
expected wall shadow length: 1.9746 m
```

### Known limitations

- Qt 5 / CloudCompare 2.13.x is not supported by this release.
- CLI rendering still requires a usable Qt/OpenGL platform.
- Diffuse sky radiance is isotropic.
- Linke turbidity is constant over a calculation unless separate runs are made.
- No measured cloud/weather input is included.
- GPU-side irradiance accumulation is not yet implemented; the architecture
  intentionally leaves a backend seam for this future optimization.
