// SPDX-FileCopyrightText: 2025 Systems and Multimedia Lab @ Rutgers University
// SPDX-License-Identifier: Apache-2.0
//
// This file is part of the 👁️NavGS (EyeNavGS) project.
// See LICENSE_EYENAVGS.md and NOTICE for details.

#pragma once

#include <core/openxr/interactionprofiles/IInteractionProfile.hpp>

namespace sibr
{
    /**
     * Interaction profile of the cross-vendor XR_EXT_eye_gaze_interaction extension.
     *
     * This is how eye gaze is exposed by runtimes/API layers that do not implement
     * XR_FB_eye_tracking_social (Meta Quest Pro only), e.g. the HTC VIVE Pro Eye through
     * SteamVR + "VIVE Console for SteamVR" (XR_APILAYER_VIVE_facial_tracking layer),
     * Varjo, Pico, Windows Mixed Reality (HP Reverb G2 Omnicept), ...
     *
     * The profile only exposes one pose: the combined ("cyclopean") gaze of both eyes.
     * See https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_EXT_eye_gaze_interaction
     */
    class EyeGazeInteractionProfile : public IInteractionProfile
    {
    public:
        EyeGazeInteractionProfile() {};
        std::string getName() const override;
        std::map<OpenXRInputType, std::string> getBindings() const override;
    };

}
