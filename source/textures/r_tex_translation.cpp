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
#include "gamedefs.h"
#include "../render/r_shared.h"


//==========================================================================
//
//  VTextureTranslation::VTextureTranslation
//
//==========================================================================
VTextureTranslation::VTextureTranslation ()
  : Crc(0)
  , nextInCache(-1)
  , TranslStart(0)
  , TranslEnd(0)
  , Color(0)
{
  Clear();
}


//==========================================================================
//
//  VTextureTranslation::Clear
//
//==========================================================================
void VTextureTranslation::Clear () {
  for (int i = 0; i < 256; ++i) {
    Table[i] = i;
    Palette[i] = r_palette[i];
  }
  Commands.Clear();
  CalcCrc();
}


//==========================================================================
//
//  VTextureTranslation::CalcCrc
//
//==========================================================================
void VTextureTranslation::CalcCrc () {
  /*
  auto Work = TCRC16();
  for (int i = 1; i < 256; ++i) {
    Work += Palette[i].r;
    Work += Palette[i].g;
    Work += Palette[i].b;
  }
  Crc = Work;
  */
  auto Work = TCRC32();
  for (int i = 1; i < 256; ++i) {
    Work += Palette[i].r;
    Work += Palette[i].g;
    Work += Palette[i].b;
  }
  Crc = Work;
}


//==========================================================================
//
//  VTextureTranslation::Serialise
//
//==========================================================================
void VTextureTranslation::Serialise (VStream &Strm) {
  //k8: is this right at all?
  //    this is used to translations added by ACS
  vuint8 xver = 0; // current version is 0
  Strm << xver;
  Strm.Serialise(Table, 256);
  Strm.Serialise(Palette, sizeof(Palette));
  vuint16 itWasCrc16 = 0;
  Strm << itWasCrc16
    << TranslStart
    << TranslEnd
    << Color;
  int CmdsSize = Commands.Num();
  Strm << STRM_INDEX(CmdsSize);
  if (Strm.IsLoading()) Commands.SetNum(CmdsSize);
  for (int i = 0; i < CmdsSize; ++i) {
    VTransCmd &C = Commands[i];
    Strm << C.Type << C.Start << C.End << C.R1 << C.G1 << C.B1 << C.R2 << C.G2 << C.B2;
  }
  //CalcCrc();
}


//==========================================================================
//
//  VTextureTranslation::BuildPlayerTrans
//
//==========================================================================
void VTextureTranslation::BuildPlayerTrans (int Start, int End, int Col) {
  int Count = End-Start+1;
  vuint8 r = (Col>>16)&255;
  vuint8 g = (Col>>8)&255;
  vuint8 b = Col&255;
  vuint8 h, s, v;
  M_RgbToHsv(r, g, b, h, s, v);
  for (int i = 0; i < Count; ++i) {
    int Idx = Start+i;
    vuint8 TmpH, TmpS, TmpV;
    M_RgbToHsv(Palette[Idx].r, Palette[Idx].g,Palette[Idx].b, TmpH, TmpS, TmpV);
    M_HsvToRgb(h, s, v*TmpV/255, Palette[Idx].r, Palette[Idx].g, Palette[Idx].b);
    Table[Idx] = R_LookupRGB(Palette[Idx].r, Palette[Idx].g, Palette[Idx].b);
  }
  //for (int f = 0; f < 256; ++f) Table[f] = R_LookupRGB(255, 0, 0);
  CalcCrc();
  TranslStart = Start;
  TranslEnd = End;
  Color = Col;
}


