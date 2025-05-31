# `scene_select.bat` - Quick Guide

This batch script helps Windows users quickly launch different 3D Gaussian Splatting (3DGS) or ZipNeRF scenes in VR mode using the 👁️NavGS SIBR VR viewer.

---

## How to Use

1. **Configure Paths:** Open `scene_select.bat` in a text editor. At the top of the script, **update the following paths** to match your system:
   
   - `viewer_exe`: Path to your `SIBR_gaussianViewer_app_d.exe`.
   - `gs_model_base`: Base directory for your 3DGS models.
   - `zipnerf_model_base`: Base directory for your ZipNeRF models.
   - `trace_output_folder`: Where recorded traces will be saved.
   
   Code snippet
   
   ```
   REM ======== User-Defined Paths ========
   set viewer_exe=D:\projects\sibr\SIBR_Gaussian_VRV\install\bin\SIBR_gaussianViewer_app_d.exe
   set gs_model_base=D:\projects\sibr\scenes\models
   set zipnerf_model_base=D:\projects\sibr\scenes\zip_nerf_3dgs
   REM ======== Defined test-user folder output Path ========
   set trace_output_folder=D:\projects\sibr\trace_saving\user000
   ```

2. **Run the Script:** Double-click `scene_select.bat` or run it from a PowerShell/Command Prompt window:
   
   Bash
   
   ```
   .\scene_select.bat
   ```

3. **Select a Scene:** The script will display a list of available scenes. Enter the corresponding number for the scene you wish to run and press Enter.

4. **VR Interaction and Recording:** The viewer will launch in **Headset Mode**. If you're recording, press **Start Trace Recording** then **End Trace Recording** in the desktop companion window to save each eye's trace (`output<number>.csv`) to the `trace_output_folder` you just configured.


