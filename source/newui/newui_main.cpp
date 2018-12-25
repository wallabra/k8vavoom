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
#include "../gamedefs.h"

#ifdef CLIENT
#include <SDL.h>
#ifdef USE_GLAD
# include "glad.h"
#else
# include <GL/gl.h>
# include <GL/glext.h>
#endif
#include "../cl_local.h"
#include "../drawer.h"
#include "../../libs/imago/imago.h"
#include "newui.h"


// ////////////////////////////////////////////////////////////////////////// //
struct NewUIICB {
  NewUIICB () {
    VDrawer::RegisterICB(&drawerICB);
  }

  static VClass *nuiMainCls;
  static VObject *nuiMainObj;
  static VMethod *nuiMDispatch;
  static VMethod *nuiMIsPaused;
  static VMethod *nuiMRenderFrame;
  static bool inited;

  static void drawerICB (int phase) {
    //GCon->Logf("NEWUI: phase=%d", phase);
    if (phase == VDrawer::VCB_InitVideo && !nuiMainCls) {
      nuiMainCls = VClass::FindClass("NUI_Main");
      if (nuiMainCls) {
        nuiMainObj = (VObject *)VObject::StaticSpawnObject(nuiMainCls, false); // don't skip replacement
        if (nuiMainObj) {
          nuiMDispatch = nuiMainCls->FindMethod("Dispatch");
          if (nuiMDispatch) {
            if (nuiMDispatch->NumParams != 1 ||
                nuiMDispatch->ParamTypes[0].Type != TYPE_Struct ||
                !nuiMDispatch->ParamTypes[0].Struct ||
                nuiMDispatch->ParamTypes[0].Struct->Name != "event_t" ||
                nuiMDispatch->ParamFlags[0] != FPARM_Ref ||
                (nuiMDispatch->ReturnType.Type != TYPE_Int && nuiMDispatch->ReturnType.Type != TYPE_Bool) ||
                (nuiMDispatch->Flags&(FUNC_VarArgs|FUNC_Net|FUNC_Spawner|FUNC_NetReliable|FUNC_Iterator|FUNC_Private|FUNC_Protected)) != 0)
            {
              nuiMDispatch = nullptr;
            }
          }
          nuiMIsPaused = nuiMainCls->FindMethod("IsPaused");
          if (nuiMIsPaused) {
            if (nuiMIsPaused->NumParams != 0 ||
                (nuiMIsPaused->ReturnType.Type != TYPE_Int && nuiMIsPaused->ReturnType.Type != TYPE_Bool) ||
                (nuiMDispatch->Flags&(FUNC_VarArgs|FUNC_Net|FUNC_Spawner|FUNC_NetReliable|FUNC_Iterator|FUNC_Private|FUNC_Protected)) != 0)
            {
              nuiMIsPaused = nullptr;
            }
          }
          nuiMRenderFrame = nuiMainCls->FindMethod("RenderFrame");
          if (nuiMRenderFrame) {
            if (nuiMRenderFrame->NumParams != 0 ||
                nuiMRenderFrame->ReturnType.Type != TYPE_Void ||
                (nuiMRenderFrame->Flags&(FUNC_VarArgs|FUNC_Net|FUNC_Spawner|FUNC_NetReliable|FUNC_Iterator|FUNC_Private|FUNC_Protected)) != 0)
            {
              nuiMRenderFrame = nullptr;
            }
          }
        }
      }
    }
    inited = (phase == VDrawer::VCB_InitResolution || phase == VDrawer::VCB_FinishUpdate);
    if (phase == VDrawer::VCB_FinishUpdate && nuiMRenderFrame) {
      if ((nuiMRenderFrame->Flags&FUNC_Static) == 0) P_PASS_REF(nuiMainObj);
      VObject::ExecuteFunction(nuiMRenderFrame);
    }
  }
};

static NewUIICB newuiicb;

bool NewUIICB::inited = false;
VClass *NewUIICB::nuiMainCls = nullptr;
VObject *NewUIICB::nuiMainObj = nullptr;
VMethod *NewUIICB::nuiMDispatch = nullptr;
VMethod *NewUIICB::nuiMIsPaused = nullptr;
VMethod *NewUIICB::nuiMRenderFrame = nullptr;

// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  NUI_IsPaused
//
//==========================================================================
bool NUI_IsPaused () {
  if (NewUIICB::nuiMIsPaused) {
    if ((NewUIICB::nuiMIsPaused->Flags&FUNC_Static) == 0) P_PASS_REF(NewUIICB::nuiMainObj);
    return VObject::ExecuteFunction(NewUIICB::nuiMIsPaused).getBool();
  }
  return false;
}


//==========================================================================
//
//  NUI_Responder
//
//==========================================================================
bool NUI_Responder (event_t *ev) {
  if (!ev) return false;
  if (NewUIICB::nuiMDispatch) {
    if ((NewUIICB::nuiMDispatch->Flags&FUNC_Static) == 0) P_PASS_REF(NewUIICB::nuiMainObj);
    P_PASS_PTR(ev);
    return VObject::ExecuteFunction(NewUIICB::nuiMDispatch).getBool();
  }
  return false;
}


// ////////////////////////////////////////////////////////////////////////// //
class NUIFont;

class VGLVideo : public VObject {
  DECLARE_ABSTRACT_CLASS(VGLVideo, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VGLVideo)

public:
  // (srcColor * <srcFactor>) <op> (dstColor * <dstFactor>)
  // If you want alpha blending use <srcFactor> of SRC_ALPHA and <dstFactor> of ONE_MINUS_SRC_ALPHA:
  //   (srcColor * srcAlpha) + (dstColor * (1-srcAlpha))
  enum {
    BlendNone, // disabled
    BlendNormal, // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    BlendBlend, // glBlendFunc(GL_SRC_ALPHA, GL_ONE)
    BlendFilter, // glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR)
    BlendInvert, // glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO)
    BlendParticle, // glBlendFunc(GL_DST_COLOR, GL_ZERO)
    BlendHighlight, // glBlendFunc(GL_DST_COLOR, GL_ONE);
    BlendDstMulDstAlpha, // glBlendFunc(GL_ZERO, GL_DST_ALPHA);
    InvModulate, // glBlendFunc(GL_ZERO, GL_SRC_COLOR)
    //
    BlendMax,
  };

private:
  static int mBlendMode;

  static vuint32 colorARGB; // a==0: opaque
  static NUIFont *currFont;
  static bool smoothLine;
  static bool texFiltering;
  friend class VOpenGLTexture;

public:
  static inline void forceGLTexFilter () {
    if (NewUIICB::inited) {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (texFiltering ? GL_LINEAR : GL_NEAREST));
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (texFiltering ? GL_LINEAR : GL_NEAREST));
    }
  }

private:
  static inline bool getTexFiltering () { return texFiltering; }
  static inline void setTexFiltering (bool filterit) { texFiltering = filterit; }

  static inline void realiseGLColor () {
    if (NewUIICB::inited) {
      glColor4f(
        ((colorARGB>>16)&0xff)/255.0f,
        ((colorARGB>>8)&0xff)/255.0f,
        (colorARGB&0xff)/255.0f,
        1.0f-(((colorARGB>>24)&0xff)/255.0f)
      );
      setupBlending();
    }
  }

  // returns `true` if drawing will have any effect
  static inline bool setupBlending () {
    if (mBlendMode == BlendNone) {
      glDisable(GL_BLEND);
      return true;
    } else if (mBlendMode == BlendNormal) {
      if ((colorARGB&0xff000000u) == 0) {
        // opaque
        glDisable(GL_BLEND);
        return true;
      } else {
        // either alpha, or completely transparent
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        return ((colorARGB&0xff000000u) != 0xff000000u);
      }
    } else {
      glEnable(GL_BLEND);
           if (mBlendMode == BlendBlend) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
      else if (mBlendMode == BlendFilter) glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
      else if (mBlendMode == BlendInvert) glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
      else if (mBlendMode == BlendParticle) glBlendFunc(GL_DST_COLOR, GL_ZERO);
      else if (mBlendMode == BlendHighlight) glBlendFunc(GL_DST_COLOR, GL_ONE);
      else if (mBlendMode == BlendDstMulDstAlpha) glBlendFunc(GL_ZERO, GL_DST_ALPHA);
      else if (mBlendMode == InvModulate) glBlendFunc(GL_ZERO, GL_SRC_COLOR);
      return ((colorARGB&0xff000000u) != 0xff000000u);
    }
  }

public:
  static void clearColored (int rgb=0);

  static void setFont (VName fontname);

  static inline void setColor (vuint32 clr) { if (colorARGB != clr) { colorARGB = clr; realiseGLColor(); } }
  static inline vuint32 getColor () { return colorARGB; }

  static inline int getBlendMode () { return mBlendMode; }
  static inline void setBlendMode (int v) { if (v >= BlendNormal && v <= BlendMax && v != mBlendMode) { mBlendMode = v; setupBlending(); } }

  static inline bool isFullyOpaque () { return ((colorARGB&0xff000000) == 0); }
  static inline bool isFullyTransparent () { return ((colorARGB&0xff000000) == 0xff000000); }

  static void drawTextAt (int x, int y, const VStr &text);

  DECLARE_FUNCTION(isInitialized)
  DECLARE_FUNCTION(screenWidth)
  DECLARE_FUNCTION(screenHeight)

  //DECLARE_FUNCTION(isMouseCursorVisible)
  //DECLARE_FUNCTION(hideMouseCursor)
  //DECLARE_FUNCTION(showMouseCursor)

  DECLARE_FUNCTION(glHasExtension)

  DECLARE_FUNCTION(get_glHasNPOT)

  DECLARE_FUNCTION(get_scissorEnabled)
  DECLARE_FUNCTION(set_scissorEnabled)
  DECLARE_FUNCTION(getScissor)
  DECLARE_FUNCTION(setScissor)
  DECLARE_FUNCTION(copyScissor)

  DECLARE_FUNCTION(glPushMatrix);
  DECLARE_FUNCTION(glPopMatrix);
  DECLARE_FUNCTION(glLoadIdentity);
  DECLARE_FUNCTION(glScale);
  DECLARE_FUNCTION(glTranslate);
  //DECLARE_FUNCTION(glRotate);

  DECLARE_FUNCTION(clearScreen)

  DECLARE_FUNCTION(get_smoothLine)
  DECLARE_FUNCTION(set_smoothLine)

  DECLARE_FUNCTION(get_colorARGB) // aarrggbb
  DECLARE_FUNCTION(set_colorARGB) // aarrggbb

  DECLARE_FUNCTION(get_blendMode)
  DECLARE_FUNCTION(set_blendMode)

  DECLARE_FUNCTION(get_textureFiltering)
  DECLARE_FUNCTION(set_textureFiltering)

  DECLARE_FUNCTION(get_fontName)
  DECLARE_FUNCTION(set_fontName)

  DECLARE_FUNCTION(loadFontDF)
  DECLARE_FUNCTION(loadFontPCF)

  DECLARE_FUNCTION(fontHeight)
  DECLARE_FUNCTION(getCharInfo)
  DECLARE_FUNCTION(charWidth)
  DECLARE_FUNCTION(spaceWidth)
  DECLARE_FUNCTION(textWidth)
  DECLARE_FUNCTION(textHeight)
  DECLARE_FUNCTION(drawTextAt)

  DECLARE_FUNCTION(drawLine)
  DECLARE_FUNCTION(drawRect)
  DECLARE_FUNCTION(fillRect)

  DECLARE_FUNCTION(getMousePos)
};


// ////////////////////////////////////////////////////////////////////////// //
// refcounted object
class VOpenGLTexture {
private:
  int rc;
  VStr mPath;

public:
  struct TexQuad {
    int x0, y0, x1, y1;
    float tx0, ty0, tx1, ty1;
  };

  const VStr &getPath () const { return mPath; }
  int getRC () const { return rc; }

public:
  VImage *img;
  GLuint tid; // !0: texture loaded
  bool mTransparent; // fully
  bool mOpaque; // fully
  bool mOneBitAlpha; // this means that the only alpha values are 255 or 0
  VOpenGLTexture *prev;
  VOpenGLTexture *next;

  bool getTransparent () const { return mTransparent; }
  bool getOpaque () const { return mOpaque; }
  bool get1BitAlpha () const { return mOneBitAlpha; }

private:
  void registerMe ();
  void analyzeImage ();

  VOpenGLTexture (int awdt, int ahgt); // dimensions must be valid!

public:
  VOpenGLTexture (VImage *aimg, const VStr &apath);
  ~VOpenGLTexture (); // don't call this manually!

