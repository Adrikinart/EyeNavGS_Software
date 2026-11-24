// Software Name : SIBR_core
// SPDX-FileCopyrightText: Copyright (c) 2023 Orange
// SPDX-License-Identifier: Apache 2.0
//
// This software is distributed under the Apache 2.0 License;
// see the LICENSE file for more details.
//
// Author: Cédric CHEDALEUX <cedric.chedaleux@orange.com> et al.

#include <core/openxr/OpenXRHelper.hpp>
#include <core/openxr/OpenXRInput.hpp>
#include <core/openxr/interactionprofiles/KhronosSimpleControllerProfile.hpp>
#include <core/openxr/interactionprofiles/HTCViveControllerProfile.hpp>
#include <core/openxr/interactionprofiles/OculusTouchControllerProfile.hpp>
#include <core/openxr/interactionprofiles/ValveIndexControllerProfile.hpp>
#include <core/openxr/interactionprofiles/EyeGazeInteractionProfile.hpp>

#include <map>
#include <exception>

namespace sibr
{
    OpenXRInput::OpenXRInput(XrInstance &instance, XrSession &session, XrSpace& refSpace, bool enableEyeGaze) : m_instance(instance),
                                                                         m_session(session),
                                                                         m_refSpace(refSpace),
                                                                         m_eyeGazeEnabled(enableEyeGaze)
    {
        if (!createActions())
        {
            SIBR_ERR << "Failed to create OpenXR actions" << std::endl;
        }

        registerInteractionProfile(KhronosSimpleControllerProfile());
        registerInteractionProfile(HTCViveControllerProfile());
        registerInteractionProfile(OculusTouchControllerProfile());
        registerInteractionProfile(ValveIndexControllerProfile());
        if (m_eyeGazeEnabled)
        {
            {
#ifdef _WIN32
                char *value = nullptr;
                size_t length = 0;
                if (_dupenv_s(&value, &length, "EYENAVGS_GAZE_SAMPLE_TIME") == 0 && value != nullptr)
                {
                    m_chainGazeSampleTime = (value[0] != '0');
                    free(value);
                }
#else
                const char *value = std::getenv("EYENAVGS_GAZE_SAMPLE_TIME");
                if (value != nullptr)
                    m_chainGazeSampleTime = (value[0] != '0');
#endif
                SIBR_LOG << "[EyeTracking] XrEyeGazeSampleTimeEXT chaining: " << (m_chainGazeSampleTime ? "on" : "off") << std::endl;
            }
            // XR_EXT_eye_gaze_interaction: its interaction profile must be suggested in its own
            // xrSuggestInteractionProfileBindings() call, before the action set is attached (below).
            registerInteractionProfile(EyeGazeInteractionProfile());
        }

        if (!attachActionSet())
        {
            SIBR_ERR << "Failed to attach OpenXR action set" << std::endl;
        }
    }

    OpenXRInput::~OpenXRInput()
    {
        XrResult result;
        // Action spaces are children of the session (m_session is a reference to the HMD's session
        // handle, reset to XR_NULL_HANDLE when the runtime already destroyed the session)
        if (m_session != XR_NULL_HANDLE)
        {
            for (auto space : m_xrSpaces)
            {
                result = xrDestroySpace(space.second);
                xrCheck(m_instance, result, "Failed to destroy action space!");
            }
        }
        m_xrSpaces.clear();

        for (auto action : m_xrActions)
        {
            result = xrDestroyAction(action.second);
            xrCheck(m_instance, result, "Failed to destroy action!");
        }
        m_xrActions.clear();

        result = xrDestroyActionSet(m_actionSet);
        xrCheck(m_instance, result, "Failed to destroy action set!");
    }

    bool OpenXRInput::sync(XrTime time)
    {
        XrActiveActionSet activeActionSet;
        activeActionSet.actionSet = m_actionSet;
        activeActionSet.subactionPath = XR_NULL_PATH;

        XrActionsSyncInfo actionsSyncInfo;
        actionsSyncInfo.type = XR_TYPE_ACTIONS_SYNC_INFO;
        actionsSyncInfo.next = NULL;
        actionsSyncInfo.countActiveActionSets = 1;
        actionsSyncInfo.activeActionSets = &activeActionSet;
        XR_TRACE("xrSyncActions ->");
        XrResult result = xrSyncActions(m_session, &actionsSyncInfo);
        XR_TRACE("xrSyncActions <- " << result);
        if (!xrCheck(m_instance, result, "failed to sync actions!"))
        {
            return false;
        }

        // Extract all input events and dispatch callbacks
        XR_TRACE("controller events ->");
        handleTrackpadEvent(Hand::LEFT);
        handleTrackpadEvent(Hand::RIGHT);
        handleThumbstickEvent(Hand::LEFT);
        handleThumbstickEvent(Hand::RIGHT);
        handleTriggerEvent(Hand::LEFT);
        handleTriggerEvent(Hand::RIGHT);
        XR_TRACE("controller events <-");

        locateSpaces(time);
        XR_TRACE("locateSpaces <-");

        return true;
    }

