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


// ////////////////////////////////////////////////////////////////////////// //
// RR GG BB or -1
static int parseHexRGB (const VStr &str) {
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
    TVec Offset;
    TVec Scale;
    int SkinIndex;

    void copyFrom (const VFrame &src) {
      Index = src.Index;
      PositionIndex = src.PositionIndex;
      AlphaStart = src.AlphaStart;
      AlphaEnd = src.AlphaEnd;
      Offset = src.Offset;
      Scale = src.Scale;
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
struct TVertMap {
  int VertIndex;
  int STIndex;
};


// ////////////////////////////////////////////////////////////////////////// //
struct VTempEdge {
  vuint16 Vert1;
  vuint16 Vert2;
  //vuint16 OrigVert1;
  //vuint16 OrigVert2;
  vint16 Tri1;
  vint16 Tri2;
};


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

static const float r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};


// ////////////////////////////////////////////////////////////////////////// //
static void ParseGZModelDefs ();
#include "r_model_gz.cpp"


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

  if (!GArgs.CheckParm("-no-modeldef") && !GArgs.CheckParm("-no-modeldefs") &&
      !GArgs.CheckParm("-no-gzmodeldef") && !GArgs.CheckParm("-no-gzmodeldefs") &&
      !GArgs.CheckParm("-no-gz-modeldef") && !GArgs.CheckParm("-no-gz-modeldefs"))
  {
    ParseGZModelDefs();
  }
}


