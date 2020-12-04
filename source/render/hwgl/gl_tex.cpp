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


extern VCvarB r_allow_cameras;
extern VCvarB gl_pic_filtering;
static VCvarB gl_recreate_changed_textures("gl_recreate_changed_textures", false, "Destroy and create new OpenGL textures for changed DooM animated ones?", CVAR_Archive);
static VCvarB gl_camera_texture_use_readpixels("gl_camera_texture_use_readpixels", true, "Use ReadPixels to update camera textures?", CVAR_Archive);

//static VCvarB r_gozzo_fucked_black("__r_gozzo_fucked_black", false, "Emulate fucked-up gozzo idea about black colors in non-translucent textures?", CVAR_PreInit);


//==========================================================================
//
//  VOpenGLDrawer::GenerateLightmapAtlasTextures
//
//==========================================================================
void VOpenGLDrawer::GenerateLightmapAtlasTextures () {
  vassert(!atlasesGenerated);
  glGenTextures(NUM_BLOCK_SURFS, lmap_id);
  glGenTextures(NUM_BLOCK_SURFS, addmap_id);
  atlasesGenerated = true;
  memset(atlases_updated, 0, sizeof(atlases_updated));
  //GCon->Log(NAME_Debug, "******** VOpenGLDrawer::GenerateLightmapAtlasTextures ********");
}


//==========================================================================
//
//  VOpenGLDrawer::DeleteLightmapAtlases
//
//==========================================================================
void VOpenGLDrawer::DeleteLightmapAtlases () {
  if (atlasesGenerated) {
    glDeleteTextures(NUM_BLOCK_SURFS, lmap_id);
    glDeleteTextures(NUM_BLOCK_SURFS, addmap_id);
    atlasesGenerated = false;
    memset(lmap_id, 0, sizeof(lmap_id));
    memset(addmap_id, 0, sizeof(addmap_id));
    memset(atlases_updated, 0, sizeof(atlases_updated));
    //GCon->Log(NAME_Debug, "******** VOpenGLDrawer::DeleteLightmapAtlases ********");
  }
}


//==========================================================================
//
//  VOpenGLDrawer::FlushTextures
//
//==========================================================================
void VOpenGLDrawer::FlushTextures (bool forced) {
  for (int i = 0; i < GTextureManager.GetNumTextures(); ++i) {
    VTexture *tex = GTextureManager.getIgnoreAnim(i);
    if (tex) {
      if (forced) DeleteTexture(tex); else FlushTexture(tex);
      tex->lastUpdateFrame = 0;
    }
  }
  for (int i = 0; i < GTextureManager.GetNumMapTextures(); ++i) {
    VTexture *tex = GTextureManager.getMapTexIgnoreAnim(i);
    if (tex) {
      if (forced) DeleteTexture(tex); else FlushTexture(tex);
      tex->lastUpdateFrame = 0;
    }
  }
}


//==========================================================================
//
//  VOpenGLDrawer::FlushOneTexture
//
//==========================================================================
void VOpenGLDrawer::FlushOneTexture (VTexture *tex, bool forced) {
  if (!tex) return;
  if (forced) DeleteTexture(tex); else FlushTexture(tex);
  tex->lastUpdateFrame = 0;
}


//==========================================================================
//
//  VOpenGLDrawer::DeleteTextures
//
//  called on drawer shutdown
//
//==========================================================================
void VOpenGLDrawer::DeleteTextures () {
  FlushTextures(true); // forced
  DeleteLightmapAtlases();
  UnloadModels();
}


//==========================================================================
//
//  VOpenGLDrawer::FlushTexture
//
//==========================================================================
void VOpenGLDrawer::FlushTexture (VTexture *Tex) {
  if (!Tex) return;
  if (Tex->DriverHandle) {
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, (GLuint *)&Tex->DriverHandle);
    Tex->DriverHandle = 0;
    Tex->lastTextureFiltering = -1;
  }
  for (auto &&it : Tex->DriverTranslated) {
    if (it.Handle) {
      glBindTexture(GL_TEXTURE_2D, 0);
      glDeleteTextures(1, (GLuint *)&it.Handle);
    }
  }
  Tex->ResetTranslations();
  Tex->lastUpdateFrame = 0;
  if (Tex->Brightmap) FlushTexture(Tex->Brightmap);
}


