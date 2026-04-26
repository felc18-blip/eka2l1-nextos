#include <drivers/graphics/backend/ogl/common_ogl.h>

// glad's generated header only exposes GL_BGRA (same numeric value),
// the EXT_texture_format_BGRA8888 token used on ES 2.0.
#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif

namespace eka2l1::drivers {
    // NextOS GLES2 port: simple module-level flag flipped on by the OGL
    // backend when the active context is GLES.  Plain bool is fine — the
    // driver thread is the only one calling into these helpers.
    static bool g_ogl_strict_active = false;

    void eka2l1_ogl_set_strict_active(bool strict) { g_ogl_strict_active = strict; }
    bool eka2l1_ogl_is_strict_active() { return g_ogl_strict_active; }

    GLenum data_format_to_gl_enum(const data_format format) {
        switch (format) {
        case data_format::byte:
            return GL_UNSIGNED_BYTE;

        case data_format::sbyte:
            return GL_BYTE;

        case data_format::sword:
            return GL_SHORT;

        case data_format::word:
            return GL_UNSIGNED_SHORT;

        case data_format::uint:
            return GL_UNSIGNED_INT;

        case data_format::sint:
            return GL_INT;

        case data_format::sfloat:
            return GL_FLOAT;

        case data_format::fixed:
            return GL_FIXED;

        default:
            break;
        }

        return GL_INVALID_ENUM;
    }
    
    GLint texture_format_to_gl_enum(const texture_format format) {
        switch (format) {
        case texture_format::r:
            return GL_RED;

        case texture_format::r8:
            return GL_R8;

        case texture_format::rg:
            return GL_RG;

        case texture_format::rg8:
            return GL_RG8;

        case texture_format::rgb:
            return GL_RGB;

        case texture_format::bgr:
            return GL_BGR;

        case texture_format::bgra:
            return GL_BGRA;

        case texture_format::rgba:
            return GL_RGBA;

        case texture_format::rgba4:
            return GL_RGBA4;

        case texture_format::rgb565:
            return GL_RGB565;

        case texture_format::rgb5_a1:
            return GL_RGB5_A1;

        case texture_format::depth24_stencil8:
            return GL_DEPTH24_STENCIL8;

        case texture_format::depth_stencil:
            return GL_DEPTH_STENCIL;

        case texture_format::depth16:
            return GL_DEPTH_COMPONENT16;

        case texture_format::stencil8:
            return GL_STENCIL_INDEX8;

        case texture_format::etc2_rgb8:
            return GL_COMPRESSED_RGB8_ETC2;
        
        case texture_format::pvrtc_4bppv1_rgba:
            return GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG;

        case texture_format::pvrtc_2bppv1_rgba:
            return GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG;

        case texture_format::pvrtc_4bppv1_rgb:
            return GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG;

        case texture_format::pvrtc_2bppv1_rgb:
            return GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG;

        default:
            break;
        }

        return 0;
    }

