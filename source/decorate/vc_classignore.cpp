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
//**  Copyright (C) 2018-2021 Ketmar Dark
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
// this directly included from "vc_decorate.cpp"

static bool wasD4VFixes = false;


// ////////////////////////////////////////////////////////////////////////// //
struct DCKnownClassIgnore {
public:
  enum {
    Unknown,
    D4V,
    D4VRequired,
    Skulltag,
  };

public:
  VStr className;
  VStr parentName;
  VStr replaceName;
  VStr fixName;
  int fixType;

public:
  DCKnownClassIgnore () noexcept : className(), parentName(), replaceName(), fixName(), fixType(Unknown) {}
  ~DCKnownClassIgnore () noexcept { className.clear(); parentName.clear(); replaceName.clear(); fixName.clear(); }

  bool matchClass (const VStr &NameStr, const VStr &ParentStr, const VStr &ReplaceStr) const noexcept {
    if (fixType == D4VRequired && !wasD4VFixes) return false;
    if (!className.isEmpty()) { if (!NameStr.globMatchCI(className)) return false; }
    if (!parentName.isEmpty()) { if (!ParentStr.globMatchCI(parentName)) return false; }
    if (!replaceName.isEmpty()) { if (!ReplaceStr.globMatchCI(replaceName)) return false; }
    if (fixType == D4V) wasD4VFixes = true;
    return true;
  }

  bool matchClass (const VStr &NameStr, const VStr &ParentStr) const noexcept {
    if (fixType == D4VRequired && !wasD4VFixes) return false;
    if (!replaceName.isEmpty()) return false;
    if (!className.isEmpty()) { if (!NameStr.globMatchCI(className)) return false; }
    if (!parentName.isEmpty()) { if (!ParentStr.globMatchCI(parentName)) return false; }
    if (fixType == D4V) wasD4VFixes = true;
    return true;
  }
};


static TArray<DCKnownClassIgnore> knownClassIgnores;


//==========================================================================
//
//  CheckParentErrorHacks
//
//  returns `true` if this error should be ignored
//
//==========================================================================
static bool CheckParentErrorHacks (VScriptParser *sc, const VStr &NameStr, const VStr &ParentStr) {
  for (auto &&ign : knownClassIgnores) {
    if (ign.matchClass(NameStr, ParentStr)) {
      sc->Message(va("Parent class `%s` not found for actor `%s` (ignored)", *ParentStr, *NameStr));
      return true;
    }
  }
  //GCon->Logf(NAME_Debug, "class_ignore \"WTF\" {\n  class = \"%s\";\n  parent = \"%s\";\n  replace = \"%s\";\n  type = Unknown;\n}", *NameStr, *ParentStr, *ReplaceStr);
  return false;
}


//==========================================================================
//
//  CheckReplaceErrorHacks
//
//  returns `true` if this error should be ignored
//
//==========================================================================
static bool CheckReplaceErrorHacks (VScriptParser *sc, const VStr &NameStr, const VStr &ParentStr, const VStr &ReplaceStr) {
  for (auto &&ign : knownClassIgnores) {
    if (ign.matchClass(NameStr, ParentStr, ReplaceStr)) {
      sc->Message(va("Replaced class `%s` not found for actor `%s` (%s fix applied)", *ReplaceStr, *NameStr, *ign.fixName));
      return true;
    }
  }
  //GCon->Logf(NAME_Debug, "class_ignore \"WTF\" {\n  class = \"%s\";\n  parent = \"%s\";\n  replace = \"%s\";\n  type = Unknown;\n}", *NameStr, *ParentStr, *ReplaceStr);
  return false;
}


//==========================================================================
//
//  ParseKnownClassIgnoreSection
//
//==========================================================================
static void ParseKnownClassIgnoreSection (VScriptParser *sc) {
  sc->ExpectString();
  if (sc->String.IsEmpty()) sc->Error("empty ignore group name");
  DCKnownClassIgnore kclass;
  kclass.fixName = sc->String;
  sc->Expect("{");
  while (!sc->Check("}")) {
    // type
    if (sc->Check("type")) {
      sc->Expect("=");
           if (sc->Check("D4V")) kclass.fixType = DCKnownClassIgnore::D4V;
      else if (sc->Check("D4VRequired")) kclass.fixType = DCKnownClassIgnore::D4VRequired;
      else if (sc->Check("Skulltag")) kclass.fixType = DCKnownClassIgnore::Skulltag;
      else if (sc->Check("Unknown")) kclass.fixType = DCKnownClassIgnore::Unknown;
      else { sc->ExpectString(); sc->Error(va("invalid ignore type '%s'", *sc->String)); }
      sc->Expect(";");
      continue;
    }
    // other properties
    VStr *propStr = nullptr;
    // class
         if (sc->Check("class")) propStr = &kclass.className;
    else if (sc->Check("parent")) propStr = &kclass.parentName;
    else if (sc->Check("replace")) propStr = &kclass.replaceName;
    else { sc->ExpectString(); sc->Error(va("unknown `class_ignore` property '%s'", *sc->String)); }
    vassert(propStr != nullptr);
    sc->Expect("=");
    sc->ExpectString();
    (*propStr) = sc->String;
    sc->Expect(";");
    continue;
  }
  knownClassIgnores.append(kclass);
}


//==========================================================================
//
//  ParseKnownClassIgnoreFile
//
//==========================================================================
static void ParseKnownClassIgnoreFile (VScriptParser *sc) {
  sc->SetCMode(true);
  sc->SetEscape(true);
  for (;;) {
    if (sc->Check("class_ignore")) {
      ParseKnownClassIgnoreSection(sc);
      continue;
    }
    if (!sc->GetString()) break;
    sc->Error(va("unknown section '%s'", *sc->String));
  }
  delete sc;
}


//==========================================================================
//
//  LoadKnownClassIgnores
//
//==========================================================================
static void LoadKnownClassIgnores () {
  for (auto &&it : WadFileIterator("vavoom_class_ignores.rc")) {
    GLog.Logf(NAME_Init, "Parsing known class ignore script '%s'", *it.getFullName());
    ParseKnownClassIgnoreFile(new VScriptParser(it.getFullName(), W_CreateLumpReaderNum(it.lump)));
  }
}


//==========================================================================
//
//  ShutdownKnownClassIgnores
//
//==========================================================================
static void ShutdownKnownClassIgnores () {
  knownClassIgnores.clear();
}
