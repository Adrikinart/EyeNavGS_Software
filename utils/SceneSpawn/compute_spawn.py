#!/usr/bin/env python3
"""
Natural VR spawn for a 3DGS scene: level floor, real-world scale, start position next to the content.

Computes the viewer's --initial-position / --initial-quaternion / --initial-scale (headset mode) so that:
  * the SteamVR floor (STAGE y = 0) is the floor of the scene, and the scene is level (no tilt);
  * 1 m walked in the room is 1 m in the scene (scene units per metre: known value, or a capture-height heuristic);
  * the centre of the play area is a free spot next to the content, at the distance the training photos were
    taken from, and the play area's forward direction (SteamVR Room Setup) faces the content.
The user's real eye height is kept: a tall participant sees the scene from higher up, like in reality.

Writes <scene>/eyenavgs_spawn.json, which scripts/windows/record_vive_pro_eye.ps1 uses automatically.
Optionally writes a synthetic trace (--preview-trace) of a person standing at the spawn, to check the result
without a headset with the desktop replay (--rendering-mode 4).

Requires numpy. Usage:
  python compute_spawn.py <scene> [--units-per-metre U] [--face-camera I] [--distance M] [--preview-trace out.csv]
  python compute_spawn.py --self-test
"""
import argparse
import datetime
import json
import math
import os
import re
import sys

import numpy as np

# Flip applied by the renderer ("Y-Invert scene", on by default) and by the desktop replay:
# OpenXR (+y up, -z forward) -> 3DGS/COLMAP (+y down, +z forward).
F = np.diag([1.0, -1.0, -1.0])

# Default VIVE Pro Eye per-eye field of view (radians: left, right, down, up), from a real trace.
VIVE_PRO_EYE_FOV = ((-0.9363585, 0.8817525, -0.9604573, 0.9605297),
                    (-0.8808923, 0.9351698, -0.9587100, 0.9604700))

TRACE_HEADER = ("ViewIndex,FOV1,FOV2,FOV3,FOV4,PositionX,PositionY,PositionZ,QuaternionX,QuaternionY,QuaternionZ,"
                "QuaternionW,GazeQX,GazeQY,GazeQZ,GazeQW,GazePosX,GazePosY,GazePosZ,timestamp,realtime,elapsed_ms,"
                "iso_time,epoch_ms,GazeValid,GazeTracked,GazeSource,xr_display_time_ns,gaze_sample_time_ns,"
                "PupilDiameter_mm,PupilPosX,PupilPosY,EyeOpenness,timestep")


# --------------------------------------------------------------------------------------------------------------
# Viewer transform (src/core/openxr/OpenXRHMD.cpp + OpenXRRdrMode.cpp)
#
#   play space : p1 = Ra^T (p - ta),  R1 = Ra^T R     ta, Ra = --initial-position / --initial-quaternion, used as
#                                                       poseInReferenceSpace of the play space (xrCreateReferenceSpace)
#   scale      : p2 = ta + s (p1 - ta)                  s = --initial-scale (OpenXRHMD::applyInitialScale).
#                                                       (p2, R1) is what the trace file records.
#   scene      : p3 = F p2,  R3 = F R1                  F = diag(1, -1, -1)
#
# so p3 = s (F Ra^T) p + F ((1 - s) I - s Ra^T) ta. For a wanted rotation M = F Ra^T (STAGE axes -> scene axes) and
# floor point g = p3(STAGE origin):  Ra = M^T F,  ta = ((1 - s) I - s Ra^T)^-1 F g   (singular only for s = 0.5).
# --------------------------------------------------------------------------------------------------------------
def stage_to_trace(p, R, ta, Ra, s):
    """STAGE pose -> pose recorded in the trace (before the renderer's flip)."""
    p1 = Ra.T @ (np.asarray(p, float) - ta)
    return ta + s * (p1 - ta), Ra.T @ R


def stage_to_scene(p, R, ta, Ra, s):
    p2, R1 = stage_to_trace(p, R, ta, Ra, s)
    return F @ p2, F @ R1


def solve_initial_pose(M, g, s):
    """Viewer initial pose (ta, Ra) for scene rotation M (columns: STAGE x, y, z axes in the scene) and floor point g."""
    if abs(s - 0.5) < 1e-3:
        raise ValueError("--initial-scale 0.5 cannot express a translation (viewer formula); use e.g. 0.501")
    Ra = M.T @ F
    ta = np.linalg.solve((1.0 - s) * np.eye(3) - s * Ra.T, F @ g)
    return ta, Ra


