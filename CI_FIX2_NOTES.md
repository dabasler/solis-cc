# qSOLIS v1.1.0 CI portability fix 2

This update addresses failures found by the first full GitHub Actions run on Linux, macOS arm64, and Windows x64.

## Solar UTC/local equivalence

The test correctly exposed a model inconsistency: when a fixed local UTC offset was selected, the solar hour-angle calculation converted the civil clock to UTC, but the NOAA fractional-year term (declination/equation-of-time) still used the local clock fraction. Therefore two representations of the same instant, such as 12:00 UTC and 14:00 UTC+2, produced slightly different solar positions.

The fractional-year term and extraterrestrial irradiance are now evaluated at the corresponding UTC day-of-year. The local civil clock is still used for the displayed/requested time and the UTC offset is still applied to true solar time. UTC-mode results are unchanged.

The solar unit test now verifies equivalence of altitude, azimuth, DNI, DHI, and GHI.

## Windows Crown Exposure export test

The CSV/metadata unit test previously instantiated `ccPointCloud` and linked `QCC_DB_LIB` only to provide a metadata map. On the hosted Windows runner the test process did not terminate and CTest killed it after 1500 seconds.

Crown Exposure export is now split into:

- a QtCore-only metadata parser / CSV serializer (`SOLISCrownExport.cpp`), and
- a thin CloudCompare DB wrapper (`SOLISCrownExportCloud.cpp`).

The unit test exercises the QtCore-only path and no longer instantiates CloudCompare DB objects. The plugin itself continues to read real `ccPointCloud` metadata through the wrapper. A 30-second CTest timeout guards against future hangs.
