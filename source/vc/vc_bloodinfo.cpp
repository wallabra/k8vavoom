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
  VStr name;
  TArray<VStr> matches;
  bool allowSuper;

public:
  DCKnownBlood () noexcept : name(), matches(), allowSuper(true) {}
  ~DCKnownBlood () noexcept { matches.clear(); name.clear(); }

  bool matchClass (VClass *c) const noexcept {
    for (; c; c = c->GetSuperClass()) {
      VStr cname(c->Name);
      for (auto &&mts : matches) {
        if (mts.isEmpty()) continue; // just in case
        if (cname.globMatchCI(mts)) return true;
      }
      if (!allowSuper) break;
    }
    return false;
  }
};


static TArray<DCKnownBlood> knownBlood;


//==========================================================================
//
//  DetectKnownBloodClass
//
//==========================================================================
static VStr DetectKnownBloodClass (VClass *c) {
  if (!c) return VStr::EmptyString;
  for (auto &&kb : knownBlood) if (kb.matchClass(c)) return kb.name;
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
    if (sc->Check("match")) {
      sc->Expect("=");
      if (sc->Check("[")) {
        while (!sc->Check("]")) {
          sc->ExpectString();
          if (!sc->String.isEmpty()) kblood.matches.append(sc->String);
          if (sc->Check(",")) continue;
          sc->Expect("]");
          break;
        }
      } else {
        sc->ExpectString();
        if (!sc->String.isEmpty()) kblood.matches.append(sc->String);
      }
      sc->Expect(";");
      continue;
    }
    sc->Error(va("unknown `know_blood` property '%s'", *sc->String));
  }
  if (kblood.name.isEmpty()) sc->Error("nameless `known_blood`");
  if (kblood.matches.length() == 0) return; // will never match anyway
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
//  ShutdownKnowBlood
//
//==========================================================================
static void ShutdownKnowBlood () {
  knownBlood.clear();
}