  void addRef ();
  void release (); //WARNING: can delete `this`!

  void update ();

  static VOpenGLTexture *Load (const VStr &fname);
  static VOpenGLTexture *CreateEmpty (VStr txname, int wdt, int hgt);

  inline int getWidth () const { return (img ? img->width : 0); }
  inline int getHeight () const { return (img ? img->height : 0); }

  PropertyRO<const VStr &, VOpenGLTexture> path {this, &VOpenGLTexture::getPath};
  PropertyRO<int, VOpenGLTexture> width {this, &VOpenGLTexture::getWidth};
  PropertyRO<int, VOpenGLTexture> height {this, &VOpenGLTexture::getHeight};

  // that is:
  //  isTransparent==true: no need to draw anything
  //  isOpaque==true: no need to do alpha-blending
  //  isOneBitAlpha==true: no need to do advanced alpha-blending

  PropertyRO<bool, VOpenGLTexture> isTransparent {this, &VOpenGLTexture::getTransparent};
  PropertyRO<bool, VOpenGLTexture> isOpaque {this, &VOpenGLTexture::getOpaque};
  // this means that the only alpha values are 255 or 0
  PropertyRO<bool, VOpenGLTexture> isOneBitAlpha {this, &VOpenGLTexture::get1BitAlpha};

  // angle is in degrees
  void blitExt (int dx0, int dy0, int dx1, int dy1, int x0, int y0, int x1, int y1, float angle) const;
  void blitAt (int dx0, int dy0, float scale=1, float angle=0) const;

  // this uses integer texture coords
  void blitExtRep (int dx0, int dy0, int dx1, int dy1, int x0, int y0, int x1, int y1) const;
};


// ////////////////////////////////////////////////////////////////////////// //
// VaVoom C wrapper
class VGLTexture : public VObject {
  DECLARE_CLASS(VGLTexture, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VGLTexture)

private:
  VOpenGLTexture *tex;
  int id;

public:
  virtual void Destroy () override;

public:
  DECLARE_FUNCTION(Destroy)
  DECLARE_FUNCTION(Load) // native final static GLTexture Load (string fname);
  DECLARE_FUNCTION(GetById); // native final static GLTexture GetById (int id);
  DECLARE_FUNCTION(width)
  DECLARE_FUNCTION(height)
  DECLARE_FUNCTION(isTransparent)
  DECLARE_FUNCTION(isOpaque)
  DECLARE_FUNCTION(isOneBitAlpha)
  DECLARE_FUNCTION(blitExt)
  DECLARE_FUNCTION(blitExtRep)
  DECLARE_FUNCTION(blitAt)

  DECLARE_FUNCTION(CreateEmpty) // native final static GLTexture CreateEmpty (int wdt, int hgt, optional name txname);
  DECLARE_FUNCTION(setPixel) // native final static void setPixel (int x, int y, int argb); // aarrggbb; a==0 is completely opaque
  DECLARE_FUNCTION(getPixel) // native final static int getPixel (int x, int y); // aarrggbb; a==0 is completely opaque
  DECLARE_FUNCTION(upload) // native final static void upload ();
  DECLARE_FUNCTION(smoothEdges) // native final void smoothEdges (); // call after manual texture building

  friend class NUIFont;
};


// ////////////////////////////////////////////////////////////////////////// //
// base class for fonts
class NUIFont {
protected:
  static NUIFont *fontList;

public:
  struct FontChar {
    vint32 ch;
    vint32 width, height; // height may differ from font height
    //vint32 advance; // horizontal advance to print next char
    vint32 topofs; // offset from font top (i.e. y+topofs should be used to draw char)
    vint32 leftbear, rightbear; // positive means "more to the respective bbox side"
    vint32 ascent, descent; // both are positive, which actually means "offset from baseline to the respective direction"
    float tx0, ty0; // texture coordinates, [0..1)
    float tx1, ty1; // texture coordinates, [0..1) -- cached for convenience
    VOpenGLTexture *tex; // don't destroy this!

    FontChar () : ch(-1), width(0), height(0), tex(nullptr) {}
  };

public:
  VName name;
  NUIFont *next;
  VOpenGLTexture *tex;
  bool singleTexture;

  // font characters (cp1251)
  TArray<FontChar> chars;
  vint32 spaceWidth; // width of the space character
  vint32 fontHeight; // height of the font
  vint32 minWidth, maxWidth, avgWidth;

protected:
  NUIFont (); // this inits nothing, and intended to be used in `LoadXXX()`

public:
  //NUIFont (VName aname, const VStr &fnameIni, const VStr &fnameTexture);
  ~NUIFont ();

  static NUIFont *LoadDF (VName aname, const VStr &fnameIni, const VStr &fnameTexture);
  static NUIFont *LoadPCF (VName aname, const VStr &filename);

  const FontChar *getChar (vint32 ch) const;
  vint32 charWidth (vint32 ch) const;
  vint32 textWidth (const VStr &s) const;
  vint32 textHeight (const VStr &s) const;
  // will clear lines; returns maximum text width
  //int splitTextWidth (const VStr &text, TArray<VSplitLine> &lines, int maxWidth) const;

  inline VName getName () const { return name; }
  inline vint32 getSpaceWidth () const { return spaceWidth; }
  inline vint32 getHeight () const { return fontHeight; }
  inline vint32 getMinWidth () const { return minWidth; }
  inline vint32 getMaxWidth () const { return maxWidth; }
  inline vint32 getAvgWidth () const { return avgWidth; }
  inline const VOpenGLTexture *getTexture () const { return tex; }

public:
  static NUIFont *findFont (VName name);
};


static VOpenGLTexture *txHead = nullptr, *txTail = nullptr;
static TMap<VStr, VOpenGLTexture *> txLoaded;


// ////////////////////////////////////////////////////////////////////////// //
class PcfFont {
public:
  const vuint32 PCF_PROPERTIES = 1<<0;
  const vuint32 PCF_ACCELERATORS = 1<<1;
  const vuint32 PCF_METRICS = 1<<2;
  const vuint32 PCF_BITMAPS = 1<<3;
  const vuint32 PCF_INK_METRICS = 1<<4;
  const vuint32 PCF_BDF_ENCODINGS = 1<<5;
  const vuint32 PCF_SWIDTHS = 1<<6;
  const vuint32 PCF_GLYPH_NAMES = 1<<7;
  const vuint32 PCF_BDF_ACCELERATORS = 1<<8;

  const vuint32 PCF_DEFAULT_FORMAT = 0x00000000;
  const vuint32 PCF_INKBOUNDS = 0x00000200;
  const vuint32 PCF_ACCEL_W_INKBOUNDS = 0x00000100;
  const vuint32 PCF_COMPRESSED_METRICS = 0x00000100;

  const vuint32 PCF_GLYPH_PAD_MASK = 3<<0; // See the bitmap table for explanation
  const vuint32 PCF_BYTE_MASK = 1<<2; // If set then Most Sig Byte First
  const vuint32 PCF_BIT_MASK = 1<<3; // If set then Most Sig Bit First
  const vuint32 PCF_SCAN_UNIT_MASK = 3<<4; // See the bitmap table for explanation

  struct CharMetrics {
    int leftSidedBearing;
    int rightSideBearing;
    int width;
    int ascent;
    int descent;
    vuint32 attrs;

    CharMetrics () : leftSidedBearing(0), rightSideBearing(0), width(0), ascent(0), descent(0), attrs(0) {}

    void clear () { leftSidedBearing = rightSideBearing = width = ascent = descent = 0; attrs = 0; }

    inline bool operator == (const CharMetrics &ci) const {
      return
        leftSidedBearing == ci.leftSidedBearing &&
        rightSideBearing == ci.rightSideBearing &&
        width == ci.width &&
        ascent == ci.ascent &&
        descent == ci.descent &&
        attrs == ci.attrs;
    }

    /*
    string toString () const {
      import std.format : format;
      return "leftbearing=%d; rightbearing=%d; width=%d; ascent=%d; descent=%d; attrs=%u".format(leftSidedBearing, rightSideBearing, width, ascent, descent, attrs);
    }
    */
  };

  struct TocEntry {
    vuint32 type; // See below, indicates which table
    vuint32 format; // See below, indicates how the data are formatted in the table
    vuint32 size; // In bytes
    vuint32 offset; // from start of file

    inline bool isTypeCorrect () const {
      if (type == 0) return false;
      if (type&((~1)<<8)) return false;
      bool was1 = false;
      for (unsigned shift = 0; shift < 9; ++shift) {
        if (type&(1<<shift)) {
          if (was1) return false;
          was1 = true;
        }
      }
      return true;
    }
  };

  /*
  struct Prop {
    VStr name;
    VStr sv; // empty: this is integer property
    vuint32 iv;

    inline bool isString () const { return (*sv != nullptr); }
    inline bool isInt () const { return (*sv == nullptr); }
  };
  */

  struct Glyph {
    //vuint32 index; // glyph index
    vuint32 codepoint;
    CharMetrics metrics;
    CharMetrics inkmetrics;
    vuint8* bitmap; // may be greater than necessary; shared between all glyphs; don't destroy
    vuint32 bmpfmt;
    //VStr name; // may be absent

    Glyph () : codepoint(0), metrics(), inkmetrics(), bitmap(nullptr), bmpfmt(0) {}
    Glyph (const Glyph &g) : codepoint(0), metrics(), inkmetrics(), bitmap(nullptr), bmpfmt(0) {
      //index = g.index;
      codepoint = g.codepoint;
      metrics = g.metrics;
      inkmetrics = g.inkmetrics;
      bitmap = g.bitmap;
      bmpfmt = g.bmpfmt;
      //name = g.name;
    }

    void clear () {
      //index = 0;
      codepoint = 0;
      metrics.clear();
      inkmetrics.clear();
      bitmap = nullptr;
      bmpfmt = 0;
      //name.clear();
    }

    void operator = (const Glyph &g) {
      if (&g != this) {
        //index = g.index;
        codepoint = g.codepoint;
        metrics = g.metrics;
        inkmetrics = g.inkmetrics;
        bitmap = g.bitmap;
        bmpfmt = g.bmpfmt;
        //name = g.name;
      }
    }

    inline int bmpWidth () const { return (metrics.width > 0 ? metrics.width : 0); }
    inline int bmpHeight () const { return (metrics.ascent+metrics.descent > 0 ? metrics.ascent+metrics.descent : 0); }

    /*
    void dumpMetrics () const nothrow @trusted {
      conwriteln(
          "left bearing : ", metrics.leftSidedBearing,
        "\nright bearing: ", metrics.rightSideBearing,
        "\nwidth        : ", metrics.width,
        "\nascent       : ", metrics.ascent,
        "\ndescent      : ", metrics.descent,
        "\nattrs        : ", metrics.attrs,
      );
      if (inkmetrics != metrics) {
        conwriteln(
            "ink left bearing : ", inkmetrics.leftSidedBearing,
          "\nink right bearing: ", inkmetrics.rightSideBearing,
          "\nink width        : ", inkmetrics.width,
          "\nink ascent       : ", inkmetrics.ascent,
          "\nink descent      : ", inkmetrics.descent,
          "\nink attrs        : ", inkmetrics.attrs,
        );
      }
    }

    void dumpBitmapFormat () const nothrow @trusted {
      conwriteln("bitmap width (in pixels) : ", bmpwidth);
      conwriteln("bitmap height (in pixels): ", bmpheight);
      conwriteln("bitmap element size      : ", 1<<(bmpfmt&3), " bytes");
      conwriteln("byte order               : ", (bmpfmt&4 ? "M" : "L"), "SByte first");
      conwriteln("bit order                : ", (bmpfmt&8 ? "M" : "L"), "SBit first");
    }
    */

