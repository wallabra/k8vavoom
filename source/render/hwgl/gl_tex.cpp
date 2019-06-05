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


extern VCvarB gl_pic_filtering;
static VCvarB gl_recreate_changed_textures("gl_recreate_changed_textures", false, "Destroy and create new OpenGL textures for changed DooM animated ones?", CVAR_Archive);


//==========================================================================
//
//  VOpenGLDrawer::GenerateTextures
//
//==========================================================================
void VOpenGLDrawer::GenerateTextures () {
  glGenTextures(NUM_BLOCK_SURFS, lmap_id);
  glGenTextures(NUM_BLOCK_SURFS, addmap_id);
  texturesGenerated = true;
}


//==========================================================================
//
//  VOpenGLDrawer::FlushTextures
//
//==========================================================================
void VOpenGLDrawer::FlushTextures () {
  for (int i = 0; i < GTextureManager.GetNumTextures(); ++i) DeleteTexture(GTextureManager[i]);
}


//==========================================================================
//
//  VOpenGLDrawer::FlushOneTexture
//
//==========================================================================
void VOpenGLDrawer::FlushOneTexture (VTexture *tex) {
  if (!tex) return;
  DeleteTexture(tex);
}


//==========================================================================
//
//  VOpenGLDrawer::DeleteTextures
//
//==========================================================================
void VOpenGLDrawer::DeleteTextures () {
  if (texturesGenerated) {
    FlushTextures();
    glDeleteTextures(NUM_BLOCK_SURFS, lmap_id);
    glDeleteTextures(NUM_BLOCK_SURFS, addmap_id);
    texturesGenerated = false;
  }

  // delete all created shader objects
  for (int i = CreatedShaderObjects.length()-1; i >= 0; --i) {
    p_glDeleteObjectARB(CreatedShaderObjects[i]);
  }
  CreatedShaderObjects.Clear();

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
    if (Tex->SavedDriverHandle && Tex->SavedDriverHandle != Tex->DriverHandle) glDeleteTextures(1, (GLuint *)&Tex->SavedDriverHandle);
    if (gl_recreate_changed_textures) {
      glDeleteTextures(1, (GLuint *)&Tex->DriverHandle);
      Tex->SavedDriverHandle = 0;
    } else {
      Tex->SavedDriverHandle = Tex->DriverHandle;
    }
    Tex->DriverHandle = 0;
  }
  for (int j = 0; j < Tex->DriverTranslated.length(); ++j) {
    glDeleteTextures(1, (GLuint *)&Tex->DriverTranslated[j].Handle);
  }
  Tex->DriverTranslated.resetNoDtor();
}


//==========================================================================
//
//  VOpenGLDrawer::DeleteTexture
//
//==========================================================================
void VOpenGLDrawer::DeleteTexture (VTexture *Tex) {
  if (!Tex) return;
  if (Tex->DriverHandle) {
    if (Tex->SavedDriverHandle && Tex->SavedDriverHandle != Tex->DriverHandle) glDeleteTextures(1, (GLuint *)&Tex->SavedDriverHandle);
    glDeleteTextures(1, (GLuint*)&Tex->DriverHandle);
    Tex->DriverHandle = 0;
    Tex->SavedDriverHandle = 0;
  } else if (Tex->SavedDriverHandle) {
    glDeleteTextures(1, (GLuint *)&Tex->SavedDriverHandle);
    Tex->SavedDriverHandle = 0;
  }
  for (int j = 0; j < Tex->DriverTranslated.length(); ++j) {
    glDeleteTextures(1, (GLuint*)&Tex->DriverTranslated[j].Handle);
  }
  Tex->DriverTranslated.Clear();
  if (Tex->Brightmap) DeleteTexture(Tex->Brightmap);
}


//==========================================================================
//
//  VOpenGLDrawer::PrecacheTexture
//
//==========================================================================
void VOpenGLDrawer::PrecacheTexture (VTexture *Tex) {
  if (!Tex) return;
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
  SetTexture(Tex, 0); // default colormap
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  SetupTextureFiltering(r_brightmaps_filter ? 4 : 0); // trilinear or none
}


