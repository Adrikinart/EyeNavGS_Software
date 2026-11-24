# CheckTrace

`check_trace.py` sanity-checks an EyeNavGS headset trace. It works with the 34-column traces of this fork and with the original 24-column EyeNavGS files.

```powershell
python utils\CheckTrace\check_trace.py <trace.csv> [--plot]
```

- **Requirements:** Python 3.8+, standard library only. `--plot` additionally needs `matplotlib`.
- **Exit code:**
  - `0`: the trace contains valid eye-tracking data;
  - `1`: no valid gaze sample;
  - `2`: format error (unreadable file, missing columns, bad command line).
- `python` must be a real Python, not the Microsoft Store stub. `py -3 utils\CheckTrace\check_trace.py ...` also works.

The checks are described in [tutorial/07_analysis.md](../../tutorial/07_analysis.md). `scripts\windows\record_vive_pro_eye.ps1` runs this script automatically on every new trace at the end of a session.
