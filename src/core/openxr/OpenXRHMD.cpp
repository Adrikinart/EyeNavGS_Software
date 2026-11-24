// Software Name : SIBR_core
// SPDX-FileCopyrightText: Copyright (c) 2023 Orange
// SPDX-License-Identifier: Apache 2.0
//
// This software is distributed under the Apache 2.0 License;
// see the LICENSE file for more details.
//
// Author: Cédric CHEDALEUX <cedric.chedaleux@orange.com> et al.

// SPDX-FileCopyrightText: 2025 Systems and Multimedia Lab @ Rutgers University
// SPDX-License-Identifier: Apache-2.0
//
// This file is part of the 👁️NavGS (EyeNavGS) project.
// It is a modified version of original code from the SIBR project.
// See LICENSE_EYENAVGS.md and NOTICE for details.

#include <cstdlib>
#include <iostream>
#include <stdio.h>
#include <string>
#include <chrono>
#include <cstring>
#include <cmath>
#include <thread>

#include <core/openxr/OpenXRHelper.hpp>
#include <core/openxr/OpenXRHMD.hpp>
#include <core/system/Config.hpp>

namespace sibr
{

    // functions belonging to extensions must be loaded with xrGetInstanceProcAddr before use
    static PFN_xrCreateEyeTrackerFB xrCreateEyeTrackerFB_ = NULL;
    static PFN_xrDestroyEyeTrackerFB xrDestroyEyeTrackerFB_ = NULL;
    static PFN_xrGetEyeGazesFB xrGetEyeGazesFB_ = NULL;
    // XR_HTC_eye_tracker (VIVE Pro Eye through the VIVE OpenXR facial tracking API layer)
    static PFN_xrCreateEyeTrackerHTC xrCreateEyeTrackerHTC_ = NULL;
    static PFN_xrDestroyEyeTrackerHTC xrDestroyEyeTrackerHTC_ = NULL;
    static PFN_xrGetEyeGazeDataHTC xrGetEyeGazeDataHTC_ = NULL;
    static PFN_xrGetEyePupilDataHTC xrGetEyePupilDataHTC_ = NULL;
    static PFN_xrGetEyeGeometricDataHTC xrGetEyeGeometricDataHTC_ = NULL;

    // Look up an extension function, trying alternative names (the VIVE layer manifest and its
    // implementation do not use the same names, e.g. xrGetEyeGazesHTC vs xrGetEyeGazeDataHTC)
    static PFN_xrVoidFunction getProcAddress(XrInstance instance, std::initializer_list<const char*> names)
    {
        for (const char* name : names)
        {
            PFN_xrVoidFunction function = NULL;
            if (XR_SUCCEEDED(xrGetInstanceProcAddr(instance, name, &function)) && function != NULL)
                return function;
        }
        return NULL;
    }

    static bool load_htc_eye_tracker_function_pointers(XrInstance instance)
    {
        xrCreateEyeTrackerHTC_ = (PFN_xrCreateEyeTrackerHTC)getProcAddress(instance, { "xrCreateEyeTrackerHTC" });
        xrDestroyEyeTrackerHTC_ = (PFN_xrDestroyEyeTrackerHTC)getProcAddress(instance, { "xrDestroyEyeTrackerHTC" });
        xrGetEyeGazeDataHTC_ = (PFN_xrGetEyeGazeDataHTC)getProcAddress(instance, { "xrGetEyeGazeDataHTC", "xrGetEyeGazesHTC" });
        xrGetEyePupilDataHTC_ = (PFN_xrGetEyePupilDataHTC)getProcAddress(instance, { "xrGetEyePupilDataHTC", "xrGetPupilDatasHTC" });
        xrGetEyeGeometricDataHTC_ = (PFN_xrGetEyeGeometricDataHTC)getProcAddress(instance, { "xrGetEyeGeometricDataHTC", "xrGetEyeGeometricDatasHTC" });
        SIBR_LOG << "[EyeTracking] XR_HTC_eye_tracker functions: create " << (xrCreateEyeTrackerHTC_ ? "ok" : "MISSING")
                 << ", destroy " << (xrDestroyEyeTrackerHTC_ ? "ok" : "MISSING")
                 << ", gaze " << (xrGetEyeGazeDataHTC_ ? "ok" : "MISSING")
                 << ", pupil " << (xrGetEyePupilDataHTC_ ? "ok" : "missing")
                 << ", geometric " << (xrGetEyeGeometricDataHTC_ ? "ok" : "missing") << std::endl;
        return xrCreateEyeTrackerHTC_ && xrDestroyEyeTrackerHTC_ && xrGetEyeGazeDataHTC_;
    }
    static PFN_xrGetOpenGLGraphicsRequirementsKHR pfnGetOpenGLGraphicsRequirementsKHR = NULL;
#if defined(XR_USE_TIMESPEC)
    static PFN_xrConvertTimespecTimeToTimeKHR pfnConvertTimespecTimeToTimeKHR = NULL;
#elif defined(XR_USE_PLATFORM_WIN32)
    static PFN_xrConvertWin32PerformanceCounterToTimeKHR pfnConvertWin32PerformanceCounterToTimeKHR = NULL;
#endif

    static bool
    load_extension_function_pointers(XrInstance instance, bool loadFbEyeTracking)
    {
        XrResult result =
            xrGetInstanceProcAddr(instance, "xrGetOpenGLGraphicsRequirementsKHR",
                                  (PFN_xrVoidFunction *)&pfnGetOpenGLGraphicsRequirementsKHR);
        if (!xrCheck(instance, result, "Failed to get OpenGL graphics requirements function!"))
            return false;
        if (loadFbEyeTracking) {
            result =
                xrGetInstanceProcAddr(
                    instance, "xrCreateEyeTrackerFB", (PFN_xrVoidFunction*)(&xrCreateEyeTrackerFB_));
            if (!xrCheck(instance, result, "Failed to get eye tracker create function!"))
                return false;

            result = xrGetInstanceProcAddr(
                instance,
                "xrDestroyEyeTrackerFB",
                (PFN_xrVoidFunction*)(&xrDestroyEyeTrackerFB_));
            if (!xrCheck(instance, result, "Failed to get eye tracker destroy function!"))
                return false;

            result = xrGetInstanceProcAddr(
                instance, "xrGetEyeGazesFB", (PFN_xrVoidFunction*)(&xrGetEyeGazesFB_));
            if (!xrCheck(instance, result, "Failed to get eye gazes get function!"))
                return false;
        }
        
#if defined(XR_USE_TIMESPEC)
        result =
            xrGetInstanceProcAddr(instance, "xrConvertTimespecTimeToTimeKHR",
                                  (PFN_xrVoidFunction *)&pfnConvertTimespecTimeToTimeKHR);
        if (!xrCheck(instance, result, "Failed to get OpenXR convert time function!"))
            return false;
#elif defined(XR_USE_PLATFORM_WIN32)
        result =
            xrGetInstanceProcAddr(instance, "xrConvertWin32PerformanceCounterToTimeKHR",
                                  (PFN_xrVoidFunction *)&pfnConvertWin32PerformanceCounterToTimeKHR);
        if (!xrCheck(instance, result, "Failed to get win32 time conversion function!"))
            return false;
#endif

        return true;
    }

    static XrPosef createIdentityPose() {
        XrPosef pose;
        pose.orientation = XrQuaternionf{ 0, 0, 0, 1.0 };
        pose.position = XrVector3f{ 0.f, 0.f, 0.f };
        return pose;
    }

    // we need an identity pose for creating spaces without offsets
    static XrPosef identity_pose = createIdentityPose();


// See https://www.khronos.org/opengl/wiki/Load_OpenGL_Functions
#ifdef _WIN32
    void *GetAnyGLFuncAddress(const char *name)
    {
        void *p = (void *)wglGetProcAddress(name);
        if (p == 0 ||
            (p == (void *)0x1) || (p == (void *)0x2) || (p == (void *)0x3) ||
            (p == (void *)-1))
        {
            HMODULE module = LoadLibraryA("opengl32.dll");
            p = (void *)GetProcAddress(module, name);
        }

        return p;
    }
#endif

    OpenXRHMD::OpenXRHMD(const std::string &applicationName, bool seated) : m_applicationName(applicationName),
                                                                            m_playSpaceType(seated ? XR_REFERENCE_SPACE_TYPE_LOCAL : XR_REFERENCE_SPACE_TYPE_STAGE)
    {
    }

    OpenXRHMD::~OpenXRHMD()
    {
        // Note: the FB eye tracker (if any) is destroyed in closeSession(), while its session still exists
        closeSession();
        terminate();
    }

    const OpenXRHMD::FrameRefreshReport &OpenXRHMD::getRefreshReport() const
    {
        return m_lastFrameRefreshReport;
    }

    Eigen::Vector3f OpenXRHMD::quaternionToEulerAngles(const XrQuaternionf &q)
    {
        // roll (x-axis rotation)
        float sinr_cosp = 2 * (q.w * q.x + q.y * q.z);
        float cosr_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
        float roll = std::atan2(sinr_cosp, cosr_cosp);

        // pitch (y-axis rotation)
        float sinp = std::sqrt(1 + 2 * (q.w * q.y - q.x * q.z));
        float cosp = std::sqrt(1 - 2 * (q.w * q.y - q.x * q.z));
        float pitch =  2.f * std::atan2(sinp, cosp) - (float) M_PI / 2.f;

        // yaw (z-axis rotation)
        float siny_cosp = 2 * (q.w * q.z + q.x * q.y);
        float cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
        float yaw =  std::atan2(siny_cosp, cosy_cosp);

        return Vector3f(roll, pitch, yaw);
    }


