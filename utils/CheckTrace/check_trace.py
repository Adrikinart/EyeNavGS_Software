# SPDX-FileCopyrightText: 2025 Systems and Multimedia Lab @ Rutgers University
# SPDX-License-Identifier: Apache-2.0
#
# This file is part of the EyeNavGS project.
# See LICENSE_EYENAVGS.md and NOTICE for details.
"""
Sanity-check an EyeNavGS headset trace (CSV written by the OpenXR rendering mode).

It only uses the Python standard library (matplotlib is optional, for --plot).

Checks performed
  * header / column count of every row
  * left/right rows alternate (ViewIndex 0 then 1)
  * recording duration and frame rate (from elapsed_ms)
  * head quaternion and gaze quaternion norms
  * eye-tracking validity: percentage of rows with a valid gaze
    (GazeValid column when present, otherwise "gaze quaternion != identity")
  * gaze eccentricity: angle between the head forward axis and the gaze axis
    (both use the OpenXR convention: -Z is forward). With a working eye tracker this is
    typically a few degrees to ~30 degrees; it is exactly 0 when no gaze data is recorded.
  * distance between the gaze origin and the eye/head position (in cm: trace positions are divided by the
    scene units per metre of the eyenavgs_spawn.json found next to the trace, or of --units-per-metre)

Usage
  python check_trace.py <trace.csv> [--plot] [--units-per-metre U]

Exit code: 0 if the trace contains valid eye-tracking data, 1 if it does not, 2 on format errors.
"""
import argparse
import csv
import json
import math
import os
import statistics
import sys

REQUIRED = [
    "ViewIndex", "FOV1", "FOV2", "FOV3", "FOV4",
    "PositionX", "PositionY", "PositionZ",
    "QuaternionX", "QuaternionY", "QuaternionZ", "QuaternionW",
    "GazeQX", "GazeQY", "GazeQZ", "GazeQW",
    "GazePosX", "GazePosY", "GazePosZ",
]


def quat_rotate(qx, qy, qz, qw, vx, vy, vz):
    """Rotate vector v by unit quaternion q (x, y, z, w)."""
    # t = 2 * cross(q.xyz, v)
    tx = 2.0 * (qy * vz - qz * vy)
    ty = 2.0 * (qz * vx - qx * vz)
    tz = 2.0 * (qx * vy - qy * vx)
    # v' = v + w * t + cross(q.xyz, t)
    return (vx + qw * tx + (qy * tz - qz * ty),
            vy + qw * ty + (qz * tx - qx * tz),
            vz + qw * tz + (qx * ty - qy * tx))


def angle_deg(a, b):
    dot = sum(x * y for x, y in zip(a, b))
    na = math.sqrt(sum(x * x for x in a))
    nb = math.sqrt(sum(x * x for x in b))
    if na == 0 or nb == 0:
        return float("nan")
    return math.degrees(math.acos(max(-1.0, min(1.0, dot / (na * nb)))))


def pct(n, d):
    return 100.0 * n / d if d else 0.0


