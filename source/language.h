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

class VLanguage {
private:
  struct VLangEntry;

  TMap<VName, VLangEntry> *table;

  void FreeNonDehackedStrings ();
  void ParseLanguageScript (vint32 Lump, const char *InCode, bool ExactMatch, vint32 PassNum);
  VStr HandleEscapes (const VStr &Src);

public:
  VLanguage ();
  ~VLanguage ();

  void FreeData ();
  void LoadStrings (const char *LangId);

  VStr Find (VName) const;
  VStr operator [] (VName) const;

  bool HasTranslation (VName s) const;

  VName GetStringId (const VStr &);
  void ReplaceString (VName, const VStr &);
};


extern VLanguage GLanguage;
