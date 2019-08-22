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
//  VTextureTranslation::MapToRange
//
//==========================================================================
void VTextureTranslation::MapToRange (int AStart, int AEnd, int ASrcStart, int ASrcEnd) {
  int Start;
  int End;
  int SrcStart;
  int SrcEnd;
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
  for (int i = Start; i < End; ++i, CurCol += ColStep) {
    Table[i] = int(CurCol);
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
  for (int i = Start; i < End; i++, CurR += RStep, CurG += GStep, CurB += BStep) {
    Palette[i].r = int(CurR);
    Palette[i].g = int(CurG);
    Palette[i].b = int(CurB);
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
//  CheckChar
//
//==========================================================================
static bool CheckChar (const char *&pStr, char Chr) {
  // skip whitespace
  while (*pStr && *((const vuint8 *)pStr) <= ' ') ++pStr;
  if (*pStr != Chr) return false;
  ++pStr;
  return true;
}


//==========================================================================
//
//  VTextureTranslation::AddTransString
//
//==========================================================================
void VTextureTranslation::AddTransString (VStr Str) {
  const char *pStr = *Str;

  // parse start and end of the range
  int Start = strtol(pStr, (char **)&pStr, 10);
  if (!CheckChar(pStr, ':')) return;

  int End = strtol(pStr, (char **)&pStr, 10);
  if (!CheckChar(pStr, '=')) return;

  /*if (CheckChar(pStr, '%')) {
    // desaturated crap
  } else*/
  if (!CheckChar(pStr, '[')) {
    int SrcStart = strtol(pStr, (char **)&pStr, 10);
    if (!CheckChar(pStr, ':')) return;
    int SrcEnd = strtol(pStr, (char **)&pStr, 10);
    MapToRange(Start, End, SrcStart, SrcEnd);
  } else {
    int R1 = strtol(pStr, (char **)&pStr, 10);
    if (!CheckChar(pStr, ',')) return;
    int G1 = strtol(pStr, (char **)&pStr, 10);
    if (!CheckChar(pStr, ',')) return;
    int B1 = strtol(pStr, (char **)&pStr, 10);
    if (!CheckChar(pStr, ']')) return;
    if (!CheckChar(pStr, ':')) return;
    if (!CheckChar(pStr, '[')) return;
    int R2 = strtol(pStr, (char **)&pStr, 10);
    if (!CheckChar(pStr, ',')) return;
    int G2 = strtol(pStr, (char **)&pStr, 10);
    if (!CheckChar(pStr, ',')) return;
    int B2 = strtol(pStr, (char **)&pStr, 10);
    if (!CheckChar(pStr, ']')) return;
    MapToColors(Start, End, R1, G1, B1, R2, G2, B2);
  }
}
