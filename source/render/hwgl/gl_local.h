//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, version 3 of the License ONLY.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**  You should have received a copy of the GNU General Public License
//**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//**
//**************************************************************************
#ifndef VAVOOM_GL_LOCAL_HEADER
#define VAVOOM_GL_LOCAL_HEADER

#ifdef _WIN32
# include <windows.h>
#endif
#ifdef USE_GLAD
# include "glad.h"
#else
# include <GL/gl.h>
#endif
#ifdef _WIN32
# include <GL/glext.h>
#endif

#ifndef APIENTRY
#define APIENTRY
#endif

#include "gamedefs.h"
#include "cl_local.h"
#include "../r_shared.h"


// extensions

// ARB_multitexture
#ifndef GL_ARB_multitexture
#define GL_TEXTURE0_ARB           0x84C0
#define GL_TEXTURE1_ARB           0x84C1
#define GL_TEXTURE2_ARB           0x84C2
#define GL_TEXTURE3_ARB           0x84C3
#define GL_TEXTURE4_ARB           0x84C4
#define GL_TEXTURE5_ARB           0x84C5
#define GL_TEXTURE6_ARB           0x84C6
#define GL_TEXTURE7_ARB           0x84C7
#define GL_TEXTURE8_ARB           0x84C8
#define GL_TEXTURE9_ARB           0x84C9
#define GL_TEXTURE10_ARB          0x84CA
#define GL_TEXTURE11_ARB          0x84CB
#define GL_TEXTURE12_ARB          0x84CC
#define GL_TEXTURE13_ARB          0x84CD
#define GL_TEXTURE14_ARB          0x84CE
#define GL_TEXTURE15_ARB          0x84CF
#define GL_TEXTURE16_ARB          0x84D0
#define GL_TEXTURE17_ARB          0x84D1
#define GL_TEXTURE18_ARB          0x84D2
#define GL_TEXTURE19_ARB          0x84D3
#define GL_TEXTURE20_ARB          0x84D4
#define GL_TEXTURE21_ARB          0x84D5
#define GL_TEXTURE22_ARB          0x84D6
#define GL_TEXTURE23_ARB          0x84D7
#define GL_TEXTURE24_ARB          0x84D8
#define GL_TEXTURE25_ARB          0x84D9
#define GL_TEXTURE26_ARB          0x84DA
#define GL_TEXTURE27_ARB          0x84DB
#define GL_TEXTURE28_ARB          0x84DC
#define GL_TEXTURE29_ARB          0x84DD
#define GL_TEXTURE30_ARB          0x84DE
#define GL_TEXTURE31_ARB          0x84DF
#define GL_ACTIVE_TEXTURE_ARB       0x84E0
#define GL_CLIENT_ACTIVE_TEXTURE_ARB    0x84E1
#define GL_MAX_TEXTURE_UNITS_ARB      0x84E2
#endif

//typedef void (APIENTRY*glMultiTexCoord2fARB_t)(GLenum, GLfloat, GLfloat);
typedef void (APIENTRY*glActiveTextureARB_t)(GLenum);

// EXT_point_parameters
#ifndef GL_EXT_point_parameters
#define GL_POINT_SIZE_MIN_EXT       0x8126
#define GL_POINT_SIZE_MAX_EXT       0x8127
#define GL_POINT_FADE_THRESHOLD_SIZE_EXT  0x8128
#define GL_DISTANCE_ATTENUATION_EXT     0x8129
#endif

typedef void (APIENTRY*glPointParameterfEXT_t)(GLenum, GLfloat);
typedef void (APIENTRY*glPointParameterfvEXT_t)(GLenum, const GLfloat *);

// EXT_texture_filter_anisotropic
#ifndef GL_EXT_texture_filter_anisotropic
#define GL_TEXTURE_MAX_ANISOTROPY_EXT   0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

// SGIS_texture_edge_clamp
#ifndef GL_SGIS_texture_edge_clamp
#define GL_CLAMP_TO_EDGE_SGIS       0x812F
#endif

typedef void (APIENTRY*glStencilFuncSeparate_t)(GLenum, GLenum, GLint, GLuint);
typedef void (APIENTRY*glStencilOpSeparate_t)(GLenum, GLenum, GLenum, GLenum);

#ifndef GL_EXT_stencil_wrap
#define GL_INCR_WRAP_EXT          0x8507
#define GL_DECR_WRAP_EXT          0x8508
#endif

#ifndef GL_ARB_shader_objects
#define GL_PROGRAM_OBJECT_ARB       0x8B40
#define GL_SHADER_OBJECT_ARB        0x8B48
#define GL_OBJECT_TYPE_ARB          0x8B4E
#define GL_OBJECT_SUBTYPE_ARB       0x8B4F
#define GL_FLOAT_VEC2_ARB         0x8B50
#define GL_FLOAT_VEC3_ARB         0x8B51
#define GL_FLOAT_VEC4_ARB         0x8B52
#define GL_INT_VEC2_ARB           0x8B53
#define GL_INT_VEC3_ARB           0x8B54
#define GL_INT_VEC4_ARB           0x8B55
#define GL_BOOL_ARB             0x8B56
#define GL_BOOL_VEC2_ARB          0x8B57
#define GL_BOOL_VEC3_ARB          0x8B58
#define GL_BOOL_VEC4_ARB          0x8B59
#define GL_FLOAT_MAT2_ARB         0x8B5A
#define GL_FLOAT_MAT3_ARB         0x8B5B
#define GL_FLOAT_MAT4_ARB         0x8B5C
#define GL_SAMPLER_1D_ARB         0x8B5D
#define GL_SAMPLER_2D_ARB         0x8B5E
#define GL_SAMPLER_3D_ARB         0x8B5F
#define GL_SAMPLER_CUBE_ARB         0x8B60
#define GL_SAMPLER_1D_SHADOW_ARB      0x8B61
#define GL_SAMPLER_2D_SHADOW_ARB      0x8B62
#define GL_SAMPLER_2D_RECT_ARB        0x8B63
#define GL_SAMPLER_2D_RECT_SHADOW_ARB   0x8B64
#define GL_OBJECT_DELETE_STATUS_ARB     0x8B80
#define GL_OBJECT_COMPILE_STATUS_ARB    0x8B81
#define GL_OBJECT_LINK_STATUS_ARB     0x8B82
#define GL_OBJECT_VALIDATE_STATUS_ARB   0x8B83
#define GL_OBJECT_INFO_LOG_LENGTH_ARB   0x8B84
#define GL_OBJECT_ATTACHED_OBJECTS_ARB    0x8B85
#define GL_OBJECT_ACTIVE_UNIFORMS_ARB   0x8B86
#define GL_OBJECT_ACTIVE_UNIFORM_MAX_LENGTH_ARB 0x8B87
#define GL_OBJECT_SHADER_SOURCE_LENGTH_ARB  0x8B88

typedef char GLcharARB;
typedef unsigned int GLhandleARB;
#endif

