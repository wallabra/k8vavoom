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

  void BuildTranslations (const bool *ColoursUsed, rgba_t *Pal, bool ConsoleTrans, bool Rescale);
  int FindChar (int) const;

  static void ParseTextColours ();
  static void ParseFontDefs ();
  static void MarkUsedColours (VTexture *, bool *);

public:
  VFont ();
  VFont (VName, const VStr&, int, int, int);
  ~VFont ();

  VTexture *GetChar (int, int*, int) const;
  int GetCharWidth (int) const;
  int StringWidth (const VStr&) const;
  int TextWidth (const VStr&) const;
  int TextHeight (const VStr&) const;
  int SplitText (const VStr&, TArray<VSplitLine>&, int, bool trimRight=true) const;
  VStr SplitTextWithNewlines (const VStr &Text, int MaxWidth, bool trimRight=true) const;

  inline int GetSpaceWidth () const { return SpaceWidth; }
  inline int GetHeight () const { return FontHeight; }
  inline int GetKerning () const { return Kerning; }

  static void StaticInit ();
  static void StaticShutdown ();
  static VFont *FindFont (VName);
  static VFont *GetFont (VName, VName);
  static int ParseColourEscape (const char *&, int, int);
  static int FindTextColour (VName);
};
