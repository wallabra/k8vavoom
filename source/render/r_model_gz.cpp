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
// included from "r_model.cpp"

// ////////////////////////////////////////////////////////////////////////// //
class GZModelDef {
public:
  struct Frame {
    VStr sprbase;
    int sprframe;
    int mdindex; // model index
    int frindex; // frame index
  };

protected:
  void checkModelSanity (VScriptParser *sc);

public:
  VStr className;
  VStr path;
  TArray<VStr> skins;
  TArray<VStr> models;
  TVec scale = TVec(0, 0, 0);
  TVec offset = TVec(0, 0, 0);
  float rotationSpeed = 0; // !0: rotating
  TAVec angleOffset = TAVec(0, 0, 0);
  TArray<Frame> frames;
  // set in `checkModelSanity()`
  TArray<int> maxModelFrames; // frames used for each model (0: model is unused)

  void clear ();

  inline bool isEmpty () const { return (models.length() == 0 || frames.length() == 0); }

  // "model" keyword already eaten
  void parse (VScriptParser *sc);

  VStr createXml ();
};


//==========================================================================
//
//  GZModelDef::clear
//
//==========================================================================
void GZModelDef::clear () {
  className.clear();
  path.clear();
  skins.clear();
  models.clear();
  scale = TVec(0, 0, 0);
  offset = TVec(0, 0, 0);
  rotationSpeed = 0;
  angleOffset = TAVec(0, 0, 0);
  frames.clear();
  maxModelFrames.clear();
}


