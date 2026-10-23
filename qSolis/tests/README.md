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