    XrQuaternionf OpenXRHMD::eulerAnglestoQuaternion(float roll, float pitch, float yaw) // roll (x), pitch (Y), yaw (z)
    {
        // Abbreviations for the various angular functions

        float cr = cosf(roll * 0.5f);
        float sr = sinf(roll * 0.5f);
        float cp = cosf(pitch * 0.5f);
        float sp = sinf(pitch * 0.5f);
        float cy = cosf(yaw * 0.5f);
        float sy = sinf(yaw * 0.5f);

        return XrQuaternionf{
            cr * cp * cy + sr * sp * sy,
            sr * cp * cy - cr * sp * sy,
            cr * sp * cy + sr * cp * sy,
            cr * cp * sy - sr * sp * cy};
    }

    Eigen::Vector3f OpenXRHMD::getPoseOrientation(OpenXRHMD::Eye eye, AngleUnit unit) const
    {
        uint32_t viewIndex = eyeToViewIndex(eye);
        if (viewIndex >= m_viewCount)
        {
            fprintf(stderr, "View for %s eye does not exist\n", eyeToString(eye));
            return Vector3f::Zero();
        }

        // XrPoseOrientation is a unit quaternion, convert it to euler angles
        Vector3f orientation = quaternionToEulerAngles(views[viewIndex].pose.orientation);
        return (unit == AngleUnit::RADIAN ? orientation : radianToDegree(orientation));
    }

    Eigen::Quaternionf OpenXRHMD::getPoseQuaternion(OpenXRHMD::Eye eye) const
    {
        uint32_t viewIndex = eyeToViewIndex(eye);
        if (viewIndex >= m_viewCount)
        {
            fprintf(stderr, "View for %s eye does not exist\n", eyeToString(eye));
            return Quaternionf();
        }

        // XrPoseOrientation is a unit quaternion
        return Quaternionf{views[viewIndex].pose.orientation.w, views[viewIndex].pose.orientation.x, views[viewIndex].pose.orientation.y, views[viewIndex].pose.orientation.z};
    }

    Eigen::Vector3f OpenXRHMD::getPosePosition(OpenXRHMD::Eye eye) const
    {
        uint32_t viewIndex = eyeToViewIndex(eye);
        if (viewIndex >= m_viewCount)
        {
            fprintf(stderr, "View for %s eye does not exist\n", eyeToString(eye));
            return Vector3f::Zero();
        }

        // XrPosePosition are in world coordinates
        return Vector3f(views[viewIndex].pose.position.x,
                        views[viewIndex].pose.position.y,
                        views[viewIndex].pose.position.z);
    }

    Eigen::Quaternionf OpenXRHMD::getHeadPoseQuaternion() const
    {
        return getPoseQuaternion(Eye::LEFT);
    }

    Eigen::Vector3f OpenXRHMD::getHeadPoseOrientation(AngleUnit unit) const
    {
        return getPoseOrientation(Eye::LEFT);
    }

    Eigen::Vector3f OpenXRHMD::getHeadPosePosition() const
    {
        return (getPosePosition(Eye::LEFT) + getPosePosition(Eye::RIGHT)) / 2.f;
    }

    Eigen::Vector4f OpenXRHMD::getFieldOfView(OpenXRHMD::Eye eye, AngleUnit unit) const
    {
        uint32_t viewIndex = eyeToViewIndex(eye);
        if (viewIndex >= m_viewCount)
        {
            fprintf(stderr, "View for %s eye does not exist\n", eyeToString(eye));
            return Vector4f::Zero();
        }

        // Fov angles are in radians
        Vector4f fov(
            views[viewIndex].fov.angleLeft,
            views[viewIndex].fov.angleRight,
            views[viewIndex].fov.angleDown,
            views[viewIndex].fov.angleUp);
        return (unit == AngleUnit::RADIAN ? fov : radianToDegree(fov));
    }

    Eigen::Vector2f OpenXRHMD::getHVFieldOfView(AngleUnit unit) const
    {
        if (m_viewCount < 2)
        {
            fprintf(stderr, "No view exists\n");
            return Vector2f::Zero();
        }

        // Compute vertical and horizontal fov
        uint32_t viewIndex = eyeToViewIndex(Eye::LEFT);
        Vector2f hvFov(
            views[viewIndex].fov.angleRight - views[viewIndex].fov.angleLeft,
            views[viewIndex].fov.angleUp - views[viewIndex].fov.angleDown);

        // Fov angles are in radians
        return (unit == AngleUnit::RADIAN ? hvFov : radianToDegree(hvFov));
    }

    Eigen::Vector2f OpenXRHMD::getScreenCenter(Eye eye) const
    {
        uint32_t viewIndex = eyeToViewIndex(eye);
        if (viewIndex >= m_viewCount)
        {
            fprintf(stderr, "View for %s eye does not exist\n", eyeToString(eye));
            return Vector2f::Zero();
        }

        // Fov angles are in radians
        float tanLeft = tan(abs(views[viewIndex].fov.angleLeft));
        float tanRight = tan(abs(views[viewIndex].fov.angleRight));
        float tanUp = tan(abs(views[viewIndex].fov.angleUp));
        float tanDown = tan(abs(views[viewIndex].fov.angleDown));
        float centerX = tanLeft / (tanLeft + tanRight);
        float centerY = tanDown / (tanDown + tanUp);
        return Vector2f(centerX, centerY);
    }

    float OpenXRHMD::getInterPupillaryDistance() const
    {
        const auto left = getPosePosition(Eye::LEFT);
        const auto right = getPosePosition(Eye::RIGHT);
        return sqrt(powf(left.x() - right.x(), 2) + powf(left.y() - right.y(), 2) + powf(left.z() - right.z(), 2));
    }

