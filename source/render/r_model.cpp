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
#include "gamedefs.h"
#include "r_local.h"


#define SMOOTHSTEP(x) ((x)*(x)*(3.0f-2.0f*(x)))


enum { NUMVERTEXNORMALS = 162 };

extern VCvarF gl_alpha_threshold;
static inline float getAlphaThreshold () { float res = gl_alpha_threshold; if (res < 0) res = 0; else if (res > 1) res = 1; return res; }

static VCvarB mdl_report_errors("mdl_report_errors", false, "Show errors in alias models?", 0/*CVAR_Archive*/);
static VCvarI mdl_verbose_loading("mdl_verbose_loading", "0", "Verbose alias model loading?", 0/*CVAR_Archive*/);

static VCvarB gl_dbg_log_model_rendering("gl_dbg_log_model_rendering", false, "Some debug log.", CVAR_PreInit);

static VCvarB r_model_autorotating("r_model_autorotating", true, "Allow model autorotation?", CVAR_Archive);
static VCvarB r_model_autobobbing("r_model_autobobbing", true, "Allow model autobobbing?", CVAR_Archive);

static VCvarB r_preload_alias_models("r_preload_alias_models", true, "Preload all alias models and their skins?", CVAR_Archive|CVAR_PreInit);

static VCvarB dbg_dump_gzmodels("dbg_dump_gzmodels", false, "Dump xml files for gz modeldefs?", /*CVAR_Archive|*/CVAR_PreInit);


static int cli_DisableModeldef = 0;
static TMap<VStrCI, bool> cli_IgnoreModelClass;

/*static*/ bool cliRegister_rmodel_args =
  VParsedArgs::RegisterFlagSet("-no-modeldef", "disable GZDoom MODELDEF lump parsing", &cli_DisableModeldef) &&
  VParsedArgs::RegisterCallback("-model-ignore-classes", "!do not use model for the following class names", [] (VArgs &args, int idx) -> int {
    for (++idx; !VParsedArgs::IsArgBreaker(args, idx); ++idx) {
      VStr mn = args[idx];
      if (!mn.isEmpty()) cli_IgnoreModelClass.put(mn, true);
    }
    return idx;
  });



// ////////////////////////////////////////////////////////////////////////// //
// RR GG BB or -1
static int parseHexRGB (VStr str) {
  vuint32 ppc = M_ParseColor(*str);
  return (ppc&0xffffff);
}


// ////////////////////////////////////////////////////////////////////////// //
struct VScriptSubModel {
  struct VFrame {
    int Index;
    int PositionIndex;
    float AlphaStart;
    float AlphaEnd;
    AliasModelTrans Transform;
    int SkinIndex;

    void copyFrom (const VFrame &src) {
      Index = src.Index;
      PositionIndex = src.PositionIndex;
      AlphaStart = src.AlphaStart;
      AlphaEnd = src.AlphaEnd;
      Transform = src.Transform;
      SkinIndex = src.SkinIndex;
    }
  };

  VMeshModel *Model;
  VMeshModel *PositionModel;
  int SkinAnimSpeed;
  int SkinAnimRange;
  int Version;
  int MeshIndex; // for md3
  TArray<VFrame> Frames;
  TArray<VMeshModel::SkinInfo> Skins;
  bool FullBright;
  bool NoShadow;
  bool UseDepth;
  bool AllowTransparency;

  void copyFrom (VScriptSubModel &src) {
    Model = src.Model;
    PositionModel = src.PositionModel;
    SkinAnimSpeed = src.SkinAnimSpeed;
    SkinAnimRange = src.SkinAnimRange;
    Version = src.Version;
    MeshIndex = src.MeshIndex; // for md3
    FullBright = src.FullBright;
    NoShadow = src.NoShadow;
    UseDepth = src.UseDepth;
    AllowTransparency = src.AllowTransparency;
    // copy skin names
    Skins.setLength(src.Skins.length());
    for (int f = 0; f < src.Skins.length(); ++f) Skins[f] = src.Skins[f];
    // copy skin shades
    //SkinShades.setLength(src.SkinShades.length());
    //for (int f = 0; f < src.SkinShades.length(); ++f) SkinShades[f] = src.SkinShades[f];
    // copy frames
    Frames.setLength(src.Frames.length());
    for (int f = 0; f < src.Frames.length(); ++f) Frames[f].copyFrom(src.Frames[f]);
  }
};


// ////////////////////////////////////////////////////////////////////////// //
struct VScriptModel {
  VName Name;
  TArray<VScriptSubModel> SubModels;
};


// ////////////////////////////////////////////////////////////////////////// //
struct ModelAngle {
public:
  enum Mode { Relative, Absolute, RelativeRandom, AbsoluteRandom };
public:
  float angle;
  Mode mode;

  ModelAngle () : angle(0.0f), mode(Relative) {}

  inline void SetRelative (float aangle) { angle = AngleMod(aangle); mode = Relative; }
  inline void SetAbsolute (float aangle) { angle = AngleMod(aangle); mode = Absolute; }
  inline void SetAbsoluteRandom () { angle = AngleMod(360.0f*Random()); mode = AbsoluteRandom; }
  inline void SetRelativeRandom () { angle = AngleMod(360.0f*Random()); mode = RelativeRandom; }

