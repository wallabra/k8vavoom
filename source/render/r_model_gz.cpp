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
    int mdindex; // model index in `models`
    int origmdindex; // this is used to find replacements on merge
    int frindex; // frame index in model (will be used to build frame map)
    VStr frname; // for MD2 named frames
    // in k8vavoom, this is set per-frame
    // set in `checkModelSanity()`
    TAVec angleOffset;
    float rotationSpeed; // !0: rotating
    int vvindex; // vavoom frame index in the given model (-1: invalid frame)

    VStr toString () const {
      VStr res = sprbase.toUpperCase();
      res += (char)(sprframe+'A');
      res += va(" mdi(%d) origmdi(%d) fri(%d) vvi(%d) ao(%g,%g,%g) rs(%g)",
        mdindex, origmdindex, frindex, vvindex,
        angleOffset.yaw, angleOffset.pitch, angleOffset.roll,
        rotationSpeed);
      return res;
    }
  };

  struct MdlFrameInfo {
    int mdlindex; // model index in `models`
    int mdlframe; // model frame number
    int vvframe; // k8vavoom frame number
    // as we can merge models, different frames can have different scale
    TVec scale;
    TVec offset;
    // temporary working data
    bool used;
  };

  // model and skin definition
  struct MSDef {
    VStr modelFile; // with path
    VStr skinFile; // with path
    // set in `checkModelSanity()`
    TArray<MdlFrameInfo> frameMap;
    // used in sanity checks
    bool reported;
  };

protected:
  void checkModelSanity (VScriptParser *sc);

  // -1: not found
  int findModelFrame (int mdlindex, int mdlframe, bool allowAppend=true);

public:
  VStr className;
  VStr path;
  TArray<MSDef> models;
  TVec scale;
  TVec offset;
  float rotationSpeed; // !0: rotating
  TAVec angleOffset;
  TArray<Frame> frames;
  // set in `checkModelSanity()`

public:
  GZModelDef ();
  virtual ~GZModelDef ();

  void clear ();

  inline bool isEmpty () const { return (models.length() == 0 || frames.length() == 0); }

  // "model" keyword already eaten
  void parse (VScriptParser *sc);

  // merge this model frames with another model frames
  // GZDoom seems to do this, so we have too
  void merge (GZModelDef &other);

  VStr createXml ();

  // override this function to allow "Frame" modeldef parsing
  // return `true` if model was succesfully found and parsed, or
  // false if model wasn't found or in invalid format
  // WARNING: don't clear `names` array!
  virtual bool ParseMD2Frames (VStr mdpath, TArray<VStr> &names);
};


//==========================================================================
//
//  GZModelDef::GZModelDef
//
//==========================================================================
GZModelDef::GZModelDef ()
  : className()
  , path()
  , models()
  , scale(0, 0, 0)
  , offset(0, 0, 0)
  , rotationSpeed(0)
  , angleOffset(0, 0, 0)
  , frames()
{
}


//==========================================================================
//
//  GZModelDef::~GZModelDef
//
//==========================================================================
GZModelDef::~GZModelDef () {
  clear();
}


//==========================================================================
//
//  GZModelDef::clear
//
//==========================================================================
void GZModelDef::clear () {
  className.clear();
  path.clear();
  models.clear();
  scale = TVec(0, 0, 0);
  offset = TVec(0, 0, 0);
  rotationSpeed = 0;
  angleOffset = TAVec(0, 0, 0);
  frames.clear();
}