    bool OpenXRHMD::init()
    {
        XrResult result = XR_SUCCESS;

        if (m_printApiLayers)
            printApiLayers();

        // xrEnumerate*() functions are usually called once with CapacityInput = 0.
        // The function will write the required amount into CountOutput. We then have
        // to allocate an array to hold CountOutput elements and call the function
        // with CountOutput as CapacityInput.
        uint32_t ext_count = 0;
        result = xrEnumerateInstanceExtensionProperties(NULL, 0, &ext_count, NULL);

        /* TODO: instance null will not be able to convert XrResult to string */
        if (!xrCheck(NULL, result, "Failed to enumerate number of extension properties"))
            return false;

        XrExtensionProperties *ext_props = (XrExtensionProperties *)malloc(sizeof(XrExtensionProperties) * ext_count);
        for (uint16_t i = 0; i < ext_count; i++)
        {
            // we usually have to fill in the type (for validation) and set
            // next to NULL (or a pointer to an extension specific struct)
            ext_props[i].type = XR_TYPE_EXTENSION_PROPERTIES;
            ext_props[i].next = NULL;
        }

        // Extensions always required
        std::vector<const char *> enabledExtensions = {
            XR_KHR_OPENGL_ENABLE_EXTENSION_NAME,
#if defined(XR_USE_PLATFORM_WIN32)
            XR_KHR_WIN32_CONVERT_PERFORMANCE_COUNTER_TIME_EXTENSION_NAME,
#elif defined(XR_USE_TIMESPEC)
            XR_KHR_CONVERT_TIMESPEC_TIME_EXTENSION_NAME,
#endif
        };

        result = xrEnumerateInstanceExtensionProperties(NULL, ext_count, &ext_count, ext_props);
        if (!xrCheck(NULL, result, "Failed to enumerate extension properties"))
            return false;

        bool opengl_supported = false;
        bool fbEyeTrackingAvailable = false;
        bool extEyeGazeAvailable = false;
        bool htcEyeTrackerAvailable = false;

        if (m_printRuntimeExtensions)
            SIBR_LOG << "Runtime supports " << ext_count << " extensions" << std::endl;
        for (uint32_t i = 0; i < ext_count; i++)
        {
            if (m_printRuntimeExtensions)
                SIBR_LOG << "\t" << ext_props[i].extensionName << " v" << ext_props[i].extensionVersion << std::endl;
            if (strcmp(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME, ext_props[i].extensionName) == 0)
            {
                opengl_supported = true;
            }
            if (strcmp(XR_FB_EYE_TRACKING_SOCIAL_EXTENSION_NAME, ext_props[i].extensionName) == 0)
                fbEyeTrackingAvailable = true;
            if (strcmp(XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME, ext_props[i].extensionName) == 0)
                extEyeGazeAvailable = true;
            if (strcmp(XR_HTC_EYE_TRACKER_EXTENSION_NAME, ext_props[i].extensionName) == 0)
                htcEyeTrackerAvailable = true;
        }
        free(ext_props);

        // A graphics extension like OpenGL is required to draw anything in VR
        if (!opengl_supported)
        {
            printf("Runtime does not support OpenGL extension!\n");
            return false;
        }

        // Select the eye tracking backend:
        //  - XR_FB_eye_tracking_social (Meta Quest Pro): per-eye gaze, used by the original EyeNavGS dataset
        //  - XR_EXT_eye_gaze_interaction (HTC VIVE Pro Eye via VIVE Console/SteamVR, Varjo, ...): combined gaze
        // The environment variable EYENAVGS_EYE_TRACKING=fb|ext|none forces a choice (when available).
        SIBR_LOG << "[EyeTracking] " << XR_FB_EYE_TRACKING_SOCIAL_EXTENSION_NAME << " available: " << (fbEyeTrackingAvailable ? "yes" : "no")
                 << ", " << XR_HTC_EYE_TRACKER_EXTENSION_NAME << " available: " << (htcEyeTrackerAvailable ? "yes" : "no")
                 << ", " << XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME << " available: " << (extEyeGazeAvailable ? "yes" : "no") << std::endl;
        if (!htcEyeTrackerAvailable && !fbEyeTrackingAvailable)
        {
            SIBR_LOG << "[EyeTracking] Note: for the HTC VIVE Pro Eye, enable the 'VIVE OpenXR Facial tracking' API layer "
                     << "(SteamVR > Settings > OpenXR > Manage OpenXR API layers) to get XR_HTC_eye_tracker." << std::endl;
        }
        std::string eyeTrackingPreference = "auto";
        {
#ifdef _WIN32
            char *envValue = nullptr;
            size_t envLength = 0;
            if (_dupenv_s(&envValue, &envLength, "EYENAVGS_EYE_TRACKING") == 0 && envValue != nullptr)
            {
                eyeTrackingPreference = envValue;
                free(envValue);
            }
#else
            const char *envValue = std::getenv("EYENAVGS_EYE_TRACKING");
            if (envValue != nullptr)
                eyeTrackingPreference = envValue;
#endif
            if (eyeTrackingPreference != "auto")
                SIBR_LOG << "[EyeTracking] EYENAVGS_EYE_TRACKING=" << eyeTrackingPreference << std::endl;
        }
        m_eyeTrackingBackend = EyeTrackingBackend::NONE;
        if (eyeTrackingPreference == "none")
        {
            m_eyeTrackingBackend = EyeTrackingBackend::NONE;
        }
        else if (eyeTrackingPreference == "ext")
        {
            if (extEyeGazeAvailable)
                m_eyeTrackingBackend = EyeTrackingBackend::EXT_EYE_GAZE_INTERACTION;
        }
        else if (eyeTrackingPreference == "fb")
        {
            if (fbEyeTrackingAvailable)
                m_eyeTrackingBackend = EyeTrackingBackend::FB_EYE_TRACKING_SOCIAL;
        }
        else if (eyeTrackingPreference == "htc")
        {
            if (htcEyeTrackerAvailable)
                m_eyeTrackingBackend = EyeTrackingBackend::HTC_EYE_TRACKER;
        }
        else // auto
        {
            if (fbEyeTrackingAvailable)
                m_eyeTrackingBackend = EyeTrackingBackend::FB_EYE_TRACKING_SOCIAL;
            else if (htcEyeTrackerAvailable)
                m_eyeTrackingBackend = EyeTrackingBackend::HTC_EYE_TRACKER;
            else if (extEyeGazeAvailable)
                m_eyeTrackingBackend = EyeTrackingBackend::EXT_EYE_GAZE_INTERACTION;
        }
        if (m_eyeTrackingBackend == EyeTrackingBackend::FB_EYE_TRACKING_SOCIAL)
            enabledExtensions.push_back(XR_FB_EYE_TRACKING_SOCIAL_EXTENSION_NAME);
        else if (m_eyeTrackingBackend == EyeTrackingBackend::HTC_EYE_TRACKER)
            enabledExtensions.push_back(XR_HTC_EYE_TRACKER_EXTENSION_NAME);
        else if (m_eyeTrackingBackend == EyeTrackingBackend::EXT_EYE_GAZE_INTERACTION)
            enabledExtensions.push_back(XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME);
        else
            SIBR_WRG << "[EyeTracking] No eye tracking extension available/selected: gaze columns will be recorded as identity (GazeValid=0)" << std::endl;
        SIBR_LOG << "[EyeTracking] Selected backend: " << getEyeTrackingBackendName() << std::endl;

        if (m_printRuntimeExtensions)
        {
            SIBR_LOG << "Enable following extensions:" << std::endl;
            for (auto ext : enabledExtensions)
            {
                SIBR_LOG << "\t" << ext << std::endl;
            }
        }

        // Create XrInstance
        XrInstanceCreateInfo instanceCreateInfo;
        instanceCreateInfo.type = XR_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.next = NULL;
        instanceCreateInfo.createFlags = 0;
        XrApplicationInfo applicationInfo;
        applicationInfo.applicationVersion = 1;
        applicationInfo.engineVersion = 0;
        applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
        instanceCreateInfo.applicationInfo = applicationInfo;
        instanceCreateInfo.enabledApiLayerCount = 0;
        instanceCreateInfo.enabledApiLayerNames = NULL;

        instanceCreateInfo.enabledExtensionCount = (uint32_t)enabledExtensions.size();
        instanceCreateInfo.enabledExtensionNames = enabledExtensions.data();

        stringCopy(instanceCreateInfo.applicationInfo.applicationName, m_applicationName.c_str(),
            XR_MAX_APPLICATION_NAME_SIZE);
        stringCopy(instanceCreateInfo.applicationInfo.engineName, "SIBR_core", XR_MAX_ENGINE_NAME_SIZE);

        result = xrCreateInstance(&instanceCreateInfo, &m_instance);
        if (!xrCheck(NULL, result, "Failed to create XR m_instance.")) 
            return false;
        


        if (!load_extension_function_pointers(m_instance, m_eyeTrackingBackend == EyeTrackingBackend::FB_EYE_TRACKING_SOCIAL))
            return false;
        if (m_eyeTrackingBackend == EyeTrackingBackend::HTC_EYE_TRACKER && !load_htc_eye_tracker_function_pointers(m_instance))
        {
            SIBR_WRG << "[EyeTracking] XR_HTC_eye_tracker functions not found: eye tracking disabled" << std::endl;
            m_eyeTrackingBackend = EyeTrackingBackend::NONE;
        }

        // Get runtime name and version
        if (!getRuntimeNameAndVersion(m_instance, m_runtimeName, m_runtimeVersion))
            SIBR_LOG << "Unable to retrieve OpenXR runtime name and version" << std::endl;

        // --- Get XrSystemId
        XrSystemGetInfo systemGetInfo;
        systemGetInfo.type = XR_TYPE_SYSTEM_GET_INFO;
        systemGetInfo.next = NULL;
        systemGetInfo.formFactor = m_formFactor;

        result = xrGetSystem(m_instance, &systemGetInfo, &m_systemId);
        if (!xrCheck(m_instance, result, "Failed to get system for HMD form factor."))
            return false;

        if (m_printSystemProperties)
            printSystemProperties(m_instance, m_systemId);

        if (m_eyeTrackingBackend == EyeTrackingBackend::HTC_EYE_TRACKER)
        {
            XrSystemEyeTrackingPropertiesHTC htcProperties{ XR_TYPE_SYSTEM_EYE_TRACKING_PROPERTIES_HTC };
            htcProperties.next = NULL;
            htcProperties.supportsEyeTracking = XR_FALSE;
            XrSystemProperties systemProperties{ XR_TYPE_SYSTEM_PROPERTIES };
            systemProperties.next = &htcProperties;
            result = xrGetSystemProperties(m_instance, m_systemId, &systemProperties);
            if (XR_SUCCEEDED(result))
            {
                SIBR_LOG << "[EyeTracking] System '" << systemProperties.systemName << "': XR_HTC_eye_tracker supportsEyeTracking = "
                         << (htcProperties.supportsEyeTracking ? "true" : "false") << std::endl;
                if (!htcProperties.supportsEyeTracking)
                    SIBR_WRG << "[EyeTracking] The VIVE layer reports no eye tracking support (SRanipal not ready, eye tracking disabled, or not calibrated?). "
                             << "The eye tracker will be created anyway; check the GazeValid column." << std::endl;
            }
            else
            {
                xrCheck(m_instance, result, "Failed to get system properties for XR_HTC_eye_tracker");
            }
        }

        if (m_eyeTrackingBackend == EyeTrackingBackend::EXT_EYE_GAZE_INTERACTION)
        {
            // Ask the system whether eye gaze interaction is actually supported
            XrSystemEyeGazeInteractionPropertiesEXT eyeGazeProperties{ XR_TYPE_SYSTEM_EYE_GAZE_INTERACTION_PROPERTIES_EXT };
            eyeGazeProperties.next = NULL;
            eyeGazeProperties.supportsEyeGazeInteraction = XR_FALSE;
            XrSystemProperties systemProperties{ XR_TYPE_SYSTEM_PROPERTIES };
            systemProperties.next = &eyeGazeProperties;
            result = xrGetSystemProperties(m_instance, m_systemId, &systemProperties);
            if (XR_SUCCEEDED(result))
            {
                SIBR_LOG << "[EyeTracking] System '" << systemProperties.systemName << "': supportsEyeGazeInteraction = "
                         << (eyeGazeProperties.supportsEyeGazeInteraction ? "true" : "false") << std::endl;
                if (!eyeGazeProperties.supportsEyeGazeInteraction)
                {
                    // Keep the action anyway: some layers report false until the eye tracker is ready.
                    // The GazeValid column of the trace tells whether gaze was really delivered.
                    SIBR_WRG << "[EyeTracking] The runtime reports no eye gaze support (eye tracker/SRanipal not running or not calibrated?). "
                             << "Gaze will be recorded only if the runtime delivers it." << std::endl;
                }
            }
            else
            {
                xrCheck(m_instance, result, "Failed to get system properties for eye gaze interaction");
            }
        }

        result = xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_viewType, 0, &m_viewCount, NULL);
        if (!xrCheck(m_instance, result, "Failed to get view configuration view count!"))
            return false;