//==========================================================================
//
//  VOpenGLDrawer::DeleteTexture
//
//==========================================================================
void VOpenGLDrawer::DeleteTexture (VTexture *Tex) {
  if (!Tex) return;
  if (Tex->DriverHandle) {
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, (GLuint *)&Tex->DriverHandle);
    Tex->DriverHandle = 0;
    Tex->lastTextureFiltering = -1;
  }
  for (auto &&it : Tex->DriverTranslated) {
    if (it.Handle) {
      glBindTexture(GL_TEXTURE_2D, 0);
      glDeleteTextures(1, (GLuint *)&it.Handle);
    }
  }
  Tex->ClearTranslations();
  Tex->lastUpdateFrame = 0;
  if (Tex->Brightmap) DeleteTexture(Tex->Brightmap);
}


//==========================================================================
//
//  VOpenGLDrawer::PrecacheTexture
//
//==========================================================================
void VOpenGLDrawer::PrecacheTexture (VTexture *Tex) {
  if (!Tex) return;
  if (Tex->bIsCameraTexture) return;
  ResetTextureUpdateFrames();
  SetTexture(Tex, 0);
  if (Tex->Brightmap) SetBrightmapTexture(Tex->Brightmap);
}


//==========================================================================
//
//  VOpenGLDrawer::SetBrightmapTexture
//
//==========================================================================
void VOpenGLDrawer::SetBrightmapTexture (VTexture *Tex) {
  if (!Tex || /*Tex->Type == TEXTYPE_Null ||*/ Tex->Width < 1 || Tex->Height < 1) return;
  if (Tex->bIsCameraTexture) return;
  //SetTexture(Tex, 0); // default colormap
  (void)SetSpriteLump(Tex, nullptr, 0, false, 0u);
  /*
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  */
  // `asModel` sets GL_REPEAT
  SetupTextureFiltering(Tex, (r_brightmaps_filter ? 4 : 0), TexWrapRepeat); // trilinear or none
}


//==========================================================================
//
//  VOpenGLDrawer::SetTexture
//
//  returns `false` if non-main texture was bound
//
//==========================================================================
bool VOpenGLDrawer::SetTexture (VTexture *Tex, int CMap, vuint32 ShadeColor) {
  if (!Tex) Sys_Error("cannot set null texture");
  // camera textures are special
  const bool res = SetSpriteLump(Tex, nullptr, CMap, false, ShadeColor);
  SetOrForceTextureFiltering(res, Tex, texture_filter);
  return res;
}


//==========================================================================
//
//  VOpenGLDrawer::SetDecalTexture
//
//  returns `false` if non-main texture was bound
//
//==========================================================================
bool VOpenGLDrawer::SetDecalTexture (VTexture *Tex, VTextureTranslation *Translation, int CMap, vuint32 ShadeColor) {
  if (!Tex) Sys_Error("cannot set null texture");
  const bool res = SetSpriteLump(Tex, Translation, CMap, false, ShadeColor);
  SetOrForceTextureFiltering(res, Tex, texture_filter);
  return res;
}


//==========================================================================
//
//  VOpenGLDrawer::SetPic
//
//  returns `false` if non-main texture was bound
//
//==========================================================================
bool VOpenGLDrawer::SetPic (VTexture *Tex, VTextureTranslation *Trans, int CMap, vuint32 ShadeColor) {
  const bool res = SetSpriteLump(Tex, Trans, CMap, true, ShadeColor);
  const int oldAniso = gl_texture_filter_anisotropic.asInt();
  gl_texture_filter_anisotropic.Set(1);
  SetOrForceTextureFiltering(res, Tex, (gl_pic_filtering ? 3 : 0), TexWrapClamp);
  gl_texture_filter_anisotropic.Set(oldAniso);
  /*
  int flt = (gl_pic_filtering ? GL_LINEAR : GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, flt);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, flt);
  if (anisotropyExists) glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), 1.0f);
  */
  return res;
}


//==========================================================================
//
//  VOpenGLDrawer::SetPicModel
//
//  returns `false` if non-main texture was bound
//
//==========================================================================
bool VOpenGLDrawer::SetPicModel (VTexture *Tex, VTextureTranslation *Trans, int CMap, vuint32 ShadeColor) {
  const bool res = SetSpriteLump(Tex, Trans, CMap, false, ShadeColor);
  SetOrForceTextureFiltering(res, Tex, model_filter, TexWrapRepeat);
  /*
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  */
  return res;
}