//==========================================================================
//
//  GZModelDef::parse
//
//==========================================================================
void GZModelDef::parse (VScriptParser *sc) {
  clear();
  // get class name
  sc->ExpectString();
  className = sc->String;
  sc->Expect("{");
  bool rotating = false;
  bool fixZOffset = false;
  while (!sc->Check("}")) {
    // skip flags
    if (sc->Check("PITCHFROMMOMENTUM") ||
        sc->Check("IGNORETRANSLATION") ||
        sc->Check("INHERITACTORPITCH") ||
        sc->Check("INHERITACTORROLL") ||
        sc->Check("INTERPOLATEDOUBLEDFRAMES") ||
        sc->Check("NOINTERPOLATION") ||
        sc->Check("USEACTORPITCH") ||
        sc->Check("USEACTORROLL") ||
        sc->Check("DONTCULLBACKFACES") ||
        sc->Check("USEROTATIONCENTER"))
    {
      sc->Message(va("modeldef flag '%s' is not supported yet in model '%s'", *sc->String, *className));
      continue;
    }
    // "rotating"
    if (sc->Check("rotating")) {
      //if (rotationSpeed == 0) rotationSpeed = 8; // arbitrary value, ignored for now
      rotating = true;
      continue;
    }
    // "rotation-speed"
    if (sc->Check("rotation-speed")) {
      sc->ExpectFloatWithSign();
      rotationSpeed = sc->Float;
      continue;
    }
    // "rotation-center"
    if (sc->Check("rotation-center") || sc->Check("rotation-vector")) {
      sc->Message(va("modeldef command '%s' is not supported yet in model '%s'", *sc->String, *className));
      sc->ExpectFloatWithSign();
      sc->ExpectFloatWithSign();
      sc->ExpectFloatWithSign();
      continue;
    }
    // "path"
    if (sc->Check("path")) {
      sc->ExpectString();
      path = sc->String;
      continue;
    }
    // "skin"
    if (sc->Check("skin")) {
      sc->ExpectNumber();
      int skidx = sc->Number;
      if (skidx < 0 || skidx > 1024) sc->Error(va("invalid skin number (%d) in model '%s'", skidx, *className));
      sc->ExpectString();
      while (skins.length() <= skidx) skins.alloc();
      skins[skidx] = sc->String.toLowerCase();
      continue;
    }
    // "SurfaceSkin"
    if (sc->Check("SurfaceSkin")) {
      // SurfaceSkin model-index surface-index skin-file
      sc->Message(va("modeldef command '%s' is not supported yet in model '%s'", *sc->String, *className));
      sc->ExpectNumber();
      sc->ExpectNumber();
      sc->ExpectString();
      continue;
    }
    // "model"
    if (sc->Check("model")) {
      sc->ExpectNumber();
      int mdidx = sc->Number;
      if (mdidx < 0 || mdidx > 1024) sc->Error(va("invalid model number (%d) in model '%s'", mdidx, *className));
      sc->ExpectString();
      while (models.length() <= mdidx) models.alloc();
      models[mdidx] = sc->String.toLowerCase();
      continue;
    }
    // "scale"
    if (sc->Check("scale")) {
      // x
      sc->ExpectFloatWithSign();
      if (sc->Float == 0) sc->Error(va("invalid x scale in model '%s'", *className));
      scale.x = sc->Float;
      // y
      sc->ExpectFloatWithSign();
      if (sc->Float == 0) sc->Error(va("invalid y scale in model '%s'", *className));
      scale.y = sc->Float;
      // z
      sc->ExpectFloatWithSign();
      if (sc->Float == 0) sc->Error(va("invalid z scale in model '%s'", *className));
      scale.z = sc->Float;
      continue;
    }
    // "frameindex"
    if (sc->Check("frameindex")) {
      // FrameIndex sprbase sprframe modelindex frameindex
      Frame frm;
      // sprite name
      sc->ExpectString();
      frm.sprbase = sc->String.toLowerCase();
      if (frm.sprbase.length() != 4) sc->Error(va("invalid sprite name '%s' in model '%s'", *frm.sprbase, *className));
      // sprite frame
      sc->ExpectString();
      if (sc->String.length() != 1) sc->Error(va("invalid sprite frame '%s' in model '%s'", *sc->String, *className));
      char fc = sc->String[0];
      if (fc >= 'a' && fc <= 'z') fc = fc-'a'+'A';
      frm.sprframe = fc-'A';
      if (frm.sprframe < 0 || frm.sprframe > 31) sc->Error(va("invalid sprite frame '%s' in model '%s'", *sc->String, *className));
      // model index
      sc->ExpectNumber();
      if (sc->Number < 0 || sc->Number > 1024) sc->Error(va("invalid model index %d in model '%s'", sc->Number, *className));
      frm.mdindex = sc->Number;
      // frame index
      sc->ExpectNumber();
      if (sc->Number < 0 || sc->Number > 1024) sc->Error(va("invalid model frame %d in model '%s'", sc->Number, *className));
      frm.frindex = sc->Number;
      // store it
      frames.append(frm);
      continue;
    }
    // "frame"
    if (sc->Check("frame")) sc->Error(va("'frame' declaration is not supported in model '%s'", *className));
    // "AngleOffset"
    if (sc->Check("AngleOffset")) {
      sc->ExpectFloatWithSign();
      angleOffset.yaw = AngleMod(sc->Float);
      continue;
    }
    // "PitchOffset"
    if (sc->Check("PitchOffset")) {
      sc->ExpectFloatWithSign();
      angleOffset.pitch = AngleMod(sc->Float);
      continue;
    }
    // "RollOffset"
    if (sc->Check("RollOffset")) {
      sc->ExpectFloatWithSign();
      angleOffset.roll = AngleMod(sc->Float);
      continue;
    }
    // "Offset"
    if (sc->Check("Offset")) {
      sc->ExpectFloatWithSign();
      offset.x = sc->Float;
      sc->ExpectFloatWithSign();
      offset.y = sc->Float;
      sc->ExpectFloatWithSign();
      offset.z = sc->Float;
      fixZOffset = false;
      continue;
    }
    // "ZOffset"
    if (sc->Check("ZOffset")) {
      sc->ExpectFloatWithSign();
      offset.z = sc->Float;
      fixZOffset = true;
      continue;
    }
    // unknown shit, try to ignore it
    if (!sc->GetString()) sc->Error(va("unexpected EOF in model '%s'", *className));
    sc->Message(va("unknown MODELDEF command '%s' in model '%s'", *sc->String, *className));
    for (;;) {
      if (!sc->GetString()) sc->Error(va("unexpected EOF in model '%s'", *className));
      if (sc->String.strEqu("{")) sc->Error(va("unexpected '{' in model '%s'", *className));
      if (sc->String.strEqu("}")) { sc->UnGet(); break; }
      if (sc->Crossed) { sc->UnGet(); break; }
    }
  }
  if (rotating && rotationSpeed == 0) rotationSpeed = 8; // arbitrary value
  if (!rotating) rotationSpeed = 0; // reset rotation flag
  if (fixZOffset && scale.z) offset.z /= scale.z;
  checkModelSanity(sc);
}


