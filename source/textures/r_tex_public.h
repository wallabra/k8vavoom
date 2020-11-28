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
  //
  TEXTYPE_MAX,
};


// texture data formats
enum {
  TEXFMT_8,    // paletised texture in main palette
  TEXFMT_8Pal, // paletised texture with custom palette
  TEXFMT_RGBA, // truecolor texture
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
  float xscale;
  float yscale;
  vint32 widthNonScaled;
  vint32 heightNonScaled;
  vint32 xoffsetNonScaled;
  vint32 yoffsetNonScaled;
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

  TSwitch () : Tex(0), PairIndex(0), Sound(0), Frames(nullptr), NumFrames(0), Quest(false) {}
  ~TSwitch () { if (Frames) { delete[] Frames; Frames = nullptr; } }
};


// ////////////////////////////////////////////////////////////////////////// //
class VTextureTranslation {
public:
  vuint8 Table[256];
  rgba_t Palette[256];

  vuint32 Crc;
  int nextInCache;

  // used to detect changed player translations
  vuint8 TranslStart;
  vuint8 TranslEnd;
  vint32 Color;

  // used to replicate translation tables in more efficient way
  struct VTransCmd {
    // 0: MapToRange (only Start/End matters)
    // 1: MapToColors (everything matters)
    // 2: MapDesaturated (RGBs are scaled from [0..2] range)
    // 3: MapBlended (only R1,G1,B1 matters)
    // 4: MapTinted (R2 is amount in percents, G2 and B2 doesn't matter)
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

public:
  VTextureTranslation ();

  void Clear ();
  void CalcCrc ();
  void Serialise (VStream &Strm);
  void BuildPlayerTrans (int Start, int End, int Col);
  void MapToRange (int AStart, int AEnd, int ASrcStart, int ASrcEnd);
  void MapToColors (int AStart, int AEnd, int AR1, int AG1, int AB1, int AR2, int AG2, int AB2);
  void MapDesaturated (int AStart, int AEnd, float rs, float gs, float bs, float re, float ge, float be);
  void MapBlended (int AStart, int AEnd, int R, int G, int B);
  void MapTinted (int AStart, int AEnd, int R, int G, int B, int Amount);
  void BuildBloodTrans (int Col);
  void AddTransString (VStr Str);

  inline const vuint8 *GetTable () const noexcept { return Table; }
  inline const rgba_t *GetPalette () const noexcept { return Palette; }
};


// ////////////////////////////////////////////////////////////////////////// //
class VTexture {
public:
  int Type; // -1: in `ReleasePixels()`; used to avoid circular loops

protected:
  inline bool InReleasingPixels () const noexcept { return (Type == -1); }

  struct ReleasePixelsLock {
  public:
    VTexture *tex;
    int oldType;
  public:
    VV_DISABLE_COPY(ReleasePixelsLock)
    inline ReleasePixelsLock (VTexture *atex) noexcept : tex(atex), oldType(atex->Type) { atex->Type = -1; }
    inline ~ReleasePixelsLock () noexcept { if (tex) tex->Type = oldType; tex = nullptr; }
  };

protected:
  int mFormat; // never use this directly!
  // original format; `mFormat` can be changed by `ConvertPixelsToRGBA()`, and
  // we need to restore it in `ReleasePixels()`
  int mOrigFormat;

public:
  VName Name;
  int Width;
  int Height;
  int SOffset;
  int TOffset;
  bool bNoRemap0;
  bool bWorldPanning;
  bool bIsCameraTexture;
  bool bForcedSpriteOffset; // do not try to guess sprite offset, if this is set
  int SOffsetFix; // set only if `bForcedSpriteOffset` is true
  int TOffsetFix; // set only if `bForcedSpriteOffset` is true
  vuint8 WarpType;
  float SScale; // horizontal scaling; divide by this to get "view size"
  float TScale; // vertical scaling; divide by this to get "view size"
  int TextureTranslation; // animation
  int HashNext;
  int SourceLump;
  // real picture dimensions (w/o transparent part), in pixels; MIN_VINT32 means "not calculated yet"
  int RealX0;
  int RealY0;
  // this is from (0,0)
  int RealHeight;
  int RealWidth;

