# Example trace

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