//==========================================================================
//
//  VTextureTranslation::BuildBloodTrans
//
//==========================================================================
void VTextureTranslation::BuildBloodTrans (int Col) {
  vuint8 r = (Col>>16)&255;
  vuint8 g = (Col>>8)&255;
  vuint8 b = Col&255;
  // don't remap color 0
  for (int i = 1; i < 256; ++i) {
    int Bright = max3(r_palette[i].r, r_palette[i].g, r_palette[i].b);
    Palette[i].r = r*Bright/255;
    Palette[i].g = g*Bright/255;
    Palette[i].b = b*Bright/255;
    Table[i] = R_LookupRGB(Palette[i].r, Palette[i].g, Palette[i].b);
    //Table[i] = R_LookupRGB(255, 0, 0);
  }
  CalcCrc();
  Color = Col;
}


//==========================================================================
//
//  VTextureTranslation::MapToRange
//
//==========================================================================
void VTextureTranslation::MapToRange (int AStart, int AEnd, int ASrcStart, int ASrcEnd) {
  int Start;
  int End;
  int SrcStart;
  int SrcEnd;
  AStart = clampval(AStart, 0, 255);
  AEnd = clampval(AEnd, 0, 255);
  ASrcStart = clampval(ASrcStart, 0, 255);
  ASrcEnd = clampval(ASrcEnd, 0, 255);
  // swap range if necesary
  if (AStart > AEnd) {
    Start = AEnd;
    End = AStart;
    SrcStart = ASrcEnd;
    SrcEnd = ASrcStart;
  } else {
    Start = AStart;
    End = AEnd;
    SrcStart = ASrcStart;
    SrcEnd = ASrcEnd;
  }
  // check for single color change
  if (Start == End) {
    Table[Start] = SrcStart;
    Palette[Start] = r_palette[SrcStart];
    return;
  }
  float CurCol = SrcStart;
  float ColStep = (float(SrcEnd)-float(SrcStart))/(float(End)-float(Start));
  for (int i = Start; i <= End; ++i, CurCol += ColStep) {
    Table[i] = clampToByte(int(CurCol));
    Palette[i] = r_palette[Table[i]];
  }
  //for (int f = 0; f < 256; ++f) Table[f] = R_LookupRGB(255, 0, 0);
  VTransCmd &C = Commands.Alloc();
  C.Type = 0;
  C.Start = Start;
  C.End = End;
  C.R1 = SrcStart;
  C.R2 = SrcEnd;
  CalcCrc();
}


//==========================================================================
//
//  VTextureTranslation::MapToColors
//
//==========================================================================
void VTextureTranslation::MapToColors (int AStart, int AEnd, int AR1, int AG1, int AB1, int AR2, int AG2, int AB2) {
  int Start;
  int End;
  int R1, G1, B1;
  int R2, G2, B2;
  AStart = clampval(AStart, 0, 255);
  AEnd = clampval(AEnd, 0, 255);
  AR1 = clampval(AR1, 0, 255);
  AG1 = clampval(AG1, 0, 255);
  AB1 = clampval(AB1, 0, 255);
  AR2 = clampval(AR2, 0, 255);
  AG2 = clampval(AG2, 0, 255);
  AB2 = clampval(AB2, 0, 255);
  // swap range if necesary
  if (AStart > AEnd) {
    Start = AEnd;
    End = AStart;
    R1 = AR2;
    G1 = AG2;
    B1 = AB2;
    R2 = AR1;
    G2 = AG1;
    B2 = AB1;
  } else {
    Start = AStart;
    End = AEnd;
    R1 = AR1;
    G1 = AG1;
    B1 = AB1;
    R2 = AR2;
    G2 = AG2;
    B2 = AB2;
  }
  // check for single color change
  if (Start == End) {
    Palette[Start].r = R1;
    Palette[Start].g = G1;
    Palette[Start].b = B1;
    Table[Start] = R_LookupRGB(R1, G1, B1);
    return;
  }
  float CurR = R1;
  float CurG = G1;
  float CurB = B1;
  float RStep = (float(R2)-float(R1))/(float(End)-float(Start));
  float GStep = (float(G2)-float(G1))/(float(End)-float(Start));
  float BStep = (float(B2)-float(B1))/(float(End)-float(Start));
  if (!isFiniteF(RStep)) RStep = 0;
  if (!isFiniteF(GStep)) GStep = 0;
  if (!isFiniteF(BStep)) BStep = 0;
  for (int i = Start; i <= End; ++i, CurR += RStep, CurG += GStep, CurB += BStep) {
    Palette[i].r = clampToByte(int(CurR));
    Palette[i].g = clampToByte(int(CurG));
    Palette[i].b = clampToByte(int(CurB));
    Table[i] = R_LookupRGB(Palette[i].r, Palette[i].g, Palette[i].b);
  }
  VTransCmd &C = Commands.Alloc();
  C.Type = 1;
  C.Start = Start;
  C.End = End;
  C.R1 = R1;
  C.G1 = G1;
  C.B1 = B1;
  C.R2 = R2;
  C.G2 = G2;
  C.B2 = B2;
  CalcCrc();
}