//==========================================================================
//
//  GZModelDef::ParseMD2Frames
//
//==========================================================================
bool GZModelDef::ParseMD2Frames (VStr mdpath, TArray<VStr> &names) {
  return false;
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
      while (models.length() <= skidx) models.alloc();
      models[skidx].skinFile = sc->String.toLowerCase();
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
      VStr mname = sc->String.toLowerCase();
      VStr ext = mname.extractFileExtension();
      if (ext.isEmpty()) {
        sc->Message(va("gz alias model '%s' is in unknown format, defaulted to md3", *className));
        //mname += ".md3"; // no need to do it, we must preserve file name
      } else if (!ext.strEquCI(".md2") && !ext.strEquCI(".md3")) {
        sc->Message(va("gz alias model '%s' is in unknown format '%s', defaulted to md3", *className, *ext+1));
        //mname.clear(); // ok, allow it to load, the loader will take care of throwing it away
      }
      while (models.length() <= mdidx) models.alloc();
      models[mdidx].modelFile = mname;
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
      frm.mdindex = frm.origmdindex = sc->Number;
      // frame index
      sc->ExpectNumber();
      if (sc->Number < 0 || sc->Number > 1024) sc->Error(va("invalid model frame %d in model '%s'", sc->Number, *className));
      frm.frindex = sc->Number;
      // check if we already have equal frame, there is no need to keep duplicates
      bool replaced = false;
      for (auto &&ofr : frames) {
        if (frm.sprframe == ofr.sprframe &&
            frm.mdindex == ofr.mdindex &&
            frm.sprbase == ofr.sprbase)
        {
          // i found her!
          ofr.frindex = frm.frindex;
          replaced = true;
        }
      }
      // store it, if it wasn't a replacement
      if (!replaced) frames.append(frm);
      continue;
    }
    // "frame"
    if (sc->Check("frame")) {
      // Frame sprbase sprframe modelindex framename
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
      frm.mdindex = frm.origmdindex = sc->Number;
      // frame name
      sc->ExpectString();
      //if (sc->String.isEmpty()) sc->Error(va("empty model frame name model '%s'", *className));
      frm.frindex = -1;
      frm.frname = sc->String;
      // check if we already have equal frame, there is no need to keep duplicates
      bool replaced = false;
      for (auto &&ofr : frames) {
        if (frm.sprframe == ofr.sprframe &&
            frm.mdindex == ofr.mdindex &&
            frm.sprbase == ofr.sprbase)
        {
          // i found her!
          ofr.frindex = -1;
          ofr.frname = frm.frname;
          replaced = true;
        }
      }
      // store it, if it wasn't a replacement
      if (!replaced) frames.append(frm);
      continue;
    }
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
  if (fixZOffset && scale.z) { offset.z /= scale.z; offset.z += 4; } // `+4` is temprorary hack for QStuff Ultra

  checkModelSanity(sc);
}


//==========================================================================
//
//  GZModelDef::findModelFrame
//
//  -1: not found
//
//==========================================================================
int GZModelDef::findModelFrame (int mdlindex, int mdlframe, bool allowAppend) {
  if (mdlindex < 0 || mdlindex >= models.length() || models[mdlindex].modelFile.isEmpty()) return -1;
  //k8: dunno if i have to check it here
  VStr mn = models[mdlindex].modelFile.extractFileExtension().toLowerCase();
  if (!mn.isEmpty() && mn != ".md2" && mn != ".md3") return -1;
  for (auto &&it : models[mdlindex].frameMap.itemsIdx()) {
    const MdlFrameInfo &fi = it.value();
    check(fi.mdlindex == mdlindex);
    if (fi.mdlframe == mdlframe) return it.index();
  }
  if (!allowAppend) return -1;
  // append it
  MdlFrameInfo &fi = models[mdlindex].frameMap.alloc();
  fi.mdlindex = mdlindex;
  fi.mdlframe = mdlframe;
  fi.vvframe = models[mdlindex].frameMap.length()-1;
  fi.scale = scale;
  fi.offset = offset;
  fi.used = true; // why not?
  return fi.vvframe;
}


