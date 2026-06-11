/*
===========================================================================
Desktop GL 1.x constants for GLES builds (compile-time only where needed)
Each symbol guarded individually — ES2 headers define some but not all.
===========================================================================
*/
#ifndef GL_LEGACY_CONST_H
#define GL_LEGACY_CONST_H

#ifndef GL_TEXTURE0_ARB
#define GL_TEXTURE0_ARB 0x84C0
#endif
#ifndef GL_TEXTURE1_ARB
#define GL_TEXTURE1_ARB 0x84C1
#endif
#ifndef GL_TEXTURE2_ARB
#define GL_TEXTURE2_ARB 0x84C2
#endif

#ifndef GL_LINE
#define GL_LINE 0x1B01
#endif
#ifndef GL_FILL
#define GL_FILL 0x1B02
#endif
#ifndef GL_ALPHA_TEST
#define GL_ALPHA_TEST 0x0BC0
#endif
#ifndef GL_QUADS
#define GL_QUADS 0x0007
#endif
#ifndef GL_POLYGON
#define GL_POLYGON 0x0009
#endif

#ifndef GL_BACK_LEFT
#define GL_BACK_LEFT 0x0402
#endif
#ifndef GL_BACK_RIGHT
#define GL_BACK_RIGHT 0x0403
#endif
#ifndef GL_STENCIL_INDEX
#define GL_STENCIL_INDEX 0x1901
#endif

#ifndef GL_RGBA4
#define GL_RGBA4 0x8056
#endif
#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif
#ifndef GL_RGB5
#define GL_RGB5 0x8050
#endif
#ifndef GL_RGB8
# if defined( GL_RGB8_OES )
#  define GL_RGB8 GL_RGB8_OES
# else
#  define GL_RGB8 0x8051
# endif
#endif
#ifndef GL_LUMINANCE8
#define GL_LUMINANCE8 0x8040
#endif
#ifndef GL_LUMINANCE16
#define GL_LUMINANCE16 0x8042
#endif
#ifndef GL_LUMINANCE8_ALPHA8
#define GL_LUMINANCE8_ALPHA8 0x8045
#endif
#ifndef GL_LUMINANCE16_ALPHA16
#define GL_LUMINANCE16_ALPHA16 0x8048
#endif

#ifndef GL_RGB4_S3TC
#define GL_RGB4_S3TC 0x83A0
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
#endif
#ifndef GL_COMPRESSED_RGBA_BPTC_UNORM_ARB
#define GL_COMPRESSED_RGBA_BPTC_UNORM_ARB 0x8E8C
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB
#define GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB 0x8E8D
#endif
#ifndef GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT
#define GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT 0x8C72
#endif

#ifndef GL_SRGB_EXT
#define GL_SRGB_EXT 0x8C40
#endif
#ifndef GL_SRGB8_EXT
#define GL_SRGB8_EXT 0x8C41
#endif
#ifndef GL_SRGB_ALPHA_EXT
#define GL_SRGB_ALPHA_EXT 0x8C42
#endif
#ifndef GL_SRGB8_ALPHA8_EXT
#define GL_SRGB8_ALPHA8_EXT 0x8C43
#endif
#ifndef GL_SLUMINANCE_EXT
#define GL_SLUMINANCE_EXT 0x8C46
#endif
#ifndef GL_SLUMINANCE8_EXT
#define GL_SLUMINANCE8_EXT 0x8C47
#endif
#ifndef GL_SLUMINANCE_ALPHA_EXT
#define GL_SLUMINANCE_ALPHA_EXT 0x8C44
#endif
#ifndef GL_SLUMINANCE8_ALPHA8_EXT
#define GL_SLUMINANCE8_ALPHA8_EXT 0x8C45
#endif

#ifndef GL_STACK_OVERFLOW
#define GL_STACK_OVERFLOW 0x0503
#endif
#ifndef GL_STACK_UNDERFLOW
#define GL_STACK_UNDERFLOW 0x0504
#endif
#ifndef GL_SMOOTH
#define GL_SMOOTH 0x1D01
#endif
#ifndef GL_VERTEX_ARRAY
#define GL_VERTEX_ARRAY 0x8074
#endif
#ifndef GL_FRONT_AND_BACK
#define GL_FRONT_AND_BACK 0x0408
#endif
#ifndef GL_TEXTURE_BORDER_COLOR
#define GL_TEXTURE_BORDER_COLOR 0x1004
#endif
#ifndef GL_COLOR_ARRAY
#define GL_COLOR_ARRAY 0x8076
#endif
#ifndef GL_TEXTURE_COORD_ARRAY
#define GL_TEXTURE_COORD_ARRAY 0x8078
#endif
#ifndef GL_NORMAL_ARRAY
#define GL_NORMAL_ARRAY 0x8075
#endif
#ifndef GL_POLYGON_OFFSET_FILL
#define GL_POLYGON_OFFSET_FILL 0x8037
#endif

#endif /* GL_LEGACY_CONST_H */
