# Known Limitations - qSOLIS v1.1.0

- qSOLIS v1.1.0 targets the CloudCompare 2.14 / Qt 6 code line. It does not
  target CloudCompare 2.13.x / Qt 5.
- CLI mode is silent-capable but still requires a usable Qt/OpenGL platform;
  it is not a software-only renderer.
- The atmospheric calculation is clear-sky only and uses one Linke turbidity
  value per calculation; measured/cloud-cover weather time series are not read.
- Diffuse irradiance uses an isotropic deterministic hemisphere model.
- True surface irradiance depends on existing normal quality and orientation.
- Shadow boundaries are resolution/depth-bias dependent because visibility is
  rasterized to a finite-resolution offscreen depth buffer.
- Day-of-year calculations do not carry a calendar year; DOY 366 is accepted,
  while the current solar approximation uses a 365-day fractional-year formula.
- Direct angular grouping is an optional approximation and should be benchmarked
  against an ungrouped reference for scientific production work.

## Canopy exposure

- Sky Exposure is geometric openness, not wind/aerodynamic exposure.
- Sun Exposure is time-weighted geometric access.
- Radiative Exposure v1.1 uses a horizontal reference receiver and the existing clear-sky/isotropic-diffuse model.
- Point-cloud Crown Exposure depends on voxel size; the automatic value is bounding-box diagonal / 100.
- Crown Exposure Index is not equivalent to the traditional ordinal Crown Illumination Index.
