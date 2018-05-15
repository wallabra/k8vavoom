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
//**  Copyright (C) 2018 Ketmar Dark
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
#include "net/network.h"
#include "sv_local.h"


// ////////////////////////////////////////////////////////////////////////// //
VDecalDef* VDecalDef::listHead = nullptr;
VDecalAnim* VDecalAnim::listHead = nullptr;
VDecalGroup* VDecalGroup::listHead = nullptr;


// ////////////////////////////////////////////////////////////////////////// //
static bool parseHexRGB (const VStr& str, float clr[]) {
  clr[0] = clr[1] = clr[2] = 0;
  size_t pos = 0;
  for (int f = 0; f < 3; ++f) {
    while (pos < str.Length() && str[pos] <= ' ') ++pos;
    int n = 0;
    for (int dnum = 0; dnum < 2; ++dnum) {
      if (pos >= str.Length()) {
        if (dnum == 0) return false;
        break;
      }
      char ch = str[pos++];
      if (ch <= ' ') break;
           if (ch >= '0' && ch <= '9') ch -= '0';
      else if (ch >= 'A' && ch <= 'F') ch -= 'A'-10;
      else if (ch >= 'a' && ch <= 'f') ch -= 'a'-10;
      else return false; // alas
      n = n*16+ch;
    }
    clr[f] = n/255.0f;
  }
  while (pos < str.Length() && str[pos] <= ' ') ++pos;
  return (pos >= str.Length());
}


// ////////////////////////////////////////////////////////////////////////// //
void VDecalDef::addToList (VDecalDef* dc) {
  if (!dc) return;
  dc->next = listHead;
  listHead = dc;
}


void VDecalDef::removeFromList (VDecalDef* dc) {
  VDecalDef* prev = nullptr;
  VDecalDef* cur = listHead;
  while (cur && cur != dc) { prev = cur; cur = cur->next; }
  // remove it from list, if found
  if (cur) {
    if (prev) prev->next = cur->next; else listHead = cur->next;
  }
}


// ////////////////////////////////////////////////////////////////////////// //
VDecalDef::~VDecalDef () {
  removeFromList(this);
}


// name is not parsed yet
bool VDecalDef::parse (VScriptParser* sc) {
  guard(VDecalDef::parse);

  sc->SetCMode(true);
  sc->ExpectString();
  if (sc->String.Length() == 0) { sc->Error("invalid decal name"); return false; }
  name = VName(*sc->String);
  sc->Expect("{");

  while (!sc->AtEnd()) {
    if (sc->Check("}")) return true;

    if (sc->Check("pic")) {
      sc->ExpectName8();
      pic = sc->Name8;
      continue;
    }

    if (sc->Check("shade")) {
      sc->ExpectString();
      if (!parseHexRGB(sc->String, shade)) { sc->Error("invalid color"); return false; }
      shade[3] = 1; // it colored!
      continue;
    }

    if (sc->Check("xscale")) { sc->ExpectFloat(); scaleX = sc->Float; continue; }
    if (sc->Check("yscale")) { sc->ExpectFloat(); scaleY = sc->Float; continue; }

    if (sc->Check("flipx")) { flipX = FlipAlways; continue; }
    if (sc->Check("flipy")) { flipY = FlipAlways; continue; }

    if (sc->Check("randomflipx")) { flipX = FlipRandom; continue; }
    if (sc->Check("randomflipy")) { flipY = FlipRandom; continue; }

    if (sc->Check("solid")) { alpha = 1; continue; }

    if (sc->Check("translucent")) { sc->ExpectFloat(); alpha = sc->Float; continue; }
    if (sc->Check("add")) { sc->ExpectFloat(); addAlpha = sc->Float; continue; }

    if (sc->Check("fuzzy")) { fuzzy = true; continue; }
    if (sc->Check("fullbright")) { fullbright = true; continue; }

    if (sc->Check("lowerdecal")) { sc->Error("'lowerdecal' property is not implemented"); return false; }

    if (sc->Check("animator")) { sc->ExpectString(); animname = VName(*sc->String); continue; }

    sc->Error(va("unknown decal keyword '%s'", *sc->String));
    break;
  }

  return false;
  unguard;
}