  inline float GetAngle (float baseangle, vuint8 rndVal) const {
    switch (mode) {
      case Relative: return AngleMod(baseangle+angle);
      case Absolute: return angle;
      case RelativeRandom: return AngleMod(baseangle+angle+(float)rndVal*360.0f/255.0f);
      case AbsoluteRandom: return AngleMod(angle+(float)rndVal*360.0f/255.0f);
    }
    return angle;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
struct VScriptedModelFrame {
  int Number;
  float Inter;
  int ModelIndex;
  int SubModelIndex; // you can select submodel from any model if you wish to; use -1 to render all submodels; -2 to render none
  int FrameIndex;
  float AngleStart;
  float AngleEnd;
  float AlphaStart;
  float AlphaEnd;
  ModelAngle angleYaw;
  ModelAngle angleRoll;
  ModelAngle anglePitch;
  float rotateSpeed; // yaw rotation speed
  float bobSpeed; // bobbing speed
  //
  VName sprite;
  int frame; // sprite frame
  // index for next frame with the same sprite and frame
  int nextSpriteIdx;
  // index for next frame with the same number
  int nextNumberIdx;

  void copyFrom (const VScriptedModelFrame &src) {
    Number = src.Number;
    Inter = src.Inter;
    ModelIndex = src.ModelIndex;
    SubModelIndex = src.SubModelIndex;
    FrameIndex = src.FrameIndex;
    AngleStart = src.AngleStart;
    AngleEnd = src.AngleEnd;
    AlphaStart = src.AlphaStart;
    AlphaEnd = src.AlphaEnd;
    angleYaw = src.angleYaw;
    angleRoll = src.angleRoll;
    anglePitch = src.anglePitch;
    rotateSpeed = src.rotateSpeed;
    bobSpeed = src.bobSpeed;
    sprite = src.sprite;
    frame = src.frame;
    nextSpriteIdx = src.nextSpriteIdx;
    nextNumberIdx = src.nextNumberIdx;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
struct VClassModelScript {
  VName Name;
  VModel *Model;
  bool NoSelfShadow;
  bool OneForAll; // no need to do anything, this is "one model for all frames"
  TArray<VScriptedModelFrame> Frames;
  bool CacheBuilt;
  bool isGZDoom;
  TMapNC<vuint32, int> SprFrameMap; // sprite name -> frame index (first)
  TMapNC<int, int> NumFrameMap; // frame number -> frame index (first)
};


// ////////////////////////////////////////////////////////////////////////// //
struct VModel {
  VStr Name;
  TArray<VScriptModel> Models;
  VClassModelScript *DefaultClass;
};


// ////////////////////////////////////////////////////////////////////////// //
// precalculated dot products for quantized angles
/*
enum { SHADEDOT_QUANT = 16 };
static const float r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anorm_dots.h"
;
*/


static TArray<VModel *> mod_known;
static TArray<VMeshModel *> GMeshModels;
static TArray<VClassModelScript *> ClassModels;
static TMapNC<VName, VClassModelScript *> ClassModelMap;
static bool ClassModelMapRebuild = true;
TArray<int> AllModelTextures;
static TMapNC<int, bool> AllModelTexturesSeen;
static TMap<VStr, VModel *> fixedModelMap;

static const float r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};


// ////////////////////////////////////////////////////////////////////////// //
static void ParseGZModelDefs ();
#include "r_model_gz.cpp"

class GZModelDefEx : public GZModelDef {
public:
  virtual bool ParseMD2Frames (VStr mdpath, TArray<VStr> &names) override;
  virtual bool IsModelFileExists (VStr mdpath) override;
};


//==========================================================================
//
//  SprNameFrameToInt
//
//==========================================================================
static inline vuint32 SprNameFrameToInt (VName name, int frame) {
  return (vuint32)name.GetIndex()|((vuint32)frame<<19);
}


//==========================================================================
//
//  FindClassModelByName
//
//==========================================================================
static VClassModelScript *FindClassModelByName (VName clsName) {
  if (ClassModelMapRebuild) {
    // build map
    ClassModelMapRebuild = false;
    ClassModelMap.reset();
    for (auto &&mdl : ClassModels) {
      if (mdl->Name == NAME_None || !mdl->Model || mdl->Frames.length() == 0) continue;
      ClassModelMap.put(mdl->Name, mdl);
    }
  }
  auto mp = ClassModelMap.find(clsName);
  return (mp ? *mp : nullptr);
}


//==========================================================================
//
//  R_ModelNoSelfShadow
//
//==========================================================================
bool R_ModelNoSelfShadow (VName clsName) {
  VClassModelScript *cs = FindClassModelByName(clsName);
  return (cs && cs->NoSelfShadow);
}


//==========================================================================
//
//  R_InitModels
//
//==========================================================================
void R_InitModels () {
  for (int Lump = W_IterateFile(-1, "models/models.xml"); Lump != -1; Lump = W_IterateFile(Lump, "models/models.xml")) {
    VStream *lumpstream = W_CreateLumpReaderNum(Lump);
    VCheckedStream Strm(lumpstream);
    if (mdl_verbose_loading) {
      GCon->Logf(NAME_Init, "parsing model definition '%s'", *W_FullLumpName(Lump));
    }
    // parse the file
    VXmlDocument *Doc = new VXmlDocument();
    Doc->Parse(Strm, "models/models.xml");
    for (VXmlNode *N = Doc->Root.FindChild("include"); N; N = N->FindNext()) Mod_FindName(N->GetAttribute("file"));
    delete Doc;
  }

  if (!cli_DisableModeldef) ParseGZModelDefs();
}


//==========================================================================
//
//  R_FreeModels
//
//==========================================================================
void R_FreeModels () {
  for (auto &&it : mod_known) {
    delete it;
    it = nullptr;
  }
  mod_known.clear();

  for (auto &&it : GMeshModels) {
    delete it;
    it = nullptr;
  }
  GMeshModels.clear();

  for (auto &&it : ClassModels) {
    delete it;
    it = nullptr;
  }
  ClassModels.clear();

  fixedModelMap.clear();
  ClassModelMap.clear();
  ClassModelMapRebuild = true;
}


//==========================================================================
//
//  Mod_FindMeshModel
//
//==========================================================================
static VMeshModel *Mod_FindMeshModel (VStr filename, VStr name, int meshIndex) {
  if (name.IsEmpty()) Sys_Error("Mod_ForName: nullptr name");

  if (name.indexOf('/') < 0) {
    filename = filename.ExtractFilePath().toLowerCase();
    if (filename.length()) name = filename+name;
  }

  // search the currently loaded models
  for (auto &&it : GMeshModels) {
    if (it->MeshIndex == meshIndex && it->Name == name) return it;
  }

  VMeshModel *mod = new VMeshModel();
  mod->Name = name;
  mod->MeshIndex = meshIndex;
  mod->loaded = false;
  GMeshModels.Append(mod);

  return mod;
}


//==========================================================================
//
//  ParseAngle
//
//==========================================================================
static void ParseAngle (VXmlNode *N, const char *name, ModelAngle &angle) {
  angle.SetRelative(0.0f);
  VStr aname = VStr("angle_")+name;
  if (N->HasAttribute(aname)) {
    VStr val = N->GetAttribute(aname);
    if (val.ICmp("random") == 0) angle.SetAbsoluteRandom(); else angle.SetAbsolute(VStr::atof(*val));
  } else {
    aname = VStr("rotate_")+name;
    if (N->HasAttribute(aname)) {
      VStr val = N->GetAttribute(aname);
      if (val.ICmp("random") == 0) angle.SetRelativeRandom(); else angle.SetRelative(VStr::atof(*val));
    }
  }
}


//==========================================================================
//
//  ParseBool
//
//==========================================================================
static bool ParseBool (VXmlNode *N, const char *name, bool defval) {
  if (!N->HasAttribute(name)) return defval;
  VStr val = N->GetAttribute(name);
  if (val.ICmp("yes") == 0 || val.ICmp("tan") == 0 || val.ICmp("true") == 0) return true;
  return false;
}


//==========================================================================
//
//  ParseVector
//
//  `vec` must be initialised
//
//==========================================================================
static void ParseVector (VXmlNode *SN, TVec &vec, const char *basename) {
  vassert(SN);
  vassert(basename);
  if (SN->HasAttribute(basename)) {
    vec.x = VStr::atof(*SN->GetAttribute("scale"), vec.x);
    vec.y = vec.x;
    vec.z = vec.x;
  } else {
    VStr xname;
    xname = VStr(basename)+"_x"; if (SN->HasAttribute(xname)) vec.x = VStr::atof(*SN->GetAttribute(xname), vec.x);
    xname = VStr(basename)+"_y"; if (SN->HasAttribute(xname)) vec.y = VStr::atof(*SN->GetAttribute(xname), vec.y);
    xname = VStr(basename)+"_z"; if (SN->HasAttribute(xname)) vec.z = VStr::atof(*SN->GetAttribute(xname), vec.z);
  }
}


//==========================================================================
//
//  ParseTransform
//
//  `trans` must be initialised
//
//==========================================================================
static void ParseTransform (VXmlNode *SN, AliasModelTrans &trans) {
  ParseVector(SN, trans.Shift, "shift");
  ParseVector(SN, trans.Offset, "offset");
  ParseVector(SN, trans.Scale, "scale");
}


//==========================================================================
//
//  ParseIntWithDefault
//
//==========================================================================
static int ParseIntWithDefault (VXmlNode *SN, const char *fieldname, int defval) {
  vassert(SN);
  vassert(fieldname && fieldname[0]);
  if (!SN->HasAttribute(fieldname)) return defval;
  int val = defval;
  if (!SN->GetAttribute(fieldname).trimAll().convertInt(&val)) Sys_Error("model node '%s' should have integer value, but '%s' found", fieldname, *SN->GetAttribute(fieldname));
  return val;
}


//==========================================================================
//
//  ParseFloatWithDefault
//
//==========================================================================
static float ParseFloatWithDefault (VXmlNode *SN, const char *fieldname, float defval) {
  vassert(SN);
  vassert(fieldname && fieldname[0]);
  if (!SN->HasAttribute(fieldname)) return defval;
  float val = defval;
  if (!SN->GetAttribute(fieldname).trimAll().convertFloat(&val)) Sys_Error("model node '%s' should have floating value, but '%s' found", fieldname, *SN->GetAttribute(fieldname));
  return val;
}


//==========================================================================
//
//  ParseModelScript
//
//==========================================================================
static void ParseModelXml (VModel *Mdl, VXmlDocument *Doc, bool isGZDoom=false) {
  // verify that it's a model definition file
  if (Doc->Root.Name != "vavoom_model_definition") Sys_Error("%s is not a valid model definition file", *Mdl->Name);

  Mdl->DefaultClass = nullptr;

  // process model definitions
  for (VXmlNode *N = Doc->Root.FindChild("model"); N; N = N->FindNext()) {
    VScriptModel &SMdl = Mdl->Models.Alloc();
    SMdl.Name = *N->GetAttribute("name");

    // process model parts
    const char *mdx = (N->FindChild("md2") ? "md2" : "md3");
    for (VXmlNode *SN = N->FindChild(mdx); SN; SN = SN->FindNext()) {
      VScriptSubModel &Md2 = SMdl.SubModels.Alloc();

      bool hasMeshIndex = false;
      Md2.MeshIndex = 0;
      if (SN->HasAttribute("mesh_index")) {
        hasMeshIndex = true;
        Md2.MeshIndex = VStr::atoi(*SN->GetAttribute("mesh_index"));
      }

      Md2.Model = Mod_FindMeshModel(Mdl->Name, SN->GetAttribute("file").ToLower().FixFileSlashes(), Md2.MeshIndex);

      // version
      Md2.Version = ParseIntWithDefault(SN, "version", -1);

      // position model
      Md2.PositionModel = nullptr;
      if (SN->HasAttribute("position_file")) {
        Md2.PositionModel = Mod_FindMeshModel(Mdl->Name, SN->GetAttribute("position_file").ToLower().FixFileSlashes(), Md2.MeshIndex);
      }

      // skin animation
      Md2.SkinAnimSpeed = 0;
      Md2.SkinAnimRange = 0;
      if (SN->HasAttribute("skin_anim_speed")) {
        if (!SN->HasAttribute("skin_anim_range")) Sys_Error("'skin_anim_speed' requires 'skin_anim_range'");
        Md2.SkinAnimSpeed = ParseIntWithDefault(SN, "skin_anim_speed", 1);
        Md2.SkinAnimRange = ParseIntWithDefault(SN, "skin_anim_range", 1);
      }

      AliasModelTrans BaseTransform;
      ParseTransform(SN, BaseTransform);

      // fullbright flag
      Md2.FullBright = ParseBool(SN, "fullbright", false);
      // no shadow flag
      Md2.NoShadow = ParseBool(SN, "noshadow", false);
      // force depth test flag (for things like monsters with alpha transaparency)
      Md2.UseDepth = ParseBool(SN, "usedepth", false);

      // allow transparency in skin files
      // for skins that are transparent in solid models (Alpha = 1.0f)
      Md2.AllowTransparency = ParseBool(SN, "allowtransparency", false);

      // process frames
      int curframeindex = 0;
      for (VXmlNode *FN = SN->FindChild("frame"); FN; FN = FN->FindNext(), ++curframeindex) {
        VScriptSubModel::VFrame &F = Md2.Frames.Alloc();
        //FIXME: require index?
        F.Index = ParseIntWithDefault(FN, "index", curframeindex);

        // position model frame index
        F.PositionIndex = ParseIntWithDefault(FN, "position_index", 0);

        // frame transformation
        F.Transform = BaseTransform;
        ParseTransform(FN, F.Transform);

        // alpha
        F.AlphaStart = ParseFloatWithDefault(FN, "alpha_start", 1.0f);
        F.AlphaEnd = ParseFloatWithDefault(FN, "alpha_end", 1.0f);

        // skin index
        F.SkinIndex = ParseIntWithDefault(FN, "skin_index", -1);
      }

      // process skins
      for (VXmlNode *SkN = SN->FindChild("skin"); SkN; SkN = SkN->FindNext()) {
        VStr sfl = SkN->GetAttribute("file").ToLower().FixFileSlashes();
        if (sfl.length()) {
          if (sfl.indexOf('/') < 0) sfl = Md2.Model->Name.ExtractFilePath()+sfl;
          if (mdl_verbose_loading > 2) GCon->Logf("model '%s': skin file '%s'", *SMdl.Name, *sfl);
          VMeshModel::SkinInfo &si = Md2.Skins.alloc();
          si.fileName = *sfl;
          si.textureId = -1;
          si.shade = -1;
          if (SkN->HasAttribute("shade")) {
            sfl = SkN->GetAttribute("shade");
            si.shade = parseHexRGB(sfl);
          }
        }
      }

      // if this is MD3 without mesh index, create additional models for all meshes
      if (!hasMeshIndex && mdx[2] == '3') {
        // load model and get number of meshes
        VStream *md3strm = FL_OpenFileRead(Md2.Model->Name);
        // allow missing models
        if (md3strm) {
          char sign[4] = {0};
          md3strm->Serialise(sign, 4);
          if (memcmp(sign, "IDP3", 4) == 0) {
            // skip uninteresting data
            md3strm->Seek(4+4+64+4+4+4);
            vuint32 n = 0;
            md3strm->Serialise(&n, 4);
            n = LittleLong(n);
            if (n > 1 && n < 64) {
              GCon->Logf(NAME_Init, "model '%s' got automatic submodel%s for %u more mesh%s", *Md2.Model->Name, (n > 2 ? "s" : ""), n-1, (n > 2 ? "es" : ""));
              for (unsigned f = 1; f < n; ++f) {
                VScriptSubModel &newmdl = SMdl.SubModels.Alloc();
                newmdl.copyFrom(Md2);
                newmdl.MeshIndex = f;
                newmdl.Model = Mod_FindMeshModel(Mdl->Name, newmdl.Model->Name, newmdl.MeshIndex);
                if (newmdl.PositionModel) {
                  newmdl.PositionModel = Mod_FindMeshModel(Mdl->Name, newmdl.PositionModel->Name, newmdl.MeshIndex);
                }
              }
            } else {
              if (n != 1) GCon->Logf(NAME_Warning, "model '%s' has invalid number of meshes (%u)", *Md2.Model->Name, n);
            }
          }
          delete md3strm;
        }
      }
    }
  }

  bool ClassDefined = false;
  for (VXmlNode *CN = Doc->Root.FindChild("class"); CN; CN = CN->FindNext()) {
    VStr vcClassName = CN->GetAttribute("name");
    VClass *xcls = VClass::FindClassNoCase(*vcClassName);
    if (xcls && !xcls->IsChildOf(VEntity::StaticClass())) xcls = nullptr;
    if (xcls) {
      if (developer) GCon->Logf(NAME_Dev, "found 3d model for class `%s`", xcls->GetName());
    } else {
      GCon->Logf(NAME_Init, "found 3d model for unknown class `%s`", *vcClassName);
    }
    VClassModelScript *Cls = new VClassModelScript();
    Cls->Model = Mdl;
    Cls->Name = (xcls ? xcls->GetName() : *vcClassName);
    Cls->NoSelfShadow = ParseBool(CN, "noselfshadow", false);
    Cls->OneForAll = false;
    Cls->CacheBuilt = false;
    Cls->isGZDoom = isGZDoom;
    bool deleteIt = false;
    if (cli_IgnoreModelClass.has(*Cls->Name)) {
      GCon->Logf(NAME_Init, "model '%s' ignored by user request", *Cls->Name);
      deleteIt = true;
    }
    if (!deleteIt && xcls) {
      if (!Mdl->DefaultClass) Mdl->DefaultClass = Cls;
      ClassModels.Append(Cls);
      ClassModelMapRebuild = true;
    }
    ClassDefined = true;
    //GCon->Logf("found model for class '%s'", *Cls->Name);

    bool hasOneAll = false;
    bool hasOthers = false;

    // process frames
    for (VXmlNode *N = CN->FindChild("state"); N; N = N->FindNext()) {
      VScriptedModelFrame &F = Cls->Frames.Alloc();

      ParseAngle(N, "yaw", F.angleYaw);
      ParseAngle(N, "pitch", F.anglePitch);
      ParseAngle(N, "roll", F.angleRoll);

      if (ParseBool(N, "rotation", false)) F.rotateSpeed = 100.0f;
      if (ParseBool(N, "bobbing", false)) F.bobSpeed = 180.0f;

      int lastIndex = -666;
      if (N->HasAttribute("index")) {
        F.Number = ParseIntWithDefault(N, "index", 0);
        lastIndex = ParseIntWithDefault(N, "last_index", lastIndex);
        F.sprite = NAME_None;
        F.frame = -1;
      } else if (N->HasAttribute("sprite") && N->HasAttribute("sprite_frame")) {
        VName sprname = VName(*VStr(N->GetAttribute("sprite")).toLowerCase());
        if (sprname == NAME_None) Sys_Error("Model '%s' has invalid state (empty sprite name)", *Mdl->Name);
        VStr sprframe = N->GetAttribute("sprite_frame");
        if (sprframe.length() != 1) Sys_Error("Model '%s' has invalid state (invalid sprite frame '%s')", *Mdl->Name, *sprframe);
        int sfr = sprframe[0];
        if (sfr >= 'a' && sfr <= 'z') sfr = sfr-'a'+'A';
        sfr -= 'A';
        if (sfr < 0 || sfr > 31) Sys_Error("Model '%s' has invalid state (invalid sprite frame '%s')", *Mdl->Name, *sprframe);
        F.Number = -1;
        F.sprite = sprname;
        F.frame = sfr;
      } else {
        Sys_Error("Model '%s' has invalid state", *Mdl->Name);
      }

      F.FrameIndex = ParseIntWithDefault(N, "frame_index", 0);
      F.SubModelIndex = ParseIntWithDefault(N, "submodel_index", -1);
      if (F.SubModelIndex < 0) F.SubModelIndex = -1;
      if (ParseBool(N, "hidden", false)) F.SubModelIndex = -2; // hidden

      F.ModelIndex = -1;
      VStr MdlName = N->GetAttribute("model");
      for (int i = 0; i < Mdl->Models.Num(); ++i) {
        if (Mdl->Models[i].Name == *MdlName) {
          F.ModelIndex = i;
          break;
        }
      }
      if (F.ModelIndex == -1) Sys_Error("%s has no model %s", *Mdl->Name, *MdlName);

      F.Inter = ParseFloatWithDefault(N, "inter", 0.0f);

      F.AngleStart = ParseFloatWithDefault(N, "angle_start", 0.0f);
      F.AngleEnd = ParseFloatWithDefault(N, "angle_end", 0.0f);

      F.AlphaStart = ParseFloatWithDefault(N, "alpha_start", 1.0f);
      F.AlphaEnd = ParseFloatWithDefault(N, "alpha_end", 1.0f);

      if (F.Number >= 0 && lastIndex > 0) {
        for (int cfidx = F.Number+1; cfidx <= lastIndex; ++cfidx) {
          VScriptedModelFrame &ffr = Cls->Frames.Alloc();
          ffr.copyFrom(F);
          ffr.Number = cfidx;
        }
        hasOthers = true;
      } else {
        if (F.Number < 0 && F.sprite == NAME_None) {
          F.Number = -666;
          hasOneAll = true;
        } else {
          hasOthers = true;
        }
      }
    }
    if (Cls->Frames.length() == 0) Sys_Error("model '%s' class '%s' has no states defined", *Mdl->Name, *Cls->Name);
    if (deleteIt) { delete Cls; continue; }
    if (hasOneAll && !hasOthers) {
      Cls->OneForAll = true;
      //GCon->Logf("model '%s' for class '%s' is \"one-for-all\"", *Mdl->Name, *Cls->Name);
    }
  }
  if (!ClassDefined) Sys_Error("model '%s' defined no classes", *Mdl->Name);

  // we don't need the xml file anymore
  delete Doc;
}


//==========================================================================
//
//  ParseModelScript
//
//==========================================================================
static void ParseModelScript (VModel *Mdl, VStream &Strm, bool isGZDoom=false) {
  // parse xml file
  VXmlDocument *Doc = new VXmlDocument();
  Doc->Parse(Strm, Mdl->Name);
  ParseModelXml(Mdl, Doc, isGZDoom);
}


//==========================================================================
//
//  Mod_FindName
//
//  used in VC `InstallModel()`
//
//==========================================================================
VModel *Mod_FindName (VStr name) {
  if (name.IsEmpty()) Sys_Error("Mod_ForName: nullptr name");

  // search the currently loaded models
  for (auto &&it : mod_known) if (it->Name.ICmp(name) == 0) return it;

  VModel *mod = new VModel();
  mod->Name = name;
  mod_known.Append(mod);

  // load the file
  VStream *Strm = FL_OpenFileRead(mod->Name);
  if (!Strm) Sys_Error("Couldn't load `%s` (Mod_FindName)", *mod->Name);
  if (mdl_verbose_loading > 1) GCon->Logf(NAME_Init, "parsing model script '%s'...", *mod->Name);
  ParseModelScript(mod, *Strm);
  delete Strm;

  return mod;
}


//==========================================================================
//
//  ParseGZModelDefs
//
//==========================================================================
static void ParseGZModelDefs () {
  // build model list, so we can append them backwards
  TArray<GZModelDefEx *> gzmdlist;
  TMap<VStr, int> gzmdmap;

  VName mdfname = VName("modeldef", VName::FindLower);
  if (mdfname == NAME_None) return; // no such chunk
  // parse all modeldefs
  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) != mdfname) continue;
    GCon->Logf(NAME_Init, "parsing GZDoom ModelDef script \"%s\"...", *W_FullLumpName(Lump));
    // parse modeldef
    auto sc = new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump));
    sc->SetEscape(false);
    while (sc->GetString()) {
      if (sc->String.strEquCI("model")) {
        auto mdl = new GZModelDefEx();
        mdl->parse(sc);
        // find model
        if (!mdl->isEmpty() && !mdl->className.isEmpty()) {
          // search the currently loaded models
          VClass *xcls = VClass::FindClassNoCase(*mdl->className);
          if (xcls && !xcls->IsChildOf(VEntity::StaticClass())) xcls = nullptr;
          if (!xcls) {
            GCon->Logf(NAME_Init, "  found 3d GZDoom model for unknown class `%s`", *mdl->className);
            delete mdl;
            continue;
          }
          // check if we already have k8vavoom model for this class
          if (FindClassModelByName(xcls->GetName())) {
            for (auto &&cm : ClassModels) {
              if (cm->Name == NAME_None || !cm->Model || cm->Frames.length() == 0) continue;
              if (!cm->isGZDoom && cm->Name == xcls->GetName()) {
                GCon->Logf(NAME_Init, "  skipped GZDoom model for '%s' (found native k8vavoom definition)", xcls->GetName());
                delete mdl;
                mdl = nullptr;
                break;
              }
            }
            if (!mdl) continue;
          }
          // merge with already existing model, if there is any
          VStr locname = mdl->className.toLowerCase();
          auto omp = gzmdmap.find(locname);
          if (omp) {
            gzmdlist[*omp]->merge(*mdl);
            delete mdl;
          } else {
            // new model
            gzmdmap.put(locname, gzmdlist.length());
            gzmdlist.append(mdl);
          }
        }
        continue;
      }
      sc->Error(va("invalid MODELDEF directive '%s'", *sc->String));
      //GLog.WriteLine("%s: <%s>", *sc->GetLoc().toStringNoCol(), *sc->String);
    }
    delete sc;
  }

  // insert GZDoom alias models
  int cnt = 0;
  for (auto &&mdl : gzmdlist) {
    VClass *xcls = VClass::FindClassNoCase(*mdl->className);
    vassert(xcls);
    // get xml here, because we're going to modify the model
    auto xml = mdl->createXml();
    // create impossible name, because why not?
    if (dbg_dump_gzmodels) GCon->Logf(NAME_Debug, "====\n%s\n====", *xml);
    mdl->className = va("/gzmodels/..%s/..gzmodel_%d.xml", xcls->GetName(), cnt++);
    //GCon->Logf("***<%s>", *mdl->className);
    GCon->Logf(NAME_Init, "  found GZDoom model for '%s'", xcls->GetName());
    VModel *mod = new VModel();
    mod->Name = mdl->className;
    mod_known.Append(mod);
    // parse xml
    VStream *Strm = new VMemoryStreamRO(mdl->className, xml.getCStr(), xml.length());
    ParseModelScript(mod, *Strm, true); // gzdoom flag
    delete Strm;
    // this is not strictly safe, but meh
    delete mdl;
  }
}