def quat_xyzw_from_matrix(R):
    """Rotation matrix -> unit quaternion (x, y, z, w), w >= 0."""
    m = R
    tr = m[0, 0] + m[1, 1] + m[2, 2]
    if tr > 0:
        k = 2.0 * math.sqrt(tr + 1.0)
        q = [(m[2, 1] - m[1, 2]) / k, (m[0, 2] - m[2, 0]) / k, (m[1, 0] - m[0, 1]) / k, 0.25 * k]
    elif m[0, 0] > m[1, 1] and m[0, 0] > m[2, 2]:
        k = 2.0 * math.sqrt(1.0 + m[0, 0] - m[1, 1] - m[2, 2])
        q = [0.25 * k, (m[0, 1] + m[1, 0]) / k, (m[0, 2] + m[2, 0]) / k, (m[2, 1] - m[1, 2]) / k]
    elif m[1, 1] > m[2, 2]:
        k = 2.0 * math.sqrt(1.0 + m[1, 1] - m[0, 0] - m[2, 2])
        q = [(m[0, 1] + m[1, 0]) / k, 0.25 * k, (m[1, 2] + m[2, 1]) / k, (m[0, 2] - m[2, 0]) / k]
    else:
        k = 2.0 * math.sqrt(1.0 + m[2, 2] - m[0, 0] - m[1, 1])
        q = [(m[0, 2] + m[2, 0]) / k, (m[1, 2] + m[2, 1]) / k, 0.25 * k, (m[1, 0] - m[0, 1]) / k]
    q = np.array(q)
    q /= np.linalg.norm(q)
    return q if q[3] >= 0 else -q


def matrix_from_quat_xyzw(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def stage_axes_in_scene(up, forward):
    """Rotation M whose columns are the STAGE +x (right), +y (up), +z (backward) axes expressed in the scene."""
    up = up / np.linalg.norm(up)
    forward = forward - (forward @ up) * up
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, up)
    return np.column_stack([right, up, -forward])


def yaw_pitch_matrix(yaw, pitch):
    """STAGE head orientation: yaw (left positive, around +y), then pitch (up positive)."""
    cy, sy, cp, sp = math.cos(yaw), math.sin(yaw), math.cos(pitch), math.sin(pitch)
    Ry = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]])
    Rx = np.array([[1, 0, 0], [0, cp, -sp], [0, sp, cp]])
    return Ry @ Rx


# --------------------------------------------------------------------------------------------------------------
# Scene loading
# --------------------------------------------------------------------------------------------------------------
_PLY_TYPES = {"char": "i1", "uchar": "u1", "int8": "i1", "uint8": "u1", "short": "i2", "ushort": "u2",
              "int16": "i2", "uint16": "u2", "int": "i4", "uint": "u4", "int32": "i4", "uint32": "u4",
              "float": "f4", "float32": "f4", "double": "f8", "float64": "f8"}


def read_ply_vertices(path, fields):
    """Read some vertex properties of a binary little-endian PLY (memory-mapped: large 3DGS files are fine)."""
    with open(path, "rb") as f:
        header = []
        while True:
            line = f.readline()
            if not line:
                raise ValueError(f"{path}: no end_header")
            header.append(line.decode("ascii", "replace").strip())
            if header[-1] == "end_header":
                break
        offset = f.tell()
    if "format binary_little_endian 1.0" not in header:
        raise ValueError(f"{path}: only binary little-endian PLY files are supported")
    props, count, state = [], None, "start"
    for line in header:
        t = line.split()
        if t[:1] == ["element"]:
            if t[1] == "vertex" and state == "start":
                count, state = int(t[2]), "vertex"
            elif state == "vertex":
                state = "after"  # other elements after the vertices do not matter
            else:
                raise ValueError(f"{path}: the vertex element must come first")
        elif t[:1] == ["property"] and state == "vertex":
            if t[1] == "list" or t[1] not in _PLY_TYPES:
                raise ValueError(f"{path}: unsupported vertex property '{line}'")
            props.append((t[2], "<" + _PLY_TYPES[t[1]]))
    data = np.memmap(path, dtype=np.dtype(props), mode="r", offset=offset, shape=(count,))
    return {k: np.asarray(data[k], dtype=np.float64) for k in fields if k in data.dtype.names}


