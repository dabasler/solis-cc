# qSOLIS v1.1.0 canopy-context fix 1

Compiler compatibility fix for current CloudCompare 2.14/CCCoreLib const-correctness.

`ccPointCloud::getBoundingBox()` is a non-const member in the current CloudCompare
API, while crown exposure intentionally receives a `const ccPointCloud*`.
`SOLISCrownExposure.cpp` now computes the point-cloud bounds directly through the
const-safe `getPoint()` accessor instead of casting away constness.

This changes no exposure formulas, result naming, metadata, renderer behavior,
or v1.0 Direct/Diffuse/Total calculations.
