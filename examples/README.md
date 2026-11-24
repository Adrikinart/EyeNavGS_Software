# Examples

## Spawn file of the example scene

`figurines_splatting_spawn.json` places the participant naturally in the `figurines_splatting` scene (LERF *figurines*, trained with `gaussian-splatting`):

- level floor and real-world scale (6.3 scene units per metre, measured on the Rubik's cube and the table);
- start at the bar table with the figurines, facing it.

It was made with `utils\SceneSpawn\compute_spawn.py` ([README](../utils/SceneSpawn/README.md)). To use it, either:

- copy it into the scene folder as `eyenavgs_spawn.json`: the recording script then uses it automatically;
- or pass it explicitly:

  ```powershell
  powershell -ExecutionPolicy Bypass -File scripts\windows\record_vive_pro_eye.ps1 -Scene D:\scenes\figurines_splatting `
      -Participant P01 -SpawnFile examples\figurines_splatting_spawn.json
  ```

The values only fit that reconstruction. Another training run of the same photos has another origin and scale.

## Example trace

`sample_trace_vive_pro_eye.csv` is a **real** 15 s trace, recorded while this tutorial was being tested:

- HTC VIVE Pro Eye, SteamVR + VIVE Console (`XR_HTC_eye_tracker`), SR_Runtime 1.3.6.12;
- scene `figurines_splatting`, `--downscale 2`, `record_vive_pro_eye.ps1 -Duration 15`.

What it is useful for:

- see the exact file format ([tutorial/06_trace_format.md](../tutorial/06_trace_format.md));
- try the analysis tools without hardware:

  ```powershell
  python utils\CheckTrace\check_trace.py examples\sample_trace_vive_pro_eye.csv
  ```

Please note:

- **Nobody was wearing the headset.** It was only held, so:
  - only about 15 % of the rows have `GazeValid=1`;
  - SteamVR throttled the frame rate to about 7 frames/s.

  A real participant session has most rows valid and runs at the headset frame rate.
- The file was recorded just before the `timestep` column was added to the viewer. That last column was appended afterwards with the viewer's formula, `round(elapsed_ms)`. All other columns are exactly as written by the viewer.
