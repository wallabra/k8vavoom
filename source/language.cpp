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

#include "gamedefs.h"


// ////////////////////////////////////////////////////////////////////////// //
struct VLanguage::VLangEntry {
  vint32 PassNum;
  VStr Value;
};


// ////////////////////////////////////////////////////////////////////////// //
VLanguage GLanguage;


//==========================================================================
//
//  VLanguage::VLanguage
//
//==========================================================================
VLanguage::VLanguage () : table(nullptr) {
}


//==========================================================================
//
//  VLanguage::~VLanguage
//
//==========================================================================
VLanguage::~VLanguage () {
  FreeData();
}


//==========================================================================
//
//  VLanguage::FreeData
//
//==========================================================================
void VLanguage::FreeData () {
  guard(VLanguage::FreeData);
  delete table;
  table = nullptr;
  unguard;
}


//==========================================================================
//
//  VLanguage::FreeNonDehackedStrings
//
//==========================================================================
void VLanguage::FreeNonDehackedStrings () {
  guard(VLanguage::FreeNonDehackedStrings);
  if (!table) return;
  for (auto it = table->first(); it; ++it) {
    if (it.getValue().PassNum != 0) it.removeCurrent();
  }
  unguard;
}


//==========================================================================
//
//  VLanguage::LoadStrings
//
//==========================================================================
void VLanguage::LoadStrings (const char *LangId) {
  guard(VLanguage::LoadStrings);
  if (!table) table = new TMap<VName, VLangEntry>();

  FreeNonDehackedStrings();

  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) == NAME_language) {
      int j = 1;
      if (VStr::Cmp(LangId, "**") != 0) {
        ParseLanguageScript(Lump, "*", true, j++);
        ParseLanguageScript(Lump, LangId, true, j++);
        ParseLanguageScript(Lump, LangId, false, j++);
      }
      ParseLanguageScript(Lump, "**", true, j++);
    }
  }
  unguard;
}


//==========================================================================
//
//  VLanguage::ParseLanguageScript
//
//==========================================================================
void VLanguage::ParseLanguageScript (vint32 Lump, const char *InCode, bool ExactMatch, vint32 PassNum) {
  guard(VLanguage::ParseLanguageScript);
  //fprintf(stderr, "LANG: <%s>\n", *W_LumpName(Lump));

  char Code[4];
  Code[0] = VStr::ToLower(InCode[0]);
  Code[1] = VStr::ToLower(InCode[1]);
  Code[2] = (ExactMatch ? VStr::ToLower(InCode[2]) : 0);
  Code[3] = 0;

  VScriptParser *sc = new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump));
  sc->SetCMode(true);

  bool GotLanguageCode = false;
  bool Skip = false;
  bool Finished = false;

  while (!sc->AtEnd()) {
    if (sc->Check("[")) {
      // language identifiers
      Skip = true;
      while (!sc->Check("]")) {
        sc->ExpectString();
        size_t Len = sc->String.Length();
        char CurCode[4];
        if (Len != 2 && Len != 3) {
          if (Len == 1 && sc->String[0] == '*') {
            CurCode[0] = '*';
            CurCode[1] = 0;
            CurCode[2] = 0;
          } else if (Len == 7 && !sc->String.ICmp("default")) {
            CurCode[0] = '*';
            CurCode[1] = '*';
            CurCode[2] = 0;
          } else {
            sc->Error(va("Language code must be 2 or 3 characters long, %s is %u characters long", *sc->String, (unsigned)Len));
            // shut up compiler
            CurCode[0] = 0;
            CurCode[1] = 0;
            CurCode[2] = 0;
          }
        } else {
          CurCode[0] = VStr::ToLower(sc->String[0]);
          CurCode[1] = VStr::ToLower(sc->String[1]);
          CurCode[2] = (ExactMatch ? VStr::ToLower(sc->String[2]) : 0);
          CurCode[3] = 0;
        }
        if (Code[0] == CurCode[0] && Code[1] == CurCode[1] && Code[2] == CurCode[2]) {
          Skip = false;
        }
        if (!GotLanguageCode && !Skip) {
          GCon->Logf(NAME_Dev, "parsing language script '%s' for language '%s'...", *W_FullLumpName(Lump), Code);
        }
        GotLanguageCode = true;
      }
    } else {
      if (!GotLanguageCode) {
        // skip old binary LANGUAGE lumps
        if (!sc->IsText()) {
          if (!Finished) GCon->Logf("Skipping binary LANGUAGE lump");
          Finished = true;
          return;
        }
        sc->Error("Found a string without language specified");
      }

      // parse string definitions
      if (Skip) {
        // we are skipping this language
        sc->ExpectString();
        //sc->Expect("=");
        //sc->ExpectString();
        while (!sc->Check(";")) sc->ExpectString();
        continue;
      }

      sc->ExpectString();

      if (sc->String == "$") {
        GCon->Logf(NAME_Warning, "%s: conditionals in language script is not supported yet", *sc->GetLoc().toStringNoCol());
        sc->Expect("ifgame");
        sc->Expect("(");
        while (!sc->Check(")")) sc->ExpectString();
        sc->ExpectString();
        sc->Expect("=");
        while (!sc->Check(";")) sc->ExpectString();
        continue;
      }

      VName Key(*sc->String, VName::AddLower);
      sc->Expect("=");
      sc->ExpectString();
      VStr Value = HandleEscapes(sc->String);
      while (!sc->Check(";")) {
        sc->ExpectString();
        Value += HandleEscapes(sc->String);
      }

      // check for replacement
      VLangEntry *Found = table->Find(Key);
      if (!Found || Found->PassNum >= PassNum) {
        VLangEntry Entry;
        Entry.Value = Value;
        Entry.PassNum = PassNum;
        table->Set(Key, Entry);
        //fprintf(stderr, "  LNG<%s>=<%s>\n", *Key, *Value);
        //GCon->Logf(NAME_Dev, "  [%s]=[%s]", *VStr(Key).quote(), *Value.quote());
      }
    }
  }
  delete sc;
  sc = nullptr;
  unguard;
}