def find_point_cloud(scene, iteration=None):
    base = os.path.join(scene, "point_cloud")
    if iteration is None:
        its = [int(m.group(1)) for d in (os.listdir(base) if os.path.isdir(base) else [])
               for m in [re.fullmatch(r"iteration_(\d+)", d)] if m]
        if not its:
            raise FileNotFoundError(f"no point_cloud/iteration_* folder in {scene}")
        iteration = max(its)
    path = os.path.join(base, f"iteration_{iteration}", "point_cloud.ply")
    if not os.path.isfile(path):
        raise FileNotFoundError(path)
    return path


def load_scene(scene, iteration=None, max_points=2_000_000, seed=0):
    cams = json.load(open(os.path.join(scene, "cameras.json"), encoding="utf-8"))
    C = np.array([c["position"] for c in cams], float)
    R = np.array([c["rotation"] for c in cams], float)  # camera-to-world (columns: camera x, y (down), z (forward))
    ply = find_point_cloud(scene, iteration)
    v = read_ply_vertices(ply, ["x", "y", "z", "opacity"])
    P = np.column_stack([v["x"], v["y"], v["z"]])
    if "opacity" in v:
        P = P[1.0 / (1.0 + np.exp(-v["opacity"])) > 0.2]  # drop transparent splats (floaters)
    P = P[np.isfinite(P).all(1)]
    if len(P) > max_points:
        P = P[np.random.default_rng(seed).choice(len(P), max_points, replace=False)]
    return cams, C, R, P, ply


# --------------------------------------------------------------------------------------------------------------
# Geometry
# --------------------------------------------------------------------------------------------------------------
def horizontal_frame(up):
    a = np.array([0.0, 0.0, 1.0]) if abs(up[2]) < 0.9 else np.array([1.0, 0.0, 0.0])
    e1 = np.cross(up, a)
    e1 /= np.linalg.norm(e1)
    return e1, np.cross(up, e1)


def look_at_center(C, R):
    """Least-squares closest point to all optical axes (what the photos look at)."""
    A, b = np.zeros((3, 3)), np.zeros(3)
    for c, d in zip(C, R[:, :, 2]):
        M = np.eye(3) - np.outer(d, d)
        A += M
        b += M @ c
    return np.linalg.solve(A, b)


def ransac_plane(Q, up_hint, max_deg, thr, rng, iters=2500):
    """Near-horizontal plane with most inliers. Returns (normal oriented like up_hint, offset, inlier mask)."""
    if len(Q) < 50:
        return None
    S = Q if len(Q) <= 150_000 else Q[rng.choice(len(Q), 150_000, replace=False)]
    cos_max = math.cos(math.radians(max_deg))
    best_n, best_d, best_count = None, None, -1
    for _ in range(iters):
        a, b, c = S[rng.choice(len(S), 3, replace=False)]
        n = np.cross(b - a, c - a)
        ln = np.linalg.norm(n)
        if ln < 1e-12:
            continue
        n /= ln
        if n @ up_hint < 0:
            n = -n
        if n @ up_hint < cos_max:
            continue
        d = n @ a
        count = int((np.abs(S @ n - d) < thr).sum())
        if count > best_count:
            best_n, best_d, best_count = n, d, count
    if best_n is None:
        return None
    inl = np.abs(Q @ best_n - best_d) < thr
    for _ in range(4):  # least-squares refinement
        X = Q[inl]
        if len(X) < 3:
            return None
        c = X.mean(0)
        n = np.linalg.svd(X - c, full_matrices=False)[2][-1]
        best_n = n if n @ up_hint > 0 else -n
        best_d = best_n @ c
        inl = np.abs(Q @ best_n - best_d) < thr
    return best_n, best_d, inl