//==========================================================================
//
//  GZModelDef::checkModelSanity
//
//==========================================================================
void GZModelDef::checkModelSanity (VScriptParser *sc) {
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

  // build frame map
  bool hasValidFrames = false;
  bool hasInvalidFrames = false;

  // clear existing frame maps, just in case
  for (auto &&mdl : models) {
    mdl.frameMap.clear();
    mdl.reported = false;
  }


  for (auto &&it : frames.itemsIdx()) {
    Frame &frm = it.value();
    // check for MD2 named frames
    if (frm.frindex == -1) {
      int mdlindex = frm.mdindex;
      if (mdlindex < 0 || mdlindex >= models.length() || models[mdlindex].modelFile.isEmpty() || models[mdlindex].reported) {
        frm.vvindex = -1;
      } else {
        TArray<VStr> frlist;
        VStr mfn = path+models[mdlindex].modelFile;
        if (!ParseMD2Frames(mfn, frlist)) {
          GLog.WriteLine(NAME_Warning, "alias model '%s' not found for class '%s'", *mfn, *className);
          frm.vvindex = -1;
          models[mdlindex].reported = true;
        } else {
          frm.vvindex = -1;
          for (auto &&nit : frlist.itemsIdx()) {
            if (nit.value().strEquCI(frm.frname)) {
              frm.vvindex = nit.index();
              break;
            }
          }
        }
      }
      if (frm.vvindex >= 0) {
        frm.vvindex = findModelFrame(frm.mdindex, frm.vvindex, true); // allow appending
      }
    } else {
      frm.vvindex = findModelFrame(frm.mdindex, frm.frindex, true); // allow appending
    }

    if (frm.vvindex < 0) {
      if (frm.frindex >= 0) {
        GLog.WriteLine(NAME_Warning, "alias model '%s' has invalid model index (%d) in frame %d", *className, frm.mdindex, it.index());
      } else {
        GLog.WriteLine(NAME_Warning, "alias model '%s' has invalid model frame '%s' in frame %d", *className, *frm.frname, it.index());
      }
      hasInvalidFrames = true;
      continue;
    }
    hasValidFrames = true;
    frm.angleOffset = angleOffset; // copy it here
    frm.rotationSpeed = rotationSpeed;
  }

  // is it empty?
  if (!hasValidFrames && !hasInvalidFrames) { clear(); return; }

  if (!hasValidFrames) {
    GLog.WriteLine(NAME_Warning, "gz alias model '%s' nas no valid frames!", *className);
    clear();
    return;
  }

  // remove invalid frames
  if (hasInvalidFrames) {
    int fidx = 0;
    while (fidx < frames.length()) {
      if (frames[fidx].vvindex < 0) {
        //GLog.WriteLine(NAME_Warning, "alias model '%s': removing frame #%d: %s", *className, fidx, *frames[fidx].toString());
        frames.removeAt(fidx);
      } else {
        ++fidx;
      }
    }
    check(frames.length() > 0); // invariant
  }

  // prepend path to skins and models (and clear unused model names)
  for (auto &&mdl : models) {
    if (mdl.frameMap.length() == 0) {
      mdl.modelFile.clear();
      mdl.skinFile.clear();
    } else {
      mdl.modelFile = path+mdl.modelFile;
      if (!mdl.skinFile.isEmpty()) mdl.skinFile = path+mdl.skinFile;
    }
  }
}