#include "r_model_parsers.cpp"


//==========================================================================
//
//  PositionModel
//
//==========================================================================
static void PositionModel (TVec &Origin, TAVec &Angles, VMeshModel *wpmodel, int InFrame) {
  wpmodel->LoadFromWad();
  unsigned frame = (unsigned)InFrame;
  if (frame >= (unsigned)wpmodel->Frames.length()) frame = 0;
  TVec p[3];
  /*k8: dunno
  mtriangle_t *ptris = (mtriangle_t *)((vuint8 *)pmdl+pmdl->ofstris);
  mframe_t *pframe = (mframe_t *)((vuint8 *)pmdl+pmdl->ofsframes+frame*pmdl->framesize);
  trivertx_t *pverts = (trivertx_t *)(pframe+1);
  for (int vi = 0; vi < 3; ++vi) {
    p[vi].x = pverts[ptris[0].vertindex[vi]].v[0]*pframe->scale[0]+pframe->scale_origin[0];
    p[vi].y = pverts[ptris[0].vertindex[vi]].v[1]*pframe->scale[1]+pframe->scale_origin[1];
    p[vi].z = pverts[ptris[0].vertindex[vi]].v[2]*pframe->scale[2]+pframe->scale_origin[2];
  }
  */
  const VMeshFrame &frm = wpmodel->Frames[(int)frame];
  for (int vi = 0; vi < 3; ++vi) {
    p[vi].x = frm.Verts[wpmodel->Tris[0].VertIndex[vi]].x;
    p[vi].y = frm.Verts[wpmodel->Tris[0].VertIndex[vi]].y;
    p[vi].z = frm.Verts[wpmodel->Tris[0].VertIndex[vi]].z;
  }
  TVec md_forward(0, 0, 0), md_left(0, 0, 0), md_up(0, 0, 0);
  AngleVectors(Angles, md_forward, md_left, md_up);
  md_left = -md_left;
  Origin += md_forward*p[0].x+md_left*p[0].y+md_up*p[0].z;
  TAVec wangles;
  VectorAngles(p[1]-p[0], wangles);
  Angles.yaw = AngleMod(Angles.yaw+wangles.yaw);
  Angles.pitch = AngleMod(Angles.pitch+wangles.pitch);
}


