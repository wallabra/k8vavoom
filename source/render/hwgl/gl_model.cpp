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
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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


static VCvarB gl_dbg_adv_render_textures_models("gl_dbg_adv_render_textures_models", true, "Render model textures in advanced renderer?", 0);
static VCvarB gl_dbg_adv_render_ambient_models("gl_dbg_adv_render_ambient_models", true, "Render model ambient light in advanced renderer?", 0);
static VCvarB gl_dbg_adv_render_light_models("gl_dbg_adv_render_light_models", true, "Render model dynamic light in advanced renderer?", 0);
static VCvarB gl_dbg_adv_render_alias_models("gl_dbg_adv_render_alias_models", true, "Render alias models?", 0);
static VCvarB gl_dbg_adv_render_shadow_models("gl_dbg_adv_render_shadow_models", true, "Render model shadow volumes?", 0);
static VCvarB gl_dbg_adv_render_fog_models("gl_dbg_adv_render_fog_models", true, "Render model fog?", 0);


/*
{
  GLint unicount;
  p_glGetProgramiv(SurfModel.prog, GL_ACTIVE_UNIFORMS, &unicount);
  GCon->Logf("=== active uniforms: %d ===", unicount);
  for (int f = 0; f < unicount; ++f) {
    char name[1024];
    GLint size;
    GLenum type;
    p_glGetActiveUniform(SurfModel.prog, (unsigned)f, sizeof(name), nullptr, &size, &type, name);
    GCon->Logf("  %d: <%s> (%d : %s)", f, name, size, glTypeName(type));
  }
}
*/


//==========================================================================
//
//  AliasSetUpTransform
//
//==========================================================================
static void AliasSetUpTransform (const TVec &modelorg, const TAVec &angles,
                                 const TVec &Offset, const TVec &Scale,
                                 VMatrix4 &RotationMatrix)
{
  VMatrix4 t3matrix = VMatrix4::Identity;
  t3matrix[0][0] = Scale.x;
  t3matrix[1][1] = Scale.y;
  t3matrix[2][2] = Scale.z;

  t3matrix[0][3] = Scale.x*Offset.x;
  t3matrix[1][3] = Scale.y*Offset.y;
  t3matrix[2][3] = Scale.z*Offset.z;

  TVec alias_forward, alias_right, alias_up;
  AngleVectors(angles, alias_forward, alias_right, alias_up);

  VMatrix4 t2matrix = VMatrix4::Identity;
  for (unsigned i = 0; i < 3; ++i) {
    t2matrix[i][0] = alias_forward[i];
    t2matrix[i][1] = -alias_right[i];
    t2matrix[i][2] = alias_up[i];
  }

  t2matrix[0][3] = modelorg[0];
  t2matrix[1][3] = modelorg[1];
  t2matrix[2][3] = modelorg[2];

  RotationMatrix = t2matrix*t3matrix;
}


//==========================================================================
//
//  AliasSetUpNormalTransform
//
//==========================================================================
static void AliasSetUpNormalTransform (const TAVec &angles, const TVec &Scale, VMatrix4 &RotationMatrix) {
  TVec alias_forward(0, 0, 0), alias_right(0, 0, 0), alias_up(0, 0, 0);
  AngleVectors(angles, alias_forward, alias_right, alias_up);

  VMatrix4 t3matrix = VMatrix4::Identity;
  t3matrix[0][0] = Scale.x;
  t3matrix[1][1] = Scale.y;
  t3matrix[2][2] = Scale.z;

  VMatrix4 t2matrix = VMatrix4::Identity;
  for (int i = 0; i < 3; ++i) {
    t2matrix[i][0] = alias_forward[i];
    t2matrix[i][1] = -alias_right[i];
    t2matrix[i][2] = alias_up[i];
  }

  RotationMatrix = t2matrix*t3matrix;

  if (fabsf(Scale.x) != fabsf(Scale.y) || fabsf(Scale.x) != fabsf(Scale.z)) {
    // non-uniform scale, do full inverse transpose
    RotationMatrix = RotationMatrix.Inverse().Transpose();
  }
}


