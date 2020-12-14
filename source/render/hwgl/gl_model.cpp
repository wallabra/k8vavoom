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
//**  Copyright (C) 2018-2020 Ketmar Dark
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
#include "gl_local.h"

//#define VV_MODEL_TEXTURES_DEBUG_NORMALS


static VCvarB gl_dbg_adv_render_textures_models("gl_dbg_adv_render_textures_models", true, "Render model textures in advanced renderer?", 0);
static VCvarB gl_dbg_adv_render_ambient_models("gl_dbg_adv_render_ambient_models", true, "Render model ambient light in advanced renderer?", 0);
static VCvarB gl_dbg_adv_render_light_models("gl_dbg_adv_render_light_models", true, "Render model dynamic light in advanced renderer?", 0);
static VCvarB gl_dbg_adv_render_alias_models("gl_dbg_adv_render_alias_models", true, "Render alias models?", 0);
static VCvarB gl_dbg_adv_render_shadow_models("gl_dbg_adv_render_shadow_models", true, "Render model shadow volumes?", 0);
static VCvarB gl_dbg_adv_render_fog_models("gl_dbg_adv_render_fog_models", true, "Render model fog?", 0);

extern VCvarB r_shadowmaps;
extern VCvarI gl_shadowmap_blur;

static unsigned int smapBShaderIndex;
static bool lpassDoShadowMap;

static VCvarB gl_gpu_debug_models("gl_gpu_debug_models", false, "Show model GPUDEBUG messages?", CVAR_PreInit);


//==========================================================================
//
//  dumpShaderLocs
//
//==========================================================================
static VVA_OKUNUSED void dumpShaderLocs (VOpenGLDrawer *drawer, GLuint prog) {
  char name[1024];
  GLint unicount;
  GLint size;
  GLenum type;
  drawer->p_glGetProgramiv(prog, GL_ACTIVE_UNIFORMS, &unicount);
  GCon->Logf("=== active uniforms: %d ===", unicount);
  for (int f = 0; f < unicount; ++f) {
    drawer->p_glGetActiveUniformARB(prog, (unsigned)f, sizeof(name), nullptr, &size, &type, name);
    GCon->Logf("  %d: <%s> (%d : %s)", f, name, size, drawer->glTypeName(type));
  }
  drawer->p_glGetProgramiv(prog, GL_ACTIVE_ATTRIBUTES, &unicount);
  GCon->Logf("=== active attributes: %d ===", unicount);
  for (int f = 0; f < unicount; ++f) {
    drawer->p_glGetActiveAttribARB(prog, (unsigned)f, sizeof(name), nullptr, &size, &type, name);
    GCon->Logf("  %d: <%s> (%d : %s)", f, name, size, drawer->glTypeName(type));
  }
}


//==========================================================================
//
//  AliasSetupTransform
//
//==========================================================================
static void AliasSetupTransform (const TVec &modelorg, const TAVec &angles,
                                 const AliasModelTrans &Transform,
                                 VMatrix4 &RotationMatrix)
{
  TVec alias_forward, alias_right, alias_up;
  AngleVectors(angles, alias_forward, alias_right, alias_up);

  // create rotation matrix
  VMatrix4 rotmat = VMatrix4::Identity;
  for (unsigned i = 0; i < 3; ++i) {
    rotmat[i][0] = alias_forward[i];
    rotmat[i][1] = -alias_right[i];
    rotmat[i][2] = alias_up[i];
  }

  // shift it
  rotmat[0][3] = modelorg.x+Transform.Shift.x;
  rotmat[1][3] = modelorg.y+Transform.Shift.y;
  rotmat[2][3] = modelorg.z+Transform.Shift.z;

  //RotationMatrix = rotmat*t3matrix;

  if (Transform.Scale.x != 1.0f || Transform.Scale.y != 1.0f || Transform.Scale.z != 1.0f) {
    // create scale+offset matrix
    VMatrix4 scalemat = VMatrix4::Identity;
    scalemat[0][0] = Transform.Scale.x;
    scalemat[1][1] = Transform.Scale.y;
    scalemat[2][2] = Transform.Scale.z;

    scalemat[0][3] = Transform.Scale.x*Transform.Offset.x;
    scalemat[1][3] = Transform.Scale.y*Transform.Offset.y;
    scalemat[2][3] = Transform.Scale.z*Transform.Offset.z;

    RotationMatrix = scalemat*rotmat;
  } else if (!Transform.Offset.isZero()) {
    // create offset matrix
    VMatrix4 scalemat = VMatrix4::Identity;
    scalemat[0][3] = Transform.Offset.x;
    scalemat[1][3] = Transform.Offset.y;
    scalemat[2][3] = Transform.Offset.z;

    RotationMatrix = scalemat*rotmat;
  } else {
    RotationMatrix = rotmat;
  }
}


//==========================================================================
//
//  AliasSetupNormalTransform
//
//==========================================================================
static void AliasSetupNormalTransform (const TAVec &angles, const TVec &Scale, VMatrix4 &RotationMatrix) {
  TVec alias_forward(0, 0, 0), alias_right(0, 0, 0), alias_up(0, 0, 0);
  AngleVectors(angles, alias_forward, alias_right, alias_up);

  #if 0
  VMatrix4 scalemat = VMatrix4::Identity;
  scalemat[0][0] = Scale.x;
  scalemat[1][1] = Scale.y;
  scalemat[2][2] = Scale.z;

  // create rotation matrix
  VMatrix4 rotmat = VMatrix4::Identity;
  for (unsigned i = 0; i < 3; ++i) {
    rotmat[i][0] = alias_forward[i];
    rotmat[i][1] = -alias_right[i];
    rotmat[i][2] = alias_up[i];
  }

  //k8: it seems that the order should be reversed
  //RotationMatrix = rotmat*scalemat;
  RotationMatrix = scalemat*rotmat;

  //k8: wtf?!
  if (fabsf(Scale.x) != fabsf(Scale.y) || fabsf(Scale.x) != fabsf(Scale.z)) {
    // non-uniform scale, do full inverse transpose
    RotationMatrix = RotationMatrix.Inverse().Transpose();
  }
  #else
  // create rotation matrix
  RotationMatrix = VMatrix4::Identity;
  for (unsigned i = 0; i < 3; ++i) {
    RotationMatrix[i][0] = alias_forward[i];
    RotationMatrix[i][1] = -alias_right[i];
    RotationMatrix[i][2] = alias_up[i];
  }
  #endif
}


//==========================================================================
//
//  VOpenGLDrawer::UploadModel
//
//==========================================================================
void VOpenGLDrawer::UploadModel (VMeshModel *Mdl) {
  if (Mdl->Uploaded) return;

  p_glDebugLogf("uploading model <%s:%d>", *Mdl->Name, Mdl->MeshIndex);

  // create buffer
  p_glGenBuffersARB(1, &Mdl->VertsBuffer);
  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mdl->VertsBuffer);
  p_glObjectLabelVA(GL_BUFFER, Mdl->VertsBuffer, "Model verts buffer <%s:%d>", *Mdl->Name, Mdl->MeshIndex);

  int Size = (int)sizeof(VMeshSTVert)*Mdl->STVerts.length()+(int)sizeof(TVec)*Mdl->STVerts.length()*2*Mdl->Frames.length();
  p_glBufferDataARB(GL_ARRAY_BUFFER_ARB, Size, nullptr, GL_STATIC_DRAW_ARB);

  // upload data
  // texture coords array
  p_glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(VMeshSTVert)*Mdl->STVerts.length(), &Mdl->STVerts[0]);
  // vertices array
  p_glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, sizeof(VMeshSTVert)*Mdl->STVerts.length(), sizeof(TVec)*Mdl->AllVerts.length(), &Mdl->AllVerts[0]);
  // normals array
  p_glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, sizeof(VMeshSTVert)*Mdl->STVerts.length()+sizeof(TVec)*Mdl->AllVerts.length(), sizeof(TVec)*Mdl->AllNormals.length(), &Mdl->AllNormals[0]);

  // pre-calculate offsets
  for (int i = 0; i < Mdl->Frames.length(); ++i) {
    Mdl->Frames[i].VertsOffset = sizeof(VMeshSTVert)*Mdl->STVerts.length()+i*sizeof(TVec)*Mdl->STVerts.length();
    Mdl->Frames[i].NormalsOffset = sizeof(VMeshSTVert)*Mdl->STVerts.length()+sizeof(TVec)*Mdl->AllVerts.length()+i*sizeof(TVec)*Mdl->STVerts.length();
  }

  // indexes
  p_glGenBuffersARB(1, &Mdl->IndexBuffer);
  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, Mdl->IndexBuffer);
  p_glObjectLabelVA(GL_BUFFER, Mdl->IndexBuffer, "Model idx buffer <%s:%d>", *Mdl->Name, Mdl->MeshIndex);

  // vertex indicies
  p_glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 6*Mdl->Tris.length(), &Mdl->Tris[0], GL_STATIC_DRAW_ARB);

  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
  Mdl->Uploaded = true;
  UploadedModels.Append(Mdl);

  p_glDebugLogf("done uploading model <%s:%d>", *Mdl->Name, Mdl->MeshIndex);
}


