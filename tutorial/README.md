# Tutorial: record 6-DoF + eye-tracking traces in 3DGS scenes with an HTC VIVE Pro Eye

This tutorial takes you from a blank Windows PC to CSV traces in which every frame contains:

- the head pose (one row per eye);
- the per-eye gaze;
- the pupil diameter and eye openness.

They are recorded while a participant walks through a photorealistic 3D Gaussian Splatting (3DGS) scene in VR, with the EyeNavGS viewer.

| Step | Page | Time |
|---|---|---|
| 0 | [Overview: how it works, and what used to go wrong](00_overview.md) | 5 min read |
| 1 | [Requirements and environment check](01_requirements.md) | 30–60 min of installs |
| 2 | [Build the viewer](02_build.md) | ~15 min (mostly automatic) |
| 3 | [Set up SteamVR, VIVE Console and SRanipal (once)](03_vr_eye_tracking_setup.md) | 20 min |
| 4 | [Get a 3DGS scene](04_scenes.md) | download time |
| 5 | [Record a session](05_recording.md) | per participant |
| 6 | [Trace format reference](06_trace_format.md) | reference |
| 7 | [Check and analyze traces](07_analysis.md) | – |
| 8 | [Troubleshooting](08_troubleshooting.md) | when needed |
| 9 | [What was changed in the code](09_code_changes.md) | for developers |

**Fast path** (if your PC already has the tools of step 1):

```powershell
powershell -ExecutionPolicy Bypass -File scripts\windows\check_environment.ps1     # what is missing?
powershell -ExecutionPolicy Bypass -File scripts\windows\build_eyenavgs.ps1        # build
powershell -ExecutionPolicy Bypass -File scripts\windows\record_vive_pro_eye.ps1 -Scene D:\scenes\truck -Participant P01
```

All commands are run from the root of the cloned repository, in a PowerShell window.

---
[Start with the overview →](00_overview.md)