//==========================================================================
//
//  VOpenGLDrawer::SetTexture
//
//==========================================================================
void VOpenGLDrawer::SetTexture (VTexture *Tex, int CMap) {
  if (!Tex) Sys_Error("cannot set null texture");
  SetSpriteLump(Tex, nullptr, CMap, false);
  SetupTextureFiltering(texture_filter);
}


//==========================================================================
//
//  VOpenGLDrawer::SetSpriteLump
//
//==========================================================================
void VOpenGLDrawer::SetSpriteLump (VTexture *Tex, VTextureTranslation *Translation, int CMap, bool asPicture) {
  if (mInitialized) {
    if (Tex->CheckModified()) FlushTexture(Tex);
    if (Translation || CMap) {
      VTexture::VTransData *TData = Tex->FindDriverTrans(Translation, CMap);
      if (TData) {
        glBindTexture(GL_TEXTURE_2D, TData->Handle);
      } else {
        //if (Translation) GCon->Logf("*** NEW TRANSLATION for texture '%s'", *Tex->Name);
        TData = &Tex->DriverTranslated.Alloc();
        TData->Handle = 0;
        TData->Trans = Translation;
        TData->ColorMap = CMap;
        GenerateTexture(Tex, (GLuint *)&TData->Handle, Translation, CMap, asPicture);
      }
    } else {
      if (!Tex->DriverHandle) {
        if (Tex->SavedDriverHandle) {
          if (gl_recreate_changed_textures) {
            glDeleteTextures(1, (GLuint *)&Tex->SavedDriverHandle);
            Tex->SavedDriverHandle = 0;
          } else {
            Tex->DriverHandle = Tex->SavedDriverHandle;
            Tex->SavedDriverHandle = 0;
            //fprintf(stderr, "reusing texture %u!\n", Tex->DriverHandle);
          }
        }
        GenerateTexture(Tex, &Tex->DriverHandle, nullptr, 0, asPicture);
      } else {
        glBindTexture(GL_TEXTURE_2D, Tex->DriverHandle);
      }
    }
  }
  tex_w = Tex->GetWidth();
  tex_h = Tex->GetHeight();
  tex_iw = 1.0f/tex_w;
  tex_ih = 1.0f/tex_h;
}


//==========================================================================
//
//  VOpenGLDrawer::SetPic
//
//==========================================================================
void VOpenGLDrawer::SetPic (VTexture *Tex, VTextureTranslation *Trans, int CMap) {
  SetSpriteLump(Tex, Trans, CMap, true);
  int flt = (gl_pic_filtering ? GL_LINEAR : GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, flt);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, flt);
  if (anisotropyExists) glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), 1.0f);
}


//==========================================================================
//
//  VOpenGLDrawer::SetPicModel
//
//==========================================================================
void VOpenGLDrawer::SetPicModel (VTexture *Tex, VTextureTranslation *Trans, int CMap) {
  SetSpriteLump(Tex, Trans, CMap, false);
  SetupTextureFiltering(model_filter);
}