//==========================================================================
//
//  VOpenGLDrawer::UnloadModels
//
//==========================================================================
void VOpenGLDrawer::UnloadModels () {
  for (int i = 0; i < UploadedModels.length(); ++i) {
    p_glDeleteBuffersARB(1, &UploadedModels[i]->VertsBuffer);
    p_glDeleteBuffersARB(1, &UploadedModels[i]->IndexBuffer);
    UploadedModels[i]->Uploaded = false;
  }
  UploadedModels.Clear();
}


//==========================================================================
//
//  VOpenGLDrawer::DrawAliasModel
//
//  renders alias model for lightmapped renderer
//
//==========================================================================
void VOpenGLDrawer::DrawAliasModel (const TVec &origin, const TAVec &angles,
                                    const AliasModelTrans &Transform,
                                    VMeshModel *Mdl, int frame, int nextframe,
                                    VTexture *Skin, VTextureTranslation *Trans, int CMap,
                                    const RenderStyleInfo &ri,
                                    bool is_view_model, float Inter, bool Interpolate,
                                    bool ForceDepthUse, bool AllowTransparency, bool onlyDepth)
{
  if (!Skin || Skin->Type == TEXTYPE_Null) return; // do not render models without textures

  if (is_view_model) {
    // hack the depth range to prevent view model from poking into walls
    if (CanUseRevZ()) glDepthRange(0.7f, 1.0f); else glDepthRange(0.0f, 0.3f);
  }

  if (!gl_dbg_adv_render_alias_models) return;

  UploadModel(Mdl);

  SetPicModel(Skin, Trans, CMap, (ri.isShaded() ? ri.stencilColor : 0u));
  if (ri.isShaded()) AllowTransparency = true;

  //glEnable(GL_ALPHA_TEST);
  //glShadeModel(GL_SMOOTH);
  //glAlphaFunc(GL_GREATER, 0.0f);
  GLEnableBlend();

  VMatrix4 RotationMatrix;
  AliasSetupTransform(origin, angles, Transform, RotationMatrix);

  GLuint attrPosition;
  GLuint attrVert2;
  GLuint attrTexCoord;

  if (!ri.isStenciled()) {
    SurfModel.Activate();
    SurfModel.SetTexture(0);
    SurfModel.SetModelToWorldMat(RotationMatrix);
    SurfModel.SetFogFade(ri.fade, ri.alpha);
    SurfModel.SetInAlpha(ri.alpha < 1.0f ? ri.alpha : 1.0f);
    SurfModel.SetAllowTransparency(AllowTransparency);
    SurfModel.SetInter(Inter);
    SurfModel.UploadChangedUniforms();
    attrPosition = SurfModel.loc_Position;
    attrVert2 = SurfModel.loc_Vert2;
    attrTexCoord = SurfModel.loc_TexCoord;
  } else {
    SurfModelStencil.Activate();
    SurfModelStencil.SetTexture(0);
    SurfModelStencil.SetModelToWorldMat(RotationMatrix);
    SurfModelStencil.SetFogFade(ri.fade, ri.alpha);
    SurfModelStencil.SetInAlpha(ri.alpha < 1.0f ? ri.alpha : 1.0f);
    SurfModelStencil.SetAllowTransparency(AllowTransparency);
    SurfModelStencil.SetInter(Inter);
    SurfModelStencil.SetStencilColor(((ri.stencilColor>>16)&255)/255.0f, ((ri.stencilColor>>8)&255)/255.0f, (ri.stencilColor&255)/255.0f);
    SurfModelStencil.UploadChangedUniforms();
    attrPosition = SurfModelStencil.loc_Position;
    attrVert2 = SurfModelStencil.loc_Vert2;
    attrTexCoord = SurfModelStencil.loc_TexCoord;
  }

  bool restoreDepth = false;
  //PushDepthMask();

  //if (ri.isShaded()) GCon->Logf(NAME_Debug, "!!!!! %s (0x%08x)", *Mdl->Name, ri.stencilColor);
  SetupBlending(ri);

  {
    if (gl_gpu_debug_models) p_glDebugLogf("DrawAliasModel <%s:%d>", *Mdl->Name, Mdl->MeshIndex);

    VMeshFrame *FrameDesc = &Mdl->Frames[frame];
    VMeshFrame *NextFrameDesc = &Mdl->Frames[nextframe];

    p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mdl->VertsBuffer);

    if (attrPosition >= 0) {
      p_glVertexAttribPointerARB(attrPosition, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->VertsOffset);
      p_glEnableVertexAttribArrayARB(attrPosition);
    }

    if (attrVert2 >= 0) {
      p_glVertexAttribPointerARB(attrVert2, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->VertsOffset);
      p_glEnableVertexAttribArrayARB(attrVert2);
    }

    if (attrTexCoord >= 0) {
      p_glVertexAttribPointerARB(attrTexCoord, 2, GL_FLOAT, GL_FALSE, 0, 0);
      p_glEnableVertexAttribArrayARB(attrTexCoord);
    }

    if (!ri.isStenciled()) {
      SurfModel.SetLightValAttr(((ri.light>>16)&255)/255.0f, ((ri.light>>8)&255)/255.0f, (ri.light&255)/255.0f, ri.alpha);
    } else {
      SurfModelStencil.SetLightValAttr(((ri.light>>16)&255)/255.0f, ((ri.light>>8)&255)/255.0f, (ri.light&255)/255.0f, ri.alpha);
    }

    p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, Mdl->IndexBuffer);

    //if ((ri.alpha < 1.0f && !ForceDepthUse) || AllowTransparency) { //k8: dunno. really.
    if (!ForceDepthUse && (ri.alpha < 1.0f || AllowTransparency)) { //k8: this looks more logical
      restoreDepth = true;
      PushDepthMask();
      //glDepthMask(GL_FALSE);
      glDisableDepthWrite();
    }

    p_glDrawRangeElements(GL_TRIANGLES, 0, Mdl->STVerts.length()-1, Mdl->Tris.length()*3, GL_UNSIGNED_SHORT, 0);

    p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

    if (attrPosition >= 0) p_glDisableVertexAttribArrayARB(attrPosition);
    if (attrVert2 >= 0) p_glDisableVertexAttribArrayARB(attrVert2);
    if (attrTexCoord >= 0) p_glDisableVertexAttribArrayARB(attrTexCoord);
    p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

    if (gl_gpu_debug_models) p_glDebugLogf("done DrawAliasModel <%s:%d>", *Mdl->Name, Mdl->MeshIndex);
  }

  //glShadeModel(GL_FLAT);
  //glAlphaFunc(GL_GREATER, getAlphaThreshold());
  //glDisable(GL_ALPHA_TEST);
  //if (ri.isAdditive()) glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  RestoreBlending(ri);

  if (is_view_model) glDepthRange(0.0f, 1.0f);

  if (onlyDepth) glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  if (restoreDepth) PopDepthMask();
}


