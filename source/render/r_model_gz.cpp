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
// included from "r_model.cpp"

// ////////////////////////////////////////////////////////////////////////// //
class GZModelDef {
public:
  struct Frame {
    VStr sprbase; // always lowercased
    int sprframe; // sprite frame index (i.e. 'A' is 0)
    int mdindex; // model index in `models`
    int origmdindex; // this is used to find replacements on merge
    int origmdlframe; // this is used to find replacements on merge
    int frindex; // frame index in model (will be used to build frame map)
    VStr frname; // for MD2 named frames
    // in k8vavoom, this is set per-frame
    // set in `checkModelSanity()`
    TAVec angleOffset;
    float rotationSpeed; // !0: rotating
    bool usePitch;
    bool usePitchInverted;
    bool usePitchMomentum;
    bool useRoll;
    int vvindex; // vavoom frame index in the given model (-1: invalid frame)
    // used only in sanity check method
    int linkSprBase; // <0: end of list

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
    float zoffset;
    // temporary working data
    bool used;

    VStr toString () const {
      return VStr(va("mdl(%d); frm(%d); vvfrm(%d); scale=(%g,%g,%g); ofs=(%g,%g,%g); zofs=%g", mdlindex, mdlframe, vvframe, scale.x, scale.y, scale.z, offset.x, offset.y, offset.z, zoffset));
    }
  };

  // model and skin definition
  struct MSDef {
    VStr modelFile; // with path
    VStr skinFile; // with path
    // set in `checkModelSanity()`
    TArray<MdlFrameInfo> frameMap;
    // used in sanity checks
    bool reported;
    TArray<VStr> frlist; // frame names for MD2 model
    bool frlistLoaded;
  };

protected:
  void checkModelSanity ();

  // -1: not found
  int findModelFrame (int mdlindex, int mdlframe, bool allowAppend=true);

  VStr buildPath (VScriptParser *sc, VStr path);

public:
  VStr className;
  TArray<MSDef> models;
  TVec scale;
  TVec offset;
  float zoffset;
  float rotationSpeed; // !0: rotating
  bool usePitch;
  bool usePitchInverted;
  bool usePitchMomentum;
  bool useRoll;
  TAVec angleOffset;
  TArray<Frame> frames;

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

  virtual bool IsModelFileExists (VStr mdpath);
};


//==========================================================================
//
//  GZModelDef::GZModelDef
//
//==========================================================================
GZModelDef::GZModelDef ()
  : className()
  , models()
  , scale(1, 1, 1)
  , offset(0, 0, 0)
  , zoffset(0)
  , rotationSpeed(0)
  , usePitch(false)
  , usePitchInverted(false)
  , usePitchMomentum(false)
  , useRoll(false)
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
  models.clear();
  scale = TVec(1, 1, 1);
  offset = TVec(0, 0, 0);
  zoffset = 0;
  rotationSpeed = 0;
  usePitch = usePitchInverted = usePitchMomentum = useRoll = false;
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
//  GZModelDef::IsModelFileExists
//
//==========================================================================
bool GZModelDef::IsModelFileExists (VStr mdpath) {
  return true;
}


