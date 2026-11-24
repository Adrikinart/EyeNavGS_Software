// SPDX-FileCopyrightText: 2025 Systems and Multimedia Lab @ Rutgers University
// SPDX-License-Identifier: Apache-2.0
//
// This file is part of the 👁️NavGS (EyeNavGS) project.
// See LICENSE_EYENAVGS.md and NOTICE for details.

#include <core/openxr/interactionprofiles/EyeGazeInteractionProfile.hpp>

namespace sibr
{
    std::string EyeGazeInteractionProfile::getName() const {
        return "/interaction_profiles/ext/eye_gaze_interaction";
    }

    // See specs: https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_EXT_eye_gaze_interaction
    std::map<OpenXRInputType, std::string> EyeGazeInteractionProfile::getBindings() const {
        return {
            {OpenXRInputType::EYE_GAZE_POSE, "/user/eyes_ext/input/gaze_ext/pose"}
        };
    }

}