def find_floor(P, C, up0, target, rng, log):
    """Floor = lowest near-horizontal plane, with real support, below the cameras.
    Candidates are the splats below (almost) all cameras and farther from the content than the cameras: the floor
    extends there, while the photographed table/object (many more opaque splats than a carpet) does not."""
    e1, e2 = horizontal_frame(up0)
    cam_r = np.hypot((C - target) @ e1, (C - target) @ e2)
    ring = max(float(np.median(cam_r)), 1e-6)
    size = float(np.median(np.linalg.norm(C - target, axis=1)))
    thr = 0.01 * size
    h = P @ up0
    r = np.hypot((P - target) @ e1, (P - target) @ e2)
    cand = P[(h < np.percentile(C @ up0, 2) - 0.05 * size) & (r > ring) & (r < 4.0 * ring)]
    log(f"    {len(cand)} candidate splats (below the cameras, 1-4x the camera distance from the content)")
    planes, remaining = [], cand
    for _ in range(8):
        res = ransac_plane(remaining, up0, 15.0, thr, rng)
        if res is None:
            break
        n, d, inl = res
        if inl.sum() < max(200, 0.004 * len(cand)):
            break
        X = remaining[inl]
        ext = float(np.percentile(np.hypot((X - target) @ e1, (X - target) @ e2), 90))
        height = float(d - n @ target)  # signed offset of the plane below/above the target, along its normal
        planes.append({"normal": n, "d": float(d), "count": int(inl.sum()), "extent": ext, "height": height})
        remaining = remaining[~inl]
    if not planes:
        raise RuntimeError("no horizontal plane found below the cameras: give the floor manually is not supported")
    biggest = max(p["count"] for p in planes)
    for p in planes:
        log(f"    plane: {p['count']:7d} splats, {p['height']:+8.3f} units from the target along its normal, "
            f"extent p90 {p['extent']:.2f} (camera ring {ring:.2f}), tilt vs camera-up "
            f"{math.degrees(math.acos(min(1.0, p['normal'] @ up0))):.1f} deg")
    ok = [p for p in planes if p["count"] >= 0.1 * biggest and p["extent"] >= 1.0 * ring]
    if not ok:
        log("    WARNING: no plane extends beyond the cameras; using the largest one as the floor")
        ok = [max(planes, key=lambda p: p["count"])]
    floor = min(ok, key=lambda p: p["height"])
    # final refit with every splat near that plane
    res = ransac_plane(P[np.abs(P @ floor["normal"] - floor["d"]) < 3 * thr], floor["normal"], 5.0, thr, rng, iters=300)
    if res is not None:
        floor["normal"], floor["d"], inl = res[0], res[1], res[2]
    return floor, thr, ring


def camera_azimuth_density(az, step_deg=5, window_deg=30):
    """Number of cameras within +-window/2 of each candidate azimuth."""
    cand = np.radians(np.arange(0, 360, step_deg))
    diff = np.abs((az[None, :] - cand[:, None] + np.pi) % (2 * np.pi) - np.pi)
    return cand, (diff < math.radians(window_deg / 2)).sum(1)


