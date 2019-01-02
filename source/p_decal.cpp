//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
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
#include "net/network.h"
#include "sv_local.h"
#include "render/r_local.h"


// ////////////////////////////////////////////////////////////////////////// //
VCvarB r_decals_enabled("r_decals_enabled", true, "Enable decal spawning, processing and rendering?", CVAR_Archive);


// ////////////////////////////////////////////////////////////////////////// //
// all names are lowercased
static TMapNC<VName, bool> optionalDecals;
static TMapNC<VName, bool> optionalDecalGroups;


static void addOptionalDecal (VName name) {
  if (name == NAME_None) return;
  VName n(*name, VName::AddLower);
  optionalDecals.put(n, true);
}

static void addOptionalDecalGroup (VName name) {
  if (name == NAME_None) return;
  VName n(*name, VName::AddLower);
  optionalDecalGroups.put(n, true);
}


static bool isOptionalDecal (VName name) {
  if (name == NAME_None) return true;
  VName n(*name, VName::AddLower);
  return optionalDecals.has(n);
}


static bool isOptionalDecalGroup (VName name) {
  if (name == NAME_None) return true;
  VName n(*name, VName::AddLower);
  return optionalDecalGroups.has(n);
}


// ////////////////////////////////////////////////////////////////////////// //
VDecalDef *VDecalDef::listHead = nullptr;
VDecalAnim *VDecalAnim::listHead = nullptr;
VDecalGroup *VDecalGroup::listHead = nullptr;


// ////////////////////////////////////////////////////////////////////////// //
static bool parseHexRGB (const VStr &str, int *clr) {
  vuint32 ppc = M_ParseColour(str);
  if (clr) *clr = ppc&0xffffff;
  return true;
}


// ////////////////////////////////////////////////////////////////////////// //
void VDecalDef::addToList (VDecalDef *dc) {
  if (!dc) return;
  if (dc->name == NAME_None) { delete dc; return; }
  // remove old definitions
  //FIXME: memory leak
  VDecalDef::removeFromList(VDecalDef::find(dc->name));
  VDecalGroup::removeFromList(VDecalGroup::find(dc->name));
  // insert new one
  dc->next = listHead;
  listHead = dc;
}


void VDecalDef::removeFromList (VDecalDef *dc) {
  VDecalDef *prev = nullptr;
  VDecalDef *cur = listHead;
  while (cur && cur != dc) { prev = cur; cur = cur->next; }
  // remove it from list, if found
  if (cur) {
    if (prev) prev->next = cur->next; else listHead = cur->next;
  }
}


VDecalDef *VDecalDef::find (const VStr &aname) {
  VName xn = VName(*aname, VName::Find);
  if (xn == NAME_None) return nullptr;
  return find(xn);
}

VDecalDef *VDecalDef::find (const VName &aname) {
  for (auto it = listHead; it; it = it->next) {
    if (it->name == aname) return it;
  }
  for (auto it = listHead; it; it = it->next) {
    if (VStr::ICmp(*it->name, *aname) == 0) return it;
  }
  return nullptr;
}


VDecalDef *VDecalDef::findById (int id) {
  if (id < 0) return nullptr;
  for (auto it = listHead; it; it = it->next) {
    if (it->id == id) return it;
  }
  return nullptr;
}


bool VDecalDef::hasDecal (const VName &aname) {
  if (VDecalDef::find(aname)) return true;
  if (VDecalGroup::find(aname)) return true;
  return false;
}


VDecalDef *VDecalDef::getDecal (const VStr &aname) {
  VName xn = VName(*aname, VName::Find);
  if (xn == NAME_None) return nullptr;
  return getDecal(xn);
}


VDecalDef *VDecalDef::getDecal (const VName &aname) {
  VDecalDef *dc = VDecalDef::find(aname);
  if (dc) return dc;
  // try group
  VDecalGroup *gp = VDecalGroup::find(aname);
  if (!gp) return nullptr;
  return gp->chooseDecal();
}