//==========================================================================
//
//  GZModelDef::checkModelSanity
//
//==========================================================================
void GZModelDef::checkModelSanity (VScriptParser *sc) {
  if (isEmpty()) return; // nothing to do
  maxModelFrames.setLength(models.length());
  static_assert(sizeof(maxModelFrames[0]) == sizeof(int), "!!!");
  memset(maxModelFrames.ptr(), 0, sizeof(maxModelFrames[0]));
  //GLog.WriteLine("=== MODEL '%s' ===", *className);
  for (auto &&it : frames.itemsIdx()) {
    //GLog.WriteLine("  frame #%d: %s %d %d %d", it.index(), *it.value().sprbase, it.value().sprframe, it.value().mdindex, it.value().frindex);
    const Frame &frm = it.value();
    if (frm.mdindex >= models.length()) sc->Error(va("model '%s' has invalid model index (%d) in frame %d", *className, frm.mdindex, it.index()));
    VStr mn = models[frm.mdindex];
    if (mn.isEmpty()) sc->Error(va("model '%s' has empty model with index (%d) in frame %d", *className, frm.mdindex, it.index()));
    if (!mn.extractFileExtension().strEquCI(".md2") && !mn.extractFileExtension().strEquCI(".md3")) {
      sc->Error(va("model '%s' has unknown model format (%s) with index (%d) in frame %d", *className, *mn, frm.mdindex, it.index()));
    }
    maxModelFrames[frm.mdindex] = max2(maxModelFrames[frm.mdindex], frm.frindex+1);
  }
  while (skins.length() < models.length()) skins.alloc();
  // normalize path
  if (path.length()) {
    TArray<VStr> parr;
    path.SplitPath(parr);
    if (parr.length() && parr[0] == "/") parr.removeAt(0);
    int pidx = 0;
    while (pidx < parr.length()) {
      if (parr[pidx] == ".") {
        parr.removeAt(pidx);
        continue;
      }
      if (parr[pidx] == "..") {
        parr.removeAt(pidx);
        if (pidx > 0) {
          --pidx;
          parr.removeAt(pidx);
        }
        continue;
      }
      bool allDots = true;
      for (const char *s = parr[pidx].getCStr(); *s; ++s) if (*s != '.') { allDots = false; break; }
      if (allDots) sc->Error(va("invalid model path '%s' in model '%s'", *parr[pidx], *className));
      ++pidx;
    }
    path.clear();
    for (auto &&s : parr) {
      path += s.toLowerCase();
      path += '/';
    }
    //GLog.WriteLine("<%s>", *path);
  }
}


//==========================================================================
//
//  GZModelDef::createXml
//
//==========================================================================
VStr GZModelDef::createXml () {
  VStr res = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n";
  res += "<!-- ";
  res += className;
  res += " -->\n";
  res += "<vavoom_model_definition>\n";
  // write models
  for (auto &&it : models.itemsIdx()) {
    int maxfrm = maxModelFrames[it.index()];
    if (!maxfrm) continue;
    res += va("  <model name=\"%s_%d\">\n", *className.toLowerCase().xmlEscape(), it.index());
    res += va("    <%s file=\"%s%s\" noshadow=\"false\"", *it.value().extractFileExtension()+1, *path.xmlEscape(), *it.value().xmlEscape());
    if (scale.x == scale.y && scale.y == scale.z) {
      if (scale.x != 1) res += va(" scale=\"%g\"", scale.x);
    } else {
      if (scale.x != 1) res += va(" scale_x=\"%g\"", scale.x);
      if (scale.y != 1) res += va(" scale_y=\"%g\"", scale.y);
      if (scale.z != 1) res += va(" scale_z=\"%g\"", scale.z);
    }
    if (offset.x != 0) res += va(" offset_x=\"%g\"", offset.x);
    if (offset.y != 0) res += va(" offset_y=\"%g\"", offset.y);
    if (offset.z != 0) res += va(" offset_z=\"%g\"", offset.z);
    res += ">\n";
    if (!skins[it.index()].isEmpty()) {
      res += va("      <skin file=\"%s%s\" />\n", *path.xmlEscape(), *skins[it.index()].xmlEscape());
    }
    // write frame list
    for (int fn = 0; fn < maxModelFrames[it.index()]; ++fn) {
      res += va("      <frame index=\"%d\" />\n", fn);
    }
    res += va("    </%s>\n", *it.value().extractFileExtension()+1);
    res += "  </model>\n";
  }
  // write class definition
  res += va("  <class name=\"%s\" noselfshadow=\"true\">\n", *className.xmlEscape());
  for (auto &&frm : frames) {
    res += va("    <state sprite=\"%s\" sprite_frame=\"%s\" model=\"%s_%d\" frame_index=\"%d\"",
      *frm.sprbase.toUpperCase().xmlEscape(),
      *VStr((char)(frm.sprframe+'A')).xmlEscape(),
      *className.toLowerCase().xmlEscape(), frm.mdindex,
      frm.frindex);
    if (rotationSpeed) res += " rotation=\"true\"";
    if (angleOffset.yaw) res += va("  rotate_yaw=\"%g\"", angleOffset.yaw);
    if (angleOffset.pitch) res += va("  rotate_pitch=\"%g\"", angleOffset.pitch);
    if (angleOffset.roll) res += va("  rotate_roll=\"%g\"", angleOffset.roll);
    res += " />\n";
  }
  res += "  </class>\n";
  res += "</vavoom_model_definition>\n";
  return res;
}