    void OpenXRInput::setStickMoveCallback(Hand hand, const std::function<void(float, float)> &callback)
    {
        m_stickMoveCallback[static_cast<int>(hand)] = callback;
    }

    void OpenXRInput::setStickClickCallback(Hand hand, const std::function<void()> &callback)
    {
        m_stickClickCallback[static_cast<int>(hand)] = callback;
    }

    void OpenXRInput::setTriggerCallback(Hand hand, const std::function<void(float value)> &callback)
    {
        m_triggerCallback[static_cast<int>(hand)] = callback;
    }


    Vector3f OpenXRInput::getHandPosePosition(Hand hand) const {
        auto position = m_handPose[static_cast<int>(hand)].position;
        return Vector3f {position.x, position.y, position.z};
    }

    Quaternionf OpenXRInput::getHandPoseOrientation(Hand hand) const {
        auto orientation = m_handPose[static_cast<int>(hand)].orientation;
        return Quaternionf {orientation.w, orientation.x, orientation.y, orientation.z};
    }

    bool OpenXRInput::createActions()
    {
        XrResult result;

        // Create action set
        XrActionSetCreateInfo actionSetInfo;
        actionSetInfo.type = XR_TYPE_ACTION_SET_CREATE_INFO;
        actionSetInfo.next = NULL;
        actionSetInfo.priority = 0;
        stringCopy(actionSetInfo.actionSetName, "gameplay_default", XR_MAX_ACTION_SET_NAME_SIZE);
        stringCopy(actionSetInfo.localizedActionSetName, "Gameplay default", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE);

        result = xrCreateActionSet(m_instance, &actionSetInfo, &m_actionSet);
        if (!xrCheck(m_instance, result, "xrCreateActionSet"))
        {
            return false;
        }

        createAction(OpenXRInputType::LEFT_TRACKPAD_X, XR_ACTION_TYPE_FLOAT_INPUT, "left_trackpad_x");
        createAction(OpenXRInputType::LEFT_TRACKPAD_Y, XR_ACTION_TYPE_FLOAT_INPUT, "left_trackpad_y");
        createAction(OpenXRInputType::LEFT_TRACKPAD_CLICK, XR_ACTION_TYPE_BOOLEAN_INPUT, "left_trackpad_click");
        createAction(OpenXRInputType::RIGHT_TRACKPAD_X, XR_ACTION_TYPE_FLOAT_INPUT, "right_trackpad_x");
        createAction(OpenXRInputType::RIGHT_TRACKPAD_Y, XR_ACTION_TYPE_FLOAT_INPUT, "right_trackpad_y");
        createAction(OpenXRInputType::RIGHT_TRACKPAD_CLICK, XR_ACTION_TYPE_BOOLEAN_INPUT, "right_trackpad_click");
        createAction(OpenXRInputType::LEFT_THUMBSTICK_X, XR_ACTION_TYPE_FLOAT_INPUT, "left_thumbstick_x");
        createAction(OpenXRInputType::LEFT_THUMBSTICK_Y, XR_ACTION_TYPE_FLOAT_INPUT, "left_thumbstick_y");
        createAction(OpenXRInputType::LEFT_THUMBSTICK_CLICK, XR_ACTION_TYPE_BOOLEAN_INPUT, "left_thumbstick_click");
        createAction(OpenXRInputType::RIGHT_THUMBSTICK_X, XR_ACTION_TYPE_FLOAT_INPUT, "right_thumbstick_x");
        createAction(OpenXRInputType::RIGHT_THUMBSTICK_Y, XR_ACTION_TYPE_FLOAT_INPUT, "right_thumbstick_y");
        createAction(OpenXRInputType::RIGHT_THUMBSTICK_CLICK, XR_ACTION_TYPE_BOOLEAN_INPUT, "right_thumbstick_click");
        createAction(OpenXRInputType::LEFT_TRIGGER, XR_ACTION_TYPE_FLOAT_INPUT, "left_trigger");
        createAction(OpenXRInputType::RIGHT_TRIGGER, XR_ACTION_TYPE_FLOAT_INPUT, "right_trigger");
        createAction(OpenXRInputType::LEFT_AIM_POSE, XR_ACTION_TYPE_POSE_INPUT, "left_aim_pose");
        createAction(OpenXRInputType::RIGHT_AIM_POSE, XR_ACTION_TYPE_POSE_INPUT, "right_aim_pose");
        if (m_eyeGazeEnabled)
        {
            // Creates the action and its action space (see createAction())
            if (!createAction(OpenXRInputType::EYE_GAZE_POSE, XR_ACTION_TYPE_POSE_INPUT, "eye_gaze_pose"))
            {
                SIBR_WRG << "Failed to create the eye gaze action: eye gaze will not be recorded" << std::endl;
                m_eyeGazeEnabled = false;
            }
        }

        return true;
    }

