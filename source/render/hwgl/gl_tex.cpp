//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
#include "gl_local.h"


extern VCvarB gl_pic_filtering;
static VCvarB gl_recreate_changed_textures("gl_recreate_changed_textures", false, "Destroy and create new OpenGL textures for changed DooM animated ones?", CVAR_Archive);


static const vuint8 ptex[8][8] = {
  { 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 1, 1, 0, 0, 0 },
  { 0, 0, 1, 1, 1, 1, 0, 0 },
  { 0, 0, 1, 1, 1, 1, 0, 0 },
  { 0, 0, 0, 1, 1, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0 },
};


//==========================================================================
//
//  VOpenGLDrawer::GenerateTextures
//
//==========================================================================
void VOpenGLDrawer::GenerateTextures () {
  guard(VOpenGLDrawer::GenerateTextures);
  rgba_t pbuf[8][8];

  glGenTextures(NUM_BLOCK_SURFS, lmap_id);
  glGenTextures(NUM_BLOCK_SURFS, addmap_id);
  glGenTextures(1, &particle_texture);

  for (int j = 0; j < 8; ++j) {
    for (int i = 0; i < 8; ++i) {
      pbuf[j][i].r = 255;
      pbuf[j][i].g = 255;
      pbuf[j][i].b = 255;
      pbuf[j][i].a = vuint8(ptex[j][i]*255);
    }
  }
  VTexture::PremultiplyRGBAInPlace((vuint8 *)pbuf, 8, 8);
  VTexture::SmoothEdges((vuint8 *)pbuf, 8, 8);

  glBindTexture(GL_TEXTURE_2D, particle_texture);
  // set up texture anisotropic filtering
  if (max_anisotropy > 1.0) {
    //glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), (GLfloat)(max_anisotropy));
    glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT),
      (GLfloat)(gl_texture_filter_anisotropic < 1 ? 1.0f : gl_texture_filter_anisotropic > max_anisotropy ? max_anisotropy : gl_texture_filter_anisotropic)
    );
  }
  glTexImage2D(GL_TEXTURE_2D, 0, 4, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, pbuf);

  texturesGenerated = true;
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::FlushTextures
//
//==========================================================================
void VOpenGLDrawer::FlushTextures () {
  guard(VOpenGLDrawer::FlushTextures);
  for (int i = 0; i < GTextureManager.GetNumTextures(); ++i) DeleteTexture(GTextureManager[i]);
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::DeleteTextures
//
//==========================================================================
void VOpenGLDrawer::DeleteTextures () {
  guard(VOpenGLDrawer::DeleteTextures);
  if (texturesGenerated) {
    FlushTextures();
    glDeleteTextures(NUM_BLOCK_SURFS, lmap_id);
    glDeleteTextures(NUM_BLOCK_SURFS, addmap_id);
    glDeleteTextures(1, &particle_texture);
    texturesGenerated = false;
  }

  // delete all created shader objects
  for (int i = CreatedShaderObjects.length()-1; i >= 0; --i) {
    p_glDeleteObjectARB(CreatedShaderObjects[i]);
  }
  CreatedShaderObjects.Clear();

  UnloadModels();
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::FlushTexture
//
//==========================================================================
void VOpenGLDrawer::FlushTexture (VTexture *Tex) {
  guard(VOpenGLDrawer::FlushTexture);
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
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::DeleteTexture
//
//==========================================================================
void VOpenGLDrawer::DeleteTexture (VTexture *Tex) {
  guard(VOpenGLDrawer::FlushTexture);
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
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::PrecacheTexture
//
//==========================================================================
void VOpenGLDrawer::PrecacheTexture (VTexture *Tex) {
  guard(VOpenGLDrawer::PrecacheTexture);
  if (!Tex) return;
  SetTexture(Tex, 0);
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::SetTexture
//
//==========================================================================
void VOpenGLDrawer::SetTexture (VTexture *Tex, int CMap) {
  guard(VOpenGLDrawer::SetTexture);
  if (!Tex) Sys_Error("cannot set null texture");
  SetSpriteLump(Tex, nullptr, CMap, false);
  SetupTextureFiltering(texture_filter);
  /*
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, maxfilter);
  if (Tex->Type == TEXTYPE_WallPatch || Tex->Type == TEXTYPE_Wall || Tex->Type == TEXTYPE_Flat) {
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR / *GL_NEAREST* / / *GL_LINEAR_MIPMAP_NEAREST* /);
  } else {
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipfilter);
  }
  if (max_anisotropy > 1.0) {
    //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_filter_anisotropic);
    glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), (GLfloat)(gl_texture_filter_anisotropic < 0 ? 0 : gl_texture_filter_anisotropic > max_anisotropy ? max_anisotropy : gl_texture_filter_anisotropic));
  }
  */
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::SetSpriteLump
//
//==========================================================================
void VOpenGLDrawer::SetSpriteLump (VTexture *Tex, VTextureTranslation *Translation, int CMap, bool asPicture) {
  guard(VOpenGLDrawer::SetSpriteLump);
  if (mInitialized) {
    if (Tex->CheckModified()) FlushTexture(Tex);
    if (Translation || CMap) {
      VTexture::VTransData *TData = Tex->FindDriverTrans(Translation, CMap);
      if (TData) {
        glBindTexture(GL_TEXTURE_2D, TData->Handle);
      } else {
        TData = &Tex->DriverTranslated.Alloc();
        TData->Handle = 0;
        TData->Trans = Translation;
        TData->ColourMap = CMap;
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
  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::SetPic
//
//==========================================================================
void VOpenGLDrawer::SetPic (VTexture *Tex, VTextureTranslation *Trans, int CMap) {
  guard(VOpenGLDrawer::SetPic);
  /*
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, maxfilter);
  if (Tex->Type == TEXTYPE_Skin || Tex->Type == TEXTYPE_FontChar) {
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipfilter);
  } else {
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minfilter);
  }
  */

  SetSpriteLump(Tex, Trans, CMap, true);
  int flt = (gl_pic_filtering ? GL_LINEAR : GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, flt);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, flt);
  if (max_anisotropy > 1.0) glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), 1.0f);

  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::SetPicModel
//
//==========================================================================
void VOpenGLDrawer::SetPicModel (VTexture *Tex, VTextureTranslation *Trans, int CMap) {
  guard(VOpenGLDrawer::SetPicModel);

  SetSpriteLump(Tex, Trans, CMap, false);
  SetupTextureFiltering(model_filter);
  /*
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, maxfilter);
  if (Tex->Type == TEXTYPE_Skin || Tex->Type == TEXTYPE_FontChar) {
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipfilter);
  } else {
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minfilter);
  }
  */

  unguard;
}


//==========================================================================
//
//  VOpenGLDrawer::GenerateTexture
//
//==========================================================================
void VOpenGLDrawer::GenerateTexture (VTexture *Tex, GLuint *pHandle, VTextureTranslation *Translation, int CMap, bool asPicture) {
  guard(VOpenGLDrawer::GenerateTexture);

  if (!*pHandle) glGenTextures(1, pHandle);
  glBindTexture(GL_TEXTURE_2D, *pHandle);

  // try to load high resolution version
  VTexture *SrcTex = Tex->GetHighResolutionTexture();
  if (!SrcTex) SrcTex = Tex;

  // upload data
  if (Translation && CMap) {
    // both colormap and translation
    rgba_t tmppal[256];
    const vuint8 *TrTab = Translation->GetTable();
    const rgba_t *CMPal = ColourMaps[CMap].GetPalette();
    for (int i = 0; i < 256; ++i) tmppal[i] = CMPal[TrTab[i]];
    UploadTexture8A(SrcTex->GetWidth(), SrcTex->GetHeight(), SrcTex->GetPixels8A(), tmppal);
  } else if (Translation) {
    // only translation
    UploadTexture8A(SrcTex->GetWidth(), SrcTex->GetHeight(), SrcTex->GetPixels8A(), Translation->GetPalette());
  } else if (CMap) {
    // only colormap
    //GCon->Logf(NAME_Dev, "uploading colormapped texture '%s' (%dx%d)", *SrcTex->Name, SrcTex->GetWidth(), SrcTex->GetHeight());
    UploadTexture8A(SrcTex->GetWidth(), SrcTex->GetHeight(), SrcTex->GetPixels8A(), ColourMaps[CMap].GetPalette());
  } else {
    // normal uploading
    vuint8 *block = SrcTex->GetPixels();
    if (SrcTex->Format == TEXFMT_8 || SrcTex->Format == TEXFMT_8Pal) {
      UploadTexture8(SrcTex->GetWidth(), SrcTex->GetHeight(), block, SrcTex->GetPalette());
    } else {
      UploadTexture(SrcTex->GetWidth(), SrcTex->GetHeight(), (rgba_t *)block);
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
  unguard;
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
  guard(VOpenGLDrawer::UploadTexture);

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

  // get two temporary buffers: 0 for resampled image, 1 for premultiplied image
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
  }

  VTexture::AdjustGamma((rgba_t *)image, w*h);
  //VTexture::PremultiplyRGBA(pmimage, image, w, h);
  //VTexture::SmoothEdges(pmimage, w, h, pmimage);
  memcpy(pmimage, image, w*h*4);
  //for (int f = 0; f < w*h; ++f) pmimage[f*4+3] = 255;

  /*
  if (hasHWMipmaps) {
    //glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
    glHint(GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST);
  }
  */
  glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
  glTexImage2D(GL_TEXTURE_2D, 0, 4, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pmimage);

  // generate mipmaps
  /*
  if (hasHWMipmaps) {
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_FALSE);
  } else {
    for (int level = 1; w > 1 || h > 1; ++level) {
      VTexture::MipMap(w, h, image);
      if (w > 1) w >>= 1;
      if (h > 1) h >>= 1;
      VTexture::PremultiplyRGBA(pmimage, image, w, h);
      glTexImage2D(GL_TEXTURE_2D, level, 4, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pmimage);
    }
  }
  */

  unguard;
}
