# qSOLIS tests

Enable the lightweight C++ solar-model test with:

```bash
cmake -S . -B build-test \
  -DPLUGIN_STANDARD_QSOLIS=ON \
  -DBUILD_TESTING=ON \
  -DSOLIS_BUILD_TESTS=ON
cmake --build build-test --parallel
ctest --test-dir build-test --output-on-failure -R qsolis
```

The `regression/` directory contains the end-to-end CloudCompare tests used for
v1.0.0 acceptance. They verify surface-normal incidence, missing-normal rejection,
and the OpenGL/FBO wall-shadow path. See `regression/README.md`.

## Canopy exposure

`qsolis_exposure_math` tests fraction normalization, REI denominator/range handling, and weighted statistics. `qsolis_crown_exposure_math` verifies occupied-voxel aggregation and point-density invariance without OpenGL. The end-to-end `regression/run_qsolis_exposure_regression.sh` test exercises the new CLI and FBO visibility path.

- `qsolis_crown_export`: validates persisted Crown Exposure metadata parsing, newest-run selection, CSV quoting/serialization, provenance fields, and file writing.
