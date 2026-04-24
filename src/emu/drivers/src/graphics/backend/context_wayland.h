// NextOS: stub do context_wayland — device sem Wayland; classe vira alias
// inline pra gl_context_egl. context_wayland.cpp também está stubbed.

#pragma once

#include "context_egl.h"

namespace eka2l1::drivers::graphics {
    class gl_context_egl_wayland : public gl_context_egl {
    public:
        gl_context_egl_wayland() = default;
        gl_context_egl_wayland(const window_system_info& wsi, bool stereo, bool core)
            : gl_context_egl(wsi, stereo, core, true /*gles*/) {}
        ~gl_context_egl_wayland() override = default;
    };
}