    void OpenXRInput::registerInteractionProfile(const IInteractionProfile &profile)
    {
        XrPath interactionProfilePath;
        XrResult result = xrStringToPath(m_instance, profile.getName().c_str(), &interactionProfilePath);
        if (!xrCheck(m_instance, result, "failed to get interaction profile"))
        {
            return;
        }

        std::vector<XrActionSuggestedBinding> xrActionBindings;
        for (const auto &binding : profile.getBindings())
        {
            XrAction xrAction = getXrAction(binding.first);
            if (!xrAction)
            {
                SIBR_LOG << "Cannot register binding '" << binding.second << "' for profile '" << profile.getName() << "': invalid XRAction" << std::endl;
            }
            else
            {
                XrActionSuggestedBinding actionSuggestedBinding;
                actionSuggestedBinding.action = xrAction;
                actionSuggestedBinding.binding = getXrPath(binding.second);
                xrActionBindings.push_back(actionSuggestedBinding);
            }
        }

        if (xrActionBindings.empty())
        {
            SIBR_WRG << "Invalid action bindings for profile '" << profile.getName() << "': no binding defined!" << std::endl;
            return;
        }

        XrInteractionProfileSuggestedBinding profileSuggestedBindings;
        profileSuggestedBindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
        profileSuggestedBindings.next = NULL;
        profileSuggestedBindings.interactionProfile = interactionProfilePath;
        profileSuggestedBindings.countSuggestedBindings = (uint32_t)xrActionBindings.size();
        profileSuggestedBindings.suggestedBindings = xrActionBindings.data();

        result = xrSuggestInteractionProfileBindings(m_instance, &profileSuggestedBindings);
        if (!xrCheck(m_instance, result, "failed to suggest bindings"))
        {
            return;
        }
    }

    bool OpenXRInput::attachActionSet()
    {
        XrSessionActionSetsAttachInfo actionSetAttachInfo;
        actionSetAttachInfo.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
        actionSetAttachInfo.next = NULL;
        actionSetAttachInfo.countActionSets = 1;
        actionSetAttachInfo.actionSets = &m_actionSet;
        XrResult result = xrAttachSessionActionSets(m_session, &actionSetAttachInfo);
        return xrCheck(m_instance, result, "failed to attach action set");
    }

    void OpenXRInput::locateEyeGaze(XrTime time)
    {
        m_eyeGaze.isActive = false;
        m_eyeGaze.locationFlags = 0;
        m_eyeGaze.sampleTime = 0;

        if (!m_eyeGazeEnabled || !m_eyeGazeQueryAllowed)
        {
            return;
        }

        // Some API layers implementing XR_EXT_eye_gaze_interaction throw C++ exceptions through the
        // OpenXR C API (seen with the VIVE OpenXR eye tracking layer when its SRanipal backend has no data).
        // Do not let that kill the recording: report it, disable eye gaze and keep recording head poses.
        try
        {
            locateEyeGazeUnchecked(time);
        }
        catch (const std::exception &e)
        {
            SIBR_WRG << "[EyeTracking] The OpenXR runtime/API layer threw an exception while reading the eye gaze: '"
                     << e.what() << "'. Eye gaze is disabled for this session (GazeValid=0)." << std::endl;
            m_eyeGazeEnabled = false;
            m_eyeGaze.isActive = false;
            m_eyeGaze.locationFlags = 0;
        }
        catch (...)
        {
            SIBR_WRG << "[EyeTracking] The OpenXR runtime/API layer threw an unknown exception while reading the eye gaze. "
                     << "Eye gaze is disabled for this session (GazeValid=0)." << std::endl;
            m_eyeGazeEnabled = false;
            m_eyeGaze.isActive = false;
            m_eyeGaze.locationFlags = 0;
        }
    }