//==========================================================================
//
//  VOpenGLDrawer::SetSpriteLump
//
//  returns `false` if non-main texture was bound
//
//==========================================================================
bool VOpenGLDrawer::SetSpriteLump (VTexture *Tex, VTextureTranslation *Translation, int CMap, bool asPicture, vuint32 ShadeColor) {
  vassert(Tex);
  bool res = true;
  if (mInitialized) {
    if (ShadeColor) {
      Translation = nullptr; // just in case
      ShadeColor |= 0xff000000u; // normalise it
    }
    bool needUp = false;
    if (Tex->lastUpdateFrame != updateFrame && Tex->CheckModified()) {
      //GCon->Logf(NAME_Debug, "texture '%s' needs update! (%u : %u)", *Tex->Name, Tex->lastUpdateFrame, updateFrame);
      if (gl_recreate_changed_textures) FlushTexture(Tex);
      needUp = true;
    }
    Tex->lastUpdateFrame = updateFrame;
    if (Translation || CMap || ShadeColor) {
      // color translation, color map, or stenciled
      // find translation, and mark it as recently used
      //FIXME!
      res = false;
      VTexture::VTransData *TData = (ShadeColor ? Tex->FindDriverShaded(ShadeColor, CMap, true) : Tex->FindDriverTrans(Translation, CMap, true));
      if (TData) {
        if (needUp || !TData->Handle || Tex->bIsCameraTexture) {
          GenerateTexture(Tex, &TData->Handle, Translation, CMap, asPicture, needUp, ShadeColor);
        } else {
          glBindTexture(GL_TEXTURE_2D, TData->Handle);
        }
      } else {
        // drop some old translations
        const vint32 ctime = Tex->GetTranslationCTime();
        if (ctime >= 0) {
          for (int f = 0; f < Tex->DriverTranslated.length(); ++f) {
            TData = &Tex->DriverTranslated[f];
            const vint32 ttime = TData->lastUseTime;
            // keep it for six seconds, and then drop it
            if (ttime >= ctime || ctime-ttime < 35*6) continue;
            // drop it
            if (TData->Handle) glDeleteTextures(1, (GLuint *)&TData->Handle);
            Tex->ClearTranslationAt(f);
            Tex->DriverTranslated.removeAt(f);
            --f;
          }
        }
        // new translation (insert it at the top of the list, why not)
        VTexture::VTransData newtrans;
        newtrans.wipe();
        newtrans.Handle = 0;
        newtrans.Trans = Translation;
        newtrans.ColorMap = CMap;
        newtrans.ShadeColor = ShadeColor;
        if (ctime >= 0) newtrans.lastUseTime = ctime;
        Tex->DriverTranslated.insert(0, newtrans);
        TData = &Tex->DriverTranslated[0];
        GenerateTexture(Tex, &TData->Handle, Translation, CMap, asPicture, needUp, ShadeColor);
      }
    } else if (!Tex->DriverHandle || needUp || Tex->bIsCameraTexture) {
      // generate and bind new texture
      GenerateTexture(Tex, &Tex->DriverHandle, nullptr, 0, asPicture, needUp, ShadeColor);
    } else {
      // existing normal texture
      glBindTexture(GL_TEXTURE_2D, Tex->DriverHandle);
    }
  }

  tex_w = max2(1, Tex->GetWidth());
  tex_h = max2(1, Tex->GetHeight());
  tex_iw = 1.0f/tex_w;
  tex_ih = 1.0f/tex_h;

  tex_w_sc = max2(1, Tex->GetScaledWidth());
  tex_h_sc = max2(1, Tex->GetScaledHeight());
  tex_iw_sc = 1.0f/tex_w_sc;
  tex_ih_sc = 1.0f/tex_h_sc;

  tex_scale_x = (Tex->SScale ? Tex->SScale : 1.0f);
  tex_scale_y = (Tex->TScale ? Tex->TScale : 1.0f);

  return res;
}


