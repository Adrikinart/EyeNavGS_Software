# 4. Get a 3DGS scene

The viewer loads a standard **3D Gaussian Splatting output folder**, as produced by the INRIA [`gaussian-splatting`](https://github.com/graphdeco-inria/gaussian-splatting) training code:

```
<scene>\
  cfg_args                                    # required (sh_degree, white_background, source_path, ...)
  cameras.json                                # required (training cameras; identifies the folder as a 3DGS model)
  input.ply                                   # required (sparse point cloud)
  point_cloud\iteration_7000\point_cloud.ply
  point_cloud\iteration_30000\point_cloud.ply # at least one iteration; the largest is loaded by default (--iteration N to choose)
```

`record_vive_pro_eye.ps1` checks that these files are present.

## Sources

- Pretrained INRIA scenes (truck, bicycle, garden, ...): <https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/datasets/pretrained/models.zip>. Extract it, e.g. into `D:\scenes`, and point `-Scene` to one scene folder, e.g. `D:\scenes\truck` (the path used throughout this tutorial).
- Your own captures trained with `gaussian-splatting`: use the training output folder.

## Good to know

- `cfg_args` usually points (`source_path=...`) to the PC used for training. The viewer then prints the lines below, which are **harmless**: it falls back to the model folder.

  ```
  ##  ERROR  ##: ... Cannot determine type of dataset at /f:/...
  Did not find specified input folder, loading from model path
  ```

- The scene name used in output file names is the last folder name of the scene path. Do not end the path with `\` when calling the viewer yourself, or the name becomes `unknownscene`. The launcher script trims it.
- Big scenes (millions of splats) lower the frame rate, i.e. the trace sampling rate. Use `-Iteration 7000` (fewer splats) or `-Downscale 2` ([step 5](05_recording.md#55-frame-rate--sampling-rate)).

## Scene initialization (tilt, scale, start position)

A raw 3DGS scene has no real-world unit, its "up" is wherever the photos made it, and its origin is arbitrary. Without initialization, the participant is often:

- tilted (33° for *figurines*);
- at the wrong size (6 times too small for *figurines*);
- somewhere random, sometimes inside an object.

The viewer corrects this with `--initial-position`, `--initial-quaternion` and `--initial-scale`. There are two ways to get these values.

### Automatic: natural spawn with real-world scale (recommended for your own scenes)

```powershell
python utils\SceneSpawn\compute_spawn.py D:\scenes\myscene                       # scale from a heuristic
python utils\SceneSpawn\compute_spawn.py D:\scenes\myscene --units-per-metre 6.3  # scale you measured
```

It writes `D:\scenes\myscene\eyenavgs_spawn.json`. The recording script then uses it automatically. With it:

- the SteamVR floor is the scene floor and the scene is level;
- 1 m walked is 1 m in the scene;
- the participant starts in the middle of the play area, next to the content and facing it. They must face the play area's forward direction, chosen in Room Setup.

Their own eye height is kept. How it works, the options, and how to measure the scale: [utils/SceneSpawn](../utils/SceneSpawn/README.md).

**Check it before a session**, without a headset: `--preview-trace` writes a synthetic trace of a person standing at the spawn. Render it with the desktop replay (`--rendering-mode 4`, see the [SceneSpawn README](../utils/SceneSpawn/README.md#check-the-result-without-a-headset)).

The example scene of this tutorial, `figurines_splatting` (LERF *figurines*), has a ready-made spawn: [`examples/figurines_splatting_spawn.json`](../examples/figurines_splatting_spawn.json). Copy it into the scene folder as `eyenavgs_spawn.json`, or pass `-SpawnFile examples\figurines_splatting_spawn.json`. With this spawn:

- the participant stands at a 1.05 m bar table with the figurines, 25 cm from its edge, facing it and the windows;
- the scale is 6.3 units per metre, measured on the Rubik's cube (5.7 cm) and checked on the table and the camera heights.

### Manual: the EyeNavGS paper values

The EyeNavGS paper corrects each scene's tilt and scale and places the participant at a start position. The values are in the table of the [original README](../docs/ORIGINAL_README.md#example-scene-initialization-values) and in `scene_setting.csv` of the [EyeNavGS Rutgers dataset](https://github.com/symmru/EyeNavGS_Rutgers_Dataset). Use them to compare your traces with that dataset. Example for *truck*:

```
-InitialPosition "-2 1.8 -4" -InitialQuaternion "-0.0872 0 0 0.9962" -InitialScale 0.8
```

Values given on the command line take precedence over a spawn file.

### Keep track of them

The launcher stores the values in `session_info.txt` and copies the spawn file into the session folder ([step 5](05_recording.md)). They are needed to reproduce a live session, to compare a trace with the dataset, or to convert positions to metres ([step 6.2](06_trace_format.md#62-coordinate-frames)). The replay modes do not need them: the recorded poses already include the initialization.

---
[← 3. VR and eye-tracking setup](03_vr_eye_tracking_setup.md) · [Tutorial index](README.md) · [5. Recording →](05_recording.md)
