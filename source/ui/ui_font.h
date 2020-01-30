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

struct VSplitLine {
  VStr Text;
  vint32 Width;
};


// base class for fonts
class VFont {
protected:
  struct FFontChar {
    int Char;
    int TexNum;
    VTexture *BaseTex;
    VTexture **Textures;
  };

  VName Name;
  VFont *Next;

  // font characters
  TArray<FFontChar> Chars;
  // fast look-up for ASCII characters
  int AsciiChars[128];
  // range of available characters
  int FirstChar;
  int LastChar;

  // width of the space character
  int SpaceWidth;
  // height of the font
  int FontHeight;
  // additional distance betweeen characters
  int Kerning;

  rgba_t *Translation;

  static VFont *Fonts;

  void BuildTranslations (const bool *ColorsUsed, rgba_t *Pal, bool ConsoleTrans, bool Rescale);
  int FindChar (int) const;

  static void ParseTextColors ();
  static void ParseFontDefs ();
  static void MarkUsedColors (VTexture *, bool *);

public:
  VFont ();
  VFont (VName, VStr, int, int, int);
  ~VFont ();

  inline VName GetFontName () const noexcept { return Name; }

  VTexture *GetChar (int, int*, int) const;
  int GetCharWidth (int) const;
  int StringWidth (VStr) const;
  int TextWidth (VStr) const;
  int TextHeight (VStr) const;
  int SplitText (VStr, TArray<VSplitLine>&, int, bool trimRight=true) const;
  VStr SplitTextWithNewlines (VStr Text, int MaxWidth, bool trimRight=true) const;

  inline int GetSpaceWidth () const { return SpaceWidth; }
  inline int GetHeight () const { return FontHeight; }
  inline int GetKerning () const { return Kerning; }

  static void StaticInit ();
  static void StaticShutdown ();
  static VFont *FindFont (VName);
  static VFont *GetFont (VName, VName);
  static int ParseColorEscape (const char *&, int, int, VStr *escstr=nullptr);
  static int FindTextColor (VName);
};