        m_viewConfigViews = (XrViewConfigurationView *)malloc(sizeof(XrViewConfigurationView) * m_viewCount);
        for (uint32_t i = 0; i < m_viewCount; i++)
        {
            m_viewConfigViews[i].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
            m_viewConfigViews[i].next = NULL;
        }

        result = xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_viewType, m_viewCount,
                                                   &m_viewCount, m_viewConfigViews);
        if (!xrCheck(m_instance, result, "Failed to enumerate view configuration views!"))
            return false;

        if (m_printViewConfigInfos)
            printViewconfigViewInfo(m_viewCount, m_viewConfigViews);

        // Now we have the recommended resolution for the headset, set the output resolution
        m_resolution = getRecommendedResolution();

        // OpenXR requires checking graphics requirements before creating a m_session.
        XrGraphicsRequirementsOpenGLKHR opengl_reqs;
        opengl_reqs.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR;
        opengl_reqs.next = NULL;

        // this function pointer was loaded with xrGetInstanceProcAddr
        result = pfnGetOpenGLGraphicsRequirementsKHR(m_instance, m_systemId, &opengl_reqs);
        if (!xrCheck(m_instance, result, "Failed to get OpenGL graphics requirements!"))
            return false;

        return true;
    }

    Eigen::Vector2i OpenXRHMD::getRecommendedResolution() const
    {
        return (m_viewCount > 0 ? Vector2i(m_viewConfigViews[0].recommendedImageRectWidth, m_viewConfigViews[0].recommendedImageRectHeight) : Vector2i::Zero());
    }

    void OpenXRHMD::setResolution(const Eigen::Vector2i &resolution)
    {
        if (!isSessionRunning())
        {
            m_resolution = resolution;
        }
        else
        {
            SIBR_WRG << "Cannot change the resolution: XR session is already running." << std::endl;
        }
    }

    const Eigen::Vector2i &OpenXRHMD::getResolution() const
    {
        return m_resolution;
    }

    template <typename T>
    bool OpenXRHMD::startSession(T graphicsBindingGL)
    {
        // Initialize the OpenGL stack to be able to render textures for each eye
        // and submit the OpenGL framebuffer to OpenXR
        GLenum err = glewInit();
        if (GLEW_OK != err)
        {
            printf("Error initializing OpenGL: %u\n", err);
            return false;
        }

        SIBR_LOG << "Starting XR session: OpenGL version = " << glGetString(GL_VERSION) << ", renderer = " << glGetString(GL_RENDERER) << std::endl;

        XrSessionCreateInfo sessionCreateInfo;
        sessionCreateInfo.type = XR_TYPE_SESSION_CREATE_INFO;
        sessionCreateInfo.next = &graphicsBindingGL;
        sessionCreateInfo.createFlags = 0;
        sessionCreateInfo.systemId = m_systemId;
        if (!(createSession(sessionCreateInfo) && createReferenceSpace() && createSwapchain())) {
            return false;
        }

        m_xrInput = std::make_unique<OpenXRInput>(m_instance, m_session, m_playSpace,
                                                  m_eyeTrackingBackend == EyeTrackingBackend::EXT_EYE_GAZE_INTERACTION);

        return synchronizeSession();
    }

#ifdef XR_USE_PLATFORM_XLIB
    template bool OpenXRHMD::startSession<XrGraphicsBindingOpenGLXlibKHR>(XrGraphicsBindingOpenGLXlibKHR);
#endif
#ifdef XR_USE_PLATFORM_WIN32
    template bool OpenXRHMD::startSession<XrGraphicsBindingOpenGLWin32KHR>(XrGraphicsBindingOpenGLWin32KHR);