//==========================================================================
//
//  VLanguage::HandleEscapes
//
//==========================================================================
VStr VLanguage::HandleEscapes (const VStr &Src) {
  guard(VLanguage::HandleEscapes);
  bool hasWork = false;
  for (size_t i = Src.Length(); i > 0; --i) if (Src[i-1] == '\\') { hasWork = true; break; }
  if (!hasWork) return VStr(Src);
  VStr Ret;
  for (int i = 0; i < Src.Length(); ++i) {
    char c = Src[i];
    if (c == '\\') {
      ++i;
      c = Src[i];
           if (c == 'n') c = '\n';
      else if (c == 'r') c = '\r';
      else if (c == 't') c = '\t';
      else if (c == 'c') c = -127;
      else if (c == '\n') continue;
    }
    Ret += c;
  }
  return Ret;
  unguard;
}


//==========================================================================
//
//  VLanguage::Find
//
//==========================================================================
VStr VLanguage::Find (VName Key, bool *found) const {
  guard(VLanguage::Find);
  if (Key == NAME_None) {
    if (found) *found = true;
    return VStr();
  }
  VLangEntry *Found = table->Find(Key);
  if (Found) {
    if (found) *found = true;
    return Found->Value;
  }
  // try lowercase
  for (const char *s = *Key; *s; ++s) {
    if (*s >= 'A' && *s <= 'Z') {
      // found uppercase letter, try lowercase name
      VName loname = VName(*VStr(*Key).toLowerCase());
      Found = table->Find(loname);
      if (Found) {
        if (found) *found = true;
        return Found->Value;
      }
    }
  }
  if (found) *found = false;
  return VStr();
  unguard;
}


//==========================================================================
//
//  VLanguage::operator[]
//
//==========================================================================
VStr VLanguage::operator [] (VName Key) const {
  guard(VLanguage::operator[]);
  bool found = false;
  VStr res = Find(Key, &found);
  if (found) return res;
  return VStr(Key);
  unguard;
}


//==========================================================================
//
//  VLanguage::HasTranslation
//
//==========================================================================
bool VLanguage::HasTranslation (VName s) const {
  bool found = false;
  VStr res = Find(s, &found);
  return found;
}


//==========================================================================
//
//  VLanguage::GetStringId
//
//==========================================================================
VName VLanguage::GetStringId (const VStr &Str) {
  guard(VLanguage::GetStringId);
  if (!table) return NAME_None;
  for (auto it = table->first(); it; ++it) {
    if (it.getValue().Value == Str) return it.GetKey();
  }
  return NAME_None;
  unguard;
}


//==========================================================================
//
//  VLanguage::ReplaceString
//
//==========================================================================
void VLanguage::ReplaceString (VName Key, const VStr &Value) {
  guard(VLanguage::ReplaceString);
  VLangEntry Entry;
  Entry.Value = Value;
  Entry.PassNum = 0;
  table->Set(Key, Entry);
  unguard;
}
