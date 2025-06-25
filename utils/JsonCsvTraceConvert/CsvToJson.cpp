// SPDX-FileCopyrightText: 2025 Systems and Multimedia Lab @ Rutgers University
// SPDX-License-Identifier: Apache-2.0
//
// This file is part of the 👁️NavGS (EyeNavGS) project.
// See LICENSE_EYENAVGS.txt and NOTICE for details.
//
// This script performs the inverse operation of JsonToCSV.cpp,
// converting the CSV trace back to the original JSON format.

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <nlohmann/json.hpp>
#include <Eigen/Dense>

using json = nlohmann::json;

// A simple utility to split a string by a delimiter.
// This is used to parse each row of the CSV file.
std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

int main(int argc, char* argv[]) {
    // The program requires the input CSV, output JSON, width, and height.
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <input.csv> <output.json> <width> <height>" << std::endl;
        return 1;
    }

    // Parse command-line arguments
    std::string inputCsvPath = argv[1];
    std::string outputJsonPath = argv[2];
    double width, height;
    try {
        width = std::stod(argv[3]);
        height = std::stod(argv[4]);
    }
    catch (const std::invalid_argument& e) {
        std::cerr << "Error: Invalid width or height provided. Please provide numbers." << std::endl;
        return 1;
    }

    std::ifstream inputFile(inputCsvPath);
    if (!inputFile.is_open()) {
        std::cerr << "Error: Could not open the input CSV file at " << inputCsvPath << std::endl;
        return 1;
    }

    json reconstructedJson = json::array();
    std::string line;
    int frame_id = 0;

    std::getline(inputFile, line);

    while (std::getline(inputFile, line)) {
        std::vector<std::string> values = split(line, ',');

        if (values.size() != 12) {
            std::cerr << "Warning: Skipping malformed row " << frame_id + 1 << " with " << values.size() << " columns." << std::endl;
            continue;
        }

        try {
            double fov1 = std::stod(values[1]);
            double fov2 = std::stod(values[2]);
            double fov3 = std::stod(values[3]);
            double fov4 = std::stod(values[4]);

            double fovX_rad = fov2 - fov1;
            double fovY_rad = fov4 - fov3;

            // Invert the 'atan' formula to find the camera intrinsics fx and fy
            double fx = width / (2.0 * tan(fovX_rad / 2.0));
            double fy = height / (2.0 * tan(fovY_rad / 2.0));

            json position = {
                std::stod(values[5]), // PositionX
                -std::stod(values[6]), // Position-Y
                -std::stod(values[7])  // Position-Z
            };

            double qw = std::stod(values[11]); // W 
            double qz_neg = std::stod(values[10]); // -Z
            double qy_neg = std::stod(values[9]); // -Y
            double qx = std::stod(values[8]); // X

            // Create the Eigen Quaternion object
            Eigen::Quaternionf quaternion(qw, qx, -qy_neg, -qz_neg);

            // Normalize the quaternion
            quaternion.normalize();

            // Convert the quaternion back to a 3x3 rotation matrix
            Eigen::Matrix3f rotationMatrix = quaternion.toRotationMatrix();
            json rotationJson = json::array();
            for (int i = 0; i < 3; ++i) {
                rotationJson.push_back({ rotationMatrix(i, 0), rotationMatrix(i, 1), rotationMatrix(i, 2) });
            }

            // The 'id', 'img_name', and 'is_key_frame' fields were lost
            json frame;
            frame["id"] = frame_id++;
            frame["img_name"] = "reconstructed_" + std::to_string(frame_id);
            frame["width"] = width;
            frame["height"] = height;
            frame["position"] = position;
            frame["rotation"] = rotationJson;
            frame["fy"] = fy;
            frame["fx"] = fx;
            frame["is_key_frame"] = false; // Placeholder value

            reconstructedJson.push_back(frame);

        }
        catch (const std::invalid_argument& e) {
            std::cerr << "Warning: Could not parse row " << frame_id + 1 << ". Invalid number format." << std::endl;
        }
    }

    inputFile.close();

    std::ofstream outputFile(outputJsonPath);
    if (!outputFile.is_open()) {
        std::cerr << "Error: Could not open the output JSON file for writing at " << outputJsonPath << std::endl;
        return 1;
    }

    outputFile << reconstructedJson.dump(4);
    outputFile.close();

    std::cout << "Successfully converted " << inputCsvPath << " to " << outputJsonPath << std::endl;
    std::cout << "Reconstructed " << frame_id << " frames." << std::endl;

    return 0;
}