    bool getPixel (int x, int y) const {
      int hgt = metrics.ascent+metrics.descent;
      if (x < 0 || y < 0 || x >= metrics.width || y >= hgt) return false;
      /* the byte order (format&4 => LSByte first)*/
      /* the bit order (format&8 => LSBit first) */
      /* how each row in each glyph's bitmap is padded (format&3) */
      /*  0=>bytes, 1=>shorts, 2=>ints */
      /* what the bits are stored in (bytes, shorts, ints) (format>>4)&3 */
      /*  0=>bytes, 1=>shorts, 2=>ints */
      const vuint32 bytesPerItem = 1<<(bmpfmt&3);
      const vuint32 bytesWidth = metrics.width/8+(metrics.width%8 ? 1 : 0);
      const vuint32 bytesPerLine = ((bytesWidth/bytesPerItem)+(bytesWidth%bytesPerItem ? 1 : 0))*bytesPerItem;
      const vuint32 ofs = y*bytesPerLine+x/(bytesPerItem*8)+(bmpfmt&4 ? (x/8%bytesPerItem) : 1-(x/8%bytesPerItem));
      //!!!if (ofs >= bitmap.length) return false;
      const vuint8 mask = (vuint8)(bmpfmt&8 ? 0x80>>(x%8) : 0x01<<(x%8));
      return ((bitmap[ofs]&mask) != 0);
    }
  };

public:
  //Prop[VStr] props;
  TArray<Glyph> glyphs;
  //vuint32[dchar] glyphc2i; // codepoint -> glyph index
  /*dchar*/vuint32 defaultchar;
  vuint8* bitmaps;
  vuint32 bitmapsSize;

public:
  CharMetrics minbounds, maxbounds;
  CharMetrics inkminbounds, inkmaxbounds;
  bool noOverlap;
  bool constantMetrics;
  bool terminalFont;
  bool constantWidth;
  bool inkInside;
  bool inkMetrics;
  bool drawDirectionLTR;
  int padding;
  int fntAscent;
  int fntDescent;
  int maxOverlap;

private:
  void clearInternal () {
    glyphs.clear();
    defaultchar = 0;
    delete[] bitmaps;
    bitmaps = nullptr;
    bitmapsSize = 0;
    minbounds.clear();
    maxbounds.clear();
    inkminbounds.clear();
    inkmaxbounds.clear();
    noOverlap = false;
    constantMetrics = false;
    terminalFont = false;
    constantWidth = false;
    inkInside = false;
    inkMetrics = false;
    drawDirectionLTR = false;
    padding = 0;
    fntAscent = 0;
    fntDescent = 0;
    maxOverlap = 0;
  }

public:
  PcfFont () : glyphs(), bitmaps(nullptr), bitmapsSize(0) { clearInternal(); }

  ~PcfFont () { clear(); }

  inline bool isValid () const { return (glyphs.length() > 0); }

  inline int height () const { return fntAscent+fntDescent; }

  VOpenGLTexture *createGlyphTexture (int gidx) {
    Glyph &gl = glyphs[gidx];
    //!fprintf(stderr, "glyph #%d (ch=%u); wdt=%d; hgt=%d\n", gidx, gl.codepoint, gl.bmpWidth(), gl.bmpHeight());
    if (gl.bmpWidth() < 1 || gl.bmpHeight() < 1) return nullptr;
    VOpenGLTexture *tex = VOpenGLTexture::CreateEmpty(VStr::EmptyString, gl.bmpWidth(), gl.bmpHeight());
    for (int dy = 0; dy < gl.bmpHeight(); ++dy) {
      //!fprintf(stderr, "  ");
      for (int dx = 0; dx < gl.bmpWidth(); ++dx) {
        bool pix = gl.getPixel(dx, dy);
        //!fprintf(stderr, "%c", (pix ? '#' : '.'));
        tex->img->setPixel(dx, dy, (pix ? VImage::RGBA(255, 255, 255, 255) : VImage::RGBA(0, 0, 0, 0)));
      }
      //!fprintf(stderr, "\n");
    }
    return tex;
  }

  // return width
  /*
  bool drawGlyph (ref CharMetrics mtr, dchar dch, scope void delegate (in ref CharMetrics mtr, int x, int y) nothrow @trusted @nogc putPixel) nothrow @trusted @nogc {
    if (!valid) return false;
    auto gip = dch in glyphc2i;
    if (gip is null) {
      gip = defaultchar in glyphc2i;
      if (gip is null) return false;
    }
    if (*gip >= glyphs.length) return false;
    auto glp = glyphs.ptr+(*gip);
    if (putPixel !is null) {
      // draw it
      int y = -glp.metrics.ascent;
      version(all) {
        foreach (immutable dy; 0..glp.bmpheight) {
          foreach (immutable dx; 0..glp.bmpwidth) {
            if (glp.getPixel(dx, dy)) putPixel(glp.metrics, dx, y);
          }
          ++y;
        }
      } else {
        foreach (immutable dy; 0..glp.bmpheight) {
          foreach (immutable dx; glp.metrics.leftSidedBearing..glp.metrics.rightSideBearing+1) {
            if (glp.getPixel(dx-glp.metrics.leftSidedBearing, dy)) putPixel(glp.metrics, dx-glp.metrics.leftSidedBearing, y);
          }
          ++y;
        }
      }
    }
    mtr = glp.metrics;
    return true;
    //return glp.inkmetrics.leftSidedBearing+glp.inkmetrics.rightSideBearing;
    //return glp.metrics.rightSideBearing;
    //return glp.metrics.width;
  }
  */

  void clear () {
    clearInternal();
  }

  bool load (VStream &fl) {
    clear();

    vuint32 bmpfmt = 0;
    bool bebyte = false;
    bool bebit = false;
    vuint32 curfmt;

#define resetFormat()  do { bebyte = false; bebit = false; } while (0)

#define setupFormat(atbl)  do { \
  curfmt = atbl.format; \
  bebyte = ((atbl.format&(1<<2)) != 0); \
  bebit = ((atbl.format&(1<<3)) == 0); \
} while (0)

#define readInt(atype,dest)  do { \
  dest = 0; \
  vuint8 tmpb; \
  if (bebyte) { \
    for (unsigned f = 0; f < sizeof(atype); ++f) { \
      fl.Serialise(&tmpb, 1); \
      dest <<= 8; \
      dest |= tmpb; \
    } \
  } else { \
    for (unsigned f = 0; f < sizeof(atype); ++f) { \
      fl.Serialise(&tmpb, 1); \
      dest |= ((vuint32)tmpb)<<(f*8); \
    } \
  } \
  \
  if (bebit) { \
    atype nv = 0; \
    for (unsigned sft = 0; sft < sizeof(atype)*8; ++sft) { \
      bool set = ((dest&(1U<<sft)) != 0); \
      if (set) nv |= 1U<<(sizeof(atype)-sft-1); \
    } \
    dest = nv; \
  } \
  if (fl.IsError()) return false; \
} while (0)

#define readMetrics(mt)  do { \
  if ((curfmt&~0xffU) == PCF_DEFAULT_FORMAT) { \
    readInt(vint16, mt.leftSidedBearing); \
    readInt(vint16, mt.rightSideBearing); \
    readInt(vint16, mt.width); \
    readInt(vint16, mt.ascent); \
    readInt(vint16, mt.descent); \
    readInt(vuint16, mt.attrs); \
  } else if ((curfmt&~0xffU) == PCF_COMPRESSED_METRICS) { \
    readInt(vuint8, mt.leftSidedBearing); mt.leftSidedBearing -= 0x80; \
    readInt(vuint8, mt.rightSideBearing); mt.rightSideBearing -= 0x80; \
    readInt(vuint8, mt.width); mt.width -= 0x80; \
    readInt(vuint8, mt.ascent); mt.ascent -= 0x80; \
    readInt(vuint8, mt.descent); mt.descent -= 0x80; \
    mt.attrs = 0; \
  } else { \
    return false; \
  } \
} while (0)

