# qSOLIS v1.1.0 Release Checklist

## Source review

- [x] Version is `1.1.0` in `VERSION`, CMake, `info.json`, and `SOLISVersion.h`.
- [x] Result metadata schema is 3.
- [x] Existing Direct/Diffuse/Total naming and numerical equations are unchanged.
- [x] `SOLIS Exposure...` GUI action is registered.
- [x] `-SOLIS_EXPOSURE` CLI command is registered.
- [x] Exposure/crown metadata participates in cleanup.
- [x] Canopy-exposure formulas and limitations are documented.
- [x] Pure exposure/crown math tests pass in the preparation environment.

## Required build acceptance

From a clean CloudCompare 2.14/Qt6 source checkout:

```bash
cmake -S . -B build-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DPLUGIN_STANDARD_QSOLIS=ON \
  -DOPTION_BUILD_CCVIEWER=OFF \
  -DBUILD_TESTING=ON \
  -DSOLIS_BUILD_TESTS=ON
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure -R qsolis
cmake --install build-release
```

Then run both end-to-end suites against the installed executable:

```bash
qSolis/tests/regression/run_qsolis_regression.sh /path/to/CloudCompare
qSolis/tests/regression/run_qsolis_exposure_regression.sh /path/to/CloudCompare
```

## Manual GUI acceptance

- [ ] `SOLIS Exposure...` appears between Calculate and Cleanup.
- [ ] Sky-only disables location/time/atmosphere controls.
- [ ] Crown summary forces Radiative Exposure on.
- [ ] Mesh-only selection reports surface-area weighting and disables voxel controls.
- [ ] Point-cloud selection reports occupied-voxel weighting.
- [ ] All exposure fields are finite and within `[0,1]`.
- [ ] Last Run removes exposure fields and crown metadata only.
- [ ] Existing Calculate GUI still reproduces the frozen v1.0 regression values.
