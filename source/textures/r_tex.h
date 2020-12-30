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
#ifndef VAVOOM_R_TEX_HEADER
#define VAVOOM_R_TEX_HEADER

#include "../drawer.h"


// dummy texture
class VDummyTexture : public VTexture {
public:
  VDummyTexture ();
  virtual vuint8 *GetPixels () override;
};


// standard Doom patch
class VPatchTexture : public VTexture {
public:
  static VTexture *Create (VStream &Strm, int LumpNum);

  VPatchTexture (int, int, int, int, int);
  virtual ~VPatchTexture () override;
  virtual vuint8 *GetPixels () override;
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
    STYLE_Overlay,
    STYLE_CopyAlpha,
    STYLE_CopyNewAlpha,
  };

  struct VTexPatch {
    // block origin (allways UL), which has allready accounted for the internal origin of the patch
    int XOrigin;
    int YOrigin;
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


protected:
  void ParseGfxPart (VScriptParser *sc, EWadNamespace prioNS, TArray<VTexPatch> &Parts);

public:
  VMultiPatchTexture (VStream &, int, TArray<VTextureManager::WallPatchInfo> &, int, bool);
  VMultiPatchTexture (VScriptParser *, int);
  virtual ~VMultiPatchTexture () override;
  virtual void SetFrontSkyLayer () override;
  virtual void ReleasePixels () override;
  virtual vuint8 *GetPixels () override;
};


// standard Doom flat
class VFlatTexture : public VTexture {
public:
  static VTexture *Create (VStream &Strm, int LumpNum);

  VFlatTexture (int InLumpNum);
  virtual ~VFlatTexture () override;
  virtual vuint8 *GetPixels () override;
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
  virtual void ReleasePixels () override;
  virtual vuint8 *GetPixels () override;
  virtual rgba_t *GetPalette () override;
};


// raven's automap background
class VAutopageTexture : public VTexture {
public:
  static VTexture *Create (VStream &Strm, int LumpNum);

  VAutopageTexture (int ALumpNum);
  virtual ~VAutopageTexture () override;
  virtual vuint8 *GetPixels () override;
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
};


// PCX file
class VPcxTexture : public VTexture {
private:
  rgba_t *Palette;

public:
  static VTexture *Create (VStream &Strm, int LumpNum);

  VPcxTexture (int, struct pcx_t &);
  virtual ~VPcxTexture () override;
  virtual void ReleasePixels () override;
  virtual vuint8 *GetPixels () override;
  virtual rgba_t *GetPalette () override;
};


// TGA file
class VTgaTexture : public VTexture {
private:
  rgba_t *Palette;

public:
  static VTexture *Create (VStream &Strm, int LumpNum);

  VTgaTexture (int, struct TGAHeader_t &);
  virtual ~VTgaTexture () override;
  virtual void ReleasePixels () override;
  virtual vuint8 *GetPixels () override;
  virtual rgba_t *GetPalette () override;
};


// PNG file
class VPngTexture : public VTexture {
public:
  static VTexture *Create (VStream &Strm, int LumpNum);

  VPngTexture (int, int, int, int, int);
  virtual ~VPngTexture () override;
  virtual vuint8 *GetPixels () override;
};


// JPEG file
class VJpegTexture : public VTexture {
public:
  static VTexture *Create (VStream &Strm, int LumpNum);

  VJpegTexture(int, int, int);
  virtual ~VJpegTexture () override;
  virtual vuint8 *GetPixels () override;
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
  virtual bool IsDynamicTexture () const noexcept override;
  virtual void SetFrontSkyLayer () override;
  virtual bool CheckModified () override;
  virtual void ReleasePixels () override;
  virtual vuint8 *GetPixels () override;
  virtual rgba_t *GetPalette () override;
  virtual VTexture *GetHighResolutionTexture () override;
};


// different style of warping
class VWarp2Texture : public VWarpTexture {
public:
  VWarp2Texture (VTexture *, float aspeed=1);
  virtual vuint8 *GetPixels () override;
};


extern VCvarB r_hirestex;
extern VCvarB r_showinfo;


// ////////////////////////////////////////////////////////////////////////// //
// animated textures support
// ////////////////////////////////////////////////////////////////////////// //

extern void R_InitFTAnims (); // called by `R_InitTexture()`
extern void R_ShutdownFTAnims (); // called by `R_ShutdownTexture()`


#endif