VDecalDef *VDecalDef::getDecalById (int id) {
  return VDecalDef::findById(id);
}


// ////////////////////////////////////////////////////////////////////////// //
VDecalDef::~VDecalDef () {
  removeFromList(this);
}


void VDecalDef::fixup () {
  if (animname == NAME_None) return;
  animator = VDecalAnim::find(animname);
  if (!animator) GCon->Logf(NAME_Warning, "decal '%s': animator '%s' not found!", *name, *animname);
}


// name is not parsed yet
bool VDecalDef::parse (VScriptParser *sc) {
  guard(VDecalDef::parse);

  sc->SetCMode(false);
  sc->ExpectString();
  if (sc->String.Length() == 0) { sc->Error("invalid decal name"); return false; }
  name = VName(*sc->String);
  if (sc->CheckNumber()) id = sc->Number; // this is decal id

  if (sc->Check("optional")) addOptionalDecal(name);

  sc->Expect("{");

  VName pic = NAME_None;
  texid = -1;
  int shadeclr = -1;

  while (!sc->AtEnd()) {
    if (sc->Check("}")) {
      // load texture (and shade it if necessary)
      if (pic == NAME_None) {
        if (!isOptionalDecal(name)) GCon->Logf(NAME_Warning, "decal '%s' has no pic defined", *name);
        return true;
      }
      //texid = GTextureManager./*AddPatch*/CheckNumForNameAndForce(pic, TEXTYPE_Pic, false, false, true);
      if (shadeclr != -1) {
        texid = GTextureManager.AddPatchShaded(pic, TEXTYPE_Pic, shadeclr, true);
        if (texid < 0 && VStr::length(*pic) > 8) {
          // try short version
          VStr pn = VStr(*pic);
          VName pp = *pn.left(8);
          texid = GTextureManager.AddPatchShaded(pp, TEXTYPE_Pic, shadeclr, true);
        }
        //GCon->Logf(NAME_Init, "SHADED DECAL: texture is '%s', shade is 0x%08x", *pic, (vuint32)shadeclr);
      } else {
        texid = GTextureManager.AddPatch(pic, TEXTYPE_Pic, true);
        if (texid < 0 && VStr::length(*pic) > 8) {
          // try short version
          VStr pn = VStr(*pic);
          VName pp = *pn.left(8);
          texid = GTextureManager.AddPatch(pp, TEXTYPE_Pic, true);
        }
      }
      if (texid < 0) {
        if (!isOptionalDecal(name)) GCon->Logf(NAME_Warning, "decal '%s' has no pic '%s'", *name, *pic);
        return true;
      }
      return true;
    }

    if (sc->Check("pic")) {
      sc->ExpectName();
      pic = sc->Name;
      continue;
    }

    if (sc->Check("shade")) {
      sc->ExpectString();
      if (sc->String.ICmp("BloodDefault") == 0) {
        if (!parseHexRGB("88 00 00", &shadeclr)) { sc->Error("invalid color"); return false; }
      } else {
        if (!parseHexRGB(sc->String, &shadeclr)) { sc->Error("invalid color"); return false; }
      }
      continue;
    }

    if (sc->Check("x-scale")) { sc->ExpectFloat(); scaleX = sc->Float; continue; }
    if (sc->Check("y-scale")) { sc->ExpectFloat(); scaleY = sc->Float; continue; }

    if (sc->Check("flipx")) { flipX = FlipAlways; continue; }
    if (sc->Check("flipy")) { flipY = FlipAlways; continue; }

    if (sc->Check("randomflipx")) { flipX = FlipRandom; continue; }
    if (sc->Check("randomflipy")) { flipY = FlipRandom; continue; }

    if (sc->Check("solid")) { alpha = 1; continue; }

    if (sc->Check("translucent")) { sc->ExpectFloat(); alpha = sc->Float; continue; }
    if (sc->Check("add")) { sc->ExpectFloat(); addAlpha = sc->Float; continue; }

    if (sc->Check("fuzzy")) { fuzzy = true; continue; }
    if (sc->Check("fullbright")) { fullbright = true; continue; }

    if (sc->Check("lowerdecal")) {
      sc->ExpectString();
      if (sc->String.Length() == 0) { sc->Error("invalid lower decal name"); return false; }
      lowername = VName(*sc->String);
      continue;
    }

    if (sc->Check("animator")) { sc->ExpectString(); animname = VName(*sc->String); continue; }

    sc->Error(va("unknown decal keyword '%s'", *sc->String));
    break;
  }

  return false;
  unguard;
}


