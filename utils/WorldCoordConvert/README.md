# 👁️NavGS: A 6-DoF Navigation Dataset and Record-n-Replay Software for Real-World 3DGS Scenes in VR

---

## Overview

**World Coord Convert Tool** from `👁️NavGS(EyeNavGS)`

`utils/WorldCoordConvert/`

This utility folder provides a Python script that **converts recorded “virtual world coordinates” back to 1:1 “physical stage coordinates”** by applying the inverse of initialization transforms (tilt, scale, translation). Given a trajectory CSV and per-scene transformation metadata, it outputs a corrected CSV aligned to real-world coordinates, suitable for cross-user and cross-scene analysis.

---

## 1. Prerequisites

1. **Python 3.7+**

2. **Install required Python packages** (run from your virtual environment or system):
   
   ```bash
   pip install numpy pandas scipy
   ```

---

## 2. Folder Structure

```textile
utils/WorldCoordConvert/
├── coord_convert.py
└── README.md
```

- **coord_convert.py**: Converts one or more CSV trace files into real-world coordinates.

- **[From dataset]** `scene_setting.csv`: Metadata for each scene including rotation, scale, and position.

---

## 3. How to Use

### Step 1: Prepare Inputs

1. One or more input CSV trace files with format `userXYZ_<scene>.csv`

2. A `scene_setting.csv` file with per-scene transformation metadata

### Step 2: Run the Script

#### A. Convert a Single CSV File

```bash
python coord_convert.py --mode cli --input_csv "D:\\dataset\\scene_name\\user101_truck.csv" --scene_settings "D:\\dataset\\scene_setting.csv" --output "D:\\dataset\\converted"
```

#### B. Convert All CSVs in a Folder

```bash
python coord_convert.py --mode cli --input_csv "D:\\dataset\\scene_name\\" --scene_settings "D:\\dataset\\scene_setting.csv" --output "D:\\dataset\\converted"
```

---

## 4. Configuration Tips

- **Scene Name Extraction**: The scene name is inferred from each file name (e.g., `user123_truck.csv` → scene=`truck`).

- **`scene_setting.csv` Format**:

```textile
Scene_Name,Quaternion,Scale,Initial_Position
truck,"-0.0896, 0.0000, 0.0000, 0.9960",0.76,"0,2.1,-4"
```

- **Output Path**: Output folder `corrected_csv` is created automatically if it doesn't exist.

---

## 5. Example Workflow

1. **Navigate to the folder**:

```bash
cd ../utils/WorldCoordConvert/
```

2. **Run with CLI**:

```bash
python coord_convert.py --mode cli \  
    --input_csv "test_convert/alameda/user101_alameda.csv" \  
    --scene_settings "scene_setting.csv" \  
    --output "test_convert/alameda/res"
```

3. **Check Output**

---

## 6. Troubleshooting

- **Scene not found**: Ensure the scene name in the filename exists in `scene_setting.csv`

- **Path errors**: Use forward slashes `/` or escaped backslashes `\\` in Windows

- **Unbound variable errors**: Ensure correct use of `--input_csv` for files and `--input_folder` for batch processing

- **No output written**: Check for empty CSV or missing `ViewIndex == 0` rows

---
