# qSOLIS v1.0.0 Release Checklist

Use this checklist before publishing source or a platform binary.

## Source release

- [x] Version centralized as `1.0.0` in `SOLISVersion.h`.
- [x] GUI shows `qSOLIS v1.0.0`.
- [x] Plugin `info.json` reports `1.0.0`.
- [x] Result metadata records the qSOLIS version.
- [x] Runtime diagnostic spam removed; errors and concise result messages retained.
- [x] README, CLI reference, naming rules, cleanup docs, changelog, release notes, and manual consolidated.
- [x] Frozen regression inputs and expected results included.
- [x] User manual visually verified as DOCX and PDF.

## Build acceptance

Run from a clean CloudCompare checkout/build directory:

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

Then run `qSolis/tests/regression/run_qsolis_regression.sh` against the installed
CloudCompare executable.

## Binary release acceptance

For each OS/architecture/CloudCompare target:

- [ ] Build against an exact CloudCompare tag or commit, not an unspecified `master`.
- [ ] Record CloudCompare version/commit, Qt version, compiler/toolchain, OS, and architecture.
- [ ] Test installation into a clean matching CloudCompare installation.
- [ ] Run the frozen qSOLIS regression suite on that binary.
- [ ] Smoke-test GUI Calculate and Cleanup actions.
- [ ] Confirm `512`, `1024`, and `2048` render resolutions return non-zero consistent results.
- [ ] Confirm cancellation works.
- [ ] Confirm Surface mode accepts normals and rejects missing normals.
- [ ] Compute SHA-256 for every published asset.
- [ ] Include `LICENSE` and a link to corresponding source.

## Publication

- [ ] Tag source as `v1.0.0`.
- [ ] Create a GitHub Release from the tag.
- [ ] Attach the user manual PDF.
- [ ] Attach platform binaries only when they pass the binary acceptance list.
- [ ] State exact supported CloudCompare version(s) in each binary asset description.
