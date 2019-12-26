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
// ONLY included from "gl_local.h"
#ifndef APIENTRY
# define APIENTRY
#endif

#if defined(VV_GLDECLS)

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
typedef void (APIENTRY*glDepthBounds_t)(GLclampd zmin, GLclampd zmax);

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

typedef void (APIENTRY*glDrawRangeElements_t)(GLenum, GLuint, GLuint, GLsizei, GLenum, const GLvoid *);

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


typedef void (APIENTRY *glFramebufferTexture2D_t) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (APIENTRY *glDeleteFramebuffers_t) (GLsizei n, const GLuint *framebuffers);
typedef void (APIENTRY *glGenFramebuffers_t) (GLsizei n, GLuint *framebuffers);
typedef GLenum (APIENTRY *glCheckFramebufferStatus_t) (GLenum target);
typedef void (APIENTRY *glBindFramebuffer_t) (GLenum target, GLuint framebuffer);

typedef void (APIENTRY *glClipControl_t) (GLenum origin, GLenum depth);
typedef void (APIENTRY *glBlitFramebuffer_t) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);

typedef void (APIENTRY *glGetProgramiv_t) (GLuint program, GLenum pname, GLint *params);
typedef void (APIENTRY *glGetPointerv_t) (GLenum pname,  GLvoid **params);
//typedef void (APIENTRY *glGetActiveUniform_t) (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, char *name);
//typedef void (APIENTRY *glGetActiveAttrib_t) (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, char *name);

typedef void (APIENTRY *glBlendFuncSeparate_t) (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);

typedef void (APIENTRY *glDeleteRenderbuffers_t) (GLsizei n, const GLuint *renderbuffers);
typedef void (APIENTRY *glGenRenderbuffers_t) (GLsizei n, GLuint *renderbuffers);
typedef void (APIENTRY *glRenderbufferStorage_t) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRY *glBindRenderbuffer_t) (GLenum target, GLuint renderbuffer);

typedef void (APIENTRY *glFramebufferRenderbuffer_t) (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);

typedef void (APIENTRY *glGenerateMipmap_t) (GLenum target);

