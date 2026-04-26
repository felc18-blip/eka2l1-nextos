#pragma once

#include <drivers/graphics/common.h>
#include <glad/glad.h>

namespace eka2l1::drivers {
    enum {
        GL_BACKEND_MAX_VBO_SLOTS = 10
    };

    GLenum data_format_to_gl_enum(const data_format format);
    GLint texture_format_to_gl_enum(const texture_format format);
    // NextOS GLES2 port: ES 2.0 only accepts unsized base internal formats
    // (GL_RGB / GL_RGBA / GL_LUMINANCE / GL_ALPHA) and a few sized ones for
    // renderbuffer storage.  This helper coerces the GL 3.x sized formats
    // (GL_RGBA8, GL_R8, GL_RG, GL_BGR, ...) into something the Mali blob
    // accepts when running on the ES 2 path.
    GLint texture_format_to_gl_enum_es2(const texture_format format, bool for_renderbuffer);

    // NextOS GLES2 port: set by the OGL backend at init time so helpers
    // outside the driver class (texture / framebuffer / buffer) can pick
    // ES 2-safe enums without holding a driver pointer.
    void eka2l1_ogl_set_strict_active(bool strict);
    bool eka2l1_ogl_is_strict_active();
    GLint texture_data_type_to_gl_enum(const texture_data_type data_type);
    GLint to_filter_option(const filter_option op);
    GLenum to_tex_parameter_enum(const addressing_direction dir);
    GLint to_tex_wrapping_enum(const addressing_option opt);
}