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
// this directly included from "vc_decorate.cpp"


struct DCKnownBlood {
public:
  struct ReplInfo {
    VStr oldname;
    VStr newname;
  };

public:
  VStr name;
  TArray<VStr> matches;
  TArray<VStr> anymatches;
  TArray<ReplInfo> replaces;
  bool allowSuper;
  bool blockSpawn;

private:
  static bool doMatch (const TArray<VStr> &mtlist, VClass *c, bool allowSuper) noexcept {
    for (; c; c = c->GetSuperClass()) {
      VStr cname(c->Name);
      for (auto &&mts : mtlist) {
        if (mts.isEmpty()) continue; // just in case
        if (cname.globMatchCI(mts)) return true;
      }
      if (!allowSuper) break;
    }
    return false;
  }

public:
  DCKnownBlood () noexcept : name(), matches(), anymatches(), replaces(), allowSuper(true), blockSpawn(true) {}
  ~DCKnownBlood () noexcept { matches.clear(); anymatches.clear(); replaces.clear(); name.clear(); }

  void appendReplace (VStr src, VStr dest) noexcept {
    if (src.isEmpty() || dest.isEmpty()) return;
    ReplInfo &ri = replaces.alloc();
    ri.oldname = src;
    ri.newname = dest;
  }

  bool matchClass (VClass *c, bool isBloodReplacement) const noexcept {
    if (isBloodReplacement && doMatch(matches, c, allowSuper)) return true;
    if (doMatch(anymatches, c, allowSuper)) return true;
    return false;
  }

  VClass *findReplace (VClass *c) const noexcept {
    if (!c) return nullptr;
    VStr cname(c->Name);
    for (auto &&it : replaces) {
      if (it.oldname.isEmpty()) continue; // just in case
      if (cname.globMatchCI(it.oldname)) {
        if (it.newname.isEmpty()) return nullptr; //wtf?!
        VClass *rep = VClass::FindClassNoCase(*it.newname);
        if (rep) return rep;
      }
    }
    return nullptr;
  }
};


static TArray<DCKnownBlood> knownBlood;


//==========================================================================
//
//  IsAnyBloodClass
//
//==========================================================================
static bool IsAnyBloodClass (VClass *c) {
  for (; c; c = c->GetSuperClass()) {
    if (c->Name == "Blood" || c->Name == "BloodSplatter" ||
        c->Name == "BloodSmear" || c->Name == "BloodSmearRadius" ||
        c->Name == "BloodSplatRadius" || c->Name == "BloodSplat" ||

        c->Name == "BloodGreen" || c->Name == "BloodSplatterGreen" ||
        c->Name == "BloodSmearGreen" || c->Name == "BloodSmearRadiusGreen" ||
        c->Name == "BloodSplatRadiusGreen" || c->Name == "BloodSplatGreen" ||

        c->Name == "BloodBlue" || c->Name == "BloodSplatterBlue" ||
        c->Name == "BloodSmearBlue" || c->Name == "BloodSmearRadiusBlue" ||
        c->Name == "BloodSplatRadiusBlue" || c->Name == "BloodSplatBlue")
    {
      return true;
    }
  }
  return false;
}


//==========================================================================
//
//  FindKnowBloodForcedReplacement
//
//==========================================================================
static VClass *FindKnowBloodForcedReplacement (VClass *c) {
  if (!c) return nullptr;
  for (auto &&kb : knownBlood) {
    VClass *rep = kb.findReplace(c);
    if (rep) {
      if (rep == c) return nullptr;
      return rep;
    }
  }
  return nullptr;
}


//==========================================================================
//
//  DetectKnownBloodClass
//
//==========================================================================
static VStr DetectKnownBloodClass (VClass *c, bool *blockSpawn, bool isBloodReplacement) {
  if (!c) return VStr::EmptyString;
  //GCon->Logf(NAME_Debug, "KNB: <%s> (%s)", c->GetName(), (isBloodReplacement ? "brepl" : "normal"));
  for (auto &&kb : knownBlood) {
    if (kb.matchClass(c, isBloodReplacement)) {
      if (blockSpawn) *blockSpawn = kb.blockSpawn;
      return kb.name;
    }
  }
  return VStr::EmptyString;
}


//==========================================================================
//
//  ParseKnownBloodSection
//
//==========================================================================
static void ParseKnownBloodSection (VScriptParser *sc) {
  sc->Expect("{");
  DCKnownBlood kblood;
  while (!sc->Check("}")) {
    // name
    if (sc->Check("name")) {
      if (!kblood.name.isEmpty()) sc->Error("duplicate known blood name");
      sc->Expect("=");
      sc->ExpectString();
      if (sc->String.isEmpty()) sc->Error("empty known blood name");
      kblood.name = sc->String;
      sc->Expect(";");
      continue;
    }
    // match
    int matchtype = (sc->Check("match") ? 1 : sc->Check("matchany") ? 2 : 0);
    if (matchtype) {
      sc->Expect("=");
      if (sc->Check("[")) {
        while (!sc->Check("]")) {
          sc->ExpectString();
          if (!sc->String.isEmpty()) {
            if (matchtype == 1) kblood.matches.append(sc->String); else kblood.anymatches.append(sc->String);
          }
          if (sc->Check(",")) continue;
          sc->Expect("]");
          break;
        }
      } else {
        sc->ExpectString();
        if (!sc->String.isEmpty()) {
          if (matchtype == 1) kblood.matches.append(sc->String); else kblood.anymatches.append(sc->String);
        }
      }
      sc->Expect(";");
      continue;
    }
    // blockspawn
    if (sc->Check("blockspawn")) {
      sc->Expect("=");
           if (sc->Check("true") || sc->Check("tan")) kblood.blockSpawn = true;
      else if (sc->Check("false") || sc->Check("ona")) kblood.blockSpawn = false;
      else sc->Error(va("boolean value expected for 'blockspawn', but got '%s'", *sc->String));
      sc->Expect(";");
      continue;
    }
    // replace
    if (sc->Check("replace")) {
      sc->ExpectString();
      VStr src = sc->String;
      sc->Expect("with");
      sc->ExpectString();
      VStr dest = sc->String;
      sc->Expect(";");
      kblood.appendReplace(src, dest);
      continue;
    }
    sc->Error(va("unknown `know_blood` property '%s'", *sc->String));
  }
  if (kblood.name.isEmpty()) sc->Error("nameless `known_blood`");
  if (kblood.matches.length() == 0 && kblood.anymatches.length() == 0 && kblood.replaces.length() == 0) return; // will never match anyway
  knownBlood.append(kblood);
}


//==========================================================================
//
//  ParseKnownBloodFile
//
//==========================================================================
static void ParseKnownBloodFile (VScriptParser *sc) {
  sc->SetCMode(true);
  sc->SetEscape(true);
  for (;;) {
    if (sc->Check("known_blood")) {
      ParseKnownBloodSection(sc);
      continue;
    }
    if (!sc->GetString()) break;
    sc->Error(va("unknown section '%s'", *sc->String));
  }
  delete sc;
}


//==========================================================================
//
//  LoadKnownBlood
//
//==========================================================================
static void LoadKnownBlood () {
  for (auto &&it : WadFileIterator("vavoom_known_blood.rc")) {
    GLog.Logf(NAME_Init, "Parsing known blood script '%s'...", *it.getFullName());
    ParseKnownBloodFile(new VScriptParser(it.getFullName(), W_CreateLumpReaderNum(it.lump)));
  }
}


//==========================================================================
//
//  ShutdownKnownBlood
//
//==========================================================================
static void ShutdownKnownBlood () {
  knownBlood.clear();
}
