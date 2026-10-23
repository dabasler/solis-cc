#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 /path/to/CloudCompare /path/to/input.{ply,bin,las,...}" >&2
  exit 2
fi

CC=$1
INPUT=$2

if [[ ! -x "$CC" ]]; then
  echo "CloudCompare executable is not executable: $CC" >&2
  exit 2
fi
if [[ ! -f "$INPUT" ]]; then
  echo "Input file not found: $INPUT" >&2
  exit 2
fi

run() {
  echo "+ $*"
  "$@"
}

echo "== qSOLIS CLI: direct instantaneous =="
run "$CC" -VERBOSITY 1 -SILENT -AUTO_SAVE OFF -O "$INPUT" \
  -SOLIS -COMPONENT DIRECT -OUTPUT_MODE INSTANT -SPATIAL_MODE HORIZONTAL \
  -DOY 172 -TIME 12:00 -LAT 45 -LON 0 -ELV 0 -RESOLUTION 256

echo "== qSOLIS CLI: invalid DOY should fail =="
set +e
"$CC" -VERBOSITY 1 -SILENT -AUTO_SAVE OFF -O "$INPUT" \
  -SOLIS -COMPONENT DIRECT -OUTPUT_MODE INSTANT \
  -DOY 0 -TIME 12:00 -LAT 45 -LON 0 -RESOLUTION 256
rc=$?
set -e
if [[ $rc -eq 0 ]]; then
  echo "ERROR: DOY 0 unexpectedly succeeded" >&2
  exit 1
fi
echo "Invalid DOY correctly failed (exit $rc)."

echo "== qSOLIS CLI: direct + diffuse + total =="
run "$CC" -VERBOSITY 1 -SILENT -AUTO_SAVE OFF -O "$INPUT" \
  -SOLIS -COMPONENT ALL -OUTPUT_MODE ENERGY -ENERGY_UNIT WH \
  -SPATIAL_MODE HORIZONTAL -DOY 172 -TIME 00:00 \
  -DURATION 1 -TIMESTEP 30 -LAT 45 -LON 0 \
  -DIFFUSE_RAYS 16 -RESOLUTION 256

echo "== qSOLIS CLI: batch time series =="
run "$CC" -VERBOSITY 1 -SILENT -AUTO_SAVE OFF -O "$INPUT" \
  -SOLIS -COMPONENT DIRECT -OUTPUT_MODE INSTANT -SPATIAL_MODE HORIZONTAL \
  -DOY 175 -TIME 08:00 -BATCH_END_TIME 10:00 -BATCH_STEP 60 \
  -LAT 45 -LON 0 -RESOLUTION 256

echo "== qSOLIS CLI smoke tests including batch completed =="