// ////////////////////////////////////////////////////////////////////////// //
void VDecalGroup::addToList (VDecalGroup *dg) {
  if (!dg) return;
  if (dg->name == NAME_None) { delete dg; return; }
  // remove old definitions
  //FIXME: memory leak
  //if (VDecalDef::find(dg->name)) GCon->Logf("replaced decal '%s'...", *dg->name);
  VDecalDef::removeFromList(VDecalDef::find(dg->name));
  //if (VDecalGroup::find(dg->name)) GCon->Logf("replaced group '%s'...", *dg->name);
  VDecalGroup::removeFromList(VDecalGroup::find(dg->name));
  //GCon->Logf("new group: '%s' (%d items)", *dg->name, dg->nameList.Num());
  // insert new one
  dg->next = listHead;
  listHead = dg;
}


void VDecalGroup::removeFromList (VDecalGroup *dg) {
  VDecalGroup *prev = nullptr;
  VDecalGroup *cur = listHead;
  while (cur && cur != dg) { prev = cur; cur = cur->next; }
  // remove it from list, if found
  if (cur) {
    if (prev) prev->next = cur->next; else listHead = cur->next;
  }
}


VDecalGroup *VDecalGroup::find (const VStr &aname) {
  VName xn = VName(*aname, VName::Find);
  if (xn == NAME_None) return nullptr;
  return find(xn);
}

VDecalGroup *VDecalGroup::find (const VName &aname) {
  for (auto it = listHead; it; it = it->next) {
    if (it->name == aname) return it;
  }
  for (auto it = listHead; it; it = it->next) {
    if (VStr::ICmp(*it->name, *aname) == 0) return it;
  }
  return nullptr;
}


// ////////////////////////////////////////////////////////////////////////// //
void VDecalGroup::fixup () {
  //GCon->Logf("fixing decal group '%s'...", *name);
  for (int f = 0; f < nameList.Num(); ++f) {
    auto it = VDecalDef::find(nameList[f].name);
    if (it) {
      auto li = new ListItem(it, nullptr);
      //GCon->Logf("  adding decal '%s' (%u)", *it->name, (unsigned)nameList[f].weight);
      list.AddEntry(li, nameList[f].weight);
      continue;
    }
    auto itg = VDecalGroup::find(nameList[f].name);
    if (itg) {
      auto li = new ListItem(nullptr, itg);
      //GCon->Logf("  adding group '%s' (%u)", *itg->name, (unsigned)nameList[f].weight);
      list.AddEntry(li, nameList[f].weight);
      continue;
    }
    if (!isOptionalDecalGroup(name) && !isOptionalDecal(nameList[f].name)) {
      GCon->Logf(NAME_Warning, "decalgroup '%s' contains unknown decal '%s'!", *name, *nameList[f].name);
    }
  }
}


VDecalDef *VDecalGroup::chooseDecal (int reclevel) {
  if (reclevel > 64) return nullptr; // too deep
  auto li = list.PickEntry();
  if (li) {
    if (li->dd) return li->dd;
    if (li->dg) return li->dg->chooseDecal(reclevel+1);
  }
  return nullptr;
}


