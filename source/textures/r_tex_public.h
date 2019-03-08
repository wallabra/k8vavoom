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

// texture use types
enum {
  TEXTYPE_Any,
  TEXTYPE_WallPatch,
  TEXTYPE_Wall,
  TEXTYPE_Flat,
  TEXTYPE_Overload,
  TEXTYPE_Sprite,
  TEXTYPE_SkyMap,
  TEXTYPE_Skin,
  TEXTYPE_Pic,
  TEXTYPE_Autopage,
  TEXTYPE_Null,
  TEXTYPE_FontChar,
};


// texture data formats
enum {
  TEXFMT_8,    // paletised texture in main palette
  TEXFMT_8Pal, // paletised texture with custom palette
  TEXFMT_RGBA, // truecolour texture
};


struct __attribute__((__packed__)) rgb_t {
  vuint8 r, g, b;
  rgb_t () : r(0), g(0), b(0) {}
  rgb_t (vuint8 ar, vuint8 ag, vuint8 ab) : r(ar), g(ag), b(ab) {}
};
static_assert(sizeof(rgb_t) == 3, "invalid rgb_t size");

struct __attribute__((__packed__)) rgba_t {
  vuint8 r, g, b, a;
  rgba_t () : r(0), g(0), b(0), a(0) {}
  rgba_t (vuint8 ar, vuint8 ag, vuint8 ab, vuint8 aa=255) : r(ar), g(ag), b(ab), a(aa) {}
  static inline rgba_t Transparent () { return rgba_t(0, 0, 0, 0); }
};
static_assert(sizeof(rgba_t) == 4, "invalid rgba_t size");

struct __attribute__((__packed__)) pala_t {
  vuint8 idx, a;
  pala_t () : idx(0), a(0) {}
  pala_t (vuint8 aidx, vuint8 aa=255) : idx(aidx), a(aa) {}
};
static_assert(sizeof(pala_t) == 2, "invalid pala_t size");


struct picinfo_t {
  vint32 width;
  vint32 height;
  vint32 xoffset;
  vint32 yoffset;
};


struct VAnimDoorDef {
  vint32 Texture;
  VName OpenSound;
  VName CloseSound;
  vint32 *Frames;
  vint32 NumFrames;
};


struct TSwitchFrame {
  vint16 Texture;
  vint16 BaseTime;
  vint16 RandomRange;
};


struct TSwitch {
  vint16 Tex;
  vint16 PairIndex;
  vint16 Sound;
  TSwitchFrame *Frames;
  vint16 NumFrames;
  bool Quest;

  TSwitch () : Frames(nullptr) {}
  ~TSwitch () { if (Frames) { delete[] Frames; Frames = nullptr; } }
};


// ////////////////////////////////////////////////////////////////////////// //
class VTextureTranslation {
public:
  vuint8 Table[256];
  rgba_t Palette[256];

  vuint16 Crc;

  // used to detect changed player translations
  vuint8 TranslStart;
  vuint8 TranslEnd;
  vint32 Colour;

  // used to replicate translation tables in more efficient way
  struct VTransCmd {
    vuint8 Type;
    vuint8 Start;
    vuint8 End;
    vuint8 R1;
    vuint8 G1;
    vuint8 B1;
    vuint8 R2;
    vuint8 G2;
    vuint8 B2;
  };
  TArray<VTransCmd> Commands;

  VTextureTranslation ();

  void Clear ();
  void CalcCrc ();
  void Serialise (VStream &);
  void BuildPlayerTrans (int, int, int);
  void MapToRange (int, int, int, int);
  void MapToColours (int, int, int, int, int, int, int, int);
  void BuildBloodTrans (int);
  void AddTransString (const VStr &);

  inline const vuint8 *GetTable () const { return Table; }
  inline const rgba_t *GetPalette () const { return Palette; }
};


// ////////////////////////////////////////////////////////////////////////// //
class VTexture {
public:
  int Type;

protected:
  int mFormat; // never use this directly!

public:
  VName Name;
  int Width;
  int Height;
  int SOffset;
  int TOffset;
  bool bNoRemap0;
  bool bWorldPanning;
  bool bIsCameraTexture;
  vuint8 WarpType;
  float SScale; // scaling
  float TScale;
  int TextureTranslation; // animation
  int HashNext;
  int SourceLump;

  bool noDecals;
  bool staticNoDecals;
  bool animNoDecals;
  bool animated; // used to select "no decals" flag
  bool needFBO;
  bool transparent; // `true` if texture has any non-solid pixels; set in `GetPixels()`

  GLuint mFBO;
  GLuint mFBOColorTid;
  GLuint mFBODepthStencilTid;

  // driver data
  struct VTransData {
    union {
      vuint32 Handle;
      void *Data;
    };
    VTextureTranslation *Trans;
    int ColourMap;
  };

