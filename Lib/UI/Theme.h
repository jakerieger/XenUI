//
// Created by conta on 3/18/2024.
//

#pragma once

#include "XenAPI.h"
#include "Color.h"

namespace Xen {
    struct XEN_API Theme {
        Color WindowBackground;
        Color FrameBackground;
        Color ContentBackground;
        Color FrameBorder;
        Color Border;
        Color Primary;
        Color Secondary;
        Color Tertiary;
        Color Success;
        Color Warning;
        Color Error;
        Color Info;
        Color TextHighlight;
        Color TextPrimary;
        Color TextSecondary;
        Color TextDisabled;
        Color Black       = Colors::Black;
        Color White       = Colors::White;
        Color Transparent = Colors::Transparent;
    };

    inline const Theme XEN_API XenDarkTheme = {.WindowBackground  = Color(0xFF0A0B10),
                                               .FrameBackground   = Color(0xFF161722),
                                               .ContentBackground = Color(0xFF1A1C29),
                                               .FrameBorder       = Color(0xFF242538),
                                               .Border            = Color(0xFF363a55),
                                               .Primary           = Color(0xFFFF3366),
                                               .Secondary         = Color(0xFF7aa2f7),
                                               .Tertiary          = Color(0xFF2ac3de),
                                               .Success           = Color(0xFF9ece6a),
                                               .Warning           = Color(0xFF0A0B10),
                                               .Error             = Color(0xFFFF3366),
                                               .Info              = Color(0xFF7dcfff),
                                               .TextHighlight     = Color(0xFFdee2ff),
                                               .TextPrimary       = Color(0xFFabb1dc),
                                               .TextSecondary     = Color(0xFF8f94ba),
                                               .TextDisabled      = Color(0xFF5d6079)};
}  // namespace Xen