    void OpenXRInput::locateEyeGazeUnchecked(XrTime time)
    {
        auto spaceIt = m_xrSpaces.find(OpenXRInputType::EYE_GAZE_POSE);
        XrAction action = getXrAction(OpenXRInputType::EYE_GAZE_POSE);
        if (spaceIt == m_xrSpaces.end() || !action)
        {
            return;
        }

        XrActionStatePose actionState{ XR_TYPE_ACTION_STATE_POSE };
        actionState.next = NULL;
        XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
        getInfo.next = NULL;
        getInfo.action = action;
        getInfo.subactionPath = XR_NULL_PATH;
        XR_TRACE("xrGetActionStatePose(eye gaze) ->");
        XrResult result = xrGetActionStatePose(m_session, &getInfo, &actionState);
        XR_TRACE("xrGetActionStatePose(eye gaze) <- " << result << " isActive=" << actionState.isActive);
        if (XR_FAILED(result))
        {
            return;
        }
        m_eyeGaze.isActive = (actionState.isActive == XR_TRUE);
        if (!m_eyeGaze.isActive)
        {
            // Not bound (yet) or eye tracker not available: nothing to locate
            return;
        }

        // XrEyeGazeSampleTimeEXT can be chained to get the time of the eye tracker sample
        XrEyeGazeSampleTimeEXT eyeGazeSampleTime{ XR_TYPE_EYE_GAZE_SAMPLE_TIME_EXT };
        eyeGazeSampleTime.next = NULL;
        eyeGazeSampleTime.time = 0;
        XrSpaceLocation location{ XR_TYPE_SPACE_LOCATION };
        location.next = m_chainGazeSampleTime ? &eyeGazeSampleTime : NULL;
        XR_TRACE("xrLocateSpace(eye gaze) ->");
        result = xrLocateSpace(spaceIt->second, m_refSpace, time, &location);
        XR_TRACE("xrLocateSpace(eye gaze) <- " << result << " flags=" << location.locationFlags);
        if (XR_FAILED(result))
        {
            // Be tolerant with runtimes/layers that would not accept the chained structure
            location.next = NULL;
            location.locationFlags = 0;
            eyeGazeSampleTime.time = 0;
            result = xrLocateSpace(spaceIt->second, m_refSpace, time, &location);
            if (XR_FAILED(result))
            {
                return;
            }
        }

        m_eyeGaze.locationFlags = location.locationFlags;
        m_eyeGaze.sampleTime = eyeGazeSampleTime.time;
        if (location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)
        {
            m_eyeGaze.pose = location.pose;
        }
    }

    void OpenXRInput::logEyeGazeChanges()
    {
        // Diagnostics: log transitions of the gaze action state / location flags (rate limited)
        static const int kMaxLoggedChanges = 30;
        if (m_eyeGaze.isActive != m_lastLoggedGazeActive)
        {
            SIBR_LOG << "[EyeTracking] gaze action isActive: " << (m_lastLoggedGazeActive ? "true" : "false")
                     << " -> " << (m_eyeGaze.isActive ? "true" : "false") << std::endl;
            m_lastLoggedGazeActive = m_eyeGaze.isActive;
        }
        const XrSpaceLocationFlags relevantFlags = m_eyeGaze.locationFlags &
            (XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT |
             XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT | XR_SPACE_LOCATION_POSITION_TRACKED_BIT);
        if (relevantFlags != m_lastLoggedGazeFlags)
        {
            if (m_loggedGazeFlagChanges < kMaxLoggedChanges)
            {
                auto describe = [](XrSpaceLocationFlags f) {
                    std::string s;
                    s += (f & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) ? "O" : "-";
                    s += (f & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT) ? "o" : "-";
                    s += (f & XR_SPACE_LOCATION_POSITION_VALID_BIT) ? "P" : "-";
                    s += (f & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) ? "p" : "-";
                    return s;
                };
                SIBR_LOG << "[EyeTracking] gaze location flags (O/o: orientation valid/tracked, P/p: position valid/tracked): "
                         << describe(m_lastLoggedGazeFlags) << " -> " << describe(relevantFlags)
                         << (m_loggedGazeFlagChanges + 1 == kMaxLoggedChanges ? "  (further changes are not logged)" : "") << std::endl;
            }
            ++m_loggedGazeFlagChanges;
            m_lastLoggedGazeFlags = relevantFlags;
        }
    }