//==========================================================================
//
//  UpdateClassFrameCache
//
//==========================================================================
static void UpdateClassFrameCache (VClassModelScript &Cls) {
  if (!Cls.CacheBuilt) {
    for (int i = 0; i < Cls.Frames.length(); ++i) {
      VScriptedModelFrame &frm = Cls.Frames[i];
      frm.nextSpriteIdx = frm.nextNumberIdx = -1;
      // sprite cache
      if (frm.sprite != NAME_None) {
        //FIXME: sanity checks
        vassert(frm.frame >= 0 && frm.frame < 4096);
        vassert(frm.sprite.GetIndex() > 0 && frm.sprite.GetIndex() < 524288);
        vuint32 nfi = SprNameFrameToInt(frm.sprite, frm.frame);
        if (!Cls.SprFrameMap.has(nfi)) {
          // new one
          Cls.SprFrameMap.put(nfi, i);
          //GCon->Logf(NAME_Debug, "*NEW sprite frame for '%s': %s %c (%d)", *Cls.Name, *frm.sprite, 'A'+frm.frame, i);
        } else {
          // add to list
          int idx = *Cls.SprFrameMap.get(nfi);
          while (Cls.Frames[idx].nextSpriteIdx != -1) idx = Cls.Frames[idx].nextSpriteIdx;
          Cls.Frames[idx].nextSpriteIdx = i;
          //GCon->Logf(NAME_Debug, "*  more sprite frames for '%s': %s %c (%d)", *Cls.Name, *frm.sprite, 'A'+frm.frame, i);
        }
      }
      // frame number cache
      if (frm.Number >= 0) {
        if (!Cls.NumFrameMap.has(frm.Number)) {
          // new one
          Cls.NumFrameMap.put(frm.Number, i);
        } else {
          // add to list
          int idx = *Cls.NumFrameMap.get(frm.Number);
          while (Cls.Frames[idx].nextNumberIdx != -1) idx = Cls.Frames[idx].nextNumberIdx;
          Cls.Frames[idx].nextNumberIdx = i;
        }
      }
    }
    Cls.CacheBuilt = true;
  }
}