//==========================================================================
//
//  R_FreeModels
//
//==========================================================================
void R_FreeModels () {
  for (int i = 0; i < mod_known.Num(); ++i) {
    delete mod_known[i];
    mod_known[i] = nullptr;
  }
  mod_known.Clear();

  for (int i = 0; i < GMeshModels.Num(); ++i) {
    //if (GMeshModels[i]->Data) Z_Free(GMeshModels[i]->Data);
    delete GMeshModels[i];
    GMeshModels[i] = nullptr;
  }
  GMeshModels.Clear();

  for (int i = 0; i < ClassModels.Num(); ++i) {
    delete ClassModels[i];
    ClassModels[i] = nullptr;
  }
  ClassModels.Clear();
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
  for (int i = 0; i < GMeshModels.Num(); ++i) {
    if (GMeshModels[i]->MeshIndex == meshIndex && GMeshModels[i]->Name == name) return GMeshModels[i];
  }

  VMeshModel *mod = new VMeshModel();
  mod->Name = name;
  mod->MeshIndex = meshIndex;
  //mod->Data = nullptr;
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
    const VStr &val = N->GetAttribute(aname);
    if (val.ICmp("random") == 0) angle.SetAbsoluteRandom(); else angle.SetAbsolute(VStr::atof(*val));
  } else {
    aname = VStr("rotate_")+name;
    if (N->HasAttribute(aname)) {
      const VStr &val = N->GetAttribute(aname);
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
  const VStr &val = N->GetAttribute(name);
  if (val.ICmp("yes") == 0 || val.ICmp("tan") == 0 || val.ICmp("true") == 0) return true;
  return false;
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
      Md2.Version = -1;
      if (SN->HasAttribute("version")) Md2.Version = VStr::atoi(*SN->GetAttribute("version"));

      // position model
      Md2.PositionModel = nullptr;
      if (SN->HasAttribute("position_file")) {
        Md2.PositionModel = Mod_FindMeshModel(Mdl->Name, SN->GetAttribute("position_file").ToLower().FixFileSlashes(), Md2.MeshIndex);
      }

      // skin animation
      Md2.SkinAnimSpeed = 0;
      Md2.SkinAnimRange = 0;
      if (SN->HasAttribute("skin_anim_speed")) {
        Md2.SkinAnimSpeed = VStr::atoi(*SN->GetAttribute("skin_anim_speed"));
        Md2.SkinAnimRange = VStr::atoi(*SN->GetAttribute("skin_anim_range"));
      }

      // base offset
      TVec Offset(0.0f, 0.0f, 0.0f);
      if (SN->HasAttribute("offset_x")) Offset.x = VStr::atof(*SN->GetAttribute("offset_x"));
      if (SN->HasAttribute("offset_y")) Offset.y = VStr::atof(*SN->GetAttribute("offset_y"));
      if (SN->HasAttribute("offset_z")) Offset.z = VStr::atof(*SN->GetAttribute("offset_z"));

      // base scaling
      TVec Scale(1.0f, 1.0f, 1.0f);
      if (SN->HasAttribute("scale")) {
        Scale.x = VStr::atof(*SN->GetAttribute("scale"), 1);
        Scale.y = Scale.x;
        Scale.z = Scale.x;
      }
      if (SN->HasAttribute("scale_x")) Scale.x = VStr::atof(*SN->GetAttribute("scale_x"), 1);
      if (SN->HasAttribute("scale_y")) Scale.y = VStr::atof(*SN->GetAttribute("scale_y"), 1);
      if (SN->HasAttribute("scale_z")) Scale.z = VStr::atof(*SN->GetAttribute("scale_z"), 1);

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
      for (VXmlNode *FN = SN->FindChild("frame"); FN; FN = FN->FindNext()) {
        VScriptSubModel::VFrame &F = Md2.Frames.Alloc();
        F.Index = VStr::atoi(*FN->GetAttribute("index"));

        // position model frame index
        F.PositionIndex = 0;
        if (FN->HasAttribute("position_index")) F.PositionIndex = VStr::atoi(*FN->GetAttribute("position_index"));

        // offset
        F.Offset = Offset;
        if (FN->HasAttribute("offset_x")) F.Offset.x = VStr::atof(*FN->GetAttribute("offset_x"));
        if (FN->HasAttribute("offset_y")) F.Offset.y = VStr::atof(*FN->GetAttribute("offset_y"));
        if (FN->HasAttribute("offset_z")) F.Offset.z = VStr::atof(*FN->GetAttribute("offset_z"));

        // scale
        F.Scale = Scale;
        if (FN->HasAttribute("scale")) {
          F.Scale.x = VStr::atof(*FN->GetAttribute("scale"), 1);
          F.Scale.y = F.Scale.x;
          F.Scale.z = F.Scale.x;
        }
        if (FN->HasAttribute("scale_x")) F.Scale.x = VStr::atof(*FN->GetAttribute("scale_x"), 1);
        if (FN->HasAttribute("scale_y")) F.Scale.y = VStr::atof(*FN->GetAttribute("scale_y"), 1);
        if (FN->HasAttribute("scale_z")) F.Scale.z = VStr::atof(*FN->GetAttribute("scale_z"), 1);

        // alpha
        F.AlphaStart = 1.0f;
        F.AlphaEnd = 1.0f;
        if (FN->HasAttribute("alpha_start")) F.AlphaStart = VStr::atof(*FN->GetAttribute("alpha_start"));
        if (FN->HasAttribute("alpha_end")) F.AlphaEnd = VStr::atof(*FN->GetAttribute("alpha_end"), 1);

        // skin index
        F.SkinIndex = -1;
        if (FN->HasAttribute("skin_index")) F.SkinIndex = VStr::atoi(*FN->GetAttribute("skin_index"));
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
    {
      int fp = GArgs.CheckParm("-model-ignore-class");
      while (++fp != GArgs.Count()) {
        if (GArgs[fp][0] == '-' || GArgs[fp][0] == '+') break;
        VStr cname = VStr(GArgs[fp]);
        if (cname.ICmp(*Cls->Name) == 0) { deleteIt = true; break; }
      }
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
        F.Number = VStr::atoi(*N->GetAttribute("index"));
        if (N->HasAttribute("last_index")) lastIndex = VStr::atoi(*N->GetAttribute("last_index"));
        F.sprite = NAME_None;
        F.frame = -1;
      } else if (N->HasAttribute("sprite") && N->HasAttribute("sprite_frame")) {
        VName sprname = VName(*VStr(N->GetAttribute("sprite")).toLowerCase());
        if (sprname == NAME_None) Sys_Error("Model '%s' has invalid state (empty sprite name)", *Mdl->Name);
        VStr sprframe = N->GetAttribute("sprite_frame");
        if (sprframe.length() != 1) Sys_Error("Model '%s' has invalid state (invalid sprite frame '%s')", *Mdl->Name, *sprframe);
        int sfr = sprframe[0];
             if (sfr >= 'A' && sfr <= 'Z') sfr -= 'A';
        else if (sfr >= 'a' && sfr <= 'z') sfr -= 'a';
        else Sys_Error("Model '%s' has invalid state (invalid sprite frame '%s')", *Mdl->Name, *sprframe);
        F.Number = -1;
        F.sprite = sprname;
        F.frame = sfr;
      } else {
        Sys_Error("Model '%s' has invalid state", *Mdl->Name);
      }

      F.FrameIndex = VStr::atoi(*N->GetAttribute("frame_index"));

      F.ModelIndex = -1;
      VStr MdlName = N->GetAttribute("model");
      for (int i = 0; i < Mdl->Models.Num(); ++i) {
        if (Mdl->Models[i].Name == *MdlName) {
          F.ModelIndex = i;
          break;
        }
      }
      if (F.ModelIndex == -1) Sys_Error("%s has no model %s", *Mdl->Name, *MdlName);

      F.Inter = 0.0f;
      if (N->HasAttribute("inter")) F.Inter = VStr::atof(*N->GetAttribute("inter"));

      F.AngleStart = 0.0f;
      F.AngleEnd = 0.0f;
      if (N->HasAttribute("angle_start")) F.AngleStart = VStr::atof(*N->GetAttribute("angle_start"));
      if (N->HasAttribute("angle_end")) F.AngleEnd = VStr::atof(*N->GetAttribute("angle_end"));

      F.AlphaStart = 1.0f;
      F.AlphaEnd = 1.0f;
      if (N->HasAttribute("alpha_start")) F.AlphaStart = VStr::atof(*N->GetAttribute("alpha_start"));
      if (N->HasAttribute("alpha_end")) F.AlphaEnd = VStr::atof(*N->GetAttribute("alpha_end"), 1);

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
    if (!Cls->Frames.Num()) Sys_Error("model '%s' class '%s' has no states defined", *Mdl->Name, *Cls->Name);
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
VModel *Mod_FindName (const VStr &name) {
  if (name.IsEmpty()) Sys_Error("Mod_ForName: nullptr name");

  // search the currently loaded models
  for (int i = 0; i < mod_known.Num(); ++i) {
    if (mod_known[i]->Name.ICmp(name) == 0) return mod_known[i];
  }

  VModel *mod = new VModel();
  mod->Name = name;
  mod_known.Append(mod);

  // load the file
  VStream *Strm = FL_OpenFileRead(mod->Name);
  if (!Strm) Sys_Error("Couldn't load `%s`", *mod->Name);
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
  VName mdfname = VName("modeldef", VName::FindLower);
  if (mdfname == NAME_None) return; // no such chunk
  // build lump list, so we can load them in backwars order
  TArray<int> lumpList;
  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) != mdfname) continue;
    lumpList.append(Lump);
  }
  if (lumpList.length() == 0) return;
  for (int llidx = lumpList.length()-1; llidx >= 0; --llidx) {
    int Lump = lumpList[llidx];
    GCon->Logf(NAME_Init, "parsing GZDoom ModelDef script \"%s\"...", *W_FullLumpName(Lump));
    int cnt = 0;
    // build model list, so we can append them backwards
    TArray<GZModelDef *> gzmdlist;
    auto sc = new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump));
    while (sc->GetString()) {
      if (sc->String.strEquCI("model")) {
        auto mdl = new GZModelDef();
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
          gzmdlist.append(mdl);
        }
        continue;
      }
      sc->Error(va("invalid MODELDEF directive '%s'", *sc->String));
      //GLog.WriteLine("%s: <%s>", *sc->GetLoc().toStringNoCol(), *sc->String);
    }
    delete sc;
    // insert GZDoom alias models, backwards
    for (int gzmidx = gzmdlist.length()-1; gzmidx >= 0; --gzmidx) {
      GZModelDef *mdl = gzmdlist[gzmidx];
      gzmdlist[gzmidx] = nullptr;
      VClass *xcls = VClass::FindClassNoCase(*mdl->className);
      check(xcls);
      bool foundCM = false;
      if (FindClassModelByName(xcls->GetName())) {
        for (auto &&cm : ClassModels) {
          if (cm->Name == NAME_None || !cm->Model || cm->Frames.length() == 0) continue;
          if (cm->Name == xcls->GetName()) {
            // allow GZDoom alias model overrides
            GCon->Logf(NAME_Init, "  skipped GZDoom model for '%s' (found %s definition)", xcls->GetName(), (cm->isGZDoom ? "GZDoom" : "native k8vavoom"));
            foundCM = true;
            break;
          }
        }
      }
      if (!foundCM) {
        // get xml here, because we're going to modify the model
        auto xml = mdl->createXml();
        // create impossible name, because why not?
        mdl->className = va("/gzmodels_%d/%s/gzmodel_%d_%d.xml", Lump, xcls->GetName(), Lump, cnt++);
        //GCon->Logf("***<%s>", *mdl->className);
        GCon->Logf(NAME_Init, "  found GZDoom model for '%s'", xcls->GetName());
        VModel *mod = new VModel();
        mod->Name = mdl->className;
        mod_known.Append(mod);
        // parse xml
        VStream *Strm = new VMemoryStreamRO(W_FullLumpName(Lump), xml.getCStr(), xml.length());
        ParseModelScript(mod, *Strm, true); // gzdoom flag
        delete Strm;
      }
      delete mdl;
    }
  }
}