#define readBool(dest)  do { \
  vuint8 tmpb; \
  fl.Serialise(&tmpb, 1); \
  dest = (tmpb != 0); \
} while (0)

    char sign[4];
    auto fstart = fl.Tell();
    fl.Serialise(sign, 4);
    if (memcmp(sign, "\x01" "fcp", 4) != 0) return false;
    vuint32 tablecount;
    readInt(vuint32, tablecount);
    //version(pcf_debug) conwriteln("tables: ", tablecount);
    if (tablecount == 0 || tablecount > 128) return false;

    // load TOC
    TocEntry tables[128];
    memset(tables, 0, sizeof(tables));
    for (unsigned tidx = 0; tidx < tablecount; ++tidx) {
      TocEntry &tbl = tables[tidx];
      readInt(vuint32, tbl.type);
      readInt(vuint32, tbl.format);
      readInt(vuint32, tbl.size);
      readInt(vuint32, tbl.offset);
      if (!tbl.isTypeCorrect()) return false;
      //conwriteln("table #", tidx, " is '", tbl.typeName, "'");
    }

    // load properties
    /*
    for (unsigned tidx = 0; tidx < tablecount; ++tidx) {
      TocEntry &tbl = tables[tidx];
      if (tbl.type == PCF_PROPERTIES) {
        if (props !is null) throw new Exception("more than one property table in PCF");
        fl.seek(fstart+tbl.offset);
        auto fmt = fl.readNum!vuint32;
        assert(fmt == tbl.format);
        setupFormat(tbl);
        version(pcf_debug) conwriteln("property table format: ", fmt);
        auto count = readInt!vuint32;
        version(pcf_debug) conwriteln(count, " properties");
        if (count > int.max/16) throw new Exception("string table too big");
        static struct Prp { vuint32 nofs; vuint8 strflag; vuint32 value; }
        Prp[] fprops;
        foreach (; 0..count) {
          fprops.arrAppend(Prp());
          fprops[$-1].nofs = readInt!vuint32;
          fprops[$-1].strflag = readInt!vuint8;
          fprops[$-1].value = readInt!vuint32;
        }
        while ((fl.tell-fstart)&3) fl.readNum!vuint8;
        vuint32 stblsize = readInt!vuint32;
        if (stblsize > int.max/16) throw new Exception("string table too big");
        char[] stbl;
        scope(exit) delete stbl;
        stbl.length = stblsize;
        if (stbl.length) fl.rawReadExact(stbl);
        // build property table
        foreach (const ref prp; fprops) {
          vuint32 nend = prp.nofs;
          if (nend >= stbl.length || stbl.ptr[nend] == 0) throw new Exception("invalid property name offset");
          while (nend < stbl.length && stbl.ptr[nend]) ++nend;
          VStr pname = stbl[prp.nofs..nend].idup;
          if (prp.strflag) {
            nend = prp.value;
            if (nend >= stbl.length) throw new Exception("invalid property value offset");
            while (nend < stbl.length && stbl.ptr[nend]) ++nend;
            VStr pval = stbl[prp.value..nend].idup;
            version(pcf_debug) conwriteln("  STR property '", pname, "' is '", pval, "'");
            props[pname] = Prop(pname, pval, 0);
          } else {
            version(pcf_debug) conwriteln("  INT property '", pname, "' is ", prp.value);
            props[pname] = Prop(pname, null, prp.value);
          }
        }
      }
    }
    //if (props is null) throw new Exception("no property table in PCF");
    */

    //vuint32[] gboffsets; // glyph bitmap offsets
    //scope(exit) delete gboffsets;
    TArray<vuint32> gboffsets;

    // load bitmap table
    for (unsigned tidx = 0; tidx < tablecount; ++tidx) {
      TocEntry &tbl = tables[tidx];
      if (tbl.type == PCF_BITMAPS) {
        if (bitmaps) return false;
        fl.Seek(fstart+tbl.offset);
        resetFormat();
        vuint32 fmt;
        readInt(vuint32, fmt);
        if (fmt != tbl.format) return false;
        setupFormat(tbl);
        bmpfmt = tbl.format;
        vuint32 count;
        readInt(vuint32, count);
        if (count == 0) return false;
        if (count > 0x00ffffff) return false;
        gboffsets.SetNum((int)count);
        for (int oidx = 0; oidx < (int)count; ++oidx) {
          vuint32 v;
          readInt(vuint32, v);
          gboffsets[oidx] = v;
        }
        vuint32 sizes[4];
        readInt(vuint32, sizes[0]);
        readInt(vuint32, sizes[1]);
        readInt(vuint32, sizes[2]);
        readInt(vuint32, sizes[3]);
        vuint32 realsize = sizes[bmpfmt&3];
        if (realsize == 0) return 0;
        if (realsize > 0x00ffffff) return 0;
        bitmaps = new vuint8[realsize];
        bitmapsSize = realsize;
        fl.Serialise(bitmaps, realsize);
        if (fl.IsError()) return false;
        //bitmaps.length = realsize;
        //bitmaps.gcMarkHeadAnchor;
        //fl.rawReadExact(bitmaps);
        //version(pcf_debug) conwriteln(realsize, " bytes of bitmap data for ", count, " glyphs");
        /*!!!
        foreach (immutable idx, immutable v; gboffsets) {
          if (v >= bitmaps.length) { import std.format : format; throw new Exception("invalid bitmap data offset (0x%08x) for glyph #%d in PCF".format(v, idx)); }
        }
        */
      }
    }
    if (!bitmaps) return false;

    // load encoding table, fill glyph table
    glyphs.SetNum(gboffsets.length());
    //immutable int bytesPerItem = 1<<(bmpfmt&3);
    for (int idx = 0; idx < glyphs.length(); ++idx) {
      Glyph &gl = glyphs[idx];
      //gl.index = (vuint32)idx;
      //uint realofs = gboffsets[idx]*bytesPerItem;
      //if (realofs >= bitmaps.length) throw new Exception("invalid glyph bitmap offset in PCF");
      vuint32 realofs = gboffsets[idx];
      gl.bitmap = bitmaps+realofs;
      gl.bmpfmt = bmpfmt;
    }

    bool encodingsFound = false;
    for (unsigned tidx = 0; tidx < tablecount; ++tidx) {
      TocEntry &tbl = tables[tidx];
      if (tbl.type == PCF_BDF_ENCODINGS) {
        if (encodingsFound) return false;
        encodingsFound = true;
        fl.Seek(fstart+tbl.offset);
        resetFormat();
        vuint32 fmt;
        readInt(vuint32, fmt);
        if (fmt != tbl.format) return false;
        setupFormat(tbl);
        vuint16 mincb2; readInt(vuint16, mincb2);
        vuint16 maxcb2; readInt(vuint16, maxcb2);
        vuint16 minb1; readInt(vuint16, minb1);
        vuint16 maxb1; readInt(vuint16, maxb1);
        readInt(vuint16, defaultchar);
        if (minb1 == 0 && minb1 == maxb1) {
          // single-byte encoding
          for (vuint32 ci = mincb2; ci <= maxcb2; ++ci) {
            //immutable dchar dch = cast(dchar)ci;
            vuint32 gidx; readInt(vuint16, gidx);
            if (gidx != 0xffff) {
              if (gidx >= (vuint32)gboffsets.length()) return false;
              //glyphc2i[dch] = gidx;
              glyphs[gidx].codepoint = ci;
            }
          }
        } else {
          // multi-byte encoding
          for (vuint32 hibyte = minb1; hibyte <= maxb1; ++hibyte) {
            for (vuint32 lobyte = mincb2; lobyte <= maxcb2; ++lobyte) {
              //immutable dchar dch = cast(dchar)((hibyte<<8)|lobyte);
              vuint32 gidx; readInt(vuint16, gidx);
              if (gidx != 0xffff) {
                if (gidx >= (vuint32)gboffsets.length()) return false;
                //glyphc2i[dch] = gidx;
                glyphs[gidx].codepoint = ((hibyte<<8)|lobyte);
              }
            }
          }
        }
        /*
        version(pcf_debug) {{
          import std.algorithm : sort;
          conwriteln(glyphc2i.length, " glyphs in font");
          //foreach (dchar dch; glyphc2i.values.sort) conwritefln!"  \\u%04X"(cast(uint)dch);
        }}
        */
      }
    }
    if (!encodingsFound) return false;

    // load metrics
    bool metricsFound = false;
    for (unsigned tidx = 0; tidx < tablecount; ++tidx) {
      TocEntry &tbl = tables[tidx];
      if (tbl.type == PCF_METRICS) {
        if (metricsFound) return false;
        metricsFound = true;
        fl.Seek(fstart+tbl.offset);
        resetFormat();
        vuint32 fmt;
        readInt(vuint32, fmt);
        if (fmt != tbl.format) return false;
        setupFormat(tbl);
        vuint32 count;
        if ((curfmt&~0xffU) == PCF_DEFAULT_FORMAT) {
          //conwriteln("metrics aren't compressed");
          readInt(vuint32, count);
        } else if ((curfmt&~0xffU) == PCF_COMPRESSED_METRICS) {
          //conwriteln("metrics are compressed");
          readInt(vuint16, count);
        } else {
          return false;
        }
        if (count < (vuint32)glyphs.length()) return false;
        for (int gidx = 0; gidx < glyphs.length(); ++gidx) {
          Glyph &gl = glyphs[gidx];
          readMetrics(gl.metrics);
          gl.inkmetrics = gl.metrics;
        }
      }
    }
    if (!metricsFound) return false;

    // load ink metrics (if any)
    metricsFound = false;
    for (unsigned tidx = 0; tidx < tablecount; ++tidx) {
      TocEntry &tbl = tables[tidx];
      if (tbl.type == PCF_INK_METRICS) {
        if (metricsFound) return false;
        metricsFound = true;
        fl.Seek(fstart+tbl.offset);
        resetFormat();
        vuint32 fmt;
        readInt(vuint32, fmt);
        if (fmt != tbl.format) return false;
        setupFormat(tbl);
        vuint32 count;
        if ((curfmt&~0xffU) == PCF_DEFAULT_FORMAT) {
          //conwriteln("ink metrics aren't compressed");
          readInt(vuint32, count);
        } else if ((curfmt&~0xffU) == PCF_COMPRESSED_METRICS) {
          //conwriteln("ink metrics are compressed");
          readInt(vuint16, count);
        } else {
          return false;
        }
        if (count > (vuint32)glyphs.length()) count = (vuint32)glyphs.length();
        for (int gidx = 0; gidx < glyphs.length(); ++gidx) {
          Glyph &gl = glyphs[gidx];
          readMetrics(gl.inkmetrics);
        }
      }
    }
    //version(pcf_debug) conwriteln("ink metrics found: ", metricsFound);

    // load glyph names (if any)
    /*
    for (unsigned tidx = 0; tidx < tablecount; ++tidx) {
      TocEntry &tbl = tables[tidx];
      if (tbl.type == PCF_GLYPH_NAMES) {
        fl.seek(fstart+tbl.offset);
        auto fmt = fl.readNum!vuint32;
        assert(fmt == tbl.format);
        setupFormat(tbl);
        vuint32 count = readInt!vuint32;
        if (count == 0) break;
        if (count > int.max/16) break;
        auto nofs = new vuint32[](count);
        scope(exit) delete nofs;
        foreach (ref v; nofs) v = readInt!vuint32;
        vuint32 stblsize = readInt!vuint32;
        if (stblsize > int.max/16) break;
        auto stbl = new char[](stblsize);
        scope(exit) delete stbl;
        fl.rawReadExact(stbl);
        if (count > glyphs.length) count = cast(vuint32)glyphs.length;
        foreach (ref gl; glyphs[0..count]) {
          vuint32 stofs = nofs[gl.index];
          if (stofs >= stbl.length) break;
          vuint32 eofs = stofs;
          while (eofs < stbl.length && stbl.ptr[eofs]) ++eofs;
          if (eofs > stofs) gl.name = stbl[stofs..eofs].idup;
        }
      }
    }
    */

    /* not needed
    //int mFontHeight = 0;
    mFontMinWidth = int.max;
    mFontMaxWidth = 0;
    foreach (const ref gl; glyphs) {
      if (mFontMinWidth > gl.inkmetrics.width) mFontMinWidth = gl.inkmetrics.width;
      if (mFontMaxWidth < gl.inkmetrics.width) mFontMaxWidth = gl.inkmetrics.width;
      //int hgt = gl.inkmetrics.ascent+gl.inkmetrics.descent;
      //if (mFontHeight < hgt) mFontHeight = hgt;
    }
    if (mFontMinWidth < 0) mFontMinWidth = 0;
    if (mFontMaxWidth < 0) mFontMaxWidth = 0;
    */

    // load accelerators
    int accelTbl = -1;
    for (unsigned tidx = 0; tidx < tablecount; ++tidx) {
      TocEntry &tbl = tables[tidx];
      if (accelTbl < 0 && (tbl.type == PCF_ACCELERATORS || tbl.type == PCF_BDF_ACCELERATORS)) {
        accelTbl = (int)tidx;
        if (tbl.type == PCF_BDF_ACCELERATORS) break;
      }
    }

    if (accelTbl >= 0) {
      CharMetrics minbounds, maxbounds;
      CharMetrics inkminbounds, inkmaxbounds;
      TocEntry &tbl = tables[accelTbl];
      fl.Seek(fstart+tbl.offset);
      resetFormat();
      vuint32 fmt;
      readInt(vuint32, fmt);
      if (fmt != tbl.format) return false;
      setupFormat(tbl);
      // load font parameters
      readBool(noOverlap);
      readBool(constantMetrics);
      readBool(terminalFont);
      readBool(constantWidth);
      readBool(inkInside);
      readBool(inkMetrics);
      readBool(drawDirectionLTR);
      readInt(vuint8, padding);
      readInt(vint32, fntAscent);
      readInt(vint32, fntDescent);
      readInt(vint32, maxOverlap);
      readInt(vint16, minbounds.leftSidedBearing);
      readInt(vint16, minbounds.rightSideBearing);
      readInt(vint16, minbounds.width);
      readInt(vint16, minbounds.ascent);
      readInt(vint16, minbounds.descent);
      readInt(vuint16, minbounds.attrs);
      readInt(vint16, maxbounds.leftSidedBearing);
      readInt(vint16, maxbounds.rightSideBearing);
      readInt(vint16, maxbounds.width);
      readInt(vint16, maxbounds.ascent);
      readInt(vint16, maxbounds.descent);
      readInt(vuint16, maxbounds.attrs);
      if ((curfmt&~0xff) == PCF_ACCEL_W_INKBOUNDS) {
        readInt(vint16, inkminbounds.leftSidedBearing);
        readInt(vint16, inkminbounds.rightSideBearing);
        readInt(vint16, inkminbounds.width);
        readInt(vint16, inkminbounds.ascent);
        readInt(vint16, inkminbounds.descent);
        readInt(vuint16, inkminbounds.attrs);
        readInt(vint16, inkmaxbounds.leftSidedBearing);
        readInt(vint16, inkmaxbounds.rightSideBearing);
        readInt(vint16, inkmaxbounds.width);
        readInt(vint16, inkmaxbounds.ascent);
        readInt(vint16, inkmaxbounds.descent);
        readInt(vuint16, inkmaxbounds.attrs);
      } else {
        inkminbounds = minbounds;
        inkmaxbounds = maxbounds;
      }
      /*
      version(pcf_debug) {
        conwriteln("noOverlap       : ", noOverlap);
        conwriteln("constantMetrics : ", constantMetrics);
        conwriteln("terminalFont    : ", terminalFont);
        conwriteln("constantWidth   : ", constantWidth);
        conwriteln("inkInside       : ", inkInside);
        conwriteln("inkMetrics      : ", inkMetrics);
        conwriteln("drawDirectionLTR: ", drawDirectionLTR);
        conwriteln("padding         : ", padding);
        conwriteln("fntAscent       : ", fntAscent);
        conwriteln("fntDescent      : ", fntDescent);
        conwriteln("maxOverlap      : ", maxOverlap);
        conwriteln("minbounds       : ", minbounds);
        conwriteln("maxbounds       : ", maxbounds);
        conwriteln("ink_minbounds   : ", inkminbounds);
        conwriteln("ink_maxbounds   : ", inkmaxbounds);
      }
      */
    } else {
      //throw new Exception("no accelerator info found in PCF");
      return false;
    }

    /*
    version(pcf_debug) {
      //conwriteln("min width: ", mFontMinWidth);
      //conwriteln("max width: ", mFontMaxWidth);
      //conwriteln("height   : ", mFontHeight);
      version(pcf_debug_dump_bitmaps) {
        foreach (const ref gl; glyphs) {
          conwritefln!"=== glyph #%u (\\u%04X) %dx%d==="(gl.index, cast(vuint32)gl.codepoint, gl.bmpwidth, gl.bmpheight);
          if (gl.name.length) conwriteln("NAME: ", gl.name);
          conwritefln!"bitmap offset: 0x%08x"(gboffsets[gl.index]);
          gl.dumpMetrics();
          gl.dumpBitmapFormat();
          foreach (immutable int y; 0..gl.bmpheight) {
            conwrite("  ");
            foreach (immutable int x; 0..gl.bmpwidth) {
              if (gl.getPixel(x, y)) conwrite("##"); else conwrite("..");
            }
            conwriteln;
          }
        }
      }
    }
    */
#undef readBool
#undef readMetrics
#undef readInt
#undef setupFormat
    return true;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
struct ScissorRect {
  int x, y, w, h;
  int enabled;
};