#define FINDFRAME_CHECK_FRM  \
  const VScriptedModelFrame &frm = Cls.Frames[idx]; \
  if (res < 0 || (Cls.Frames[res].ModelIndex == frm.ModelIndex && Cls.Frames[res].SubModelIndex == frm.SubModelIndex)) { \
    if (frm.Inter <= Inter && frm.Inter > bestInter) { res = idx; bestInter = frm.Inter; } \
  }


//==========================================================================
//
//  FindFrame
//
//  returns first frame found
//  note that there can be several frames for one sprite!
//
//==========================================================================
static int FindFrame (VClassModelScript &Cls, const VAliasModelFrameInfo &Frame, float Inter) {
  UpdateClassFrameCache(Cls);

  // try cached frames
  if (Cls.OneForAll) return 0; // guaranteed

  // we have to check both index and frame here, because we don't know which was defined
  // i can preprocess this, but meh, i guess that hashtable+chain is fast enough

  int res = -1;
  float bestInter = -9999.9f;

  //FIXME: reduce pasta!
  if (Frame.sprite != NAME_None && Frame.frame >= 0 && Frame.frame < 4096) {
    // by sprite name
    int *idxp = Cls.SprFrameMap.find(SprNameFrameToInt(Frame.sprite, Frame.frame));
    if (idxp) {
      int idx = *idxp;
      while (idx >= 0) {
        FINDFRAME_CHECK_FRM
        idx = frm.nextSpriteIdx;
      }
      if (res >= 0) return res;
    }
  }

  if (Frame.index >= 0) {
    // by index
    int *idxp = Cls.NumFrameMap.find(Frame.index);
    if (idxp) {
      int idx = *idxp;
      while (idx >= 0) {
        FINDFRAME_CHECK_FRM
        idx = frm.nextNumberIdx;
      }
    }
  }

  return res;
}


#define FINDNEXTFRAME_CHECK_FRM  \
  const VScriptedModelFrame &nfrm = Cls.Frames[nidx]; \
  if (FDef.ModelIndex == nfrm.ModelIndex && FDef.SubModelIndex == nfrm.SubModelIndex && \
      nfrm.Inter >= FDef.Inter && nfrm.Inter < bestInter) \
  { \
    res = nidx; \
    bestInter = nfrm.Inter; \
  }


//==========================================================================
//
//  FindNextFrame
//
//==========================================================================
static int FindNextFrame (VClassModelScript &Cls, int FIdx, const VAliasModelFrameInfo &Frame, float Inter, float &InterpFrac) {
  if (FIdx < 0) { InterpFrac = 0.0f; return -1; } // just in case
  UpdateClassFrameCache(Cls);
  if (Inter < 0.0f) Inter = 0.0f; // just in case

  const VScriptedModelFrame &FDef = Cls.Frames[FIdx];

  // previous code was using `FIdx+1`, and it was wrong
  // just in case, check for valid `Inter`
  // walk the list
  if (FDef.Inter < 1.0f) {
    // doesn't finish time slice
    int res = -1;
    float bestInter = 9999.9f;

    if (FDef.sprite != NAME_None) {
      // by sprite name
      int nidx = FDef.nextSpriteIdx;
      while (nidx >= 0) {
        FINDNEXTFRAME_CHECK_FRM
        nidx = nfrm.nextSpriteIdx;
      }
    } else {
      // by frame index
      int nidx = FDef.nextNumberIdx;
      while (nidx >= 0) {
        FINDNEXTFRAME_CHECK_FRM
        nidx = nfrm.nextNumberIdx;
      }
    }

    // found interframe?
    if (res >= 0) {
      const VScriptedModelFrame &nfrm = Cls.Frames[res];
      if (nfrm.Inter <= FDef.Inter) {
        InterpFrac = 1.0f;
      } else {
        float frc = (Inter-FDef.Inter)/(nfrm.Inter-FDef.Inter);
        if (!isFiniteF(frc)) frc = 1.0f; // just in case
        InterpFrac = frc;
      }
      return res;
    }
  }

  // no interframe models found, get normal frame
  InterpFrac = (FDef.Inter >= 0.0f && FDef.Inter < 1.0f ? (Inter-FDef.Inter)/(1.0f-FDef.Inter) : 1.0f);
  return FindFrame(Cls, Frame, 0);
}


//==========================================================================
//
//  LoadModelSkins
//
//==========================================================================
static void LoadModelSkins (VModel *mdl) {
  if (!mdl) return;
  // load submodel skins
  for (auto &&ScMdl : mdl->Models) {
    for (auto &&SubMdl : ScMdl.SubModels) {
      //if (SubMdl.Version != -1 && SubMdl.Version != Version) continue;
      // locate the proper data
      SubMdl.Model->LoadFromWad();
      //FIXME: this should be done earilier
      if (SubMdl.Model->HadErrors) SubMdl.NoShadow = true;
      // load overriden submodel skins
      if (SubMdl.Skins.length()) {
        for (auto &&si : SubMdl.Skins) {
          if (si.textureId >= 0) continue;
          if (si.fileName == NAME_None) {
            si.textureId = GTextureManager.DefaultTexture;
          } else {
            si.textureId = GTextureManager.AddFileTextureShaded(si.fileName, TEXTYPE_Skin, si.shade);
            if (si.textureId < 0) si.textureId = GTextureManager.DefaultTexture;
          }
          if (si.textureId > 0 && !AllModelTexturesSeen.has(si.textureId)) {
            AllModelTexturesSeen.put(si.textureId, true);
            AllModelTextures.append(si.textureId);
          }
        }
      } else {
        // load base model skins
        for (auto &&si : SubMdl.Model->Skins) {
          if (si.textureId >= 0) continue;
          if (si.fileName == NAME_None) {
            si.textureId = GTextureManager.DefaultTexture;
          } else {
            si.textureId = GTextureManager.AddFileTextureShaded(si.fileName, TEXTYPE_Skin, si.shade);
            if (si.textureId < 0) si.textureId = GTextureManager.DefaultTexture;
          }
          if (si.textureId > 0 && !AllModelTexturesSeen.has(si.textureId)) {
            AllModelTexturesSeen.put(si.textureId, true);
            AllModelTextures.append(si.textureId);
          }
        }
      }
    }
  }
}


//==========================================================================
//
//  R_LoadAllModelsSkins
//
//==========================================================================
void R_LoadAllModelsSkins () {
  if (r_preload_alias_models) {
    AllModelTextures.reset();
    AllModelTexturesSeen.reset();
    AllModelTexturesSeen.put(GTextureManager.DefaultTexture, true);
    for (auto &&mdl : ClassModels) {
      if (mdl->Name == NAME_None || !mdl->Model || mdl->Frames.length() == 0) continue;
      LoadModelSkins(mdl->Model);
    }
    AllModelTexturesSeen.clear();
  }
}


