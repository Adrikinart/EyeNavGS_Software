# SPDX-FileCopyrightText: 2025 Systems and Multimedia Lab @ Rutgers University
# SPDX-License-Identifier: Apache-2.0
#
# This file is part of the 👁️NavGS (EyeNavGS) project.
# See LICENSE_EYENAVGS.txt and NOTICE for details.


import pandas as pd
import numpy as np
from pathlib import Path
from scipy.spatial.transform import Rotation as R
import argparse

# === CONFIGURATION ===
USE_CLI = True  # Set to True to use command-line arguments

DATASET_PATH = Path(r"D:\JasonD\projects\EyeNavGS_dataset\test_convert")
SCENE_SETTING_CSV = Path(r"D:\JasonD\projects\EyeNavGS_dataset\scene_setting.csv")
OUTPUT_PATH = DATASET_PATH.parent / "corrected_csv"
WINDOW_SIZE = 20
FINAL_SCALE_FACTOR = 1.0


def parse_cli_args():
    parser = argparse.ArgumentParser(description="Convert VR CSV traces to corrected coordinate CSVs.")
    parser.add_argument("--mode", choices=["config", "cli"], default="config", help="Run mode: use config or CLI")
    parser.add_argument("--input_csv", type=str, help="Path to a single input CSV file")
    parser.add_argument("--input_folder", type=str, help="Path to a folder of CSVs (one scene)")
    parser.add_argument("--scene_settings", type=str, default=None, help="Path to scene_setting.csv (optional)")
    parser.add_argument("--output", type=str, default=None, help="Output folder (optional)")
    return parser.parse_args()


def load_scene_settings(csv_path):
    df = pd.read_csv(csv_path)
    df["Quaternion"] = df["Quaternion"].apply(lambda q: [float(x) for x in q.split(",")])
    df["Initial_Position"] = df["Initial_Position"].apply(lambda p: [float(x) for x in p.split(",")])
    return df.set_index("Dataset_Name").to_dict("index")


def convert_csv(input_csv: Path, scene: str, settings: dict, output_dir: Path):
    df_full = pd.read_csv(input_csv)
    df = df_full[df_full["ViewIndex"] == 0].iloc[::2].reset_index(drop=True)

    quat = settings["Quaternion"]
    scale = settings["Scale"]
    init_pos = settings["Initial_Position"]

    P_new = df[['PositionX', 'PositionY', 'PositionZ']].values
    P_old = np.divide((P_new - init_pos), scale) + init_pos
    df['PositionX_raw'] = P_old[:, 0]
    df['PositionY_raw'] = P_old[:, 1]
    df['PositionZ_raw'] = P_old[:, 2]

    df['PositionX_scaled'] = df['PositionX_raw'] * FINAL_SCALE_FACTOR
    df['PositionY_scaled'] = df['PositionY_raw'] * FINAL_SCALE_FACTOR
    df['PositionZ_scaled'] = df['PositionZ_raw'] * FINAL_SCALE_FACTOR

    df['PositionX_smoothed'] = df['PositionX_scaled'].rolling(window=WINDOW_SIZE).mean()
    df['PositionY_smoothed'] = df['PositionY_scaled'].rolling(window=WINDOW_SIZE).mean()
    df['PositionZ_smoothed'] = df['PositionZ_scaled'].rolling(window=WINDOW_SIZE).mean()

    df = df.dropna().copy()

    rotation = R.from_quat(quat)
    smoothed = df[['PositionX_smoothed', 'PositionY_smoothed', 'PositionZ_smoothed']].values
    rotated = rotation.apply(smoothed) + init_pos

    df['PositionX_corrected'] = rotated[:, 0]
    df['PositionY_corrected'] = rotated[:, 1]
    df['PositionZ_corrected'] = rotated[:, 2]
    df['elapsed_ms'] = df["Timestamp"] - df["Timestamp"].iloc[0]

    output_dir.mkdir(parents=True, exist_ok=True)
    out_file = output_dir / input_csv.name.replace(".csv", "_corrected.csv")
    df.to_csv(out_file, index=False)
    print(f"[✓] Saved: {out_file}")


def run_with_config():
    settings = load_scene_settings(SCENE_SETTING_CSV)
    scenes = sorted([d.name for d in DATASET_PATH.iterdir() if d.is_dir()])
    print(f"Total Scenes found in setting{len(settings)}; Following scenes has been used {scenes}")
    OUTPUT_PATH.mkdir(parents=True, exist_ok=True) 


    for scene in scenes:
        if scene not in settings:
            print(f"[Skip] No settings for {scene}")
            continue

        for user_csv in (DATASET_PATH / scene).glob("user*_*.csv"):
            scene_name = user_csv.stem.split("_", 1)[1]
            if scene_name not in settings:
                print(f"[Skip] No settings for scene: {scene_name}")
                continue
            convert_csv(user_csv, scene_name, settings[scene_name], OUTPUT_PATH / scene_name)



def run_with_cli(args):
    scene_setting_path = Path(args.scene_settings) if args.scene_settings else SCENE_SETTING_CSV
    settings = load_scene_settings(scene_setting_path)

    output_path = Path(args.output) if args.output else OUTPUT_PATH
    output_path.mkdir(parents=True, exist_ok=True)

    if args.input_csv:
        input_csv = Path(args.input_csv)
        #scene = input_csv.parent.name
        scene = input_csv.stem.split("_", 1)[1]
        if scene not in settings:
            print(f"[Skip] No settings for scene: {scene}")
            return
        convert_csv(input_csv, scene, settings[scene], output_path / scene)

    elif args.input_folder:
        input_folder = Path(args.input_folder)
        
        #scene = input_folder.name
        for csv_file in input_folder.glob("user*_*.csv"):
            scene = csv_file.stem.split("_", 1)[1]
            if scene not in settings:
                print(f"[Skip] No settings for scene: {scene}")
                continue
            convert_csv(csv_file, scene, settings[scene], output_path / scene)
    else:
        print("[Error] Please specify either --input_csv or --input_folder.")


def main():
    if USE_CLI:
        args = parse_cli_args()
        if args.mode == "cli":
            run_with_cli(args)
        else:
            run_with_config()
    else:
        run_with_config()


if __name__ == "__main__":
    main()

'''
# Convert a single csv file:
    python coord_convert.py --mode cli --input_csv  "D:\\data\\scene\\raw_trace\\user101_alameda.csv"
# Convert a folder of csv files:
    python coord_convert.py --mode cli --input_folder "D:\\data\\scene\\raw_trace"
# Custom scene setting:
    python coord_convert.py --mode cli \
    --input_folder "D:\\data\\scene\\raw_trace" \
    --scene_settings "D:\\data\\scene_setting.csv" \
    --output "D:\\data\\corrected_results"
'''
# sample command to run the script with CLI arguments:
# python coord_convert.py --mode cli --input_folder "D:\\JasonD\\projects\\utils\\WorldCoordConvert\\test_convert\\alameda" --scene_settings "D:\\JasonD\\projects\\EyeNavGS_dataset\\scene_setting.csv" --output "D:\\JasonD\\projects\\SIBR_Gaussian_VRV\\utils\\WorldCoordConvert\\test_convert\\res"