# --------------------------------------------------------------------------------------------------------------
def compute(args, log=print):
    rng = np.random.default_rng(args.seed)
    cams, C, R, P, ply = load_scene(args.scene, args.iteration, seed=args.seed)
    log(f"Scene        : {os.path.abspath(args.scene)}")
    log(f"Point cloud  : {ply} ({len(P)} opaque splats used), {len(cams)} cameras")

    up_cam = -R[:, :, 1].mean(0)
    up_cam /= np.linalg.norm(up_cam)
    target = look_at_center(C, R)

    log("Floor (horizontal planes below the cameras):")
    floor, thr, ring = find_floor(P, C, up_cam, target, rng, log)
    up = floor["normal"]
    d_floor = floor["d"]
    tilt_default = math.degrees(math.acos(min(1.0, up @ np.array([0.0, -1.0, 0.0]))))
    log(f"  up (floor normal) = {np.round(up, 5).tolist()}; tilt of the uncorrected scene: {tilt_default:.1f} deg")

    cam_h = C @ up - d_floor
    if np.median(cam_h) <= 0:
        raise RuntimeError("cameras are below the detected floor: floor detection failed")
    if args.units_per_metre:
        upm = args.units_per_metre
        scale_source = args.scale_note or "given (--units-per-metre)"
    else:
        upm = float(np.median(cam_h)) / args.capture_height
        scale_source = (f"heuristic: median camera height above the floor = {args.capture_height} m "
                        f"(handheld capture); check it with --preview-trace")
    log(f"Scale        : {upm:.4f} scene units per metre ({scale_source})")
    log(f"  cameras above the floor: p5 {np.percentile(cam_h, 5) / upm:.2f} m, median "
        f"{np.median(cam_h) / upm:.2f} m, p95 {np.percentile(cam_h, 95) / upm:.2f} m")
    tgt_h = float(target @ up - d_floor)
    log(f"  photographed content (cameras' look-at point) {tgt_h / upm:.2f} m above the floor")

    # Spawn: on the side with most photos (best reconstructed), facing the content, at the distance the photos were
    # taken from, moved back until the person's standing column (radius 25 cm, 0.15-1.9 m) is free of splats
    # (table, chair, ...).
    e1, e2 = horizontal_frame(up)
    tg = target - (target @ up - d_floor) * up  # target projected on the floor
    rel = C - tg
    az = np.arctan2(rel @ e2, rel @ e1)
    cam_dist = np.hypot(rel @ e1, rel @ e2)
    base_u = args.distance * upm if args.distance else float(np.median(cam_dist))
    steps = [0.0] if args.distance else [0.05 * k for k in range(21)]  # up to 1 m farther, by 5 cm
    Ph = P @ up - d_floor
    xy = np.column_stack([(P - tg) @ e1, (P - tg) @ e2])
    obst = xy[(Ph > 0.15 * upm) & (Ph < 1.9 * upm) & (np.hypot(xy[:, 0], xy[:, 1]) < base_u + 1.3 * upm)]

    def blocking(d_u, a):
        return int((np.hypot(obst[:, 0] - d_u * math.cos(a), obst[:, 1] - d_u * math.sin(a)) < 0.25 * upm).sum())

    if args.face_camera is not None:
        if not 0 <= args.face_camera < len(cams):
            raise ValueError(f"--face-camera must be in 0..{len(cams) - 1}")
        order = [float(az[args.face_camera])]
        why = f"side of camera {args.face_camera} ({cams[args.face_camera].get('img_name', '?')})"
    else:
        cand, dens = camera_azimuth_density(az)
        order = [float(a) for a in cand[np.argsort(-dens, kind="stable")]]
        why = "side with the most training photos"
    chosen = None
    for a in order:
        for st in steps:
            d_u = base_u + st * upm
            n_block = blocking(d_u, a)
            if n_block <= args.max_obstacle_splats:
                chosen = (a, d_u, n_block)
                break
        if chosen:
            break
    if chosen is None:
        a = order[0]
        chosen = (a, base_u, blocking(base_u, a))
        log(f"  WARNING: no free standing spot found ({chosen[2]} splats in the column); spawning there anyway")
    a, dist_u, n_block = chosen
    g = tg + dist_u * (math.cos(a) * e1 + math.sin(a) * e2)
    forward = tg - g
    M = stage_axes_in_scene(up, forward)
    s = upm
    ta, Ra = solve_initial_pose(M, g, s)
    q = quat_xyzw_from_matrix(Ra)
    nearest = int(np.argmin(np.abs((az - a + np.pi) % (2 * np.pi) - np.pi)))
    log(f"Spawn        : {why}, facing the content, {dist_u / upm:.2f} m from it horizontally "
        f"(photos taken from {base_u / upm:.2f} m); {n_block} splats in the standing column")
    log(f"  closest training photo direction: {cams[nearest].get('img_name', nearest)} (camera index {nearest})")

    # Values as passed to the viewer (text), and self-check of the viewer formulas with exactly these values
    fmt = lambda x, nd: f"{x:.{nd}f}".rstrip("0").rstrip(".") if abs(x) >= 0.5 * 10 ** -nd else "0"
    pos_txt = [fmt(x, 6) for x in ta]
    q_txt = [fmt(x, 7) for x in q]
    s_txt = fmt(s, 6)
    ta_r = np.array([float(x) for x in pos_txt])
    Rq = matrix_from_quat_xyzw(np.array([float(x) for x in q_txt]))
    s = float(s_txt)
    p0, R0 = stage_to_scene([0, 0, 0], np.eye(3), ta_r, Rq, s)
    p1, _ = stage_to_scene([0, 1, 0], np.eye(3), ta_r, Rq, s)
    fwd_scene = R0 @ np.array([0, 0, -1.0])
    checks = {
        "stage_origin_to_floor_point_error_units": float(np.linalg.norm(p0 - g)),
        "one_metre_up_in_units": float(np.linalg.norm(p1 - p0)),
        "stage_up_vs_floor_normal_deg": float(math.degrees(math.acos(np.clip(((p1 - p0) / np.linalg.norm(p1 - p0)) @ up, -1, 1)))),
        "stage_forward_vs_content_deg": float(math.degrees(math.acos(np.clip(fwd_scene @ (forward / np.linalg.norm(forward)), -1, 1)))),
    }
    if (checks["stage_origin_to_floor_point_error_units"] > 1e-3 * upm or checks["stage_up_vs_floor_normal_deg"] > 0.05
            or checks["stage_forward_vs_content_deg"] > 0.05 or abs(checks["one_metre_up_in_units"] - upm) > 1e-3 * upm):
        raise RuntimeError(f"internal check failed: {checks}")

    result = {
        "scene": os.path.basename(os.path.normpath(os.path.abspath(args.scene))),
        "generated_by": "utils/SceneSpawn/compute_spawn.py",
        "generated_on": datetime.date.today().isoformat(),
        "viewer_args": ["--initial-position", *pos_txt, "--initial-quaternion", *q_txt, "--initial-scale", s_txt],
        "launcher_args": f'-InitialPosition "{" ".join(pos_txt)}" -InitialQuaternion "{" ".join(q_txt)}" -InitialScale {s_txt}',
        "initial_position": [float(x) for x in pos_txt],
        "initial_quaternion_xyzw": [float(x) for x in q_txt],
        "initial_scale": float(s_txt),
        "units_per_metre": upm,
        "scale_source": scale_source,
        "scene_frame": {
            "up": up.tolist(),
            "floor_point_under_spawn": g.tolist(),
            "facing": (forward / np.linalg.norm(forward)).tolist(),
            "content_point": target.tolist(),
        },
        "metres": {
            "spawn_to_content_horizontal": dist_u / upm,
            "content_above_floor": tgt_h / upm,
            "camera_height_above_floor_median": float(np.median(cam_h) / upm),
        },
        "tilt_of_uncorrected_scene_deg": tilt_default,
        "nearest_training_camera": {"index": nearest, "img_name": cams[nearest].get("img_name")},
        "checks": checks,
        "usage": "Stand in the middle of the SteamVR play area, facing its forward direction (Room Setup).",
    }
    return result, (ta_r, Rq, s), {"up": up, "target": target, "g": g, "d_floor": d_floor, "upm": upm}