typedef void (APIENTRY*glDeleteObjectARB_t)(GLhandleARB);
typedef GLhandleARB (APIENTRY*glGetHandleARB_t)(GLenum);
typedef void (APIENTRY*glDetachObjectARB_t)(GLhandleARB, GLhandleARB);
typedef GLhandleARB (APIENTRY*glCreateShaderObjectARB_t)(GLenum);
typedef void (APIENTRY*glShaderSourceARB_t)(GLhandleARB, GLsizei, const GLcharARB **, const GLint *);
typedef void (APIENTRY*glCompileShaderARB_t)(GLhandleARB);
typedef GLhandleARB (APIENTRY*glCreateProgramObjectARB_t)(void);
typedef void (APIENTRY*glAttachObjectARB_t)(GLhandleARB, GLhandleARB);
typedef void (APIENTRY*glLinkProgramARB_t)(GLhandleARB);
typedef void (APIENTRY*glUseProgramObjectARB_t)(GLhandleARB);
typedef void (APIENTRY*glValidateProgramARB_t)(GLhandleARB);
typedef void (APIENTRY*glUniform1fARB_t)(GLint, GLfloat);
typedef void (APIENTRY*glUniform2fARB_t)(GLint, GLfloat, GLfloat);
typedef void (APIENTRY*glUniform3fARB_t)(GLint, GLfloat, GLfloat, GLfloat);
typedef void (APIENTRY*glUniform4fARB_t)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void (APIENTRY*glUniform1iARB_t)(GLint, GLint);
typedef void (APIENTRY*glUniform2iARB_t)(GLint, GLint, GLint);
typedef void (APIENTRY*glUniform3iARB_t)(GLint, GLint, GLint, GLint);
typedef void (APIENTRY*glUniform4iARB_t)(GLint, GLint, GLint, GLint, GLint);
typedef void (APIENTRY*glUniform1fvARB_t)(GLint, GLsizei, const GLfloat *);
typedef void (APIENTRY*glUniform2fvARB_t)(GLint, GLsizei, const GLfloat *);
typedef void (APIENTRY*glUniform3fvARB_t)(GLint, GLsizei, const GLfloat *);
typedef void (APIENTRY*glUniform4fvARB_t)(GLint, GLsizei, const GLfloat *);
typedef void (APIENTRY*glUniform1ivARB_t)(GLint, GLsizei, const GLint *);
typedef void (APIENTRY*glUniform2ivARB_t)(GLint, GLsizei, const GLint *);
typedef void (APIENTRY*glUniform3ivARB_t)(GLint, GLsizei, const GLint *);
typedef void (APIENTRY*glUniform4ivARB_t)(GLint, GLsizei, const GLint *);
typedef void (APIENTRY*glUniformMatrix2fvARB_t)(GLint, GLsizei, GLboolean, const GLfloat *);
typedef void (APIENTRY*glUniformMatrix3fvARB_t)(GLint, GLsizei, GLboolean, const GLfloat *);
typedef void (APIENTRY*glUniformMatrix4fvARB_t)(GLint, GLsizei, GLboolean, const GLfloat *);
typedef void (APIENTRY*glGetObjectParameterfvARB_t)(GLhandleARB, GLenum, GLfloat *);
typedef void (APIENTRY*glGetObjectParameterivARB_t)(GLhandleARB, GLenum, GLint *);
typedef void (APIENTRY*glGetInfoLogARB_t)(GLhandleARB, GLsizei, GLsizei *, GLcharARB *);
typedef void (APIENTRY*glGetAttachedObjectsARB_t)(GLhandleARB, GLsizei, GLsizei *, GLhandleARB *);
typedef GLint (APIENTRY*glGetUniformLocationARB_t)(GLhandleARB, const GLcharARB *);
typedef void (APIENTRY*glGetActiveUniformARB_t)(GLhandleARB, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, GLcharARB *);
typedef void (APIENTRY*glGetActiveAttribARB_t)(GLhandleARB, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, GLcharARB *);
typedef void (APIENTRY*glGetUniformfvARB_t)(GLhandleARB, GLint, GLfloat *);
typedef void (APIENTRY*glGetUniformivARB_t)(GLhandleARB, GLint, GLint *);
typedef void (APIENTRY*glGetShaderSourceARB_t)(GLhandleARB, GLsizei, GLsizei *, GLcharARB *);
typedef void (APIENTRY*glDepthBoundsEXT_t)(GLclampd zmin, GLclampd zmax);

#ifndef GL_ARB_vertex_shader
#define GL_VERTEX_SHADER_ARB        0x8B31
#define GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB  0x8B4A
#define GL_MAX_VARYING_FLOATS_ARB     0x8B4B
#define GL_MAX_VERTEX_ATTRIBS_ARB     0x8869
#define GL_MAX_TEXTURE_IMAGE_UNITS_ARB    0x8872
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS_ARB 0x8B4C
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB 0x8B4D
#define GL_MAX_TEXTURE_COORDS_ARB     0x8871
#define GL_VERTEX_PROGRAM_POINT_SIZE_ARB  0x8642
#define GL_VERTEX_PROGRAM_TWO_SIDE_ARB    0x8643
#define GL_OBJECT_ACTIVE_ATTRIBUTES_ARB   0x8B89
#define GL_OBJECT_ACTIVE_ATTRIBUTE_MAX_LENGTH_ARB 0x8B8A
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED_ARB  0x8622
#define GL_VERTEX_ATTRIB_ARRAY_SIZE_ARB   0x8623
#define GL_VERTEX_ATTRIB_ARRAY_STRIDE_ARB 0x8624
#define GL_VERTEX_ATTRIB_ARRAY_TYPE_ARB   0x8625
#define GL_VERTEX_ATTRIB_ARRAY_NORMALIZED_ARB 0x886A
#define GL_CURRENT_VERTEX_ATTRIB_ARB    0x8626
#define GL_VERTEX_ATTRIB_ARRAY_POINTER_ARB  0x8645
#endif