#endif

    bool OpenXRHMD::createSession(const XrSessionCreateInfo& sessionCreateInfo)
    {
        XrResult result = xrCreateSession(m_instance, &sessionCreateInfo, &m_session);
        if (!xrCheck(m_instance, result, "Failed to create session"))
            return false;
        return true;
    }

    bool OpenXRHMD::createReferenceSpace()
    {
        /* Many runtimes support at least STAGE and LOCAL but not all do.
         * Sophisticated apps might check with xrEnumerateReferenceSpaces() if the
         * chosen one is supported and try another one if not.
         * Here we will get an error from xrCreateReferenceSpace() and exit. */
        XrReferenceSpaceCreateInfo m_playSpaceCreateInfo;
        m_playSpaceCreateInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
        m_playSpaceCreateInfo.next = NULL;
        m_playSpaceCreateInfo.referenceSpaceType = m_playSpaceType;
        m_playSpaceCreateInfo.poseInReferenceSpace = initial_adjustment;

        XrResult result = xrCreateReferenceSpace(m_session, &m_playSpaceCreateInfo, &m_playSpace);
        if (!xrCheck(m_instance, result, "Failed to create play space!"))
        {
            return false;
        }
        result = xrGetReferenceSpaceBoundsRect(m_session, m_playSpaceCreateInfo.referenceSpaceType, &m_playBounds);
        xrCheck(m_instance, result, "Failed to retrieve play space bounds!");

        gaze_space = m_playSpace;
        return true;
    }

    bool OpenXRHMD::createSwapchain()
    {
        // Create Swapchains
        uint32_t swapchain_format_count;
        XrResult result = xrEnumerateSwapchainFormats(m_session, 0, &swapchain_format_count, NULL);
        if (!xrCheck(m_instance, result, "Failed to get number of supported swapchain formats"))
            return false;

        int64_t *swapchain_formats = new int64_t[swapchain_format_count];
        result = xrEnumerateSwapchainFormats(m_session, swapchain_format_count, &swapchain_format_count,
                                             swapchain_formats);
        if (!xrCheck(m_instance, result, "Failed to enumerate swapchain formats"))
            return false;

        // Select swapchain with SRGB format
        int64_t color_format = selectSwapchainFormat(m_instance, m_session, GL_SRGB8_ALPHA8_EXT, true);

        // Create swapchain for main VR rendering
        {
            // We request the runtime here to create a swapchain for each view. The swapchain
            // attributes define the size (width, height) of the textures that application should render to
            // as well as number of texture to perform single, double or triple buffering
            // Here, we use values (resolution, sample count) recommended by the runtime
            m_swapchains = (XrSwapchain_T **)malloc(sizeof(XrSwapchain) * m_viewCount);
            m_swapchainsLengths = (uint32_t *)malloc(sizeof(uint32_t) * m_viewCount);
            m_swapchainsImages = (XrSwapchainImageOpenGLKHR **)malloc(sizeof(XrSwapchainImageOpenGLKHR *) * m_viewCount);
            for (uint32_t i = 0; i < m_viewCount; i++)
			{
				XrSwapchainCreateInfo swapchainCreateInfo;
				swapchainCreateInfo.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
				swapchainCreateInfo.next = NULL;
				swapchainCreateInfo.createFlags = 0;
				swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
				swapchainCreateInfo.format = color_format;
				swapchainCreateInfo.sampleCount = m_viewConfigViews[i].recommendedSwapchainSampleCount;
				swapchainCreateInfo.width = (uint32_t)m_resolution.x();
				swapchainCreateInfo.height = (uint32_t)m_resolution.y();
				swapchainCreateInfo.faceCount = 1;
				swapchainCreateInfo.arraySize = 1;
				swapchainCreateInfo.mipCount = 1;

                result = xrCreateSwapchain(m_session, &swapchainCreateInfo, &m_swapchains[i]);
                if (!xrCheck(m_instance, result, "Failed to create swapchain %d!", i))
                    return false;

                // The runtime controls how many textures we have to be able to render to
                result = xrEnumerateSwapchainImages(m_swapchains[i], 0, &m_swapchainsLengths[i], NULL);
                if (!xrCheck(m_instance, result, "Failed to enumerate m_swapchains"))
                    return false;

                m_swapchainsImages[i] = (XrSwapchainImageOpenGLKHR *)malloc(sizeof(XrSwapchainImageOpenGLKHR) * m_swapchainsLengths[i]);
                for (uint32_t j = 0; j < m_swapchainsLengths[i]; j++)
                {
                    m_swapchainsImages[i][j].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
                    m_swapchainsImages[i][j].next = NULL;
                }
                result =
                    xrEnumerateSwapchainImages(m_swapchains[i], m_swapchainsLengths[i], &m_swapchainsLengths[i],
                                               (XrSwapchainImageBaseHeader *)m_swapchainsImages[i]);
                if (!xrCheck(m_instance, result, "Failed to enumerate swapchain images"))
                    return false;
            }
        }

        // Do not allocate these every frame to save some resources
        views = (XrView *)malloc(sizeof(XrView) * m_viewCount);
        for (uint32_t i = 0; i < m_viewCount; i++)
        {
            views[i].type = XR_TYPE_VIEW;
            views[i].next = NULL;
        }

        m_projectionViews = (XrCompositionLayerProjectionView *)malloc(
            sizeof(XrCompositionLayerProjectionView) * m_viewCount);
        for (uint32_t i = 0; i < m_viewCount; i++)
        {
            m_projectionViews[i].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
            m_projectionViews[i].next = NULL;

            m_projectionViews[i].subImage.swapchain = m_swapchains[i];
            m_projectionViews[i].subImage.imageArrayIndex = 0;
            m_projectionViews[i].subImage.imageRect.offset.x = 0;
            m_projectionViews[i].subImage.imageRect.offset.y = 0;
            m_projectionViews[i].subImage.imageRect.extent.width = m_resolution.x();
            m_projectionViews[i].subImage.imageRect.extent.height = m_resolution.y();
        };

        return true;
    }

    bool OpenXRHMD::synchronizeSession()
    {
        // Keep polling events until we successfully synchronized with the headset
        while (m_status != SessionStatus::SYNCHRONIZED)
        {
            pollEvents();

            // Damn, we got an error!
            if (m_status == SessionStatus::FAILURE)
            {
                return false;
            }
        }

        return true;
    }

    bool OpenXRHMD::pollEvents()
    {
        // Handle runtime Events
        XrEventDataBuffer runtimeEvent;
        runtimeEvent.type = XR_TYPE_EVENT_DATA_BUFFER;
        runtimeEvent.next = NULL;

        XrResult poll_result = xrPollEvent(m_instance, &runtimeEvent);
        if (poll_result == XR_SUCCESS)
        {
            switch (runtimeEvent.type)
            {
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
            {
                XrEventDataInstanceLossPending *event = (XrEventDataInstanceLossPending *)&runtimeEvent;
                SIBR_WRG << "EVENT: instance loss pending at " << event->lossTime << "! Destroying instance." << std::endl;
                XrResult result = xrDestroyInstance(m_instance);
                if (!xrCheck(NULL, result, "Failed to destroy XR instance."))
                {
                    m_status = SessionStatus::FAILURE;
                }
                break; // (upstream fell through and decoded this event as a session state change)
            }
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
            {
                XrEventDataSessionStateChanged *event = (XrEventDataSessionStateChanged *)&runtimeEvent;
                updateCurrentSessionState(event->state);
                break;
            }
            case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
            {
                // Do not handle it for now
                break;
            }
            default:
                SIBR_LOG << "EVENT: Unhandled event (type " << runtimeEvent.type << ")" << std::endl;
            }
        }
        else if (poll_result == XR_EVENT_UNAVAILABLE)
        {
            // No new events
        }
        else
        {
            SIBR_WRG << "EVENT: Failed to poll XR Runtime events!" << std::endl;
        }

        // In case of transitory states, keep polling
        bool keepPolling = m_status == SessionStatus::BEGINNING ||
                           m_status == SessionStatus::ENDING;
        if (m_status == SessionStatus::BEGINNING)
        {
            // Ok, we initiated the m_session, the runtime is now awaiting for a first frame loop
            // synchronization (see https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrSessionState.html).
            // Let's call xrWaitFrame, xrBeginFrame and xrEndFrame to trigger the state change to
            // SYNCHRONIZED state. Let's submitting an empty frame.
            // Hopefully, we will get the first head-pose information
            waitNextFrame();
            submitFrame();
        }

        if (keepPolling)
        {
            return pollEvents();
        }

        return m_status != SessionStatus::FAILURE;
    }

    XrQuaternionf OpenXRHMD::multiply(const Vector4f q1, const XrQuaternionf& q2) {
        XrQuaternionf result;
        result.w = q1.w() * q2.w - q1.x() * q2.x - q1.y() * q2.y - q1.z() * q2.z;
        result.x = q1.w() * q2.x + q1.x() * q2.w + q1.y() * q2.z - q1.z() * q2.y;
        result.y = q1.w() * q2.y - q1.x() * q2.z + q1.y() * q2.w + q1.z() * q2.x;
        result.z = q1.w() * q2.z + q1.x() * q2.y - q1.y() * q2.x + q1.z() * q2.w;
        return result;
    }
    void OpenXRHMD::setInitialPose(Eigen::Vector3f pos, Eigen::Vector4f q, float s) {

        initial_adjustment.position.x = pos.x() + identity_pose.position.x;
        initial_adjustment.position.y = pos.y() + identity_pose.position.y;
        initial_adjustment.position.z = pos.z() + identity_pose.position.z;
        initial_adjustment.orientation = multiply(q, identity_pose.orientation);
        scale = s;
    }

    void OpenXRHMD::setIdleAppCallback(const std::function<void()> &callback)
    {
        m_idleCallback = callback;
    }

    void OpenXRHMD::setVisibleAppCallback(const std::function<void()> &callback)
    {
        m_visibleCallback = callback;
    }

    void OpenXRHMD::setFocusedAppCallback(const std::function<void()> &callback)
    {
        m_focusedCallback = callback;
    }

    bool OpenXRHMD::isSessionRunning() const
    {
        return m_status != SessionStatus::FAILURE && m_status != SessionStatus::STOPPED;
    }

    void OpenXRHMD::updateCurrentSessionState(const XrSessionState &state)
    {
        SIBR_LOG << "XR session state change: '" << sessionStateToString(m_currentState) << "' -> '" << sessionStateToString(state) << "'" << std::endl;
        m_currentState = state;

        /*
         * React to session state changes, see OpenXR spec 9.3 diagram. What we need to react to:
         *
         * * READY -> xrBeginSession STOPPING -> xrEndSession (note that the same session can be restarted)
         * * EXITING -> xrDestroySession (EXITING only happens after we went through STOPPING and called xrEndSession)
         * * IDLE -> don't run render loop, but keep polling for events
         * * SYNCHRONIZED, VISIBLE, FOCUSED -> run render loop
         */
        switch (m_currentState)
        {
        case XR_SESSION_STATE_MAX_ENUM: // must be a bug
        case XR_SESSION_STATE_UNKNOWN:
            break; // state handling switch

        // normal state
        case XR_SESSION_STATE_IDLE:
        {
            m_status = SessionStatus::IDLE;
            if (m_idleCallback)
            {
                m_idleCallback();
            }
            break; // state handling switch
        }
        case XR_SESSION_STATE_SYNCHRONIZED:
        {
            // Successful session start
            m_status = SessionStatus::SYNCHRONIZED;
            break; // state handling switch
        }
        case XR_SESSION_STATE_FOCUSED:
        {
            if (m_focusedCallback)
            {
                m_focusedCallback();
            }
            break; // state handling switch
        }
        case XR_SESSION_STATE_VISIBLE:
        {
            if (m_visibleCallback)
            {
                m_visibleCallback();
            }
            break; // state handling switch
        }

        // Session state is ready, let's request for beginning the m_session
        case XR_SESSION_STATE_READY:
        {
            // start session only if it is not starting, i.e. not when we already called xrBeginSession
            // but the runtime did not switch to the next state yet
            if (m_status != SessionStatus::BEGINNING)
            {
                XrSessionBeginInfo m_sessionBeginInfo;
                m_sessionBeginInfo.type = XR_TYPE_SESSION_BEGIN_INFO;
                m_sessionBeginInfo.next = NULL;
                m_sessionBeginInfo.primaryViewConfigurationType = m_viewType;
                XrResult result = xrBeginSession(m_session, &m_sessionBeginInfo);
                if (!xrCheck(m_instance, result, "Failed to begin session!"))
                {
                    m_status = SessionStatus::FAILURE;
                    return;
                }
                m_status = SessionStatus::BEGINNING;
            }
            break; // state handling switch
        }
        case XR_SESSION_STATE_STOPPING:
        {
            // end session only if it is running, i.e. not when we already called xrEndSession but the
            // runtime did not switch to the next state yet
            if (m_status != SessionStatus::ENDING)
            {
                XrResult result = xrEndSession(m_session);
                if (!xrCheck(m_instance, result, "Failed to end session!"))
                {
                    m_status = SessionStatus::FAILURE;
                    return;
                }
                m_status = SessionStatus::ENDING;
                // On sucess, session will go to STATE_IDLE
            }
            break; // state handling switch
        }
        // Destroy session
        case XR_SESSION_STATE_LOSS_PENDING: // The session is in the process of being lost
        case XR_SESSION_STATE_EXITING:
            XrResult result = xrDestroySession(m_session);
            if (!xrCheck(m_instance, result, "Failed to destroy session!"))
            {
                m_status = SessionStatus::FAILURE;
                return;
            }
            m_session = XR_NULL_HANDLE;
            m_status = SessionStatus::STOPPED;
            break; // state handling switch
        }
    }

    bool OpenXRHMD::waitNextFrame()
    {
        // Wait for head-pose move predication to render next frame
        m_lastFrameState.type = XR_TYPE_FRAME_STATE;
        m_lastFrameState.next = NULL;
        m_lastFrameState.predictedDisplayPeriod = 0;
        m_lastFrameState.predictedDisplayTime = 0;
        XrFrameWaitInfo frameWaitInfo;
        frameWaitInfo.type = XR_TYPE_FRAME_WAIT_INFO;
        frameWaitInfo.next = NULL;
        XR_TRACE("xrWaitFrame ->");
        XrResult result = xrWaitFrame(m_session, &frameWaitInfo, &m_lastFrameState);
        XR_TRACE("xrWaitFrame <- " << result);
        if (!xrCheck(m_instance, result, "xrWaitFrame() failed"))
            return false;

        XrTime now_ns = 0;
#if defined(XR_USE_PLATFORM_WIN32)
        LARGE_INTEGER ticks;
        if (QueryPerformanceCounter(&ticks))
        {
            pfnConvertWin32PerformanceCounterToTimeKHR(m_instance, &ticks, &now_ns); // See https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrConvertTimeToWin32PerformanceCounterKHR.html
        }
        else
        {
            SIBR_WRG << "Failed to get performance counter" << std::endl;
        }
#elif defined(XR_USE_TIMESPEC)
        struct timespec timespecTime;
        clock_gettime(CLOCK_MONOTONIC, &timespecTime);
        pfnConvertTimespecTimeToTimeKHR(m_instance, &timespecTime, &now_ns); // See https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_KHR_convert_timespec_time
#endif

        // Get the new view locations
        XrViewLocateInfo viewLocateInfo;
        viewLocateInfo.type = XR_TYPE_VIEW_LOCATE_INFO;
        viewLocateInfo.next = NULL;
        viewLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        viewLocateInfo.displayTime = m_lastFrameState.predictedDisplayTime;
        viewLocateInfo.space = m_playSpace;

        // (no designated initializers: they need C++20, and the rest of SIBR is compiled in C++14)
        XrViewState view_state{XR_TYPE_VIEW_STATE};
        view_state.next = NULL;
        XR_TRACE("xrLocateViews ->");
        result = xrLocateViews(m_session, &viewLocateInfo, &view_state, m_viewCount, &m_viewCount, views);
        XR_TRACE("xrLocateViews <- " << result);
        if (!xrCheck(m_instance, result, "Could not locate views"))
            return false;
        // Scaling the position
        for (uint32_t i = 0; i < m_viewCount; i++) {
            views[i].pose.position = applyInitialScale(views[i].pose.position);
        }

        // Eye gaze is only queried when the application is focused and the head pose is valid:
        // gaze in world space is meaningless otherwise, and the VIVE eye tracking layer was observed to
        // abort() the process when its gaze was located while the headset was not tracked.
        // The conditions must hold for 1 s (warm-up: the headset leaves standby, SRanipal starts its eye cameras).
        m_lastViewStateFlags = view_state.viewStateFlags;
        const XrViewStateFlags headValid = XR_VIEW_STATE_ORIENTATION_VALID_BIT | XR_VIEW_STATE_POSITION_VALID_BIT;
        const bool gazeConditions = isFocused() && ((m_lastViewStateFlags & headValid) == headValid) && !m_eyeTrackingFailed;
        const auto nowSteady = std::chrono::steady_clock::now();
        if (!gazeConditions)
            m_gazeConditionsSince = std::chrono::steady_clock::time_point{};
        else if (m_gazeConditionsSince == std::chrono::steady_clock::time_point{})
            m_gazeConditionsSince = nowSteady;
        m_gazeQueryAllowed = gazeConditions && (nowSteady - m_gazeConditionsSince) >= std::chrono::seconds(1);

        // m_xrInput is reset in closeSession(), which can still poll events (and thus get here)
        if (!m_xrInput)
            return true;
        m_xrInput->setEyeGazeQueryAllowed(m_gazeQueryAllowed);

        // Sync input actions (controllers and, with XR_EXT_eye_gaze_interaction, the eye gaze pose)
        if(!m_xrInput->sync(m_lastFrameState.predictedDisplayTime)) {
            return false;
        }

        // Update eye gaze (located in the play space at the predicted display time, like the views)
        updateEyeGazes();
        return true;
    }

    XrVector3f OpenXRHMD::applyInitialScale(const XrVector3f& p) const
    {
        return XrVector3f{
            initial_adjustment.position.x + (p.x - initial_adjustment.position.x) * scale,
            initial_adjustment.position.y + (p.y - initial_adjustment.position.y) * scale,
            initial_adjustment.position.z + (p.z - initial_adjustment.position.z) * scale };
    }

    void OpenXRHMD::updateEyeGazes()
    {
        m_gaze[0] = GazeSample();
        m_gaze[1] = GazeSample();

        if (!m_gazeQueryAllowed)
            return;

        if (m_eyeTrackingBackend == EyeTrackingBackend::HTC_EYE_TRACKER)
        {
            if (m_htcEyeTracker == XR_NULL_HANDLE && !m_htcCreateAttempted)
                createHtcEyeTracker();
            if (m_htcEyeTracker == XR_NULL_HANDLE || !xrGetEyeGazeDataHTC_)
                return;
            try
            {
                XrEyeGazeDataInfoHTC gazeInfo{ XR_TYPE_EYE_GAZE_DATA_INFO_HTC };
                gazeInfo.next = NULL;
                gazeInfo.baseSpace = m_playSpace;
                gazeInfo.time = m_lastFrameState.predictedDisplayTime;
                XrEyeGazeDataHTC gazeData{ XR_TYPE_EYE_GAZE_DATA_HTC };
                gazeData.next = NULL;
                XR_TRACE("xrGetEyeGazeDataHTC ->");
                XrResult result = xrGetEyeGazeDataHTC_(m_htcEyeTracker, &gazeInfo, &gazeData);
                XR_TRACE("xrGetEyeGazeDataHTC <- " << result);
                if (XR_FAILED(result))
                    return;
                for (uint32_t i = 0; i < 2; i++)
                {
                    m_gaze[i].valid = (gazeData.gaze[i].isValid == XR_TRUE);
                    m_gaze[i].tracked = m_gaze[i].valid;
                    m_gaze[i].sampleTime = gazeData.time;
                    if (m_gaze[i].valid)
                    {
                        m_gaze[i].pose.orientation = gazeData.gaze[i].gazePose.orientation;
                        m_gaze[i].pose.position = applyInitialScale(gazeData.gaze[i].gazePose.position);
                    }
                }

                if (xrGetEyePupilDataHTC_)
                {
                    XrEyePupilDataInfoHTC pupilInfo{ XR_TYPE_EYE_PUPIL_DATA_INFO_HTC };
                    pupilInfo.next = NULL;
                    XrEyePupilDataHTC pupilData{ XR_TYPE_EYE_PUPIL_DATA_HTC };
                    pupilData.next = NULL;
                    if (XR_SUCCEEDED(xrGetEyePupilDataHTC_(m_htcEyeTracker, &pupilInfo, &pupilData)))
                    {
                        // The layer was observed to flag out-of-range values as valid: range-check everything
                        auto inRange = [](float v, float lo, float hi) { return std::isfinite(v) && v >= lo && v <= hi; };
                        for (uint32_t i = 0; i < 2; i++)
                        {
                            const XrSingleEyePupilDataHTC &p = pupilData.pupilData[i];
                            m_gaze[i].pupilDiameterValid = (p.isDiameterValid == XR_TRUE) && inRange(p.pupilDiameter, 0.5f, 12.f);
                            m_gaze[i].pupilDiameterMm = p.pupilDiameter;
                            m_gaze[i].pupilPositionValid = (p.isPositionValid == XR_TRUE) &&
                                                           inRange(p.pupilPosition.x, 0.f, 1.f) && inRange(p.pupilPosition.y, 0.f, 1.f);
                            m_gaze[i].pupilPosition = p.pupilPosition;
                        }
                    }
                }
                if (xrGetEyeGeometricDataHTC_)
                {
                    XrEyeGeometricDataInfoHTC geometricInfo{ XR_TYPE_EYE_GEOMETRIC_DATA_INFO_HTC };
                    geometricInfo.next = NULL;
                    XrEyeGeometricDataHTC geometricData{ XR_TYPE_EYE_GEOMETRIC_DATA_HTC };
                    geometricData.next = NULL;
                    if (XR_SUCCEEDED(xrGetEyeGeometricDataHTC_(m_htcEyeTracker, &geometricInfo, &geometricData)))
                    {
                        for (uint32_t i = 0; i < 2; i++)
                        {
                            const float openness = geometricData.eyeGeometricData[i].eyeOpenness;
                            // observed: isValid=1 with garbage values (e.g. 4e+30) -> keep only values in [0,1]
                            m_gaze[i].opennessValid = (geometricData.eyeGeometricData[i].isValid == XR_TRUE) &&
                                                      std::isfinite(openness) && openness >= 0.f && openness <= 1.f;
                            m_gaze[i].openness = openness;
                        }
                    }
                }
            }
            catch (...)
            {
                SIBR_WRG << "[EyeTracking] The VIVE eye tracking layer threw an exception: eye tracking disabled for this session" << std::endl;
                m_eyeTrackingFailed = true;
                m_gaze[0] = GazeSample();
                m_gaze[1] = GazeSample();
            }
            return;
        }

        if (m_eyeTrackingBackend == EyeTrackingBackend::FB_EYE_TRACKING_SOCIAL)
        {
            if (eyeTracker == XR_NULL_HANDLE || !xrGetEyeGazesFB_)
                return;
            eyeGazes.type = XR_TYPE_EYE_GAZES_FB;
            eyeGazes.next = nullptr;
            gazesInfo.type = XR_TYPE_EYE_GAZES_INFO_FB;
            gazesInfo.next = nullptr;
            gazesInfo.baseSpace = m_playSpace;
            gazesInfo.time = m_lastFrameState.predictedDisplayTime; // (was left to 0 upstream)
            XrResult result = xrGetEyeGazesFB_(eyeTracker, &gazesInfo, &eyeGazes);
            if (XR_FAILED(result))
            {
                eyeGazes.gaze[0].isValid = XR_FALSE;
                eyeGazes.gaze[1].isValid = XR_FALSE;
                return;
            }
            for (uint32_t i = 0; i < 2; i++)
            {
                // keep the public eyeGazes member consistent with the scaled positions (as upstream)
                eyeGazes.gaze[i].gazePose.position = applyInitialScale(eyeGazes.gaze[i].gazePose.position);
                m_gaze[i].valid = (eyeGazes.gaze[i].isValid == XR_TRUE);
                m_gaze[i].tracked = m_gaze[i].valid;
                m_gaze[i].pose = eyeGazes.gaze[i].gazePose;
                m_gaze[i].sampleTime = eyeGazes.time;
            }
        }
        else if (m_eyeTrackingBackend == EyeTrackingBackend::EXT_EYE_GAZE_INTERACTION)
        {
            if (!m_xrInput)
                return;
            if (!m_xrInput->eyeGazeEnabled())
            {
                m_eyeTrackingFailed = true; // gaze action could not be created or the layer threw
                return;
            }
            const OpenXRInput::EyeGazeState &state = m_xrInput->getEyeGaze();
            if (!state.isActive || !(state.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT))
                return;

            GazeSample sample;
            sample.valid = true;
            sample.tracked = (state.locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT) != 0;
            sample.pose.orientation = state.pose.orientation;
            if (state.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)
            {
                sample.pose.position = applyInitialScale(state.pose.position);
            }
            else if (m_viewCount >= 2)
            {
                // No gaze origin from the runtime: use the middle of both eyes (views are already scaled)
                sample.pose.position = XrVector3f{
                    0.5f * (views[0].pose.position.x + views[1].pose.position.x),
                    0.5f * (views[0].pose.position.y + views[1].pose.position.y),
                    0.5f * (views[0].pose.position.z + views[1].pose.position.z) };
            }
            sample.sampleTime = state.sampleTime;
            // XR_EXT_eye_gaze_interaction only provides the combined gaze: same sample for both views
            m_gaze[0] = sample;
            m_gaze[1] = sample;
        }
    }

    std::string OpenXRHMD::getEyeTrackingStatus(bool& ok) const
    {
        ok = m_gaze[0].valid || m_gaze[1].valid;
        if (m_eyeTrackingBackend != EyeTrackingBackend::NONE)
        {
            if (m_eyeTrackingFailed)
                return "eye tracking FAILED (exception in the runtime/API layer): see the console";
            if (!isFocused())
                return "waiting for the application to be focused in the headset (remove the SteamVR dashboard)";
            const XrViewStateFlags headValid = XR_VIEW_STATE_ORIENTATION_VALID_BIT | XR_VIEW_STATE_POSITION_VALID_BIT;
            if ((m_lastViewStateFlags & headValid) != headValid)
                return "headset NOT tracked (base stations on and visible?): gaze is not queried";
            if (!m_gazeQueryAllowed)
                return "headset tracked: starting eye tracking...";
        }
        switch (m_eyeTrackingBackend)
        {
        case EyeTrackingBackend::HTC_EYE_TRACKER:
            if (m_htcEyeTracker == XR_NULL_HANDLE)
                return "HTC eye tracker could not be created (SR_Runtime running? eye tracking enabled?)";
            if (!ok)
                return "no valid gaze (headset worn? eyes visible? calibrated?)";
            return "gaze valid";
        case EyeTrackingBackend::FB_EYE_TRACKING_SOCIAL:
            if (eyeTracker == XR_NULL_HANDLE)
                return "eye tracker could not be created (eye tracking permission?)";
            if (!ok)
                return "no valid gaze (headset worn? eye tracking calibrated?)";
            return "gaze valid";
        case EyeTrackingBackend::EXT_EYE_GAZE_INTERACTION:
        {
            if (!m_xrInput || !m_xrInput->eyeGazeEnabled())
                return "gaze action could not be created";
            const OpenXRInput::EyeGazeState &state = m_xrInput->getEyeGaze();
            if (!state.isActive)
                return "gaze action INACTIVE: the runtime delivers no gaze (app focused? eye tracking enabled in VIVE Console / SRanipal running?)";
            if (!ok)
                return "gaze action active but no valid sample (headset worn? eyes visible? calibrated?)";
            return m_gaze[0].tracked ? "gaze valid (tracked)" : "gaze valid (not tracked)";
        }
        default:
            return "no eye tracking extension available: no gaze will be recorded";
        }
    }

    const OpenXRHMD::GazeSample& OpenXRHMD::getGazeSample(Eye eye) const
    {
        return m_gaze[eye == Eye::LEFT ? 0 : 1];
    }

    const char* OpenXRHMD::getEyeTrackingBackendName() const
    {
        switch (m_eyeTrackingBackend)
        {
        case EyeTrackingBackend::FB_EYE_TRACKING_SOCIAL:
            return "FB_eye_tracking_social";
        case EyeTrackingBackend::EXT_EYE_GAZE_INTERACTION:
            return "EXT_eye_gaze_interaction";
        case EyeTrackingBackend::HTC_EYE_TRACKER:
            return "HTC_eye_tracker";
        default:
            return "none";
        }
    }

    bool OpenXRHMD::closeSession()
    {
        // Eye tracker handles are children of the session: destroy them first
        // (if the session was already destroyed by the runtime, its children were destroyed with it)
        if (eyeTracker != XR_NULL_HANDLE && xrDestroyEyeTrackerFB_ && m_session != XR_NULL_HANDLE)
        {
            xrDestroyEyeTrackerFB_(eyeTracker);
        }
        eyeTracker = XR_NULL_HANDLE;
        if (m_htcEyeTracker != XR_NULL_HANDLE && xrDestroyEyeTrackerHTC_ && m_session != XR_NULL_HANDLE)
        {
            try { xrDestroyEyeTrackerHTC_(m_htcEyeTracker); } catch (...) {}
        }
        m_htcEyeTracker = XR_NULL_HANDLE;
        m_xrInput.reset();

        // If session is already idle, just destroy it
        if (m_status == SessionStatus::IDLE)
        {
            XrResult result = xrDestroySession(m_session);
            if (!xrCheck(m_instance, result, "Failed to destroy session!"))
            {
                m_status = SessionStatus::FAILURE;
            }
            m_session = XR_NULL_HANDLE;
            m_status = SessionStatus::STOPPED;
        }
        // If not, proper close by requesting an exit
        else if (m_status != SessionStatus::STOPPED)
        {
            XrResult result = xrRequestExitSession(m_session);
            if (!xrCheck(m_instance, result, "Failed to request exit session!"))
            {
                m_status = SessionStatus::FAILURE;
                return false;
            }
            // Keep polling event until session is properly ended and destroyed
            while (isSessionRunning())
            {
                pollEvents();
            }
        }
        return m_status != SessionStatus::STOPPED;
    }

    bool OpenXRHMD::terminate()
    {
        if (isSessionRunning())
        {
            closeSession();
        }

        // Even if session failed to close, try to destroy the instance
        if (m_instance != XR_NULL_HANDLE)
        {
            XrResult result = xrDestroyInstance(m_instance);
            if (!xrCheck(m_instance, result, "Failed to destroy instance!"))
            {
                m_status = SessionStatus::FAILURE;
                return false;
            }
            m_instance = XR_NULL_HANDLE;
        }
        return true;
    }

    bool OpenXRHMD::shouldRender() const
    {
        return m_lastFrameState.shouldRender == XR_TRUE;
    }

    bool OpenXRHMD::submitFrame(RenderFunc renderFunc)
    {
        // Let's skip the rendering if we do not need to
        if (!shouldRender())
        {
            submitFrame();
            return true;
        }

        XrFrameBeginInfo frameBeginInfo;
        frameBeginInfo.type = XR_TYPE_FRAME_BEGIN_INFO;
        frameBeginInfo.next = NULL;

        XR_TRACE("xrBeginFrame ->");
        XrResult result = xrBeginFrame(m_session, &frameBeginInfo);
        XR_TRACE("xrBeginFrame <- " << result);
        if (!xrCheck(m_instance, result, "failed to begin frame!"))
            return false;

        // Render each eye and fill projectionViews with the result
        for (uint32_t i = 0; i < m_viewCount; i++)
        {
            XrSwapchainImageAcquireInfo acquireInfo;
            acquireInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
            acquireInfo.next = NULL;
            uint32_t acquired_index;
            result = xrAcquireSwapchainImage(m_swapchains[i], &acquireInfo, &acquired_index);
            if (!xrCheck(m_instance, result, "failed to acquire swapchain image!"))
                break;

            XrSwapchainImageWaitInfo waitInfo;
            waitInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
            waitInfo.next = NULL;
            waitInfo.timeout = 1000;
            result = xrWaitSwapchainImage(m_swapchains[i], &waitInfo);
            if (!xrCheck(m_instance, result, "failed to wait for swapchain image!"))
                break;

            // Update projection views
            m_projectionViews[i].pose = views[i].pose;
            m_projectionViews[i].fov = views[i].fov;

            // Let's call renderFunc to delegate the rendering to the calling function
            // renderFunc will render the scene into the framebuffer texture referenced in the view's SwapChainImage (one for each eye)
            renderFunc(i, m_swapchainsImages[i][acquired_index].image);

            XrSwapchainImageReleaseInfo releaseInfo;
            releaseInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
            releaseInfo.next = NULL;
            result = xrReleaseSwapchainImage(m_swapchains[i], &releaseInfo);
            if (!xrCheck(m_instance, result, "failed to release swapchain image!"))
                break;
        }

        XrCompositionLayerProjection projectionLayer;
        projectionLayer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
		projectionLayer.next = NULL;
		projectionLayer.layerFlags = 0;
		projectionLayer.space = m_playSpace;
		projectionLayer.viewCount = m_viewCount;
		projectionLayer.views = m_projectionViews;

        uint32_t submitted_layer_count = 1;
        const XrCompositionLayerBaseHeader *submitted_layers[1] = {
            (const XrCompositionLayerBaseHeader *const)&projectionLayer };

        // Check if the frame meets the deadline (aka predictedDisplayTime).
        updateRefreshReport();

		XrFrameEndInfo frameEndInfo;
		frameEndInfo.type = XR_TYPE_FRAME_END_INFO;
		frameEndInfo.next = NULL;
		frameEndInfo.displayTime = m_lastFrameState.predictedDisplayTime;
		frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
		frameEndInfo.layerCount = submitted_layer_count;
		frameEndInfo.layers = submitted_layers;

        XR_TRACE("xrEndFrame ->");
        result = xrEndFrame(m_session, &frameEndInfo);
        XR_TRACE("xrEndFrame <- " << result);
        if (!xrCheck(m_instance, result, "failed to end frame!"))
            return false;

        return true;
    }

    bool OpenXRHMD::submitFrame()
    {
        XrFrameBeginInfo frameBeginInfo;
        frameBeginInfo.type = XR_TYPE_FRAME_BEGIN_INFO;
        frameBeginInfo.next = NULL;

        XR_TRACE("xrBeginFrame ->");
        XrResult result = xrBeginFrame(m_session, &frameBeginInfo);
        XR_TRACE("xrBeginFrame <- " << result);
        if (!xrCheck(m_instance, result, "failed to begin frame!"))
            return false;

		XrFrameEndInfo frameEndInfo;
		frameEndInfo.type = XR_TYPE_FRAME_END_INFO;
		frameEndInfo.next = NULL;
		frameEndInfo.displayTime = m_lastFrameState.predictedDisplayTime;
		frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
		frameEndInfo.layerCount = 0;
		frameEndInfo.layers = NULL;
        XR_TRACE("xrEndFrame ->");
        result = xrEndFrame(m_session, &frameEndInfo);
        XR_TRACE("xrEndFrame <- " << result);
        if (!xrCheck(m_instance, result, "failed to end frame!"))
            return false;

        return true;
    }

    void OpenXRHMD::updateRefreshReport()
    {
        XrTime now_ns = 0;
#if defined(XR_USE_PLATFORM_WIN32)
        LARGE_INTEGER ticks;
        if (QueryPerformanceCounter(&ticks))
        {
            pfnConvertWin32PerformanceCounterToTimeKHR(m_instance, &ticks, &now_ns); // See https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrConvertTimeToWin32PerformanceCounterKHR.html
        }
        else
        {
            SIBR_WRG << "Failed to get performance counter" << std::endl;
        }
#elif defined(XR_USE_TIMESPEC)
        struct timespec timespecTime;
        clock_gettime(CLOCK_MONOTONIC, &timespecTime);
        pfnConvertTimespecTimeToTimeKHR(m_instance, &timespecTime, &now_ns); // See https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_KHR_convert_timespec_time
#endif

        // Update current report
        if (m_lastFrameState.predictedDisplayTime - now_ns < 0)
        {
            m_currentFrameRefreshReport.nbMissedFrames++;
        }
        m_currentFrameRefreshReport.totalRenderedFrames++;
        m_currentFrameRefreshReport.expectedFramerate = 1000000000.f / m_lastFrameState.predictedDisplayPeriod;

        // Validate the report after 100 frames
        if (m_currentFrameRefreshReport.totalRenderedFrames == 100)
        {

            m_currentFrameRefreshReport.measuredFramerate = 1000.f * m_currentFrameRefreshReport.totalRenderedFrames /
                                                            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_currentFrameRefreshReport.firstFrameTimestamp).count();
            m_lastFrameRefreshReport = m_currentFrameRefreshReport;
            m_currentFrameRefreshReport = {0, 0, 0.f, 0.f, std::chrono::high_resolution_clock::now()};
        }
    }

    uint32_t OpenXRHMD::eyeToViewIndex(Eye eye)
    {
        return static_cast<uint32_t>(eye);
    }

    const char *OpenXRHMD::eyeToString(Eye eye)
    {
        switch (eye)
        {
        case Eye::LEFT:
            return "LEFT";
        case Eye::RIGHT:
            return "RIGHT";
        }
        return "UNKNOWN";
    }

    const char *OpenXRHMD::sessionStateToString(const XrSessionState &state)
    {
        switch (state)
        {
        case XR_SESSION_STATE_MAX_ENUM:
            return "MAX_ENUM";
        case XR_SESSION_STATE_IDLE:
            return "IDLE";
        case XR_SESSION_STATE_UNKNOWN:
            return "UNKNOWN";
        case XR_SESSION_STATE_FOCUSED:
            return "FOCUSED";
        case XR_SESSION_STATE_SYNCHRONIZED:
            return "SYNCHRONIZED";
        case XR_SESSION_STATE_VISIBLE:
            return "VISIBLE";
        case XR_SESSION_STATE_READY:
            return "READY";
        case XR_SESSION_STATE_STOPPING:
            return "STOPPING";
        case XR_SESSION_STATE_LOSS_PENDING:
            return "PENDING";
        case XR_SESSION_STATE_EXITING:
            return "EXITING";
        }
        return "UNKNOWN";
    }

    const char *OpenXRHMD::getReferenceSpaceType() const
    {
        switch (m_playSpaceType)
        {
        case XR_REFERENCE_SPACE_TYPE_VIEW:
            return "VIEW";
        case XR_REFERENCE_SPACE_TYPE_LOCAL:
            return "LOCAL";
        case XR_REFERENCE_SPACE_TYPE_STAGE:
            return "STAGE";
        case XR_REFERENCE_SPACE_TYPE_UNBOUNDED_MSFT:
            return "UNBOUNDED_MSFT";
        case XR_REFERENCE_SPACE_TYPE_COMBINED_EYE_VARJO:
            return "COMBINED_EYE_VARJO";
        default:
            return "UNKNOWN";
        }
    }

    bool OpenXRHMD::startEyeTracker()
    {
        // XR_FB_eye_tracking_social and XR_HTC_eye_tracker need an explicit eye tracker handle.
        // XR_EXT_eye_gaze_interaction goes through the action system (see OpenXRInput).
        if (m_eyeTrackingBackend == EyeTrackingBackend::HTC_EYE_TRACKER)
        {
            // Created lazily in updateEyeGazes(), once the app is focused and the headset tracked:
            // the VIVE layer abort()s the whole process if SRanipal cannot start its eye framework
            // (e.g. headset in standby / not tracked), which cannot be caught.
            SIBR_LOG << "[EyeTracking] The HTC eye tracker will be created once the headset is tracked and the application focused" << std::endl;
            return true;
        }
        if (m_eyeTrackingBackend == EyeTrackingBackend::FB_EYE_TRACKING_SOCIAL && m_session != XR_NULL_HANDLE) {
            XrEyeTrackerCreateInfoFB createInfo{ XR_TYPE_EYE_TRACKER_CREATE_INFO_FB };
            createInfo.next = NULL;
            XrResult result = xrCreateEyeTrackerFB_(m_session, &createInfo, &eyeTracker);
            if (!xrCheck(m_instance, result, "Failed to create eyeTracker\n"))
                return false;
        }
        return true;
    }

    bool OpenXRHMD::createHtcEyeTracker()
    {
        m_htcCreateAttempted = true;
        if (m_session == XR_NULL_HANDLE || !xrCreateEyeTrackerHTC_)
            return false;
        SIBR_LOG << "[EyeTracking] Creating the HTC eye tracker (SRanipal must be running: SR_Runtime tray icon)..." << std::endl;
        XrEyeTrackerCreateInfoHTC createInfo{ XR_TYPE_EYE_TRACKER_CREATE_INFO_HTC };
        createInfo.next = NULL;
        XrResult result = XR_ERROR_RUNTIME_FAILURE;
        try
        {
            result = xrCreateEyeTrackerHTC_(m_session, &createInfo, &m_htcEyeTracker);
        }
        catch (...)
        {
            SIBR_WRG << "[EyeTracking] xrCreateEyeTrackerHTC threw an exception" << std::endl;
            result = XR_ERROR_RUNTIME_FAILURE;
        }
        if (!xrCheck(m_instance, result, "Failed to create the HTC eye tracker"))
        {
            m_htcEyeTracker = XR_NULL_HANDLE;
            m_eyeTrackingFailed = true;
            return false;
        }
        SIBR_LOG << "[EyeTracking] HTC eye tracker created" << std::endl;
        return true;
    }


    Eigen::Vector2f OpenXRHMD::getPlaySpaceBounds() const {
        return Eigen::Vector2f { m_playBounds.width, m_playBounds.height };
    }


} /*namespace sibr*/