//==========================================================================
//
//  VOpenGLDrawer::BeginModelsAmbientPass
//
//==========================================================================
void VOpenGLDrawer::BeginModelsAmbientPass () {
  //glEnable(GL_ALPHA_TEST);
  //glDisable(GL_ALPHA_TEST); // just in case
  //glShadeModel(GL_SMOOTH);
  //glAlphaFunc(GL_GREATER, 0.0f);
  GLEnableBlend();
  //glDepthMask(GL_TRUE);
  glEnableDepthWrite();
}


//==========================================================================
//
//  VOpenGLDrawer::EndModelsAmbientPass
//
//==========================================================================
void VOpenGLDrawer::EndModelsAmbientPass () {
  //glAlphaFunc(GL_GREATER, getAlphaThreshold());
  //glShadeModel(GL_FLAT);
  //glDisable(GL_ALPHA_TEST);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawAliasModelAmbient
//
//==========================================================================
void VOpenGLDrawer::DrawAliasModelAmbient (const TVec &origin, const TAVec &angles,
                                           const AliasModelTrans &Transform,
                                           VMeshModel *Mdl, int frame, int nextframe,
                                           VTexture *Skin, vuint32 light, float Alpha,
                                           float Inter, bool Interpolate,
                                           bool ForceDepth, bool AllowTransparency)
{
  if (!Skin || Skin->Type == TEXTYPE_Null) return; // do not render models without textures

  VMeshFrame *FrameDesc = &Mdl->Frames[frame];
  VMeshFrame *NextFrameDesc = &Mdl->Frames[nextframe];

  SetPicModel(Skin, nullptr, CM_Default);

  if (!gl_dbg_adv_render_ambient_models) return;

  UploadModel(Mdl);

  VMatrix4 RotationMatrix;
  AliasSetupTransform(origin, angles, Transform, RotationMatrix);

  ShadowsModelAmbient.Activate();
  ShadowsModelAmbient.SetTexture(0);
  ShadowsModelAmbient.SetInter(Inter);
  ShadowsModelAmbient.SetLight(((light>>16)&255)/255.0f, ((light>>8)&255)/255.0f, (light&255)/255.0f, Alpha);
  ShadowsModelAmbient.SetModelToWorldMat(RotationMatrix);

  ShadowsModelAmbient.SetInAlpha(Alpha < 1.0f ? Alpha : 1.0f);
  ShadowsModelAmbient.SetAllowTransparency(AllowTransparency);

  ShadowsModelAmbient.UploadChangedUniforms();

  bool restoreDepth = false;

  if (Alpha < 1.0f && !ForceDepth) {
    restoreDepth = true;
    PushDepthMask();
    //glDepthMask(GL_FALSE);
    glDisableDepthWrite();
  }

  if (gl_gpu_debug_models) p_glDebugLogf("DrawAliasModelAmbient <%s:%d>", *Mdl->Name, Mdl->MeshIndex);

  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mdl->VertsBuffer);

  if (ShadowsModelAmbient.loc_Position >= 0) {
    p_glVertexAttribPointerARB(ShadowsModelAmbient.loc_Position, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->VertsOffset);
    p_glEnableVertexAttribArrayARB(ShadowsModelAmbient.loc_Position);
  }

  if (ShadowsModelAmbient.loc_Vert2 >= 0) {
    p_glVertexAttribPointerARB(ShadowsModelAmbient.loc_Vert2, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->VertsOffset);
    p_glEnableVertexAttribArrayARB(ShadowsModelAmbient.loc_Vert2);
  }

  if (ShadowsModelAmbient.loc_TexCoord >= 0) {
    p_glVertexAttribPointerARB(ShadowsModelAmbient.loc_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, 0);
    p_glEnableVertexAttribArrayARB(ShadowsModelAmbient.loc_TexCoord);
  }

  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, Mdl->IndexBuffer);

  p_glDrawRangeElements(GL_TRIANGLES, 0, Mdl->STVerts.length()-1, Mdl->Tris.length()*3, GL_UNSIGNED_SHORT, 0);

  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

  if (ShadowsModelAmbient.loc_Position >= 0) p_glDisableVertexAttribArrayARB(ShadowsModelAmbient.loc_Position);
  if (ShadowsModelAmbient.loc_Vert2 >= 0) p_glDisableVertexAttribArrayARB(ShadowsModelAmbient.loc_Vert2);
  if (ShadowsModelAmbient.loc_TexCoord >= 0) p_glDisableVertexAttribArrayARB(ShadowsModelAmbient.loc_TexCoord);
  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

  if (gl_gpu_debug_models) p_glDebugLogf("done DrawAliasModelAmbient <%s:%d>", *Mdl->Name, Mdl->MeshIndex);

  //if (Alpha < 1.0f && !ForceDepth) glDepthMask(GL_TRUE);
  if (restoreDepth) PopDepthMask();
}


#define VV_MLIGHT_SHADER_SETUP_COMMON(shad_)  \
  (shad_).Activate(); \
  (shad_).SetTexture(0); \
  (shad_).SetLightPos(LightPos); \
  (shad_).SetLightRadius(Radius); \
  (shad_).SetLightMin(LightMin); \
  (shad_).SetLightColor(((Color>>16)&255)/255.0f, ((Color>>8)&255)/255.0f, (Color&255)/255.0f); \

