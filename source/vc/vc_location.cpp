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

#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
# include "gamedefs.h"
#else
# if defined(IN_VCC)
#  include "../../utils/vcc/vcc.h"
# elif defined(VCC_STANDALONE_EXECUTOR)
#  include "../../vccrun/vcc_run.h"
# endif
#endif


// ////////////////////////////////////////////////////////////////////////// //
TArray<VStr> TLocation::SourceFiles;
TMapDtor<VStr, vint32> TLocation::SourceFilesMap;


//==========================================================================
//
//  TLocation::AddSourceFile
//
//  FIXME: kill Schlemiel
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
//  operator << (TLocation)
//
//==========================================================================
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
