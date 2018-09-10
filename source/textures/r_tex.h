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

#ifndef _R_TEX_H
#define _R_TEX_H

#include "drawer.h"
#include "render/r_shared.h"


// dummy texture
class VDummyTexture : public VTexture {
public:
  VDummyTexture ();
  virtual vuint8 *GetPixels () override;
  virtual void Unload () override;
};


// standard Doom patch
class VPatchTexture : public VTexture {
private:
  vuint8 *Pixels;

public:
  static VTexture *Create (VStream &Strm, int LumpNum);

  VPatchTexture (int, int, int, int, int);
  virtual ~VPatchTexture () override;
  virtual vuint8 *GetPixels () override;
  virtual void Unload () override;
};


// texture defined in TEXTURE1/TEXTURE2 lumps.
// maptexturedef_t describes a rectangular texture, which is composed of
// one or more mappatch_t structures that arrange graphic patches
class VMultiPatchTexture : public VTexture {
private:
  enum {
    STYLE_Copy,
    STYLE_Translucent,
    STYLE_Add,
    STYLE_Subtract,
    STYLE_ReverseSubtract,
    STYLE_Modulate,
    STYLE_CopyAlpha
  };

  struct VTexPatch {
    // block origin (allways UL), which has allready accounted for the internal origin of the patch
    short XOrigin;
    short YOrigin;
    vuint8 Rot;
    vuint8 Style;
    bool bOwnTrans;
    VTexture *Tex;
    VTextureTranslation *Trans;
    rgba_t Blend;
    float Alpha;
  };

  // all the Patches[PatchCount] are drawn back to front into the cached texture
  int PatchCount;
  VTexPatch *Patches;
  vuint8 *Pixels;

public:
  VMultiPatchTexture (VStream &, int, VTexture **, int, int, bool);
  VMultiPatchTexture (VScriptParser *, int);
  virtual ~VMultiPatchTexture () override;
  virtual void SetFrontSkyLayer () override;
  virtual vuint8 *GetPixels () override;
  virtual void Unload() override;
};


// standard Doom flat
class VFlatTexture : public VTexture {
private:
  vuint8 *Pixels;

public:
  static VTexture *Create (VStream &Strm, int LumpNum);

  VFlatTexture (int);
  virtual ~VFlatTexture () override;
  virtual vuint8 *GetPixels () override;
  virtual void Unload () override;
};


// raven's raw screens
class VRawPicTexture : public VTexture {
private:
  int PalLumpNum;
  vuint8 *Pixels;
  rgba_t *Palette;

public:
  static VTexture *Create (VStream &Strm, int LumpNum);

  VRawPicTexture (int, int);
  virtual ~VRawPicTexture () override;
  virtual vuint8 *GetPixels () override;
  virtual rgba_t *GetPalette () override;
  virtual void Unload () override;
};


// raven's automap background
class VAutopageTexture : public VTexture {
private:
  vuint8 *Pixels;

public:
  static VTexture *Create (VStream &Strm, int LumpNum);

  VAutopageTexture (int ALumpNum);
  virtual ~VAutopageTexture () override;
  virtual vuint8 *GetPixels () override;
  virtual void Unload () override;
};


// ZDoom's IMGZ graphics
// [RH] Just a format I invented to avoid WinTex's palette remapping
// when I wanted to insert some alpha maps
class VImgzTexture : public VTexture {
private:
  vuint8 *Pixels;

public:
  static VTexture *Create (VStream &Strm, int LumpNum);

  VImgzTexture (int, int, int, int, int);
  virtual ~VImgzTexture () override;
  virtual vuint8 *GetPixels () override;
  virtual void Unload () override;
};


// PCX file
class VPcxTexture : public VTexture {
private:
  vuint8 *Pixels;
  rgba_t *Palette;

public:
  static VTexture *Create (VStream &Strm, int LumpNum);

  VPcxTexture (int, struct pcx_t &);
  virtual ~VPcxTexture () override;
  virtual vuint8 *GetPixels () override;
  virtual rgba_t *GetPalette () override;
  virtual void Unload () override;
};


// TGA file
class VTgaTexture : public VTexture {
private:
  vuint8 *Pixels;
  rgba_t *Palette;

public:
  static VTexture *Create (VStream &Strm, int LumpNum);

  VTgaTexture (int, struct tgaHeader_t &);
  virtual ~VTgaTexture () override;
  virtual vuint8 *GetPixels () override;
  virtual rgba_t *GetPalette () override;
  virtual void Unload () override;
};


// PNG file
class VPngTexture : public VTexture {
private:
  vuint8 *Pixels;

public:
  static VTexture *Create (VStream &Strm, int LumpNum);

  VPngTexture (int, int, int, int, int);
  virtual ~VPngTexture () override;
  virtual vuint8 *GetPixels () override;
  virtual void Unload () override;
};


// JPEG file
class VJpegTexture : public VTexture {
public:
  vuint8 *Pixels;

public:
  static VTexture *Create (VStream &Strm, int LumpNum);

  VJpegTexture(int, int, int);
  virtual ~VJpegTexture () override;
  virtual vuint8 *GetPixels () override;
  virtual void Unload () override;
};


// texture that returns a wiggly version of another texture
class VWarpTexture : public VTexture {
protected:
  VTexture *SrcTex;
  vuint8 *Pixels;
  float GenTime;
  float WarpXScale;
  float WarpYScale;
  float *XSin1;
  float *XSin2;
  float *YSin1;
  float *YSin2;

public:
  VWarpTexture (VTexture *);
  virtual ~VWarpTexture () override;
  virtual void SetFrontSkyLayer () override;
  virtual bool CheckModified () override;
  virtual vuint8 *GetPixels () override;
  virtual rgba_t *GetPalette () override;
  virtual VTexture *GetHighResolutionTexture () override;
  virtual void Unload () override;
};


// different style of warping
class VWarp2Texture : public VWarpTexture {
public:
  VWarp2Texture (VTexture *);
  virtual vuint8 *GetPixels () override;
};


extern VCvarB r_hirestex;
extern VCvarB r_showinfo;


#endif
