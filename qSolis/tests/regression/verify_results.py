#!/usr/bin/env python3
import json, statistics, sys
from pathlib import Path

def rows(path):
    out = []
    for line in Path(path).read_text(errors="replace").splitlines():
        s = line.strip()
        if not s or s.startswith("//") or s.startswith("#"):
            continue
        parts = s.replace(",", " ").split()
        try:
            vals = [float(x) for x in parts]
        except ValueError:
            continue
        if len(vals) >= 4:
            out.append(vals)
    if not out:
        raise RuntimeError(f"No numeric point rows found in {path}")
    return out

def solis_value(row):
    # These regression exports are produced by CloudCompare from a cloud with:
    # X Y Z [active scalar field] [optional Nx Ny Nz].
    #
    # Therefore the qSOLIS result is column 4 (zero-based index 3), NOT the
    # final column when normals are also exported.
    return row[3]

def near(actual, expected, rel):
    return abs(actual - expected) <= rel * max(1.0, abs(expected))

def check_surface(asc_path, expected_path):
    exp = json.loads(Path(expected_path).read_text())["surface_orientation_test"]
    data = rows(asc_path)
    groups = {"HORIZONTAL": [], "SOUTH": [], "NORTH": [], "TILT45_SOUTH": []}

    for r in data:
        x = r[0]
        sf = solis_value(r)
        if x < -10:
            key = "HORIZONTAL"
        elif x < 0:
            key = "SOUTH"
        elif x < 10:
            key = "NORTH"
        else:
            key = "TILT45_SOUTH"
        groups[key].append(sf)

    failed = False
    med = {}
    for key, values in groups.items():
        med[key] = statistics.median(values)
        target = exp["expected_group_w_m2"][key]
        if key == "NORTH":
            ok = abs(med[key]) <= 1e-4
        else:
            ok = near(med[key], target, exp["relative_tolerance"])
        print(
            f"surface {key:12s}: median={med[key]:10.4f} "
            f"expected={target:10.4f} {'PASS' if ok else 'FAIL'}"
        )
        failed |= not ok

    ordering = (
        med["SOUTH"] > 2.0 * med["HORIZONTAL"]
        and med["TILT45_SOUTH"] > 2.0 * med["HORIZONTAL"]
        and abs(med["NORTH"]) < 1e-4
    )
    print(f"surface orientation ordering: {'PASS' if ordering else 'FAIL'}")
    failed |= not ordering
    return 1 if failed else 0

def check_wall(asc_path, expected_path):
    exp = json.loads(Path(expected_path).read_text())["wall_test"]
    data = rows(asc_path)

    ground = [r for r in data if abs(r[2]) < 1e-7]
    shadow = [solis_value(r) for r in ground if -4.0 <= r[0] <= 4.0 and 0.3 <= r[1] <= 1.5]
    sunlit = [solis_value(r) for r in ground if -4.0 <= r[0] <= 4.0 and 4.0 <= r[1] <= 6.0]

    if not shadow or not sunlit:
        print("FAIL: expected wall test sample regions were empty")
        return 1

    shadow_med = statistics.median(shadow)
    sun_med = statistics.median(sunlit)
    target = exp["expected_sunlit_horizontal_w_m2"]
    sun_ok = near(sun_med, target, exp["sunlit_relative_tolerance"])
    shadow_ok = shadow_med <= exp["max_shadow_fraction_of_sunlit"] * max(1e-9, sun_med)

    print(f"wall sunlit median: {sun_med:.4f} expected={target:.4f} {'PASS' if sun_ok else 'FAIL'}")
    print(
        f"wall shadow median: {shadow_med:.4f} "
        f"({100.0*shadow_med/max(1e-9,sun_med):.2f}% of sunlit) "
        f"{'PASS' if shadow_ok else 'FAIL'}"
    )
    print(f"expected geometric shadow length: {exp['expected_shadow_length_m']:.4f} m")
    return 0 if (sun_ok and shadow_ok) else 1

if __name__ == "__main__":
    if len(sys.argv) != 4 or sys.argv[1] not in ("surface", "wall"):
        print("usage: verify_results.py {surface|wall} output.asc expected_results.json", file=sys.stderr)
        raise SystemExit(2)

    if sys.argv[1] == "surface":
        raise SystemExit(check_surface(sys.argv[2], sys.argv[3]))
    raise SystemExit(check_wall(sys.argv[2], sys.argv[3]))