//==========================================================================
//
//  VOpenGLDrawer::GenerateTexture
//
//==========================================================================
void VOpenGLDrawer::GenerateTexture (VTexture *Tex, GLuint *pHandle, VTextureTranslation *Translation, int CMap, bool asPicture) {
  if (!*pHandle) glGenTextures(1, pHandle);
  glBindTexture(GL_TEXTURE_2D, *pHandle);

  // try to load high resolution version
  VTexture *SrcTex = Tex->GetHighResolutionTexture();
  if (!SrcTex) {
    SrcTex = Tex;
    //GCon->Logf("VOpenGLDrawer::GenerateTexture(%d): %s", Tex->Type, *Tex->Name);
  } else {
    //GCon->Logf("VOpenGLDrawer::GenerateTexture(%d): %s (lo: %s)", Tex->Type, *SrcTex->Name, *Tex->Name);
  }

  if (SrcTex->Type == TEXTYPE_Null) {
    // fuckin' idiots
    /*if (SrcTex->Name != NAME_None)*/ {
      GCon->Logf(NAME_Warning, "something is VERY wrong with textures in this mod (trying to upload null texture '%s')", *SrcTex->Name);
    }
    check(SrcTex->GetWidth() > 0);
    check(SrcTex->GetHeight() > 0);
    rgba_t *dummy = (rgba_t *)Z_Calloc(SrcTex->GetWidth()*SrcTex->GetHeight()*sizeof(rgba_t));
    //VTexture::checkerFillRGBA((vuint8 *)dummy, SrcTex->GetWidth(), SrcTex->GetHeight());
    UploadTexture(SrcTex->GetWidth(), SrcTex->GetHeight(), dummy);
    Z_Free(dummy);
  } else {
    // upload data
    if (Translation && CMap) {
      // both colormap and translation
      rgba_t tmppal[256];
      const vuint8 *TrTab = Translation->GetTable();
      const rgba_t *CMPal = ColorMaps[CMap].GetPalette();
      for (int i = 0; i < 256; ++i) tmppal[i] = CMPal[TrTab[i]];
      UploadTexture8A(SrcTex->GetWidth(), SrcTex->GetHeight(), SrcTex->GetPixels8A(), tmppal);
    } else if (Translation) {
      // only translation
      //GCon->Logf("uploading translated texture '%s' (%dx%d)", *SrcTex->Name, SrcTex->GetWidth(), SrcTex->GetHeight());
      //for (int f = 0; f < 256; ++f) GCon->Logf("  %3d: r:g:b=%02x:%02x:%02x", f, Translation->GetPalette()[f].r, Translation->GetPalette()[f].g, Translation->GetPalette()[f].b);
      UploadTexture8A(SrcTex->GetWidth(), SrcTex->GetHeight(), SrcTex->GetPixels8A(), Translation->GetPalette());
    } else if (CMap) {
      // only colormap
      //GCon->Logf(NAME_Dev, "uploading colormapped texture '%s' (%dx%d)", *SrcTex->Name, SrcTex->GetWidth(), SrcTex->GetHeight());
      UploadTexture8A(SrcTex->GetWidth(), SrcTex->GetHeight(), SrcTex->GetPixels8A(), ColorMaps[CMap].GetPalette());
    } else {
      // normal uploading
      vuint8 *block = SrcTex->GetPixels();
      if (SrcTex->Format == TEXFMT_8 || SrcTex->Format == TEXFMT_8Pal) {
        UploadTexture8(SrcTex->GetWidth(), SrcTex->GetHeight(), block, SrcTex->GetPalette());
      } else {
        UploadTexture(SrcTex->GetWidth(), SrcTex->GetHeight(), (rgba_t *)block);
      }
    }
  }

  // set up texture wrapping
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
  // other parameters will be set by a caller
}


//==========================================================================
//
//  VOpenGLDrawer::UploadTexture8
//
//==========================================================================
void VOpenGLDrawer::UploadTexture8 (int Width, int Height, const vuint8 *Data, const rgba_t *Pal) {
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
    for (int i = 0; i < Width*Height; ++i, ++Data, ++NewData) {
      *NewData = (*Data ? Pal[*Data] : rgba_t::Transparent());
    }
    VTexture::filterFringe(databuf, w, h);
  } else {
    memset((void *)NewData, 0, w*h*4);
  }
  UploadTexture(w, h, databuf);
  //Z_Free(NewData);
}


//==========================================================================
//
//  VOpenGLDrawer::UploadTexture8A
//
//==========================================================================
void VOpenGLDrawer::UploadTexture8A (int Width, int Height, const pala_t *Data, const rgba_t *Pal) {
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
    for (int i = 0; i < Width*Height; ++i, ++Data, ++NewData) {
      *NewData = Pal[Data->idx];
      NewData->a = Data->a;
    }
    VTexture::filterFringe(databuf, w, h);
  } else {
    memset((void *)NewData, 0, w*h*4);
  }
  UploadTexture(w, h, databuf);
  //Z_Free(NewData);
}


//==========================================================================
//
//  VOpenGLDrawer::UploadTexture
//
//==========================================================================
void VOpenGLDrawer::UploadTexture (int width, int height, const rgba_t *data) {
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
    VTexture::SmoothEdges(image, w, h);
    //VTexture::filterFringe((rgba_t *)image, w, h);
  }

  VTexture::AdjustGamma((rgba_t *)image, w*h);

  glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
  glTexImage2D(GL_TEXTURE_2D, 0, 4, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
}
