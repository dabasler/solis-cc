# qSOLIS current-CloudCompare port notes

## Target

This source tree targets the Qt 6 generation of CloudCompare (2.14+ / current master). It deliberately does not carry Qt 5 compatibility shims for CloudCompare 2.13.x.

## Compatibility work completed

The legacy `QGLPixelBuffer`/GLU rendering path has been replaced by the same broad compatibility-profile architecture used by current qPCV: `QOpenGLContext`, an OpenGL `QWindow` surface, `QOpenGLFunctions_2_1`, pixel-pack buffers, and CloudCompare `ccGL::LookAt` / `ccGL::Project`. SOLIS adds persistent VBO/EBO geometry caches so static scene geometry is not resubmitted for each ray.

The plugin shell remains based on `ccStdPluginInterface`, `Q_PLUGIN_METADATA`, CloudCompare command registration, associated point-cloud scalar fields, and `ccProgressDialog` behavior. Command-line progress is suppressed in silent mode.

## Deliberate numerical changes

This port prioritizes corrected results over reproducing legacy output. In particular:

- Direct-sun directions are true unit spherical directions. The old implementation omitted the `cos(altitude)` factor in the horizontal components.
- Latitude is north-positive and may be negative; longitude is explicitly east-positive.
- UTC input no longer cancels the longitude correction. Local civil time is represented by a fixed UTC offset.
- Instantaneous, mean, and integrated quantities are separate modes instead of sharing the old integration scaling workaround.
- Direct irradiance is based on direct-normal irradiance (DNI). Horizontal mode multiplies by solar elevation cosine; surface mode multiplies by the positive dot product of the outward normal and source direction.
- Diffuse rays are deterministic equal-solid-angle upper-hemisphere samples instead of the legacy sphere/filter distribution.
- Diffuse irradiance is integrated as isotropic sky radiance: each uniform-hemisphere ray carries `2*DHI/N` before cosine incidence.
- Buffer-neighborhood sampling at image edges is bounds checked.

Because of these changes, old SOLIS scalar fields are not regression targets.

## Performance changes

1. Geometry coordinates are cached once on the CPU for projection.
2. Render geometry is uploaded once to a VBO; indexed meshes use an EBO when the input implements `GenericIndexedMesh`.
3. Per-point projection/depth tests are partitioned over CPU threads for sufficiently large clouds.
4. Direct time samples can optionally be merged when neighboring sun directions lie inside an angular tolerance.
5. Depth/color readback still occurs once per visibility direction. This is now the dominant architectural cost and is the main target for a future GPU backend.

## GPU accumulation seam

`SOLISAccumulationBackend` separates the solar/integration layer from the visibility/accumulation implementation. The current factory returns `SOLISContext`, which performs OpenGL shadow rendering followed by CPU projection and accumulation.

A future GPU backend can implement the same contract initially, then evolve the backend contract to batch rays/samples and retain a device-side scalar buffer. The intended high-performance path is:

```text
persistent scene VBO/EBO
        -> render shadow/depth map for sample
        -> GPU point projection + depth comparison
        -> GPU irradiance accumulation
        -> repeat samples
        -> one final scalar-buffer readback
```

That removes the per-ray depth-buffer GPU-to-CPU transfer and the `rays x points` CPU projection loop while leaving solar position, time integration, UI, CLI, and output-field management unchanged.

## Known limitations / items for validation in a full build

- This source package was API-reviewed against current CloudCompare/qPCV, but a full CloudCompare + Qt 6 toolchain is required for final compile/link/runtime validation.
- The renderer intentionally follows CloudCompare's current qPCV OpenGL 2.1 compatibility-profile approach. A core-profile-only/shader renderer is deferred.
- Diffuse sky is isotropic. Perez/Haydavies anisotropic sky distribution could be added later without changing the output semantics.
- Linke turbidity is constant across the requested interval.
- Surface mode assumes supplied normals are correctly oriented. Negative incidence is clamped to zero; normals are not silently reoriented.
- Day-of-year input has no calendar year, so the solar-position approximation uses a 365-day fractional-year convention. Day 366 is accepted for user convenience but cannot encode full leap-year calendar semantics.
- Point-cloud occlusion quality remains resolution/tolerance dependent because the algorithm is fundamentally a shadow/depth-map method.

## Suggested benchmark matrix

Use the same entity and solar interval while varying only one parameter at a time: 512/1024/2048 render resolution; 128/512/2048 diffuse rays; one thread versus automatic threads; direct grouping off versus 0.1/0.25/0.5 degrees. Record wall time and compare scalar-field RMS/max differences against the highest-quality non-grouped run.

## CLI restoration (2026-10)

The modern port now registers a `SOLISCommand` with CloudCompare's current `ccCommandLineInterface`. It deliberately does **not** reuse the legacy command's numerical conversion path. CLI jobs call the same `CalculationOptions`, direct-sample generator, diffuse sampler, FBO visibility backend, and accumulation methods as the modern GUI path.

Legacy aliases retained: `-TYPE`, `-INT`, `-TS`, `-NRAYS`, `-IS_CLOSED`. New options expose temporal/spatial modes, units, fixed-offset local time, Linke turbidity, direct-ray grouping, depth bias, and CPU thread count. `-TIME HH:MM[:SS]` is provided as an unambiguous alternative to encoding clock time in the fractional part of `-DOY`.

Core option validation is centralized in `SOLIS::ValidateOptions()`. In particular, DOY 0 is rejected. The encoded day/time value is valid in `[1, 367)`, so DOY 366 at noon (`366.5`) remains valid.


## Persistent SOLIS result records

Result creation is centralized in `SOLISResults.{h,cpp}`. GUI and CLI both use `solis::ResultManager`, which guarantees non-overwriting scalar-field allocation and deterministic human-readable names. Repeated identical runs receive `#2`, `#3`, etc. rather than modifying an earlier result.

Each persistent result also gets a UUID-backed JSON metadata record at `qSOLIS/result/<resultId>` on the point/vertex cloud. Fields created by one invocation share a `runId`. Temporary Direct/Diffuse fields used internally for a `TOTAL`-only CLI request are deliberately not catalogued and are deleted after Total is created.

The current metadata schema is version 3 (`qSOLIS/schemaVersion = 3`). Schema 2 covers the v1.0 result/run/batch model; schema 3 adds canopy-exposure and crown-summary provenance, including occlusion context. Cleanup remains tolerant of older metadata. A future result-browser/time-series UI can enumerate metadata keys with the `qSOLIS/result/` prefix and verify that the recorded scalar-field name still exists. This avoids introducing a custom CloudCompare object type or duplicating geometry.