def write_preview_trace(path, ta, Ra, s, geo, eye_height, ipd=0.064, fps=30):
    """Synthetic trace (34 columns) of a person standing at the spawn: looks at the content, then turns around."""
    upm, target, g, up = geo["upm"], geo["target"], geo["g"], geo["up"]
    # content seen from the eyes: horizontal distance and drop, in metres
    horiz = np.linalg.norm((target - g) - ((target - g) @ up) * up) / upm
    drop = eye_height - (target @ up - geo["d_floor"]) / upm
    pitch_content = -math.atan2(drop, horiz)
    poses = [(0.0, pitch_content)] * 15
    poses += [(math.radians(a), pitch_content) for a in np.linspace(0, 30, 10)]  # glance left
    poses += [(math.radians(a), math.radians(-5)) for a in np.linspace(0, 360, 73)]  # turn around, nearly level
    rows = [TRACE_HEADER]
    t0 = datetime.datetime(2026, 1, 1, 12, 0, 0)
    for i, (yaw, pitch) in enumerate(poses):
        Rh = yaw_pitch_matrix(yaw, pitch)
        for eye in (0, 1):
            p_eye = np.array([0.0, eye_height, 0.0]) + Rh @ np.array([(-0.5 if eye == 0 else 0.5) * ipd, 0, 0])
            p2, R1 = stage_to_trace(p_eye, Rh, ta, Ra, s)
            qx, qy, qz, qw = quat_xyzw_from_matrix(R1)
            ms = i * 1000.0 / fps
            t = t0 + datetime.timedelta(milliseconds=ms)
            fov = VIVE_PRO_EYE_FOV[eye]
            rows.append(",".join([
                str(eye), *(f"{x:.7g}" for x in fov), *(f"{x:.7g}" for x in p2), *(f"{x:.7g}" for x in (qx, qy, qz, qw)),
                "0", "0", "0", "1", "0", "0", "0",
                str(int(ms * 1000)), t.strftime("%Y-%m-%d %H:%M:%S.%f")[:-3], f"{ms:.3f}",
                t.strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3], f"{ms:.3f}",
                "0", "0", "preview", "0", "0", "nan", "nan", "nan", "nan", str(int(round(ms)))]))
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(rows) + "\n")
    return len(poses), math.degrees(pitch_content)