//==========================================================================
//
//  AddEdge
//
//==========================================================================
static void AddEdge (TArray<VTempEdge> &Edges, int Vert1, int Vert2, int Tri) {
  // check for a match
  // compare original vertex indices since texture coordinates are not important here
  for (int i = 0; i < Edges.Num(); ++i) {
    VTempEdge &E = Edges[i];
    if (E.Tri2 == -1 && E.Vert1 == Vert2 && E.Vert2 == Vert1) {
      E.Tri2 = Tri;
      return;
    }
  }

  // add new edge
  VTempEdge &e = Edges.Alloc();
  e.Vert1 = Vert1;
  e.Vert2 = Vert2;
  e.Tri1 = Tri;
  e.Tri2 = -1;
}


//==========================================================================
//
//  getStrZ
//
//==========================================================================
static VStr getStrZ (const char *s, unsigned maxlen) {
  if (!s || maxlen == 0 || !s[0]) return VStr::EmptyString;
  const char *se = s;
  while (maxlen-- && *se) ++se;
  return VStr(s, (int)(ptrdiff_t)(se-s));
}


//==========================================================================
//
//  Mod_BuildFrames
//
//==========================================================================
static void Mod_BuildFrames (VMeshModel *mod, vuint8 *Data) {
  mmdl_t *pmodel;
  mstvert_t *pstverts;
  mtriangle_t *ptri;
  mframe_t *pframe;

  pmodel = (mmdl_t *)/*mod->*/Data;
  mod->Uploaded = false;
  mod->VertsBuffer = 0;
  mod->IndexBuffer = 0;

  // endian-adjust and swap the data, starting with the alias model header
  for (unsigned i = 0; i < sizeof(mmdl_t)/4; ++i) ((vint32 *)pmodel)[i] = LittleLong(((vint32 *)pmodel)[i]);

  if (pmodel->version != ALIAS_VERSION) Sys_Error("model '%s' has wrong version number (%i should be %i)", *mod->Name, pmodel->version, ALIAS_VERSION);
  if (pmodel->numverts <= 0) Sys_Error("model '%s' has no vertices", *mod->Name);
  if (pmodel->numverts > MAXALIASVERTS) Sys_Error("model '%s' has too many vertices", *mod->Name);
  if (pmodel->numstverts <= 0) Sys_Error("model '%s' has no texture vertices", *mod->Name);
  if (pmodel->numstverts > MAXALIASSTVERTS) Sys_Error("model '%s' has too many texture vertices", *mod->Name);
  if (pmodel->numtris <= 0) Sys_Error("model '%s' has no triangles", *mod->Name);
  if (pmodel->numtris > 65536) Sys_Error("model '%s' has too many triangles", *mod->Name);
  //if (pmodel->skinwidth&0x03) Sys_Error("Mod_LoadAliasModel: skinwidth not multiple of 4");
  if (pmodel->numskins < 1 || pmodel->numskins > 1024) Sys_Error("model '%s' has invalid number of skins: %u", *mod->Name, pmodel->numskins);
  if (pmodel->numframes < 1 || pmodel->numframes > 1024) Sys_Error("model '%s' has invalid numebr of frames: %u", *mod->Name, pmodel->numframes);

  // base s and t vertices
  pstverts = (mstvert_t *)((vuint8 *)pmodel+pmodel->ofsstverts);
  for (unsigned i = 0; i < pmodel->numstverts; ++i) {
    pstverts[i].s = LittleShort(pstverts[i].s);
    pstverts[i].t = LittleShort(pstverts[i].t);
  }

  // triangles
  //k8: this tried to collape same vertices, but meh
  TArray<TVertMap> VertMap;
  TArray<VTempEdge> Edges;
  mod->Tris.SetNum(pmodel->numtris);
  ptri = (mtriangle_t *)((vuint8 *)pmodel+pmodel->ofstris);
  for (unsigned i = 0; i < pmodel->numtris; ++i) {
    for (unsigned j = 0; j < 3; ++j) {
      ptri[i].vertindex[j] = LittleShort(ptri[i].vertindex[j]);
      ptri[i].stvertindex[j] = LittleShort(ptri[i].stvertindex[j]);
      mod->Tris[i].VertIndex[j] = VertMap.length();
      TVertMap &v = VertMap.alloc();
      v.VertIndex = ptri[i].vertindex[j];
      v.STIndex = ptri[i].stvertindex[j];
    }
    for (unsigned j = 0; j < 3; ++j) {
      AddEdge(Edges, mod->Tris[i].VertIndex[j], mod->Tris[i].VertIndex[(j+1)%3], i);
    }
  }

  // calculate remapped ST verts
  mod->STVerts.SetNum(VertMap.Num());
  for (int i = 0; i < VertMap.Num(); ++i) {
    mod->STVerts[i].S = (float)pstverts[VertMap[i].STIndex].s/(float)pmodel->skinwidth;
    mod->STVerts[i].T = (float)pstverts[VertMap[i].STIndex].t/(float)pmodel->skinheight;
  }

  // frames
  bool hadError = false;
  bool showError = true;

  // if we have only one frame, and that frame has invalid triangles, just rebuild it
  TArray<vuint8> validTri;
  if (pmodel->numframes == 1) {
    validTri.setLength((int)pmodel->numtris);
    memset(validTri.ptr(), 0, pmodel->numtris);
  }

  mod->Frames.SetNum(pmodel->numframes);
  mod->AllVerts.SetNum(pmodel->numframes*VertMap.Num());
  mod->AllNormals.SetNum(pmodel->numframes*VertMap.Num());
  mod->AllPlanes.SetNum(pmodel->numframes*pmodel->numtris);
  pframe = (mframe_t *)((vuint8 *)pmodel+pmodel->ofsframes);

  int triIgnored = 0;
  for (unsigned i = 0; i < pmodel->numframes; ++i) {
    pframe->scale[0] = LittleFloat(pframe->scale[0]);
    pframe->scale[1] = LittleFloat(pframe->scale[1]);
    pframe->scale[2] = LittleFloat(pframe->scale[2]);
    pframe->scale_origin[0] = LittleFloat(pframe->scale_origin[0]);
    pframe->scale_origin[1] = LittleFloat(pframe->scale_origin[1]);
    pframe->scale_origin[2] = LittleFloat(pframe->scale_origin[2]);

    VMeshFrame &Frame = mod->Frames[i];
    Frame.Name = getStrZ(pframe->name, 16);
    Frame.Scale = TVec(pframe->scale[0], pframe->scale[1], pframe->scale[2]);
    Frame.Origin = TVec(pframe->scale_origin[0], pframe->scale_origin[1], pframe->scale_origin[2]);
    Frame.Verts = &mod->AllVerts[i*VertMap.Num()];
    Frame.Normals = &mod->AllNormals[i*VertMap.Num()];
    Frame.Planes = &mod->AllPlanes[i*pmodel->numtris];
    Frame.VertsOffset = 0;
    Frame.NormalsOffset = 0;
    Frame.TriCount = pmodel->numtris;
    //Frame.ValidTris.setLength((int)pmodel->numtris);

    trivertx_t *Verts = (trivertx_t *)(pframe+1);
    for (int j = 0; j < VertMap.Num(); ++j) {
      const trivertx_t &Vert = Verts[VertMap[j].VertIndex];
      Frame.Verts[j].x = Vert.v[0]*pframe->scale[0]+pframe->scale_origin[0];
      Frame.Verts[j].y = Vert.v[1]*pframe->scale[1]+pframe->scale_origin[1];
      Frame.Verts[j].z = Vert.v[2]*pframe->scale[2]+pframe->scale_origin[2];
      Frame.Normals[j] = r_avertexnormals[Vert.lightnormalindex];
    }

    for (unsigned j = 0; j < pmodel->numtris; ++j) {
      TVec PlaneNormal;
      TVec v3(0, 0, 0);
      bool reported = false, hacked = false;
      for (int vnn = 0; vnn < 3; ++vnn) {
        TVec v1 = Frame.Verts[mod->Tris[j].VertIndex[(vnn+0)%3]];
        TVec v2 = Frame.Verts[mod->Tris[j].VertIndex[(vnn+1)%3]];
             v3 = Frame.Verts[mod->Tris[j].VertIndex[(vnn+2)%3]];

        TVec d1 = v2-v3;
        TVec d2 = v1-v3;
        PlaneNormal = CrossProduct(d1, d2);
        if (lengthSquared(PlaneNormal) == 0) {
          //k8:hack!
          if (mdl_report_errors && !reported) {
            GCon->Logf("Alias model '%s' has degenerate triangle %d; v1=(%f,%f,%f), v2=(%f,%f,%f); v3=(%f,%f,%f); d1=(%f,%f,%f); d2=(%f,%f,%f); cross=(%f,%f,%f)",
              *mod->Name, j, v1.x, v1.y, v1.z, v2.x, v2.y, v2.z, v3.x, v3.y, v3.z, d1.x, d1.y, d1.z, d2.x, d2.y, d2.z, PlaneNormal.x, PlaneNormal.y, PlaneNormal.z);
          }
          reported = true;
        } else {
          hacked = (vnn != 0);
          break;
        }
      }
      if (mdl_report_errors && reported) {
        if (hacked) GCon->Log("  hacked around"); else { GCon->Log("  CANNOT FIX"); PlaneNormal = TVec(0, 0, 1); }
      }
      hadError = hadError || reported;
      PlaneNormal = Normalise(PlaneNormal);
      const float PlaneDist = DotProduct(PlaneNormal, v3);
      Frame.Planes[j].Set(PlaneNormal, PlaneDist);
      if (reported) {
        ++triIgnored;
        if (mdl_report_errors) GCon->Logf("  triangle #%u is ignored", j);
        if (validTri.length()) validTri[j] = 0;
      } else {
        if (validTri.length()) validTri[j] = 1;
      }
    }
    pframe = (mframe_t *)((vuint8 *)pframe+pmodel->framesize);
  }

  if (pmodel->numframes == 1 && validTri.length()) {
    // rebuild triangle indicies, why not
    if (hadError) {
      VMeshFrame &Frame = mod->Frames[0];
      TArray<VMeshTri> NewTris; // vetex indicies
      Frame.TriCount = 0;
      for (unsigned j = 0; j < pmodel->numtris; ++j) {
        if (validTri[j]) {
          NewTris.append(mod->Tris[j]);
          ++Frame.TriCount;
        }
      }
      if (Frame.TriCount == 0) Sys_Error("model %s has no valid triangles", *mod->Name);
      // replace index array
      mod->Tris.setLength(NewTris.length());
      memcpy(mod->Tris.ptr(), NewTris.ptr(), NewTris.length()*sizeof(VMeshTri));
      pmodel->numtris = Frame.TriCount;
      if (showError) {
        GCon->Logf(NAME_Warning, "Alias model '%s' has %d degenerate triangles out of %u! model rebuilt.", *mod->Name, triIgnored, pmodel->numtris);
      }
      // rebuild edges
      mod->Edges.SetNum(0);
      for (unsigned i = 0; i < pmodel->numtris; ++i) {
        for (unsigned j = 0; j < 3; ++j) {
          //AddEdge(Edges, mod->Tris[i].VertIndex[j], ptri[i].vertindex[j], mod->Tris[i].VertIndex[(j+1)%3], ptri[i].vertindex[(j+1)%3], i);
          AddEdge(Edges, mod->Tris[i].VertIndex[j], mod->Tris[i].VertIndex[(j+1)%3], i);
        }
      }
    }
  } else {
    if (hadError && showError) {
      GCon->Logf(NAME_Warning, "Alias model '%s' has %d degenerate triangles out of %u!", *mod->Name, triIgnored, pmodel->numtris);
    }
  }

  // if there were some errors, disable shadows for this model, it is probably broken anyway
  mod->HadErrors = hadError;

  // store edges
  mod->Edges.SetNum(Edges.Num());
  for (int i = 0; i < Edges.Num(); ++i) {
    mod->Edges[i].Vert1 = Edges[i].Vert1;
    mod->Edges[i].Vert2 = Edges[i].Vert2;
    mod->Edges[i].Tri1 = Edges[i].Tri1;
    mod->Edges[i].Tri2 = Edges[i].Tri2;
  }

  /*
  for (int i = 0; i < Edges.Num(); ++i) {
    //mod->Edges[i].Vert1 = Edges[i].Vert1;
    //mod->Edges[i].Vert2 = Edges[i].Vert2;
    mod->Edges[i].Tri2 = -1;
  }
  */

  // commands
  /*
  vint32 *pcmds = (vint32 *)((vuint8 *)pmodel+pmodel->ofscmds);
  for (unsigned i = 0; i < pmodel->numcmds; ++i) pcmds[i] = LittleLong(pcmds[i]);
  */

  // skins
  mskin_t *pskindesc = (mskin_t *)((vuint8 *)pmodel+pmodel->ofsskins);
  for (unsigned i = 0; i < pmodel->numskins; ++i) {
    //mod->Skins.Append(*getStrZ(pskindesc[i].name, 64).ToLower());
    VStr name = getStrZ(pskindesc[i].name, 64).toLowerCase();
    // prepend model path
    if (!name.isEmpty()) name = mod->Name.ExtractFilePath()+name.ExtractFileBaseName();
    //GCon->Logf("model '%s' has skin #%u '%s'", *mod->Name, i, *name);
    VMeshModel::SkinInfo &si = mod->Skins.alloc();
    si.fileName = *name;
    si.textureId = -1;
    si.shade = -1;
  }

  mod->loaded = true;
}