//==========================================================================
//
//  VOpenGLDrawer::UploadModel
//
//==========================================================================
void VOpenGLDrawer::UploadModel (VMeshModel *Mdl) {
  if (Mdl->Uploaded) return;

  // create buffer
  p_glGenBuffersARB(1, &Mdl->VertsBuffer);
  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mdl->VertsBuffer);

  int Size = sizeof(VMeshSTVert)*Mdl->STVerts.length()+sizeof(TVec)*Mdl->STVerts.length()*2*Mdl->Frames.length();
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

  // vertex indicies
  p_glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 6*Mdl->Tris.length(), &Mdl->Tris[0], GL_STATIC_DRAW_ARB);

  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
  Mdl->Uploaded = true;
  UploadedModels.Append(Mdl);
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
//==========================================================================
void VOpenGLDrawer::DrawAliasModel (const TVec &origin, const TAVec &angles,
                                    const TVec &Offset, const TVec &Scale,
                                    VMeshModel *Mdl, int frame, int nextframe,
                                    VTexture *Skin, VTextureTranslation *Trans, int CMap,
                                    vuint32 light, vuint32 Fade, float Alpha, bool Additive,
                                    bool is_view_model, float Inter, bool Interpolate,
                                    bool ForceDepthUse, bool AllowTransparency, bool onlyDepth)
{
  if (is_view_model) {
    // hack the depth range to prevent view model from poking into walls
    if (CanUseRevZ()) glDepthRange(0.7f, 1.0f); else glDepthRange(0.0f, 0.3f);
  }

  if (!gl_dbg_adv_render_alias_models) return;

  UploadModel(Mdl);

  //if (onlyDepth) glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  //if (onlyDepth) glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  GLint oldDepthMask;
  glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);

  SetPicModel(Skin, Trans, CMap);

  glEnable(GL_ALPHA_TEST);
  glShadeModel(GL_SMOOTH);
  glAlphaFunc(GL_GREATER, 0.0f);
  glEnable(GL_BLEND);


  /*
  glPushMatrix();
  glTranslatef(origin.x, origin.y, origin.z);

  glRotatef(angles.yaw, 0, 0, 1);
  glRotatef(angles.pitch, 0, 1, 0);
  glRotatef(angles.roll, 1, 0, 0);

  glScalef(Scale.x, Scale.y, Scale.z);
  glTranslatef(Offset.x, Offset.y, Offset.z);
  */

  VMatrix4 RotationMatrix;
  AliasSetUpTransform(origin, angles, Offset, Scale, RotationMatrix);

  SurfModel.Activate();
  SurfModel.SetTexture(0);
  //SurfModel.SetFogType();
  SurfModel.SetModelToWorldMat(RotationMatrix);
  SurfModel.SetFogFade(Fade, Alpha);
  SurfModel.SetInAlpha(Alpha < 1.0f ? Alpha : 1.0f);
  SurfModel.SetAllowTransparency(AllowTransparency);
  SurfModel.SetInter(Inter);

  if (Additive) {
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  } else {
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  }

  {
    VMeshFrame *FrameDesc = &Mdl->Frames[frame];
    VMeshFrame *NextFrameDesc = &Mdl->Frames[nextframe];

    p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mdl->VertsBuffer);

    p_glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->VertsOffset);
    p_glEnableVertexAttribArrayARB(0);

    p_glVertexAttribPointerARB(SurfModel.loc_Vert2, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->VertsOffset);
    p_glEnableVertexAttribArrayARB(SurfModel.loc_Vert2);

    p_glVertexAttribPointerARB(SurfModel.loc_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, 0);
    p_glEnableVertexAttribArrayARB(SurfModel.loc_TexCoord);

    //SurfModel.SetViewOrigin(vieworg);
    SurfModel.SetLightVal(((light>>16)&255)/255.0f, ((light>>8)&255)/255.0f, (light&255)/255.0f, Alpha);

    p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, Mdl->IndexBuffer);

    //bool turnOnDepthMask = false;
    if ((Alpha < 1.0f && !ForceDepthUse) || AllowTransparency) { //k8: dunno. really.
      glDepthMask(GL_FALSE);
      //turnOnDepthMask = true;
    }

    /*
    if (onlyDepth) {
      glDepthMask(GL_TRUE);
      glDepthMask(GL_FALSE);
      glDisable(GL_BLEND);
    }
    */
    p_glDrawRangeElementsEXT(GL_TRIANGLES, 0, Mdl->STVerts.length()-1, Mdl->Tris.length()*3, GL_UNSIGNED_SHORT, 0);

    //if (turnOnDepthMask) glDepthMask(GL_TRUE);
    p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

    p_glDisableVertexAttribArrayARB(0);
    p_glDisableVertexAttribArrayARB(SurfModel.loc_Vert2);
    p_glDisableVertexAttribArrayARB(SurfModel.loc_TexCoord);
    p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
  }

  glDisable(GL_BLEND);
  glShadeModel(GL_FLAT);
  glAlphaFunc(GL_GREATER, getAlphaThreshold());
  glDisable(GL_ALPHA_TEST);
  if (Additive) {
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  }

  //glPopMatrix();
  if (is_view_model) glDepthRange(0.0f, 1.0f);

  if (onlyDepth) glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  /*
  if (onlyDepth) {
    glDepthMask(GL_TRUE);
    glEnable(GL_BLEND);
  }
  */
  glDepthMask(oldDepthMask);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawAliasModelAmbient
//
//==========================================================================
void VOpenGLDrawer::DrawAliasModelAmbient (const TVec &origin, const TAVec &angles,
                                           const TVec &Offset, const TVec &Scale,
                                           VMeshModel *Mdl, int frame, int nextframe,
                                           VTexture *Skin, vuint32 light, float Alpha,
                                           float Inter, bool Interpolate,
                                           bool ForceDepth, bool AllowTransparency)
{
  VMeshFrame *FrameDesc = &Mdl->Frames[frame];
  VMeshFrame *NextFrameDesc = &Mdl->Frames[nextframe];

  SetPicModel(Skin, nullptr, CM_Default);

  if (!gl_dbg_adv_render_ambient_models) return;

  UploadModel(Mdl);

  //GCon->Logf("  amb: origin=(%f,%f,%f); offset=(%f,%f,%f)", origin.x, origin.y, origin.z, Offset.x, Offset.y, Offset.z);

  GLint oldDepthMask;
  glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);

  VMatrix4 RotationMatrix;
  AliasSetUpTransform(origin, angles, Offset, Scale, RotationMatrix);
  VMatrix4 normalmatrix;
  AliasSetUpNormalTransform(angles, Scale, normalmatrix);

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

  ShadowsModelAmbient.Activate();
  ShadowsModelAmbient.SetTexture(0);
  ShadowsModelAmbient.SetInter(Inter);
  ShadowsModelAmbient.SetLight(((light>>16)&255)/255.0f, ((light>>8)&255)/255.0f, (light&255)/255.0f, Alpha);
  ShadowsModelAmbient.SetModelToWorldMat(RotationMatrix);
  ShadowsModelAmbient.SetNormalToWorldMat(NormalMat[0]);

  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mdl->VertsBuffer);

  p_glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->VertsOffset);
  p_glEnableVertexAttribArrayARB(0);

  p_glVertexAttribPointerARB(ShadowsModelAmbient.loc_VertNormal, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->NormalsOffset);
  p_glEnableVertexAttribArrayARB(ShadowsModelAmbient.loc_VertNormal);

  p_glVertexAttribPointerARB(ShadowsModelAmbient.loc_Vert2, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->VertsOffset);
  p_glEnableVertexAttribArrayARB(ShadowsModelAmbient.loc_Vert2);

  p_glVertexAttribPointerARB(ShadowsModelAmbient.loc_Vert2Normal, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->NormalsOffset);
  p_glEnableVertexAttribArrayARB(ShadowsModelAmbient.loc_Vert2Normal);

  p_glVertexAttribPointerARB(ShadowsModelAmbient.loc_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, 0);
  p_glEnableVertexAttribArrayARB(ShadowsModelAmbient.loc_TexCoord);

  ShadowsModelAmbient.SetViewOrigin(vieworg);
  ShadowsModelAmbient.SetInAlpha(Alpha < 1.0f ? Alpha : 1.0f);
  ShadowsModelAmbient.SetAllowTransparency(false);

  glEnable(GL_ALPHA_TEST);
  glShadeModel(GL_SMOOTH);
  glAlphaFunc(GL_GREATER, 0.0f);
  glEnable(GL_BLEND);
  if (Alpha < 1.0f && !ForceDepth) glDepthMask(GL_FALSE);

  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, Mdl->IndexBuffer);

  p_glDrawRangeElementsEXT(GL_TRIANGLES, 0, Mdl->STVerts.length()-1, Mdl->Tris.length()*3, GL_UNSIGNED_SHORT, 0);

  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

  p_glDisableVertexAttribArrayARB(0);
  p_glDisableVertexAttribArrayARB(ShadowsModelAmbient.loc_VertNormal);
  p_glDisableVertexAttribArrayARB(ShadowsModelAmbient.loc_Vert2);
  p_glDisableVertexAttribArrayARB(ShadowsModelAmbient.loc_Vert2Normal);
  p_glDisableVertexAttribArrayARB(ShadowsModelAmbient.loc_TexCoord);
  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

  if (Alpha < 1.0f && !ForceDepth) glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glAlphaFunc(GL_GREATER, getAlphaThreshold());
  glShadeModel(GL_FLAT);
  glDisable(GL_ALPHA_TEST);

  glDepthMask(oldDepthMask);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawAliasModelTextures
//
//==========================================================================
void VOpenGLDrawer::DrawAliasModelTextures (const TVec &origin, const TAVec &angles,
                                            const TVec &Offset, const TVec &Scale,
                                            VMeshModel *Mdl, int frame, int nextframe,
                                            VTexture *Skin, VTextureTranslation *Trans, int CMap,
                                            float Alpha, float Inter,
                                            bool Interpolate, bool ForceDepth, bool AllowTransparency)
{
  VMeshFrame *FrameDesc = &Mdl->Frames[frame];
  VMeshFrame *NextFrameDesc = &Mdl->Frames[nextframe];

  SetPicModel(Skin, Trans, CMap);

  if (!gl_dbg_adv_render_textures_models) return;

  UploadModel(Mdl);

  VMatrix4 RotationMatrix;
  AliasSetUpTransform(origin, angles, Offset, Scale, RotationMatrix);
  VMatrix4 normalmatrix;
  AliasSetUpNormalTransform(angles, Scale, normalmatrix);

  /*
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
  */

  ShadowsModelTextures.Activate();
  ShadowsModelTextures.SetTexture(0);
  ShadowsModelTextures.SetInter(Inter);
  ShadowsModelTextures.SetModelToWorldMat(RotationMatrix);
  //!ShadowsModelTextures.SetNormalToWorldMat(NormalMat[0]);

  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mdl->VertsBuffer);

  p_glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->VertsOffset);
  p_glEnableVertexAttribArrayARB(0);

  //!p_glVertexAttribPointerARB(ShadowsModelTextures.loc_VertNormal, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->NormalsOffset);
  //!p_glEnableVertexAttribArrayARB(ShadowsModelTextures.loc_VertNormal);

  p_glVertexAttribPointerARB(ShadowsModelTextures.loc_Vert2, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->VertsOffset);
  p_glEnableVertexAttribArrayARB(ShadowsModelTextures.loc_Vert2);

  //!p_glVertexAttribPointerARB(ShadowsModelTextures.loc_Vert2Normal, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->NormalsOffset);
  //!p_glEnableVertexAttribArrayARB(ShadowsModelTextures.loc_Vert2Normal);

  p_glVertexAttribPointerARB(ShadowsModelTextures.loc_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, 0);
  p_glEnableVertexAttribArrayARB(ShadowsModelTextures.loc_TexCoord);

  //ShadowsModelTextures.SetViewOrigin(vieworg);
  ShadowsModelTextures.SetInAlpha(Alpha < 1.0f ? Alpha : 1.0f);
  //!ShadowsModelTextures.SetAllowTransparency(AllowTransparency);


  /* original
  glEnable(GL_ALPHA_TEST);
  glShadeModel(GL_SMOOTH);
  glAlphaFunc(GL_GREATER, 0.0f);
  */

  //glEnable(GL_BLEND);
  //glShadeModel(GL_FLAT);
  //glDisable(GL_BLEND);
  //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  //glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  //glBlendFunc(GL_DST_COLOR, GL_ZERO);

  /*
  glEnable(GL_CULL_FACE);
  glCullFace(GL_FRONT);
  //glCullFace(GL_BACK);
  glEnable(GL_DEPTH_TEST);
  */

  glEnable(GL_BLEND);
  //glShadeModel(GL_SMOOTH);
  glDisable(GL_ALPHA_TEST);
  //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // this was for non-premultiplied
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  //glDisable(GL_BLEND);

  p_glActiveTextureARB(GL_TEXTURE0+1);
  glBindTexture(GL_TEXTURE_2D, ambLightFBOColorTid);
  p_glActiveTextureARB(GL_TEXTURE0);
  ShadowsModelTextures.SetAmbLightTexture(1);
  ShadowsModelTextures.SetScreenSize((float)ScreenWidth, (float)ScreenHeight);

  GLint oldDepthMask;
  glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);

  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, Mdl->IndexBuffer);
  glDepthMask(GL_FALSE);

  p_glDrawRangeElementsEXT(GL_TRIANGLES, 0, Mdl->STVerts.length()-1, Mdl->Tris.length()*3, GL_UNSIGNED_SHORT, 0);
  //glDepthMask(GL_TRUE);
  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

  p_glDisableVertexAttribArrayARB(0);
  //!p_glDisableVertexAttribArrayARB(ShadowsModelTextures.loc_VertNormal);
  p_glDisableVertexAttribArrayARB(ShadowsModelTextures.loc_Vert2);
  //!p_glDisableVertexAttribArrayARB(ShadowsModelTextures.loc_Vert2Normal);
  p_glDisableVertexAttribArrayARB(ShadowsModelTextures.loc_TexCoord);
  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

  p_glActiveTextureARB(GL_TEXTURE0+1);
  glBindTexture(GL_TEXTURE_2D, 0);
  p_glActiveTextureARB(GL_TEXTURE0);

  glDepthMask(oldDepthMask);

  //glShadeModel(GL_FLAT);
  //glAlphaFunc(GL_GREATER, getAlphaThreshold());
  //glDisable(GL_ALPHA_TEST);
  //glEnable(GL_BLEND); // it is already enabled
}