// ////////////////////////////////////////////////////////////////////////// //
static bool texUpload (VOpenGLTexture *tx) {
  if (!tx) return false;
  if (!tx->img) { tx->tid = 0; return false; }
  if (tx->tid) return true;

  tx->tid = 0;
  glGenTextures(1, &tx->tid);
  if (tx->tid == 0) return false;

  //fprintf(stderr, "uploading texture '%s'\n", *tx->getPath());

  glBindTexture(GL_TEXTURE_2D, tx->tid);
  /*
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  */
  VGLVideo::forceGLTexFilter();

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tx->img->width, tx->img->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr); // this creates texture

  if (!tx->img->isTrueColor) {
    VImage *tc = new VImage(VImage::ImageType::IT_RGBA, tx->img->width, tx->img->height);
    for (int y = 0; y < tx->img->height; ++y) {
      for (int x = 0; x < tx->img->width; ++x) {
        tc->setPixel(x, y, tx->img->getPixel(x, y));
      }
    }
    delete tx->img;
    tx->img = tc;
    tc->smoothEdges();
  }
  //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tc->width, tc->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tc->pixels);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0/*x*/, 0/*y*/, tx->img->width, tx->img->height, GL_RGBA, GL_UNSIGNED_BYTE, tx->img->pixels); // this updates texture

  return true;
}


void unloadAllTextures () {
  if (!NewUIICB::inited) return;
  glBindTexture(GL_TEXTURE_2D, 0);
  for (VOpenGLTexture *tx = txHead; tx; tx = tx->next) {
    if (tx->tid) { glDeleteTextures(1, &tx->tid); tx->tid = 0; }
  }
}


void uploadAllTextures () {
  if (!NewUIICB::inited) return;
  for (VOpenGLTexture *tx = txHead; tx; tx = tx->next) texUpload(tx);
}


// ////////////////////////////////////////////////////////////////////////// //
VOpenGLTexture::VOpenGLTexture (VImage *aimg, const VStr &apath)
  : rc(1)
  , mPath(apath)
  , img(aimg)
  , tid(0)
  , mTransparent(false)
  , mOpaque(false)
  , mOneBitAlpha(false)
  , prev(nullptr)
  , next(nullptr)
{
  analyzeImage();
  if (!NewUIICB::inited) texUpload(this);
  registerMe();
}


// dimensions must be valid!
VOpenGLTexture::VOpenGLTexture (int awdt, int ahgt)
  : rc(1)
  , mPath()
  , img(nullptr)
  , tid(0)
  , mTransparent(false)
  , mOpaque(false)
  , mOneBitAlpha(false)
  , prev(nullptr)
  , next(nullptr)
{
  img = new VImage(VImage::IT_RGBA, awdt, ahgt);
  for (int y = 0; y < ahgt; ++y) {
    for (int x = 0; x < awdt; ++x) {
      img->setPixel(x, y, VImage::RGBA(0, 0, 0, 0)); // transparent
    }
  }
  analyzeImage();
  //if (!NewUIICB::inited) texUpload(this);
  registerMe();
}


VOpenGLTexture::~VOpenGLTexture () {
  if (NewUIICB::inited && tid) {
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &tid);
  }
  tid = 0;
  delete img;
  img = nullptr;
  if (mPath.length()) {
    txLoaded.remove(mPath);
  } else {
    /*
    for (int idx = 0; idx < txLoadedUnnamed.length(); ++idx) {
      if (txLoadedUnnamed[idx] == this) {
        txLoadedUnnamed.removeAt(idx);
        break;
      }
    }
    */
  }
  mPath = VStr();
  if (!prev && !next) {
    if (txHead == this) { txHead = txTail = nullptr; }
  } else {
    if (prev) prev->next = next; else txHead = next;
    if (next) next->prev = prev; else txTail = prev;
  }
  prev = next = nullptr;
}


void VOpenGLTexture::registerMe () {
  if (prev || next) return;
  if (txHead == this) return;
  prev = txTail;
  if (txTail) txTail->next = this; else txHead = this;
  txTail = this;
}


void VOpenGLTexture::analyzeImage () {
  if (img) {
    img->smoothEdges();
    mTransparent = true;
    mOpaque = true;
    mOneBitAlpha = true;
    for (int y = 0; y < img->height; ++y) {
      for (int x = 0; x < img->width; ++x) {
        VImage::RGBA pix = img->getPixel(x, y);
        if (pix.a != 0 && pix.a != 255) {
          mOneBitAlpha = false;
          mTransparent = false;
          mOpaque = false;
          break; // no need to analyze more
        } else {
               if (pix.a != 0) mTransparent = false;
          else if (pix.a != 255) mOpaque = false;
        }
      }
    }
  } else {
    mTransparent = false;
    mOpaque = false;
    mOneBitAlpha = false;
  }
}


void VOpenGLTexture::addRef () {
  ++rc;
}


void VOpenGLTexture::release () {
  if (--rc == 0) delete this;
}


//FIXME: optimize this!
void VOpenGLTexture::update () {
  if (NewUIICB::inited && tid) {
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &tid);
  }
  tid = 0;
  analyzeImage();
  if (NewUIICB::inited) texUpload(this);
}


VOpenGLTexture *VOpenGLTexture::Load (const VStr &fname) {
  if (fname.length() == 0) return nullptr;
  if (fname.length() > 0) {
    VOpenGLTexture **loaded = txLoaded.find(fname);
    if (loaded) { (*loaded)->addRef(); return *loaded; }
  }
  VOpenGLTexture **loaded = txLoaded.find(fname);
  if (loaded) { (*loaded)->addRef(); return *loaded; }
  VStream *st = FL_OpenFileRead(fname);
  if (!st) return nullptr;
  VImage *img = VImage::loadFrom(st);
  delete st;
  if (!img) return nullptr;
  VOpenGLTexture *res = new VOpenGLTexture(img, fname);
  txLoaded.put(fname, res);
  //fprintf(stderr, "TXLOADED: '%s' rc=%d, (%p)\n", *res->mPath, res->rc, res);
  return res;
}


VOpenGLTexture *VOpenGLTexture::CreateEmpty (VStr txname, int wdt, int hgt) {
  if (txname.length()) {
    VOpenGLTexture **loaded = txLoaded.find(txname);
    if (loaded) {
      if ((*loaded)->width != wdt || (*loaded)->height != hgt) return nullptr; // oops
      (*loaded)->addRef();
      return *loaded;
    }
  }
  if (wdt < 1 || hgt < 1 || wdt > 32768 || hgt > 32768) return nullptr;
  VOpenGLTexture *res = new VOpenGLTexture(wdt, hgt);
  if (txname.length()) txLoaded.put(txname, res); //else txLoadedUnnamed.append(res);
  //fprintf(stderr, "TXLOADED: '%s' rc=%d, (%p)\n", *res->mPath, res->rc, res);
  return res;
}


void VOpenGLTexture::blitExt (int dx0, int dy0, int dx1, int dy1, int x0, int y0, int x1, int y1, float angle) const {
  if (!tid /*|| VGLVideo::isFullyTransparent() || mTransparent*/) return;
  if (x1 < 0) x1 = img->width;
  if (y1 < 0) y1 = img->height;
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, tid);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  //glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  VGLVideo::forceGLTexFilter();

  if (VGLVideo::getBlendMode() == VGLVideo::BlendNormal) {
    if (mOpaque && VGLVideo::isFullyOpaque()) {
      glDisable(GL_BLEND);
    } else {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
  } else {
    VGLVideo::setupBlending();
  }

  const float fx0 = (float)x0/(float)img->width;
  const float fx1 = (float)x1/(float)img->width;
  const float fy0 = (float)y0/(float)img->height;
  const float fy1 = (float)y1/(float)img->height;
  const float z = 0;

  if (angle != 0) {
    glPushMatrix();
    glTranslatef(dx0+(dx1-dx0)/2.0, dy0+(dy1-dy0)/2.0, 0);
    glRotatef(angle, 0, 0, 1);
    glTranslatef(-(dx0+(dx1-dx0)/2.0), -(dy0+(dy1-dy0)/2.0), 0);
  }

  glBegin(GL_QUADS);
    glTexCoord2f(fx0, fy0); glVertex3f(dx0, dy0, z);
    glTexCoord2f(fx1, fy0); glVertex3f(dx1, dy0, z);
    glTexCoord2f(fx1, fy1); glVertex3f(dx1, dy1, z);
    glTexCoord2f(fx0, fy1); glVertex3f(dx0, dy1, z);
  glEnd();

  if (angle != 0) glPopMatrix();
}


void VOpenGLTexture::blitExtRep (int dx0, int dy0, int dx1, int dy1, int x0, int y0, int x1, int y1) const {
  if (!tid /*|| VGLVideo::isFullyTransparent() || mTransparent*/) return;
  if (x1 < 0) x1 = img->width;
  if (y1 < 0) y1 = img->height;
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, tid);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  //glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  VGLVideo::forceGLTexFilter();

  if (VGLVideo::getBlendMode() == VGLVideo::BlendNormal) {
    if (mOpaque && VGLVideo::isFullyOpaque()) {
      glDisable(GL_BLEND);
    } else {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
  } else {
    VGLVideo::setupBlending();
  }
  const float z = 0;

  glBegin(GL_QUADS);
    glTexCoord2i(x0, y0); glVertex3f(dx0, dy0, z);
    glTexCoord2i(x1, y0); glVertex3f(dx1, dy0, z);
    glTexCoord2i(x1, y1); glVertex3f(dx1, dy1, z);
    glTexCoord2i(x0, y1); glVertex3f(dx0, dy1, z);
  glEnd();
}


void VOpenGLTexture::blitAt (int dx0, int dy0, float scale, float angle) const {
  if (!tid /*|| VGLVideo::isFullyTransparent() || scale <= 0 || mTransparent*/) return;
  int w = img->width;
  int h = img->height;
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, tid);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  //glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  VGLVideo::forceGLTexFilter();

  if (VGLVideo::getBlendMode() == VGLVideo::BlendNormal) {
    if (mOpaque && VGLVideo::isFullyOpaque()) {
      glDisable(GL_BLEND);
    } else {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
  } else {
    VGLVideo::setupBlending();
  }
  const float z = 0;

  const float dx1 = dx0+w*scale;
  const float dy1 = dy0+h*scale;

  if (angle != 0) {
    glPushMatrix();
    glTranslatef(dx0+(dx1-dx0)/2.0, dy0+(dy1-dy0)/2.0, 0);
    glRotatef(angle, 0, 0, 1);
    glTranslatef(-(dx0+(dx1-dx0)/2.0), -(dy0+(dy1-dy0)/2.0), 0);
  }

  glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(dx0, dy0, z);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(dx1, dy0, z);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(dx1, dy1, z);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(dx0, dy1, z);
  glEnd();

  if (angle != 0) glPopMatrix();
}


// ////////////////////////////////////////////////////////////////////////// //
static TMap<int, VGLTexture *> vcGLTexMap;
//FIXME: rewrite id management
static TArray<int> vcGLFreeIds;
static int vcGLFreeIdsUsed = 0;
static int vcGLLastUsedId = 0;


static int vcGLAllocId (VGLTexture *obj) {
  int res;
  if (vcGLFreeIdsUsed > 0) {
    res = vcGLFreeIds[--vcGLFreeIdsUsed];
  } else {
    // no free ids
    res = ++vcGLLastUsedId;
  }
  vcGLTexMap.put(res, obj);
  return res;
}


static void vcGLFreeId (int id) {
  if (id < 1 || id > vcGLLastUsedId) return;
  vcGLTexMap.remove(id);
  if (vcGLFreeIdsUsed == vcGLFreeIds.length()) {
    vcGLFreeIds.append(id);
    ++vcGLFreeIdsUsed;
  } else {
    vcGLFreeIds[vcGLFreeIdsUsed++] = id;
  }
}


IMPLEMENT_CLASS(V, GLTexture);


void VGLTexture::Destroy () {
  vcGLFreeId(id);
  //fprintf(stderr, "destroying texture object %p\n", this);
  if (tex) {
    //fprintf(stderr, "  releasing texture '%s'... rc=%d, (%p)\n", *tex->getPath(), tex->getRC(), tex);
    tex->release();
    tex = nullptr;
  }
  Super::Destroy();
}


IMPLEMENT_FUNCTION(VGLTexture, Destroy) {
  P_GET_SELF;
  //if (Self) Self->SetFlags(_OF_DelayedDestroy);
  if (Self) Self->ConditionalDestroy();
  //delete Self;
}


//native final static GLTexture Load (string fname);
IMPLEMENT_FUNCTION(VGLTexture, Load) {
  P_GET_STR(fname);
  VOpenGLTexture *tex = VOpenGLTexture::Load(fname);
  if (tex) {
    VGLTexture *ifile = Spawn<VGLTexture>();
    ifile->tex = tex;
    ifile->id = vcGLAllocId(ifile);
    //fprintf(stderr, "created texture object %p (%p)\n", ifile, ifile->tex);
    RET_REF((VObject *)ifile);
    return;
  }
  RET_REF(nullptr);
}