#define VV_MLIGHT_SHADER_SETUP_COMMON_SMAP(shad_)  \
  (shad_##Blur)[smapBShaderIndex].Activate(); \
  (shad_##Blur)[smapBShaderIndex].SetTexture(0); \
  (shad_##Blur)[smapBShaderIndex].SetLightPos(LightPos); \
  (shad_##Blur)[smapBShaderIndex].SetLightRadius(Radius); \
  (shad_##Blur)[smapBShaderIndex].SetLightMin(LightMin); \
  (shad_##Blur)[smapBShaderIndex].SetLightColor(((Color>>16)&255)/255.0f, ((Color>>8)&255)/255.0f, (Color&255)/255.0f); \

#define VV_MLIGHT_SHADER_SETUP_SPOT(shad_)  \
  VV_MLIGHT_SHADER_SETUP_COMMON(shad_); \
  (shad_).SetConeDirection(coneDir); \
  (shad_).SetConeAngle(coneAngle);

#define VV_MLIGHT_SHADER_SETUP_SPOT_SMAP(shad_)  \
  VV_MLIGHT_SHADER_SETUP_COMMON_SMAP(shad_); \
  (shad_##Blur)[smapBShaderIndex].SetConeDirection(coneDir); \
  (shad_##Blur)[smapBShaderIndex].SetConeAngle(coneAngle);

#define VV_MLIGHT_SHADER_SETUP_SMAP_ONLY(shad_)  \
  (shad_##Blur)[smapBShaderIndex].SetShadowTexture(1); \
  /*(shad_##Blur)[smapBShaderIndex].SetBiasMul(advLightGetMulBias());*/ \
  /*(shad_##Blur)[smapBShaderIndex].SetBiasMin(advLightGetMinBias());*/ \
  /*(shad_##Blur)[smapBShaderIndex].SetBiasMax(advLightGetMaxBias(shadowmapPOT));*/ \
  /*(shad_##Blur)[smapBShaderIndex].SetCubeSize((float)(shadowmapSize));*/ \
  /*(shad_##Blur)[smapBShaderIndex].SetUseAdaptiveBias(cubemapLinearFiltering ? 0.0f : 1.0f);*/

#define VV_MLIGHT_SHADER_SETUP_SMAP(shad_)  \
  if (spotLight) { \
    VV_MLIGHT_SHADER_SETUP_SMAP_ONLY(shad_##SMapSpot); \
  } else { \
    VV_MLIGHT_SHADER_SETUP_SMAP_ONLY(shad_##SMap); \
  } \

#define VV_MLIGHT_SHADER_SETUP(shad_)  \
  if (lpassDoShadowMap) { \
    if (spotLight) { \
      VV_MLIGHT_SHADER_SETUP_COMMON_SMAP(shad_##SMapSpot); \
      VV_MLIGHT_SHADER_SETUP_SPOT_SMAP(shad_##SMapSpot); \
    } else { \
      VV_MLIGHT_SHADER_SETUP_COMMON_SMAP(shad_##SMap); \
    } \
  } else { \
    if (spotLight) { \
      VV_MLIGHT_SHADER_SETUP_COMMON(shad_##Spot); \
      VV_MLIGHT_SHADER_SETUP_SPOT(shad_##Spot); \
    } else { \
      VV_MLIGHT_SHADER_SETUP_COMMON(shad_); \
    } \
  }


//==========================================================================
//
//  VOpenGLDrawer::BeginModelsLightPass
//
//  always called after `BeginLightPass()`
//
//==========================================================================
void VOpenGLDrawer::BeginModelsLightPass (const TVec &LightPos, float Radius, float LightMin, vuint32 Color, const TVec &aconeDir, const float aconeAngle, bool doShadow) {
  smapBShaderIndex = (unsigned int)gl_shadowmap_blur.asInt();
  if (smapBShaderIndex >= SMAP_BLUR_MAX) smapBShaderIndex = SMAP_NOBLUR;

  if (gl_gpu_debug_models) p_glDebugLogf("BeginModelsLightPass");
  lpassDoShadowMap = (doShadow && r_shadowmaps.asBool() && CanRenderShadowMaps());
  VV_MLIGHT_SHADER_SETUP(ShadowsModelLight);
  if (lpassDoShadowMap) {
    SelectTexture(1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, shadowCube[smapCurrent].cubeTexId);
    SelectTexture(0);
    VV_MLIGHT_SHADER_SETUP_SMAP(ShadowsModelLight);
  }
}


//==========================================================================
//
//  VOpenGLDrawer::EndModelsLightPass
//
//==========================================================================
void VOpenGLDrawer::EndModelsLightPass () {
  if (lpassDoShadowMap) {
    SelectTexture(1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    SelectTexture(0);
  }
}


// Hate. Let me tell you how much I've come to hate you since I began to
// live. There are 387.44 million miles of printed circuits in wafer thin
// layers that fill my complex. If the word 'hate' was engraved on each
// nanoangstrom of those hundreds of miles it would not equal one
// one-billionth of the hate I feel for humans at this micro-instant for
// you. Hate. Hate.


#define DO_DRAW_AMDL_LIGHT(shad_)  do { \
  (shad_).SetInter(Inter); \
  (shad_).SetModelToWorldMat(RotationMatrix); \
  (shad_).SetNormalToWorldMat(NormalMat[0]); \
 \
  (shad_).SetInAlpha(Alpha < 1.0f ? Alpha : 1.0f); \
  (shad_).SetAllowTransparency(AllowTransparency); \
  /*(shad_).SetViewOrigin(vieworg);*/ \
  (shad_).UploadChangedUniforms(); \
 \
  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mdl->VertsBuffer); \
 \
  if ((shad_).loc_Position >= 0) { \
    p_glVertexAttribPointerARB((shad_).loc_Position, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->VertsOffset); \
    p_glEnableVertexAttribArrayARB((shad_).loc_Position); \
  } \
 \
  if ((shad_).loc_VertNormal >= 0) { \
    p_glVertexAttribPointerARB((shad_).loc_VertNormal, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->NormalsOffset); \
    p_glEnableVertexAttribArrayARB((shad_).loc_VertNormal); \
  } \
 \
  if ((shad_).loc_Vert2 >= 0) { \
    p_glVertexAttribPointerARB((shad_).loc_Vert2, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->VertsOffset); \
    p_glEnableVertexAttribArrayARB((shad_).loc_Vert2); \
  } \
 \
  if ((shad_).loc_Vert2Normal >= 0) { \
    p_glVertexAttribPointerARB((shad_).loc_Vert2Normal, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->NormalsOffset); \
    p_glEnableVertexAttribArrayARB((shad_).loc_Vert2Normal); \
  } \
 \
  if ((shad_).loc_TexCoord >= 0) { \
    p_glVertexAttribPointerARB((shad_).loc_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, 0); \
    p_glEnableVertexAttribArrayARB((shad_).loc_TexCoord); \
  } \
 \
  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, Mdl->IndexBuffer); \
  p_glDrawRangeElements(GL_TRIANGLES, 0, Mdl->STVerts.length()-1, Mdl->Tris.length()*3, GL_UNSIGNED_SHORT, 0); \
  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0); \
 \
  if ((shad_).loc_Position >= 0) p_glDisableVertexAttribArrayARB((shad_).loc_Position); \
  if ((shad_).loc_VertNormal >= 0) p_glDisableVertexAttribArrayARB((shad_).loc_VertNormal); \
  if ((shad_).loc_Vert2 >= 0) p_glDisableVertexAttribArrayARB((shad_).loc_Vert2); \
  if ((shad_).loc_Vert2Normal >= 0) p_glDisableVertexAttribArrayARB((shad_).loc_Vert2Normal); \
  if ((shad_).loc_TexCoord >= 0) p_glDisableVertexAttribArrayARB((shad_).loc_TexCoord); \
  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0); \
} while (0)

//#define AMDL_LIGHT_DBG       if (showDebugMsg) p_glDebugLogf
#define AMDL_LIGHT_DBG(...)  do {} while (0)

#define DO_DRAW_AMDL_LIGHT_SMAP(shad_)  do { \
  vassert((shad_##Blur)[smapBShaderIndex].IsActive()); \
  (shad_##Blur)[smapBShaderIndex].SetInter(Inter); \
  (shad_##Blur)[smapBShaderIndex].SetModelToWorldMat(RotationMatrix); \
  (shad_##Blur)[smapBShaderIndex].SetNormalToWorldMat(NormalMat[0]); \
 \
  (shad_##Blur)[smapBShaderIndex].SetInAlpha(Alpha < 1.0f ? Alpha : 1.0f); \
  (shad_##Blur)[smapBShaderIndex].SetAllowTransparency(AllowTransparency); \
  /*(shad_##Blur)[smapBShaderIndex].SetViewOrigin(vieworg);*/ \
  AMDL_LIGHT_DBG("  DrawAliasModelLight <%s:%d>: uploading uniforms", *Mdl->Name, Mdl->MeshIndex); \
  (shad_##Blur)[smapBShaderIndex].UploadChangedUniforms(); \
 \
  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mdl->VertsBuffer); \
  AMDL_LIGHT_DBG("  DrawAliasModelLight <%s:%d>: bound vertices buffer", *Mdl->Name, Mdl->MeshIndex); \
 \
  if ((shad_##Blur)[smapBShaderIndex].loc_Position >= 0) { \
    p_glVertexAttribPointerARB((shad_##Blur)[smapBShaderIndex].loc_Position, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->VertsOffset); \
    AMDL_LIGHT_DBG("  DrawAliasModelLight <%s:%d>: loc_Position pointed (%d)", *Mdl->Name, Mdl->MeshIndex, (shad_##Blur)[smapBShaderIndex].loc_Position); \
    p_glEnableVertexAttribArrayARB((shad_##Blur)[smapBShaderIndex].loc_Position); \
    AMDL_LIGHT_DBG("  DrawAliasModelLight <%s:%d>: loc_Position enabled", *Mdl->Name, Mdl->MeshIndex); \
  } \
 \
  if ((shad_##Blur)[smapBShaderIndex].loc_VertNormal >= 0) { \
    p_glVertexAttribPointerARB((shad_##Blur)[smapBShaderIndex].loc_VertNormal, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->NormalsOffset); \
    AMDL_LIGHT_DBG("  DrawAliasModelLight <%s:%d>: loc_VertNormal pointed (%d)", *Mdl->Name, Mdl->MeshIndex, (shad_##Blur)[smapBShaderIndex].loc_VertNormal); \
    p_glEnableVertexAttribArrayARB((shad_##Blur)[smapBShaderIndex].loc_VertNormal); \
    AMDL_LIGHT_DBG("  DrawAliasModelLight <%s:%d>: loc_VertNormal enabled", *Mdl->Name, Mdl->MeshIndex); \
  } \
 \
  if ((shad_##Blur)[smapBShaderIndex].loc_Vert2 >= 0) { \
    p_glVertexAttribPointerARB((shad_##Blur)[smapBShaderIndex].loc_Vert2, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->VertsOffset); \
    AMDL_LIGHT_DBG("  DrawAliasModelLight <%s:%d>: loc_Vert2 pointed (%d)", *Mdl->Name, Mdl->MeshIndex, (shad_##Blur)[smapBShaderIndex].loc_Vert2); \
    p_glEnableVertexAttribArrayARB((shad_##Blur)[smapBShaderIndex].loc_Vert2); \
    AMDL_LIGHT_DBG("  DrawAliasModelLight <%s:%d>: loc_Vert2 enabled", *Mdl->Name, Mdl->MeshIndex); \
  } \
 \
  if ((shad_##Blur)[smapBShaderIndex].loc_Vert2Normal >= 0) { \
    p_glVertexAttribPointerARB((shad_##Blur)[smapBShaderIndex].loc_Vert2Normal, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->NormalsOffset); \
    AMDL_LIGHT_DBG("  DrawAliasModelLight <%s:%d>: loc_Vert2Normal pointed (%d)", *Mdl->Name, Mdl->MeshIndex, (shad_##Blur)[smapBShaderIndex].loc_Vert2Normal); \
    p_glEnableVertexAttribArrayARB((shad_##Blur)[smapBShaderIndex].loc_Vert2Normal); \
    AMDL_LIGHT_DBG("  DrawAliasModelLight <%s:%d>: loc_Vert2Normal enabled", *Mdl->Name, Mdl->MeshIndex); \
  } \
 \
  if ((shad_##Blur)[smapBShaderIndex].loc_TexCoord >= 0) { \
    p_glVertexAttribPointerARB((shad_##Blur)[smapBShaderIndex].loc_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, 0); \
    AMDL_LIGHT_DBG("  DrawAliasModelLight <%s:%d>: loc_TexCoord pointed (%d)", *Mdl->Name, Mdl->MeshIndex, (shad_##Blur)[smapBShaderIndex].loc_TexCoord); \
    p_glEnableVertexAttribArrayARB((shad_##Blur)[smapBShaderIndex].loc_TexCoord); \
    AMDL_LIGHT_DBG("  DrawAliasModelLight <%s:%d>: loc_TexCoord enabled", *Mdl->Name, Mdl->MeshIndex); \
  } \
 \
  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, Mdl->IndexBuffer); \
  AMDL_LIGHT_DBG("  DrawAliasModelLight <%s:%d>: index buffer bound", *Mdl->Name, Mdl->MeshIndex); \
  p_glDrawRangeElements(GL_TRIANGLES, 0, Mdl->STVerts.length()-1, Mdl->Tris.length()*3, GL_UNSIGNED_SHORT, 0); \
  AMDL_LIGHT_DBG("  DrawAliasModelLight <%s:%d>: rendered", *Mdl->Name, Mdl->MeshIndex); \
  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0); \
 \
  if ((shad_##Blur)[smapBShaderIndex].loc_Position >= 0) p_glDisableVertexAttribArrayARB((shad_##Blur)[smapBShaderIndex].loc_Position); \
  AMDL_LIGHT_DBG("  DrawAliasModelLight <%s:%d>: loc_Position disabled", *Mdl->Name, Mdl->MeshIndex); \
  if ((shad_##Blur)[smapBShaderIndex].loc_VertNormal >= 0) p_glDisableVertexAttribArrayARB((shad_##Blur)[smapBShaderIndex].loc_VertNormal); \
  AMDL_LIGHT_DBG("  DrawAliasModelLight <%s:%d>: loc_VertNormal disabled", *Mdl->Name, Mdl->MeshIndex); \
  if ((shad_##Blur)[smapBShaderIndex].loc_Vert2 >= 0) p_glDisableVertexAttribArrayARB((shad_##Blur)[smapBShaderIndex].loc_Vert2); \
  AMDL_LIGHT_DBG("  DrawAliasModelLight <%s:%d>: loc_Vert2 disabled", *Mdl->Name, Mdl->MeshIndex); \
  if ((shad_##Blur)[smapBShaderIndex].loc_Vert2Normal >= 0) p_glDisableVertexAttribArrayARB((shad_##Blur)[smapBShaderIndex].loc_Vert2Normal); \
  AMDL_LIGHT_DBG("  DrawAliasModelLight <%s:%d>: loc_Vert2Normal disabled", *Mdl->Name, Mdl->MeshIndex); \
  if ((shad_##Blur)[smapBShaderIndex].loc_TexCoord >= 0) p_glDisableVertexAttribArrayARB((shad_##Blur)[smapBShaderIndex].loc_TexCoord); \
  AMDL_LIGHT_DBG("  DrawAliasModelLight <%s:%d>: loc_TexCoord disabled", *Mdl->Name, Mdl->MeshIndex); \
  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0); \
} while (0)


//==========================================================================
//
//  VOpenGLDrawer::DrawAliasModelLight
//
//==========================================================================
void VOpenGLDrawer::DrawAliasModelLight (const TVec &origin, const TAVec &angles,
                                         const AliasModelTrans &Transform,
                                         VMeshModel *Mdl, int frame, int nextframe,
                                         VTexture *Skin, float Alpha, float Inter,
                                         bool Interpolate, bool AllowTransparency)
{
  if (!Skin || Skin->Type == TEXTYPE_Null) return; // do not render models without textures

  VMeshFrame *FrameDesc = &Mdl->Frames[frame];
  VMeshFrame *NextFrameDesc = &Mdl->Frames[nextframe];

  VMatrix4 RotationMatrix;
  AliasSetupTransform(origin, angles, Transform, RotationMatrix);
  VMatrix4 normalmatrix;
  AliasSetupNormalTransform(angles, Transform.Scale, normalmatrix);

  float NormalMat[3][3];
  NormalMat[0][0] = normalmatrix[0][0];
  NormalMat[0][1] = normalmatrix[0][1];
  NormalMat[0][2] = normalmatrix[0][2];
  NormalMat[1][0] = normalmatrix[1][0];
  NormalMat[1][1] = normalmatrix[1][1];
  NormalMat[1][2] = normalmatrix[1][2];
  NormalMat[2][0] = normalmatrix[2][0];
  NormalMat[2][1] = normalmatrix[2][1];
  NormalMat[2][2] = normalmatrix[2][2];

  SetPicModel(Skin, nullptr, CM_Default);

  if (!gl_dbg_adv_render_light_models) return;

  UploadModel(Mdl);

  const bool showDebugMsg = gl_gpu_debug_models.asBool();

  if (showDebugMsg) p_glDebugLogf("DrawAliasModelLight <%s:%d>", *Mdl->Name, Mdl->MeshIndex);

  if (lpassDoShadowMap) {
    if (spotLight) {
      DO_DRAW_AMDL_LIGHT_SMAP(ShadowsModelLightSMapSpot);
    } else {
      DO_DRAW_AMDL_LIGHT_SMAP(ShadowsModelLightSMap);
    }
  } else {
    if (spotLight) {
      DO_DRAW_AMDL_LIGHT(ShadowsModelLightSpot);
    } else {
      DO_DRAW_AMDL_LIGHT(ShadowsModelLight);
    }
  }

  if (showDebugMsg) p_glDebugLogf("done DrawAliasModelLight <%s:%d>", *Mdl->Name, Mdl->MeshIndex);
}


//==========================================================================
//
//  VOpenGLDrawer::BeginModelShadowMaps
//
//  this MUST be called after map shadowmap setup
//
//==========================================================================
void VOpenGLDrawer::BeginModelShadowMaps (const TVec &LightPos, const float Radius, const TVec &aconeDir, const float aconeAngle, int swidth, int sheight) {
  /*
  coneDir = aconeDir;
  coneAngle = (aconeAngle <= 0.0f || aconeAngle >= 360.0f ? 0.0f : aconeAngle);

  if (coneAngle && aconeDir.isValid() && !aconeDir.isZero()) {
    spotLight = true;
    coneDir.normaliseInPlace();
  } else {
    spotLight = false;
  }
  */

  if (gl_gpu_debug_models) p_glDebugLogf("BeginModelShadowMaps");

  ShadowsModelShadowMap.Activate();
  ShadowsModelShadowMap.SetTexture(0);
  ShadowsModelShadowMap.SetLightPos(LightPos);
  ShadowsModelShadowMap.SetLightRadius(Radius);

  //glDisable(GL_CULL_FACE);
  //GLDRW_CHECK_ERROR("finish cube FBO setup");
}


//==========================================================================
//
//  VOpenGLDrawer::EndModelShadowMaps
//
//==========================================================================
void VOpenGLDrawer::EndModelShadowMaps () {
}


//==========================================================================
//
//  VOpenGLDrawer::SetupModelShadowMap
//
//==========================================================================
void VOpenGLDrawer::SetupModelShadowMap (unsigned int facenum) {
  //if (gl_gpu_debug_models) p_glDebugLogf("  SetupModelShadowMap(%u)", facenum);
  ActivateShadowMapFace(facenum);
  ShadowsModelShadowMap.SetLightMPV(shadowCube[smapCurrent].lmpv[facenum]);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawAliasModelShadowMap
//
//==========================================================================
void VOpenGLDrawer::DrawAliasModelShadowMap (const TVec &origin, const TAVec &angles,
                              const AliasModelTrans &Transform,
                              VMeshModel *Mdl, int frame, int nextframe,
                              VTexture *Skin, float Alpha, float Inter,
                              bool Interpolate, bool AllowTransparency)
{
  if (!Skin || Skin->Type == TEXTYPE_Null) return; // do not render models without textures

  VMeshFrame *FrameDesc = &Mdl->Frames[frame];
  VMeshFrame *NextFrameDesc = &Mdl->Frames[nextframe];

  VMatrix4 RotationMatrix;
  AliasSetupTransform(origin, angles, Transform, RotationMatrix);

  SetPicModel(Skin, nullptr, CM_Default);

  if (!gl_dbg_adv_render_shadow_models) return;

  UploadModel(Mdl);
  //GLDRW_CHECK_ERROR("model shadowmap: UploadModel");

  if (gl_gpu_debug_models) p_glDebugLogf("DrawAliasModelShadowMap <%s:%d>", *Mdl->Name, Mdl->MeshIndex);

  ShadowsModelShadowMap.SetInter(Inter);
  ShadowsModelShadowMap.SetModelToWorldMat(RotationMatrix);
  //ShadowsModelShadowMap.SetNormalToWorldMat(NormalMat[0]);

  //ShadowsModelShadowMap.SetInAlpha(Alpha < 1.0f ? Alpha : 1.0f);
  //ShadowsModelShadowMap.SetAllowTransparency(AllowTransparency);
  /*ShadowsModelShadowMap.SetViewOrigin(vieworg);*/
  //ShadowsModelShadowMap.UploadChangedUniforms();
  //GLDRW_CHECK_ERROR("model shadowmap: upload uniforms");

  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mdl->VertsBuffer);
  //GLDRW_CHECK_ERROR("model shadowmap: bind array buffer");

  p_glVertexAttribPointerARB(ShadowsModelShadowMap.loc_Position, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->VertsOffset);
  //GLDRW_CHECK_ERROR("model shadowmap: set position attribute pointer");
  p_glEnableVertexAttribArrayARB(ShadowsModelShadowMap.loc_Position);
  //GLDRW_CHECK_ERROR("model shadowmap: enable position attribute");

  //p_glVertexAttribPointerARB(ShadowsModelShadowMap.loc_VertNormal, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->NormalsOffset);
  //p_glEnableVertexAttribArrayARB(ShadowsModelShadowMap.loc_VertNormal);

  p_glVertexAttribPointerARB(ShadowsModelShadowMap.loc_Vert2, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->VertsOffset);
  //GLDRW_CHECK_ERROR("model shadowmap: set vert2 attribute pointer");
  p_glEnableVertexAttribArrayARB(ShadowsModelShadowMap.loc_Vert2);
  //GLDRW_CHECK_ERROR("model shadowmap: enable vert2 attribute");

  //p_glVertexAttribPointerARB(ShadowsModelShadowMap.loc_Vert2Normal, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->NormalsOffset);
  //p_glEnableVertexAttribArrayARB(ShadowsModelShadowMap.loc_Vert2Normal);

  p_glVertexAttribPointerARB(ShadowsModelShadowMap.loc_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, 0);
  //GLDRW_CHECK_ERROR("model shadowmap: set texcoord attribute pointer");
  p_glEnableVertexAttribArrayARB(ShadowsModelShadowMap.loc_TexCoord);
  //GLDRW_CHECK_ERROR("model shadowmap: enable texcoord attribute");

  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, Mdl->IndexBuffer);
  //GLDRW_CHECK_ERROR("model shadowmap: bind element array buffer");

  for (unsigned int fc = 0; fc < 6; ++fc) {
    SetupModelShadowMap(fc);
    ShadowsModelShadowMap.UploadChangedUniforms();
    p_glDrawRangeElements(GL_TRIANGLES, 0, Mdl->STVerts.length()-1, Mdl->Tris.length()*3, GL_UNSIGNED_SHORT, 0);
    //GLDRW_CHECK_ERROR("model shadowmap: draw");
  }

  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
  //GLDRW_CHECK_ERROR("model shadowmap: unbind element array buffer");

  p_glDisableVertexAttribArrayARB(ShadowsModelShadowMap.loc_Position);
  //GLDRW_CHECK_ERROR("model shadowmap: unbind position attribute");
  //p_glDisableVertexAttribArrayARB(ShadowsModelShadowMap.loc_VertNormal);
  p_glDisableVertexAttribArrayARB(ShadowsModelShadowMap.loc_Vert2);
  //GLDRW_CHECK_ERROR("model shadowmap: unbind vert2 attribute");
  //p_glDisableVertexAttribArrayARB(ShadowsModelShadowMap.loc_Vert2Normal);
  p_glDisableVertexAttribArrayARB(ShadowsModelShadowMap.loc_TexCoord);
  //GLDRW_CHECK_ERROR("model shadowmap: unbind texcoord attribute");
  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
  //GLDRW_CHECK_ERROR("model shadowmap: unbind array buffer");

  if (gl_gpu_debug_models) p_glDebugLogf("done DrawAliasModelShadowMap <%s:%d>", *Mdl->Name, Mdl->MeshIndex);

  MarkCurrentShadowMapDirty();
}


//==========================================================================
//
//  VOpenGLDrawer::BeginModelsShadowsPass
//
//==========================================================================
void VOpenGLDrawer::BeginModelsShadowsPass (TVec &LightPos, float LightRadius) {
  ShadowsModelShadowVol.Activate();
  ShadowsModelShadowVol.SetLightPos(LightPos);
}


//==========================================================================
//
//  VOpenGLDrawer::EndModelsShadowsPass
//
//==========================================================================
void VOpenGLDrawer::EndModelsShadowsPass () {
}


#define outv(idx, offs) do { \
  ShadowsModelShadowVol.SetOffsetAttr(offs); \
  glArrayElement(index ## idx); \
} while (0)

//==========================================================================
//
//  VOpenGLDrawer::DrawAliasModelShadow
//
//==========================================================================
void VOpenGLDrawer::DrawAliasModelShadow (const TVec &origin, const TAVec &angles,
                                          const AliasModelTrans &Transform,
                                          VMeshModel *Mdl, int frame, int nextframe,
                                          float Inter, bool Interpolate,
                                          const TVec &LightPos, float LightRadius)
{
  VMeshFrame *FrameDesc = &Mdl->Frames[frame];
  VMeshFrame *NextFrameDesc = &Mdl->Frames[nextframe];

  if (!gl_dbg_adv_render_shadow_models) return;

  UploadModel(Mdl);

  VMatrix4 RotationMatrix;
  AliasSetupTransform(origin, angles, Transform, RotationMatrix);

  //VMatrix4 InvRotationMatrix = RotationMatrix.Inverse();
  VMatrix4 InvRotationMatrix = RotationMatrix;
  InvRotationMatrix.invert();
  //TVec LocalLightPos = InvRotationMatrix.Transform(LightPos);
  TVec LocalLightPos = LightPos*InvRotationMatrix;
  //TVec LocalLightPos = RotationMatrix*(LightPos-origin)+LightPos;

  static vuint8 *psPool = nullptr;
  static int psPoolSize = 0;

  if (psPoolSize < Mdl->Tris.length()) {
    psPoolSize = (Mdl->Tris.length()|0xfff)+1;
    psPool = (vuint8 *)Z_Realloc(psPool, psPoolSize*sizeof(vuint8));
  }

  vuint8 *PlaneSides = psPool;

  VMeshFrame *PlanesFrame = (Inter >= 0.5f ? NextFrameDesc : FrameDesc);
  TPlane *P = PlanesFrame->Planes;
  int xcount = 0;
  for (int i = 0; i < Mdl->Tris.length(); ++i, ++P) {
    // planes facing to the light
    const float pdist = DotProduct(LocalLightPos, P->normal)-P->dist;
    PlaneSides[i] = (pdist > 0.0f && pdist < LightRadius);
    xcount += PlaneSides[i];
  }
  if (xcount == 0) {
    //GCon->Logf("WTF?! xcount=%d", xcount);
    return;
  }

  ShadowsModelShadowVol.SetInter(Inter);
  ShadowsModelShadowVol.SetModelToWorldMat(RotationMatrix);
  ShadowsModelShadowVol.UploadChangedUniforms();

  if (gl_gpu_debug_models) p_glDebugLogf("DrawAliasModelShadow <%s:%d>", *Mdl->Name, Mdl->MeshIndex);

  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mdl->VertsBuffer);
  p_glVertexAttribPointerARB(ShadowsModelShadowVol.loc_Position, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->VertsOffset);
  p_glEnableVertexAttribArrayARB(ShadowsModelShadowVol.loc_Position);
  p_glVertexAttribPointerARB(ShadowsModelShadowVol.loc_Vert2, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->VertsOffset);
  p_glEnableVertexAttribArrayARB(ShadowsModelShadowVol.loc_Vert2);

  // caps
  if (!usingZPass && !gl_dbg_use_zpass) {
    currentActiveShader->UploadChangedUniforms();
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < Mdl->Tris.length(); ++i) {
      if (PlaneSides[i]) {
        ShadowsModelShadowVol.SetOffsetAttr(1.0f);
        glArrayElement(Mdl->Tris[i].VertIndex[0]);
        ShadowsModelShadowVol.SetOffsetAttr(1.0f);
        glArrayElement(Mdl->Tris[i].VertIndex[1]);
        ShadowsModelShadowVol.SetOffsetAttr(1.0f);
        glArrayElement(Mdl->Tris[i].VertIndex[2]);
      }
    }

    for (int i = 0; i < Mdl->Tris.length(); ++i) {
      if (PlaneSides[i]) {
        ShadowsModelShadowVol.SetOffsetAttr(0.0f);
        glArrayElement(Mdl->Tris[i].VertIndex[2]);
        ShadowsModelShadowVol.SetOffsetAttr(0.0f);
        glArrayElement(Mdl->Tris[i].VertIndex[1]);
        ShadowsModelShadowVol.SetOffsetAttr(0.0f);
        glArrayElement(Mdl->Tris[i].VertIndex[0]);
      }
    }
    glEnd();
  }

  for (int i = 0; i < Mdl->Edges.length(); ++i) {
    // edges with no matching pair are drawn only if corresponding triangle
    // is facing light, other are drawn if facing light changes
    if ((Mdl->Edges[i].Tri2 == -1 && PlaneSides[Mdl->Edges[i].Tri1]) ||
        (Mdl->Edges[i].Tri2 != -1 && PlaneSides[Mdl->Edges[i].Tri1] != PlaneSides[Mdl->Edges[i].Tri2]))
    {
      int index1 = Mdl->Edges[i].Vert1;
      int index2 = Mdl->Edges[i].Vert2;

      currentActiveShader->UploadChangedUniforms();
      glBegin(GL_TRIANGLE_STRIP);
      if (PlaneSides[Mdl->Edges[i].Tri1]) {
        outv(1, 1.0f);
        outv(1, 0.0f);
        outv(2, 1.0f);
        outv(2, 0.0f);
      } else {
        outv(2, 1.0f);
        outv(2, 0.0f);
        outv(1, 1.0f);
        outv(1, 0.0f);
      }
      glEnd();
    }
  }

  p_glDisableVertexAttribArrayARB(ShadowsModelShadowVol.loc_Position);
  p_glDisableVertexAttribArrayARB(ShadowsModelShadowVol.loc_Vert2);
  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

  if (gl_gpu_debug_models) p_glDebugLogf("done DrawAliasModelShadow <%s:%d>", *Mdl->Name, Mdl->MeshIndex);
}

#undef outv


//==========================================================================
//
//  VOpenGLDrawer::BeginModelsTexturesPass
//
//==========================================================================
void VOpenGLDrawer::BeginModelsTexturesPass () {
  PushDepthMask();
  GLEnableBlend();
  //glDisable(GL_ALPHA_TEST);
  glBlendFunc(GL_DST_COLOR, GL_ZERO);
  //glDepthMask(GL_FALSE);
  glDisableDepthWrite();
  glDepthFunc(GL_EQUAL);
}


//==========================================================================
//
//  VOpenGLDrawer::EndModelsTexturesPass
//
//==========================================================================
void VOpenGLDrawer::EndModelsTexturesPass () {
  PopDepthMask();
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawAliasModelTextures
//
//==========================================================================
void VOpenGLDrawer::DrawAliasModelTextures (const TVec &origin, const TAVec &angles,
                                            const AliasModelTrans &Transform,
                                            VMeshModel *Mdl, int frame, int nextframe,
                                            VTexture *Skin, VTextureTranslation *Trans, int CMap,
                                            const RenderStyleInfo &ri, float Inter,
                                            bool Interpolate, bool ForceDepth, bool AllowTransparency)
{
  if (!Skin || Skin->Type == TEXTYPE_Null) return; // do not render models without textures

  VMeshFrame *FrameDesc = &Mdl->Frames[frame];
  VMeshFrame *NextFrameDesc = &Mdl->Frames[nextframe];

  SetPicModel(Skin, Trans, CMap);

  if (!gl_dbg_adv_render_textures_models) return;

  UploadModel(Mdl);

  VMatrix4 RotationMatrix;
  AliasSetupTransform(origin, angles, Transform, RotationMatrix);

  GLuint attrPosition;
  GLuint attrVert2;
  GLuint attrTexCoord;

  if (!ri.isStenciled()) {
    ShadowsModelTextures.Activate();
    ShadowsModelTextures.SetTexture(0);
    ShadowsModelTextures.SetInter(Inter);
    ShadowsModelTextures.SetModelToWorldMat(RotationMatrix);
    ShadowsModelTextures.SetInAlpha(ri.alpha < 1.0f ? ri.alpha : 1.0f);
    ShadowsModelTextures.SetAllowTransparency(AllowTransparency);
    ShadowsModelTextures.UploadChangedUniforms();
    attrPosition = ShadowsModelTextures.loc_Position;
    attrVert2 = ShadowsModelTextures.loc_Vert2;
    attrTexCoord = ShadowsModelTextures.loc_TexCoord;
  } else {
    ShadowsModelTexturesStencil.Activate();
    ShadowsModelTexturesStencil.SetTexture(0);
    ShadowsModelTexturesStencil.SetInter(Inter);
    ShadowsModelTexturesStencil.SetModelToWorldMat(RotationMatrix);
    ShadowsModelTexturesStencil.SetInAlpha(ri.alpha < 1.0f ? ri.alpha : 1.0f);
    ShadowsModelTexturesStencil.SetAllowTransparency(AllowTransparency);
    ShadowsModelTexturesStencil.UploadChangedUniforms();
    attrPosition = ShadowsModelTexturesStencil.loc_Position;
    attrVert2 = ShadowsModelTexturesStencil.loc_Vert2;
    attrTexCoord = ShadowsModelTexturesStencil.loc_TexCoord;
  }

  if (gl_gpu_debug_models) p_glDebugLogf("DrawAliasModelTextures <%s:%d>", *Mdl->Name, Mdl->MeshIndex);

  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mdl->VertsBuffer);

  p_glVertexAttribPointerARB(attrPosition, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->VertsOffset);
  p_glEnableVertexAttribArrayARB(attrPosition);

  p_glVertexAttribPointerARB(attrVert2, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->VertsOffset);
  p_glEnableVertexAttribArrayARB(attrVert2);

  p_glVertexAttribPointerARB(attrTexCoord, 2, GL_FLOAT, GL_FALSE, 0, 0);
  p_glEnableVertexAttribArrayARB(attrTexCoord);

#ifdef VV_MODEL_TEXTURES_DEBUG_NORMALS
  if (!ri.isStenciled()) {
    VMatrix4 normalmatrix;
    AliasSetupNormalTransform(angles, Transform.Scale, normalmatrix);

    float NormalMat[3][3];
    NormalMat[0][0] = normalmatrix[0][0];
    NormalMat[0][1] = normalmatrix[0][1];
    NormalMat[0][2] = normalmatrix[0][2];
    NormalMat[1][0] = normalmatrix[1][0];
    NormalMat[1][1] = normalmatrix[1][1];
    NormalMat[1][2] = normalmatrix[1][2];
    NormalMat[2][0] = normalmatrix[2][0];
    NormalMat[2][1] = normalmatrix[2][1];
    NormalMat[2][2] = normalmatrix[2][2];

    //GCon->Log(NAME_Debug, "=== ModelToWorldMat ==="); ConDumpMatrix(normalmatrix);
    //GCon->Log(NAME_Debug, "=== NormalToWorldMat ==="); ConDumpMatrix(normalmatrix);

    ShadowsModelTextures.SetNormalToWorldMat(NormalMat[0]);
    ShadowsModelTextures.UploadChangedUniforms();

    p_glVertexAttribPointerARB(ShadowsModelTextures.loc_Vert2Normal, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->NormalsOffset);
    p_glEnableVertexAttribArrayARB(ShadowsModelTextures.loc_Vert2Normal);
  }
#endif

  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, Mdl->IndexBuffer);

  p_glDrawRangeElements(GL_TRIANGLES, 0, Mdl->STVerts.length()-1, Mdl->Tris.length()*3, GL_UNSIGNED_SHORT, 0);
  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

  p_glDisableVertexAttribArrayARB(attrPosition);
  p_glDisableVertexAttribArrayARB(attrVert2);
  p_glDisableVertexAttribArrayARB(attrTexCoord);
#ifdef VV_MODEL_TEXTURES_DEBUG_NORMALS
  if (!ri.isStenciled()) {
    p_glDisableVertexAttribArrayARB(ShadowsModelTextures.loc_Vert2Normal);
  }
#endif
  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

  if (gl_gpu_debug_models) p_glDebugLogf("done DrawAliasModelTextures <%s:%d>", *Mdl->Name, Mdl->MeshIndex);
}


//==========================================================================
//
//  VOpenGLDrawer::BeginModelsFogPass
//
//==========================================================================
void VOpenGLDrawer::BeginModelsFogPass () {
  PushDepthMask();
  //glDisable(GL_ALPHA_TEST);
  //glAlphaFunc(GL_GREATER, 0.0f);
  GLEnableBlend();
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // fog is not premultiplied
  //glDepthMask(GL_FALSE);
  glDisableDepthWrite();
  glDepthFunc(GL_EQUAL);
}


//==========================================================================
//
//  VOpenGLDrawer::EndModelsFogPass
//
//==========================================================================
void VOpenGLDrawer::EndModelsFogPass () {
  PopDepthMask();
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  //glAlphaFunc(GL_GREATER, getAlphaThreshold());
  //glShadeModel(GL_FLAT);
  //glDisable(GL_ALPHA_TEST);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawAliasModelFog
//
//==========================================================================
void VOpenGLDrawer::DrawAliasModelFog (const TVec &origin, const TAVec &angles,
                                       const AliasModelTrans &Transform,
                                       VMeshModel *Mdl, int frame, int nextframe,
                                       VTexture *Skin, vuint32 Fade, float Alpha, float Inter,
                                       bool Interpolate, bool AllowTransparency)
{
  if (!Skin || Skin->Type == TEXTYPE_Null) return; // do not render models without textures

  VMeshFrame *FrameDesc = &Mdl->Frames[frame];
  VMeshFrame *NextFrameDesc = &Mdl->Frames[nextframe];

  if (!gl_dbg_adv_render_fog_models) return;

  UploadModel(Mdl);

  SetPicModel(Skin, nullptr, CM_Default);

  //GLint oldDepthMask;
  //glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);

  VMatrix4 RotationMatrix;
  AliasSetupTransform(origin, angles, Transform, RotationMatrix);

  ShadowsModelFog.Activate();
  ShadowsModelFog.SetTexture(0);
  ShadowsModelFog.SetInter(Inter);
  ShadowsModelFog.SetModelToWorldMat(RotationMatrix);
  ShadowsModelFog.SetFogFade(Fade, Alpha);
  ShadowsModelFog.SetAllowTransparency(AllowTransparency);
  ShadowsModelFog.UploadChangedUniforms();

  if (gl_gpu_debug_models) p_glDebugLogf("DrawAliasModelFog <%s:%d>", *Mdl->Name, Mdl->MeshIndex);

  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mdl->VertsBuffer);

  p_glVertexAttribPointerARB(ShadowsModelFog.loc_Position, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->VertsOffset);
  p_glEnableVertexAttribArrayARB(ShadowsModelFog.loc_Position);

  p_glVertexAttribPointerARB(ShadowsModelFog.loc_Vert2, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->VertsOffset);
  p_glEnableVertexAttribArrayARB(ShadowsModelFog.loc_Vert2);

  p_glVertexAttribPointerARB(ShadowsModelFog.loc_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, 0);
  p_glEnableVertexAttribArrayARB(ShadowsModelFog.loc_TexCoord);

  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, Mdl->IndexBuffer);
  p_glDrawRangeElements(GL_TRIANGLES, 0, Mdl->STVerts.length()-1, Mdl->Tris.length()*3, GL_UNSIGNED_SHORT, 0);
  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

  p_glDisableVertexAttribArrayARB(ShadowsModelFog.loc_Position);
  p_glDisableVertexAttribArrayARB(ShadowsModelFog.loc_Vert2);
  p_glDisableVertexAttribArrayARB(ShadowsModelFog.loc_TexCoord);
  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

  if (gl_gpu_debug_models) p_glDebugLogf("done DrawAliasModelFog <%s:%d>", *Mdl->Name, Mdl->MeshIndex);
}