//==========================================================================
//
//  GZModelDef::buildPath
//
//==========================================================================
VStr GZModelDef::buildPath (VScriptParser *sc, VStr path) {
  // normalize path
  if (path.length()) {
    TArray<VStr> parr;
    path.fixSlashes().SplitPath(parr);
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
  return path;
}


//==========================================================================
//
//  sanitiseScale
//
//==========================================================================
static TVec sanitiseScale (const TVec &scale) {
  TVec res = scale;
  if (!isFiniteF(res.x) || !res.x) res.x = 1.0f;
  if (!isFiniteF(res.y) || !res.y) res.y = 1.0f;
  if (!isFiniteF(res.z) || !res.z) res.z = 1.0f;
  // fix negative scales (i don't know what gozzo does with them, but we'll convert them to 1/scale)
  // usually, scale "-1" is for HUD weapons. wtf?!
  /*
  if (res.x < 0 && res.x != -1.0f) GCon->Logf("!!! scalex=%g", res.x);
  if (res.y < 0 && res.y != -1.0f) GCon->Logf("!!! scaley=%g", res.y);
  if (res.z < 0 && res.z != -1.0f) GCon->Logf("!!! scalez=%g", res.z);
  */
  if (res.x < 0) res.x = 1.0f/(-res.x);
  if (res.y < 0) res.y = 1.0f/(-res.y);
  if (res.z < 0) res.z = 1.0f/(-res.z);
  return res;
}


//==========================================================================
//
//  GZModelDef::parse
//
//==========================================================================
void GZModelDef::parse (VScriptParser *sc) {
  clear();
  VStr path;
  // get class name
  sc->ExpectString();
  className = sc->String;
  sc->Expect("{");
  bool rotating = false;
  bool wasZOffset = false; // temp hack for QStuffUltra
  while (!sc->Check("}")) {
    // skip flags
    if (sc->Check("IGNORETRANSLATION") ||
        sc->Check("INTERPOLATEDOUBLEDFRAMES") ||
        sc->Check("NOINTERPOLATION") ||
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
      path = buildPath(sc, sc->String);
      continue;
    }
    // "skin"
    if (sc->Check("skin")) {
      sc->ExpectNumber();
      int skidx = sc->Number;
      if (skidx < 0 || skidx > 1024) sc->Error(va("invalid skin number (%d) in model '%s'", skidx, *className));
      sc->ExpectString();
      while (models.length() <= skidx) models.alloc();
      VStr xpath = (!sc->String.isEmpty() ? path+sc->String.toLowerCase() : VStr::EmptyString);
      models[skidx].skinFile = xpath;
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
      if (!mname.isEmpty()) {
        VStr ext = mname.extractFileExtension();
        if (ext.isEmpty()) {
          sc->Message(va("gz alias model '%s' is in unknown format, defaulted to md3", *className));
          //mname += ".md3"; // no need to do it, we must preserve file name
        } else if (!ext.strEquCI(".md2") && !ext.strEquCI(".md3")) {
          sc->Message(va("gz alias model '%s' is in unknown format '%s', defaulted to md3", *className, *ext+1));
          //mname.clear(); // ok, allow it to load, the loader will take care of throwing it away
        }
        mname = path+mname;
      }
      while (models.length() <= mdidx) models.alloc();
      models[mdidx].modelFile = mname.fixSlashes();
      continue;
    }
    // "scale"
    if (sc->Check("scale")) {
      // x
      sc->ExpectFloatWithSign();
      if (sc->Float == 0) sc->Message(va("invalid x scale in model '%s'", *className));
      scale.x = sc->Float;
      // y
      sc->ExpectFloatWithSign();
      if (sc->Float == 0) sc->Message(va("invalid y scale in model '%s'", *className));
      scale.y = sc->Float;
      // z
      sc->ExpectFloatWithSign();
      if (sc->Float == 0) sc->Message(va("invalid z scale in model '%s'", *className));
      scale.z = sc->Float;
      // normalize
      scale = sanitiseScale(scale);
      // seems that scale scales previous offsets
      offset.x /= scale.x;
      offset.y /= scale.y;
      offset.z /= scale.z;
      zoffset /= scale.z;
      // qstuffultra hack
      if (wasZOffset && zoffset > 8) { offset.z = zoffset+scale.z; zoffset = 0; }
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
      if (sc->String.length() == 0) sc->Error(va("empty sprite frame in model '%s'", *className));
      if (sc->String.length() != 1) {
        // gozzo wiki says that there can be only one frame, so fuck it
        sc->Message(va("invalid sprite frame '%s' in model '%s'; FIX YOUR BROKEN CODE!", *sc->String, *className));
      }
      char fc = sc->String[0];
      if (fc >= 'a' && fc <= 'z') fc = fc-'a'+'A';
      frm.sprframe = fc-'A';
      if (frm.sprframe < 0 || frm.sprframe > 31) sc->Error(va("invalid sprite frame '%s' in model '%s'", *sc->String, *className));
      // model index
      sc->ExpectNumber();
      // model "-1" means "hidden"
      if (sc->Number < 0 || sc->Number > 1024) sc->Error(va("invalid model index %d in model '%s'", sc->Number, *className));
      frm.mdindex = frm.origmdindex = sc->Number;
      // frame index
      sc->ExpectNumber();
      if (sc->Number < 0 || sc->Number > 1024) sc->Error(va("invalid model frame %d in model '%s'", sc->Number, *className));
      frm.frindex = frm.origmdlframe = sc->Number;
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
      // model "-1" means "hidden"
      if (sc->Number < 0 || sc->Number > 1024) sc->Error(va("invalid model index %d in model '%s'", sc->Number, *className));
      frm.mdindex = frm.origmdindex = sc->Number;
      // frame name
      sc->ExpectString();
      //if (sc->String.isEmpty()) sc->Error(va("empty model frame name model '%s'", *className));
      frm.frindex = frm.origmdlframe = -1;
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
      continue;
    }
    // "ZOffset"
    if (sc->Check("ZOffset")) {
      sc->ExpectFloatWithSign();
      zoffset = sc->Float;
      wasZOffset = true;
      continue;
    }
    // "InheritActorPitch"
    if (sc->Check("InheritActorPitch")) {
      usePitch = false;
      usePitchInverted = true;
      usePitchMomentum = false;
      continue;
    }
    // "InheritActorRoll"
    if (sc->Check("InheritActorRoll")) {
      useRoll = true;
      continue;
    }
    // "UseActorPitch"
    if (sc->Check("UseActorPitch")) {
      usePitch = true;
      usePitchInverted = false;
      usePitchMomentum = false;
      continue;
    }
    // "UseActorRoll"
    if (sc->Check("UseActorRoll")) {
      useRoll = true;
      continue;
    }
    // "PitchFromMomentum"
    if (sc->Check("PitchFromMomentum")) {
      usePitch = false;
      usePitchInverted = false;
      usePitchMomentum = true;
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

  checkModelSanity();
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
    vassert(fi.mdlindex == mdlindex);
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
  fi.zoffset = zoffset;
  fi.used = true; // why not?
  return fi.vvframe;
}


//==========================================================================
//
//  GZModelDef::checkModelSanity
//
//==========================================================================
void GZModelDef::checkModelSanity () {
  // build frame map
  bool hasValidFrames = false;
  bool hasInvalidFrames = false;

  // clear existing frame maps, just in case
  int validModelCount = 0;
  for (auto &&mdl : models) {
    mdl.frameMap.clear();
    mdl.reported = false;
    mdl.frlist.clear();
    mdl.frlistLoaded = false;
    // remove non-existant model
    if (mdl.modelFile.isEmpty()) continue;
    if (!IsModelFileExists(mdl.modelFile)) { mdl.modelFile.clear(); continue; }
    ++validModelCount;
  }

  TMap<VStr, int> frameMap; // key: frame base; value; first in list

  for (auto &&it : frames.itemsIdx()) {
    Frame &frm = it.value();
    frm.linkSprBase = -1;
    // check for MD2 named frames
    if (frm.frindex == -1) {
      int mdlindex = frm.mdindex;
      if (mdlindex < 0 || mdlindex >= models.length() || models[mdlindex].modelFile.isEmpty() || models[mdlindex].reported) {
        frm.vvindex = -1;
      } else {
        if (!models[mdlindex].frlistLoaded) {
          VStr mfn = models[mdlindex].modelFile;
          if (!ParseMD2Frames(mfn, models[mdlindex].frlist)) {
            GLog.WriteLine(NAME_Warning, "alias model '%s' not found for class '%s'", *mfn, *className);
            models[mdlindex].reported = true;
          }
        }
        frm.vvindex = -1;
        for (auto &&nit : models[mdlindex].frlist.itemsIdx()) {
          if (nit.value().strEquCI(frm.frname)) {
            frm.vvindex = nit.index();
            break;
          }
        }
        if (frm.vvindex < 0 && !models[mdlindex].reported) {
          GLog.WriteLine(NAME_Warning, "alias model '%s' not found for class '%s'", *models[mdlindex].modelFile, *className);
          models[mdlindex].reported = true;
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
    frm.usePitch = usePitch;
    frm.usePitchInverted = usePitchInverted;
    frm.usePitchMomentum = usePitchMomentum;
    frm.useRoll = useRoll;

    // add to frame map; order doesn't matter
    {
      auto fsp = frameMap.get(frm.sprbase);
      frm.linkSprBase = (fsp ? *fsp : -1);
      frameMap.put(frm.sprbase, it.index());
    }
  }

  // is it empty?
  if (!hasValidFrames && !hasInvalidFrames) { clear(); return; }

  if (!hasValidFrames) {
    GLog.WriteLine(NAME_Warning, "gz alias model '%s' nas no valid frames!", *className);
    clear();
    return;
  }

  {
    // check if we have several models, but only one model defined for sprite frame
    // if it is so, attach all models to this frame (it seems that GZDoom does this)
    // note that invalid frames weren't added to frame map
    const int origFrLen = frames.length();
    for (int fridx = 0; fridx < origFrLen; ++fridx) {
      Frame &frm = frames[fridx];
      if (frm.vvindex < 0) continue; // ignore invalid frames
      auto fsp = frameMap.get(frm.sprbase);
      if (!fsp) continue; // the thing that should not be
      // remove duplicate frames, count models
      int mdcount = 0;
      for (int idx = *fsp; idx >= 0; idx = frames[idx].linkSprBase) {
        if (idx == fridx) { ++mdcount; continue; }
        Frame &cfr = frames[idx];
        if (cfr.sprframe != frm.sprframe) continue; // different sprite animation frame
        // different model?
        if (cfr.mdindex != frm.mdindex) { ++mdcount; continue; }
        // check if it is a duplicate model frame
        if (cfr.frindex == frm.frindex) cfr.vvindex = -1; // don't render this, it is excessive
      }
      vassert(frm.vvindex >= 0);
      // if only one model used, but we have more, attach all models
      if (mdcount < 2 && validModelCount > 1) {
        GCon->Logf(NAME_Warning, "force-attaching all models to gz alias model '%s', frame %s %c", *className, *frm.sprbase.toUpperCase(), 'A'+frm.sprframe);
        for (int mnum = 0; mnum < models.length(); ++mnum) {
          if (mnum == frm.mdindex) continue;
          if (models[mnum].modelFile.isEmpty()) continue;
          Frame newfrm = frm;
          newfrm.mdindex = mnum;
          if (newfrm.frindex == -1) {
            //md2 named
            newfrm.vvindex = findModelFrame(newfrm.mdindex, newfrm.vvindex, true); // allow appending
          } else {
            //indexed
            newfrm.vvindex = findModelFrame(newfrm.mdindex, newfrm.frindex, true); // allow appending
          }
          frames.append(newfrm);
        }
      }
    }
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
    if (frames.length() == 0) { hasValidFrames = false; hasInvalidFrames = false; clear(); return; }
  }

  // clear unused model names
  for (auto &&mdl : models) {
    if (mdl.frameMap.length() == 0) {
      mdl.modelFile.clear();
      mdl.skinFile.clear();
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

  //GCon->Logf(NAME_Debug, "MERGE: <%s> and <%s>", *className, *other.className);

  // this is brute-force approach, which can be made faster, but meh...
  // just go through other model def frames, and try to find the correspondence
  // in this model, or append new data.
  bool compactFrameMaps = false;
  for (auto &&ofrm : other.frames) {
    if (ofrm.vvindex < 0) continue; // this frame is invalid
    // ok, we have a frame, try to find a model for it
    VStr omdf = other.models[ofrm.mdindex].modelFile;
    VStr osdf = other.models[ofrm.mdindex].skinFile;
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
      //GCon->Logf(NAME_Debug, "  new model: <%s> <%s>", *omdf, *osdf);
    }

    // try to find a model frame to reuse
    MSDef &rmdl = models[mdlindex];
    const MdlFrameInfo &omfrm = other.models[ofrm.mdindex].frameMap[ofrm.vvindex];
    int frmapindex = -1;
    if (!newModel) {
      for (auto &&mfrm : rmdl.frameMap) {
        if (mfrm.mdlindex == mdlindex &&
            mfrm.mdlframe == omfrm.mdlframe &&
            mfrm.scale == omfrm.scale &&
            mfrm.offset == omfrm.offset &&
            mfrm.zoffset == omfrm.zoffset)
        {
          // yay, i found her!
          // reuse this frame
          frmapindex = mfrm.vvframe;
          /*
          GCon->Logf(NAME_Debug, "  merge frames: %d -> %d (frmapindex=%d)", ofrm.vvindex, mfrm.vvframe, frmapindex);
          GCon->Logf(NAME_Debug, "    mfrm=%s", *mfrm.toString());
          GCon->Logf(NAME_Debug, "    ofrm=%s", *ofrm.toString());
          */
          break;
        }
      }
    }

    if (frmapindex < 0) {
      // ok, we have no suitable model frame, append a new one
      //GCon->Logf(NAME_Debug, "  new frame (mdlindex=%d; frindex=%d); ofrm=%s", mdlindex, ofrm.frindex, *ofrm.toString());
      frmapindex = rmdl.frameMap.length();
      MdlFrameInfo &nfi = rmdl.frameMap.alloc();
      nfi.mdlindex = mdlindex;
      nfi.mdlframe = ofrm.frindex;
      nfi.vvframe = rmdl.frameMap.length()-1;
      nfi.scale = omfrm.scale;
      nfi.offset = omfrm.offset;
      nfi.zoffset = omfrm.zoffset;
    }

    // find sprite frame to replace
    // HACK: same model indicies will be replaced; this is how GZDoom does it
    int spfindex = -1;
    for (auto &&sit : frames.itemsIdx()) {
      Frame &ff = sit.value();
      if (ff.sprframe == ofrm.sprframe &&
          ff.origmdindex == ofrm.origmdindex &&
          ff.sprbase == ofrm.sprbase)
      {
        GLog.WriteLine(NAME_Warning, "class '%s' (%s%c) attaches alias models several times!", *className, *ff.sprbase.toUpperCase(), 'A'+ff.sprframe);
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
    newfrm.usePitch = ofrm.usePitch;
    newfrm.usePitchInverted = ofrm.usePitchInverted;
    newfrm.usePitchMomentum = ofrm.usePitchMomentum;
    newfrm.useRoll = ofrm.useRoll;
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
    vassert(unusedFramesCount >= 0);
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
        vassert(newvvindex[frm.vvindex] >= 0);
        frm.vvindex = newvvindex[frm.vvindex];
      }
      mdl.frameMap = newmap;
      // fix indicies
      for (auto &&xit : mdl.frameMap.itemsIdx()) xit.value().vvframe = xit.index();
    }
  }
}


//==========================================================================
//
//  appendScale
//
//==========================================================================
static void appendScale (VStr &res, TVec scale, const TVec *baseScale) {
  scale = sanitiseScale(scale);
  if (baseScale) {
    if (*baseScale == scale) return; // base scale is set
    if (baseScale->x != scale.x && baseScale->y != scale.y && baseScale->z != scale.z) {
      if (scale.x != 1) res += va(" scale_x=\"%g\"", scale.x);
      if (scale.y != 1) res += va(" scale_y=\"%g\"", scale.y);
      if (scale.z != 1) res += va(" scale_z=\"%g\"", scale.z);
    } else {
      if (baseScale->x != scale.x) res += va(" scale_x=\"%g\"", scale.x);
      if (baseScale->y != scale.y) res += va(" scale_y=\"%g\"", scale.y);
      if (baseScale->z != scale.z) res += va(" scale_z=\"%g\"", scale.z);
    }
  } else {
    if (scale.x != 1) res += va(" scale_x=\"%g\"", scale.x);
    if (scale.y != 1) res += va(" scale_y=\"%g\"", scale.y);
    if (scale.z != 1) res += va(" scale_z=\"%g\"", scale.z);
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
    vassert(!mdl.modelFile.isEmpty());
    const char *mdtag = (mdl.modelFile.extractFileExtension().strEquCI(".md2") ? "md2" : "md3");
    res += va("  <model name=\"%s_%d\">\n", *className.toLowerCase().xmlEscape(), it.index());
    res += va("    <%s file=\"%s\" noshadow=\"false\"", mdtag, *mdl.modelFile.xmlEscape());
    appendScale(res, scale, nullptr);
    if (offset.x != 0) res += va(" offset_x=\"%g\"", offset.x);
    if (offset.y != 0) res += va(" offset_y=\"%g\"", offset.y);
    if (offset.z != 0) res += va(" offset_z=\"%g\"", offset.z);
    if (zoffset != 0) res += va(" shift_z=\"%g\"", zoffset);
    res += ">\n";
    if (!mdl.skinFile.isEmpty()) {
      res += va("      <skin file=\"%s\" />\n", *mdl.skinFile.xmlEscape());
    }
    // write frame list
    for (auto &&fit : mdl.frameMap.itemsIdx()) {
      const MdlFrameInfo &fi = fit.value();
      vassert(it.index() == fi.mdlindex);
      vassert(fit.index() == fi.vvframe);
      res += va("      <frame index=\"%d\"", fi.mdlframe);
      appendScale(res, fi.scale, &scale);
      if (fi.offset.x != offset.x) res += va(" offset_x=\"%g\"", fi.offset.x);
      if (fi.offset.y != offset.y) res += va(" offset_y=\"%g\"", fi.offset.y);
      if (fi.offset.z != offset.z) res += va(" offset_z=\"%g\"", fi.offset.z);
      if (fi.zoffset != zoffset) res += va(" shift_z=\"%g\"", fi.zoffset);
      res += " />\n";
    }
    res += va("    </%s>\n", mdtag);
    res += "  </model>\n";
  }

  // write class definition
  res += va("  <class name=\"%s\" noselfshadow=\"true\">\n", *className.xmlEscape());
  for (auto &&frm : frames) {
    if (frm.vvindex < 0) continue;
    res += va("    <state sprite=\"%s\" sprite_frame=\"%s\" model=\"%s_%d\" frame_index=\"%d\" gzdoom=\"true\"",
      *frm.sprbase.toUpperCase().xmlEscape(),
      *VStr((char)(frm.sprframe+'A')).xmlEscape(),
      *className.toLowerCase().xmlEscape(), frm.mdindex,
      frm.vvindex);
    if (frm.rotationSpeed) res += " rotation=\"true\"";
    if (frm.angleOffset.yaw) res += va(" rotate_yaw=\"%g\"", frm.angleOffset.yaw);
    if (frm.angleOffset.pitch) res += va(" rotate_pitch=\"%g\"", frm.angleOffset.pitch);
    if (frm.angleOffset.roll) res += va(" rotate_roll=\"%g\"", frm.angleOffset.roll);
    if (frm.usePitch) res += va(" usepitch=\"true\"");
    if (frm.usePitchInverted) res += va(" usepitch=\"inverted\"");
    if (frm.usePitchMomentum) res += va(" usepitch=\"momentum\"");
    if (frm.useRoll) res += va(" useroll=\"true\"");
    res += " />\n";
  }
  res += "  </class>\n";
  res += "</vavoom_model_definition>\n";
  return res;
}