//==========================================================================
//
//  VOpenGLDrawer::GenerateTexture
//
//  TODO: remove some old translations if they weren't used for a long time
//
//==========================================================================
void VOpenGLDrawer::GenerateTexture (VTexture *Tex, GLuint *pHandle, VTextureTranslation *Translation, int CMap, bool asPicture, bool needUpdate, vuint32 ShadeColor) {
  // special handling for camera textures
  bool isCamTexture = Tex->bIsCameraTexture;

  if (isCamTexture) {
    VCameraTexture *CamTex = (VCameraTexture *)Tex;
    if (r_allow_cameras) {
      CamTex->bUsedInFrame = true;
      GLuint tid = GetCameraFBOTextureId(CamTex->camfboidx);
      if (!tid || Translation || CMap || currMainFBO == CamTex->camfboidx || gl_camera_texture_use_readpixels) {
        // game can append new cameras dynamically, and FBO can be unready yet
        if (CamTex->camfboidx >= 0) {
          // copy texture pixels for translations (or if user requested it)
          if (!CamTex->bPixelsLoaded) {
            // read FBO data
            //vassert(CamTex->camfboidx >= 0);
            if (CamTex->camfboidx < cameraFBOList.length()) {
              CamTex->bPixelsLoaded = true;
              CameraFBOInfo *cfi = cameraFBOList[CamTex->camfboidx];
              rgba_t *px = (rgba_t *)Tex->GetPixels();
              ReadFBOPixels(&cfi->fbo, Tex->Width, Tex->Height, px);
            } else {
              GCon->Logf(NAME_Error, "THE THING THAT SHOULD NOT BE: camera FBO array is too small! (want %d, size is %d)", CamTex->camfboidx+1, cameraFBOList.length());
            }
          }
        } else {
          //GCon->Logf(NAME_Debug, "trying to select unintialized camera texture (this is harmeless)");
        }
        isCamTexture = false;
      }
    } else {
      isCamTexture = false;
    }
  }

  // flag can be changed here, so recheck
  if (isCamTexture) {
    // handle camera texture
    VCameraTexture *CamTex = (VCameraTexture *)Tex;
    //FlushTexture(Tex); // remove all created textures
    GCon->Logf(NAME_Debug, "***** camtex; fboindex=%d; len=%d", CamTex->camfboidx, cameraFBOList.length());
    GLuint tid = GetCameraFBOTextureId(CamTex->camfboidx);
    vassert(tid);
    glBindTexture(GL_TEXTURE_2D, tid);
    #if 1
    GLint oldbindtex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldbindtex);
    GCon->Logf(NAME_Debug, "set camera texture; fboidx=%d; fbotid=%u; tid=%d; curtid=%d; size=%dx%d; ofs=(%d,%d); scale=(%g,%g)", CamTex->camfboidx, cameraFBOList[CamTex->camfboidx]->fbo.getColorTid(), tid, oldbindtex, Tex->Width, Tex->Height, Tex->SOffset, Tex->TOffset, Tex->SScale, Tex->TScale);
    #endif
  } else {
    // handle non-camera texture
    if (!*pHandle) glGenTextures(1, pHandle);
    glBindTexture(GL_TEXTURE_2D, *pHandle);
    //GCon->Logf(NAME_Debug, "texture '%s'; tid=%d", *Tex->Name, *pHandle);

    // try to load high resolution version
    VTexture *SrcTex = Tex;
    if (!Tex->bIsCameraTexture) {
      VTexture *hitex = Tex->GetHighResolutionTexture();
      if (hitex && hitex->Type != TEXTYPE_Null) SrcTex = hitex;
    }

    if (SrcTex->Type == TEXTYPE_Null) {
      // fuckin' idiots
      GCon->Logf(NAME_Warning, "something is VERY wrong with textures in this mod (trying to upload null texture '%s')", *SrcTex->Name);
      if (SrcTex->SourceLump >= 0) GCon->Logf(NAME_Warning, "...source lump: %s", *W_FullLumpName(SrcTex->SourceLump));
      rgba_t dummy[4];
      memset((void *)dummy, 0, sizeof(dummy));
      VTexture::checkerFillRGBA((vuint8 *)dummy, 2, 2);
      UploadTexture(2, 2, dummy, false, -1); // no fringe filtering
    } else if (Translation && CMap) {
      // both colormap and translation
      rgba_t tmppal[256];
      const vuint8 *TrTab = Translation->GetTable();
      const rgba_t *CMPal = ColorMaps[CMap].GetPalette();
      for (int i = 0; i < 256; ++i) tmppal[i] = CMPal[TrTab[i]];
      if (needUpdate) (void)SrcTex->GetPixels(); // this updates warp and other textures
      UploadTexture8A(SrcTex->GetWidth(), SrcTex->GetHeight(), SrcTex->GetPixels8A(), tmppal, SrcTex->SourceLump);
    } else if (Translation) {
      // only translation
      //GCon->Logf("uploading translated texture '%s' (%dx%d)", *SrcTex->Name, SrcTex->GetWidth(), SrcTex->GetHeight());
      //for (int f = 0; f < 256; ++f) GCon->Logf("  %3d: r:g:b=%02x:%02x:%02x", f, Translation->GetPalette()[f].r, Translation->GetPalette()[f].g, Translation->GetPalette()[f].b);
      if (needUpdate) (void)SrcTex->GetPixels(); // this updates warp and other textures
      UploadTexture8A(SrcTex->GetWidth(), SrcTex->GetHeight(), SrcTex->GetPixels8A(), Translation->GetPalette(), SrcTex->SourceLump);
    } else if (CMap) {
      // only colormap
      //GCon->Logf(NAME_Dev, "uploading colormapped texture '%s' (%dx%d)", *SrcTex->Name, SrcTex->GetWidth(), SrcTex->GetHeight());
      if (needUpdate) (void)SrcTex->GetPixels(); // this updates warp and other textures
      UploadTexture8A(SrcTex->GetWidth(), SrcTex->GetHeight(), SrcTex->GetPixels8A(), ColorMaps[CMap].GetPalette(), SrcTex->SourceLump);
    } else if (ShadeColor) {
      // shade (and possible colormap)
      //GLog.Logf(NAME_Debug, "*** SHADE: tex='%s'; color=0x%08x", *SrcTex->Name, ShadeColor);
      rgba_t *shadedPixels = SrcTex->CreateShadedPixels(ShadeColor, (CMap ? ColorMaps[CMap].GetPalette() : nullptr));
      UploadTexture(SrcTex->GetWidth(), SrcTex->GetHeight(), shadedPixels, false/*remove fringe*/, -1);
      SrcTex->FreeShadedPixels(shadedPixels);
    } else {
      // normal uploading
      vuint8 *block = SrcTex->GetPixels();
      //if (SrcTex->SourceLump >= 0) GCon->Logf(NAME_Debug, "uploading normal texture '%s' (%dx%d)", *SrcTex->Name, SrcTex->GetWidth(), SrcTex->GetHeight());
      if (SrcTex->Format == TEXFMT_8 || SrcTex->Format == TEXFMT_8Pal) {
        UploadTexture8(SrcTex->GetWidth(), SrcTex->GetHeight(), block, SrcTex->GetPalette(), SrcTex->SourceLump);
      } else {
        UploadTexture(SrcTex->GetWidth(), SrcTex->GetHeight(), (rgba_t *)block, (SrcTex->isTransparent() || SrcTex->isTranslucent()), SrcTex->SourceLump);
      }
    }

    if (SrcTex && gl_release_ram_textures_mode.asInt() >= 2) SrcTex->ReleasePixels();
  }

  // set up texture wrapping
  const GLint rep = (asPicture || !Tex->isWrapRepeat() ? ClampToEdge : GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, rep);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, rep);
  /*
  if (asPicture) {
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, ClampToEdge);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, ClampToEdge);
  } else {
    if (Tex->Type == TEXTYPE_Wall || Tex->Type == TEXTYPE_Flat || Tex->Type == TEXTYPE_Overload || Tex->Type == TEXTYPE_WallPatch) {
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    } else {
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, ClampToEdge);
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, ClampToEdge);
    }
  }
  */
  // other parameters will be set by the caller
}