typedef void (APIENTRY*glVertexAttrib1dARB_t)(GLuint, GLdouble);
typedef void (APIENTRY*glVertexAttrib1dvARB_t)(GLuint, const GLdouble *);
typedef void (APIENTRY*glVertexAttrib1fARB_t)(GLuint, GLfloat);
typedef void (APIENTRY*glVertexAttrib1fvARB_t)(GLuint, const GLfloat *);
typedef void (APIENTRY*glVertexAttrib1sARB_t)(GLuint, GLshort);
typedef void (APIENTRY*glVertexAttrib1svARB_t)(GLuint, const GLshort *);
typedef void (APIENTRY*glVertexAttrib2dARB_t)(GLuint, GLdouble, GLdouble);
typedef void (APIENTRY*glVertexAttrib2dvARB_t)(GLuint, const GLdouble *);
typedef void (APIENTRY*glVertexAttrib2fARB_t)(GLuint, GLfloat, GLfloat);
typedef void (APIENTRY*glVertexAttrib2fvARB_t)(GLuint, const GLfloat *);
typedef void (APIENTRY*glVertexAttrib2sARB_t)(GLuint, GLshort, GLshort);
typedef void (APIENTRY*glVertexAttrib2svARB_t)(GLuint, const GLshort *);
typedef void (APIENTRY*glVertexAttrib3dARB_t)(GLuint, GLdouble, GLdouble, GLdouble);
typedef void (APIENTRY*glVertexAttrib3dvARB_t)(GLuint, const GLdouble *);
typedef void (APIENTRY*glVertexAttrib3fARB_t)(GLuint, GLfloat, GLfloat, GLfloat);
typedef void (APIENTRY*glVertexAttrib3fvARB_t)(GLuint, const GLfloat *);
typedef void (APIENTRY*glVertexAttrib3sARB_t)(GLuint, GLshort, GLshort, GLshort);
typedef void (APIENTRY*glVertexAttrib3svARB_t)(GLuint, const GLshort *);
typedef void (APIENTRY*glVertexAttrib4NbvARB_t)(GLuint, const GLbyte *);
typedef void (APIENTRY*glVertexAttrib4NivARB_t)(GLuint, const GLint *);
typedef void (APIENTRY*glVertexAttrib4NsvARB_t)(GLuint, const GLshort *);
typedef void (APIENTRY*glVertexAttrib4NubARB_t)(GLuint, GLubyte, GLubyte, GLubyte, GLubyte);
typedef void (APIENTRY*glVertexAttrib4NubvARB_t)(GLuint, const GLubyte *);
typedef void (APIENTRY*glVertexAttrib4NuivARB_t)(GLuint, const GLuint *);
typedef void (APIENTRY*glVertexAttrib4NusvARB_t)(GLuint, const GLushort *);
typedef void (APIENTRY*glVertexAttrib4bvARB_t)(GLuint, const GLbyte *);
typedef void (APIENTRY*glVertexAttrib4dARB_t)(GLuint, GLdouble, GLdouble, GLdouble, GLdouble);
typedef void (APIENTRY*glVertexAttrib4dvARB_t)(GLuint, const GLdouble *);
typedef void (APIENTRY*glVertexAttrib4fARB_t)(GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void (APIENTRY*glVertexAttrib4fvARB_t)(GLuint, const GLfloat *);
typedef void (APIENTRY*glVertexAttrib4ivARB_t)(GLuint, const GLint *);
typedef void (APIENTRY*glVertexAttrib4sARB_t)(GLuint, GLshort, GLshort, GLshort, GLshort);
typedef void (APIENTRY*glVertexAttrib4svARB_t)(GLuint, const GLshort *);
typedef void (APIENTRY*glVertexAttrib4ubvARB_t)(GLuint, const GLubyte *);
typedef void (APIENTRY*glVertexAttrib4uivARB_t)(GLuint, const GLuint *);
typedef void (APIENTRY*glVertexAttrib4usvARB_t)(GLuint, const GLushort *);
typedef void (APIENTRY*glVertexAttribPointerARB_t)(GLuint, GLint, GLenum, GLboolean, GLsizei, const GLvoid *);
typedef void (APIENTRY*glEnableVertexAttribArrayARB_t)(GLuint);
typedef void (APIENTRY*glDisableVertexAttribArrayARB_t)(GLuint);
typedef void (APIENTRY*glBindAttribLocationARB_t)(GLhandleARB, GLuint, const GLcharARB *);
typedef void (APIENTRY*glGetActiveAttribARB_t)(GLhandleARB, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, GLcharARB *);
typedef GLint (APIENTRY*glGetAttribLocationARB_t)(GLhandleARB, const GLcharARB *);
typedef void (APIENTRY*glGetVertexAttribdvARB_t)(GLuint, GLenum, GLdouble *);
typedef void (APIENTRY*glGetVertexAttribfvARB_t)(GLuint, GLenum, GLfloat *);
typedef void (APIENTRY*glGetVertexAttribivARB_t)(GLuint, GLenum, GLint *);
typedef void (APIENTRY*glGetVertexAttribPointervARB_t)(GLuint, GLenum, GLvoid **);

#ifndef GL_ARB_fragment_shader
#define GL_FRAGMENT_SHADER_ARB        0x8B30
#define GL_MAX_FRAGMENT_UNIFORM_COMPONENTS_ARB  0x8B49
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT_ARB  0x8B8B
#endif

#ifndef GL_ARB_shading_language_100
#define GL_SHADING_LANGUAGE_VERSION_ARB   0x8B8C
#endif

#ifndef GL_ARB_depth_clamp
#define GL_DEPTH_CLAMP            0x864F
#endif

#ifndef GL_ARB_vertex_buffer_object
#define GL_BUFFER_SIZE_ARB          0x8764
#define GL_BUFFER_USAGE_ARB         0x8765
#define GL_ARRAY_BUFFER_ARB         0x8892
#define GL_ELEMENT_ARRAY_BUFFER_ARB     0x8893
#define GL_ARRAY_BUFFER_BINDING_ARB     0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB 0x8895
#define GL_VERTEX_ARRAY_BUFFER_BINDING_ARB  0x8896
#define GL_NORMAL_ARRAY_BUFFER_BINDING_ARB  0x8897
#define GL_COLOR_ARRAY_BUFFER_BINDING_ARB 0x8898
#define GL_INDEX_ARRAY_BUFFER_BINDING_ARB 0x8899
#define GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING_ARB 0x889A
#define GL_EDGE_FLAG_ARRAY_BUFFER_BINDING_ARB 0x889B
#define GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING_ARB 0x889C
#define GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING_ARB  0x889D
#define GL_WEIGHT_ARRAY_BUFFER_BINDING_ARB  0x889E
#define GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING_ARB 0x889F
#define GL_READ_ONLY_ARB          0x88B8
#define GL_WRITE_ONLY_ARB         0x88B9
#define GL_READ_WRITE_ARB         0x88BA
#define GL_BUFFER_ACCESS_ARB        0x88BB
#define GL_BUFFER_MAPPED_ARB        0x88BC
#define GL_BUFFER_MAP_POINTER_ARB     0x88BD
#define GL_STREAM_DRAW_ARB          0x88E0
#define GL_STREAM_READ_ARB          0x88E1
#define GL_STREAM_COPY_ARB          0x88E2
#define GL_STATIC_DRAW_ARB          0x88E4
#define GL_STATIC_READ_ARB          0x88E5
#define GL_STATIC_COPY_ARB          0x88E6
#define GL_DYNAMIC_DRAW_ARB         0x88E8
#define GL_DYNAMIC_READ_ARB         0x88E9
#define GL_DYNAMIC_COPY_ARB         0x88EA

/* GL types for handling large vertex buffer objects */
typedef ptrdiff_t GLintptrARB;
typedef ptrdiff_t GLsizeiptrARB;
#endif

typedef void (APIENTRY*glBindBufferARB_t)(GLenum, GLuint);
typedef void (APIENTRY*glDeleteBuffersARB_t)(GLsizei, const GLuint *);
typedef void (APIENTRY*glGenBuffersARB_t)(GLsizei, GLuint *);
typedef GLboolean (APIENTRY*glIsBufferARB_t)(GLuint);
typedef void (APIENTRY*glBufferDataARB_t)(GLenum, GLsizeiptrARB, const GLvoid *, GLenum);
typedef void (APIENTRY*glBufferSubDataARB_t)(GLenum, GLintptrARB, GLsizeiptrARB, const GLvoid *);
typedef void (APIENTRY*glGetBufferSubDataARB_t)(GLenum, GLintptrARB, GLsizeiptrARB, GLvoid *);
typedef GLvoid *(APIENTRY*glMapBufferARB_t)(GLenum, GLenum);
typedef GLboolean (APIENTRY*glUnmapBufferARB_t)(GLenum);
typedef void (APIENTRY*glGetBufferParameterivARB_t)(GLenum, GLenum, GLint *);
typedef void (APIENTRY*glGetBufferPointervARB_t)(GLenum, GLenum, GLvoid **);

#ifndef GL_EXT_draw_range_elements
#define GL_MAX_ELEMENTS_VERTICES_EXT    0x80E8
#define GL_MAX_ELEMENTS_INDICES_EXT     0x80E9
#endif

#ifndef GL_DEPTH_BOUNDS_TEST_EXT
# define GL_DEPTH_BOUNDS_TEST_EXT  0x8890
#endif

#ifndef GL_DEPTH_BOUNDS_EXT
# define GL_DEPTH_BOUNDS_EXT  0x8891
#endif

typedef void (APIENTRY*glDrawRangeElementsEXT_t)(GLenum, GLuint, GLuint, GLsizei, GLenum, const GLvoid *);

#ifndef GL_FRAMEBUFFER
# define GL_FRAMEBUFFER  0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
# define GL_COLOR_ATTACHMENT0  0x8CE0
#endif
#ifndef GL_DEPTH_STENCIL_ATTACHMENT
# define GL_DEPTH_STENCIL_ATTACHMENT  0x821A
#endif

#ifndef GL_FRAMEBUFFER_COMPLETE
# define GL_FRAMEBUFFER_COMPLETE  0x8CD5
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT
# define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT  0x8CD6
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT
# define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT  0x8CD7
#endif
#ifndef GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS
# define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS  0x8CD9
#endif
#ifndef GL_FRAMEBUFFER_UNSUPPORTED
# define GL_FRAMEBUFFER_UNSUPPORTED  0x8CDD
#endif

#ifndef GL_COLOR_LOGIC_OP
# define GL_COLOR_LOGIC_OP  0x0BF2
#endif
#ifndef GL_CLEAR
# define GL_CLEAR  0x1500
#endif
#ifndef GL_COPY
# define GL_COPY  0x1503
#endif
#ifndef GL_XOR
# define GL_XOR  0x1506
#endif

#ifndef GL_FRAMEBUFFER_BINDING
# define GL_FRAMEBUFFER_BINDING  0x8CA6
#endif

#ifndef GL_DEPTH_STENCIL
# define GL_DEPTH_STENCIL  0x84F9
#endif

#ifndef GL_UNSIGNED_INT_24_8
# define GL_UNSIGNED_INT_24_8  0x84FA
#endif


typedef void (APIENTRY *glFramebufferTexture2DFn) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (APIENTRY *glDeleteFramebuffersFn) (GLsizei n, const GLuint *framebuffers);
typedef void (APIENTRY *glGenFramebuffersFn) (GLsizei n, GLuint *framebuffers);
typedef GLenum (APIENTRY *glCheckFramebufferStatusFn) (GLenum target);
typedef void (APIENTRY *glBindFramebufferFn) (GLenum target, GLuint framebuffer);

typedef void (APIENTRY *glClipControl_t) (GLenum origin, GLenum depth);
typedef void (APIENTRY *glBlitFramebuffer_t) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);