#elif defined(VV_GLIMPORTS)
// here, `VGLAPIPTR(name,required)` should be defined
# ifndef VGLAPIPTR
#  error "VGLAPIPTR is not defined!"
# endif
//#define VGLAPIPTR(x)  x##_t p_##x

  VGLAPIPTR(glActiveTextureARB, true);

  VGLAPIPTR(glStencilFuncSeparate, false);
  VGLAPIPTR(glStencilOpSeparate, false);

  VGLAPIPTR(glDeleteObjectARB, true);
  VGLAPIPTR(glGetHandleARB, true);
  VGLAPIPTR(glDetachObjectARB, true);
  VGLAPIPTR(glCreateShaderObjectARB, true);
  VGLAPIPTR(glShaderSourceARB, true);
  VGLAPIPTR(glCompileShaderARB, true);
  VGLAPIPTR(glCreateProgramObjectARB, true);
  VGLAPIPTR(glAttachObjectARB, true);
  VGLAPIPTR(glLinkProgramARB, true);
  VGLAPIPTR(glUseProgramObjectARB, true);
  VGLAPIPTR(glValidateProgramARB, true);
  VGLAPIPTR(glUniform1fARB, true);
  VGLAPIPTR(glUniform2fARB, true);
  VGLAPIPTR(glUniform3fARB, true);
  VGLAPIPTR(glUniform4fARB, true);
  VGLAPIPTR(glUniform1iARB, true);
  VGLAPIPTR(glUniform2iARB, true);
  VGLAPIPTR(glUniform3iARB, true);
  VGLAPIPTR(glUniform4iARB, true);
  VGLAPIPTR(glUniform1fvARB, true);
  VGLAPIPTR(glUniform2fvARB, true);
  VGLAPIPTR(glUniform3fvARB, true);
  VGLAPIPTR(glUniform4fvARB, true);
  VGLAPIPTR(glUniform1ivARB, true);
  VGLAPIPTR(glUniform2ivARB, true);
  VGLAPIPTR(glUniform3ivARB, true);
  VGLAPIPTR(glUniform4ivARB, true);
  VGLAPIPTR(glUniformMatrix2fvARB, true);
  VGLAPIPTR(glUniformMatrix3fvARB, true);
  VGLAPIPTR(glUniformMatrix4fvARB, true);
  VGLAPIPTR(glGetObjectParameterfvARB, true);
  VGLAPIPTR(glGetObjectParameterivARB, true);
  VGLAPIPTR(glGetInfoLogARB, true);
  VGLAPIPTR(glGetAttachedObjectsARB, true);
  VGLAPIPTR(glGetUniformLocationARB, true);
  VGLAPIPTR(glGetActiveUniformARB, true);
  VGLAPIPTR(glGetUniformfvARB, true);
  VGLAPIPTR(glGetUniformivARB, true);
  VGLAPIPTR(glGetShaderSourceARB, true);

  VGLAPIPTR(glVertexAttrib1dARB, true);
  VGLAPIPTR(glVertexAttrib1dvARB, true);
  VGLAPIPTR(glVertexAttrib1fARB, true);
  VGLAPIPTR(glVertexAttrib1fvARB, true);
  VGLAPIPTR(glVertexAttrib1sARB, true);
  VGLAPIPTR(glVertexAttrib1svARB, true);
  VGLAPIPTR(glVertexAttrib2dARB, true);
  VGLAPIPTR(glVertexAttrib2dvARB, true);
  VGLAPIPTR(glVertexAttrib2fARB, true);
  VGLAPIPTR(glVertexAttrib2fvARB, true);
  VGLAPIPTR(glVertexAttrib2sARB, true);
  VGLAPIPTR(glVertexAttrib2svARB, true);
  VGLAPIPTR(glVertexAttrib3dARB, true);
  VGLAPIPTR(glVertexAttrib3dvARB, true);
  VGLAPIPTR(glVertexAttrib3fARB, true);
  VGLAPIPTR(glVertexAttrib3fvARB, true);
  VGLAPIPTR(glVertexAttrib3sARB, true);
  VGLAPIPTR(glVertexAttrib3svARB, true);
  VGLAPIPTR(glVertexAttrib4NbvARB, true);
  VGLAPIPTR(glVertexAttrib4NivARB, true);
  VGLAPIPTR(glVertexAttrib4NsvARB, true);
  VGLAPIPTR(glVertexAttrib4NubARB, true);
  VGLAPIPTR(glVertexAttrib4NubvARB, true);
  VGLAPIPTR(glVertexAttrib4NuivARB, true);
  VGLAPIPTR(glVertexAttrib4NusvARB, true);
  VGLAPIPTR(glVertexAttrib4bvARB, true);
  VGLAPIPTR(glVertexAttrib4dARB, true);
  VGLAPIPTR(glVertexAttrib4dvARB, true);
  VGLAPIPTR(glVertexAttrib4fARB, true);
  VGLAPIPTR(glVertexAttrib4fvARB, true);
  VGLAPIPTR(glVertexAttrib4ivARB, true);
  VGLAPIPTR(glVertexAttrib4sARB, true);
  VGLAPIPTR(glVertexAttrib4svARB, true);
  VGLAPIPTR(glVertexAttrib4ubvARB, true);
  VGLAPIPTR(glVertexAttrib4uivARB, true);
  VGLAPIPTR(glVertexAttrib4usvARB, true);
  VGLAPIPTR(glVertexAttribPointerARB, true);
  VGLAPIPTR(glEnableVertexAttribArrayARB, true);
  VGLAPIPTR(glDisableVertexAttribArrayARB, true);
  VGLAPIPTR(glBindAttribLocationARB, true);
  VGLAPIPTR(glGetActiveAttribARB, true);
  VGLAPIPTR(glGetAttribLocationARB, true);
  VGLAPIPTR(glGetVertexAttribdvARB, true);
  VGLAPIPTR(glGetVertexAttribfvARB, true);
  VGLAPIPTR(glGetVertexAttribivARB, true);
  VGLAPIPTR(glGetVertexAttribPointervARB, true);

  VGLAPIPTR(glBindBufferARB, true);
  VGLAPIPTR(glDeleteBuffersARB, true);
  VGLAPIPTR(glGenBuffersARB, true);
  VGLAPIPTR(glIsBufferARB, true);
  VGLAPIPTR(glBufferDataARB, true);
  VGLAPIPTR(glBufferSubDataARB, true);
  VGLAPIPTR(glGetBufferSubDataARB, true);
  VGLAPIPTR(glMapBufferARB, true);
  VGLAPIPTR(glUnmapBufferARB, true);
  VGLAPIPTR(glGetBufferParameterivARB, true);
  VGLAPIPTR(glGetBufferPointervARB, true);

  //VGLAPIPTR(glDrawRangeElementsEXT, true);
  VGLAPIPTR(glDrawRangeElements, true);

  VGLAPIPTR(glClipControl, false);
  //VGLAPIPTR(glDepthBoundsEXT, false);
  VGLAPIPTR(glDepthBounds, false);
  VGLAPIPTR(glBlitFramebuffer, false);

  VGLAPIPTR(glGetProgramiv, true);
  //VGLAPIPTR(glGetPointerv, true);

  VGLAPIPTR(glBlendFuncSeparate, false);

  VGLAPIPTR(glDeleteRenderbuffers, true);
  VGLAPIPTR(glGenRenderbuffers, true);
  VGLAPIPTR(glRenderbufferStorage, true);
  VGLAPIPTR(glBindRenderbuffer, true);
  VGLAPIPTR(glFramebufferRenderbuffer, true);
  VGLAPIPTR(glGenerateMipmap, false);

  VGLAPIPTR(glFramebufferTexture2D, true);
  VGLAPIPTR(glDeleteFramebuffers, true);
  VGLAPIPTR(glGenFramebuffers, true);
  VGLAPIPTR(glCheckFramebufferStatus, true);
  VGLAPIPTR(glBindFramebuffer, true);

#else
# error "neither VV_GLDECLS, nor VV_GLIMPORTS were defined!"
#endif