//==========================================================================
//
//  VOpenGLDrawer::BeginModelsLightPass
//
//==========================================================================
void VOpenGLDrawer::BeginModelsLightPass (const TVec &LightPos, float Radius, float LightMin, vuint32 Colour) {
  ShadowsModelLight.Activate();
  ShadowsModelLight.SetTexture(0);
  ShadowsModelLight.SetLightPos(LightPos);
  ShadowsModelLight.SetLightRadius(Radius);
  ShadowsModelLight.SetLightMin(LightMin);
  ShadowsModelLight.SetLightColour(((Colour>>16)&255)/255.0f, ((Colour>>8)&255)/255.0f, (Colour&255)/255.0f);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawAliasModelLight
//
//==========================================================================
void VOpenGLDrawer::DrawAliasModelLight (const TVec &origin, const TAVec &angles,
                                         const TVec &Offset, const TVec &Scale,
                                         VMeshModel *Mdl, int frame, int nextframe,
                                         VTexture *Skin, float Alpha, float Inter,
                                         bool Interpolate, bool AllowTransparency)
{
  VMeshFrame *FrameDesc = &Mdl->Frames[frame];
  VMeshFrame *NextFrameDesc = &Mdl->Frames[nextframe];

  VMatrix4 RotationMatrix;
  AliasSetUpTransform(origin, angles, Offset, Scale, RotationMatrix);
  VMatrix4 normalmatrix;
  AliasSetUpNormalTransform(angles, Scale, normalmatrix);

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

  ShadowsModelLight.SetInter(Inter);
  ShadowsModelLight.SetModelToWorldMat(RotationMatrix);
  ShadowsModelLight.SetNormalToWorldMat(NormalMat[0]);

  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mdl->VertsBuffer);

  p_glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->VertsOffset);
  p_glEnableVertexAttribArrayARB(0);

  p_glVertexAttribPointerARB(ShadowsModelLight.loc_VertNormal, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->NormalsOffset);
  p_glEnableVertexAttribArrayARB(ShadowsModelLight.loc_VertNormal);

  p_glVertexAttribPointerARB(ShadowsModelLight.loc_Vert2, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->VertsOffset);
  p_glEnableVertexAttribArrayARB(ShadowsModelLight.loc_Vert2);

  p_glVertexAttribPointerARB(ShadowsModelLight.loc_Vert2Normal, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->NormalsOffset);
  p_glEnableVertexAttribArrayARB(ShadowsModelLight.loc_Vert2Normal);

  p_glVertexAttribPointerARB(ShadowsModelLight.loc_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, 0);
  p_glEnableVertexAttribArrayARB(ShadowsModelLight.loc_TexCoord);

  ShadowsModelLight.SetInAlpha(Alpha < 1.0f ? Alpha : 1.0f);
  ShadowsModelLight.SetAllowTransparency(AllowTransparency);
  ShadowsModelLight.SetViewOrigin(vieworg);

  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, Mdl->IndexBuffer);
  p_glDrawRangeElementsEXT(GL_TRIANGLES, 0, Mdl->STVerts.length()-1, Mdl->Tris.length()*3, GL_UNSIGNED_SHORT, 0);
  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

  p_glDisableVertexAttribArrayARB(0);
  p_glDisableVertexAttribArrayARB(ShadowsModelLight.loc_VertNormal);
  p_glDisableVertexAttribArrayARB(ShadowsModelLight.loc_Vert2);
  p_glDisableVertexAttribArrayARB(ShadowsModelLight.loc_Vert2Normal);
  p_glDisableVertexAttribArrayARB(ShadowsModelLight.loc_TexCoord);
  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}