typedef void (APIENTRY *glGetProgramiv_t) (GLuint program, GLenum pname, GLint *params);
typedef void (APIENTRY *glGetPointerv_t) (GLenum pname,  GLvoid **params);
//typedef void (APIENTRY *glGetActiveUniform_t) (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, char *name);
//typedef void (APIENTRY *glGetActiveAttrib_t) (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, char *name);

typedef void (APIENTRY *glBlendFuncSeparate_t) (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);


// ////////////////////////////////////////////////////////////////////////// //
extern VCvarF gl_alpha_threshold;
extern VCvarB gl_sort_textures;
extern VCvarI r_ambient_min;
extern VCvarB r_allow_ambient;
extern VCvarB r_decals_enabled;
extern VCvarB r_decals_wall_masked;
extern VCvarB r_decals_wall_alpha;
extern VCvarB gl_decal_debug_nostencil;
extern VCvarB gl_decal_debug_noalpha;
extern VCvarB gl_decal_dump_max;
extern VCvarB gl_decal_reset_max;
extern VCvarB gl_uusort_textures;
extern VCvarB gl_dbg_adv_render_surface_textures;
extern VCvarB gl_dbg_adv_render_surface_fog;
extern VCvarB gl_dbg_adv_render_offset_shadow_volume;
extern VCvarB gl_dbg_adv_render_never_offset_shadow_volume;
extern VCvarB gl_dbg_render_stack_portal_bounds;
extern VCvarB gl_use_stencil_quad_clear;
extern VCvarI gl_dbg_use_zpass;
extern VCvarB gl_dbg_wireframe;
extern VCvarF gl_maxdist;
extern VCvarB r_brightmaps;
extern VCvarB r_brightmaps_sprite;
extern VCvarB r_brightmaps_additive;
extern VCvarB r_brightmaps_filter;
extern VCvarB r_glow_flat;
extern VCvarB gl_lmap_allow_partial_updates;


// ////////////////////////////////////////////////////////////////////////// //
class VOpenGLDrawer : public VDrawer {
public:
  class VGLShader {
  public:
    VGLShader *next;
    VOpenGLDrawer *owner;
    const char *progname;
    const char *vssrcfile;
    const char *fssrcfile;
    // compiled vertex program
    GLhandleARB prog;
    TArray<VStr> defines;

    typedef float glsl_float2[2];
    typedef float glsl_float3[3];
    typedef float glsl_float4[4];
    typedef float glsl_float9[9];

  public:
    VGLShader() : next(nullptr), owner(nullptr), progname(nullptr), vssrcfile(nullptr), fssrcfile(nullptr), prog(-1) {}

    void MainSetup (VOpenGLDrawer *aowner, const char *aprogname, const char *avssrcfile, const char *afssrcfile);

    virtual void Compile ();
    virtual void Unload ();
    virtual void Setup (VOpenGLDrawer *aowner) = 0;
    virtual void LoadUniforms () = 0;
    virtual void UnloadUniforms () = 0;
    virtual void UploadChangedUniforms (bool forced=false) = 0;
    //virtual void UploadChangedAttrs () = 0;

    void Activate ();
    void Deactivate ();

    static inline bool notEqual_float (const float &v1, const float &v2) { return (FASI(v1) != FASI(v2)); }
    static inline bool notEqual_bool (const bool v1, const bool v2) { return (v1 != v2); }
    static inline bool notEqual_vec2 (const float *v1, const float *v2) { return (memcmp(v1, v2, sizeof(float)*2) != 0); }
    static inline bool notEqual_vec3 (const TVec &v1, const TVec &v2) { return (memcmp(&v1.x, &v2.x, sizeof(float)*3) != 0); }
    static inline bool notEqual_vec4 (const float *v1, const float *v2) { return (memcmp(v1, v2, sizeof(float)*4) != 0); }
    static inline bool notEqual_mat3 (const float *v1, const float *v2) { return (memcmp(v1, v2, sizeof(float)*9) != 0); }
    static inline bool notEqual_mat4 (const VMatrix4 &v1, const VMatrix4 &v2) { return (memcmp(&v1.m[0][0], &v2.m[0][0], sizeof(float)*16) != 0); }
    static inline bool notEqual_sampler2D (const vuint32 v1, const vuint32 v2) { return (v1 != v2); }

    static inline void copyValue_float (float &dest, const float &src) { dest = src; }
    static inline void copyValue_bool (bool &dest, const bool &src) { dest = src; }
    static inline void copyValue_vec3 (TVec &dest, const TVec &src) { dest = src; }
    static inline void copyValue_mat4 (VMatrix4 &dest, const VMatrix4 &src) { memcpy(&dest.m[0][0], &src.m[0][0], sizeof(float)*16); }
    static inline void copyValue_vec4 (float *dest, const float *src) { memcpy(dest, src, sizeof(float)*4); }
    static inline void copyValue_vec2 (float *dest, const float *src) { memcpy(dest, src, sizeof(float)*2); }
    static inline void copyValue_mat3 (float *dest, const float *src) { memcpy(dest, src, sizeof(float)*9); }
    static inline void copyValue_sampler2D (vuint32 &dest, const vuint32 &src) { dest = src; }
  };

  friend class VGLShader;

  class FBO {
    //friend class VOpenGLDrawer;
  private:
    class VOpenGLDrawer *mOwner = nullptr;
    GLuint mFBO = 0;
    GLuint mColorTid = 0;
    GLuint mDepthStencilTid = 0;
    bool mHasDepthStencil = false;
    int mWidth = 0;
    int mHeight = 0;

  public:
    FBO ();
    ~FBO ();

    FBO (const FBO &) = delete;
    FBO &operator = (const FBO &) = delete;

    inline bool isValid () const noexcept { return (mOwner != nullptr); }
    inline int getWidth () const noexcept { return mWidth; }
    inline int getHeight () const noexcept { return mHeight; }

    inline GLuint getColorTid () const noexcept { return mColorTid; }
    inline GLuint getFBOid () const noexcept { return mFBO; }

    void create (VOpenGLDrawer *aowner, int awidth, int aheight, bool createDepthStencil=false);
    void destroy ();

    void activate ();
    void deactivate ();

    // this blits only color info
    // restore active FBO manually after calling this
    // it also can reset shader program
    // if `dest` is nullptr, blit to screen buffer (not yet)
    void blitTo (FBO *dest, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLenum filter);