    bool OpenXRInput::locateSpaces(XrTime time) {
        locateEyeGaze(time);
        if (m_eyeGazeEnabled)
        {
            logEyeGazeChanges();
        }

        for (const auto& space: m_xrSpaces)
        {
            if (space.first == OpenXRInputType::EYE_GAZE_POSE) {
                continue; // handled by locateEyeGaze()
            }
            XrAction action = getXrAction(space.first);
            if (!action) {
                continue;
            }

            XrActionStatePose xrValue;
            xrValue.type = XR_TYPE_ACTION_STATE_POSE;
            xrValue.next = NULL;
            XrActionStateGetInfo getInfo = {};
            getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
            getInfo.next = NULL;
            getInfo.action = action;
            getInfo.subactionPath = 0;

            XrResult result = xrGetActionStatePose(m_session, &getInfo, &xrValue);
            if (!xrCheck(m_instance, result, "failed to get track pose value!")) {
                continue;
            }

            if (xrValue.isActive)
            {
                XrSpaceVelocity velocity;
                velocity.type = XR_TYPE_SPACE_VELOCITY;
                velocity.next = NULL;
                XrSpaceLocation location;
                location.type = XR_TYPE_SPACE_LOCATION;
                location.next = &velocity;
                XrResult result = xrLocateSpace(space.second, m_refSpace, time, &location);
                if (!xrCheck(m_instance, result, "failed to locate action space"))
                {
                    continue;
                }

                if (location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT &&
                    location.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT &&
                    location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT &&
                    location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT)
                {
                    if (space.first == OpenXRInputType::LEFT_AIM_POSE) {
                        m_handPose[static_cast<int>(Hand::LEFT)] = location.pose;
                    } else if (space.first == OpenXRInputType::RIGHT_AIM_POSE) {
                        m_handPose[static_cast<int>(Hand::RIGHT)] = location.pose;
                    }
                    // else: other pose types are not supported
                }
            }
        }
        return true;
    }

    XrPath OpenXRInput::getXrPath(const std::string &path)
    {
        auto it = m_xrPaths.find(path);
        if (it != m_xrPaths.cend())
        {
            return (*it).second;
        }

        XrPath xrPath = XR_NULL_PATH;
        xrStringToPath(m_instance, path.c_str(), &xrPath);
        m_xrPaths.insert({path, xrPath});
        return xrPath;
    }

    XrAction OpenXRInput::getXrAction(OpenXRInputType inputType) const
    {
        auto it = m_xrActions.find(inputType);
        if (it != m_xrActions.cend())
        {
            return (*it).second;
        }
        return XR_NULL_HANDLE;
    }

    bool OpenXRInput::createAction(OpenXRInputType inputType, XrActionType xrActionType, const std::string &name)
    {
        XrAction action;
        XrActionCreateInfo actionInfo;
        actionInfo.type = XR_TYPE_ACTION_CREATE_INFO;
        actionInfo.next = NULL;
        actionInfo.actionType = xrActionType;
        actionInfo.countSubactionPaths = 0;
        actionInfo.subactionPaths = NULL;
        stringCopy(actionInfo.actionName, name.c_str(), XR_MAX_ACTION_NAME_SIZE);
        stringCopy(actionInfo.localizedActionName, name.c_str(), XR_MAX_LOCALIZED_ACTION_NAME_SIZE);

        XrResult result = xrCreateAction(m_actionSet, &actionInfo, &action);
        if (!xrCheck(m_instance, result, "failed to create action for path '%s'", name))
        {
            return false;
        }

        if (xrActionType == XR_ACTION_TYPE_POSE_INPUT)
        {
            XrSpace space = XR_NULL_HANDLE;
            XrActionSpaceCreateInfo aspci = {};
            aspci.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
            aspci.next = NULL;
            aspci.action = action;
            XrPosef identity;
            identity.orientation = {0.0f, 0.0f, 0.0f, 1.0f};
            identity.position = {0.0f, 0.0f, 0.0f};
            aspci.poseInActionSpace = identity;
            aspci.subactionPath = 0;
            result = xrCreateActionSpace(m_session, &aspci, &space);
            if (!xrCheck(m_instance, result, "xrCreateActionSpace"))
            {
                return false;
            }

            m_xrSpaces.insert({inputType, space});
        }

        m_xrActions.insert({inputType, action});

        return true;
    }

