# qSOLIS v1.1.0 Canopy Exposure — Implementation Notes

## Core architecture

`SOLISAccumulationBackend` now exposes a geometric `visibility()` primitive that
renders one direction and returns a per-point 0/1 mask. The existing
`accumulate()` path is implemented on top of the same primitive, preserving the
v1.0 Direct/Diffuse/Total incidence calculations.

`SOLISExposure.cpp` performs one render for each required sky direction and one
for each required solar direction. The resulting masks are shared among the
selected metrics:

```text
sky mask   -> SEF + diffuse REI
solar mask -> SuEF + direct REI
```

`SOLISCrownExposure.cpp` performs entity-level spatial aggregation. Point clouds
use occupied voxels; meshes use `A/3` triangle-area contributions to vertices.

## New files

```text
qSolis/include/SOLISExposure.h
qSolis/include/SOLISExposureMath.h
qSolis/include/SOLISCrownExposure.h
qSolis/include/SOLISCrownExposureMath.h
qSolis/include/SOLISExposureCommand.h
qSolis/include/ccSolisExposureDlg.h

qSolis/src/SOLISExposure.cpp
qSolis/src/SOLISCrownExposure.cpp
qSolis/src/SOLISExposureCommand.cpp
qSolis/src/ccSolisExposureDlg.cpp

qSolis/ui/solisExposureDlg.ui
qSolis/tests/test_exposure_math.cpp
qSolis/tests/test_crown_exposure_math.cpp
qSolis/tests/regression/run_qsolis_exposure_regression.sh
qSolis/tests/regression/verify_exposure.py
qSolis/tests/regression/exposure_open_point.ply
qSolis/tests/regression/context_target_ground.ply
qSolis/tests/regression/context_neighbor_wall.ply
```

## Local validation performed while preparing this tree

- both Qt Designer `.ui` files parse as valid XML;
- every new exposure header/source/UI is registered by qSOLIS CMake files;
- `test_exposure_math.cpp` compiles standalone with C++17 and passes;
- `test_crown_exposure_math.cpp` compiles standalone with C++17 and passes, including automatic voxel scale, triangle-area weight distribution, and point-density invariance checks;
- new Python verifier compiles with `py_compile`;
- new shell regression script passes `bash -n` and includes open-reference SEF/SuEF/REI, no-daylight rejection, wall Sun/Sky/REI cases, plus separate target/neighbor context tests for TARGET versus LOADED occlusion;
- the existing v1.0 `SOLIS.cpp` calculation implementation is unchanged in the v1.1 tree.

A complete CloudCompare/Qt6 development SDK is not available in the artifact
preparation runtime, so the final CloudCompare plugin compile and OpenGL
end-to-end runs must be performed in the normal CloudCompare build environment.

## Multi-entity occlusion context

The exposure renderer keeps two independent domains:

1. `m_points`: target point/vertex coordinates that receive visibility values;
2. a flattened context VBO with point/triangle draw batches for all occluding entities.

`SOLISAccumulationBackend::setOcclusionContext()` is called before `init()` only by the exposure workflow. Existing Direct/Diffuse/Total callers never configure a context and therefore retain the original single-entity renderer/cache path. Context meshes are rendered double-sided for geometric occlusion; point-cloud blockers retain the one-pixel depth footprint used by the existing renderer. Bounds for the orthographic exposure render are computed over the complete context while output remains the target point array.


The core calculator refuses to silently downgrade a requested multi-entity context when called without a preconfigured renderer. Its internal renderer fallback is permitted only for `TargetOnly`, preventing a Selected/All-visible request from accidentally becoming a self-only calculation.


## Crown Exposure CSV export

`SOLISCrownExport` parses the persistent `qSOLIS/crownExposure/<runId>` JSON records and serializes a stable UTF-8 CSV. The GUI action exports the newest valid crown record per selected entity. `SOLISCrownExportCommand` provides the same export for loaded CLI entities and supports `ALL_RUNS`. No exposure recalculation is performed during export.