// name is not parsed yet
bool VDecalGroup::parse (VScriptParser *sc) {
  guard(VDecalGroup::parse);

  sc->SetCMode(false);
  sc->ExpectString();
  if (sc->String.Length() == 0) { sc->Error("invalid decal group name"); return false; }
  name = VName(*sc->String);

  if (sc->Check("optional")) addOptionalDecalGroup(name);

  sc->Expect("{");

  while (!sc->AtEnd()) {
    if (sc->Check("}")) return true;

    sc->ExpectString();
    if (sc->String.Length() == 0) { sc->Error("invalid decal name in group"); return false; }
    VName dn = VName(*sc->String);

    sc->ExpectNumber();
    if (sc->Number > 65535) sc->Number = 65535;
    nameList.Append(NameListItem(dn, sc->Number));
  }

  return false;
  unguard;
}


// ////////////////////////////////////////////////////////////////////////// //
void VDecalAnim::addToList (VDecalAnim *anim) {
  if (!anim) return;
  if (anim->name == NAME_None) { delete anim; return; }
  // remove old definition
  for (auto it = listHead, prev = (VDecalAnim *)nullptr; it; prev = it, it = it->next) {
    if (it->name == anim->name) {
      if (prev) prev->next = it->next; else listHead = it->next;
      delete it;
      break;
    }
  }
  // insert new one
  anim->next = listHead;
  listHead = anim;
}


void VDecalAnim::removeFromList (VDecalAnim *anim) {
  VDecalAnim *prev = nullptr;
  VDecalAnim *cur = listHead;
  while (cur && cur != anim) { prev = cur; cur = cur->next; }
  // remove it from list, if found
  if (cur) {
    if (prev) prev->next = cur->next; else listHead = cur->next;
  }
}


VDecalAnim *VDecalAnim::find (const VStr &aname) {
  VName xn = VName(*aname, VName::Find);
  if (xn == NAME_None) return nullptr;
  return find(xn);
}

VDecalAnim *VDecalAnim::find (const VName &aname) {
  for (auto it = listHead; it; it = it->next) {
    if (it->name == aname) return it;
  }
  for (auto it = listHead; it; it = it->next) {
    if (VStr::ICmp(*it->name, *aname) == 0) return it;
  }
  return nullptr;
}


// ////////////////////////////////////////////////////////////////////////// //
// base decal animator class
VDecalAnim::~VDecalAnim () {
  removeFromList(this);
}


void VDecalAnim::fixup () {
}


// ////////////////////////////////////////////////////////////////////////// //
VDecalAnimFader::~VDecalAnimFader () {
}


VDecalAnim *VDecalAnimFader::clone () {
  VDecalAnimFader *res = new VDecalAnimFader();
  res->name = name;
  res->startTime = startTime;
  res->actionTime = actionTime;
  res->timePassed = timePassed;
  return res;
}


void VDecalAnimFader::doIO (VStream &Strm) {
  guard(VDecalAnimFader::doIO);
  Strm << timePassed;
  Strm << startTime;
  Strm << actionTime;
  unguard;
}


bool VDecalAnimFader::animate (decal_t *decal, float timeDelta) {
  guard(VDecalAnimFader::animate);
  if (decal->origAlpha <= 0 || decal->alpha <= 0) return false;
  timePassed += timeDelta;
  if (timePassed < startTime) return true; // not yet
  if (timePassed >= startTime+actionTime || actionTime <= 0) {
    //GCon->Logf("decal %p completely faded away", decal);
    decal->alpha = 0;
    return false;
  }
  float dtx = timePassed-startTime;
  float aleft = decal->origAlpha;
  decal->alpha = aleft-aleft*dtx/actionTime;
  //GCon->Logf("decal %p: dtx=%f; origA=%f; a=%f", decal, dtx, decal->origAlpha, decal->alpha);
  return (decal->alpha > 0);
  unguard;
}