# --------------------------------------------------------------------------------------------------------------
def self_test():
    """Check the inverse formulas against the forward viewer model on random cases (no scene needed)."""
    rng = np.random.default_rng(42)
    for _ in range(200):
        up = rng.normal(size=3)
        up /= np.linalg.norm(up)
        fwd = rng.normal(size=3)
        M = stage_axes_in_scene(up, fwd)
        assert np.allclose(M.T @ M, np.eye(3), atol=1e-9) and np.linalg.det(M) > 0
        g = rng.normal(size=3) * 5
        s = float(rng.choice([rng.uniform(0.05, 0.45), rng.uniform(0.55, 20)]))
        ta, Ra = solve_initial_pose(M, g, s)
        Ra = matrix_from_quat_xyzw(quat_xyzw_from_matrix(Ra))
        p = rng.normal(size=3)
        R = yaw_pitch_matrix(rng.uniform(-3, 3), rng.uniform(-1, 1))
        p3, R3 = stage_to_scene(p, R, ta, Ra, s)
        assert np.allclose(p3, g + s * (M @ p), atol=1e-6), "position"
        assert np.allclose(R3, M @ R, atol=1e-6), "orientation"
        # trace -> STAGE metres, as documented in tutorial/06_trace_format.md
        p2, R1 = stage_to_trace(p, R, ta, Ra, s)
        assert np.allclose(Ra @ (ta + (p2 - ta) / s) + ta, p, atol=1e-6), "inverse position"
        assert np.allclose(Ra @ R1, R, atol=1e-6), "inverse orientation"
    print("self-test OK (200 random spawns: viewer transform reproduces floor point, up, forward and scale; "
          "trace -> STAGE inverse)")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("scene", nargs="?", help="3DGS model folder (cameras.json, point_cloud/)")
    ap.add_argument("--units-per-metre", type=float, help="scene units per metre, if known (e.g. from an object of known size)")
    ap.add_argument("--scale-note", help="where --units-per-metre comes from (stored in the JSON)")
    ap.add_argument("--capture-height", type=float, default=1.4,
                    help="heuristic when --units-per-metre is not given: median height of the capture camera above "
                         "the floor, in metres (default 1.4, handheld)")
    ap.add_argument("--face-camera", type=int, help="spawn on the side of this camera index of cameras.json")
    ap.add_argument("--distance", type=float, help="horizontal distance spawn -> content in metres "
                                                   "(default: median distance of the training cameras)")
    ap.add_argument("--max-obstacle-splats", type=int, default=50,
                    help="splats allowed in the standing column (radius 25 cm, 0.15-1.9 m) of the spawn spot")
    ap.add_argument("--iteration", type=int, help="point_cloud/iteration_N to analyse (default: highest)")
    ap.add_argument("--out", help="output JSON (default: <scene>/eyenavgs_spawn.json)")
    ap.add_argument("--preview-trace", help="also write a synthetic trace for --rendering-mode 4")
    ap.add_argument("--preview-eye-height", type=float, default=1.6, help="eye height of the preview person (m)")
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--self-test", action="store_true", help="check the transform maths and exit")
    args = ap.parse_args()
    if args.self_test:
        self_test()
        return 0
    if not args.scene:
        ap.error("scene folder required")
    result, (ta, Ra, s), geo = compute(args)
    out = args.out or os.path.join(args.scene, "eyenavgs_spawn.json")
    with open(out, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2)
    print(f"\nWritten      : {out}")
    print(f"Viewer       : {' '.join(result['viewer_args'])}")
    print(f"Launcher     : {result['launcher_args']}  (used automatically from {os.path.basename(out)})"
          if not args.out else f"Launcher     : {result['launcher_args']}")
    if args.preview_trace:
        n, pitch = write_preview_trace(args.preview_trace, ta, Ra, s, geo, args.preview_eye_height)
        print(f"Preview trace: {args.preview_trace} ({n} frames; eyes at {args.preview_eye_height} m, looking "
              f"{-pitch:.0f} deg down at the content, then turning around). Render it with:\n"
              f"  SIBR_gaussianViewer_app.exe -m <scene> -in {args.preview_trace} --rendering-mode 4 --rendering-size 720 800")
    return 0


if __name__ == "__main__":
    sys.exit(main())