//==========================================================================
//
//  VOpenGLDrawer::UploadTexture8
//
//==========================================================================
void VOpenGLDrawer::UploadTexture8 (int Width, int Height, const vuint8 *Data, const rgba_t *Pal, int SourceLump) {
  // this is single-threaded, so why not?
  int w = (Width > 0 ? Width : 2);
  int h = (Height > 0 ? Height : 2);
  static rgba_t *databuf = nullptr;
  static size_t databufSize = 0;
  if (databufSize < (size_t)(w*h*4)) {
    databuf = (rgba_t *)Z_Realloc(databuf, w*h*4);
    databufSize = (size_t)(w*h*4);
  }
  rgba_t *NewData = databuf;
  if (Width > 0 && Height > 0) {
    bool doFringeRemove = false;
    for (int i = 0; i < Width*Height; ++i, ++Data, ++NewData) {
      *NewData = (*Data ? Pal[*Data] : rgba_t::Transparent());
      if (!doFringeRemove && NewData->a != 255) doFringeRemove = true;
    }
    if (doFringeRemove) VTexture::FilterFringe(databuf, w, h);
  } else {
    memset((void *)NewData, 0, w*h*4);
  }
  UploadTexture(w, h, databuf, false, -1);
  //Z_Free(NewData);
}