// native final static GLTexture GetById (int id);
IMPLEMENT_FUNCTION(VGLTexture, GetById) {
  P_GET_INT(id);
  if (id > 0 && id <= vcGLLastUsedId) {
    auto opp = vcGLTexMap.find(id);
    if (opp) RET_REF((VGLTexture *)(*opp)); else RET_REF(nullptr);
  } else {
    RET_REF(nullptr);
  }
}


IMPLEMENT_FUNCTION(VGLTexture, width) {
  P_GET_SELF;
  RET_INT(Self && Self->tex ? Self->tex->width : 0);
}

IMPLEMENT_FUNCTION(VGLTexture, height) {
  P_GET_SELF;
  RET_INT(Self && Self->tex ? Self->tex->height : 0);
}

IMPLEMENT_FUNCTION(VGLTexture, isTransparent) {
  P_GET_SELF;
  RET_BOOL(Self && Self->tex ? Self->tex->isTransparent : true);
}

IMPLEMENT_FUNCTION(VGLTexture, isOpaque) {
  P_GET_SELF;
  RET_BOOL(Self && Self->tex ? Self->tex->isOpaque : false);
}

IMPLEMENT_FUNCTION(VGLTexture, isOneBitAlpha) {
  P_GET_SELF;
  RET_BOOL(Self && Self->tex ? Self->tex->isOneBitAlpha : true);
}


// void blitExt (int dx0, int dy0, int dx1, int dy1, int x0, int y0, optional int x1, optional int y1, optional float angle);
IMPLEMENT_FUNCTION(VGLTexture, blitExt) {
  P_GET_FLOAT_OPT(angle, 0);
  P_GET_INT(specifiedY1);
  P_GET_INT(y1);
  P_GET_INT(specifiedX1);
  P_GET_INT(x1);
  P_GET_INT(y0);
  P_GET_INT(x0);
  P_GET_INT(dy1);
  P_GET_INT(dx1);
  P_GET_INT(dy0);
  P_GET_INT(dx0);
  P_GET_SELF;
  if (!specifiedX1) x1 = -1;
  if (!specifiedY1) y1 = -1;
  if (Self && Self->tex) Self->tex->blitExt(dx0, dy0, dx1, dy1, x0, y0, x1, y1, angle);
}


// void blitExtRep (int dx0, int dy0, int dx1, int dy1, int x0, int y0, optional int x1, optional int y1);
IMPLEMENT_FUNCTION(VGLTexture, blitExtRep) {
  P_GET_INT(specifiedY1);
  P_GET_INT(y1);
  P_GET_INT(specifiedX1);
  P_GET_INT(x1);
  P_GET_INT(y0);
  P_GET_INT(x0);
  P_GET_INT(dy1);
  P_GET_INT(dx1);
  P_GET_INT(dy0);
  P_GET_INT(dx0);
  P_GET_SELF;
  if (!specifiedX1) x1 = -1;
  if (!specifiedY1) y1 = -1;
  if (Self && Self->tex) Self->tex->blitExtRep(dx0, dy0, dx1, dy1, x0, y0, x1, y1);
}


// void blitAt (int dx0, int dy0, optional float scale, optional float angle);
IMPLEMENT_FUNCTION(VGLTexture, blitAt) {
  P_GET_FLOAT_OPT(angle, 0);
  P_GET_INT(specifiedScale);
  P_GET_FLOAT(scale);
  P_GET_INT(dy0);
  P_GET_INT(dx0);
  P_GET_SELF;
  if (!specifiedScale) scale = 1;
  if (Self && Self->tex) Self->tex->blitAt(dx0, dy0, scale, angle);
}


// native final static GLTexture CreateEmpty (int wdt, int hgt, optional string txname);
IMPLEMENT_FUNCTION(VGLTexture, CreateEmpty) {
  P_GET_STR_OPT(txname, VStr::EmptyString);
  P_GET_INT(hgt);
  P_GET_INT(wdt);
  if (wdt < 1 || hgt < 1 || wdt > 32768 || hgt > 32768) { RET_REF(nullptr); return; }
  VOpenGLTexture *tex = VOpenGLTexture::CreateEmpty(txname, wdt, hgt);
  if (tex) {
    VGLTexture *ifile = Spawn<VGLTexture>();
    ifile->tex = tex;
    ifile->id = vcGLAllocId(ifile);
    //fprintf(stderr, "created texture object %p (%p)\n", ifile, ifile->tex);
    RET_REF((VObject *)ifile);
    return;
  }
  RET_REF(nullptr);
}

// native final static void setPixel (int x, int y, int argb); // aarrggbb; a==0 is completely opaque
IMPLEMENT_FUNCTION(VGLTexture, setPixel) {
  P_GET_INT(argb);
  P_GET_INT(y);
  P_GET_INT(x);
  P_GET_SELF;
  if (Self && Self->tex && Self->tex->img) {
    vuint8 a = 255-((argb>>24)&0xff);
    vuint8 r = (argb>>16)&0xff;
    vuint8 g = (argb>>8)&0xff;
    vuint8 b = argb&0xff;
    Self->tex->img->setPixel(x, y, VImage::RGBA(r, g, b, a));
  }
}

// native final static int getPixel (int x, int y); // aarrggbb; a==0 is completely opaque
IMPLEMENT_FUNCTION(VGLTexture, getPixel) {
  P_GET_INT(y);
  P_GET_INT(x);
  P_GET_SELF;
  if (Self && Self->tex && Self->tex->img) {
    auto c = Self->tex->img->getPixel(x, y);
    vuint32 argb = (((vuint32)c.r)<<16)|(((vuint32)c.g)<<8)|((vuint32)c.b)|(((vuint32)(255-c.a))<<24);
    RET_INT((vint32)argb);
  } else {
    RET_INT(0xff000000); // completely transparent
  }
}

// native final static void upload ();
IMPLEMENT_FUNCTION(VGLTexture, upload) {
  P_GET_SELF;
  if (Self && Self->tex) Self->tex->update();
}

// native final void smoothEdges (); // call after manual texture building
IMPLEMENT_FUNCTION(VGLTexture, smoothEdges) {
  P_GET_SELF;
  if (Self && Self->tex && Self->tex->img) {
    Self->tex->img->smoothEdges();
  }
}


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_CLASS(V, GLVideo);

bool VGLVideo::smoothLine = false;
bool VGLVideo::texFiltering = false;


// ////////////////////////////////////////////////////////////////////////// //
void VGLVideo::clearColored (int rgb) {
  if (!NewUIICB::inited) return;

  glClearColor(((rgb>>16)&0xff)/255.0f, ((rgb>>8)&0xff)/255.0f, (rgb&0xff)/255.0f, 0.0);
  //glClearDepth(1.0);
  //glClearStencil(0);

  //glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
  //glClear(GL_COLOR_BUFFER_BIT|(depthTest ? GL_DEPTH_BUFFER_BIT : 0));
  glClear(GL_COLOR_BUFFER_BIT);
}


// ////////////////////////////////////////////////////////////////////////// //
vuint32 VGLVideo::colorARGB = 0xffffff;
int VGLVideo::mBlendMode = VGLVideo::BlendNormal;
NUIFont *VGLVideo::currFont = nullptr;


void VGLVideo::setFont (VName fontname) {
  if (currFont && currFont->getName() == fontname) return;
  currFont = NUIFont::findFont(fontname);
}


void VGLVideo::drawTextAt (int x, int y, const VStr &text) {
  if (!currFont /*|| isFullyTransparent()*/ || text.isEmpty()) return;
  if (!NewUIICB::inited) return;

  const VOpenGLTexture *tex = currFont->getTexture();
  if (currFont->singleTexture) {
    if (!tex || !tex->tid) return; // oops
  }

  glEnable(GL_TEXTURE_2D);
  //glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  if (currFont->singleTexture) glBindTexture(GL_TEXTURE_2D, tex->tid);
  glEnable(GL_BLEND); // font is rarely fully opaque, so don't bother checking
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  const float z = 0;
  if (currFont->singleTexture) glBegin(GL_QUADS);
  int sx = x;
  for (int f = 0; f < text.length(); ++f) {
    int ch = (vuint8)text[f];
    //if (ch == '\r') { x = sx; continue; }
    if (ch == '\n') { x = sx; y += currFont->getHeight(); continue; }
    auto fc = currFont->getChar(ch);
    if (!fc) {
      //fprintf(stderr, "NO CHAR #%d\n", ch);
      continue;
    }
    // draw char
    if (!currFont->singleTexture) {
      glBindTexture(GL_TEXTURE_2D, fc->tex->tid);
      glBegin(GL_QUADS);
      //fprintf(stderr, "rebound texture (tid=%u) for char %d\n", fc->tex->tid, ch);
    }
    int xofs = fc->leftbear;
    glTexCoord2f(fc->tx0, fc->ty0); glVertex3f(x+xofs, y+fc->topofs, z);
    glTexCoord2f(fc->tx1, fc->ty0); glVertex3f(x+xofs+fc->width, y+fc->topofs, z);
    glTexCoord2f(fc->tx1, fc->ty1); glVertex3f(x+xofs+fc->width, y+fc->topofs+fc->height, z);
    glTexCoord2f(fc->tx0, fc->ty1); glVertex3f(x+xofs, y+fc->topofs+fc->height, z);
    if (!currFont->singleTexture) glEnd();
    // advance
    //x += fc->advance;
    x += fc->leftbear+fc->rightbear;
  }
  if (currFont->singleTexture) glEnd();
}


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_FUNCTION(VGLVideo, isInitialized) { RET_BOOL(NewUIICB::inited); }
IMPLEMENT_FUNCTION(VGLVideo, screenWidth) { RET_INT(ScreenWidth); }
IMPLEMENT_FUNCTION(VGLVideo, screenHeight) { RET_INT(ScreenHeight); }

//IMPLEMENT_FUNCTION(VGLVideo, isMouseCursorVisible) { RET_BOOL(NewUIICB::inited ? SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE : true); }
//IMPLEMENT_FUNCTION(VGLVideo, hideMouseCursor) { if (NewUIICB::inited) SDL_ShowCursor(SDL_DISABLE); }
//IMPLEMENT_FUNCTION(VGLVideo, showMouseCursor) { if (NewUIICB::inited) SDL_ShowCursor(SDL_ENABLE); }

IMPLEMENT_FUNCTION(VGLVideo, clearScreen) {
  P_GET_INT_OPT(rgb, 0);
  VGLVideo::clearColored(rgb);
}


/*
IMPLEMENT_FUNCTION(VGLVideo, get_glHasNPOT) {
  RET_BOOL(NewUIICB::inited ? hasNPOT : false);
}
*/


IMPLEMENT_FUNCTION(VGLVideo, get_scissorEnabled) {
  if (NewUIICB::inited) {
    RET_BOOL((glIsEnabled(GL_SCISSOR_TEST) ? 1 : 0));
  } else {
    RET_BOOL(0);
  }
}

IMPLEMENT_FUNCTION(VGLVideo, set_scissorEnabled) {
  P_GET_BOOL(v);
  if (NewUIICB::inited) {
    if (v) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
  }
}

IMPLEMENT_FUNCTION(VGLVideo, copyScissor) {
  P_GET_PTR(ScissorRect, s);
  P_GET_PTR(ScissorRect, d);
  if (d) {
    if (s) {
      *d = *s;
    } else {
      d->x = d->y = 0;
      d->w = ScreenWidth;
      d->h = ScreenHeight;
      d->enabled = 0;
    }
  }
}

IMPLEMENT_FUNCTION(VGLVideo, getScissor) {
  P_GET_PTR(ScissorRect, sr);
  if (sr) {
    if (!NewUIICB::inited) { sr->x = sr->y = sr->w = sr->h = sr->enabled = 0; return; }
    sr->enabled = (glIsEnabled(GL_SCISSOR_TEST) ? 1 : 0);
    GLint scxywh[4];
    glGetIntegerv(GL_SCISSOR_BOX, scxywh);
    int y0 = ScreenHeight-(scxywh[1]+scxywh[3]);
    int y1 = ScreenHeight-scxywh[1]-1;
    sr->x = scxywh[0];
    sr->y = y0;
    sr->w = scxywh[2];
    sr->h = y1-y0+1;
  }
}