    bool OpenXRInput::extractFloatValue(XrAction xrAction, float &val) const
    {
        if (!xrAction)
        {
            return false;
        }
        XrActionStateFloat xrValue;
        xrValue.type = XR_TYPE_ACTION_STATE_FLOAT;
        xrValue.next = NULL;
        XrActionStateGetInfo getInfo;
        getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
        getInfo.next = NULL;
        getInfo.action = xrAction;
        getInfo.subactionPath = 0;

        XrResult result = xrGetActionStateFloat(m_session, &getInfo, &xrValue);
        xrCheck(m_instance, result, "failed to get track value!");

        if (xrValue.isActive)
        {
            val = xrValue.currentState;
            return true;
        }
        return false;
    }

    bool OpenXRInput::extractBooleanValue(XrAction xrAction, bool &val) const
    {
        if (!xrAction)
        {
            return false;
        }
        XrActionStateBoolean xrValue;
        xrValue.type = XR_TYPE_ACTION_STATE_BOOLEAN;
        xrValue.next = NULL;
        XrActionStateGetInfo getInfo;
        getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
        getInfo.next = NULL;
        getInfo.action = xrAction;
        getInfo.subactionPath = 0;

        XrResult result = xrGetActionStateBoolean(m_session, &getInfo, &xrValue);
        xrCheck(m_instance, result, "failed to get track value!");

        if (xrValue.isActive && xrValue.changedSinceLastSync)
        {
            val = xrValue.currentState;
            return true;
        }
        return false;
    }


    void OpenXRInput::handleTrackpadEvent(Hand hand)
    {
        int handIdx = static_cast<int>(hand);
        auto clickType = hand == Hand::LEFT ? OpenXRInputType::LEFT_TRACKPAD_CLICK : OpenXRInputType::RIGHT_TRACKPAD_CLICK;
        auto padXType = hand == Hand::LEFT ? OpenXRInputType::LEFT_TRACKPAD_X : OpenXRInputType::RIGHT_TRACKPAD_X;
        auto padYType = hand == Hand::LEFT ? OpenXRInputType::LEFT_TRACKPAD_Y : OpenXRInputType::RIGHT_TRACKPAD_Y;

        bool click = false;
        if (extractBooleanValue(getXrAction(clickType), click))
        {
            if (click)
            {
                if (m_stickClickCallback[handIdx])
                {
                    m_stickClickCallback[handIdx]();
                }

                // HTC Vive controller keeps sending trackpad move events when idle. Let's filter the trackpad move
                // with a click event
                float x, y;
                if (extractFloatValue(getXrAction(padXType), x) &&
                    extractFloatValue(getXrAction(padYType), y))
                {
                    if (m_stickMoveCallback[handIdx])
                    {
                        m_stickMoveCallback[handIdx](x, y);
                    }
                }
            }
        }
    }

    void OpenXRInput::handleThumbstickEvent(Hand hand)
    {
        int handIdx = static_cast<int>(hand);
        auto stickXType = hand == Hand::LEFT ? OpenXRInputType::LEFT_THUMBSTICK_X : OpenXRInputType::RIGHT_THUMBSTICK_X;
        auto stickYType = hand == Hand::LEFT ? OpenXRInputType::LEFT_THUMBSTICK_Y : OpenXRInputType::RIGHT_THUMBSTICK_Y;
        float x, y;
        if (extractFloatValue(getXrAction(stickXType), x) &&
            extractFloatValue(getXrAction(stickYType), y))
        {
            if (m_stickMoveCallback[handIdx])
            {
                m_stickMoveCallback[handIdx](x, y);
            }
        }

        auto clickType = hand == Hand::LEFT ? OpenXRInputType::LEFT_THUMBSTICK_CLICK : OpenXRInputType::RIGHT_TRACKPAD_CLICK;
        bool click = false;
        if (extractBooleanValue(getXrAction(clickType), click))
        {
            if (click && m_stickClickCallback[handIdx])
            {
                m_stickClickCallback[handIdx]();
            }
        }
    }

    void OpenXRInput::handleTriggerEvent(Hand hand)
    {
        int handIdx = static_cast<int>(hand);
        auto triggerType = hand == Hand::LEFT ? OpenXRInputType::LEFT_TRIGGER : OpenXRInputType::RIGHT_TRIGGER;
        float trigger = 0.0f;
        if (extractFloatValue(getXrAction(triggerType), trigger))
        {
            if (m_triggerCallback[handIdx])
            {
                m_triggerCallback[handIdx](trigger);
            }
        }
    }
}
