#!/usr/bin/env bash
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CC="${1:-$HOME/dev/CloudCompare/build-dev/qCC/CloudCompare}"
OUT="${2:-$HERE/out-exposure}"
mkdir -p "$OUT"
COMMON=(-SILENT -AUTO_SAVE OFF -NO_TIMESTAMP)

run_open() {
  local metric="$1"
  local expected_name="$2"
  local extra=()
  if [[ "$metric" != "SKY" ]]; then
    extra=(-DOY 172 -TIME 12:00 -DURATION 0 -LAT 45 -LON 0 -ELV 0)
  fi
  local lower="${metric,,}"
  "$CC" "${COMMON[@]}" -O "$HERE/exposure_open_point.ply" \
    -SOLIS_EXPOSURE -METRICS "$metric" "${extra[@]}" \
    -SKY_RAYS 256 -RESOLUTION 1024 \
    -C_EXPORT_FMT ASC -PREC 12 -SEP SPACE \
    -SAVE_CLOUDS FILE "$OUT/open_${lower}.asc" >"$OUT/open_${lower}.log" 2>&1
  grep -F "SOLIS Exposure: created scalar field '$expected_name'" "$OUT/open_${lower}.log" >/dev/null
  "$HERE/verify_exposure.py" open "$OUT/open_${lower}.asc"
}

echo '== Exposure 1/9 Sky Exposure open reference =='
run_open SKY 'SOLIS Sky Exposure [fraction] rays256 R1024'

echo
echo '== Exposure 2/9 Sun Exposure open reference =='
run_open SUN 'SOLIS Sun Exposure [fraction] D172 T1200 UTC R1024'

echo
echo '== Exposure 3/9 Radiative Exposure + Crown Exposure open reference =='
"$CC" "${COMMON[@]}" -O "$HERE/exposure_open_point.ply" \
  -SOLIS_EXPOSURE -METRICS RADIATIVE -DOY 172 -TIME 12:00 -DURATION 0 \
  -LAT 45 -LON 0 -ELV 0 -SKY_RAYS 256 -RESOLUTION 1024 \
  -CROWN_SUMMARY -CROWN_VOXEL_AUTO \
  -C_EXPORT_FMT ASC -PREC 12 -SEP SPACE \
  -SAVE_CLOUDS FILE "$OUT/open_radiative.asc" >"$OUT/open_radiative.log" 2>&1
grep -F "SOLIS Exposure: created scalar field 'SOLIS Radiative Exposure [fraction] D172 T1200 UTC H R1024'" "$OUT/open_radiative.log" >/dev/null
grep -E "SOLIS Crown Exposure .*CEI=1(\.0+)?([, ]|$)" "$OUT/open_radiative.log" >/dev/null
"$HERE/verify_exposure.py" open "$OUT/open_radiative.asc"

echo
echo '== Exposure 4/9 No-daylight Sun Exposure must be rejected =='
set +e
"$CC" "${COMMON[@]}" -O "$HERE/exposure_open_point.ply" \
  -SOLIS_EXPOSURE -METRICS SUN -DOY 172 -TIME 00:00 -DURATION 0 \
  -LAT 45 -LON 0 -ELV 0 -RESOLUTION 1024 >"$OUT/no_daylight.log" 2>&1
NO_DAYLIGHT_STATUS=$?
set -e
grep -i "contains no daylight" "$OUT/no_daylight.log" >/dev/null
if grep -F "SOLIS Exposure: created scalar field" "$OUT/no_daylight.log" >/dev/null; then
  echo "no-daylight rejection created a scalar field" >&2
  exit 1
fi
echo "no-daylight rejection: PASS (CloudCompare exit code $NO_DAYLIGHT_STATUS)"

echo
echo '== Exposure 5/9 Wall direct-sun accessibility =='
"$CC" "${COMMON[@]}" -O "$HERE/wall_regression.ply" \
  -SOLIS_EXPOSURE -METRICS SUN -DOY 172 -TIME 12:00 -DURATION 0 \
  -LAT 45 -LON 0 -ELV 0 -RESOLUTION 1024 \
  -C_EXPORT_FMT ASC -PREC 12 -SEP SPACE \
  -SAVE_CLOUDS FILE "$OUT/wall_sun.asc" >"$OUT/wall_sun.log" 2>&1
"$HERE/verify_exposure.py" wall-sun "$OUT/wall_sun.asc"

echo
echo '== Exposure 6/9 Wall sky openness =='
"$CC" "${COMMON[@]}" -O "$HERE/wall_regression.ply" \
  -SOLIS_EXPOSURE -METRICS SKY -SKY_RAYS 256 -RESOLUTION 1024 \
  -C_EXPORT_FMT ASC -PREC 12 -SEP SPACE \
  -SAVE_CLOUDS FILE "$OUT/wall_sky.asc" >"$OUT/wall_sky.log" 2>&1
"$HERE/verify_exposure.py" wall-sky "$OUT/wall_sky.asc"

echo
echo '== Exposure 7/9 Wall radiative exposure =='
"$CC" "${COMMON[@]}" -O "$HERE/wall_regression.ply" \
  -SOLIS_EXPOSURE -METRICS RADIATIVE -DOY 172 -TIME 12:00 -DURATION 0 \
  -LAT 45 -LON 0 -ELV 0 -SKY_RAYS 256 -RESOLUTION 1024 \
  -C_EXPORT_FMT ASC -PREC 12 -SEP SPACE \
  -SAVE_CLOUDS FILE "$OUT/wall_rei.asc" >"$OUT/wall_rei.log" 2>&1
"$HERE/verify_exposure.py" wall-rei "$OUT/wall_rei.asc"


echo
echo '== Exposure 8/9 Target-only context ignores neighboring loaded wall =='
"$CC" "${COMMON[@]}" \
  -O "$HERE/context_target_ground.ply" \
  -O "$HERE/context_neighbor_wall.ply" \
  -SOLIS_EXPOSURE -METRICS SUN -CONTEXT TARGET -DOY 172 -TIME 12:00 -DURATION 0 \
  -LAT 45 -LON 0 -ELV 0 -RESOLUTION 1024 \
  -C_EXPORT_FMT ASC -PREC 12 -SEP SPACE \
  -SAVE_CLOUDS FILE "$OUT/context_target_only.asc" "$OUT/context_target_only_wall.asc" >"$OUT/context_target_only.log" 2>&1
"$HERE/verify_exposure.py" context-target "$OUT/context_target_only.asc"

echo
echo '== Exposure 9/9 Loaded context lets neighboring wall occlude target =='
"$CC" "${COMMON[@]}" \
  -O "$HERE/context_target_ground.ply" \
  -O "$HERE/context_neighbor_wall.ply" \
  -SOLIS_EXPOSURE -METRICS SUN -CONTEXT LOADED -DOY 172 -TIME 12:00 -DURATION 0 \
  -LAT 45 -LON 0 -ELV 0 -RESOLUTION 1024 \
  -C_EXPORT_FMT ASC -PREC 12 -SEP SPACE \
  -SAVE_CLOUDS FILE "$OUT/context_loaded.asc" "$OUT/context_loaded_wall.asc" >"$OUT/context_loaded.log" 2>&1
grep -F "context=selected-entities" "$OUT/context_loaded.log" >/dev/null
"$HERE/verify_exposure.py" context-loaded "$OUT/context_loaded.asc"

echo
echo 'ALL qSOLIS exposure regression tests PASSED'
echo "Artifacts/logs: $OUT"