//==========================================================================
//
//  GZModelDef::merge
//
//  merge this model frames with another model frames
//  GZDoom seems to do this, so we have too
//
//==========================================================================
void GZModelDef::merge (GZModelDef &other) {
  if (&other == this) return; // nothing to do
  if (other.isEmpty()) return; // nothing to do

  // this is brute-force approach, which can be made faster, but meh...
  // just go through other model def frames, and try to find the correspondence
  // in this model, or append new data.
  bool compactFrameMaps = false;
  for (auto &&ofrm : other.frames) {
    if (ofrm.vvindex < 0) continue; // this frame is invalid
    // ok, we have a frame, try to find a model for it
    const VStr &omdf = other.models[ofrm.mdindex].modelFile;
    const VStr &osdf = other.models[ofrm.mdindex].skinFile;
    int mdlEmpty = -1; // first empty model in model array, to avoid looping twice
    int mdlindex = -1;
    for (auto &&mdlit : models.itemsIdx()) {
      if (omdf.strEquCI(mdlit.value().modelFile) && osdf.strEquCI(mdlit.value().skinFile)) {
        mdlindex = mdlit.index();
        break;
      }
      if (mdlEmpty < 0 && mdlit.value().frameMap.length() == 0) mdlEmpty = mdlit.index();
    }
    // if we didn't found a suitable model, append a new one
    bool newModel = false;
    if (mdlindex < 0) {
      newModel = true;
      if (mdlEmpty < 0) {
        mdlEmpty = models.length();
        models.alloc();
      }
      mdlindex = mdlEmpty;
      MSDef &nmdl = models[mdlindex];
      nmdl.modelFile = omdf;
      nmdl.skinFile = osdf;
    }
    // try to find a model frame to reuse
    MSDef &rmdl = models[mdlindex];
    const MdlFrameInfo &omfrm = other.models[ofrm.mdindex].frameMap[ofrm.vvindex];
    int frmapindex = -1;
    if (!newModel) {
      for (auto &&mfrm : rmdl.frameMap) {
        if (mfrm.mdlframe == mdlindex &&
            mfrm.scale == omfrm.scale &&
            mfrm.offset == omfrm.offset)
        {
          // yay, i found her!
          // reuse this frame
          frmapindex = mfrm.vvframe;
          break;
        }
      }
    }
    if (frmapindex < 0) {
      // ok, we have no suitable model frame, append a new one
      frmapindex = rmdl.frameMap.length();
      MdlFrameInfo &nfi = rmdl.frameMap.alloc();
      nfi.mdlindex = mdlindex;
      nfi.mdlframe = ofrm.frindex;
      nfi.vvframe = rmdl.frameMap.length()-1;
      nfi.scale = omfrm.scale;
      nfi.offset = omfrm.offset;
    }
    // find sprite frame to replace
    // HACK: same model indicies will be replaced; this is how GZDoom does it
    int spfindex = -1;
    for (auto &&sit : frames.itemsIdx()) {
      Frame &ff = sit.value();
      if (ff.sprframe == ofrm.sprframe &&
          /*ff.origmdindex == ofrm.origmdindex &&*/
          ff.sprbase == ofrm.sprbase)
      {
        if (ff.origmdindex != ofrm.origmdindex) {
          GLog.WriteLine(NAME_Warning, "class '%s' (%s%c) has several attached alias models; this is not supported in k8vavoom!",
            *className, *ff.sprbase.toUpperCase(), 'A'+ff.sprframe);
        }
        spfindex = sit.index();
        break;
      }
    }
    if (spfindex < 0) {
      // append new sprite frame
      spfindex = frames.length();
      (void)frames.alloc();
    } else {
      if (!compactFrameMaps && frames[spfindex].vvindex != frmapindex) compactFrameMaps = true;
    }
    // replace sprite frame
    Frame &newfrm = frames[spfindex];
    newfrm.sprbase = ofrm.sprbase;
    newfrm.sprframe = ofrm.sprframe;
    newfrm.mdindex = mdlindex;
    newfrm.origmdindex = ofrm.origmdindex;
    newfrm.frindex = ofrm.frindex;
    newfrm.angleOffset = ofrm.angleOffset;
    newfrm.rotationSpeed = ofrm.rotationSpeed;
    newfrm.vvindex = frmapindex;
  }

  // remove unused model frames
  if (compactFrameMaps) {
    // actually, simply rebuild frame maps for each model
    // i may rewrite it in the future, but for now it is ok
    int unusedFramesCount = 0;
    for (auto &&mdl : models) {
      for (auto &&frm : mdl.frameMap) {
        frm.used = false;
        ++unusedFramesCount;
      }
    }
    // mark all used frames
    for (auto &&frm : frames) {
      if (frm.vvindex < 0) continue; // just in case
      if (!models[frm.mdindex].frameMap[frm.vvindex].used) {
        models[frm.mdindex].frameMap[frm.vvindex].used = true;
        --unusedFramesCount;
      }
    }
    check(unusedFramesCount >= 0);
    if (unusedFramesCount == 0) return; // nothing to do

    // rebuild frame maps
    for (auto &&mit : models.itemsIdx()) {
      MSDef &mdl = mit.value();
      TArray<MdlFrameInfo> newmap;
      TArray<int> newvvindex;
      newvvindex.setLength(mdl.frameMap.length());
      for (auto &&frm : mdl.frameMap) {
        if (!frm.used) continue;
        newvvindex[frm.vvframe] = newmap.length();
        newmap.append(frm);
      }
      if (newmap.length() == mdl.frameMap.length()) continue; // nothing to do
      //FIXME: make this faster!
      for (auto &&frm : frames) {
        if (frm.vvindex < 0) continue; // just in case
        if (frm.mdindex != mit.index()) continue;
        check(newvvindex[frm.vvindex] >= 0);
        frm.vvindex = newvvindex[frm.vvindex];
      }
      mdl.frameMap = newmap;
    }
  }
}


