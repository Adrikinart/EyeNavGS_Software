# 4. Get a 3DGS scene

The viewer loads a standard **3D Gaussian Splatting output folder**, as produced by the INRIA [`gaussian-splatting`](https://github.com/graphdeco-inria/gaussian-splatting) training code:

```
<scene>\
  cfg_args                                    # mandatory (sh_degree, white_background, source_path, ...)
  cameras.json                                # training cameras (desktop view)
  input.ply                                   # sparse point cloud
  point_cloud\iteration_7000\point_cloud.ply
  point_cloud\iteration_30000\point_cloud.ply # the largest iteration is loaded by default (--iteration N to choose)
```

## Sources

- Pretrained INRIA scenes (truck, bicycle, garden, ...): <https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/datasets/pretrained/models.zip>. Extract it and point `-Scene` to one sub-folder, e.g. `D:\scenes\models\truck`.
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

The EyeNavGS paper corrects each scene's tilt and scale and places the participant at a start position. The values are in the README's table and in `scene_setting.csv` of the [EyeNavGS Rutgers dataset](https://github.com/symmru/EyeNavGS_Rutgers_Dataset). Example for *truck*:

```
-InitialPosition "-2 1.8 -4" -InitialQuaternion "-0.0872 0 0 0.9962" -InitialScale 0.8
```

Record which values you used. They are needed to replay a trace or to compare it with the dataset (see `session_info.txt`, [step 5](05_recording.md)).

---
[← 3. VR and eye-tracking setup](03_vr_eye_tracking_setup.md) · [Tutorial index](README.md) · [5. Recording →](05_recording.md)