// ////////////////////////////////////////////////////////////////////////// //
void VDecalGroup::addToList (VDecalGroup* dg) {
  if (!dg) return;
  dg->next = listHead;
  listHead = dg;
}


void VDecalGroup::removeFromList (VDecalGroup* dg) {
  VDecalGroup* prev = nullptr;
  VDecalGroup* cur = listHead;
  while (cur && cur != dg) { prev = cur; cur = cur->next; }
  // remove it from list, if found
  if (cur) {
    if (prev) prev->next = cur->next; else listHead = cur->next;
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// name is not parsed yet
bool VDecalGroup::parse (VScriptParser* sc) {
  guard(VDecalGroup::parse);

  sc->SetCMode(true);
  sc->ExpectString();
  if (sc->String.Length() == 0) { sc->Error("invalid decal group name"); return false; }
  name = VName(*sc->String);
  sc->Expect("{");

  while (!sc->AtEnd()) {
    if (sc->Check("}")) return true;

    sc->ExpectString();
    if (sc->String.Length() == 0) { sc->Error("invalid decal name in group"); return false; }
    VName dn = VName(*sc->String);

    sc->ExpectNumber();
    while (sc->Number-- > 0) nameList.Append(dn);
  }

  return false;
  unguard;
}


// ////////////////////////////////////////////////////////////////////////// //
void VDecalAnim::addToList (VDecalAnim* anim) {
  if (!anim) return;
  anim->next = listHead;
  listHead = anim;
}


void VDecalAnim::removeFromList (VDecalAnim* anim) {
  VDecalAnim* prev = nullptr;
  VDecalAnim* cur = listHead;
  while (cur && cur != anim) { prev = cur; cur = cur->next; }
  // remove it from list, if found
  if (cur) {
    if (prev) prev->next = cur->next; else listHead = cur->next;
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// base decal animator class
VDecalAnim::~VDecalAnim () {
  removeFromList(this);
}


// ////////////////////////////////////////////////////////////////////////// //
VDecalAnimFader::~VDecalAnimFader () {
}


VDecalAnim* VDecalAnimFader::clone () {
  VDecalAnimFader* res = new VDecalAnimFader();
  res->startTime = startTime;
  res->actionTime = actionTime;
  return res;
}


void VDecalAnimFader::animate (decal_t* decal) {
}


bool VDecalAnimFader::parse (VScriptParser* sc) {
  guard(VDecalAnimFader::parse);

  sc->SetCMode(true);
  sc->ExpectString();
  if (sc->String.Length() == 0) { sc->Error("invalid decal fader name"); return false; }
  name = VName(*sc->String);
  sc->Expect("{");

  while (!sc->AtEnd()) {
    if (sc->Check("}")) return true;

    if (sc->Check("decaystart")) { sc->ExpectFloat(); startTime = sc->Float; continue; }
    if (sc->Check("decaytime")) { sc->ExpectFloat(); actionTime = sc->Float; continue; }

    sc->Error(va("unknown decal keyword '%s'", *sc->String));
    break;
  }

  return false;
  unguard;
}



// ////////////////////////////////////////////////////////////////////////// //
VDecalAnimStretcher::~VDecalAnimStretcher () {
}


VDecalAnim* VDecalAnimStretcher::clone () {
  VDecalAnimStretcher* res = new VDecalAnimStretcher();
  res->goalX = goalX;
  res->goalY = goalY;
  res->startTime = startTime;
  res->actionTime = actionTime;
  return res;
}


void VDecalAnimStretcher::animate (decal_t* decal) {
}


bool VDecalAnimStretcher::parse (VScriptParser* sc) {
  guard(VDecalAnimStretcher::parse);

  sc->SetCMode(true);
  sc->ExpectString();
  if (sc->String.Length() == 0) { sc->Error("invalid decal fader name"); return false; }
  name = VName(*sc->String);
  sc->Expect("{");

  while (!sc->AtEnd()) {
    if (sc->Check("}")) return true;

    if (sc->Check("goalx")) { sc->ExpectFloat(); goalX = sc->Float; continue; }
    if (sc->Check("goaly")) { sc->ExpectFloat(); goalY = sc->Float; continue; }
    if (sc->Check("stretchstart")) { sc->ExpectFloat(); startTime = sc->Float; continue; }
    if (sc->Check("stretchtime")) { sc->ExpectFloat(); actionTime = sc->Float; continue; }

    sc->Error(va("unknown decal keyword '%s'", *sc->String));
    break;
  }

  return false;
  unguard;
}


// ////////////////////////////////////////////////////////////////////////// //
VDecalAnimSlider::~VDecalAnimSlider () {
}


VDecalAnim* VDecalAnimSlider::clone () {
  VDecalAnimSlider* res = new VDecalAnimSlider();
  res->distX = distX;
  res->distY = distY;
  res->startTime = startTime;
  res->actionTime = actionTime;
  return res;
}


void VDecalAnimSlider::animate (decal_t* decal) {
}


bool VDecalAnimSlider::parse (VScriptParser* sc) {
  guard(VDecalAnimSlider::parse);

  sc->SetCMode(true);
  sc->ExpectString();
  if (sc->String.Length() == 0) { sc->Error("invalid decal fader name"); return false; }
  name = VName(*sc->String);
  sc->Expect("{");

  while (!sc->AtEnd()) {
    if (sc->Check("}")) return true;

    if (sc->Check("distx")) { sc->ExpectFloat(); distX = sc->Float; continue; }
    if (sc->Check("disty")) { sc->ExpectFloat(); distY = sc->Float; continue; }
    if (sc->Check("slidestart")) { sc->ExpectFloat(); startTime = sc->Float; continue; }
    if (sc->Check("slidetime")) { sc->ExpectFloat(); actionTime = sc->Float; continue; }

    sc->Error(va("unknown decal keyword '%s'", *sc->String));
    break;
  }

  return false;
  unguard;
}


// ////////////////////////////////////////////////////////////////////////// //
VDecalAnimColorChanger::~VDecalAnimColorChanger () {
}


VDecalAnim* VDecalAnimColorChanger::clone () {
  VDecalAnimColorChanger* res = new VDecalAnimColorChanger();
  res->dest[0] = dest[0];
  res->dest[1] = dest[1];
  res->dest[2] = dest[2];
  res->startTime = startTime;
  res->actionTime = actionTime;
  return res;
}


void VDecalAnimColorChanger::animate (decal_t* decal) {
}


bool VDecalAnimColorChanger::parse (VScriptParser* sc) {
  guard(VDecalAnimColorChanger::parse);

  sc->SetCMode(true);
  sc->ExpectString();
  if (sc->String.Length() == 0) { sc->Error("invalid decal fader name"); return false; }
  name = VName(*sc->String);
  sc->Expect("{");

  while (!sc->AtEnd()) {
    if (sc->Check("}")) return true;

    if (sc->Check("color")) {
      sc->ExpectString();
      if (!parseHexRGB(sc->String, dest)) { sc->Error("invalid color"); return false; }
      continue;
    }
    if (sc->Check("fadestart")) { sc->ExpectFloat(); startTime = sc->Float; continue; }
    if (sc->Check("fadetime")) { sc->ExpectFloat(); actionTime = sc->Float; continue; }

    sc->Error(va("unknown decal keyword '%s'", *sc->String));
    break;
  }

  return false;
  unguard;
}


// ////////////////////////////////////////////////////////////////////////// //
VDecalAnimCombiner::~VDecalAnimCombiner () {
}


VDecalAnim* VDecalAnimCombiner::clone () {
  VDecalAnimCombiner* res = new VDecalAnimCombiner();
  for (int f = 0; f < nameList.Num(); ++f) res->nameList.Append(nameList[f]);
  for (int f = 0; f < list.Num(); ++f) res->list.Append(list[f]);
  return res;
}


void VDecalAnimCombiner::animate (decal_t* decal) {
}


bool VDecalAnimCombiner::parse (VScriptParser* sc) {
  guard(VDecalAnimCombiner::parse);

  sc->SetCMode(true);
  sc->ExpectString();
  if (sc->String.Length() == 0) { sc->Error("invalid decal fader name"); return false; }
  name = VName(*sc->String);
  sc->Expect("{");

  while (!sc->AtEnd()) {
    if (sc->Check("}")) return true;

    sc->ExpectString();
    if (sc->String.Length() == 0) { sc->Error("invalid animation name in group"); return false; }
    VName dn = VName(*sc->String);

    nameList.Append(dn);
  }

  return false;
  unguard;
}


// ////////////////////////////////////////////////////////////////////////// //
void ParseDecalDef (VScriptParser* sc) {
  guard(ParseDecalDef);

  const unsigned int MaxStack = 64;
  VScriptParser* scstack[MaxStack];
  unsigned int scsp = 0;
  bool error = false;

  sc->SetCMode(false);

  for (;;) {
    while (!sc->AtEnd()) {
      if (sc->Check("include")) {
        sc->ExpectString();
        int lmp = W_CheckNumForFileName(sc->String);
        if (lmp >= 0) {
          if (scsp >= MaxStack) {
            sc->Error(va("mapinfo include nesting too deep"));
            error = true;
            break;
          }
          GCon->Logf("Including '%s'...", *sc->String);
          scstack[scsp++] = sc;
          sc = new VScriptParser(*sc->String, W_CreateLumpReaderNum(lmp));
        } else {
          sc->Error(va("mapinfo include '%s' not found", *sc->String));
          error = true;
          break;
        }
        continue;
      }

      if (sc->Check("generator")) {
        // not here yet
        sc->Message("WARNING: ignored 'generator' definition");
        sc->ExpectString();
        sc->ExpectString();
        continue;
      }

      if (sc->Check("decal")) {
        auto dc = new VDecalDef();
        if ((error = !dc->parse(sc)) == true) { delete dc; break; }
        sc->SetCMode(false);
        VDecalDef::addToList(dc);
        continue;
      }

      if (sc->Check("decalgroup")) {
        auto dg = new VDecalGroup();
        if ((error = !dg->parse(sc)) == true) { delete dg; break; }
        sc->SetCMode(false);
        VDecalGroup::addToList(dg);
        continue;
      }

      if (sc->Check("fader")) {
        auto ani = new VDecalAnimFader();
        if ((error = !ani->parse(sc)) == true) { delete ani; break; }
        sc->SetCMode(false);
        VDecalAnim::addToList(ani);
        continue;
      }

      if (sc->Check("stretcher")) {
        auto ani = new VDecalAnimStretcher();
        if ((error = !ani->parse(sc)) == true) { delete ani; break; }
        sc->SetCMode(false);
        VDecalAnim::addToList(ani);
        continue;
      }

      if (sc->Check("slider")) {
        auto ani = new VDecalAnimSlider();
        if ((error = !ani->parse(sc)) == true) { delete ani; break; }
        sc->SetCMode(false);
        VDecalAnim::addToList(ani);
        continue;
      }

      if (sc->Check("colorchanger")) {
        auto ani = new VDecalAnimColorChanger();
        if ((error = !ani->parse(sc)) == true) { delete ani; break; }
        sc->SetCMode(false);
        VDecalAnim::addToList(ani);
        continue;
      }

      if (sc->Check("combiner")) {
        auto ani = new VDecalAnimCombiner();
        if ((error = !ani->parse(sc)) == true) { delete ani; break; }
        sc->SetCMode(false);
        VDecalAnim::addToList(ani);
        continue;
      }

      sc->Error(va("Invalid command %s", *sc->String));
      error = true;
      break;
    }

    if (error) {
      while (scsp > 0) { delete sc; sc = scstack[--scsp]; }
      break;
    }
    if (scsp == 0) break;
    GCon->Logf("Finished included '%s'", *sc->GetLoc().GetSource());
    delete sc;
    sc = scstack[--scsp];
  }

  delete sc;

  unguard;
}


/*
void ParseWarning(TLocation l, const char *text, ...)
{
  char Buffer[2048];
  va_list argPtr;

  va_start(argPtr, text);
  vsprintf(Buffer, text, argPtr);
  va_end(argPtr);
#ifdef IN_VCC
  fprintf(stderr, "%s:%d: warning: %s\n", *l.GetSource(), l.GetLine(), Buffer);
#else
  GCon->Logf("%s:%d: warning: %s", *l.GetSource(), l.GetLine(), Buffer);
#endif
}
*/
