#!/usr/bin/env python3
import argparse
import math
import statistics
import sys


def rows(path):
    output = []
    with open(path, "r", encoding="utf-8", errors="replace") as stream:
        for line in stream:
            line = line.strip()
            if not line or line.startswith("//") or line.startswith("#"):
                continue
            try:
                values = [float(value) for value in line.split()]
            except ValueError:
                continue
            if len(values) >= 4:
                output.append(values)
    return output


def wall_regions(data):
    near_wall = [row[3] for row in data
                 if -4 <= row[0] <= 4 and 0.3 <= row[1] <= 1.5 and abs(row[2]) < 1e-6]
    far_open = [row[3] for row in data
                if -4 <= row[0] <= 4 and 4 <= row[1] <= 6 and abs(row[2]) < 1e-6]
    return near_wall, far_open


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=["open", "wall-sun", "wall-sky", "wall-rei", "context-target", "context-loaded"])
    parser.add_argument("file")
    args = parser.parse_args()

    data = rows(args.file)
    if not data:
        print("no numeric rows", file=sys.stderr)
        return 2

    values = [row[3] for row in data]
    if any((not math.isfinite(value)) or value < -1e-6 or value > 1.0 + 1e-6 for value in values):
        print("exposure values outside [0,1]", file=sys.stderr)
        return 1

    if args.mode == "open":
        value = data[0][3]
        ok = abs(value - 1.0) <= 0.01
        print(f"open exposure: {value:.6f} expected≈1.0 {'PASS' if ok else 'FAIL'}")
        return 0 if ok else 1

    if args.mode in {"context-target", "context-loaded"}:
        near_wall, far_open = wall_regions(data)
        if not near_wall or not far_open:
            print("context sample regions empty", file=sys.stderr)
            return 2
        near_median = statistics.median(near_wall)
        far_median = statistics.median(far_open)
        if args.mode == "context-target":
            ok = near_median >= 0.95 and far_median >= 0.95
            print(f"target-only context near={near_median:.4f} far={far_median:.4f} "
                  f"{'PASS' if ok else 'FAIL'}")
        else:
            ok = near_median <= 0.05 and far_median >= 0.95
            print(f"loaded-neighbor context near={near_median:.4f} far={far_median:.4f} "
                  f"{'PASS' if ok else 'FAIL'}")
        return 0 if ok else 1

    near_wall, far_open = wall_regions(data)
    if not near_wall or not far_open:
        print("wall sample regions empty", file=sys.stderr)
        return 2

    near_median = statistics.median(near_wall)
    far_median = statistics.median(far_open)

    if args.mode == "wall-sun":
        ok = near_median <= 0.05 and far_median >= 0.95
        print(f"wall Sun Exposure shadow median={near_median:.4f} "
              f"sunlit median={far_median:.4f} {'PASS' if ok else 'FAIL'}")
    elif args.mode == "wall-sky":
        # Both samples still see the finite wall, so the farther control is not
        # expected to be an open-sky value of 1.0.  The regression requirement
        # is geometric: moving away from the wall must restore a substantial
        # fraction of the visible upper hemisphere.  Keep broad thresholds so
        # small cross-platform rasterization differences do not cause failures.
        delta = far_median - near_median
        ok = (far_median >= 0.70
              and near_median <= 0.70
              and delta >= 0.15)
        print(f"wall Sky Exposure near={near_median:.4f} far={far_median:.4f} "
              f"delta={delta:.4f} {'PASS' if ok else 'FAIL'}")
    else:
        # At summer noon the north-side shadow loses the direct term, but retains
        # part of the diffuse sky term; far-open ground should remain near 1.
        ok = far_median >= 0.95 and near_median <= far_median - 0.20
        print(f"wall REI shadow={near_median:.4f} open={far_median:.4f} "
              f"{'PASS' if ok else 'FAIL'}")

    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