//==========================================================================
//
//  VOpenGLDrawer::UploadTexture8A
//
//==========================================================================
void VOpenGLDrawer::UploadTexture8A (int Width, int Height, const pala_t *Data, const rgba_t *Pal, int SourceLump) {
  // this is single-threaded, so why not?
  int w = (Width > 0 ? Width : 2);
  int h = (Height > 0 ? Height : 2);
  static rgba_t *databuf = nullptr;
  static size_t databufSize = 0;
  if (databufSize < (size_t)(w*h*4)) {
    databuf = (rgba_t *)Z_Realloc(databuf, w*h*4);
    databufSize = (size_t)(w*h*4);
  }
  rgba_t *NewData = databuf;
  //rgba_t *NewData = (rgba_t *)Z_Calloc(Width*Height*4);
  if (Width > 0 && Height > 0) {
    bool doFringeRemove = false;
    for (int i = 0; i < Width*Height; ++i, ++Data, ++NewData) {
      *NewData = Pal[Data->idx];
      NewData->a = Data->a;
      if (!doFringeRemove && NewData->a != 255) doFringeRemove = true;
    }
    if (doFringeRemove) VTexture::FilterFringe(databuf, w, h);
  } else {
    memset((void *)NewData, 0, w*h*4);
  }
  UploadTexture(w, h, databuf, false, -1);
  //Z_Free(NewData);
}


//==========================================================================
//
//  VOpenGLDrawer::UploadTexture
//
//==========================================================================
void VOpenGLDrawer::UploadTexture (int width, int height, const rgba_t *data, bool doFringeRemove, int SourceLump) {
  if (width < 1 || height < 1) Sys_Error("WARNING: fucked texture (w=%d; h=%d)", width, height);
  if (!data) Sys_Error("WARNING: fucked texture (w=%d; h=%d, no data)", width, height);

  int w, h;

  if (hasNPOT) {
    w = width;
    h = height;
  } else {
    w = ToPowerOf2(width);
    h = ToPowerOf2(height);
  }

  if (w > maxTexSize) w = maxTexSize;
  if (h > maxTexSize) h = maxTexSize;

  // get two temporary buffers
  if (tmpImgBufSize < w*h*4) {
    tmpImgBufSize = ((w*h*4)|0xffff)+1;
    tmpImgBuf0 = (vuint8 *)Z_Realloc(tmpImgBuf0, tmpImgBufSize);
    tmpImgBuf1 = (vuint8 *)Z_Realloc(tmpImgBuf1, tmpImgBufSize);
  }

  /*
  vuint8 *gozzoFucked = nullptr;
  if (SourceLump >= 0 && r_gozzo_fucked_black && !W_IsIWADLump(SourceLump) && W_IsPakFile(W_LumpFile(SourceLump))) {
    bool wasConversion = false;
    gozzoFucked = (vuint8 *)Z_Malloc(w*h*4);
    memcpy(gozzoFucked, data, w*h*4);
    rgba_t *col = (rgba_t *)gozzoFucked;
    for (int y = 0; y < w; ++y) {
      for (int x = 0; x < h; ++x, ++col) {
        if ((col->r|col->g|col->b) == 0 && col->a == 255) {
          wasConversion = true;
          col->a = 0;
        }
      }
    }
    if (wasConversion) GCon->Logf(NAME_Warning, "hacked gozzo-black texture '%s'", *W_FullLumpName(SourceLump));
    data = (rgba_t *)gozzoFucked;
  }
  */

  vuint8 *image = tmpImgBuf0;
  vuint8 *pmimage = tmpImgBuf1;

  if (w != width || h != height) {
    // smooth transparent edges
    if (width <= maxTexSize && height <= maxTexSize) {
      memcpy(pmimage, data, width*height*4);
      VTexture::SmoothEdges(pmimage, width, height);
      // must rescale image to get "top" mipmap texture image
      VTexture::ResampleTexture(width, height, pmimage, w, h, image, multisampling_sample);
    } else {
      VTexture::ResampleTexture(width, height, (const vuint8 *)data, w, h, image, multisampling_sample);
    }
  } else {
    memcpy(image, data, w*h*4);
    if (doFringeRemove) VTexture::SmoothEdges(image, w, h);
    //if (doFringeRemove) VTexture::FilterFringe((rgba_t *)image, w, h);
    //VTexture::PremultiplyImage((rgba_t *)image, w, h);
  }

  VTexture::AdjustGamma((rgba_t *)image, w*h);

  glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
  glTexImage2D(GL_TEXTURE_2D, 0, 4, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

  //if (gozzoFucked) Z_Free(gozzoFucked);
}
