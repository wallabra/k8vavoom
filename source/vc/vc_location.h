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


// ////////////////////////////////////////////////////////////////////////// //
// describes location in a source file
class TLocation {
private:
  vuint32 Loc;
  vuint32 Col;
  static TArray<VStr> SourceFiles;
  static TMapDtor<VStr, vint32> SourceFilesMap; // to avoid Schlemiel's curse

public:
  TLocation () : Loc(0), Col(0) {}
  TLocation (int SrcIdx, int Line, int ACol) : Loc(((SrcIdx&0xffff)<<16)|(Line&0xffff)), Col(ACol&0x7fffffff) {}
  inline int GetLine () const { return (Loc&0xffff); }
  inline void SetLine (int Line) { Loc = (Loc&0xffff0000)|(Line&0xffff); }
  inline int GetCol () const { return Col&0x7fffffff; }
  VStr GetSource () const;
  inline bool isInternal () const { return (Loc == 0); }

  static int AddSourceFile (const VStr &);
  static void ClearSourceFiles ();

  VStr toString () const;
  VStr toStringNoCol () const;
  VStr toStringLineCol () const;

  inline void ConsumeChar (bool doNewline) {
    if (doNewline) { ++Loc; Col = 1; } else ++Col;
  }

  //friend VStream &operator << (VStream &, TLocation &);
};
