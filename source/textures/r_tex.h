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
#ifndef VAVOOM_R_TEX_HEADER
#define VAVOOM_R_TEX_HEADER

#include "drawer.h"
#include "render/r_shared.h"


struct WallPatchInfo {
  int index;
  VName name;
  VTexture *tx; // can be null
};


// dummy texture
class VDummyTexture : public VTexture {
public:
  VDummyTexture ();
  virtual vuint8 *GetPixels () override;
  virtual void Unload () override;
};


// standard Doom patch
class VPatchTexture : public VTexture {
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

public:
  VMultiPatchTexture (VStream &, int, TArray<WallPatchInfo> &, int, bool);
  VMultiPatchTexture (VScriptParser *, int);
  virtual ~VMultiPatchTexture () override;
  virtual void SetFrontSkyLayer () override;
  virtual vuint8 *GetPixels () override;
  virtual void Unload () override;
};


// standard Doom flat
class VFlatTexture : public VTexture {
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
  rgba_t *Palette;

public:
  static VTexture *Create (VStream &Strm, int LumpNum);

  VTgaTexture (int, struct TGAHeader_t &);
  virtual ~VTgaTexture () override;
  virtual vuint8 *GetPixels () override;
  virtual rgba_t *GetPalette () override;
  virtual void Unload () override;
};


// PNG file
class VPngTexture : public VTexture {
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
  float GenTime;
  float Speed;
  float WarpXScale;
  float WarpYScale;
  float *XSin1;
  float *XSin2;
  float *YSin1;
  float *YSin2;

public:
  VWarpTexture (VTexture *, float aspeed=1);
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
  VWarp2Texture (VTexture *, float aspeed=1);
  virtual vuint8 *GetPixels () override;
};


extern VCvarB r_hirestex;
extern VCvarB r_showinfo;


#endif