  VTexture *Brightmap;

  bool noDecals;
  bool staticNoDecals;
  bool animNoDecals;
  bool animated; // used to select "no decals" flag
  bool needFBO;
  bool transparent; // `true` if texture has any non-solid pixels; set in `GetPixels()`
  bool translucent; // `true` if texture has some non-integral alpha pixels; set in `GetPixels()`
  bool nofullbright; // valid for all textures; forces "no fullbright" for sprite brightmaps (ONLY!)
  vuint32 glowing; // is this a glowing texture? (has any meaning only for floors and ceilings; 0: none; if high byte is 0xff, it is fullbright)
  bool noHires; // hires texture tried and not found

  vuint32 lastUpdateFrame;

  // driver data
  struct VTransData {
    vuint32 Handle;
    VTextureTranslation *Trans;
    int ColorMap;
    // if non-zero, this is shade color
    vuint32 ShadeColor;
    vint32 lastUseTime; // this is "game time", from `GLevel->TicTime`

    // call to wipe it -- not "properly clear", but wipe completely
    inline void wipe () noexcept { memset((void *)this, 0, sizeof(VTransData)); }
  };

  vuint32 DriverHandle;
  TArray<VTransData> DriverTranslated;

  void ResetTranslations ();
  void ClearTranslations ();

  void ClearTranslationAt (int idx);
  // returns new index; also, marks it as "recently used"
  int MoveTranslationToTop (int idx);

  // -1 if not available
  static inline vint32 GetTranslationCTime () noexcept {
    return (GLevel ? GLevel->TicTime : GClLevel ? GClLevel->TicTime : -1);
  }

  inline bool IsGlowFullbright () const noexcept { return ((glowing&0xff000000u) == 0xff000000u); }

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
  static void checkerFillRGB (vuint8 *dest, int width, int height, int alpha=-1); // alpha <0 means 3-byte RGB texture
  static inline void checkerFillRGBA (vuint8 *dest, int width, int height) { checkerFillRGB(dest, width, height, 255); }

  // `dest` points at column, `x` is used only to build checker
  static void checkerFillColumn8 (vuint8 *dest, int x, int pitch, int height);

  static const char *TexTypeToStr (int ttype) {
    switch (ttype) {
      case TEXTYPE_Any: return "any";
      case TEXTYPE_WallPatch: return "patch";
      case TEXTYPE_Wall: return "wall";
      case TEXTYPE_Flat: return "flat";
      case TEXTYPE_Overload: return "overload";
      case TEXTYPE_Sprite: return "sprite";
      case TEXTYPE_SkyMap: return "sky";
      case TEXTYPE_Skin: return "skin";
      case TEXTYPE_Pic: return "pic";
      case TEXTYPE_Autopage: return "autopage";
      case TEXTYPE_Null: return "null";
      case TEXTYPE_FontChar: return "fontchar";
    }
    return "unknown";
  }

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

  void CalcRealDimensions ();

public:
  static void FilterFringe (rgba_t *pic, int wdt, int hgt);
  static void PremultiplyImage (rgba_t *pic, int wdt, int hgt);

  // use `153` to calculate glow color
  rgb_t GetAverageColor (vuint32 maxout);
  void ResizeCanvas (int newwdt, int newhgt);

public:
  //k8: please note that due to my sloppy coding, real format checking should be preceded by `GetPixels()`
  inline int GetFormat () const { return (shadeColor == -1 ?  mFormat : TEXFMT_RGBA); }
  PropertyRO<int, VTexture> Format {this, &VTexture::GetFormat};

  inline int GetRealX0 () { if (RealX0 == MIN_VINT32) CalcRealDimensions(); return RealX0; }
  inline int GetRealY0 () { if (RealY0 == MIN_VINT32) CalcRealDimensions(); return RealY0; }
  inline int GetRealHeight () { if (RealHeight == MIN_VINT32) CalcRealDimensions(); return RealHeight; }
  inline int GetRealWidth () { if (RealWidth == MIN_VINT32) CalcRealDimensions(); return RealWidth; }

public:
  VTexture ();
  virtual ~VTexture ();

