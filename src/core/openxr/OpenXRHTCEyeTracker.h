// SPDX-FileCopyrightText: 2025 Systems and Multimedia Lab @ Rutgers University
// SPDX-License-Identifier: Apache-2.0
//
// This file is part of the 👁️NavGS (EyeNavGS) project.
// See LICENSE_EYENAVGS.md and NOTICE for details.

#pragma once

// Declarations of the vendor extension XR_HTC_eye_tracker.
//
// It is NOT part of the Khronos registry/openxr.h. It is implemented by the "VIVE OpenXR Facial tracking"
// API layer shipped with VIVE Console for SteamVR (XR_APILAYER_VIVE_facial_tracking), on top of SRanipal,
// for the HTC VIVE Pro Eye. It provides per-eye gaze, pupil diameter/position and eye openness.
//
// Source of the definitions (structure type values, struct layouts and function names):
//   HTC's ViveSoftware/VIVE-OpenXR-Unity:
//   - com.htc.upm.vive.openxr/Runtime/OpenXRUtils.cs (XrStructureType values 1000326001..1000326011)
//   - com.htc.upm.vive.openxr/Runtime/Features/EyeTracker/ViveEyeTrackerHelper.cs (struct layouts)
//   - com.htc.upm.vive.openxr/Runtime/Features/EyeTracker/ViveEyeTracker.cs (function names / usage)
// The values were cross-checked against the constants embedded in ViveOpenXRFacialTracking.dll.

#include <openxr/openxr.h>

#ifndef XR_HTC_eye_tracker
#define XR_HTC_eye_tracker 1

#define XR_HTC_EYE_TRACKER_EXTENSION_NAME "XR_HTC_eye_tracker"

XR_DEFINE_HANDLE(XrEyeTrackerHTC)

#define XR_TYPE_EYE_TRACKER_CREATE_INFO_HTC        ((XrStructureType)1000326001)
#define XR_TYPE_EYE_GAZE_DATA_INFO_HTC             ((XrStructureType)1000326002)
#define XR_TYPE_EYE_GAZE_DATA_HTC                  ((XrStructureType)1000326003)
#define XR_TYPE_SYSTEM_EYE_TRACKING_PROPERTIES_HTC ((XrStructureType)1000326005)
#define XR_TYPE_EYE_PUPIL_DATA_INFO_HTC            ((XrStructureType)1000326006)
#define XR_TYPE_EYE_PUPIL_DATA_HTC                 ((XrStructureType)1000326007)
#define XR_TYPE_EYE_GEOMETRIC_DATA_INFO_HTC        ((XrStructureType)1000326009)
#define XR_TYPE_EYE_GEOMETRIC_DATA_HTC             ((XrStructureType)1000326010)

typedef enum XrEyePositionHTC {
    XR_EYE_POSITION_LEFT_HTC = 0,
    XR_EYE_POSITION_RIGHT_HTC = 1,
    XR_EYE_POSITION_COUNT_HTC = 2,
    XR_EYE_POSITION_MAX_ENUM_HTC = 0x7FFFFFFF
} XrEyePositionHTC;

// Chained to XrSystemProperties
typedef struct XrSystemEyeTrackingPropertiesHTC {
    XrStructureType type;
    void* XR_MAY_ALIAS next;
    XrBool32 supportsEyeTracking;
} XrSystemEyeTrackingPropertiesHTC;

typedef struct XrEyeTrackerCreateInfoHTC {
    XrStructureType type;
    const void* XR_MAY_ALIAS next;
} XrEyeTrackerCreateInfoHTC;

typedef struct XrEyeGazeDataInfoHTC {
    XrStructureType type;
    const void* XR_MAY_ALIAS next;
    XrSpace baseSpace;
    XrTime time;
} XrEyeGazeDataInfoHTC;

typedef struct XrSingleEyeGazeDataHTC {
    XrBool32 isValid;
    XrPosef gazePose; // gaze direction = -Z axis of the pose
} XrSingleEyeGazeDataHTC;

typedef struct XrEyeGazeDataHTC {
    XrStructureType type;
    void* XR_MAY_ALIAS next;
    XrTime time;
    XrSingleEyeGazeDataHTC gaze[XR_EYE_POSITION_COUNT_HTC];
} XrEyeGazeDataHTC;

typedef struct XrEyePupilDataInfoHTC {
    XrStructureType type;
    const void* XR_MAY_ALIAS next;
} XrEyePupilDataInfoHTC;

typedef struct XrSingleEyePupilDataHTC {
    XrBool32 isDiameterValid;
    XrBool32 isPositionValid;
    float pupilDiameter;      // millimeters
    XrVector2f pupilPosition; // normalized [0,1] in the eye tracker sensor area, +X right, +Y up
} XrSingleEyePupilDataHTC;

typedef struct XrEyePupilDataHTC {
    XrStructureType type;
    void* XR_MAY_ALIAS next;
    XrTime time;
    XrSingleEyePupilDataHTC pupilData[XR_EYE_POSITION_COUNT_HTC];
} XrEyePupilDataHTC;

typedef struct XrEyeGeometricDataInfoHTC {
    XrStructureType type;
    const void* XR_MAY_ALIAS next;
} XrEyeGeometricDataInfoHTC;

typedef struct XrSingleEyeGeometricDataHTC {
    XrBool32 isValid;
    float eyeOpenness; // [0,1], 0 = closed
    float eyeWide;     // [0,1]
    float eyeSqueeze;  // [0,1]
} XrSingleEyeGeometricDataHTC;

typedef struct XrEyeGeometricDataHTC {
    XrStructureType type;
    void* XR_MAY_ALIAS next;
    XrTime time;
    XrSingleEyeGeometricDataHTC eyeGeometricData[XR_EYE_POSITION_COUNT_HTC];
} XrEyeGeometricDataHTC;

typedef XrResult (XRAPI_PTR *PFN_xrCreateEyeTrackerHTC)(XrSession session, const XrEyeTrackerCreateInfoHTC* createInfo, XrEyeTrackerHTC* eyeTracker);
typedef XrResult (XRAPI_PTR *PFN_xrDestroyEyeTrackerHTC)(XrEyeTrackerHTC eyeTracker);
typedef XrResult (XRAPI_PTR *PFN_xrGetEyeGazeDataHTC)(XrEyeTrackerHTC eyeTracker, const XrEyeGazeDataInfoHTC* gazeInfo, XrEyeGazeDataHTC* eyeGazes);
typedef XrResult (XRAPI_PTR *PFN_xrGetEyePupilDataHTC)(XrEyeTrackerHTC eyeTracker, const XrEyePupilDataInfoHTC* pupilDataInfo, XrEyePupilDataHTC* pupilData);
typedef XrResult (XRAPI_PTR *PFN_xrGetEyeGeometricDataHTC)(XrEyeTrackerHTC eyeTracker, const XrEyeGeometricDataInfoHTC* info, XrEyeGeometricDataHTC* eyeGeometricData);

#endif // XR_HTC_eye_tracker