//==========================================================================
//
//  Mod_BuildFramesMD3
//
//==========================================================================
static void Mod_BuildFramesMD3 (VMeshModel *mod, vuint8 *Data) {
  mod->Uploaded = false;
  mod->VertsBuffer = 0;
  mod->IndexBuffer = 0;

  // endian-adjust and swap the data, starting with the alias model header
  MD3Header *pmodel = (MD3Header *)Data;
  pmodel->ver = LittleLong(pmodel->ver);
  pmodel->flags = LittleLong(pmodel->flags);
  pmodel->frameNum = LittleLong(pmodel->frameNum);
  pmodel->tagNum = LittleLong(pmodel->tagNum);
  pmodel->surfaceNum = LittleLong(pmodel->surfaceNum);
  pmodel->skinNum = LittleLong(pmodel->skinNum);
  pmodel->frameOfs = LittleLong(pmodel->frameOfs);
  pmodel->tagOfs = LittleLong(pmodel->tagOfs);
  pmodel->surfaceOfs = LittleLong(pmodel->surfaceOfs);
  pmodel->eofOfs = LittleLong(pmodel->eofOfs);

  if (pmodel->ver != MD3_VERSION) Sys_Error("model '%s' has wrong version number (%u should be %i)", *mod->Name, pmodel->ver, MD3_VERSION);
  if (pmodel->frameNum < 1 || pmodel->frameNum > 1024) Sys_Error("model '%s' has invalid numebr of frames: %u", *mod->Name, pmodel->frameNum);
  if (pmodel->surfaceNum < 1) Sys_Error("model '%s' has no meshes", *mod->Name);
  if ((unsigned)mod->MeshIndex >= pmodel->surfaceNum) Sys_Error("model '%s' has no mesh with index %d", *mod->Name, mod->MeshIndex);
  //if (pmodel->surfaceNum > 1) GCon->Logf(NAME_Warning, "model '%s' has more than one mesh (%u); ignoring extra meshes", *mod->Name, pmodel->surfaceNum);

  // convert frame data
  MD3Frame *pframe = (MD3Frame *)((vuint8 *)pmodel+pmodel->frameOfs);
  for (unsigned i = 0; i < pmodel->frameNum; ++i) {
    for (unsigned f = 0; f < 3; ++f) {
      pframe[i].bmin[f] = LittleFloat(pframe[i].bmin[f]);
      pframe[i].bmax[f] = LittleFloat(pframe[i].bmax[f]);
      pframe[i].origin[f] = LittleFloat(pframe[i].origin[f]);
    }
    pframe[i].radius = LittleFloat(pframe[i].radius);
  }


  // load first mesh
  MD3Surface *pmesh = (MD3Surface *)(Data+pmodel->surfaceOfs);

  // skip to relevant mesh
  for (int f = 0; f < mod->MeshIndex; ++f) {
    if (memcmp(pmesh->sign, "IDP3", 4) != 0) Sys_Error("model '%s' has invalid mesh signature", *mod->Name);
    pmesh->flags = LittleLong(pmesh->flags);
    pmesh->frameNum = LittleLong(pmesh->frameNum);
    pmesh->shaderNum = LittleLong(pmesh->shaderNum);
    pmesh->vertNum = LittleLong(pmesh->vertNum);
    pmesh->triNum = LittleLong(pmesh->triNum);
    pmesh->triOfs = LittleLong(pmesh->triOfs);
    pmesh->shaderOfs = LittleLong(pmesh->shaderOfs);
    pmesh->stOfs = LittleLong(pmesh->stOfs);
    pmesh->vertOfs = LittleLong(pmesh->vertOfs);
    pmesh->endOfs = LittleLong(pmesh->endOfs);

    if (pmesh->shaderNum < 1 || pmesh->shaderNum > 1024) Sys_Error("model '%s' has invalid number of shaders: %u", *mod->Name, pmesh->shaderNum);
    if (pmesh->frameNum != pmodel->frameNum) Sys_Error("model '%s' has mismatched number of frames in mesh", *mod->Name);
    if (pmesh->vertNum < 1) Sys_Error("model '%s' has no vertices", *mod->Name);
    if (pmesh->vertNum > MAXALIASVERTS) Sys_Error("model '%s' has too many vertices", *mod->Name);
    if (pmesh->triNum < 1) Sys_Error("model '%s' has no triangles", *mod->Name);
    if (pmesh->triNum > 65536) Sys_Error("model '%s' has too many triangles", *mod->Name);

    pmesh = (MD3Surface *)((vuint8 *)pmesh+pmesh->endOfs);
  }

  // convert and copy shader data
  MD3Shader *pshader = (MD3Shader *)((vuint8 *)pmesh+pmesh->shaderOfs);
  for (unsigned i = 0; i < pmesh->shaderNum; ++i) {
    pshader[i].index = LittleLong(pshader[i].index);
    VStr name = getStrZ(pshader[i].name, 64).toLowerCase();
    // prepend model path
    if (!name.isEmpty()) name = mod->Name.ExtractFilePath()+name.ExtractFileBaseName();
    //GCon->Logf("SKIN: %s", *name);
    VMeshModel::SkinInfo &si = mod->Skins.alloc();
    si.fileName = *name;
    si.textureId = -1;
    si.shade = -1;
  }

  // convert S and T (texture coordinates)
  MD3ST *pstverts = (MD3ST *)((vuint8 *)pmesh+pmesh->stOfs);
  for (unsigned i = 0; i < pmesh->vertNum; ++i) {
    pstverts[i].s = LittleFloat(pstverts[i].s);
    pstverts[i].t = LittleFloat(pstverts[i].t);
  }

  // convert vertex data
  MD3Vertex *pverts = (MD3Vertex *)((vuint8 *)pmesh+pmesh->vertOfs);
  for (unsigned i = 0; i < pmesh->vertNum*pmodel->frameNum; ++i) {
    pverts[i].x = LittleShort(pverts[i].x);
    pverts[i].y = LittleShort(pverts[i].y);
    pverts[i].z = LittleShort(pverts[i].z);
    pverts[i].normal = LittleShort(pverts[i].normal);
  }

  // convert triangle data
  MD3Tri *ptri = (MD3Tri *)((vuint8 *)pmesh+pmesh->triOfs);
  for (unsigned i = 0; i < pmesh->triNum; ++i) {
    ptri[i].v0 = LittleLong(ptri[i].v0);
    ptri[i].v1 = LittleLong(ptri[i].v1);
    ptri[i].v2 = LittleLong(ptri[i].v2);
    if (ptri[i].v0 >= pmesh->vertNum || ptri[i].v1 >= pmesh->vertNum || ptri[i].v2 >= pmesh->vertNum) Sys_Error("model '%s' has invalid vertex index in triangle #%u", *mod->Name, i);
  }

  // copy texture coordinates
  mod->STVerts.setLength((int)pmesh->vertNum);
  for (unsigned i = 0; i < pmesh->vertNum; ++i) {
    mod->STVerts[i].S = pstverts[i].s;
    mod->STVerts[i].T = pstverts[i].t;
  }

  // copy triangles, create edges
  TArray<VTempEdge> Edges;
  mod->Tris.setLength(pmesh->triNum);
  for (unsigned i = 0; i < pmesh->triNum; ++i) {
    mod->Tris[i].VertIndex[0] = ptri[i].v0;
    mod->Tris[i].VertIndex[1] = ptri[i].v1;
    mod->Tris[i].VertIndex[2] = ptri[i].v2;
    for (unsigned j = 0; j < 3; ++j) {
      AddEdge(Edges, mod->Tris[i].VertIndex[j], mod->Tris[i].VertIndex[(j+1)%3], i);
    }
  }

  // copy vertices
  mod->AllVerts.setLength(pmodel->frameNum*pmesh->vertNum);
  mod->AllNormals.setLength(pmodel->frameNum*pmesh->vertNum);
  for (unsigned i = 0; i < pmesh->vertNum*pmodel->frameNum; ++i) {
    mod->AllVerts[i] = md3vert(pverts+i);
    mod->AllNormals[i] = md3vertNormal(pverts+i);
  }

  // frames
  bool hadError = false;
  bool showError = true;

  // if we have only one frame, and that frame has invalid triangles, just rebuild it
  TArray<vuint8> validTri;
  if (pmodel->frameNum == 1) {
    validTri.setLength((int)pmesh->triNum);
    memset(validTri.ptr(), 0, pmesh->triNum);
  }

  mod->Frames.setLength(pmodel->frameNum);
  mod->AllPlanes.setLength(pmodel->frameNum*pmesh->triNum);

  int triIgnored = 0;
  for (unsigned i = 0; i < pmodel->frameNum; ++i, ++pframe) {
    VMeshFrame &Frame = mod->Frames[i];
    Frame.Name = getStrZ(pframe->name, 16);
    Frame.Scale = TVec(1.0f, 1.0f, 1.0f);
    Frame.Origin = TVec(pframe->origin[0], pframe->origin[1], pframe->origin[2]);
    Frame.Verts = &mod->AllVerts[i*pmesh->vertNum];
    Frame.Normals = &mod->AllNormals[i*pmesh->vertNum];
    Frame.Planes = &mod->AllPlanes[i*pmesh->triNum];
    Frame.VertsOffset = 0;
    Frame.NormalsOffset = 0;
    Frame.TriCount = pmesh->triNum;

    // process triangles
    for (unsigned j = 0; j < pmesh->triNum; ++j) {
      TVec PlaneNormal;
      TVec v3(0, 0, 0);
      bool reported = false, hacked = false;
      for (int vnn = 0; vnn < 3; ++vnn) {
        TVec v1 = Frame.Verts[mod->Tris[j].VertIndex[(vnn+0)%3]];
        TVec v2 = Frame.Verts[mod->Tris[j].VertIndex[(vnn+1)%3]];
             v3 = Frame.Verts[mod->Tris[j].VertIndex[(vnn+2)%3]];

        TVec d1 = v2-v3;
        TVec d2 = v1-v3;
        PlaneNormal = CrossProduct(d1, d2);
        if (lengthSquared(PlaneNormal) == 0) {
          //k8:hack!
          if (mdl_report_errors && !reported) {
            GCon->Logf("Alias model '%s' has degenerate triangle %d; v1=(%f,%f,%f), v2=(%f,%f,%f); v3=(%f,%f,%f); d1=(%f,%f,%f); d2=(%f,%f,%f); cross=(%f,%f,%f)",
              *mod->Name, j, v1.x, v1.y, v1.z, v2.x, v2.y, v2.z, v3.x, v3.y, v3.z, d1.x, d1.y, d1.z, d2.x, d2.y, d2.z, PlaneNormal.x, PlaneNormal.y, PlaneNormal.z);
          }
          reported = true;
        } else {
          hacked = (vnn != 0);
          break;
        }
      }
      if (mdl_report_errors && reported) {
        if (hacked) GCon->Log("  hacked around"); else { GCon->Log("  CANNOT FIX"); PlaneNormal = TVec(0, 0, 1); }
      }
      hadError = hadError || reported;
      PlaneNormal = Normalise(PlaneNormal);
      const float PlaneDist = DotProduct(PlaneNormal, v3);
      Frame.Planes[j].Set(PlaneNormal, PlaneDist);
      if (reported) {
        ++triIgnored;
        if (mdl_report_errors) GCon->Logf("  triangle #%u is ignored", j);
        if (validTri.length()) validTri[j] = 0;
      } else {
        if (validTri.length()) validTri[j] = 1;
      }
    }
  }

  if (pmodel->frameNum == 1 && validTri.length()) {
    // rebuild triangle indicies, why not
    if (hadError) {
      VMeshFrame &Frame = mod->Frames[0];
      TArray<VMeshTri> NewTris; // vetex indicies
      Frame.TriCount = 0;
      for (unsigned j = 0; j < pmesh->triNum; ++j) {
        if (validTri[j]) {
          NewTris.append(mod->Tris[j]);
          ++Frame.TriCount;
        }
      }
      if (Frame.TriCount == 0) Sys_Error("model %s has no valid triangles", *mod->Name);
      // replace index array
      mod->Tris.setLength(NewTris.length());
      memcpy(mod->Tris.ptr(), NewTris.ptr(), NewTris.length()*sizeof(VMeshTri));
      pmesh->triNum = Frame.TriCount;
      if (showError) {
        GCon->Logf(NAME_Warning, "Alias model '%s' has %d degenerate triangles out of %u! model rebuilt.", *mod->Name, triIgnored, pmesh->triNum);
      }
      // rebuild edges
      mod->Edges.setLength(0);
      for (unsigned i = 0; i < pmesh->triNum; ++i) {
        for (unsigned j = 0; j < 3; ++j) {
          //AddEdge(Edges, mod->Tris[i].VertIndex[j], ptri[i].vertindex[j], mod->Tris[i].VertIndex[(j+1)%3], ptri[i].vertindex[(j+1)%3], i);
          AddEdge(Edges, mod->Tris[i].VertIndex[j], mod->Tris[i].VertIndex[(j+1)%3], i);
        }
      }
    }
  } else {
    if (hadError && showError) {
      GCon->Logf(NAME_Warning, "Alias model '%s' has %d degenerate triangles out of %u!", *mod->Name, triIgnored, pmesh->triNum);
    }
  }

  // if there were some errors, disable shadows for this model, it is probably broken anyway
  mod->HadErrors = hadError;

  // store edges
  mod->Edges.setLength(Edges.Num());
  for (int i = 0; i < Edges.Num(); ++i) {
    mod->Edges[i].Vert1 = Edges[i].Vert1;
    mod->Edges[i].Vert2 = Edges[i].Vert2;
    mod->Edges[i].Tri1 = Edges[i].Tri1;
    mod->Edges[i].Tri2 = Edges[i].Tri2;
  }

  /*
  for (int i = 0; i < Edges.Num(); ++i) {
    //mod->Edges[i].Vert1 = Edges[i].Vert1;
    //mod->Edges[i].Vert2 = Edges[i].Vert2;
    mod->Edges[i].Tri2 = -1;
  }
  */

  mod->loaded = true;
}