  // this won't add texture to texture hash
  static VTexture *CreateTexture (int Type, int LumpNum, bool setName=true);

  // WARNING! this converts texture to RGBA!
  // DO NOT USE! DEBUG ONLY!
  void WriteToPNG (VStream *strm);

  int GetWidth () const noexcept { return Width; }
  int GetHeight () const noexcept { return Height; }

  int GetScaledWidth () const noexcept { return max2(1, (int)(Width/SScale)); }
  int GetScaledHeight () const noexcept { return max2(1, (int)(Height/TScale)); }

  int GetScaledSOffset () const noexcept { return (int)(SOffset/SScale); }
  int GetScaledTOffset () const noexcept { return (int)(TOffset/TScale); }

  // get texture pixel; will call `GetPixels()`
  rgba_t getPixel (int x, int y);

  inline bool isTransparent () {
    if (!Pixels && !Pixels8BitValid && !Pixels8BitAValid) (void)GetPixels(); // this will set the flag
    return transparent;
  }

  inline bool isTranslucent () {
    if (!Pixels && !Pixels8BitValid && !Pixels8BitAValid) (void)GetPixels(); // this will set the flag
    return translucent;
  }

  inline bool isSeeThrough () {
    if (!Pixels && !Pixels8BitValid && !Pixels8BitAValid) (void)GetPixels(); // this will set the flag
    return (transparent || translucent);
  }

  virtual void SetFrontSkyLayer ();
  virtual bool CheckModified ();
  virtual void Shade (int shade); // should be called before any `GetPixels()` call!

  // this can be called to release texture memory
  // the texture will be re-read on next `GetPixels()`
  // can be used to release hires texture memory
  virtual void ReleasePixels ();

  virtual vuint8 *GetPixels () = 0;
  vuint8 *GetPixels8 ();
  pala_t *GetPixels8A ();
  virtual rgba_t *GetPalette ();
  virtual VTexture *GetHighResolutionTexture ();

  // this returns temporary data, which should be freed with `FreeShadedPixels()`
  // WARNING: next call to `CreateShadedPixels()` may invalidate the pointer!
  //          that means that you MUST call `FreeShadedPixels()` before trying to
  //          get another shaded pixels
  // `palette` can be `nullptr` if no translation needed
  rgba_t *CreateShadedPixels (vuint32 shadeColor, const rgba_t *palette);
  void FreeShadedPixels (rgba_t *shadedPixels);

  VTransData *FindDriverTrans (VTextureTranslation *TransTab, int CMap, bool markUsed);
  VTransData *FindDriverShaded (vuint32 ShadeColor, int CMap, bool markUsed);

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
      return *this;
    }
  };

private:
  void rehashTextures ();

  inline VTexture *getTxByIndex (int idx) const noexcept {
    if (idx < FirstMapTextureIndex) {
      return ((vuint32)idx < (vuint32)Textures.length() ? Textures[idx] : nullptr);
    } else {
      idx -= FirstMapTextureIndex;
      return ((vuint32)idx < (vuint32)MapTextures.length() ? MapTextures[idx] : nullptr);
    }
  }

public:
  vint32 DefaultTexture;
  float Time; // time value for warp textures