IMPLEMENT_FUNCTION(VGLVideo, setScissor) {
  P_GET_PTR_OPT(ScissorRect, sr, nullptr);
  if (sr) {
    if (!NewUIICB::inited) return;
    if (sr->enabled) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    //glScissor(sr->x0, mHeight-sr->y0-1, sr->x1, mHeight-sr->y1-1);
    int w = (sr->w > 0 ? sr->w : 0);
    int h = (sr->h > 0 ? sr->h : 0);
    int y1 = ScreenHeight-sr->y-1;
    int y0 = ScreenHeight-(sr->y+h);
    glScissor(sr->x, y0, w, y1-y0+1);
  } else {
    if (NewUIICB::inited) {
      glDisable(GL_SCISSOR_TEST);
      GLint scxywh[4];
      glGetIntegerv(GL_VIEWPORT, scxywh);
      glScissor(scxywh[0], scxywh[1], scxywh[2], scxywh[3]);
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// static native final void glPushMatrix ();
IMPLEMENT_FUNCTION(VGLVideo, glPushMatrix) { if (NewUIICB::inited) glPushMatrix(); }

// static native final void glPopMatrix ();
IMPLEMENT_FUNCTION(VGLVideo, glPopMatrix) { if (NewUIICB::inited) glPopMatrix(); }

// static native final void glLoadIdentity ();
IMPLEMENT_FUNCTION(VGLVideo, glLoadIdentity) { if (NewUIICB::inited) glLoadIdentity(); }

// static native final void glScale (float sx, float sy, optional float sz);
IMPLEMENT_FUNCTION(VGLVideo, glScale) {
  P_GET_FLOAT_OPT(z, 1);
  P_GET_FLOAT(y);
  P_GET_FLOAT(x);
  if (NewUIICB::inited) glScalef(x, y, z);
}

// static native final void glTranslate (float dx, float dy, optional float dz);
IMPLEMENT_FUNCTION(VGLVideo, glTranslate) {
  P_GET_FLOAT_OPT(z, 0);
  P_GET_FLOAT(y);
  P_GET_FLOAT(x);
  if (NewUIICB::inited) glTranslatef(x, y, z);
}

// static native final void glRotate (float ax, float ay, optional float az);
/*
IMPLEMENT_FUNCTION(VGLVideo, glRotate) {
  P_GET_FLOAT(x);
  P_GET_FLOAT(y);
  P_GET_FLOAT_OPT(z, 1);
  glRotatef(x, y, z);
}
*/


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_FUNCTION(VGLVideo, get_smoothLine) {
  RET_BOOL(smoothLine);
}

IMPLEMENT_FUNCTION(VGLVideo, set_smoothLine) {
  P_GET_BOOL(v);
  if (smoothLine != v) {
    smoothLine = v;
    if (NewUIICB::inited) {
      if (v) glEnable(GL_LINE_SMOOTH); else glDisable(GL_LINE_SMOOTH);
    }
  }
}


//native final static int getColorARGB ();
IMPLEMENT_FUNCTION(VGLVideo, get_colorARGB) {
  RET_INT(colorARGB);
}

//native final static void setColorARGB (int v);
IMPLEMENT_FUNCTION(VGLVideo, set_colorARGB) {
  P_GET_INT(c);
  setColor((vuint32)c);
}

//native final static int getBlendMode ();
IMPLEMENT_FUNCTION(VGLVideo, get_blendMode) {
  RET_INT(getBlendMode());
}

//native final static void set_blendMode (int v);
IMPLEMENT_FUNCTION(VGLVideo, set_blendMode) {
  P_GET_INT(c);
  setBlendMode(c);
}

//native final static bool get_textureFiltering ();
IMPLEMENT_FUNCTION(VGLVideo, get_textureFiltering) {
  RET_BOOL(getTexFiltering());
}

//native final static void set_textureFiltering (bool v);
IMPLEMENT_FUNCTION(VGLVideo, set_textureFiltering) {
  P_GET_BOOL(tf);
  setTexFiltering(tf);
}


// ////////////////////////////////////////////////////////////////////////// //
// aborts if font cannot be loaded
//native final static void loadFontDF (name fname, string fnameIni, string fnameTexture);
IMPLEMENT_FUNCTION(VGLVideo, loadFontDF) {
  /*
  P_GET_STR(fnameTexture);
  P_GET_STR(fnameIni);
  P_GET_NAME(fname);
  */
  VName fname;
  VStr fnameIni;
  VStr fnameTexture;
  vobjGetParam(fname, fnameIni, fnameTexture);
  //fprintf(stderr, "fname=<%s>; ini=<%s>; tx=<%s>\n", *fname, *fnameIni, *fnameTexture);
  if (NUIFont::findFont(fname)) return;
  NUIFont::LoadDF(fname, fnameIni, fnameTexture);
}


// aborts if font cannot be loaded
//native final static void loadFontPCF (name fname, string filename);
IMPLEMENT_FUNCTION(VGLVideo, loadFontPCF) {
  P_GET_STR(filename);
  P_GET_NAME(fontname);
  if (NUIFont::findFont(fontname)) return;
  NUIFont::LoadPCF(fontname, filename);
}


struct FontCharInfo {
  int ch;
  //int width, height; // height may differ from font height
  //int advance; // horizontal advance to print next char
  //int topofs; // offset from font top (i.e. y+topofs should be used to draw char)
  int leftbear, rightbear; // positive means "more to the respective bbox side"
  int ascent, descent; // both are positive, which actually means "offset from baseline to the respective direction"
};


// native final static bool getCharInfo (int ch, out FontCharInfo ci); // returns `false` if char wasn't found
IMPLEMENT_FUNCTION(VGLVideo, getCharInfo) {
  P_GET_PTR(FontCharInfo, fc);
  P_GET_INT(ch);
  if (!fc) { RET_BOOL(false); return; }
  if (!currFont) {
    memset(fc, 0, sizeof(*fc));
    RET_BOOL(false);
    return;
  }
  auto fci = currFont->getChar(ch);
  if (!fci || fci->ch == 0) {
    memset(fc, 0, sizeof(*fc));
    RET_BOOL(false);
    return;
  }
  fc->ch = fci->ch;
  //fc->width = fci->width;
  //fc->height = fci->height;
  //fc->advance = fci->advance;
  //fc->topofs = fci->topofs;
  fc->leftbear = fci->leftbear;
  fc->rightbear = fci->rightbear;
  fc->ascent = fci->ascent;
  fc->descent = fci->descent;
  RET_BOOL(true);
}


//native final static void setFont (name fontname);
IMPLEMENT_FUNCTION(VGLVideo, set_fontName) {
  P_GET_NAME(fontname);
  setFont(fontname);
}

//native final static name getFont ();
IMPLEMENT_FUNCTION(VGLVideo, get_fontName) {
  if (!currFont) {
    RET_NAME(NAME_None);
  } else {
    RET_NAME(currFont->getName());
  }
}

//native final static void fontHeight ();
IMPLEMENT_FUNCTION(VGLVideo, fontHeight) {
  RET_INT(currFont ? currFont->getHeight() : 0);
}

//native final static int spaceWidth ();
IMPLEMENT_FUNCTION(VGLVideo, spaceWidth) {
  RET_INT(currFont ? currFont->getSpaceWidth() : 0);
}

//native final static int charWidth (int ch);
IMPLEMENT_FUNCTION(VGLVideo, charWidth) {
  P_GET_INT(ch);
  RET_INT(currFont ? currFont->charWidth(ch) : 0);
}

//native final static int textWidth (string text);
IMPLEMENT_FUNCTION(VGLVideo, textWidth) {
  P_GET_STR(text);
  RET_INT(currFont ? currFont->textWidth(text) : 0);
}

//native final static int textHeight (string text);
IMPLEMENT_FUNCTION(VGLVideo, textHeight) {
  P_GET_STR(text);
  RET_INT(currFont ? currFont->textHeight(text) : 0);
}

//native final static void drawText (int x, int y, string text);
IMPLEMENT_FUNCTION(VGLVideo, drawTextAt) {
  P_GET_STR(text);
  P_GET_INT(y);
  P_GET_INT(x);
  drawTextAt(x, y, text);
}


//native final static void drawLine (int x0, int y0, int x1, int y1);
IMPLEMENT_FUNCTION(VGLVideo, drawLine) {
  P_GET_INT(y1);
  P_GET_INT(x1);
  P_GET_INT(y0);
  P_GET_INT(x0);
  if (!NewUIICB::inited /*|| isFullyTransparent()*/) return;
  setupBlending();
  glDisable(GL_TEXTURE_2D);
  const float z = 0;
  glBegin(GL_LINES);
    glVertex3f(x0+0.5f, y0+0.5f, z);
    glVertex3f(x1+0.5f, y1+0.5f, z);
  glEnd();
}


//native final static void drawRect (int x0, int y0, int w, int h);
IMPLEMENT_FUNCTION(VGLVideo, drawRect) {
  P_GET_INT(h);
  P_GET_INT(w);
  P_GET_INT(y0);
  P_GET_INT(x0);
  if (!NewUIICB::inited /*|| isFullyTransparent()*/ || w < 1 || h < 1) return;
  setupBlending();
  glDisable(GL_TEXTURE_2D);
  const float z = 0;
  glBegin(GL_LINE_LOOP);
    glVertex3f(x0+0+0.5f, y0+0+0.5f, z);
    glVertex3f(x0+w-0.5f, y0+0+0.5f, z);
    glVertex3f(x0+w-0.5f, y0+h-0.5f, z);
    glVertex3f(x0+0+0.5f, y0+h-0.5f, z);
  glEnd();
}


//native final static void fillRect (int x0, int y0, int w, int h);
IMPLEMENT_FUNCTION(VGLVideo, fillRect) {
  P_GET_INT(h);
  P_GET_INT(w);
  P_GET_INT(y0);
  P_GET_INT(x0);
  if (!NewUIICB::inited /*|| isFullyTransparent()*/ || w < 1 || h < 1) return;
  setupBlending();
  glDisable(GL_TEXTURE_2D);

  // no need for 0.5f here, or rect will be offset
  const float z = 0;
  glBegin(GL_QUADS);
    glVertex3f(x0+0, y0+0, z);
    glVertex3f(x0+w, y0+0, z);
    glVertex3f(x0+w, y0+h, z);
    glVertex3f(x0+0, y0+h, z);
  glEnd();
}

// native final static bool getMousePos (out int x, out int y)
IMPLEMENT_FUNCTION(VGLVideo, getMousePos) {
  P_GET_PTR(int, yp);
  P_GET_PTR(int, xp);
  if (NewUIICB::inited && Drawer) {
    Drawer->GetMousePosition(xp, yp);
    RET_BOOL(true);
  } else {
    if (xp) *xp = 0;
    if (yp) *yp = 0;
    RET_BOOL(false);
  }
}


// ////////////////////////////////////////////////////////////////////////// //
static VStr readLine (VStream *strm, bool allTrim=true) {
  if (!strm || strm->IsError()) return VStr();
  VStr res;
  while (!strm->AtEnd()) {
    char ch;
    strm->Serialize(&ch, 1);
    if (strm->IsError()) return VStr();
    if (ch == '\r') {
      if (!strm->AtEnd()) {
        strm->Serialize(&ch, 1);
        if (strm->IsError()) return VStr();
        if (ch != '\n') strm->Seek(strm->Tell()-1);
      }
      break;
    }
    if (ch == '\n') break;
    if (ch == 0) ch = ' ';
    res += ch;
  }
  if (allTrim) {
    while (!res.isEmpty() && (vuint8)res[0] <= ' ') res.chopLeft(1);
    while (!res.isEmpty() && (vuint8)res[res.length()-1] <= ' ') res.chopRight(1);
  }
  return res;
}


static VStr getKey (const VStr &s) {
  int epos = s.indexOf('=');
  if (epos < 0) return s;
  VStr res = s.left(epos);
  while (!res.isEmpty() && (vuint8)res[res.length()-1] <= ' ') res.chopRight(1);
  return res;
}


static VStr getValue (const VStr &s) {
  int epos = s.indexOf('=');
  if (epos < 0) return VStr();
  VStr res = s;
  res.chopLeft(epos+1);
  while (!res.isEmpty() && (vuint8)res[0] <= ' ') res.chopLeft(1);
  while (!res.isEmpty() && (vuint8)res[res.length()-1] <= ' ') res.chopRight(1);
  return res;
}


static int getIntValue (const VStr &s) {
  VStr v = getValue(s);
  if (v.isEmpty()) return 0;
  bool neg = v.startsWith("-");
  if (neg) v.chopLeft(1);
  int res = 0;
  for (int f = 0; f < v.length(); ++f) {
    int d = VStr::digitInBase(v[f]);
    if (d < 0) break;
    res = res*10+d;
  }
  if (neg) res = -res;
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
NUIFont *NUIFont::fontList;


//==========================================================================
//
//  NUIFont::findFont
//
//==========================================================================
NUIFont *NUIFont::findFont (VName name) {
  for (NUIFont *cur = fontList; cur; cur = cur->next) if (cur->name == name) return cur;
  return nullptr;
}


//==========================================================================
//
//  NUIFont::NUIFont
//
//==========================================================================
NUIFont::NUIFont ()
  : name(NAME_None)
  , next(nullptr)
  , tex(nullptr)
  , singleTexture(true)
  , chars()
  , spaceWidth(0)
  , fontHeight(0)
  , minWidth(0)
  , maxWidth(0)
  , avgWidth(0)
{
}


//==========================================================================
//
//  NUIFont::~NUIFont
//
//==========================================================================
NUIFont::~NUIFont() {
  if (singleTexture) {
    if (tex) tex->release();
  } else {
    for (int f = 0; f < chars.length(); ++f) {
      FontChar &fc = chars[f];
      if (fc.tex) fc.tex->release();
    }
  }
  NUIFont *prev = nullptr, *cur = fontList;
  while (cur && cur != this) { prev = cur; cur = cur->next; }
  if (cur) {
    if (prev) prev->next = next; else fontList = next;
  }
}


//==========================================================================
//
//  NUIFont::LoadDF
//
//==========================================================================
NUIFont *NUIFont::LoadDF (VName aname, const VStr &fnameIni, const VStr &fnameTexture) {
  VOpenGLTexture *tex = VOpenGLTexture::Load(fnameTexture);
  if (!tex) Sys_Error(va("cannot load font '%s' (texture not found)", *aname));

  auto inif = FL_OpenFileRead(fnameIni);
  if (!inif) { tex->release(); tex = nullptr; Sys_Error(va("cannot load font '%s' (description not found)", *aname)); }
  //fprintf(stderr, "*** %d %d %d %d\n", (int)inif->AtEnd(), (int)inif->IsError(), inif->TotalSize(), inif->Tell());

  VStr currSection;

  int cwdt = -1, chgt = -1, kern = 0;
  int xwidth[256];
  memset(xwidth, 0, sizeof(xwidth));

  // parse ini file
  while (!inif->AtEnd()) {
    VStr line = readLine(inif);
    if (inif->IsError()) { delete inif; tex->release(); tex = nullptr; Sys_Error(va("cannot load font '%s' (error loading description)", *aname)); }
    if (line.isEmpty() || line[0] == ';' || line.startsWith("//")) continue;
    if (line[0] == '[') { currSection = line; continue; }
    // fontmap?
    auto key = getKey(line);
    //fprintf(stderr, "line:<%s>; key:<%s>; intval=%d\n", *line, *key, getIntValue(line));
    if (currSection.equ1251CI("[FontMap]")) {
      if (key.equ1251CI("CharWidth")) { cwdt = getIntValue(line); continue; }
      if (key.equ1251CI("CharHeight")) { chgt = getIntValue(line); continue; }
      if (key.equ1251CI("Kerning")) { kern = getIntValue(line); continue; }
      continue;
    }
    if (currSection.length() < 2 || VStr::digitInBase(currSection[1]) < 0 || !currSection.endsWith("]")) continue;
    if (!key.equ1251CI("Width")) continue;
    int cidx = 0;
    for (int f = 1; f < currSection.length(); ++f) {
      int d = VStr::digitInBase(currSection[f]);
      if (d < 0) {
        if (f != currSection.length()-1) cidx = -1;
        break;
      }
      cidx = cidx*10+d;
    }
    if (cidx >= 0 && cidx < 256) {
      int w = getIntValue(line);
      //fprintf(stderr, "cidx=%d; w=%d\n", cidx, w);
      if (w < 0) w = 0;
      xwidth[cidx] = w;
    }
  }

  delete inif;

  if (cwdt < 1 || chgt < 1) { tex->release(); tex = nullptr; Sys_Error(va("cannot load font '%s' (invalid description 00)", *aname)); }
  int xchars = tex->width/cwdt;
  int ychars = tex->height/chgt;
  if (xchars < 1 || ychars < 1 || xchars*ychars < 128) { tex->release(); tex = nullptr; Sys_Error(va("cannot load font '%s' (invalid description 01)", *aname)); }

  NUIFont *fnt = new NUIFont();
  fnt->name = aname;
  fnt->singleTexture = true;
  fnt->tex = tex;

  fnt->chars.setLength(xchars*ychars);

  fnt->fontHeight = chgt;
  int totalWdt = 0, maxWdt = -0x7fffffff, minWdt = 0x7fffffff, totalCount = 0;

  for (int cx = 0; cx < xchars; ++cx) {
    for (int cy = 0; cy < ychars; ++cy) {
      FontChar &fc = fnt->chars[cy*xchars+cx];
      fc.ch = cy*xchars+cx;
      fc.width = cwdt;
      fc.height = chgt;
      //fc.advance = xwidth[fc.ch]+kern;
      fc.leftbear = 0;
      fc.rightbear = xwidth[fc.ch]+kern;
      if (minWdt > cwdt) minWdt = cwdt;
      if (maxWdt < cwdt) maxWdt = cwdt;
      totalWdt += cwdt;
      ++totalCount;
      if (fc.ch == 32) fnt->spaceWidth = fc.leftbear+fc.rightbear;
      fc.topofs = 0;
      fc.tx0 = (float)(cx*cwdt)/(float)tex->getWidth();
      fc.ty0 = (float)(cy*chgt)/(float)tex->getHeight();
      fc.tx1 = (float)(cx*cwdt+cwdt)/(float)tex->getWidth();
      fc.ty1 = (float)(cy*chgt+chgt)/(float)tex->getHeight();
      fc.tex = tex;
    }
  }

  fnt->minWidth = minWdt;
  fnt->maxWidth = maxWdt;
  fnt->avgWidth = (totalCount ? totalWdt/totalCount : 0);

  fnt->next = fontList;
  fontList = fnt;

  return fnt;
}


//==========================================================================
//
//  NUIFont::LoadPCF
//
//==========================================================================
NUIFont *NUIFont::LoadPCF (VName aname, const VStr &filename) {
/*
  static const vuint16 cp12512Uni[128] = {
    0x0402,0x0403,0x201A,0x0453,0x201E,0x2026,0x2020,0x2021,0x20AC,0x2030,0x0409,0x2039,0x040A,0x040C,0x040B,0x040F,
    0x0452,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,0x003F,0x2122,0x0459,0x203A,0x045A,0x045C,0x045B,0x045F,
    0x00A0,0x040E,0x045E,0x0408,0x00A4,0x0490,0x00A6,0x00A7,0x0401,0x00A9,0x0404,0x00AB,0x00AC,0x00AD,0x00AE,0x0407,
    0x00B0,0x00B1,0x0406,0x0456,0x0491,0x00B5,0x00B6,0x00B7,0x0451,0x2116,0x0454,0x00BB,0x0458,0x0405,0x0455,0x0457,
    0x0410,0x0411,0x0412,0x0413,0x0414,0x0415,0x0416,0x0417,0x0418,0x0419,0x041A,0x041B,0x041C,0x041D,0x041E,0x041F,
    0x0420,0x0421,0x0422,0x0423,0x0424,0x0425,0x0426,0x0427,0x0428,0x0429,0x042A,0x042B,0x042C,0x042D,0x042E,0x042F,
    0x0430,0x0431,0x0432,0x0433,0x0434,0x0435,0x0436,0x0437,0x0438,0x0439,0x043A,0x043B,0x043C,0x043D,0x043E,0x043F,
    0x0440,0x0441,0x0442,0x0443,0x0444,0x0445,0x0446,0x0447,0x0448,0x0449,0x044A,0x044B,0x044C,0x044D,0x044E,0x044F,
  };
*/

  auto fl = FL_OpenFileRead(filename);
  if (!fl) { Sys_Error(va("cannot load font '%s'", *aname)); }

  PcfFont pcf;
  if (!pcf.load(*fl)) { Sys_Error(va("invalid PCF font '%s'", *aname)); }

  NUIFont *fnt = new NUIFont();
  fnt->name = aname;
  fnt->singleTexture = false;
  fnt->tex = nullptr;

  fnt->chars.setLength(256);
  for (int f = 0; f < fnt->chars.length(); ++f) fnt->chars[f].ch = -1;

  fnt->fontHeight = pcf.fntAscent+pcf.fntDescent;
  fnt->spaceWidth = -1;

  int totalWdt = 0, maxWdt = -0x7fffffff, minWdt = 0x7fffffff, totalCount = 0;

  for (int gidx = 0; gidx < pcf.glyphs.length(); ++gidx) {
    PcfFont::Glyph &gl = pcf.glyphs[gidx];
    int chidx = -1;
    if (gl.codepoint < 128) {
      chidx = (int)gl.codepoint;
    } else {
      //for (unsigned f = 0; f < 128; ++f) if (cp12512Uni[f] == gl.codepoint) { chidx = (int)f+128; break; }
      if (gl.codepoint > 255) continue;
      chidx = (int)gl.codepoint;
    }
    if (chidx < 0) continue; // unused glyph
    //fprintf(stderr, "gidx=%d; codepoint=%u; chidx=%d\n", gidx, gl.codepoint, chidx);
    FontChar &fc = fnt->chars[chidx];
    if (fc.ch >= 0) continue; // duplicate glyph

    fc.tex = pcf.createGlyphTexture(gidx);
    if (!fc.tex) continue;

    fc.ch = chidx;
    fc.tx0 = fc.ty0 = 0;
    fc.tx1 = fc.ty1 = 1;

    fc.width = fc.tex->img->width;
    fc.height = fc.tex->img->height;
    //fc.advance = gl.metrics.leftSidedBearing+gl.metrics.width+gl.metrics.rightSideBearing;
    fc.topofs = pcf.fntAscent-gl.metrics.ascent;
    fc.leftbear = gl.metrics.leftSidedBearing;
    fc.rightbear = gl.metrics.rightSideBearing;
    fc.ascent = gl.metrics.ascent;
    fc.descent = gl.metrics.descent;

    if (fc.ch == 32) fnt->spaceWidth = fc.leftbear+fc.rightbear;

    if (minWdt > gl.metrics.width) minWdt = gl.metrics.width;
    if (maxWdt < gl.metrics.width) maxWdt = gl.metrics.width;
    totalWdt += gl.metrics.width;
    ++totalCount;

    /*
    VGLTexture *ifile = Spawn<VGLTexture>();
    ifile->tex = fc.tex;
    ifile->id = vcGLAllocId(ifile);
    */

    fc.tex->update();
  }

  fnt->minWidth = minWdt;
  fnt->maxWidth = maxWdt;
  fnt->avgWidth = (totalCount ? totalWdt/totalCount : 0);

  if (fnt->spaceWidth < 0) fnt->spaceWidth = fnt->avgWidth;

  fnt->next = fontList;
  fontList = fnt;

  return fnt;
}


//==========================================================================
//
//  NUIFont::GetChar
//
//==========================================================================
const NUIFont::FontChar *NUIFont::getChar (int ch) const {
  //fprintf(stderr, "GET CHAR #%d (%d); internal:%d\n", ch, chars.length(), chars[ch].ch);
  if (ch < 0) return nullptr;
  if (ch < 0 || ch >= chars.length()) {
    ch = VStr::upcase1251(ch);
    if (ch < 0 || ch >= chars.length()) return nullptr;
  }
  return (chars[ch].ch >= 0 ? &chars[ch] : nullptr);
}


//==========================================================================
//
//  NUIFont::charWidth
//
//==========================================================================
int NUIFont::charWidth (int ch) const {
  auto fc = getChar(ch);
  return (fc ? fc->width : 0);
}


//==========================================================================
//
//  NUIFont::textWidth
//
//==========================================================================
int NUIFont::textWidth (const VStr &s) const {
  int res = 0, curwdt = 0;
  for (int f = 0; f < s.length(); ++f) {
    vuint8 ch = vuint8(s[f]);
    if (ch == '\n') {
      if (res < curwdt) res = curwdt;
      curwdt = 0;
      continue;
    }
    auto fc = getChar(ch);
    if (fc) curwdt += fc->leftbear+fc->rightbear;
  }
  if (res < curwdt) res = curwdt;
  return res;
}


//==========================================================================
//
//  NUIFont::textHeight
//
//==========================================================================
int NUIFont::textHeight (const VStr &s) const {
  int res = fontHeight;
  for (int f = 0; f < s.length(); ++f) if (s[f] == '\n') res += fontHeight;
  return res;
}


#endif
