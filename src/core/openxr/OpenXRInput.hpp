// Software Name : SIBR_core
// SPDX-FileCopyrightText: Copyright (c) 2023 Orange
// SPDX-License-Identifier: Apache 2.0
//
// This software is distributed under the Apache 2.0 License;
// see the LICENSE file for more details.
//
// Author: Cédric CHEDALEUX <cedric.chedaleux@orange.com> et al.

#pragma once

#include <functional>
#include <memory>
#include <openxr/openxr.h>

#include <core/system/Quaternion.hpp>
#include <core/openxr/interactionprofiles/IInteractionProfile.hpp>

namespace sibr
{

    /**
     *	Class that handles all input controllers events
     *   \ingroup sibr_openxr
     */
    class OpenXRInput
    {
    public:

        enum class Hand {
            LEFT,
            RIGHT
        };

        /**
         * State of the XR_EXT_eye_gaze_interaction gaze pose for the last synced frame.
         * The pose is expressed in the reference space given to the constructor.
         */
        struct EyeGazeState {
            bool isActive = false;                 ///< the gaze action is bound and active (runtime delivers gaze)
            XrSpaceLocationFlags locationFlags = 0; ///< raw XR_SPACE_LOCATION_* flags of the last xrLocateSpace
            XrPosef pose{ {0.f, 0.f, 0.f, 1.f}, {0.f, 0.f, 0.f} };
            XrTime sampleTime = 0;                 ///< time of the eye tracker sample (XrEyeGazeSampleTimeEXT), 0 if unknown
        };

        /**
         * @param enableEyeGaze: also create the XR_EXT_eye_gaze_interaction action and bindings.
         *        Only set it if the XR_EXT_eye_gaze_interaction instance extension was enabled.
         */
        OpenXRInput(XrInstance& instance, XrSession& session, XrSpace& refSpace, bool enableEyeGaze = false);
        ~OpenXRInput();

        /** @return true if the eye gaze action has been created (XR_EXT_eye_gaze_interaction) */
        bool eyeGazeEnabled() const { return m_eyeGazeEnabled; }

        /** @return the eye gaze state located at the time given to the last sync() call */
        const EyeGazeState& getEyeGaze() const { return m_eyeGaze; }

        /** Allow/forbid querying the eye gaze during the next sync() (e.g. not while the head is untracked) */
        void setEyeGazeQueryAllowed(bool allowed) { m_eyeGazeQueryAllowed = allowed; }

        /**
         * @brief Update all actions to receive input events and dispatch callbacks
         * @param: timestamp to locate action pose
         */
        bool sync(XrTime time);

        /**
         * @brief Stick/trackpad move events
         * @param hand: controller's hand
         * @param callback: called when the stick from the controller has moved
         */
        void setStickMoveCallback(Hand hand, const std::function<void(float, float)> &callback);

        /**
         * @brief Stick click events
         * @param hand: controller's hand
         * @param callback: called when the stick from the controller has been clicked
         */
        void setStickClickCallback(Hand hand, const std::function<void()> &callback);

        /**
         * @brief Stick click events
         * @param hand: controller's hand
         * @param callback: called when the stick from the controller's trigger has been pressed
         */
        void setTriggerCallback(Hand hand, const std::function<void(float)> &callback);

        /**
         * @brief Return the position of the requested hand
         * @param hand: controller's hand
         */
        Vector3f getHandPosePosition(Hand hand) const;

        /**
         * @brief Return the quaternion of the requested hand
         * @param hand: controller's hand
         */
        Quaternionf getHandPoseOrientation(Hand hand) const;

    private:
        XrInstance& m_instance;
        XrSession& m_session;
        XrSpace& m_refSpace;
        XrActionSet m_actionSet;
        XrPosef m_handPose[2];
        bool m_eyeGazeEnabled = false;
        bool m_eyeGazeQueryAllowed = false;
        EyeGazeState m_eyeGaze;
        bool m_chainGazeSampleTime = true; ///< chain XrEyeGazeSampleTimeEXT to xrLocateSpace (env EYENAVGS_GAZE_SAMPLE_TIME=0 disables)
        // diagnostics
        bool m_lastLoggedGazeActive = false;
        XrSpaceLocationFlags m_lastLoggedGazeFlags = 0;
        int m_loggedGazeFlagChanges = 0;
        void logEyeGazeChanges();
        std::map<std::string, XrPath> m_xrPaths = {};
        std::map<OpenXRInputType, XrSpace> m_xrSpaces = {};
        std::map<OpenXRInputType, XrAction> m_xrActions = {};

        // Callbacks for notifying XR input events
        std::function<void(float, float)> m_stickMoveCallback[2];
        std::function<void()> m_stickClickCallback[2];
        std::function<void(float)> m_triggerCallback[2];

        // Internal methods
        bool createActions();
        bool createBindings();
        void registerInteractionProfile(const IInteractionProfile& profile);
        XrPath getXrPath(const std::string& path);
        XrAction getXrAction(OpenXRInputType inputType) const;
        bool createAction(OpenXRInputType inputType, XrActionType actionType, const std::string& name);
        bool attachActionSet();
        bool locateSpaces(XrTime time);
        void locateEyeGaze(XrTime time);
        void locateEyeGazeUnchecked(XrTime time);
        bool extractFloatValue(XrAction action, float& value) const;
        bool extractBooleanValue(XrAction action, bool& value) const;
        bool extractPoseValue(XrAction action, XrSpaceLocation location, Vector3f& value, Quaternionf& orientation) const;
        void handleTrackpadEvent(Hand hand);
        void handleThumbstickEvent(Hand hand);
        void handleTriggerEvent(Hand hand);
    };

} /*namespace sibr*/