    // NextOS GLES2 port (Mali-450): the regular helper hands back GL 3.x
    // sized internal formats which ES 2.0 rejects with GL_INVALID_ENUM
    // (1280) on glTexImage2D.  ES 2.0 wants the unsized base format AND
    // requires that internalformat == format on upload.  For renderbuffer
    // storage a small set of sized formats (RGBA4/RGB565/RGB5_A1, depth16,
    // stencil8, depth24_stencil8 via OES_packed_depth_stencil) is allowed.
    GLint texture_format_to_gl_enum_es2(const texture_format format, bool for_renderbuffer) {
        switch (format) {
        case texture_format::r:
        case texture_format::r8:
            // Single-channel: ES 2.0 has GL_LUMINANCE / GL_ALPHA only.
            // LUMINANCE replicates the value to RGB which is what most
            // Symbian masks/alpha-only textures actually want.
            return GL_LUMINANCE;
        case texture_format::rg:
        case texture_format::rg8:
            return GL_LUMINANCE_ALPHA;
        case texture_format::rgb:
            return GL_RGB;
        case texture_format::bgr:
            // No BGR in ES 2.  Caller is responsible for swizzling on
            // upload; presenting as RGB so the driver accepts the call.
            return GL_RGB;
        case texture_format::bgra:
            // Mali blob exposes EXT_texture_format_BGRA8888 which maps
            // GL_BGRA_EXT as both internal and format.  Symbian app
            // framebuffers are BGRA, so this is the hot path.
            return GL_BGRA_EXT;
        case texture_format::rgba:
            return GL_RGBA;
        case texture_format::rgba4:
            return for_renderbuffer ? GL_RGBA4 : GL_RGBA;
        case texture_format::rgb565:
            return for_renderbuffer ? GL_RGB565 : GL_RGB;
        case texture_format::rgb5_a1:
            return for_renderbuffer ? GL_RGB5_A1 : GL_RGBA;
        case texture_format::depth24_stencil8:
            return GL_DEPTH24_STENCIL8;
        case texture_format::depth_stencil:
            return GL_DEPTH_STENCIL;
        case texture_format::depth16:
            return GL_DEPTH_COMPONENT16;
        case texture_format::stencil8:
            return GL_STENCIL_INDEX8;
        case texture_format::etc2_rgb8:
            return GL_COMPRESSED_RGB8_ETC2;
        case texture_format::pvrtc_4bppv1_rgba:
            return GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG;
        case texture_format::pvrtc_2bppv1_rgba:
            return GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG;
        case texture_format::pvrtc_4bppv1_rgb:
            return GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG;
        case texture_format::pvrtc_2bppv1_rgb:
            return GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG;
        default:
            break;
        }
        return 0;
    }

    GLint texture_data_type_to_gl_enum(const texture_data_type data_type) {
        switch (data_type) {
        case texture_data_type::ubyte:
            return GL_UNSIGNED_BYTE;

        case texture_data_type::ushort:
            return GL_UNSIGNED_SHORT;

        case texture_data_type::uint_24_8:
            return GL_UNSIGNED_INT_24_8;

        case texture_data_type::ushort_4_4_4_4:
            return GL_UNSIGNED_SHORT_4_4_4_4;

        case texture_data_type::ushort_5_6_5:
            return GL_UNSIGNED_SHORT_5_6_5;

        case texture_data_type::ushort_5_5_5_1:
            return GL_UNSIGNED_SHORT_5_5_5_1;

        default:
            break;
        }

        return 0;
    }

    GLint to_filter_option(const filter_option op) {
        switch (op) {
        case filter_option::linear:
            return GL_LINEAR;

        case filter_option::nearest:
            return GL_NEAREST;

        case filter_option::linear_mipmap_linear:
            return GL_LINEAR_MIPMAP_LINEAR;

        case filter_option::linear_mipmap_nearest:
            return GL_LINEAR_MIPMAP_NEAREST;

        case filter_option::nearest_mipmap_linear:
            return GL_NEAREST_MIPMAP_LINEAR;

        case filter_option::nearest_mipmap_nearest:
            return GL_NEAREST_MIPMAP_NEAREST;

        default:
            break;
        }

        return 0;
    }
    
    GLenum to_tex_parameter_enum(const addressing_direction dir) {
        switch (dir) {
        case addressing_direction::s:
            return GL_TEXTURE_WRAP_S;

        case addressing_direction::t:
            return GL_TEXTURE_WRAP_T;

        case addressing_direction::r:
            return GL_TEXTURE_WRAP_R;

        default:
            break;
        }

        return 0;
    }

    GLint to_tex_wrapping_enum(const addressing_option opt) {
        switch (opt) {
        case addressing_option::clamp_to_edge:
            return GL_CLAMP_TO_EDGE;

        case addressing_option::repeat:
            return GL_REPEAT;

        default:
            break;
        }

        return 0;
    }
}