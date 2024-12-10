# Environment Setup

1. **Clone the repository:**
   ```sh
   git clone https://github.com/symmru/SIBR_Gaussian_VRV.git
   ```

2. **Install requirements and ensure they are in your PATH:**

    Follow the [Install requirements](#install-requirements) and make sure they are in the PATH, run the command below to test:

   ```sh
   python --version
   doxygen --version
   nvcc --version
   cmake --version
   ```
3. **Download and unzip the dataset:**

   Download the pretrained model data from:
   ```
   https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/datasets/pretrained/models.zip
   ```
   Extract it into a directory of your choice (e.g., `C:\User\SIBR\models`).


# Compilation

1. **Generate Visual Studio project with CMake-GUI:**
   - Open CMake-GUI.
   
       ![openxr gaussian viewer](./docs/img/cmake-0.png)

   - Set the source directory to the repository root and the build directory to `build/`.

        ![openxr gaussian viewer](./docs/img/Cmake-1.png)

   - Click "Configure" and select the Visual Studio C++ Win64 compiler.
   - Select the desired BUILD options and then click "Generate".
        ![openxr gaussian viewer](./docs/img/Cmake-2.png)


2. **Compile using Visual Studio:**
   - Open `build/sibr_projects.sln` in Visual Studio.
   - For the `core/sibr_openxr` project, set the C++ standard to C++20 or higher.
   - Build the `ALL_BUILD` target, then the `INSTALL` target.

   The resulting executables will be placed in `install/bin`. The main executable is:
   ```
   SIBR_gaussianViewer_app_d.exe
   ```

   Ensure `install/bin` is in your PATH.


# The SIBR VR Viewer

The SIBR VR Viewer allows users to visualize 3D Gaussian Splatting scenes either in a Desktop (non-VR) environment or through a VR headset using OpenXR.

For more detailed documentation on the SIBR core system, please refer to the original [SIBR Core](#sibr-core) section and documentation.

## Desktop Mode

**What is Desktop Mode?**  
Desktop mode runs the viewer as a monocular 2D application on the computer monitor. You can interact using a keyboard and mouse. This mode is useful for saving traces and recording camera paths without needing a VR headset.

## Headset Mode

**What is Headset Mode?**  
Headset mode uses OpenXR to stream stereoscopic views to a VR headset. This mode immerses the user inside the virtual environment, allowing movement tracking through head movements. This is ideal for VR demonstrations, immersive walkthroughs, or data collection via a head-mounted display.


# Starting the SIBR VR Viewer

## Command Line Arguments

The main arguments include:

- `-m <dataset_path>`: Specifies the dataset (model) path.
- `--rendering-mode <mode>`: Selects the rendering mode.
  - `0`: Monocular desktop mode (manual navigation)
  - `1`: Stereo anglaph desktop mode (manual navigation)
  - `2`: Headset mode (VR)
  - `3`: Headset mode replay (VR)
  - `4`: Monocular desktop mode replay
- `--initial-position <x> <y> <z>`: Set the initial position of the headset space.
- `--initial-quaternion <X> <Y> <Z> <W>`: Set the initial orientation of the headset space using a quaternion.
- `--initial-scale <float>`: Set the initial scale of the user’s "virtual body" (affects perceived size of the world).
- `-in <trace_file_path>`: Specifies a recorded trace file to replay.
- `--rendering-size <width> <height>`: Sets window size for desktop replay mode.

## Example Scene Models

Download example scene models from the provided link and place them in your chosen dataset directory. The following table gives some reference values for initialization of the viewer:

| Dataset_Name | Initial Position (x y z)    | Quaternion (X Y Z W)                            | Scale (float)        |
|--------------|-----------------------------|-------------------------------------------------|----------------------|
| truck        | --initial-position -2 1.8 -4|--initial-quaternion -0.0872 0.0000 0.0000 0.9962|--initial-scale 0.8   |
| treehill     |--initial-position  2 1 2    |--initial-quaternion -0.2164 0.0000 0.0000 0.9763|--initial-scale 1     |
| train        |--initial-position  2 0 3    |--initial-quaternion 0.0872 0.0000 0.0000 0.9962 |--initial-scale 0.2   |
| stump        |--initial-position  -1 1.1 -2|--initial-quaternion -0.4226 0.0000 0.0000 0.9063|--initial-scale 3     |
| room         |--initial-position  0 1.1 0  |--initial-quaternion -0.2164 0.0000 0.0000 0.9763|--initial-scale 2     |
| playroom     |--initial-position  0 0.8 0  |--initial-quaternion -0.2164 0.0000 0.0000 0.9763|--initial-scale 2     |
| kitchen      |--initial-position  0.6 0.7 0|--initial-quaternion -0.3420 0.0000 0.0000 0.9397|--initial-scale 5     |
| garden       |--initial-position  4 1.7 1  |--initial-quaternion -0.2588 0.0000 0.0000 0.9659|--initial-scale 1     |
| flowers      |--initial-position  0 0 -2   |--initial-quaternion 0.1305 0.0000 0.0000 0.9914 |--initial-scale 1     |
| drjohnson    |--initial-position  0 1.5 0  |--initial-quaternion -0.2126 0.2126 0.6744 0.6744|--initial-scale 1     |
| counter      |--initial-position  0 1 -0.3 |--initial-quaternion -0.3007 0.0000 0.0000 0.9537|--initial-scale 4     |
| bonsai       |--initial-position  0.7 1 -1 |--initial-quaternion -0.3420 0.0000 0.0000 0.9397|--initial-scale 3     |
| bicycle      |--initial-position  1 0.9 -2 |--initial-quaternion -0.1305 0.0000 0.0000 0.9914|--initial-scale 0.2   |

## Desktop Mode

**Example command for starting the SIBR viewer in desktop mode:**
```sh
SIBR_gaussianViewer_app_d.exe -m C:\User\SIBR\models\train --rendering-mode 0
```
  
You will see a window with the scene rendered as a 2D view. Interact using your mouse and keyboard.

![Desktop](./docs/img/Monocular_desktop.png)


## Headset Mode

**Example command for starting the SIBR viewer in headset mode:**
```sh
SIBR_gaussianViewer_app_d.exe -m C:\User\SIBR\models\truck --rendering-mode 2 --initial-position -2 1.8 -2 --initial-quaternion -0.0872 0.0000 0.0000 0.9962 --initial-scale 0.8
```

With a supported OpenXR runtime (such as SteamVR on PC or Oculus Link on Windows), you can view and move around in the scene using a VR headset. And you can also see a window with 2 subwindows on the Desktop representing two eyes in the Headset.

![Headset](./docs/img/Headset_mode.png)

# Trace Recording

You can record the camera’s movement trajectory (position), field-of-view (FOV), and orientation (quaternion) for future replay.

## Desktop Mode

- Start the viewer in desktop mode. Then you can see a subwindow on the desktop as shown below:

    ![openxr gaussian viewer](./docs/img/Saving_trace_Desktop.png)

- Click “Record” to start recording your camera path.
- Click “Stop” to end the recording.
- Click “Save path” to choose a location to save the recorded `.csv` file.

## Headset Mode
- Start the viewer in headset mode. Then you can see a subwindow on the desktop as shown below:

    ![openxr gaussian viewer](./docs/img/Saving_trace_button.png) 

- Press “Save Traces” to start recording your head movements and FOV as you move in VR.
- Press “Stop Saving” to end recording.
- The output files (`output[number].csv`) are saved in the current directory.

    ![openxr gaussian viewer](./docs/img/output_trace.png) 

# Format of Recorded Traces

The recorded traces are stored in `.csv` files. Each row corresponds to a captured frame/state of the viewer.

| ViewIndex |   FOV1    |   FOV2    |   FOV3    |   FOV4    | PositionX  | PositionY  | PositionZ  | QuaternionX | QuaternionY | QuaternionZ | QuaternionW |
|-----------|-----------:|----------:|----------:|----------:|-----------:|-----------:|-----------:|------------:|------------:|------------:|------------:|
| 0         | -0.94248   | 0.698132  | -0.95993  | 0.767945  | 0.003914   | 0.895811   | -0.07397   | 0.257321    | -0.107277   | -0.030081   | 0.959882    |
| 1         | -0.69813   | 0.942478  | -0.95993  | 0.767945  | 0.065237   | 0.888708   | -0.06199   | 0.257321    | -0.107277   | -0.030081   | 0.959882    |
| 0         | -0.94248   | 0.698132  | -0.95993  | 0.767945  | 0.002297   | 0.89517    | -0.0753    | 0.254336    | -0.107171   | -0.0282334  | 0.960745    |
| 1         | -0.69813   | 0.942478  | -0.95993  | 0.767945  | 0.063636   | 0.88833    | -0.06326   | 0.254336    | -0.107171   | -0.0282334  | 0.960745    |
| 0         | -0.94248   | 0.698132  | -0.95993  | 0.767945  | 0.002278   | 0.895369   | -0.07456   | 0.254109    | -0.108516   | -0.0283014  | 0.960652    |
| 1         | -0.69813   | 0.942478  | -0.95993  | 0.767945  | 0.06358    | 0.888482   | -0.06235   | 0.254109    | -0.108516   | -0.0283014  | 0.960652    |
* ViewIndex: Index used to identify left eye or right eye, 0 is left, 1 is right.  
* FOV1: The left field of view angle.  
* FOV2: The right field of view angle.  
* FOV3: The top field of view angle.  
* FOV4: The bottom field of view angle.  
* PositionX,Y,Z: The camera's position coordinates in 3D space, defining where the camera is located.  
* QuaternionX,Y,Z,W: Defines the camera's rotation as a quaternion, which represents 3D rotations without the risk of gimbal lock.


# Trace Replay

To replay a previously recorded trace, you can use the command line arguments described above in the [Command Line Arguments](#command-line-arguments) section. Make sure to specify the input trace file and select the appropriate rendering mode.

## Desktop Mode

**Example command for desktop mode replay:**
```sh
SIBR_gaussianViewer_app_d.exe -m C:\User\SIBR\models\train -in C:\User\SIBR\Test\trace.csv --rendering-mode 4 --rendering-size 1200 900
```
If no `--rendering-size` is provided, default is 1200x789.

## Headset Mode

**Example command for headset mode replay:**
```sh
SIBR_gaussianViewer_app_d.exe -m C:\User\SIBR\models\train -in C:\User\SIBR\Test\output0.csv --rendering-mode 3 --rendering-size 1200 900
```
If no `--rendering-size` is provided, default size for each eye is 2064x2272.