def describe(values):
    if not values:
        return "n/a"
    values = sorted(values)
    p = lambda q: values[min(len(values) - 1, int(q * (len(values) - 1)))]
    return (f"median {statistics.median(values):.2f} | p5 {p(0.05):.2f} | p95 {p(0.95):.2f} | "
            f"min {values[0]:.2f} | max {values[-1]:.2f}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv", help="trace CSV recorded in headset mode")
    ap.add_argument("--plot", action="store_true", help="plot gaze eccentricity over time (needs matplotlib)")
    ap.add_argument("--units-per-metre", type=float,
                    help="trace position units per metre (default: from eyenavgs_spawn.json next to the trace, else 1)")
    args = ap.parse_args()

    # Positions are in metres only without scene initialization (--initial-scale 1). The recording script copies the
    # spawn file it used (utils/SceneSpawn) into the session folder.
    units_per_metre, units_source = 1.0, "metres (no eyenavgs_spawn.json next to the trace)"
    spawn_file = os.path.join(os.path.dirname(os.path.abspath(args.csv)), "eyenavgs_spawn.json")
    if args.units_per_metre:
        units_per_metre, units_source = args.units_per_metre, "--units-per-metre"
    elif os.path.isfile(spawn_file):
        try:
            with open(spawn_file, encoding="utf-8") as sf:
                units_per_metre = float(json.load(sf)["units_per_metre"])
            units_source = spawn_file
        except (OSError, ValueError, KeyError, TypeError) as e:
            print(f"WARNING        : cannot read {spawn_file} ({e}); positions assumed in metres")
            units_source = "metres (unreadable eyenavgs_spawn.json)"
    if units_per_metre <= 0:
        print("ERROR: --units-per-metre must be > 0")
        return 2

    with open(args.csv, newline="", encoding="utf-8") as f:
        reader = csv.reader(f)
        try:
            header = next(reader)
        except StopIteration:
            print("ERROR: empty file")
            return 2
        header = [h.strip() for h in header]
        rows = [r for r in reader if r and any(c.strip() for c in r)]

    missing = [c for c in REQUIRED if c not in header]
    if missing:
        print(f"ERROR: missing columns: {missing}")
        print(f"       header is: {header}")
        return 2
    col = {name: i for i, name in enumerate(header)}

    print(f"File           : {args.csv}")
    print(f"Columns        : {len(header)} -> {','.join(header)}")
    print(f"Position units : {units_per_metre:g} per metre ({units_source})")
    bad_len = [i for i, r in enumerate(rows) if len(r) != len(header)]
    if bad_len:
        print(f"WARNING        : {len(bad_len)} row(s) do not have {len(header)} fields "
              f"(first at data row {bad_len[0] + 1}: {len(rows[bad_len[0]])} fields)")
        rows = [r for r in rows if len(r) == len(header)]
    if not rows:
        print("ERROR: no data rows")
        return 2

    f = lambda r, name: float(r[col[name]])
    views = [int(float(r[col["ViewIndex"]])) for r in rows]
    n_left, n_right = views.count(0), views.count(1)
    alternating = all(views[i] == (i % 2) for i in range(len(views)))
    print(f"Rows           : {len(rows)} (left: {n_left}, right: {n_right}, strictly alternating L/R: {alternating})")

    if "elapsed_ms" in col:
        t = [f(r, "elapsed_ms") for r in rows]
        duration_s = (t[-1] - t[0]) / 1000.0
        frames = len(rows) / 2.0
        if duration_s > 0:
            fps = frames / duration_s
            print(f"Duration       : {duration_s:.2f} s -> {fps:.1f} frames/s (one frame = left+right rows)")
            if fps < 45:
                print("WARNING        : low frame rate. One row pair is written per rendered frame, so this is also the "
                      "trace sampling rate. Use a lighter scene or --downscale 2 (or the 'Down scale factor' slider), "
                      "and keep the headset worn (SteamVR throttles a headset in standby).")
        non_monotonic = sum(1 for a, b in zip(t, t[1:]) if b < a)
        if non_monotonic:
            print(f"WARNING        : elapsed_ms decreases {non_monotonic} time(s)")
    if "iso_time" in col:
        print(f"Start / end    : {rows[0][col['iso_time']]}  ->  {rows[-1][col['iso_time']]}")

    head_norms, gaze_norms, ecc, origin_dist, valid_flags = [], [], [], [], []
    has_valid_col = "GazeValid" in col
    for r in rows:
        hq = (f(r, "QuaternionX"), f(r, "QuaternionY"), f(r, "QuaternionZ"), f(r, "QuaternionW"))
        gq = (f(r, "GazeQX"), f(r, "GazeQY"), f(r, "GazeQZ"), f(r, "GazeQW"))
        head_norms.append(math.sqrt(sum(c * c for c in hq)))
        gaze_norms.append(math.sqrt(sum(c * c for c in gq)))
        is_identity = abs(gq[0]) < 1e-6 and abs(gq[1]) < 1e-6 and abs(gq[2]) < 1e-6
        if has_valid_col:
            valid = r[col["GazeValid"]].strip() in ("1", "true", "True")
        else:
            valid = not is_identity
        valid_flags.append(valid)
        if valid:
            head_fwd = quat_rotate(*hq, 0.0, 0.0, -1.0)
            gaze_fwd = quat_rotate(*gq, 0.0, 0.0, -1.0)
            ecc.append(angle_deg(head_fwd, gaze_fwd))
            hp = (f(r, "PositionX"), f(r, "PositionY"), f(r, "PositionZ"))
            gp = (f(r, "GazePosX"), f(r, "GazePosY"), f(r, "GazePosZ"))
            origin_dist.append(math.dist(hp, gp) * 100.0 / units_per_metre)

    print(f"|head quat|    : {describe(head_norms)}")
    print(f"|gaze quat|    : {describe(gaze_norms)}")
    n_valid = sum(valid_flags)
    src = ""
    if "GazeSource" in col:
        src = " | source(s): " + ", ".join(sorted({r[col['GazeSource']] for r in rows}))
    print(f"Valid gaze     : {n_valid}/{len(rows)} rows ({pct(n_valid, len(rows)):.1f} %)"
          f"{' [GazeValid column]' if has_valid_col else ' [inferred: gaze quaternion != identity]'}{src}")
    if ecc:
        print(f"Gaze vs head   : angle (deg) {describe(ecc)}")
        print(f"Gaze origin    : distance to eye position (cm) {describe(origin_dist)}")

    # Extra eye data (XR_HTC_eye_tracker only; 'nan' otherwise)
    def numeric(name):
        out = []
        if name in col:
            for r in rows:
                try:
                    v = float(r[col[name]])
                except ValueError:
                    continue
                if not math.isnan(v):
                    out.append(v)
        return out
    for name, unit in (("PupilDiameter_mm", "mm"), ("EyeOpenness", "[0-1]")):
        if name in col:
            values = numeric(name)
            print(f"{name:<15}: {len(values)}/{len(rows)} rows ({pct(len(values), len(rows)):.1f} %) "
                  f"{unit} {describe(values) if values else ''}")

    if args.plot:
        try:
            import matplotlib.pyplot as plt
        except ImportError:
            print("matplotlib is not installed: skipping --plot")
        else:
            xs = [f(r, "elapsed_ms") / 1000.0 for r, v in zip(rows, valid_flags) if v] if "elapsed_ms" in col else list(range(len(ecc)))
            plt.figure(figsize=(10, 4))
            plt.plot(xs, ecc, ".", markersize=2)
            plt.xlabel("time (s)")
            plt.ylabel("gaze eccentricity (deg)")
            plt.title(args.csv)
            plt.tight_layout()
            plt.show()

    if n_valid == 0:
        print("\nRESULT: NO valid eye-tracking sample in this trace "
              "(check eye-tracker setup / calibration, see the tutorial troubleshooting section).")
        return 1
    print("\nRESULT: OK, the trace contains eye-tracking data.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, IndexError, csv.Error) as error:
        # unreadable file or malformed values: a format error, not "no valid gaze"
        print(f"ERROR: {error}")
        sys.exit(2)