//==========================================================================
//
//  appendScale
//
//==========================================================================
static void appendScale (VStr &res, const TVec &scale, const TVec *baseScale) {
  if (baseScale) {
    if (*baseScale == scale) return; // base scale is set
    if (baseScale->x != scale.x && baseScale->y != scale.y && baseScale->z != scale.z) {
      if (scale.x == scale.y && scale.y == scale.z) {
        if (scale.x != 1) res += va(" scale=\"%g\"", scale.x);
      } else {
        if (scale.x != 1) res += va(" scale_x=\"%g\"", scale.x);
        if (scale.y != 1) res += va(" scale_y=\"%g\"", scale.y);
        if (scale.z != 1) res += va(" scale_z=\"%g\"", scale.z);
      }
    } else {
      if (baseScale->x != scale.x) res += va(" scale_x=\"%g\"", scale.x);
      if (baseScale->y != scale.y) res += va(" scale_y=\"%g\"", scale.y);
      if (baseScale->z != scale.z) res += va(" scale_z=\"%g\"", scale.z);
    }
  } else {
    if (scale.x == scale.y && scale.y == scale.z) {
      if (scale.x != 1) res += va(" scale=\"%g\"", scale.x);
    } else {
      if (scale.x != 1) res += va(" scale_x=\"%g\"", scale.x);
      if (scale.y != 1) res += va(" scale_y=\"%g\"", scale.y);
      if (scale.z != 1) res += va(" scale_z=\"%g\"", scale.z);
    }
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
    const MSDef &mdl = it.value();
    if (mdl.frameMap.length() == 0) continue; // this model is unused
    check(!mdl.modelFile.isEmpty());
    const char *mdtag = (mdl.modelFile.extractFileExtension().strEquCI(".md2") ? "md2" : "md3");
    res += va("  <model name=\"%s_%d\">\n", *className.toLowerCase().xmlEscape(), it.index());
    res += va("    <%s file=\"%s\" noshadow=\"false\"", mdtag, *mdl.modelFile.xmlEscape());
    appendScale(res, scale, nullptr);
    if (offset.x != 0) res += va(" offset_x=\"%g\"", offset.x);
    if (offset.y != 0) res += va(" offset_y=\"%g\"", offset.y);
    if (offset.z != 0) res += va(" offset_z=\"%g\"", offset.z);
    res += ">\n";
    if (!mdl.skinFile.isEmpty()) {
      res += va("      <skin file=\"%s\" />\n", *mdl.skinFile.xmlEscape());
    }
    // write frame list
    for (auto &&fit : mdl.frameMap.itemsIdx()) {
      const MdlFrameInfo &fi = fit.value();
      check(it.index() == fi.mdlindex);
      check(fit.index() == fi.vvframe);
      res += va("      <frame index=\"%d\"", fi.mdlframe);
      appendScale(res, fi.scale, &scale);
      if (fi.offset.x != offset.x) res += va(" offset_x=\"%g\"", fi.offset.x);
      if (fi.offset.y != offset.y) res += va(" offset_y=\"%g\"", fi.offset.y);
      if (fi.offset.z != offset.z) res += va(" offset_z=\"%g\"", fi.offset.z);
      res += " />\n";
    }
    res += va("    </%s>\n", mdtag);
    res += "  </model>\n";
  }

  // write class definition
  res += va("  <class name=\"%s\" noselfshadow=\"true\">\n", *className.xmlEscape());
  for (auto &&frm : frames) {
    if (frm.vvindex < 0) continue;
    res += va("    <state sprite=\"%s\" sprite_frame=\"%s\" model=\"%s_%d\" frame_index=\"%d\"",
      *frm.sprbase.toUpperCase().xmlEscape(),
      *VStr((char)(frm.sprframe+'A')).xmlEscape(),
      *className.toLowerCase().xmlEscape(), frm.mdindex,
      frm.vvindex);
    if (frm.rotationSpeed) res += " rotation=\"true\"";
    if (frm.angleOffset.yaw) res += va("  rotate_yaw=\"%g\"", frm.angleOffset.yaw);
    if (frm.angleOffset.pitch) res += va("  rotate_pitch=\"%g\"", frm.angleOffset.pitch);
    if (frm.angleOffset.roll) res += va("  rotate_roll=\"%g\"", frm.angleOffset.roll);
    res += " />\n";
  }
  res += "  </class>\n";
  res += "</vavoom_model_definition>\n";
  return res;
}
