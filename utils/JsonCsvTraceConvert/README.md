# 👁️NavGS: A 6-DoF Navigation Dataset and Record-n-Replay Software for Real-World 3DGS Scenes in VR

-------------------------------------------------------------------------------------------------------

## Overview

**Trace Format Conversion Tool** from `👁️NavGS (EyeNavGS)`

`utils/JsonCsvTraceConvert/`

This utility folder provides C++ and Python tools to convert camera traces between the `.csv` format used by 👁️NavGS and the `.json` format used by other popular frameworks, ensuring cross-platform compatibility. Given a `.json` trace file exported from a viewer like NeRFstudio, the `JsonToCSV.cpp` tool converts it into the `.csv` format required by the 👁️NavGS viewer. Conversely, given a `.csv` trace recorded by 👁️NavGS, the `CsvToJson.cpp` (or `csv_to_json.py`) tool converts it into the `.json` format for use in external viewers.

Use this toolkit to seamlessly move camera trajectories between 👁️NavGS and other popular frameworks for analysis and replay.

---

#### 1. Prerequisites

##### C++ Tools (`JsonToCSV.cpp` & `CsvToJson.cpp`)

- **Compiler:** A C++17 compliant compiler (e.g., g++, Clang++, MSVC).

- **Dependencies:**
  
  1. **Eigen:** A C++ template library for linear algebra.
  
  2. **nlohmann/json:** A single-header JSON library for C++.
  
  It is recommended to place these libraries in a shared `external` directory.

##### Python Script (`csv_to_json.py`)

- **Interpreter:** Python 3.6+

- **Dependencies:** Install `NumPy` and `SciPy` via pip:
  
  ```bash
  pip install numpy scipy
  ```

---

#### 2. Folder Structure

```textile
utils/JsonCsvTraceConvert/
  ├── JsonToCSV.cpp
  ├── CsvToJson.cpp
  ├── csv_to_json.py
  └── README.md`
```



- **`JsonToCSV.cpp`**: Converts JSON traces (exported from other frameworks) to the CSV format used by the 👁️EyeNavGS viewer.

- **`CsvToJson.cpp`**: Converts 👁️EyeNavGS's CSV traces into the JSON format used by frameworks like NeRFstudio.

- **`csv_to_json.py`**: A Python-based alternative for the CSV-to-JSON conversion.

---

#### 3. How to Use

##### Step 1: Compile C++ Tools

From a terminal, run the following commands. Ensure you adjust the `-I` flag to point to the `external` directory containing the dependencies.

```bash
# Compile the JSON to CSV converter
g++ JsonToCSV.cpp -o JsonToCSV -I/path/to/external -std=c++17

# Compile the CSV to JSON converter
g++ CsvToJson.cpp -o CsvToJson -I/path/to/external -std=c++17
```

##### Step 2: Run Conversion Tools

1. **Convert JSON to CSV (for use in EyeNavGS):**
   
   ```bash
   # Usage: ./<executable> <input.json> <output.csv>
   ./JsonToCSV external_trace.json eyenavgs_trace.csv
   ```

2. **Convert CSV to JSON (for use in other frameworks):** *Note: The `--width` and `--height` of the original capture are required arguments.*
   
   Using the C++ tool:
   
   ```bash
   # Usage: ./<executable> <input.csv> <output.json> <width> <height>
   ./CsvToJson eyenavgs_trace.csv external_trace.json 1297 840
   ```
   
   Using the Python script:
   
   ```bash
   # Usage: python csv_to_json.py <input.csv> <output.json> --width <W> --height <H>
   python csv_to_json.py eyenavgs_trace.csv external_trace.json --width 1297 --height 840
   ```

---

#### 4. Important Limitations: Data Loss During Conversion

It is critical to understand that the conversion from JSON to CSV is **lossy**. The `JsonToCSV.cpp` script is designed to extract only the geometric data required for replay and does not preserve all metadata from the source file.

The following fields from a source JSON file are **discarded** during conversion to CSV:

- `id`: The unique frame identifier.

- `img_name`: The source image filename.

- `is_key_frame`: A boolean flag indicating if the frame is a keyframe.

Consequently, the inverse conversion scripts (`CsvToJson.cpp` and `csv_to_json.py`) cannot recover this lost information. They will generate **placeholder values** for these fields in the reconstructed JSON file:

- `id` is replaced with a sequential 0-based index.

- `img_name` is replaced with a generic name (e.g., "reconstructed_1").

- `is_key_frame` is hardcoded to `false`.