    void blitToScreen ();
  };

  friend class FBO;

  VGLShader *currentActiveShader;
  FBO *currentActiveFBO;

  void ReactivateCurrentFBO ();

#include "gl_shaddef.hi"

private:
  bool usingZPass; // if we are rendering shadow volumes, should we do "z-pass"?
  TVec coneDir; // current spotlight direction
  float coneAngle; // current spotlight cone angle
  bool spotLight; // is current light a spotlight?
  GLint savedDepthMask; // used in various begin/end methods
  // for `DrawTexturedPoly()` API
  VTexture *texturedPolyLastTex;
  float texturedPolyLastAlpha;
  TVec texturedPolyLastLight;
  // list of surfaces with masked textures, for z-prefill
  TArray<surface_t *> zfillMasked;

protected:
  VGLShader *shaderHead;

  void registerShader (VGLShader *shader);
  void CompileShaders ();
  void DestroyShaders ();

public:
  // scissor array indicies
  enum {
    SCS_MINX,
    SCS_MINY,
    SCS_MAXX,
    SCS_MAXY,
  };

public:
  // VDrawer interface
  VOpenGLDrawer ();
  virtual ~VOpenGLDrawer () override;
  virtual void InitResolution () override;
  virtual void DeinitResolution () override;
  virtual void StartUpdate () override;
  virtual void ClearScreen (unsigned clearFlags=CLEAR_COLOR) override;
  virtual void Setup2D () override;
  virtual void *ReadScreen (int *, bool *) override;
  virtual void ReadBackScreen (int, int, rgba_t *) override;
  void ReadFBOPixels (FBO *srcfbo, int Width, int Height, rgba_t *Dest);

  void FinishUpdate ();

  // rendering stuff
  virtual bool UseFrustumFarClip () override;
  virtual void SetupView (VRenderLevelDrawer *, const refdef_t *) override;
  virtual void SetupViewOrg () override;
  virtual void EndView (bool ignoreColorTint=false) override;

  // texture stuff
  virtual void PrecacheTexture (VTexture *) override;

  // polygon drawing
  virtual void WorldDrawing () override;
  virtual void DrawWorldAmbientPass () override;

  virtual void BeginShadowVolumesPass () override;
  virtual void BeginLightShadowVolumes (const TVec &LightPos, const float Radius, bool useZPass, bool hasScissor, const int scoords[4], const TVec &aconeDir, const float aconeAngle) override;
  virtual void EndLightShadowVolumes () override;
  virtual void RenderSurfaceShadowVolume (const surface_t *surf, const TVec &LightPos, float Radius) override;
  void RenderSurfaceShadowVolumeZPassIntr (const surface_t *surf, const TVec &LightPos, float Radius);

  virtual void BeginLightPass (const TVec &LightPos, float Radius, float LightMin, vuint32 Color, bool doShadow) override;
  virtual void DrawSurfaceLight (surface_t *surf) override;

  virtual void DrawWorldTexturesPass () override;
  virtual void DrawWorldFogPass () override;
  virtual void EndFogPass () override;

  virtual void DrawSkyPolygon (surface_t *, bool, VTexture *, float, VTexture *, float, int) override;
  virtual void DrawMaskedPolygon (surface_t *surf, float Alpha, bool Additive) override;

  virtual void BeginTranslucentPolygonDecals () override;
  virtual void DrawTranslucentPolygonDecals (surface_t *surf, float Alpha, bool Additive) override;

  virtual void DrawSpritePolygon (const TVec *cv, VTexture *Tex,
                                  float Alpha, bool Additive,
                                  VTextureTranslation *Translation, int CMap,
                                  vuint32 light, vuint32 Fade,
                                  const TVec &sprnormal, float sprpdist,
                                  const TVec &saxis, const TVec &taxis, const TVec &texorg,
                                  int hangup) override;
  virtual void DrawAliasModel(const TVec&, const TAVec&, const TVec&, const TVec&,
    VMeshModel*, int, int, VTexture*, VTextureTranslation*, int, vuint32,
    vuint32, float, bool, bool, float, bool, bool, bool, bool) override;
  virtual void DrawAliasModelAmbient(const TVec&, const TAVec&, const TVec&,
    const TVec&, VMeshModel*, int, int, VTexture*, vuint32, float, float, bool,
    bool, bool) override;
  virtual void DrawAliasModelTextures(const TVec&, const TAVec&, const TVec&, const TVec&,
    VMeshModel*, int, int, VTexture*, VTextureTranslation*, int, float, float, bool,
    bool, bool) override;
  virtual void BeginModelsLightPass(const TVec&, float, float, vuint32, const TVec &aconeDir, const float aconeAngle) override;
  virtual void DrawAliasModelLight(const TVec&, const TAVec&, const TVec&,
    const TVec&, VMeshModel*, int, int, VTexture*, float, float, bool, bool) override;
  virtual void BeginModelsShadowsPass(TVec&, float) override;
  virtual void DrawAliasModelShadow(const TVec&, const TAVec&, const TVec&,
    const TVec&, VMeshModel*, int, int, float, bool, const TVec&, float) override;
  virtual void DrawAliasModelFog(const TVec&, const TAVec&, const TVec&,
    const TVec&, VMeshModel*, int, int, VTexture*, vuint32, float, float, bool, bool) override;
  virtual bool StartPortal(VPortal*, bool) override;
  virtual void EndPortal(VPortal*, bool) override;

  // particles
  virtual void StartParticles () override;
  virtual void DrawParticle (particle_t *) override;
  virtual void EndParticles () override;

  // drawing
  virtual void DrawPic (float x1, float y1, float x2, float y2,
                        float s1, float t1, float s2, float t2,
                        VTexture *Tex, VTextureTranslation *Trans, float Alpha) override;
  virtual void DrawPicShadow (float x1, float y1, float x2, float y2,
                              float s1, float t1, float s2, float t2,
                              VTexture *Tex, float shade) override;
  virtual void FillRectWithFlat (float x1, float y1, float x2, float y2,
                                 float s1, float t1, float s2, float t2, VTexture *Tex) override;
  virtual void FillRectWithFlatRepeat (float x1, float y1, float x2, float y2,
                                       float s1, float t1, float s2, float t2, VTexture *Tex) override;
  virtual void FillRect (float x1, float y1, float x2, float y2, vuint32 color, float alpha=1.0f) override;
  virtual void DrawRect (float x1, float y1, float x2, float y2, vuint32 color, float alpha=1.0f) override;
  virtual void ShadeRect (float x1, float y1, float x2, float y2, float darkening) override;
  virtual void DrawLine (float x1, float y1, float x2, float y2, vuint32 color, float alpha=1.0f) override;
  virtual void DrawConsoleBackground (int h) override;
  virtual void DrawSpriteLump (float x1, float y1, float x2, float y2,
                               VTexture *Tex, VTextureTranslation *Translation, bool flip) override;

  virtual void BeginTexturedPolys () override;
  virtual void EndTexturedPolys () override;
  virtual void DrawTexturedPoly (const texinfo_t *tinfo, TVec light, float alpha, int vcount, const TVec *verts, const TVec *origverts=nullptr) override;

  // automap
  virtual void StartAutomap (bool asOverlay) override;
  virtual void DrawLineAM (float x1, float y1, vuint32 c1, float x2, float y2, vuint32 c2) override;
  virtual void EndAutomap () override;

  // advanced drawing.
  virtual bool SupportsShadowVolumeRendering () override;

  virtual void GetProjectionMatrix (VMatrix4 &mat) override;
  virtual void GetModelMatrix (VMatrix4 &mat) override;