//==========================================================================
//
//  DrawModel
//
//  FIXME: make this faster -- stop looping, cache data!
//
//==========================================================================
static void DrawModel (VLevel *Level, VEntity *mobj, const TVec &Org, const TAVec &Angles,
  float ScaleX, float ScaleY, VClassModelScript &Cls, int FIdx, int NFIdx,
  VTextureTranslation *Trans, int ColorMap, int Version, vuint32 Light,
  vuint32 Fade, float Alpha, bool Additive, bool IsViewModel, float Inter,
  bool Interpolate, const TVec &LightPos, float LightRadius, ERenderPass Pass, bool isShadowVol)
{
  VScriptedModelFrame &FDef = Cls.Frames[FIdx];
  VScriptedModelFrame &NFDef = Cls.Frames[NFIdx];
  VScriptModel &ScMdl = Cls.Model->Models[FDef.ModelIndex];
  const int allowedsubmod = FDef.SubModelIndex;
  if (allowedsubmod == -2) return; // this frame is hidden
  int submodindex = -1;
  for (auto &&SubMdl : ScMdl.SubModels) {
    ++submodindex;
    if (allowedsubmod >= 0 && submodindex != allowedsubmod) continue; // only one submodel allowed
    if (SubMdl.Version != -1 && SubMdl.Version != Version) continue;

    if (FDef.FrameIndex >= SubMdl.Frames.length()) {
      GCon->Logf("Bad sub-model frame index %d", FDef.FrameIndex);
      continue;
    }

    // cannot interpolate between different models or submodels
    if (Interpolate) {
      if (FDef.ModelIndex != NFDef.ModelIndex ||
          FDef.SubModelIndex != NFDef.SubModelIndex ||
          NFDef.FrameIndex >= SubMdl.Frames.length())
      {
        Interpolate = false;
      }
    }

    VScriptSubModel::VFrame &F = SubMdl.Frames[FDef.FrameIndex];
    VScriptSubModel::VFrame &NF = SubMdl.Frames[Interpolate ? NFDef.FrameIndex : FDef.FrameIndex];

    // locate the proper data
    SubMdl.Model->LoadFromWad();
    //FIXME: this should be done earilier
    if (SubMdl.Model->HadErrors) SubMdl.NoShadow = true;

    // skin animations
    int Md2SkinIdx = 0;
    if (F.SkinIndex >= 0) {
      Md2SkinIdx = F.SkinIndex;
    } else if (SubMdl.SkinAnimSpeed) {
      Md2SkinIdx = int((Level ? Level->Time : 0)*SubMdl.SkinAnimSpeed)%SubMdl.SkinAnimRange;
    }
    if (Md2SkinIdx < 0) Md2SkinIdx = 0; // just in case

    // get the proper skin texture ID
    int SkinID;
    if (SubMdl.Skins.length()) {
      // skins defined in definition file override all skins in MD2 file
      if (Md2SkinIdx < 0 || Md2SkinIdx >= SubMdl.Skins.length()) {
        if (SubMdl.Skins.length() == 0) Sys_Error("model '%s' has no skins", *SubMdl.Model->Name);
        //if (SubMdl.SkinShades.length() == 0) Sys_Error("model '%s' has no skin shades", *SubMdl.Model->Name);
        Md2SkinIdx = 0;
      }
      SkinID = SubMdl.Skins[Md2SkinIdx].textureId;
      if (SkinID < 0) {
        SkinID = GTextureManager.AddFileTextureShaded(SubMdl.Skins[Md2SkinIdx].fileName, TEXTYPE_Skin, SubMdl.Skins[Md2SkinIdx].shade);
        SubMdl.Skins[Md2SkinIdx].textureId = SkinID;
      }
    } else {
      if (SubMdl.Model->Skins.length() == 0) Sys_Error("model '%s' has no skins", *SubMdl.Model->Name);
      Md2SkinIdx = Md2SkinIdx%SubMdl.Model->Skins.length();
      if (Md2SkinIdx < 0) Md2SkinIdx = (Md2SkinIdx+SubMdl.Model->Skins.length())%SubMdl.Model->Skins.length();
      SkinID = SubMdl.Model->Skins[Md2SkinIdx].textureId;
      if (SkinID < 0) {
        //SkinID = GTextureManager.AddFileTexture(SubMdl.Model->Skins[Md2SkinIdx%SubMdl.Model->Skins.length()], TEXTYPE_Skin);
        SkinID = GTextureManager.AddFileTextureShaded(SubMdl.Model->Skins[Md2SkinIdx].fileName, TEXTYPE_Skin, SubMdl.Model->Skins[Md2SkinIdx].shade);
        SubMdl.Model->Skins[Md2SkinIdx].textureId = SkinID;
      }
    }
    if (SkinID < 0) SkinID = GTextureManager.DefaultTexture;

    // get and verify frame number
    int Md2Frame = F.Index;
    if ((unsigned)Md2Frame >= (unsigned)SubMdl.Model->Frames.length()) {
      if (developer) GCon->Logf(NAME_Dev, "no such frame %d in model '%s'", Md2Frame, *SubMdl.Model->Name);
      Md2Frame = (Md2Frame <= 0 ? 0 : SubMdl.Model->Frames.length()-1);
      // stop further warnings
      F.Index = Md2Frame;
    }

    // get and verify next frame number
    int Md2NextFrame = Md2Frame;
    if (Interpolate) {
      Md2NextFrame = NF.Index;
      if ((unsigned)Md2NextFrame >= (unsigned)SubMdl.Model->Frames.length()) {
        if (developer) GCon->Logf(NAME_Dev, "no such next frame %d in model '%s'", Md2NextFrame, *SubMdl.Model->Name);
        Md2NextFrame = (Md2NextFrame <= 0 ? 0 : SubMdl.Model->Frames.length()-1);
        // stop further warnings
        NF.Index = Md2NextFrame;
      }
    }

    // position
    TVec Md2Org = Org;

    // angle
    TAVec Md2Angle = Angles;
    if (FDef.AngleStart || FDef.AngleEnd != 1.0f) {
      Md2Angle.yaw = AngleMod(Md2Angle.yaw+FDef.AngleStart+(FDef.AngleEnd-FDef.AngleStart)*Inter);
    }

    vuint8 rndVal = (mobj ? (hashU32(mobj->GetUniqueId())>>4)&0xffu : 0);

    Md2Angle.yaw = FDef.angleYaw.GetAngle(Md2Angle.yaw, rndVal);
    Md2Angle.pitch = FDef.anglePitch.GetAngle(Md2Angle.pitch, rndVal);
    Md2Angle.roll = FDef.angleRoll.GetAngle(Md2Angle.roll, rndVal);

    if (Level && mobj) {
      if (r_model_autorotating && FDef.rotateSpeed) {
        Md2Angle.yaw = AngleMod(Md2Angle.yaw+Level->Time*FDef.rotateSpeed+rndVal*38.6f);
      }

      if (r_model_autobobbing && FDef.bobSpeed) {
        //GCon->Logf("UID: %3u (%s)", (hashU32(mobj->GetUniqueId())&0xff), *mobj->GetClass()->GetFullName());
        const float bobHeight = 4.0f;
        float zdelta = msin(AngleMod(Level->Time*FDef.bobSpeed+rndVal*44.5f))*bobHeight;
        Md2Org.z += zdelta+bobHeight;
      }
    }

    // position model
    if (SubMdl.PositionModel) PositionModel(Md2Org, Md2Angle, SubMdl.PositionModel, F.PositionIndex);

    // alpha
    float Md2Alpha = Alpha;
    if (FDef.AlphaStart != 1.0f || FDef.AlphaEnd != 1.0f) Md2Alpha *= FDef.AlphaStart+(FDef.AlphaEnd-FDef.AlphaStart)*Inter;
    if (F.AlphaStart != 1.0f || F.AlphaEnd != 1.0f) Md2Alpha *= F.AlphaStart+(F.AlphaEnd-F.AlphaStart)*Inter;

    const char *passname = nullptr;
    if (gl_dbg_log_model_rendering) {
      switch (Pass) {
        case RPASS_Normal: passname = "normal"; break;
        case RPASS_Ambient: passname = "ambient"; break;
        case RPASS_ShadowVolumes: passname = "shadow"; break;
        case RPASS_Light: passname = "light"; break;
        case RPASS_Textures: passname = "texture"; break;
        case RPASS_Fog: passname = "fog"; break;
        case RPASS_NonShadow: passname = "nonshadow"; break;
        default: Sys_Error("WTF?!");
      }
      GCon->Logf("000: MODEL(%s): class='%s'; alpha=%f; noshadow=%d; usedepth=%d", passname, *Cls.Name, Md2Alpha, (int)SubMdl.NoShadow, (int)SubMdl.UseDepth);
    }

    switch (Pass) {
      case RPASS_Normal:
      case RPASS_Ambient:
        break;
      case RPASS_ShadowVolumes:
        if (Md2Alpha < 1.0f || SubMdl.NoShadow) continue;
        break;
      case RPASS_Textures:
        if (Md2Alpha <= getAlphaThreshold()) continue;
        break;
      case RPASS_Light:
        if (Md2Alpha <= getAlphaThreshold() || SubMdl.NoShadow) continue;
        break;
      case RPASS_Fog:
        /*
        // noshadow model is rendered as "noshadow", so it doesn't need fog
        if (Md2Alpha <= getAlphaThreshold() || SubMdl.NoShadow) {
          //if (gl_dbg_log_model_rendering) GCon->Logf("  SKIP FOG FOR MODEL(%s): class='%s'; alpha=%f; noshadow=%d", passname, *Cls.Name, Md2Alpha, (int)SubMdl.NoShadow);
          continue;
        }
        */
        break;
      case RPASS_NonShadow:
        //if (Md2Alpha >= 1.0f && !Additive && !SubMdl.NoShadow) continue;
        if (Md2Alpha < 1.0f || Additive /*|| SubMdl.NoShadow*/) continue;
        break;
    }

    if (gl_dbg_log_model_rendering) GCon->Logf("     MODEL(%s): class='%s'; alpha=%f; noshadow=%d", passname, *Cls.Name, Md2Alpha, (int)SubMdl.NoShadow);

    float smooth_inter = (Interpolate ? SMOOTHSTEP(Inter) : 0.0f);

    AliasModelTrans Transform;
    if (Interpolate && smooth_inter) {
      // shift
      Transform.Shift.x = ((1-smooth_inter)*F.Transform.Shift.x+smooth_inter*NF.Transform.Shift.x);
      Transform.Shift.y = ((1-smooth_inter)*F.Transform.Shift.y+smooth_inter*NF.Transform.Shift.y);
      Transform.Shift.z = ((1-smooth_inter)*F.Transform.Shift.z+smooth_inter*NF.Transform.Shift.z);
      // scale
      Transform.Scale.x = (F.Transform.Scale.x+smooth_inter*(NF.Transform.Scale.x-F.Transform.Scale.x))*ScaleX;
      Transform.Scale.y = (F.Transform.Scale.y+smooth_inter*(NF.Transform.Scale.y-F.Transform.Scale.y))*ScaleX;
      Transform.Scale.z = (F.Transform.Scale.z+smooth_inter*(NF.Transform.Scale.z-F.Transform.Scale.z))*ScaleY;
      // offset
      Transform.Offset.x = ((1-smooth_inter)*F.Transform.Offset.x+smooth_inter*NF.Transform.Offset.x);
      Transform.Offset.y = ((1-smooth_inter)*F.Transform.Offset.y+smooth_inter*NF.Transform.Offset.y);
      Transform.Offset.z = ((1-smooth_inter)*F.Transform.Offset.z+smooth_inter*NF.Transform.Offset.z);
    } else {
      Transform = F.Transform;
      // special code for scale
      Transform.Scale.x = F.Transform.Scale.x*ScaleX;
      Transform.Scale.y = F.Transform.Scale.y*ScaleX;
      Transform.Scale.z = F.Transform.Scale.z*ScaleY;
    }

    // light
    vuint32 Md2Light = Light;
    if (SubMdl.FullBright) Md2Light = 0xffffffff;

    //if (Pass != RPASS_NonShadow) return;
    //if (Pass != RPASS_Ambient) return;

    switch (Pass) {
      case RPASS_Normal:
      case RPASS_NonShadow:
        if (true /*IsViewModel || !isShadowVol*/) {
          Drawer->DrawAliasModel(Md2Org, Md2Angle, Transform,
            SubMdl.Model, Md2Frame, Md2NextFrame, GTextureManager(SkinID),
            Trans, ColorMap, Md2Light, Fade, Md2Alpha, Additive,
            IsViewModel, smooth_inter, Interpolate, SubMdl.UseDepth,
            SubMdl.AllowTransparency,
            !IsViewModel && isShadowVol); // for advanced renderer, we need to fill z-buffer, but not color buffer
        }
        break;
      case RPASS_Ambient:
        if (!SubMdl.AllowTransparency)
          Drawer->DrawAliasModelAmbient(Md2Org, Md2Angle, Transform,
            SubMdl.Model, Md2Frame, Md2NextFrame, GTextureManager(SkinID),
            Md2Light, Md2Alpha, smooth_inter, Interpolate, SubMdl.UseDepth,
            SubMdl.AllowTransparency);
        break;
      case RPASS_ShadowVolumes:
        Drawer->DrawAliasModelShadow(Md2Org, Md2Angle, Transform,
          SubMdl.Model, Md2Frame, Md2NextFrame, smooth_inter, Interpolate,
          LightPos, LightRadius);
        break;
      case RPASS_Light:
        Drawer->DrawAliasModelLight(Md2Org, Md2Angle, Transform,
          SubMdl.Model, Md2Frame, Md2NextFrame, GTextureManager(SkinID),
          Md2Alpha, smooth_inter, Interpolate, SubMdl.AllowTransparency);
        break;
      case RPASS_Textures:
        Drawer->DrawAliasModelTextures(Md2Org, Md2Angle, Transform,
          SubMdl.Model, Md2Frame, Md2NextFrame, GTextureManager(SkinID),
          Trans, ColorMap, Md2Alpha, smooth_inter, Interpolate, SubMdl.UseDepth,
          SubMdl.AllowTransparency);
        break;
      case RPASS_Fog:
        Drawer->DrawAliasModelFog(Md2Org, Md2Angle, Transform,
          SubMdl.Model, Md2Frame, Md2NextFrame, GTextureManager(SkinID),
          Fade, Md2Alpha, smooth_inter, Interpolate, SubMdl.AllowTransparency);
        break;
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::HasAliasModel
//
//==========================================================================
bool VRenderLevelShared::HasAliasModel (VName clsName) const {
  return (clsName != NAME_None && FindClassModelByName(clsName));
}


//==========================================================================
//
//  VRenderLevelShared::IsAliasModelAllowedFor
//
//==========================================================================
bool VRenderLevelShared::IsAliasModelAllowedFor (VEntity *Ent) {
  if (!Ent || Ent->IsGoingToDie() || !r_models) return false;
  if (Ent->IsPlayer()) return r_models_players;
  if (Ent->IsMissile()) return r_models_missiles;
  if (Ent->IsCorpse()) return r_models_corpses;
  if (Ent->IsMonster()) return r_models_monsters;
  if (Ent->IsSolid()) return r_models_decorations;
  // check for pickup
  // inventory class
  static VClass *invCls = nullptr;
  static bool invClsInited = false;
  if (!invClsInited) {
    invClsInited = true;
    invCls = VMemberBase::StaticFindClass("Inventory");
  }
  if (invCls && Ent->IsA(invCls)) return r_models_pickups;
  return r_models_other;
}


//==========================================================================
//
//  VRenderLevelShared::HasEntityAliasModel
//
//==========================================================================
bool VRenderLevelShared::HasEntityAliasModel (VEntity *Ent) const {
  return (IsAliasModelAllowedFor(Ent) && FindClassModelByName(Ent->GetClass()->Name));
}


//==========================================================================
//
//  VRenderLevelShared::DrawAliasModel
//
//  this is used to draw so-called "fixed model"
//
//==========================================================================
bool VRenderLevelShared::DrawAliasModel (VEntity *mobj, const TVec &Org, const TAVec &Angles,
  float ScaleX, float ScaleY, VModel *Mdl,
  const VAliasModelFrameInfo &Frame, const VAliasModelFrameInfo &NextFrame,
  VTextureTranslation *Trans, int Version, vuint32 Light, vuint32 Fade,
  float Alpha, bool Additive, bool IsViewModel, float Inter, bool Interpolate,
  ERenderPass Pass)
{
  //if (!IsAliasModelAllowedFor(mobj)) return false;
  int FIdx = FindFrame(*Mdl->DefaultClass, Frame, Inter);
  if (FIdx == -1) return false;
  float InterpFrac;
  int NFIdx = FindNextFrame(*Mdl->DefaultClass, FIdx, NextFrame, Inter, InterpFrac);
  if (NFIdx == -1) {
    NFIdx = FIdx;
    Interpolate = false;
  }
  DrawModel(Level, mobj, Org, Angles, ScaleX, ScaleY, *Mdl->DefaultClass, FIdx,
    NFIdx, Trans, ColorMap, Version, Light, Fade, Alpha, Additive,
    IsViewModel, InterpFrac, Interpolate, CurrLightPos, CurrLightRadius,
    Pass, IsShadowVolumeRenderer());
  return true;
}


//==========================================================================
//
//  VRenderLevelShared::DrawAliasModel
//
//  this is used to draw entity models
//
//==========================================================================
bool VRenderLevelShared::DrawAliasModel (VEntity *mobj, VName clsName, const TVec &Org, const TAVec &Angles,
  float ScaleX, float ScaleY,
  const VAliasModelFrameInfo &Frame, const VAliasModelFrameInfo &NextFrame, //old:VState *State, VState *NextState,
  VTextureTranslation *Trans, int Version, vuint32 Light, vuint32 Fade,
  float Alpha, bool Additive, bool IsViewModel, float Inter, bool Interpolate,
  ERenderPass Pass)
{
  if (clsName == NAME_None) return false;
  if (!IsAliasModelAllowedFor(mobj)) return false;

  VClassModelScript *Cls = FindClassModelByName(clsName);
  if (!Cls) return false;

  int FIdx = FindFrame(*Cls, Frame, Inter);
  if (FIdx == -1) return false;

  // note that gzdoom-imported modeldef can have more than one model attached to one frame
  // process all attachments -- they should differ by model or submodel indicies

  const bool origInterp = Interpolate;
  while (FIdx >= 0) {
    float InterpFrac;
    int NFIdx = FindNextFrame(*Cls, FIdx, NextFrame, Inter, InterpFrac);
    if (NFIdx == -1) {
      NFIdx = FIdx;
      Interpolate = false;
    } else {
      Interpolate = origInterp;
    }

    DrawModel(Level, mobj, Org, Angles, ScaleX, ScaleY, *Cls, FIdx, NFIdx, Trans,
      ColorMap, Version, Light, Fade, Alpha, Additive, IsViewModel,
      InterpFrac, Interpolate, CurrLightPos, CurrLightRadius, Pass, IsShadowVolumeRenderer());

    // try next one
    const VScriptedModelFrame &cfrm = Cls->Frames[FIdx];
    int res = -1;
    if (cfrm.sprite != NAME_None) {
      // by sprite name
      //GCon->Logf(NAME_Debug, "000: %s: sprite=%s %c; midx=%d; smidx=%d; inter=%g (%g); nidx=%d", *Cls->Name, *cfrm.sprite, 'A'+cfrm.frame, cfrm.ModelIndex, cfrm.SubModelIndex, Inter, cfrm.Inter, FIdx);
      FIdx = cfrm.nextSpriteIdx;
      //GCon->Logf(NAME_Debug, "000: %s: sprite=%s %c; midx=%d; smidx=%d; inter=%g (%g); nidx=%d", *Cls->Name, *cfrm.sprite, 'A'+cfrm.frame, cfrm.ModelIndex, cfrm.SubModelIndex, Inter, cfrm.Inter, FIdx);
      while (FIdx >= 0) {
        const VScriptedModelFrame &nfrm = Cls->Frames[FIdx];
        //GCon->Logf(NAME_Debug, "  001: %s: sprite=%s %c; midx=%d; smidx=%d; inter=%g (%g)", *Cls->Name, *nfrm.sprite, 'A'+nfrm.frame, nfrm.ModelIndex, nfrm.SubModelIndex, Inter, nfrm.Inter);
        if (cfrm.ModelIndex != nfrm.ModelIndex || cfrm.SubModelIndex != nfrm.SubModelIndex) {
               if (nfrm.Inter <= Inter) res = FIdx;
          else if (nfrm.Inter > Inter) break; // the author shouldn't write incorrect defs
        }
        FIdx = nfrm.nextSpriteIdx;
      }
    } else {
      // by frame index
      FIdx = cfrm.nextNumberIdx;
      while (FIdx >= 0) {
        const VScriptedModelFrame &nfrm = Cls->Frames[FIdx];
        if (cfrm.ModelIndex != nfrm.ModelIndex || cfrm.SubModelIndex != nfrm.SubModelIndex) {
               if (nfrm.Inter <= Inter) res = FIdx;
          else if (nfrm.Inter > Inter) break; // the author shouldn't write incorrect defs
        }
        FIdx = nfrm.nextNumberIdx;
      }
    }
    FIdx = res;
  }

  return true;
}


//==========================================================================
//
//  FindFixedModelFor
//
//==========================================================================
static VModel *FindFixedModelFor (VEntity *Ent, bool verbose) {
  vassert(Ent);
  auto mpp = fixedModelMap.find(Ent->FixedModelName);
  if (mpp) return *mpp;
  // first time
  VStr fname = VStr("models/")+Ent->FixedModelName;
  if (!FL_FileExists(fname)) {
    if (verbose) GCon->Logf(NAME_Warning, "Can't find alias model '%s'", *Ent->FixedModelName);
    fixedModelMap.put(Ent->FixedModelName, nullptr);
    return nullptr;
  } else {
    VModel *mdl = Mod_FindName(fname);
    fixedModelMap.put(Ent->FixedModelName, mdl);
    return mdl;
  }
}


//==========================================================================
//
//  VRenderLevelShared::DrawEntityModel
//
//==========================================================================
bool VRenderLevelShared::DrawEntityModel (VEntity *Ent, vuint32 Light, vuint32 Fade,
  float Alpha, bool Additive, float Inter, ERenderPass Pass)
{
  //VState *DispState = (Ent->EntityFlags&VEntity::EF_UseDispState ? Ent->DispState : Ent->State);
  //VState *DispState = Ent->State; //FIXME: skipframes

  // movement interpolation
  TVec sprorigin = Ent->GetDrawOrigin();

  // check if we want to interpolate model frames
  const bool Interpolate = r_interpolate_frames;
  if (Ent->EntityFlags&VEntity::EF_FixedModel) {
    VModel *Mdl = FindFixedModelFor(Ent, true); // verbose
    if (!Mdl) return false;
    return DrawAliasModel(Ent, sprorigin,
      Ent->/*Angles*/GetModelDrawAngles(), Ent->ScaleX, Ent->ScaleY, Mdl,
      Ent->getMFI(), Ent->getNextMFI(),
      GetTranslation(Ent->Translation),
      Ent->ModelVersion, Light, Fade, Alpha, Additive, false, Inter,
      Interpolate, Pass);
  } else {
    return DrawAliasModel(Ent, Ent->GetClass()->Name, sprorigin,
      Ent->/*Angles*/GetModelDrawAngles(), Ent->ScaleX, Ent->ScaleY,
      Ent->getMFI(), Ent->getNextMFI(),
      GetTranslation(Ent->Translation), Ent->ModelVersion, Light, Fade,
      Alpha, Additive, false, Inter, Interpolate, Pass);
  }
}


//==========================================================================
//
//  VRenderLevelShared::CheckAliasModelFrame
//
//==========================================================================
bool VRenderLevelShared::CheckAliasModelFrame (VEntity *Ent, float Inter) {
  if (!Ent->State) return false;
  if (Ent->EntityFlags&VEntity::EF_FixedModel) {
    VModel *Mdl = FindFixedModelFor(Ent, false); // silent
    if (!Mdl) return false;
    return FindFrame(*Mdl->DefaultClass, Ent->getMFI(), Inter) != -1;
  } else {
    VClassModelScript *Cls = FindClassModelByName(Ent->State->Outer->Name);
    if (!Cls) return false;
    return (FindFrame(*Cls, Ent->getMFI(), Inter) != -1);
  }
}


//==========================================================================
//
//  R_DrawModelFrame
//
//  used only in UI, for model selector
//
//==========================================================================
void R_DrawModelFrame (const TVec &Origin, float Angle, VModel *Model,
  int Frame, int NextFrame,
  //const VAliasModelFrameInfo &Frame, const VAliasModelFrameInfo &NextFrame,
  const char *Skin, int TranslStart,
  int TranslEnd, int Color, float Inter)
{
  //FIXME!
  /*
  bool Interpolate = true;
  int FIdx = FindFrame(*Model->DefaultClass, Frame, Inter);
  if (FIdx == -1) return;

  float InterpFrac;
  int NFIdx = FindNextFrame(*Model->DefaultClass, FIdx, NextFrame, Inter, InterpFrac);
  if (NFIdx == -1) {
    NFIdx = 0;
    Interpolate = false;
  }

  viewangles.yaw = 180;
  viewangles.pitch = 0;
  viewangles.roll = 0;
  AngleVectors(viewangles, viewforward, viewright, viewup);
  vieworg = TVec(0, 0, 0);

  refdef_t rd;

  rd.x = 0;
  rd.y = 0;
  rd.width = Drawer->getWidth();
  rd.height = Drawer->getHeight();
  rd.fovx = tan(DEG2RADF(90)/2.0f);
  rd.fovy = rd.fovx*3.0f/4.0f;
  rd.drawworld = false;
  rd.DrawCamera = false;

  Drawer->SetupView(nullptr, &rd);
  Drawer->SetupViewOrg();

  TAVec Angles;
  Angles.yaw = Angle;
  Angles.pitch = 0;
  Angles.roll = 0;

  DrawModel(nullptr, nullptr, Origin, Angles, 1.0f, 1.0f, *Model->DefaultClass, FIdx,
    NFIdx, R_GetCachedTranslation(R_SetMenuPlayerTrans(TranslStart,
    TranslEnd, Color), nullptr), 0, 0, 0xffffffff, 0, 1.0f, false, false,
    InterpFrac, Interpolate, TVec(), 0, RPASS_Normal, true); // force draw

  Drawer->EndView();
  */
}


//==========================================================================
//
//  R_DrawStateModelFrame
//
//  called from UI widget only
//
//==========================================================================
bool R_DrawStateModelFrame (VState *State, VState *NextState, float Inter,
                            const TVec &Origin, float Angle)
{
  bool Interpolate = true;
  if (!State) return false;
  VClassModelScript *Cls = FindClassModelByName(State->Outer->Name);
  if (!Cls) return false;
  int FIdx = FindFrame(*Cls, State->getMFI(), Inter);
  if (FIdx == -1) return false;
  float InterpFrac;
  int NFIdx = FindNextFrame(*Cls, FIdx, (NextState ? NextState->getMFI() : State->getMFI()), Inter, InterpFrac);
  if (NFIdx == -1) {
    NFIdx = 0;
    Interpolate = false;
  }

  viewangles.yaw = 180;
  viewangles.pitch = 0;
  viewangles.roll = 0;
  AngleVectors(viewangles, viewforward, viewright, viewup);
  vieworg = TVec(0, 0, 0);

  refdef_t rd;

  rd.x = 0;
  rd.y = 0;
  rd.width = Drawer->getWidth();
  rd.height = Drawer->getHeight();
  TClipBase::CalcFovXY(&rd.fovx, &rd.fovy, rd.width, rd.height, 90.0f, PixelAspect);
  rd.drawworld = false;
  rd.DrawCamera = false;

  Drawer->SetupView(nullptr, &rd);
  Drawer->SetupViewOrg();

  TAVec Angles;
  Angles.yaw = Angle;
  Angles.pitch = 0;
  Angles.roll = 0;

  DrawModel(nullptr, nullptr, Origin, Angles, 1.0f, 1.0f, *Cls, FIdx, NFIdx, nullptr, 0, 0,
    0xffffffff, 0, 1.0f, false, false, InterpFrac, Interpolate,
    TVec(), 0, RPASS_Normal, true); // force draw

  Drawer->EndView();
  return true;
}
