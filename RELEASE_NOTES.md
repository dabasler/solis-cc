# qSOLIS v1.0.0 Release Notes

qSOLIS v1.0.0 modernizes the original CloudCompare SOLIS plugin for the Qt 6 /
CloudCompare 2.14+ code line and substantially expands both scientific and
workflow capabilities.

## Highlights

- Modern Qt 6/OpenGL rendering backend using an explicit offscreen FBO.
- Direct, Diffuse, and Total solar result layers.
- Instantaneous, mean, and integrated irradiation modes.
- True surface irradiance using existing CloudCompare normals.
- Batch time-series generation without duplicating point-cloud geometry.
- Persistent result metadata and deterministic scalar-field naming.
- Built-in cleanup for accidental or obsolete SOLIS batches.
- Full CLI implementation suitable for scripted workflows.
- Frozen regression suite included with the source.

## Compatibility

This release targets CloudCompare 2.14+ / current master and Qt 6. It does not
target the Qt 5 based CloudCompare 2.13.x line.

At the release date, CloudCompare 2.13.2 remains the latest stable release and
2.14 is published as a Qt 6 pre-release line. Precompiled qSOLIS assets should
therefore name the exact 2.14 tag/commit they were built against.

## Upgrade from the legacy plugin

Results should not be expected to match the older SOLIS plugin exactly. Several
legacy numerical behaviors were intentionally corrected, including sun-direction
geometry, time/longitude interpretation, and instantaneous/integrated scaling.
The diffuse sampling strategy was also redesigned.

Existing older SOLIS scalar fields are not automatically deleted. The Cleanup
dialog contains an explicit legacy-name cleanup mode if desired.

## Before publishing binaries

Build against the exact CloudCompare release/toolchain you plan to support and
run `qSolis/tests/regression/run_qsolis_regression.sh` against the installed
binary. Platform plugin binaries should be labeled with the CloudCompare version,
architecture, and compiler/Qt line they were built against.
