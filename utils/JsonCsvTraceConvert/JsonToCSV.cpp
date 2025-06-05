// SPDX-FileCopyrightText: 2025 Systems and Multimedia Lab @ Rutgers University
// SPDX-License-Identifier: Apache-2.0
//
// This file is part of the 👁️NavGS (EyeNavGS) project.
// See LICENSE_EYENAVGS.txt and NOTICE for details.


#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <string>
#include <nlohmann/json.hpp>
#include <Eigen/Dense>

using json = nlohmann::json;

// Function to calculate FOV in radians
std::tuple<double, double, double, double> calculateFOV(double fx, double fy, double width, double height) {
    double fovX = 2 * atan(width / (2 * fx));  // Field of view in X direction (radians)
    double fovY = 2 * atan(height / (2 * fy)); // Field of view in Y direction (radians)

    // Calculate FOV bounds in radians
    double fov1 = -fovX / 2; // Left FOV
    double fov2 = fovX / 2;  // Right FOV
    double fov3 = -fovY / 2; // Lower FOV
    double fov4 = fovY / 2;  // Upper FOV

    return std::make_tuple(fov1, fov2, fov3, fov4);
}

int main(int argc, char* argv[]) {
    
    if (argc != 3) {
        std::cerr<<"Usage: "<<argv[0]<<" <input JSON file> <output CSV file>" << std::endl;
        return 1;
    }
    //Get the file paths
    std::string inputFilePath = argv[1];
    std::string outputFilePath = argv[2];

    // Read JSON file
    std::ifstream inputFile(inputFilePath);   //Input Json File Path
    if (!inputFile.is_open()) {
        std::cerr << "Could not open the input JSON file." << std::endl;
        return 1;
    }

    // Parse JSON data
    json jsonData;
    inputFile >> jsonData;
    inputFile.close();

    // CSV file to write output
    std::ofstream outputFile(outputFilePath); //Output csv File Path (Used in Monocular Mode Only)
    outputFile << "ViewIndex,FOV1,FOV2,FOV3,FOV4,PositionX,PositionY,PositionZ,QuaternionX,QuaternionY,QuaternionZ,QuaternionW\n";

    // Process each JSON object
    for (const auto& item : jsonData) {
        double fx = item["fx"];
        double fy = item["fy"];
        double width = item["width"];
        double height = item["height"];

        // Calculate FOV in radians
        double fov1, fov2, fov3, fov4;
        std::tie(fov1, fov2, fov3, fov4) = calculateFOV(fx, fy, width, height);

        // Get Position
        const auto& position = item["position"];
        double posX = position[0];
        double posY = position[1];
        double posZ = position[2];

        // Get Rotation Matrix
        Eigen::Matrix3f rotation;
        const auto& rotationData = item["rotation"];
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                rotation(i, j) = rotationData[i][j];
            }
        }

        // Calculate Quaternion
        Eigen::Quaternionf quaternion = Eigen::Quaternionf(rotation);

        // Write to CSV
        outputFile << 0 << ","
            << fov1 << ","
            << fov2 << ","
            << fov3 << ","
            << fov4 << ","
            << posX << ","
            << posY << ","
            << posZ << ","
            << quaternion.w() << ","
            << quaternion.z() << ","
            << -quaternion.y() << ","
            << -quaternion.x() << "\n";
    }

    outputFile.close();
    std::cout << "Data has been written to output.csv." << std::endl;

    return 0;
}