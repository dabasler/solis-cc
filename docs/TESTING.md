# qSOLIS v1.0.0 Testing and Release Acceptance

## Lightweight solar-model test

Configure CloudCompare with:

```text
-DPLUGIN_STANDARD_QSOLIS=ON
-DBUILD_TESTING=ON
-DSOLIS_BUILD_TESTS=ON
```

Then run:

```bash
ctest --test-dir build --output-on-failure -R qsolis
```

## Frozen end-to-end regression

The release archive includes:

```text
qSolis/tests/regression/
```

Run:

```bash
cd qSolis/tests/regression
./run_qsolis_regression.sh /path/to/installed/CloudCompare
```

The suite performs:

1. Surface irradiance with known normals.
2. Surface-mode rejection when normals are absent.
3. Synthetic wall shadow / offscreen FBO visibility regression.

v1.0.0 accepted baseline:

```text
surface HORIZONTAL  : median=262.5244 expected=263.1812 PASS
surface SOUTH       : median=665.6770 expected=665.4265 PASS
surface NORTH       : median=0.0000 expected=0.0000 PASS
surface TILT45_SOUTH: median=656.6156 expected=656.6248 PASS
surface orientation ordering: PASS
wall sunlit median  : 834.6073 expected=834.6073 PASS
wall shadow median  : approximately 0 (PASS)
expected geometric shadow length: 1.9746 m
```

The surface test tolerance intentionally allows small differences from normal
quantization when CloudCompare imports/exports compressed normal directions.

## Manual acceptance checklist

Before publishing a binary for a new platform/CloudCompare release, also verify:

- plugin loads and both Calculate/Cleanup actions appear;
- version shown in GUI is `v1.0.0`;
- Direct, Diffuse, and Total fields are populated;
- 512/1024/2048 resolutions all produce non-zero consistent results;
- cancellation leaves CloudCompare usable and no corrupted partial result;
- GUI and CLI produce visually/numerically equivalent results for identical
  common-subset settings;
- batch results are stored as separate scalar fields;
- Last Batch cleanup removes only qSOLIS batch fields;
- source build and `cmake --install` work from a clean build directory.

## Updating the frozen baseline

Do not update `expected_results.json` merely because a test fails. First determine
whether the failure is a regression, platform rasterization tolerance, or an
intentional scientific-model change. Intentional model changes should be noted in
the changelog and the baseline update should be reviewed as part of that change.