  union {
    vuint32 DriverHandle;
    void *DriverData;
  };
  TArray<VTransData> DriverTranslated;
  vuint32 SavedDriverHandle;

protected:
  vuint8 *Pixels; // most textures has some kind of pixel data, so declare it here
  vuint8 *Pixels8Bit;
  pala_t *Pixels8BitA;
  VTexture *HiResTexture;
  bool Pixels8BitValid;
  bool Pixels8BitAValid;
  int shadeColor;

public:
  static void checkerFill8 (vuint8 *dest, int width, int height);
  static void checkerFillRGB (vuint8 *dest, int width, int height);
  static void checkerFillRGBA (vuint8 *dest, int width, int height);

  // `dest` points at column, `x` is used only to build checker
  static void checkerFillColumn8 (vuint8 *dest, int x, int pitch, int height);

protected:
  // this should be called after `Pixels` were converted to RGBA
  void shadePixelsRGBA (int shadeColor);
  void stencilPixelsRGBA (int shadeColor);

  // uses `Format` to convert, so it must be valid
  // `Pixels` must be set
  // will delete old `Pixels` if necessary
  void ConvertPixelsToRGBA ();

  // uses `Format` to convert, so it must be valid
  // `Pixels` must be set
  // will delete old `Pixels` if necessary
  void ConvertPixelsToShaded ();

public:
  static void filterFringe (rgba_t *pic, int wdt, int hgt);

public:
  //k8: please note that due to my sloppy coding, real format checking should be preceded by `GetPixels()`
  inline int GetFormat () const { return (shadeColor == -1 ?  mFormat : TEXFMT_RGBA); }
  PropertyRO<int, VTexture> Format {this, &VTexture::GetFormat};

public:
  VTexture ();
  virtual ~VTexture ();

  static VTexture *CreateTexture (int, int);

  // WARNING! this converts texture to RGBA!
  // DO NOT USE! DEBUG ONLY!
  void WriteToPNG (VStream *strm);

  int GetWidth () const { return Width; }
  int GetHeight () const { return Height; }

  int GetScaledWidth () const { return (int)(Width/SScale); }
  int GetScaledHeight () const { return (int)(Height/TScale); }

  int GetScaledSOffset () const { return (int)(SOffset/SScale); }
  int GetScaledTOffset () const { return (int)(TOffset/TScale); }

  // get texture pixel; will call `GetPixels()`
  rgba_t getPixel (int x, int y);

  virtual bool isTransparent ();

  virtual void SetFrontSkyLayer ();
  virtual bool CheckModified ();
  virtual void Shade (int shade); // should be called before any `GetPixels()` call!
  virtual vuint8 *GetPixels () = 0;
  vuint8 *GetPixels8 ();
  pala_t *GetPixels8A ();
  virtual rgba_t *GetPalette ();
  virtual void Unload () = 0;
  virtual VTexture *GetHighResolutionTexture ();
  VTransData *FindDriverTrans (VTextureTranslation *, int);

  static void AdjustGamma (rgba_t *, int); // for non-premultiplied
  static void SmoothEdges (vuint8 *, int, int); // for non-premultiplied
  static void ResampleTexture (int, int, const vuint8 *, int, int, vuint8 *, int); // for non-premultiplied
  static void MipMap (int, int, vuint8 *); // for non-premultiplied
  static void PremultiplyRGBAInPlace (void *databuff, int w, int h);
  static void PremultiplyRGBA (void *dest, const void *src, int w, int h);

protected:
  void FixupPalette (rgba_t *Palette);
};


// ////////////////////////////////////////////////////////////////////////// //
class VTextureManager {
public:
  struct WallPatchInfo {
    int index;
    VName name;
    VTexture *tx; // can be null
  };

private:
  enum { HASH_SIZE = 4096 };
  enum { FirstMapTextureIndex = 1000000 };

  TArray<VTexture *> Textures;
  TArray<VTexture *> MapTextures;
  int TextureHash[HASH_SIZE];

  int inMapTextures; // >0: loading map textures

public:
  struct MTLock {
  friend class VTextureManager;
  private:
    VTextureManager *tm;
    MTLock (VTextureManager *atm) : tm(atm) { ++tm->inMapTextures; }
  public:
    MTLock (const MTLock &alock) : tm(alock.tm) { ++tm->inMapTextures; }
    ~MTLock () { if (tm) --tm->inMapTextures; tm = nullptr; }
    MTLock &operator = (const MTLock &alock) {
      if (&alock == this) return *this;
      if (alock.tm != tm) {
        if (tm) --tm->inMapTextures;
        tm = alock.tm;
      }
      if (tm) ++tm->inMapTextures;
    }
  };

private:
  void rehashTextures ();

  inline VTexture *getTxByIndex (int idx) const {
    VTexture *res;
    if (idx < FirstMapTextureIndex) {
      res = ((vuint32)idx < (vuint32)Textures.length() ? Textures[idx] : nullptr);
    } else {
      idx -= FirstMapTextureIndex;
      res = ((vuint32)idx < (vuint32)MapTextures.length() ? MapTextures[idx] : nullptr);
    }
    return res;
  }


public:
  vint32 DefaultTexture;
  float Time; // time value for warp textures

public:
  VTextureManager ();

  void Init ();
  void Shutdown ();

  void DumpHashStats (EName logName=NAME_Log);

  // unload all map-local textures
  void ResetMapTextures ();