//==========================================================================
//
//  VTextureTranslation::MapDesaturated
//
//==========================================================================
void VTextureTranslation::MapDesaturated (int AStart, int AEnd, float rs, float gs, float bs, float re, float ge, float be) {
  AStart = clampval(AStart, 0, 255);
  AEnd = clampval(AEnd, 0, 255);
  rs = clampval(rs, 0.0f, 2.0f);
  gs = clampval(gs, 0.0f, 2.0f);
  bs = clampval(bs, 0.0f, 2.0f);
  re = clampval(re, 0.0f, 2.0f);
  ge = clampval(ge, 0.0f, 2.0f);
  be = clampval(be, 0.0f, 2.0f);
  // swap range if necesary
  if (AStart > AEnd) {
    int itmp = AStart;
    AStart = AEnd;
    AEnd = itmp;
  }
  //GCon->Logf(NAME_Debug, "DESAT: %d:%d [%g,%g,%g]-[%g,%g,%g]", AStart, AEnd, rs, gs, bs, re, ge, be);
  for (int i = AStart; i <= AEnd; ++i) {
    float gray = colorIntensity(r_palette[i].r, r_palette[i].g, r_palette[i].b)/255.0f;
    Palette[i].r = clampToByte((int)((rs+gray*(re-rs))*255.0f));
    Palette[i].g = clampToByte((int)((gs+gray*(ge-gs))*255.0f));
    Palette[i].b = clampToByte((int)((bs+gray*(be-bs))*255.0f));
    Table[i] = R_LookupRGB(Palette[i].r, Palette[i].g, Palette[i].b);
    //GCon->Logf(NAME_Debug, "  i=%d; gray=%g; (%d,%d,%d) -> (%d,%d,%d)", i, gray, r_palette[i].r, r_palette[i].g, r_palette[i].b, Palette[i].r, Palette[i].g, Palette[i].b);
  }
  VTransCmd &C = Commands.Alloc();
  C.Type = 2;
  C.Start = AStart;
  C.End = AEnd;
  C.R1 = clampToByte((int)(rs*128.0f));
  C.G1 = clampToByte((int)(gs*128.0f));
  C.B1 = clampToByte((int)(bs*128.0f));
  C.R2 = clampToByte((int)(re*128.0f));
  C.G2 = clampToByte((int)(ge*128.0f));
  C.B2 = clampToByte((int)(be*128.0f));
  CalcCrc();
}


//==========================================================================
//
//  VTextureTranslation::MapBlended
//
//==========================================================================
void VTextureTranslation::MapBlended (int AStart, int AEnd, int R, int G, int B) {
  AStart = clampval(AStart, 0, 255);
  AEnd = clampval(AEnd, 0, 255);
  R = clampval(R, 0, 255);
  G = clampval(G, 0, 255);
  B = clampval(B, 0, 255);
  // swap range if necesary
  if (AStart > AEnd) {
    int itmp = AStart;
    AStart = AEnd;
    AEnd = itmp;
  }
  for (int i = AStart; i <= AEnd; ++i) {
    float gray = colorIntensity(r_palette[i].r, r_palette[i].g, r_palette[i].b)/255.0f;
    Palette[i].r = clampToByte((int)(R*gray));
    Palette[i].g = clampToByte((int)(G*gray));
    Palette[i].b = clampToByte((int)(B*gray));
    Table[i] = R_LookupRGB(Palette[i].r, Palette[i].g, Palette[i].b);
  }
  VTransCmd &C = Commands.Alloc();
  C.Type = 3;
  C.Start = AStart;
  C.End = AEnd;
  C.R1 = R;
  C.G1 = G;
  C.B1 = B;
  CalcCrc();
}


