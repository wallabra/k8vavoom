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
#define VC_PUBLIC_WANT_CORE
#include "vc_public.h"


// ////////////////////////////////////////////////////////////////////////// //
TArray<VStr> TLocation::SourceFiles;
TMapDtor<VStr, vint32> TLocation::SourceFilesMap;


//==========================================================================
//
//  TLocation::AddSourceFile
//
//==========================================================================
int TLocation::AddSourceFile (const VStr &SName) {
  if (SourceFiles.length() == 0) {
    // add dummy source file
    SourceFiles.Append("<err>");
    SourceFilesMap.put("<err>", 0);
  }
  // find it
  auto val = SourceFilesMap.get(SName);
  if (val) return *val;
  // not found, add it
  int idx = SourceFiles.length();
  SourceFiles.Append(SName);
  SourceFilesMap.put(SName, idx);
  return idx;
}


//==========================================================================
//
//  TLocation::GetSource
//
//==========================================================================
VStr TLocation::GetSource () const {
  if (!Loc) return "(external)";
  int sidx = (Loc>>16)&0xffff;
  if (sidx >= SourceFiles.length()) return "<wutafuck>";
  return SourceFiles[sidx];
}


//==========================================================================
//
//  TLocation::ClearSourceFiles
//
//==========================================================================
void TLocation::ClearSourceFiles () {
  SourceFiles.Clear();
  SourceFilesMap.clear();
}


//==========================================================================
//
//  TLocation::toString
//
//==========================================================================
VStr TLocation::toString () const {
  if (GetLine()) {
    if (GetCol() > 0) {
      return GetSource()+":"+VStr(GetLine())+":"+VStr(GetCol());
    } else {
      return GetSource()+":"+VStr(GetLine())+":1";
    }
  } else {
    return VStr("(nowhere)");
  }
}


//==========================================================================
//
//  TLocation::toStringNoCol
//
//==========================================================================
VStr TLocation::toStringNoCol () const {
  if (GetLine()) {
    return GetSource()+":"+VStr(GetLine());
  } else {
    return VStr("(nowhere)");
  }
}


//==========================================================================
//
//  TLocation::toStringLineCol
//
//==========================================================================
VStr TLocation::toStringLineCol () const {
  if (GetLine()) {
    if (GetCol() > 0) {
      return VStr(GetLine())+":"+VStr(GetCol());
    } else {
      return VStr(GetLine())+":1";
    }
  } else {
    return VStr("(nowhere)");
  }
}


//==========================================================================
//
//  operator << (TLocation)
//
//==========================================================================
/*
VStream &operator << (VStream &Strm, TLocation &loc) {
  //FIXME: kill Schlemiel
  vint8 ll = (loc.Loc ? 1 : 0);
  Strm << ll;
  if (!ll) return Strm;
  vuint16 line = loc.Loc&0xffff;
  Strm << line;
  VStr sfn;
  if (Strm.IsLoading()) {
    Strm << sfn;
    int sidx = loc.AddSourceFile(sfn);
    loc.Loc = (sidx<<16)|line;
  } else {
    sfn = loc.GetSource();
    Strm << sfn;
  }
  return Strm;
}
*/