//==========================================================================
//
//  VOpenGLDrawer::BeginModelsShadowsPass
//
//==========================================================================
void VOpenGLDrawer::BeginModelsShadowsPass (TVec &LightPos, float LightRadius) {
  ShadowsModelShadow.Activate();
  ShadowsModelShadow.SetLightPos(LightPos);
}


#define outv(idx, offs) \
      ShadowsModelShadow.SetOffset(offs); \
      glArrayElement(index ## idx);

//==========================================================================
//
//  VOpenGLDrawer::DrawAliasModelShadow
//
//==========================================================================
void VOpenGLDrawer::DrawAliasModelShadow (const TVec &origin, const TAVec &angles,
                                          const TVec &Offset, const TVec &Scale,
                                          VMeshModel *Mdl, int frame, int nextframe,
                                          float Inter, bool Interpolate,
                                          const TVec &LightPos, float LightRadius)
{
  VMeshFrame *FrameDesc = &Mdl->Frames[frame];
  VMeshFrame *NextFrameDesc = &Mdl->Frames[nextframe];

  if (!gl_dbg_adv_render_shadow_models) return;

  UploadModel(Mdl);

  VMatrix4 RotationMatrix;
  AliasSetUpTransform(origin, angles, Offset, Scale, RotationMatrix);

  //VMatrix4 InvRotationMatrix = RotationMatrix.Inverse();
  VMatrix4 InvRotationMatrix = RotationMatrix;
  InvRotationMatrix.invert();
  //TVec LocalLightPos = InvRotationMatrix.Transform(LightPos);
  TVec LocalLightPos = LightPos*InvRotationMatrix;
  //TVec LocalLightPos = RotationMatrix*(LightPos-origin)+LightPos;

  //TArray<bool> PlaneSides;
  //PlaneSides.SetNum(Mdl->Tris.Num());
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

  ShadowsModelShadow.SetInter(Inter);
  ShadowsModelShadow.SetModelToWorldMat(RotationMatrix);

  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mdl->VertsBuffer);
  p_glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->VertsOffset);
  p_glEnableVertexAttribArrayARB(0);
  p_glVertexAttribPointerARB(ShadowsModelShadow.loc_Vert2, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->VertsOffset);
  p_glEnableVertexAttribArrayARB(ShadowsModelShadow.loc_Vert2);

  //const float Shadow_Offset = M_INFINITY;

  // caps
  if (!usingZPass && !gl_dbg_use_zpass) {
    glBegin(GL_TRIANGLES);
    //p_glVertexAttrib1fARB(ShadowsModelShadow_OffsetLoc, 1.0f);
    for (int i = 0; i < Mdl->Tris.length(); ++i) {
      if (PlaneSides[i]) {
        ShadowsModelShadow.SetOffset(1.0f);
        glArrayElement(Mdl->Tris[i].VertIndex[0]);
        ShadowsModelShadow.SetOffset(1.0f);
        glArrayElement(Mdl->Tris[i].VertIndex[1]);
        ShadowsModelShadow.SetOffset(1.0f);
        glArrayElement(Mdl->Tris[i].VertIndex[2]);
      }
    }

    //p_glVertexAttrib1fARB(ShadowsModelShadow_OffsetLoc, 0.0f);
    for (int i = 0; i < Mdl->Tris.length(); ++i) {
      if (PlaneSides[i]) {
        ShadowsModelShadow.SetOffset(0.0f);
        glArrayElement(Mdl->Tris[i].VertIndex[2]);
        ShadowsModelShadow.SetOffset(0.0f);
        glArrayElement(Mdl->Tris[i].VertIndex[1]);
        ShadowsModelShadow.SetOffset(0.0f);
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
  //ShadowsModelShadow.SetViewOrigin(vieworg);

  p_glDisableVertexAttribArrayARB(0);
  p_glDisableVertexAttribArrayARB(ShadowsModelShadow.loc_Vert2);
  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}

#undef outv


//==========================================================================
//
//  VOpenGLDrawer::DrawAliasModelFog
//
//==========================================================================
void VOpenGLDrawer::DrawAliasModelFog (const TVec &origin, const TAVec &angles,
                                       const TVec &Offset, const TVec &Scale,
                                       VMeshModel *Mdl, int frame, int nextframe,
                                       VTexture *Skin, vuint32 Fade, float Alpha, float Inter,
                                       bool Interpolate, bool AllowTransparency)
{
  VMeshFrame *FrameDesc = &Mdl->Frames[frame];
  VMeshFrame *NextFrameDesc = &Mdl->Frames[nextframe];

  if (!gl_dbg_adv_render_fog_models) return;

  UploadModel(Mdl);

  SetPicModel(Skin, nullptr, CM_Default);

  GLint oldDepthMask;
  glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);

  VMatrix4 RotationMatrix;
  AliasSetUpTransform(origin, angles, Offset, Scale, RotationMatrix);

  ShadowsModelFog.Activate();
  ShadowsModelFog.SetTexture(0);
  ShadowsModelFog.SetInter(Inter);
  ShadowsModelFog.SetModelToWorldMat(RotationMatrix);
  ShadowsModelFog.SetFogFade(Fade, Alpha);

  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mdl->VertsBuffer);

  p_glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->VertsOffset);
  p_glEnableVertexAttribArrayARB(0);

  p_glVertexAttribPointerARB(ShadowsModelFog.loc_Vert2, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->VertsOffset);
  p_glEnableVertexAttribArrayARB(ShadowsModelFog.loc_Vert2);

  p_glVertexAttribPointerARB(ShadowsModelFog.loc_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, 0);
  p_glEnableVertexAttribArrayARB(ShadowsModelFog.loc_TexCoord);

  ShadowsModelFog.SetViewOrigin(vieworg);
  ShadowsModelFog.SetInAlpha(Alpha < 1.0f ? Alpha : 1.0f);
  ShadowsModelFog.SetAllowTransparency(AllowTransparency);

  glEnable(GL_ALPHA_TEST);
  glShadeModel(GL_SMOOTH);
  glAlphaFunc(GL_GREATER, 0.0f);
  glEnable(GL_BLEND);

  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, Mdl->IndexBuffer);
  glDepthMask(GL_FALSE);
  p_glDrawRangeElementsEXT(GL_TRIANGLES, 0, Mdl->STVerts.length()-1, Mdl->Tris.length()*3, GL_UNSIGNED_SHORT, 0);
  //glDepthMask(GL_TRUE);
  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

  p_glDisableVertexAttribArrayARB(0);
  p_glDisableVertexAttribArrayARB(ShadowsModelFog.loc_Vert2);
  p_glDisableVertexAttribArrayARB(ShadowsModelFog.loc_TexCoord);
  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

  glDisable(GL_BLEND);
  glAlphaFunc(GL_GREATER, getAlphaThreshold());
  glShadeModel(GL_FLAT);
  glDisable(GL_ALPHA_TEST);

  glDepthMask(oldDepthMask);
}