//==========================================================================
//
//  Mod_ParseModel
//
//  Loads the data if needed
//
//==========================================================================
static void Mod_ParseModel (VMeshModel *mod) {
  if (mod->loaded) return;

  // load the file
  VStream *Strm = FL_OpenFileRead(mod->Name);
  if (!Strm) Sys_Error("Couldn't load %s", *mod->Name);

  vuint8 *Data = (vuint8 *)Z_Malloc(Strm->TotalSize());
  Strm->Serialise(Data, Strm->TotalSize());
  delete Strm;

  if (LittleLong(*(vuint32 *)Data) == IDPOLY2HEADER) {
    // swap model
    Mod_BuildFrames(mod, Data);
  } else if (LittleLong(*(vuint32 *)Data) == IDPOLY3HEADER) {
    // swap model
    Mod_BuildFramesMD3(mod, Data);
  } else {
    Sys_Error("model %s is not an md2/md3 model", *mod->Name);
  }

  Z_Free(Data);
}


//==========================================================================
//
//  PositionModel
//
//==========================================================================
static void PositionModel (TVec &Origin, TAVec &Angles, VMeshModel *wpmodel, int InFrame) {
  /*mmdl_t *pmdl = (mmdl_t *)*/Mod_ParseModel(wpmodel);
  unsigned frame = (unsigned)InFrame;
  if (frame >= /*pmdl->numframes*/(unsigned)wpmodel->Frames.length()) frame = 0;
  TVec p[3];
/*
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
//  FindFrame
//
//==========================================================================
static int FindFrame (VClassModelScript &Cls, const VAliasModelFrameInfo &Frame, float Inter) {
  // try cached frames
  if (Cls.OneForAll) return 0; // guaranteed

  if (!Cls.CacheBuilt) {
    for (int i = 0; i < Cls.Frames.Num(); ++i) {
      VScriptedModelFrame &frm = Cls.Frames[i];
      frm.nextSpriteIdx = frm.nextNumberIdx = -1;
      // sprite cache
      if (frm.sprite != NAME_None) {
        //FIXME: sanity checks
        check(frm.frame >= 0 && frm.frame < 4096);
        check(frm.sprite.GetIndex() > 0 && frm.sprite.GetIndex() < 524288);
        vuint32 nfi = SprNameFrameToInt(frm.sprite, frm.frame);
        if (!Cls.SprFrameMap.has(nfi)) {
          // new one
          Cls.SprFrameMap.put(nfi, i);
        } else {
          // add to list
          int idx = *Cls.SprFrameMap.get(nfi);
          while (Cls.Frames[idx].nextSpriteIdx != -1) idx = Cls.Frames[idx].nextSpriteIdx;
          Cls.Frames[idx].nextSpriteIdx = i;
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

#if 0
  int Ret = -1;
  int frameAny = -1;

  for (int i = 0; i < Cls.Frames.Num(); ++i) {
    const VScriptedModelFrame &frm = Cls.Frames[i];
    if (frm.Inter <= Inter) {
      if (frm.sprite != NAME_None) {
        //GCon->Logf("*** CHECKING '%s' [%c]  ('%s' [%c])", *frm.sprite, 'A'+frm.frame, *Frame.sprite, 'A'+Frame.frame);
        if (frm.sprite == Frame.sprite && frm.frame == Frame.frame) {
          Ret = i;
          // k8: no `break` here, 'cause we may find better frame (with better "inter")
          //GCon->Logf("+++ ALIASMDL: found frame '%s' [%c]", *Frame.sprite, 'A'+Frame.frame);
        }
      } else if (frm.Number == Frame.index) {
        Ret = i;
        // k8: no `break` here, 'cause we may find better frame (with better "inter")
      }
    }
    //k8: frame "-666" means "any"
    if (frameAny < 0 && Ret < 0 && frm.Number == -666) frameAny = i;
  }
  if (Ret == -1 && frameAny >= 0) return frameAny;
  return Ret;
#else
  int res = -1;
  // by index
  if (Frame.index >= 0) {
    int *idxp = Cls.NumFrameMap.find(Frame.index);
    if (idxp) {
      int idx = *idxp;
      while (idx != -1) {
        const VScriptedModelFrame &frm = Cls.Frames[idx];
             if (frm.Inter <= Inter) res = idx;
        else if (frm.Inter > Inter) break; // the author shouldn't write incorrect defs
        idx = frm.nextNumberIdx;
      }
    }
  }
  if (res >= 0) return res;
  // by sprite name
  if (Frame.sprite != NAME_None && Frame.frame >= 0 && Frame.frame < 4096) {
    int *idxp = Cls.SprFrameMap.find(SprNameFrameToInt(Frame.sprite, Frame.frame));
    if (idxp) {
      int idx = *idxp;
      while (idx != -1) {
        const VScriptedModelFrame &frm = Cls.Frames[idx];
             if (frm.Inter <= Inter) res = idx;
        else if (frm.Inter > Inter) break; // the author shouldn't write incorrect defs
        idx = frm.nextSpriteIdx;
      }
    }
  }
  return res;
#endif
}


//==========================================================================
//
//  FindNextFrame
//
//==========================================================================
static int FindNextFrame (VClassModelScript &Cls, int FIdx, const VAliasModelFrameInfo &Frame, float Inter, float &InterpFrac) {
  if (FIdx < 0) return -1; // just in case
  const VScriptedModelFrame &FDef = Cls.Frames[FIdx];
  if (FIdx < Cls.Frames.Num()-1) {
    const VScriptedModelFrame &frm = Cls.Frames[FIdx+1];
    if (FDef.sprite != NAME_None) {
      if (frm.sprite == FDef.sprite && frm.frame == FDef.frame) {
        InterpFrac = (Inter-FDef.Inter)/(frm.Inter-FDef.Inter);
        return FIdx+1;
      }
    } else if (frm.Number == FDef.Number) {
      InterpFrac = (Inter-FDef.Inter)/(Cls.Frames[FIdx+1].Inter-FDef.Inter);
      return FIdx+1;
    }
  }
  InterpFrac = (Inter-FDef.Inter)/(1.0f-FDef.Inter);
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
      /*mmdl_t *pmdl = (mmdl_t *)*/Mod_ParseModel(SubMdl.Model);
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
  bool Interpolate, const TVec &LightPos, float LightRadius, ERenderPass Pass, bool isAdvanced)
{
  VScriptedModelFrame &FDef = Cls.Frames[FIdx];
  VScriptedModelFrame &NFDef = Cls.Frames[NFIdx];
  VScriptModel &ScMdl = Cls.Model->Models[FDef.ModelIndex];
  for (int i = 0; i < ScMdl.SubModels.Num(); ++i) {
    VScriptSubModel &SubMdl = ScMdl.SubModels[i];
    if (SubMdl.Version != -1 && SubMdl.Version != Version) continue;
    if (FDef.FrameIndex >= SubMdl.Frames.Num()) {
      GCon->Logf("Bad sub-model frame index %d", FDef.FrameIndex);
      continue;
    }
    if (Interpolate && NFDef.FrameIndex >= SubMdl.Frames.Num() && NFDef.ModelIndex != FDef.ModelIndex) {
      NFDef.FrameIndex = FDef.FrameIndex;
      Interpolate = false;
      continue;
    }
    if (Interpolate && FDef.ModelIndex != NFDef.ModelIndex) Interpolate = false;
    if (NFDef.FrameIndex >= SubMdl.Frames.Num()) continue;
    VScriptSubModel::VFrame &F = SubMdl.Frames[FDef.FrameIndex];
    VScriptSubModel::VFrame &NF = SubMdl.Frames[NFDef.FrameIndex];

    // locate the proper data
    /*mmdl_t *pmdl = (mmdl_t *)*/Mod_ParseModel(SubMdl.Model);
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
    int Md2NextFrame = NF.Index;
    if ((unsigned)Md2NextFrame >= (unsigned)SubMdl.Model->Frames.length()) {
      if (developer) GCon->Logf(NAME_Dev, "no such next frame %d in model '%s'", Md2NextFrame, *SubMdl.Model->Name);
      Md2NextFrame = (Md2NextFrame <= 0 ? 0 : SubMdl.Model->Frames.length()-1);
      // stop further warnings
      NF.Index = Md2NextFrame;
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

    // scale, in case of models thing's ScaleX scales x and y and ScaleY scales z
    TVec Scale;
    if (Interpolate) {
      // interpolate scale
      Scale.x = (F.Scale.x+smooth_inter*(NF.Scale.x-F.Scale.x))*ScaleX;
      Scale.y = (F.Scale.y+smooth_inter*(NF.Scale.y-F.Scale.y))*ScaleX;
      Scale.z = (F.Scale.z+smooth_inter*(NF.Scale.z-F.Scale.z))*ScaleY;
    } else {
      Scale.x = F.Scale.x*ScaleX;
      Scale.y = F.Scale.y*ScaleX;
      Scale.z = F.Scale.z*ScaleY;
    }

    TVec Offset;
    if (Interpolate) {
      // interpolate offsets too
      Offset.x = ((1-smooth_inter)*F.Offset.x+smooth_inter*NF.Offset.x);
      Offset.y = ((1-smooth_inter)*F.Offset.y+smooth_inter*NF.Offset.y);
      Offset.z = ((1-smooth_inter)*F.Offset.z+smooth_inter*NF.Offset.z);
    } else {
      Offset.x = F.Offset.x;
      Offset.y = F.Offset.y;
      Offset.z = F.Offset.z;
    }

    // light
    vuint32 Md2Light = Light;
    if (SubMdl.FullBright) Md2Light = 0xffffffff;

    //if (Pass != RPASS_NonShadow) return;
    //if (Pass != RPASS_Ambient) return;

    switch (Pass) {
      case RPASS_Normal:
      case RPASS_NonShadow:
        if (true /*IsViewModel || !isAdvanced*/) {
          Drawer->DrawAliasModel(Md2Org, Md2Angle, Offset, Scale,
            SubMdl.Model, Md2Frame, Md2NextFrame, GTextureManager(SkinID),
            Trans, ColorMap, Md2Light, Fade, Md2Alpha, Additive,
            IsViewModel, smooth_inter, Interpolate, SubMdl.UseDepth,
            SubMdl.AllowTransparency,
            !IsViewModel && isAdvanced); // for advanced renderer, we need to fill z-buffer, but not color buffer
        }
        break;
      case RPASS_Ambient:
        if (!SubMdl.AllowTransparency)
          Drawer->DrawAliasModelAmbient(Md2Org, Md2Angle, Offset, Scale,
            SubMdl.Model, Md2Frame, Md2NextFrame, GTextureManager(SkinID),
            Md2Light, Md2Alpha, smooth_inter, Interpolate, SubMdl.UseDepth,
            SubMdl.AllowTransparency);
        break;
      case RPASS_ShadowVolumes:
        Drawer->DrawAliasModelShadow(Md2Org, Md2Angle, Offset, Scale,
          SubMdl.Model, Md2Frame, Md2NextFrame, smooth_inter, Interpolate,
          LightPos, LightRadius);
        break;
      case RPASS_Light:
        Drawer->DrawAliasModelLight(Md2Org, Md2Angle, Offset, Scale,
          SubMdl.Model, Md2Frame, Md2NextFrame, GTextureManager(SkinID),
          Md2Alpha, smooth_inter, Interpolate, SubMdl.AllowTransparency);
        break;
      case RPASS_Textures:
        Drawer->DrawAliasModelTextures(Md2Org, Md2Angle, Offset, Scale,
          SubMdl.Model, Md2Frame, Md2NextFrame, GTextureManager(SkinID),
          Trans, ColorMap, Md2Alpha, smooth_inter, Interpolate, SubMdl.UseDepth,
          SubMdl.AllowTransparency);
        break;
      case RPASS_Fog:
        Drawer->DrawAliasModelFog(Md2Org, Md2Angle, Offset, Scale,
          SubMdl.Model, Md2Frame, Md2NextFrame, GTextureManager(SkinID),
          Fade, Md2Alpha, smooth_inter, Interpolate, SubMdl.AllowTransparency);
        break;
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::DrawAliasModel
//
//==========================================================================
bool VRenderLevelShared::DrawAliasModel (VEntity *mobj, const TVec &Org, const TAVec &Angles,
  float ScaleX, float ScaleY, VModel *Mdl,
  const VAliasModelFrameInfo &Frame, const VAliasModelFrameInfo &NextFrame,
  VTextureTranslation *Trans, int Version, vuint32 Light, vuint32 Fade,
  float Alpha, bool Additive, bool IsViewModel, float Inter, bool Interpolate,
  ERenderPass Pass)
{
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
    Pass, IsAdvancedRenderer());
  return true;
}


//==========================================================================
//
//  VRenderLevelShared::HasAliasModel
//
//==========================================================================
bool VRenderLevelShared::HasAliasModel (VName clsName) const {
  if (clsName == NAME_None) return false;
  VClassModelScript *Cls = FindClassModelByName(clsName);
  return !!Cls;
}


//==========================================================================
//
//  VRenderLevelShared::DrawAliasModel
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

  VClassModelScript *Cls = FindClassModelByName(clsName);
  if (!Cls) return false;

  int FIdx = FindFrame(*Cls, /*State->getMFI()*/Frame, Inter);
  if (FIdx == -1) return false;

  float InterpFrac;
  int NFIdx = FindNextFrame(*Cls, FIdx, /*NextState->getMFI()*/NextFrame, Inter, InterpFrac);
  if (NFIdx == -1) {
    NFIdx = FIdx;
    Interpolate = false;
  }

  DrawModel(Level, mobj, Org, Angles, ScaleX, ScaleY, *Cls, FIdx, NFIdx, Trans,
    ColorMap, Version, Light, Fade, Alpha, Additive, IsViewModel,
    InterpFrac, Interpolate, CurrLightPos, CurrLightRadius, Pass, IsAdvancedRenderer());
  return true;
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
    if (!FL_FileExists(VStr("models/")+Ent->FixedModelName)) {
      GCon->Logf("Can't find %s", *Ent->FixedModelName);
      return false;
    }
    VModel *Mdl = Mod_FindName(VStr("models/")+Ent->FixedModelName);
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
    if (!FL_FileExists(VStr("models/")+Ent->FixedModelName)) return false;
    VModel *Mdl = Mod_FindName(VStr("models/")+Ent->FixedModelName);
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
  rd.width = ScreenWidth;
  rd.height = ScreenHeight;
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
  rd.width = ScreenWidth;
  rd.height = ScreenHeight;
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