  // call this before doing light scissor calculations (can be called once per scene)
  // sets `vpmats`
  // scissor setup will use those matrices (but won't modify them)
  virtual void LoadVPMatrices () override;
  // returns 0 if scissor has no sense; -1 if scissor is empty, and 1 if scissor is set
  virtual int SetupLightScissor (const TVec &org, float radius, int scoord[4], const TVec *geobbox=nullptr) override;
  virtual void ResetScissor () override;

  static inline float getAlphaThreshold () { return clampval(gl_alpha_threshold.asFloat(), 0.0f, 1.0f); }

  //virtual void GetRealWindowSize (int *rw, int *rh) override;

  virtual void DebugRenderScreenRect (int x0, int y0, int x1, int y1, vuint32 color) override;

private:
  vuint8 decalStcVal;
  bool decalUsedStencil;
  bool stencilBufferDirty;

  // last used shadow volume scissor
  // if new scissor is inside this one, and stencil buffer is not changed,
  // do not clear stencil buffer
  // reset in `BeginShadowVolumesPass()`
  // modified in `BeginLightShadowVolumes()`
  GLint lastSVScissor[4];
  // set in `BeginShadowVolumesPass()`
  GLint lastSVVport[4];

  // modified in `SetupLightScissor()`/`ResetScissor()`
  GLint currentSVScissor[4];

  // reset in `BeginLightShadowVolumes()`
  // set in `RenderSurfaceShadowVolume()`
  bool wasRenderedShadowSurface;


  enum DecalType { DT_SIMPLE, DT_LIGHTMAP, DT_ADVANCED };

  // this is required for decals
  inline void NoteStencilBufferDirty () { stencilBufferDirty = true; }
  inline bool IsStencilBufferDirty () const { return stencilBufferDirty; }
  inline void ClearStencilBuffer () { if (stencilBufferDirty) glClear(GL_STENCIL_BUFFER_BIT); stencilBufferDirty = false; decalUsedStencil = false; }

  void RenderPrepareShaderDecals (surface_t *surf);
  bool RenderFinishShaderDecals (DecalType dtype, surface_t *surf, surfcache_t *cache, int cmap);

  // regular renderer building parts
  // returns `true` if we need to re-setup texture
  bool RenderSimpleSurface (bool textureChanged, surface_t *surf);
  bool RenderLMapSurface (bool textureChanged, surface_t *surf, surfcache_t *cache);

  void RestoreDepthFunc ();

  //WARNING! take care of setting heights to non-zero, or glow shaders will fail!
  struct GlowParams {
    vuint32 glowCC, glowCF; // glow colors
    float floorZ, ceilingZ;
    float floorGlowHeight, ceilingGlowHeight;
    GlowParams () : glowCC(0), glowCF(0), floorZ(0), ceilingZ(0), floorGlowHeight(128), ceilingGlowHeight(128) {}
    inline bool isActive () const { return !!(glowCC|glowCF); }
    inline void clear () { glowCC = glowCF = 0; floorGlowHeight = ceilingGlowHeight = 128; }
  };

#define VV_GLDRAWER_ACTIVATE_GLOW(shad_,gp_)  do { \
  shad_.SetGlowColorFloor(((gp_.glowCF>>16)&0xff)/255.0f, ((gp_.glowCF>>8)&0xff)/255.0f, (gp_.glowCF&0xff)/255.0f, ((gp_.glowCF>>24)&0xff)/255.0f); \
  shad_.SetGlowColorCeiling(((gp_.glowCC>>16)&0xff)/255.0f, ((gp_.glowCC>>8)&0xff)/255.0f, (gp_.glowCC&0xff)/255.0f, ((gp_.glowCC>>24)&0xff)/255.0f); \
  shad_.SetFloorZ(gp_.floorZ); \
  shad_.SetCeilingZ(gp_.ceilingZ); \
  shad_.SetFloorGlowHeight(gp_.floorGlowHeight); \
  shad_.SetCeilingGlowHeight(gp_.ceilingGlowHeight); \
} while (0)

#define VV_GLDRAWER_DEACTIVATE_GLOW(shad_)  do { \
  shad_.SetGlowColorFloor(0.0f, 0.0f, 0.0f, 0.0f); \
  shad_.SetGlowColorCeiling(0.0f, 0.0f, 0.0f, 0.0f); \
  shad_.SetFloorGlowHeight(128); \
  shad_.SetCeilingGlowHeight(128); \
} while (0)

  inline void CalcGlow (GlowParams &gp, const surface_t *surf) const {
    gp.clear();
    if (!surf->seg || !surf->subsector) return;
    bool checkFloorFlat, checkCeilingFlat;
    const sector_t *sec = surf->subsector->sector;
    // check for glowing sector floor
    if (surf->glowFloorHeight > 0 && surf->glowFloorColor) {
      gp.floorGlowHeight = surf->glowFloorHeight;
      gp.glowCF = surf->glowFloorColor;
      gp.floorZ = sec->floor.GetPointZClamped(*surf->seg->v1);
      checkFloorFlat = false;
    } else {
      checkFloorFlat = true;
    }
    // check for glowing sector ceiling
    if (surf->glowCeilingHeight > 0 && surf->glowCeilingColor) {
      gp.ceilingGlowHeight = surf->glowCeilingHeight;
      gp.glowCC = surf->glowCeilingColor;
      gp.ceilingZ = sec->ceiling.GetPointZClamped(*surf->seg->v1);
      checkCeilingFlat = false;
    } else {
      checkCeilingFlat = true;
    }
    if ((checkFloorFlat || checkCeilingFlat) && r_glow_flat) {
      // check for glowing textures
      //FIXME: check actual view height here
      if (sec /*&& !sec->heightsec*/) {
        if (checkFloorFlat && sec->floor.pic) {
          VTexture *gtex = GTextureManager(sec->floor.pic);
          if (gtex && gtex->Type != TEXTYPE_Null && gtex->glowing) {
            gp.floorGlowHeight = 128;
            gp.glowCF = gtex->glowing;
            gp.floorZ = sec->floor.GetPointZClamped(*surf->seg->v1);
          }
        }
        if (checkCeilingFlat && sec->ceiling.pic) {
          VTexture *gtex = GTextureManager(sec->ceiling.pic);
          if (gtex && gtex->Type != TEXTYPE_Null && gtex->glowing) {
            gp.ceilingGlowHeight = 128;
            gp.glowCC = gtex->glowing;
            gp.ceilingZ = sec->ceiling.GetPointZClamped(*surf->seg->v1);
          }
        }
      }
    }
  }

public:
  GLint glGetUniLoc (const char *prog, GLhandleARB pid, const char *name, bool optional=false);
  GLint glGetAttrLoc (const char *prog, GLhandleARB pid, const char *name, bool optional=false);

private:
  vuint8 *readBackTempBuf;
  int readBackTempBufSize;

public:
  struct SurfListItem {
    surface_t *surf;
    surfcache_t *cache;
  };

private:
  TArray<SurfListItem> surfList;

  inline void surfListClear () { surfList.reset(); }

  inline void surfListAppend (surface_t *surf, surfcache_t *cache=nullptr) {
    SurfListItem &si = surfList.alloc();
    si.surf = surf;
    si.cache = cache;
  }