//==========================================================================
//
//  VTextureTranslation::MapTinted
//
//==========================================================================
void VTextureTranslation::MapTinted (int AStart, int AEnd, int R, int G, int B, int Amount) {
  AStart = clampval(AStart, 0, 255);
  AEnd = clampval(AEnd, 0, 255);
  R = clampval(R, 0, 255);
  G = clampval(G, 0, 255);
  B = clampval(B, 0, 255);
  Amount = clampval(Amount, 0, 100);
  // swap range if necesary
  if (AStart > AEnd) {
    int itmp = AStart;
    AStart = AEnd;
    AEnd = itmp;
  }
  const float origAmount = float(100-Amount)/100.0f;
  const float rAmount = float(R)*(float(Amount)/100.0f);
  const float gAmount = float(G)*(float(Amount)/100.0f);
  const float bAmount = float(B)*(float(Amount)/100.0f);
  //translatedcolor = originalcolor * (100-amount)% + (r, g, b) * amount%
  for (int i = AStart; i <= AEnd; ++i) {
    Palette[i].r = clampToByte((int)(r_palette[i].r*origAmount+rAmount));
    Palette[i].g = clampToByte((int)(r_palette[i].g*origAmount+gAmount));
    Palette[i].b = clampToByte((int)(r_palette[i].b*origAmount+bAmount));
    Table[i] = R_LookupRGB(Palette[i].r, Palette[i].g, Palette[i].b);
  }
  VTransCmd &C = Commands.Alloc();
  C.Type = 4;
  C.Start = AStart;
  C.End = AEnd;
  C.R1 = R;
  C.G1 = G;
  C.B1 = B;
  C.R2 = Amount;
  CalcCrc();
}


//==========================================================================
//
//  SkipSpaces
//
//==========================================================================
static inline void SkipSpaces (const char *&pStr) {
  // skip whitespace
  while (*pStr && *((const vuint8 *)pStr) <= ' ') ++pStr;
}


//==========================================================================
//
//  CheckChar
//
//==========================================================================
static bool CheckChar (const char *&pStr, char Chr) {
  SkipSpaces(pStr);
  if (*pStr != Chr) return false;
  ++pStr;
  return true;
}


//==========================================================================
//
//  ExpectByte
//
//  returns negative number on error
//
//==========================================================================
static int ExpectByte (const char *&pStr) {
  SkipSpaces(pStr);
  if (pStr[0] < '0' || pStr[0] > '9') return -1;
  const char *end;
  int res = strtol(pStr, (char **)&end, 10);
  if (end == pStr) return -1;
  pStr = end;
  return clampval(res, 0, 255);
}


//==========================================================================
//
//  ExpectFloat
//
//  returns negative number on error
//
//==========================================================================
static float ExpectFloat (const char *&pStr) {
  SkipSpaces(pStr);
  if (pStr[0] != '.' && (pStr[0] < '0' || pStr[0] > '9')) return -1;
  const char *end;
  float res = strtof(pStr, (char **)&end);
  if (end == pStr) return -1;
  if (!isFiniteF(res)) return -1;
  pStr = end;
  return res;
}