bool VDecalAnimFader::parse (VScriptParser *sc) {
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


VDecalAnim *VDecalAnimStretcher::clone () {
  VDecalAnimStretcher *res = new VDecalAnimStretcher();
  res->name = name;
  res->goalX = goalX;
  res->goalY = goalY;
  res->startTime = startTime;
  res->actionTime = actionTime;
  res->timePassed = timePassed;
  return res;
}


void VDecalAnimStretcher::doIO (VStream &Strm) {
  guard(VDecalAnimStretcher::doIO);
  Strm << timePassed;
  Strm << goalX;
  Strm << goalY;
  Strm << startTime;
  Strm << actionTime;
  unguard;
}


bool VDecalAnimStretcher::animate (decal_t *decal, float timeDelta) {
  guard(VDecalAnimStretcher::animate);
  if (decal->origScaleX <= 0 || decal->origScaleY <= 0) { decal->alpha = 0; return false; }
  if (decal->scaleX <= 0 || decal->scaleY <= 0) { decal->alpha = 0; return false; }
  timePassed += timeDelta;
  if (timePassed < startTime) return true; // not yet
  if (timePassed >= startTime+actionTime || actionTime <= 0) {
    if ((decal->scaleX = goalX) <= 0) { decal->alpha = 0; return false; }
    if ((decal->scaleY = goalY) <= 0) { decal->alpha = 0; return false; }
    return false;
  }
  float dtx = timePassed-startTime;
  {
    float aleft = goalX-decal->origScaleX;
    if ((decal->scaleX = decal->origScaleX+aleft*dtx/actionTime) <= 0) { decal->alpha = 0; return false; }
  }
  {
    float aleft = goalY-decal->origScaleY;
    if ((decal->scaleY = decal->origScaleY+aleft*dtx/actionTime) <= 0) { decal->alpha = 0; return false; }
  }
  return true;
  unguard;
}


bool VDecalAnimStretcher::parse (VScriptParser *sc) {
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


VDecalAnim *VDecalAnimSlider::clone () {
  VDecalAnimSlider *res = new VDecalAnimSlider();
  res->name = name;
  res->distX = distX;
  res->distY = distY;
  res->startTime = startTime;
  res->actionTime = actionTime;
  res->timePassed = timePassed;
  return res;
}


void VDecalAnimSlider::doIO (VStream &Strm) {
  guard(VDecalAnimSlider::doIO);
  Strm << timePassed;
  Strm << distX;
  Strm << distY;
  Strm << startTime;
  Strm << actionTime;
  unguard;
}


bool VDecalAnimSlider::animate (decal_t *decal, float timeDelta) {
  guard(VDecalAnimSlider::animate);
  timePassed += timeDelta;
  if (timePassed < startTime) return true; // not yet
  if (timePassed >= startTime+actionTime || actionTime <= 0) {
    decal->ofsX = distX;
    decal->ofsY = distY;
    return false;
  }
  float dtx = timePassed-startTime;
  decal->ofsX = distX*dtx/actionTime;
  decal->ofsY = distY*dtx/actionTime;
  return true;
  unguard;
}


bool VDecalAnimSlider::parse (VScriptParser *sc) {
  guard(VDecalAnimSlider::parse);

  sc->SetCMode(true);
  sc->ExpectString();
  if (sc->String.Length() == 0) { sc->Error("invalid decal fader name"); return false; }
  name = VName(*sc->String);
  sc->Expect("{");

  while (!sc->AtEnd()) {
    if (sc->Check("}")) return true;

    if (sc->Check("distx")) { sc->ExpectFloatWithSign(); distX = sc->Float; continue; }
    if (sc->Check("disty")) { sc->ExpectFloatWithSign(); distY = sc->Float; continue; }
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


VDecalAnim *VDecalAnimColorChanger::clone () {
  VDecalAnimColorChanger *res = new VDecalAnimColorChanger();
  res->name = name;
  res->dest[0] = dest[0];
  res->dest[1] = dest[1];
  res->dest[2] = dest[2];
  res->startTime = startTime;
  res->actionTime = actionTime;
  res->timePassed = timePassed;
  return res;
}


void VDecalAnimColorChanger::doIO (VStream &Strm) {
  guard(VDecalAnimColorChanger::doIO);
  Strm << timePassed;
  Strm << dest[0];
  Strm << dest[1];
  Strm << dest[2];
  Strm << startTime;
  Strm << actionTime;
  unguard;
}


bool VDecalAnimColorChanger::animate (decal_t *decal, float timeDelta) {
  guard(VDecalAnimColorChanger::animate);
  // not yet, sorry
  // as we are using pre-translated textures, color changer cannot work
  // and we need pre-translated textures for working colormaps
  return true;
  unguard;
}


bool VDecalAnimColorChanger::parse (VScriptParser *sc) {
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
      int destclr = 0;
      if (!parseHexRGB(sc->String, &destclr)) { sc->Error("invalid color"); return false; }
      dest[0] = ((destclr>>16)&0xff)/255.0f;
      dest[1] = ((destclr>>8)&0xff)/255.0f;
      dest[2] = (destclr&0xff)/255.0f;
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
  if (mIsCloned) {
    for (int f = 0; f < list.Num(); ++f) { delete list[f]; list[f] = nullptr; }
  }
}


void VDecalAnimCombiner::fixup () {
  for (int f = 0; f < nameList.Num(); ++f) {
    auto it = VDecalAnim::find(nameList[f]);
    if (it) list.Append(it); else GCon->Logf(NAME_Warning, "animgroup '%s' contains unknown anim '%s'!", *name, *nameList[f]);
  }
}


VDecalAnim *VDecalAnimCombiner::clone () {
  VDecalAnimCombiner *res = new VDecalAnimCombiner();
  res->name = name;
  res->mIsCloned = true;
  for (int f = 0; f < nameList.Num(); ++f) res->nameList.Append(nameList[f]);
  for (int f = 0; f < list.Num(); ++f) res->list.Append(list[f]->clone());
  res->timePassed = timePassed; // why not?
  return res;
}


void VDecalAnimCombiner::doIO (VStream &Strm) {
  guard(VDecalAnimCombiner::doIO);
  Strm << timePassed;
  int len = 0;
  if (Strm.IsLoading()) {
    Strm << len;
    if (len < 0 || len > 65535) Host_Error("Level load: invalid number of animations in animcombiner");
    list.SetNum(len);
    for (int f = 0; f < list.Num(); ++f) list[f] = nullptr;
  } else {
    len = list.Num();
    Strm << len;
  }
  for (int f = 0; f < list.Num(); ++f) VDecalAnim::Serialise(Strm, list[f]);
  unguard;
}


bool VDecalAnimCombiner::animate (decal_t *decal, float timeDelta) {
  guard(VDecalAnimCombiner::animate);
  bool res = false;
  int f = 0;
  while (f < list.length()) {
    if (list[f]->animate(decal, timeDelta)) {
      res = true;
      ++f;
    } else {
      delete list[f];
      list[f] = nullptr;
      list.removeAt(f);
    }
  }
  return res;
  unguard;
}


bool VDecalAnimCombiner::parse (VScriptParser *sc) {
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


void VDecalAnim::Serialise (VStream &Strm, VDecalAnim *&aptr) {
  guard(VDecalAnim::Serialise);
  vuint8 xver = 0; // current version is 0
  Strm << xver;
  // animator
  if (Strm.IsLoading()) {
    // load animator
    vuint8 type = 0;
    Strm << type;
    switch (type) {
      case 0: aptr = nullptr; return;
      case VDecalAnimFader::TypeId: aptr = new VDecalAnimFader(); break;
      case VDecalAnimStretcher::TypeId: aptr = new VDecalAnimStretcher(); break;
      case VDecalAnimSlider::TypeId: aptr = new VDecalAnimSlider(); break;
      case VDecalAnimColorChanger::TypeId: aptr = new VDecalAnimColorChanger(); break;
      case VDecalAnimCombiner::TypeId: aptr = new VDecalAnimCombiner(); break;
      default: Host_Error("Level load: unknown decal animator type");
    }
    aptr->doIO(Strm);
  } else {
    // save animator
    vuint8 type = 0;
    if (!aptr) { Strm << type; return; }
    type = aptr->getTypeId();
    Strm << type;
    aptr->doIO(Strm);
  }
  unguard;
}


// ////////////////////////////////////////////////////////////////////////// //
static void SetClassFieldName (VClass *Class, VName FieldName, VName Value) {
  guard(SetClassFieldName);
  VField *F = Class->FindFieldChecked(FieldName);
  F->SetName((VObject *)Class->Defaults, Value);
  unguard;
}


// ////////////////////////////////////////////////////////////////////////// //
void ParseDecalDef (VScriptParser *sc) {
  guard(ParseDecalDef);

  const unsigned int MaxStack = 64;
  VScriptParser *scstack[MaxStack];
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
          GCon->Logf(NAME_Init, "Including '%s'...", *sc->String);
          scstack[scsp++] = sc;
          sc = new VScriptParser(/**sc->String*/W_FullLumpName(lmp), W_CreateLumpReaderNum(lmp));
        } else {
          sc->Error(va("decal include '%s' not found", *sc->String));
          error = true;
          break;
        }
        continue;
      }

      //GCon->Logf("%s: \"%s\"", *sc->GetLoc().toStringNoCol(), *sc->String);
      if (sc->Check("generator")) {
        sc->ExpectString();
        VStr clsname = sc->String;
        sc->ExpectString();
        VStr decname = sc->String;
        // find class
        VClass *klass = VClass::FindClassLowerCase(*clsname.ToLower());
        if (klass) {
          if (developer && GArgs.CheckParm("-debug-decals")) GCon->Logf(NAME_Dev, "%s: class '%s': set decal '%s'", *sc->GetLoc().toStringNoCol(), klass->GetName(), *decname);
          SetClassFieldName(klass, VName("DecalName"), VName(*decname));
          VClass *k2 = klass->GetReplacee();
          if (k2 && k2 != klass) {
            if (developer) GCon->Logf(NAME_Dev, "  repclass '%s': set decal '%s'", k2->GetName(), *decname);
            SetClassFieldName(k2, VName("DecalName"), VName(*decname));
          }
        } else {
          GCon->Logf(NAME_Warning, "%s: ignored 'generator' definition for class '%s'", *sc->GetLoc().toStringNoCol(), *clsname);
        }
        continue;
      }

      if (sc->Check("decal")) {
        //sc->GetString();
        //GCon->Logf("%s:   DECAL \"%s\"", *sc->GetLoc().toStringNoCol(), *sc->String);
        //sc->UnGet();
        auto dc = new VDecalDef();
        if ((error = !dc->parse(sc)) == true) { delete dc; break; }
        sc->SetCMode(false);
        if (dc->texid > 0) VDecalDef::addToList(dc); else delete dc;
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
    //GCon->Logf(NAME_Init, "DONE WITH '%s'", *sc->GetLoc().GetSource());

    if (error) {
      while (scsp > 0) { delete sc; sc = scstack[--scsp]; }
      break;
    }
    if (scsp == 0) break;
    GCon->Logf(NAME_Init, "Finished included '%s'", *sc->GetLoc().GetSource());
    delete sc;
    sc = scstack[--scsp];
  }

  delete sc;

  unguard;
}


// ////////////////////////////////////////////////////////////////////////// //
void ProcessDecalDefs () {
  guard(ProcessDecalDefs);

  GCon->Logf(NAME_Init, "Parsing DECAL definitions");

  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    //fprintf(stderr, "<%s>\n", *W_LumpName(Lump));
    if (W_LumpName(Lump) == NAME_decaldef) {
      GCon->Logf(NAME_Init, "Parsing decal definition script '%s'...", *W_FullLumpName(Lump));
      ParseDecalDef(new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)));
    }
  }

  for (auto it = VDecalGroup::listHead; it; it = it->next) it->fixup();
  for (auto it = VDecalAnim::listHead; it; it = it->next) it->fixup();
  for (auto it = VDecalDef::listHead; it; it = it->next) it->fixup();

  optionalDecals.clear();
  optionalDecalGroups.clear();

  //!TLocation::ClearSourceFiles();
  unguard;
}