private:
  static inline float getSurfLightLevel (const surface_t *surf) {
    if (r_glow_flat && surf && !surf->seg && surf->subsector) {
      const sector_t *sec = surf->subsector->sector;
      //FIXME: check actual view height here
      if (sec && !sec->heightsec) {
        if (sec->floor.pic && surf->GetNormalZ() > 0.0f) {
          VTexture *gtex = GTextureManager(sec->floor.pic);
          if (gtex && gtex->Type != TEXTYPE_Null && gtex->glowing) return 1.0f;
        }
        if (sec->ceiling.pic && surf->GetNormalZ() < 0.0f) {
          VTexture *gtex = GTextureManager(sec->ceiling.pic);
          if (gtex && gtex->Type != TEXTYPE_Null && gtex->glowing) return 1.0f;
        }
      }
    }
    if (!surf) return 0;
    int slins = (r_allow_ambient ? (surf->Light>>24)&0xff : clampToByte(r_ambient_min));
    if (slins < r_ambient_min) slins = clampToByte(r_ambient_min);
    return float(slins)/255.0f;
  }

  static inline void glVertex (const TVec &v) { glVertex3f(v.x, v.y, v.z); }
  static inline void glVertex4 (const TVec &v, const float w) { glVertex4f(v.x, v.y, v.z, w); }

  struct CameraFBOInfo {
  public:
    FBO fbo;
    int texnum; // camera texture number for this FBO
    int camwidth, camheight; // camera texture dimensions for this FBO
    int index; // internal index of this FBO

  public:
    CameraFBOInfo ();
    ~CameraFBOInfo ();
    CameraFBOInfo (const CameraFBOInfo &) = delete;
    CameraFBOInfo& operator = (const CameraFBOInfo &) = delete;
  };

protected:
  vuint8 *tmpImgBuf0;
  vuint8 *tmpImgBuf1;
  int tmpImgBufSize;

  float tex_iw, tex_ih;
  int tex_w, tex_h;

  int lastgamma;
  int CurrentFade;

  bool hasNPOT;
  bool hasBoundsTest; // GL_EXT_depth_bounds_test

  FBO mainFBO;
  FBO ambLightFBO; // we'll copy ambient light texture here, so we can use it in decal renderer to light decals
  // view (texture) camera updates will use this to render camera views
  // as reading from rendered texture is very slow, we will flip-flop FBOs,
  // using `current` to render new camera views, and `current^1` to get previous ones
  TArray<CameraFBOInfo *> cameraFBOList;
  // current "main" fbo: <0: `mainFBO`, otherwise camera FBO
  int currMainFBO;

  GLint maxTexSize;

  GLuint lmap_id[NUM_BLOCK_SURFS];
  GLuint addmap_id[NUM_BLOCK_SURFS];
  bool atlasesGenerated;
  vuint8 atlases_updated[NUM_BLOCK_SURFS];

  GLenum ClampToEdge;
  GLfloat max_anisotropy; // 1.0: off
  bool anisotropyExists;

  bool usingFPZBuffer;

  bool HaveDepthClamp;
  bool HaveStencilWrap;

  int MaxTextureUnits;

  TArray<GLhandleARB> CreatedShaderObjects;
  TArray<VMeshModel *> UploadedModels;

  // console variables
  static VCvarI texture_filter;
  static VCvarI sprite_filter;
  static VCvarI model_filter;
  static VCvarI gl_texture_filter_anisotropic;
  static VCvarB clear;
  static VCvarB blend_sprites;
  static VCvarB ext_anisotropy;
  static VCvarI multisampling_sample;
  static VCvarB gl_smooth_particles;
  static VCvarB gl_dump_vendor;
  static VCvarB gl_dump_extensions;

  // extensions
  bool CheckExtension (const char *ext);
  virtual void *GetExtFuncPtr (const char *name) = 0;

  void SetFade (vuint32 NewFade);

  // returns 0 if generation is disabled, and atlas is not created
  void GenerateLightmapAtlasTextures ();
  void DeleteLightmapAtlases ();

  virtual void FlushOneTexture (VTexture *tex, bool forced=false) override; // unload one texture
  virtual void FlushTextures (bool forced=false) override; // unload all textures
  void DeleteTextures ();
  void FlushTexture (VTexture *);
  void DeleteTexture (VTexture *);
  void SetTexture (VTexture *Tex, int CMap);
  void SetDecalTexture (VTexture *Tex, VTextureTranslation *Translation, int CMap);
  void SetBrightmapTexture (VTexture *);
  void SetSpriteLump (VTexture *Tex, VTextureTranslation *Translation, int CMap, bool asPicture);
  void SetPic (VTexture *Tex, VTextureTranslation *Trans, int CMap);
  void SetPicModel (VTexture *Tex, VTextureTranslation *Trans, int CMap);
  void GenerateTexture (VTexture *Tex, GLuint *pHandle, VTextureTranslation *Translation, int CMap, bool asPicture, bool needUpdate);
  void UploadTexture8 (int Width, int Height, const vuint8 *Data, const rgba_t *Pal);
  void UploadTexture8A (int Width, int Height, const pala_t *Data, const rgba_t *Pal);
  void UploadTexture (int width, int height, const rgba_t *data, bool doFringeRemove);

  void DoHorizonPolygon (surface_t *surf);
  void DrawPortalArea (VPortal *Portal);

  GLhandleARB LoadShader (const char *progname, GLenum Type, VStr FileName, const TArray<VStr> &defines=TArray<VStr>());
  GLhandleARB CreateProgram (const char *progname, GLhandleARB VertexShader, GLhandleARB FragmentShader);

  void UploadModel (VMeshModel *Mdl);
  void UnloadModels ();

  void SetupTextureFiltering (int level); // level is taken from the appropriate cvar

public:
  virtual void SetMainFBO () override;

  virtual void ClearCameraFBOs () override;
  virtual int GetCameraFBO (int texnum, int width, int height) override; // returns index or -1; (re)creates FBO if necessary
  virtual int FindCameraFBO (int texnum) override; // returns index or -1
  virtual void SetCameraFBO (int cfboindex) override;
  virtual GLuint GetCameraFBOTextureId (int cfboindex) override; // returns 0 if cfboindex is invalid

  void DestroyCameraFBOList ();

  void ActivateMainFBO ();
  FBO *GetMainFBO ();

