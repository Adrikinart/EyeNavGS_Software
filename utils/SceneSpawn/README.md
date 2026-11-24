# SceneSpawn

`compute_spawn.py` places the participant naturally in a 3DGS scene, in headset mode:

- **level floor:** the SteamVR floor is the floor of the scene, and the scene is not tilted;
- **real-world scale:** 1 m walked in the room is 1 m in the scene;
- **start spot:** the centre of the play area is a free spot next to the photographed content, on its best-reconstructed side. The play area's forward direction, set in SteamVR Room Setup, faces the content.

The participant's real eye height is kept, so a tall person sees the scene from higher up, as in reality.

```powershell
python utils\SceneSpawn\compute_spawn.py <scene> [--units-per-metre U] [--preview-trace preview.csv]
```

It writes `<scene>\eyenavgs_spawn.json`. `scripts\windows\record_vive_pro_eye.ps1` then uses it automatically:

- it passes the values as `--initial-position`, `--initial-quaternion` and `--initial-scale`;
- it copies the file into the session folder.

Requirements: Python 3.8+ with `numpy`.

## How it works

Everything is estimated from `cameras.json` and the trained `point_cloud.ply` (splats with opacity > 0.2).

| What | How |
|---|---|
| Content | The point the training photos look at (least-squares intersection of the optical axes). |
| Floor and up | The lowest near-horizontal plane (RANSAC). It must lie below the cameras and farther out than they are, because a table or object top does not extend there. Its normal is "up". |
| Scale | `--units-per-metre` if you know it. Otherwise a heuristic: the median camera height above the floor is taken as 1.4 m, a handheld capture (`--capture-height` to change it). |
| Start spot | The side with the most training photos, which is the best reconstructed, at the distance the photos were taken from. It is moved back by 5 cm steps until a 25 cm-radius column from 0.15 to 1.9 m is free of splats. |
| Viewer values | Solved from the viewer's own transform, then checked by re-applying that transform. `--self-test` checks the maths on random cases. |

Options:

| Option | Meaning |
|---|---|
| `--units-per-metre U` | Scene units per metre. Measure an object of known size when you can (see below). |
| `--scale-note "..."` | Where that value comes from. It is stored in the JSON. |
| `--face-camera I` | Spawn on the side of camera `I` of `cameras.json`. |
| `--distance M` | Horizontal distance from the spawn to the content, in metres. |
| `--out file.json` | Write somewhere else, e.g. to try values. The launcher's `-SpawnFile` reads any file. |
| `--preview-trace file.csv` | Synthetic trace of a person, eyes at 1.6 m (`--preview-eye-height`), standing at the spawn. They look at the content, then turn around. |

## Check the result without a headset

The desktop replay renders a trace with the same transform as the headset:

```powershell
python utils\SceneSpawn\compute_spawn.py D:\scenes\myscene --preview-trace recordings\myscene_spawn_preview.csv
cd install\bin
.\SIBR_gaussianViewer_app.exe -m D:\scenes\myscene -in ..\..\recordings\myscene_spawn_preview.csv --rendering-mode 4 --rendering-size 720 800
```

The two videos (`...left.mp4`, `...right.mp4`) are written next to the CSV. Check that:

- the floor is level when turning around, and walls and window frames are vertical;
- the content is at a plausible height and distance;
- the furniture has a plausible size.

The headset field of view is about 105°, so everything looks farther away on a flat screen than in the headset.

## Measuring the scale

COLMAP reconstructions have no real-world unit. Several measurements that agree are better than one:

- **An object of known size.** Locate it in two or more training images. Measure it between 3D points on the splats, for example rays through its corners intersected with a plane fitted to the splats of one face.
- **Standard furniture.** Dining tables are 0.72–0.76 m high and bar tables 1.0–1.1 m. Doors are about 2.0–2.1 m.
- **The camera height** of a handheld capture is usually 1.2–1.6 m. This is the tool's default heuristic.

### Example: `figurines_splatting` (LERF *figurines*)

| Cue | Scene units | Real size | Units per metre |
|---|---|---|---|
| Rubik's cube edge (red and yellow faces, photo `frame_00061.jpg`) | 0.35–0.36 | 5.7 cm | 6.2–6.4 |
| Round table: height above the floor | 6.68 | bar table, about 1.05 m | about 6.3 |
| Round table: diameter | 5.89 | 0.92 m (36") | 6.4 |
| Median camera height | 8.74 | about 1.4 m (heuristic) | 6.24 |

The tool was run with `--units-per-metre 6.3`, which gives this spawn:

- the floor normal is tilted 33° from the scene's −y axis: without the spawn, the scene is tilted by that much;
- the content is 1.16 m above the floor;
- the start spot is on the side of training photo `frame_00060.jpg`, 0.71 m from the table centre (about 25 cm from its edge), facing it.

The resulting file is `examples/figurines_splatting_spawn.json`.
