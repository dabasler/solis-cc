# Building qSOLIS v1.0.0

qSOLIS is built as an in-tree CloudCompare Standard plugin. The release includes
a helper that copies or symlinks the plugin into a CloudCompare source checkout.

## Requirements

- CloudCompare 2.14+ / current Qt 6 source tree
- Qt 6 development packages
- CMake and a supported C++17 compiler
- OpenGL development/runtime support
- Ninja is recommended but not required

## Integrate the source

From the qSOLIS release root:

```bash
python3 tools/install_into_cloudcompare.py /path/to/CloudCompare --symlink
```

Use `--force` to replace an existing qSolis source folder. Without `--symlink`,
the helper copies the source instead.

The helper places qSOLIS at:

```text
CloudCompare/plugins/core/Standard/qSolis
```

and adds `add_subdirectory( qSolis )` to the Standard plugin CMake list if needed.

## Linux development build

```bash
cd /path/to/CloudCompare
cmake -S . -B build-qsolis \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DOPTION_BUILD_CCVIEWER=OFF \
  -DPLUGIN_STANDARD_QSOLIS=ON
cmake --build build-qsolis --parallel
```

## Clean release/install build

```bash
cd /path/to/CloudCompare
rm -rf build-qsolis-release /tmp/cloudcompare-qsolis-install

cmake -S . -B build-qsolis-release \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/tmp/cloudcompare-qsolis-install \
  -DOPTION_BUILD_CCVIEWER=OFF \
  -DPLUGIN_STANDARD_QSOLIS=ON \
  -DBUILD_TESTING=ON \
  -DSOLIS_BUILD_TESTS=ON

cmake --build build-qsolis-release --parallel
ctest --test-dir build-qsolis-release --output-on-failure -R qsolis
cmake --install build-qsolis-release
```

Then run the end-to-end regression suite against the installed CloudCompare:

```bash
cd /path/to/qSOLIS/qSolis/tests/regression
./run_qsolis_regression.sh /tmp/cloudcompare-qsolis-install/bin/CloudCompare
```

The installed executable path differs by platform; use the CloudCompare install
layout produced by that platform.

## CMake options

```text
PLUGIN_STANDARD_QSOLIS  Build/install qSOLIS (default OFF)
SOLIS_BUILD_TESTS       Build lightweight qSOLIS unit tests (default OFF)
BUILD_TESTING           CloudCompare/CTest switch required for the above tests
```

## macOS development note

CloudCompare's current build documentation supports a development plugin path
and a self-contained `pixi` workflow. For a distributable application, build and
bundle through the CloudCompare installation/bundling path rather than copying a
random `.dylib` between unrelated CloudCompare builds.

## Windows development note

CloudCompare's build documentation expects executables, plugins, and runtime
DLLs to be staged through the CMake install prefix. Use the `INSTALL`/`cmake
--install` output for testing and binary packaging rather than assuming the raw
Visual Studio/Ninja target directory is a complete runnable installation.