public:
#define VGLAPIPTR(x)  x##_t p_##x
  //VGLAPIPTR(glMultiTexCoord2fARB);
  VGLAPIPTR(glActiveTextureARB);

  VGLAPIPTR(glPointParameterfEXT);
  VGLAPIPTR(glPointParameterfvEXT);

  VGLAPIPTR(glStencilFuncSeparate);
  VGLAPIPTR(glStencilOpSeparate);

  VGLAPIPTR(glDeleteObjectARB);
  VGLAPIPTR(glGetHandleARB);
  VGLAPIPTR(glDetachObjectARB);
  VGLAPIPTR(glCreateShaderObjectARB);
  VGLAPIPTR(glShaderSourceARB);
  VGLAPIPTR(glCompileShaderARB);
  VGLAPIPTR(glCreateProgramObjectARB);
  VGLAPIPTR(glAttachObjectARB);
  VGLAPIPTR(glLinkProgramARB);
  VGLAPIPTR(glUseProgramObjectARB);
  VGLAPIPTR(glValidateProgramARB);
  VGLAPIPTR(glUniform1fARB);
  VGLAPIPTR(glUniform2fARB);
  VGLAPIPTR(glUniform3fARB);
  VGLAPIPTR(glUniform4fARB);
  VGLAPIPTR(glUniform1iARB);
  VGLAPIPTR(glUniform2iARB);
  VGLAPIPTR(glUniform3iARB);
  VGLAPIPTR(glUniform4iARB);
  VGLAPIPTR(glUniform1fvARB);
  VGLAPIPTR(glUniform2fvARB);
  VGLAPIPTR(glUniform3fvARB);
  VGLAPIPTR(glUniform4fvARB);
  VGLAPIPTR(glUniform1ivARB);
  VGLAPIPTR(glUniform2ivARB);
  VGLAPIPTR(glUniform3ivARB);
  VGLAPIPTR(glUniform4ivARB);
  VGLAPIPTR(glUniformMatrix2fvARB);
  VGLAPIPTR(glUniformMatrix3fvARB);
  VGLAPIPTR(glUniformMatrix4fvARB);
  VGLAPIPTR(glGetObjectParameterfvARB);
  VGLAPIPTR(glGetObjectParameterivARB);
  VGLAPIPTR(glGetInfoLogARB);
  VGLAPIPTR(glGetAttachedObjectsARB);
  VGLAPIPTR(glGetUniformLocationARB);
  VGLAPIPTR(glGetActiveUniformARB);
  VGLAPIPTR(glGetUniformfvARB);
  VGLAPIPTR(glGetUniformivARB);
  VGLAPIPTR(glGetShaderSourceARB);

  VGLAPIPTR(glVertexAttrib1dARB);
  VGLAPIPTR(glVertexAttrib1dvARB);
  VGLAPIPTR(glVertexAttrib1fARB);
  VGLAPIPTR(glVertexAttrib1fvARB);
  VGLAPIPTR(glVertexAttrib1sARB);
  VGLAPIPTR(glVertexAttrib1svARB);
  VGLAPIPTR(glVertexAttrib2dARB);
  VGLAPIPTR(glVertexAttrib2dvARB);
  VGLAPIPTR(glVertexAttrib2fARB);
  VGLAPIPTR(glVertexAttrib2fvARB);
  VGLAPIPTR(glVertexAttrib2sARB);
  VGLAPIPTR(glVertexAttrib2svARB);
  VGLAPIPTR(glVertexAttrib3dARB);
  VGLAPIPTR(glVertexAttrib3dvARB);
  VGLAPIPTR(glVertexAttrib3fARB);
  VGLAPIPTR(glVertexAttrib3fvARB);
  VGLAPIPTR(glVertexAttrib3sARB);
  VGLAPIPTR(glVertexAttrib3svARB);
  VGLAPIPTR(glVertexAttrib4NbvARB);
  VGLAPIPTR(glVertexAttrib4NivARB);
  VGLAPIPTR(glVertexAttrib4NsvARB);
  VGLAPIPTR(glVertexAttrib4NubARB);
  VGLAPIPTR(glVertexAttrib4NubvARB);
  VGLAPIPTR(glVertexAttrib4NuivARB);
  VGLAPIPTR(glVertexAttrib4NusvARB);
  VGLAPIPTR(glVertexAttrib4bvARB);
  VGLAPIPTR(glVertexAttrib4dARB);
  VGLAPIPTR(glVertexAttrib4dvARB);
  VGLAPIPTR(glVertexAttrib4fARB);
  VGLAPIPTR(glVertexAttrib4fvARB);
  VGLAPIPTR(glVertexAttrib4ivARB);
  VGLAPIPTR(glVertexAttrib4sARB);
  VGLAPIPTR(glVertexAttrib4svARB);
  VGLAPIPTR(glVertexAttrib4ubvARB);
  VGLAPIPTR(glVertexAttrib4uivARB);
  VGLAPIPTR(glVertexAttrib4usvARB);
  VGLAPIPTR(glVertexAttribPointerARB);
  VGLAPIPTR(glEnableVertexAttribArrayARB);
  VGLAPIPTR(glDisableVertexAttribArrayARB);
  VGLAPIPTR(glBindAttribLocationARB);
  VGLAPIPTR(glGetActiveAttribARB);
  VGLAPIPTR(glGetAttribLocationARB);
  VGLAPIPTR(glGetVertexAttribdvARB);
  VGLAPIPTR(glGetVertexAttribfvARB);
  VGLAPIPTR(glGetVertexAttribivARB);
  VGLAPIPTR(glGetVertexAttribPointervARB);

  VGLAPIPTR(glBindBufferARB);
  VGLAPIPTR(glDeleteBuffersARB);
  VGLAPIPTR(glGenBuffersARB);
  VGLAPIPTR(glIsBufferARB);
  VGLAPIPTR(glBufferDataARB);
  VGLAPIPTR(glBufferSubDataARB);
  VGLAPIPTR(glGetBufferSubDataARB);
  VGLAPIPTR(glMapBufferARB);
  VGLAPIPTR(glUnmapBufferARB);
  VGLAPIPTR(glGetBufferParameterivARB);
  VGLAPIPTR(glGetBufferPointervARB);

  VGLAPIPTR(glDrawRangeElementsEXT);

  VGLAPIPTR(glClipControl);
  VGLAPIPTR(glDepthBoundsEXT);
  VGLAPIPTR(glBlitFramebuffer);

  VGLAPIPTR(glGetProgramiv);
  //VGLAPIPTR(glGetPointerv);

  VGLAPIPTR(glBlendFuncSeparate);

#undef VGLAPIPTR

  inline void SelectTexture (int level) { p_glActiveTextureARB(GLenum(GL_TEXTURE0_ARB+level)); }

  static inline void SetColor (vuint32 c) {
    glColor4ub((vuint8)((c>>16)&255), (vuint8)((c>>8)&255), (vuint8)(c&255), (vuint8)((c>>24)&255));
  }

  static const char *glTypeName (GLenum type) {
    switch (type) {
      case /*GL_BYTE*/ 0x1400: return "byte";
      case /*GL_UNSIGNED_BYTE*/ 0x1401: return "ubyte";
      case /*GL_SHORT*/ 0x1402: return "short";
      case /*GL_UNSIGNED_SHORT*/ 0x1403: return "ushort";
      case /*GL_INT*/ 0x1404: return "int";
      case /*GL_UNSIGNED_INT*/ 0x1405: return "uint";
      case /*GL_FLOAT*/ 0x1406: return "float";
      case /*GL_2_BYTES*/ 0x1407: return "byte2";
      case /*GL_3_BYTES*/ 0x1408: return "byte3";
      case /*GL_4_BYTES*/ 0x1409: return "byte4";
      case /*GL_DOUBLE*/ 0x140A: return "double";
      case /*GL_FLOAT_VEC2*/ 0x8B50: return "vec2";
      case /*GL_FLOAT_VEC3*/ 0x8B51: return "vec3";
      case /*GL_FLOAT_VEC4*/ 0x8B52: return "vec4";
      case /*GL_INT_VEC2*/ 0x8B53: return "ivec2";
      case /*GL_INT_VEC3*/ 0x8B54: return "ivec3";
      case /*GL_INT_VEC4*/ 0x8B55: return "ivec4";
      case /*GL_BOOL*/ 0x8B56: return "bool";
      case /*GL_BOOL_VEC2*/ 0x8B57: return "bvec2";
      case /*GL_BOOL_VEC3*/ 0x8B58: return "bvec3";
      case /*GL_BOOL_VEC4*/ 0x8B59: return "bvec4";
      case /*GL_FLOAT_MAT2*/ 0x8B5A: return "mat2";
      case /*GL_FLOAT_MAT3*/ 0x8B5B: return "mat3";
      case /*GL_FLOAT_MAT4*/ 0x8B5C: return "mat4";
      case /*GL_SAMPLER_1D*/ 0x8B5D: return "sampler1D";
      case /*GL_SAMPLER_2D*/ 0x8B5E: return "sampler2D";
      case /*GL_SAMPLER_3D*/ 0x8B5F: return "sampler3D";
      case /*GL_SAMPLER_CUBE*/ 0x8B60: return "samplerCube";
      case /*GL_SAMPLER_1D_SHADOW*/ 0x8B61: return "sampler1D_shadow";
      case /*GL_SAMPLER_2D_SHADOW*/ 0x8B62: return "sampler2D_shadow";
    }
    return "<unknown>";
  }

public:
  glFramebufferTexture2DFn glFramebufferTexture2D;
  glDeleteFramebuffersFn glDeleteFramebuffers;
  glGenFramebuffersFn glGenFramebuffers;
  glCheckFramebufferStatusFn glCheckFramebufferStatus;
  glBindFramebufferFn glBindFramebuffer;
};


#endif