//==========================================================================
//
//  VTextureTranslation::AddTransString
//
//==========================================================================
void VTextureTranslation::AddTransString (VStr Str) {
  const char *pStr = *Str;

  // parse start and end of the range
  int Start = ExpectByte(pStr);
  if (Start < 0) return;
  if (!CheckChar(pStr, ':')) return;

  int End = ExpectByte(pStr);
  if (End < 0) return;
  if (!CheckChar(pStr, '=')) return;

  if (CheckChar(pStr, '%')) {
    // desaturated
    if (!CheckChar(pStr, '[')) return;
    float rs = ExpectFloat(pStr);
    if (rs < 0) return;
    if (!CheckChar(pStr, ',')) return;
    float gs = ExpectFloat(pStr);
    if (gs < 0) return;
    if (!CheckChar(pStr, ',')) return;
    float bs = ExpectFloat(pStr);
    if (bs < 0) return;
    if (!CheckChar(pStr, ']')) return;
    if (!CheckChar(pStr, ':')) return;
    if (!CheckChar(pStr, '[')) return;
    float re = ExpectFloat(pStr);
    if (re < 0) return;
    if (!CheckChar(pStr, ',')) return;
    float ge = ExpectFloat(pStr);
    if (ge < 0) return;
    if (!CheckChar(pStr, ',')) return;
    float be = ExpectFloat(pStr);
    if (be < 0) return;
    if (!CheckChar(pStr, ']')) return;
    //GCon->Logf(NAME_Debug, "DESAT: <%s>", *Str);
    MapDesaturated(Start, End, rs, gs, bs, re, ge, be);
  } else if (CheckChar(pStr, '#')) {
    // blended
    if (!CheckChar(pStr, '[')) return;
    int R1 = ExpectByte(pStr);
    if (R1 < 0) return;
    if (!CheckChar(pStr, ',')) return;
    int G1 = ExpectByte(pStr);
    if (G1 < 0) return;
    if (!CheckChar(pStr, ',')) return;
    int B1 = ExpectByte(pStr);
    if (B1 < 0) return;
    if (!CheckChar(pStr, ']')) return;
    MapBlended(Start, End, R1, G1, B1);
  } else if (CheckChar(pStr, '@')) {
    // tinted
    int Amount = ExpectByte(pStr);
    if (Amount < 0) return;
    if (!CheckChar(pStr, '[')) return;
    int R1 = ExpectByte(pStr);
    if (R1 < 0) return;
    if (!CheckChar(pStr, ',')) return;
    int G1 = ExpectByte(pStr);
    if (G1 < 0) return;
    if (!CheckChar(pStr, ',')) return;
    int B1 = ExpectByte(pStr);
    if (B1 < 0) return;
    if (!CheckChar(pStr, ']')) return;
    MapTinted(Start, End, R1, G1, B1, Amount);
  } else if (!CheckChar(pStr, '[')) {
    int SrcStart = ExpectByte(pStr);
    if (SrcStart < 0) return;
    if (!CheckChar(pStr, ':')) return;
    int SrcEnd = ExpectByte(pStr);
    if (SrcEnd < 0) return;
    MapToRange(Start, End, SrcStart, SrcEnd);
  } else {
    int R1 = ExpectByte(pStr);
    if (R1 < 0) return;
    if (!CheckChar(pStr, ',')) return;
    int G1 = ExpectByte(pStr);
    if (G1 < 0) return;
    if (!CheckChar(pStr, ',')) return;
    int B1 = ExpectByte(pStr);
    if (B1 < 0) return;
    if (!CheckChar(pStr, ']')) return;
    if (!CheckChar(pStr, ':')) return;
    if (!CheckChar(pStr, '[')) return;
    int R2 = ExpectByte(pStr);
    if (R2 < 0) return;
    if (!CheckChar(pStr, ',')) return;
    int G2 = ExpectByte(pStr);
    if (G2 < 0) return;
    if (!CheckChar(pStr, ',')) return;
    int B2 = ExpectByte(pStr);
    if (B2 < 0) return;
    if (!CheckChar(pStr, ']')) return;
    MapToColors(Start, End, R1, G1, B1, R2, G2, B2);
  }
}