public:
  static inline bool IsDummyTextureName (VName n) {
    return (n != NAME_None && (n == "-" || VStr::ICmp(*n, "AASTINKY") == 0));
  }

  struct Iter {
  private:
    VTextureManager *tman;
    int idx;
    VName name;
    bool allowShrink;

  private:
    inline bool restart () {
      // check for empty texture name
      if (name == NAME_None) { idx = -1; return false; }
      // check for "NoTexture" marker
      if (VTextureManager::IsDummyTextureName(name)) { idx = 0; return true; }
      // has texture manager object?
      if (!tman) { idx = -1; return false; }
      // hashmap search
      VName loname = name.GetLowerNoCreate();
      if (loname == NAME_None) { idx = -1; return false; }
      name = loname;
      int cidx = tman->TextureHash[GetTypeHash(name)&(HASH_SIZE-1)];
      while (cidx >= 0) {
        VTexture *ctex = tman->getTxByIndex(cidx);
        if (ctex->Name == name) { idx = cidx; return true; }
        cidx = ctex->HashNext;
      }
      idx = -1;
      return false;
    }

  public:
    Iter () : tman(nullptr), idx(-1), name(NAME_None), allowShrink(false) {}
    Iter (VTextureManager *atman, VName aname, bool aAllowShrink=true) : tman(atman), idx(-1), name(aname), allowShrink(aAllowShrink) { restart(); }

    inline bool empty () const { return (idx < 0); }
    inline bool isMapTexture () const { return (tman && idx >= tman->Textures.length()); }
    inline bool next () {
      if (idx < 0) return false;
      for (;;) {
        idx = tman->getTxByIndex(idx)->HashNext;
        if (idx < 0) break;
        if (tman->getTxByIndex(idx)->Name == name) return true;
      }
      // here we can restart iteration with shrinked name, if it is longer than 8 chars
      if (allowShrink && VStr::length(*name) > 8) {
        allowShrink = false;
        name = name.GetLower8NoCreate();
        if (name != NAME_None) return restart();
      }
      return false;
    }
    inline int index () const { return idx; }
    inline VTexture *tex () const { return tman->getTxByIndex(idx); }
  };

  inline Iter firstWithName (VName n, bool allowShrink=true) { return Iter(this, n, allowShrink); }
  Iter firstWithStr (VStr s);

public:
  VName SkyFlatName;
  VVA_CHECKRESULT bool IsSkyTextureName (VName n) noexcept;
  void SetSkyFlatName (VStr flatname) noexcept;

