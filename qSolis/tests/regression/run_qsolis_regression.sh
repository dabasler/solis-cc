#!/usr/bin/env bash
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CC="${1:-$HOME/dev/CloudCompare/build-dev/qCC/CloudCompare}"
OUT="${2:-$HERE/out}"

if [[ ! -x "$CC" ]]; then
  echo "ERROR: CloudCompare executable not found/executable: $CC" >&2
  echo "Usage: $0 /path/to/CloudCompare [output-dir]" >&2
  exit 2
fi

mkdir -p "$OUT"
rm -f "$OUT"/surface.asc "$OUT"/wall.asc "$OUT"/*.log

COMMON=(-SILENT -AUTO_SAVE OFF -NO_TIMESTAMP)

echo "== 1/3 Surface irradiance with normals =="
"$CC" "${COMMON[@]}" \
  -O "$HERE/surface_orientation_normals.ply" \
  -SOLIS \
  -COMPONENT DIRECT \
  -OUTPUT_MODE INSTANT \
  -SPATIAL_MODE SURFACE \
  -DOY 355 -TIME 12:00 \
  -LAT 45 -LON 0 -ELV 0 \
  -TIME_MODE UTC \
  -LINKE_TURBIDITY 3 \
  -RESOLUTION 1024 \
  -C_EXPORT_FMT ASC -PREC 12 -SEP SPACE \
  -SAVE_CLOUDS FILE "$OUT/surface.asc" \
  >"$OUT/surface.log" 2>&1

grep -F "SOLIS: created scalar field 'SOLIS Direct [W/m2] D355 T1200 UTC S R1024'" "$OUT/surface.log" >/dev/null || {
  echo "FAIL: expected surface scalar-field name not found in log"
  tail -100 "$OUT/surface.log"
  exit 1
}
python3 "$HERE/verify_results.py" surface "$OUT/surface.asc" "$HERE/expected_results.json"

echo
echo "== 2/3 Missing normals must be rejected =="
set +e
"$CC" "${COMMON[@]}" \
  -O "$HERE/surface_orientation_no_normals.ply" \
  -SOLIS \
  -COMPONENT DIRECT \
  -OUTPUT_MODE INSTANT \
  -SPATIAL_MODE SURFACE \
  -DOY 355 -TIME 12:00 \
  -LAT 45 -LON 0 -ELV 0 \
  -TIME_MODE UTC \
  -LINKE_TURBIDITY 3 \
  -RESOLUTION 1024 \
  >"$OUT/missing_normals.log" 2>&1
RC=$?
set -e

if ! grep -qi "surface irradiance requires existing point/vertex normals" "$OUT/missing_normals.log"; then
  echo "FAIL: missing-normal rejection message not found (exit code $RC)"
  tail -100 "$OUT/missing_normals.log"
  exit 1
fi
if grep -q "SOLIS: created scalar field" "$OUT/missing_normals.log"; then
  echo "FAIL: qSOLIS created a result field despite missing normals"
  exit 1
fi
echo "missing normals rejection: PASS (CloudCompare exit code $RC)"

echo
echo "== 3/3 Frozen wall-shadow regression =="
"$CC" "${COMMON[@]}" \
  -O "$HERE/wall_regression.ply" \
  -SOLIS \
  -COMPONENT DIRECT \
  -OUTPUT_MODE INSTANT \
  -SPATIAL_MODE HORIZONTAL \
  -DOY 172 -TIME 12:00 \
  -LAT 45 -LON 0 -ELV 0 \
  -TIME_MODE UTC \
  -LINKE_TURBIDITY 3 \
  -RESOLUTION 1024 \
  -C_EXPORT_FMT ASC -PREC 12 -SEP SPACE \
  -SAVE_CLOUDS FILE "$OUT/wall.asc" \
  >"$OUT/wall.log" 2>&1

grep -F "SOLIS: created scalar field 'SOLIS Direct [W/m2] D172 T1200 UTC H R1024'" "$OUT/wall.log" >/dev/null || {
  echo "FAIL: expected wall scalar-field name not found in log"
  tail -100 "$OUT/wall.log"
  exit 1
}
python3 "$HERE/verify_results.py" wall "$OUT/wall.asc" "$HERE/expected_results.json"

echo
echo "ALL qSOLIS regression tests PASSED"
echo "Artifacts/logs: $OUT"
