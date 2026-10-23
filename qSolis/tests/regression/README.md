# qSOLIS v1.0.0 regression suite

Coordinate convention for the synthetic scenes is X=east, Y=north, Z=up.

Run the installed release candidate:

```bash
./run_qsolis_regression.sh /path/to/CloudCompare
```

The suite verifies:

1. Direct instantaneous **surface irradiance** with known normals.
2. Explicit rejection of Surface mode when normals are absent.
3. Direct instantaneous **horizontal irradiance** behind a 5 m wall, including
   the offscreen OpenGL/FBO visibility path.

The frozen expected values are in `expected_results.json`. If the scientific
model is deliberately changed in a future release, update the baseline as an
explicit model change; do not simply loosen tolerances.

The original v1.0.0 acceptance run produced:

```text
surface HORIZONTAL  : median=262.5244 W/m2  PASS
surface SOUTH       : median=665.6770 W/m2  PASS
surface NORTH       : median=0.0000 W/m2    PASS
surface TILT45_SOUTH: median=656.6156 W/m2  PASS
wall sunlit median  : 834.6073 W/m2         PASS
wall shadow median  : approximately 0 W/m2 PASS
expected shadow length: 1.9746 m
```

## Multi-entity exposure context

`context_target_ground.ply` and `context_neighbor_wall.ply` are loaded as separate entities. The exposure regression verifies that `-CONTEXT TARGET` leaves the ground target exposed while `-CONTEXT LOADED` allows the separately loaded wall to cast the expected solar occlusion onto the target.


Run the v1.1 canopy-exposure suite separately:

```bash
./run_qsolis_exposure_regression.sh /path/to/CloudCompare
```

It covers open-reference exposure, no-daylight rejection, wall Sun/Sky/REI behavior, and separate target/neighbor geometry for self-only versus loaded-context occlusion.