  // call this before possible loading of map-local texture
  inline MTLock LockMapLocalTextures () { return MTLock(this); }

  int AddTexture (VTexture *Tex);
  void ReplaceTexture (int Index, VTexture *Tex);
  int CheckNumForName (VName Name, int Type, bool bOverload=false, bool bCheckAny=false);
  int NumForName (VName Name, int Type, bool bOverload=false, bool bCheckAny=false);
  int FindTextureByLumpNum (int);
  VName GetTextureName (int TexNum);
  float TextureWidth (int TexNum);
  float TextureHeight (int TexNum);
  void SetFrontSkyLayer (int tex);
  void GetTextureInfo (int TexNum, picinfo_t *info);
  int AddPatch (VName Name, int Type, bool Silent=false);
  int AddPatchShaded (VName Name, int Type, int shade, bool Silent=false); // shade==-1: don't shade
  int AddPatchLump (int LumpNum, VName Name, int Type, bool Silent=false);
  int AddRawWithPal (VName Name, VName PalName);
  int AddFileTexture (VName Name, int Type);
  int AddFileTextureShaded (VName Name, int Type, int shade); // shade==-1: don't shade
  int AddFileTextureChecked (VName Name, int Type); // returns -1 if no texture found
  // try to force-load texture
  int CheckNumForNameAndForce (VName Name, int Type, bool bOverload, bool bCheckAny, bool silent);

  inline bool IsMapLocalTexture (int TexNum) const { return (TexNum >= FirstMapTextureIndex); }

  inline bool IsEmptyTexture (int TexNum) const {
    if (TexNum <= 0) return true;
    VTexture *tx = getIgnoreAnim(TexNum);
    return (!tx || tx->Type == TEXTYPE_Null);
  }

  // get unanimated texture
  inline VTexture *operator [] (int TexNum) const {
    VTexture *res = getTxByIndex(TexNum);
    if (res) res->noDecals = res->staticNoDecals;
    return res;
  }

  inline VTexture *getIgnoreAnim (int TexNum) const {
    if (TexNum < FirstMapTextureIndex) {
      return ((vuint32)TexNum < (vuint32)Textures.length() ? Textures[TexNum] : nullptr);
    } else {
      TexNum -= FirstMapTextureIndex;
      return ((vuint32)TexNum < (vuint32)MapTextures.length() ? MapTextures[TexNum] : nullptr);
    }
  }

  //inline int TextureAnimation (int InTex) { return Textures[InTex]->TextureTranslation; }

  // get animated texture
  inline VTexture *operator () (int TexNum) {
    VTexture *origtex;
    if (TexNum < FirstMapTextureIndex) {
      if ((vuint32)TexNum >= (vuint32)Textures.length()) return nullptr;
      origtex = Textures[TexNum];
    } else {
      if ((vuint32)(TexNum-FirstMapTextureIndex) >= (vuint32)MapTextures.length()) return nullptr;
      origtex = MapTextures[TexNum-FirstMapTextureIndex];
    }
    if (!origtex) return nullptr;
    //FIXMEFIXMEFIXME: `origtex->TextureTranslation == -1`? whatisthat?
    if (origtex->TextureTranslation != TexNum /*&& (vuint32)origtex->TextureTranslation < (vuint32)Textures.length()*/) {
      //VTexture *res = Textures[origtex->TextureTranslation];
      VTexture *res = getTxByIndex(origtex->TextureTranslation);
      if (res) {
        if (res) res->noDecals = origtex->animNoDecals || res->staticNoDecals;
        return res;
      }
    }
    origtex->noDecals = (origtex->animated ? origtex->animNoDecals : false) || origtex->staticNoDecals;
    return origtex;
  }

  inline int GetNumTextures () const { return Textures.length(); }
  inline int GetNumMapTextures () const { return MapTextures.length(); }

  // to use in `ExportTexture` command
  void FillNameAutocompletion (const VStr &prefix, TArray<VStr> &list);
  VTexture *GetExistingTextureByName (const VStr &txname);

private:
  void LoadPNames (int Lump, TArray<WallPatchInfo> &patchtexlookup, TArray<VName> &numberedNames);
  void AddToHash (int Index);
  void AddTextures (TArray<VName> &numberedNames);
  void AddMissingNumberedTextures (TArray<VName> &numberedNames);
  void AddTexturesLump (TArray<WallPatchInfo> &, int, int, bool);
  void AddGroup (int, EWadNamespace);
  void AddHiResTextures ();

  void WipeWallPatches ();

  friend void R_InitTexture ();
  friend void R_DumpTextures ();
};


// ////////////////////////////////////////////////////////////////////////// //
// r_tex
void R_InitTexture ();
void R_DumpTextures ();
void R_ShutdownTexture ();
VAnimDoorDef *R_FindAnimDoor (vint32);
void R_AnimateSurfaces ();
bool R_IsAnimatedTexture (int texid);


// ////////////////////////////////////////////////////////////////////////// //
extern VTextureManager GTextureManager;
extern int skyflatnum;

// switches
extern TArray<TSwitch *> Switches;