protected:
  int AddFullNameTexture (VTexture *Tex, bool asMapTexture);

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
  int CheckNumForName (VName Name, int Type, bool bOverload=false);
  int FindPatchByName (VName Name); // used in multipatch texture builder
  int FindWallByName (VName Name, bool bOverload=true); // used to find wall texture (but can return non-wall)
  int FindFlatByName (VName Name, bool bOverload=true); // used to find flat texture (but can return non-flat)
  int NumForName (VName Name, int Type, bool bOverload=false, bool bAllowLoadAsMapTexture=false);
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
  int AddFileTextureChecked (VName Name, int Type, VName forceName=NAME_None); // returns -1 if no texture found
  // try to force-load texture
  int CheckNumForNameAndForce (VName Name, int Type, bool bOverload, bool silent);

  // this can find/load both textures without path (lump-named), and textures with full path
  // it also can return normalized texture name in `normname` (it can be `nullptr` too)
  // returns -1 if not found/cannot load
  int FindOrLoadFullyNamedTexture (VStr txname, VName *normname, int Type, bool bOverload, bool silent, bool allowLoad=true, bool forceMapTexture=false);
  int FindOrLoadFullyNamedTextureAsMapTexture (VStr txname, VName *normname, int Type, bool bOverload, bool silent=false) { return FindOrLoadFullyNamedTexture(txname, normname, Type, bOverload, silent, true, true); }
  inline int FindFullyNamedTexture (VStr txname, VName *normname, int Type, bool bOverload, bool silent=false) { return FindOrLoadFullyNamedTexture(txname, normname, Type, bOverload, silent, false); }

  inline bool IsMapLocalTexture (int TexNum) const noexcept { return (TexNum >= FirstMapTextureIndex); }

  inline bool IsEmptyTexture (int TexNum) const noexcept {
    if (TexNum <= 0) return true;
    VTexture *tx = getIgnoreAnim(TexNum);
    return (!tx || tx->Type == TEXTYPE_Null);
  }

  inline bool IsSeeThrough (int TexNum) const noexcept {
    if (TexNum <= 0) return true;
    VTexture *tx = getIgnoreAnim(TexNum);
    return (!tx || tx->Type == TEXTYPE_Null || tx->isSeeThrough());
  }

  inline bool IsSightBlocking (int TexNum) const noexcept { return !IsSeeThrough(TexNum); }

  enum TexCheckType {
    TCT_EMPTY,
    TCT_SEE_TRHOUGH,
    TCT_SOLID,
  };

  inline TexCheckType GetTextureType (int TexNum) const noexcept {
    if (TexNum <= 0) return TCT_EMPTY;
    VTexture *tx = getIgnoreAnim(TexNum);
    return (!tx || tx->Type == TEXTYPE_Null ? TCT_EMPTY : (tx->isSeeThrough() ? TCT_SEE_TRHOUGH : TCT_SOLID));
  }

  // get unanimated texture
  inline VTexture *operator [] (int TexNum) const noexcept {
    VTexture *res = getTxByIndex(TexNum);
    if (res) res->noDecals = res->staticNoDecals;
    return res;
  }

  inline VTexture *getIgnoreAnim (int TexNum) const noexcept { return getTxByIndex(TexNum); }
  inline VTexture *getMapTexIgnoreAnim (int TexNum) const noexcept { return ((vuint32)TexNum < (vuint32)MapTextures.length() ? MapTextures[TexNum] : nullptr); }

  //inline int TextureAnimation (int InTex) { return Textures[InTex]->TextureTranslation; }

  // get animated texture
  inline VTexture *operator () (int TexNum) noexcept {
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

  inline int GetNumTextures () const noexcept { return Textures.length(); }
  inline int GetNumMapTextures () const noexcept { return MapTextures.length(); }

  // to use in `ExportTexture` command
  void FillNameAutocompletion (VStr prefix, TArray<VStr> &list);
  VTexture *GetExistingTextureByName (VStr txname, int type=TEXTYPE_Any);

private:
  void LoadPNames (int Lump, TArray<WallPatchInfo> &patchtexlookup);
  void AddToHash (int Index);
  void AddTextures ();
  void AddMissingNumberedTextures ();
  void AddTexturesLump (TArray<WallPatchInfo> &patchtexlookup, int TexLump, int FirstTex, bool First);
  void AddGroup (int, EWadNamespace);

  void ParseTextureTextLump (int Lump, bool asHiRes);

  void AddTextureTextLumps ();

  // this tries to add `TEXTYPE_Pic` patch if `Type` not found
  // returns -1 on error; never returns 0 (0 is "not found")
  int FindHiResToReplace (VName name, int Type, bool bOverload=true);

  // this can also append a new texture if `OldIndex` is < 0
  // it can do `delete NewTex` too
  void ReplaceTextureWithHiRes (int OldIndex, VTexture *NewTex, int oldWidth=-1, int oldHeight=-1);
  void AddHiResTextureTextLumps ();

  void AddHiResTexturesFromNS (EWadNamespace NS, int ttype, bool &messaged);
  void AddHiResTextures (); // hires namespace

  void LoadSpriteOffsets ();

  void WipeWallPatches ();

  friend void R_InitTexture ();
  friend void R_DumpTextures ();

  friend void R_InitHiResTextures ();
};


// ////////////////////////////////////////////////////////////////////////// //
// r_tex
void R_InitTexture ();
void R_InitHiResTextures ();
void R_DumpTextures ();
void R_ShutdownTexture ();
VAnimDoorDef *R_FindAnimDoor (vint32);
void R_AnimateSurfaces ();
bool R_IsAnimatedTexture (int texid);

void R_UpdateSkyFlatNum (bool force=false);

// ////////////////////////////////////////////////////////////////////////// //
extern VTextureManager GTextureManager;
extern int skyflatnum;
extern int screenBackTexNum;

// switches
extern TArray<TSwitch *> Switches;


extern int gtxRight;
extern int gtxLeft;
extern int gtxTop;
extern int gtxBottom;
extern int gtxFront;
extern int gtxBack;
