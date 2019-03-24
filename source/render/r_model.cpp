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


// ////////////////////////////////////////////////////////////////////////// //
// RR GG BB or -1
static int parseHexRGB (const VStr &str) {
  vuint32 ppc = M_ParseColour(*str);
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
  };

  VMeshModel *Model;
  VMeshModel *PositionModel;
  int SkinAnimSpeed;
  int SkinAnimRange;
  int Version;
  TArray<VFrame> Frames;
  TArray<VName> Skins;
  TArray<int> SkinShades;
  bool FullBright;
  bool NoShadow;
  bool UseDepth;
  bool AllowTransparency;
};


struct VScriptModel {
  VName Name;
  TArray<VScriptSubModel> SubModels;
};


struct VScriptedModelFrame {
  int Number;
  float Inter;
  int ModelIndex;
  int FrameIndex;
  float AngleStart;
  float AngleEnd;
  float AlphaStart;
  float AlphaEnd;
  bool hasYaw;
  float angleYaw;
  bool hasRoll;
  float angleRoll;
  bool hasPitch;
  float anglePitch;
  //
  VName sprite;
  int frame; // sprite frame
};


struct VClassModelScript {
  VName Name;
  VModel *Model;
  TArray<VScriptedModelFrame> Frames;
};


struct VModel {
  VStr Name;
  TArray<VScriptModel> Models;
  VClassModelScript *DefaultClass;
};


struct TVertMap {
  int VertIndex;
  int STIndex;
};


struct VTempEdge {
  vuint16 Vert1;
  vuint16 Vert2;
  //vuint16 OrigVert1;
  //vuint16 OrigVert2;
  vint16 Tri1;
  vint16 Tri2;
};


// precalculated dot products for quantized angles
enum { SHADEDOT_QUANT = 16 };
static const float r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anorm_dots.h"
;


static TArray<VModel *> mod_known;
static TArray<VMeshModel *> GMeshModels;
static TArray<VClassModelScript *> ClassModels;

static const float r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};


//==========================================================================
//
//  FindClassModelByName
//
//==========================================================================
static VClassModelScript *FindClassModelByName (VName clsName) {
  static TMapNC<VName, VClassModelScript *> mdlmap;
  static int initlen = -1;
  if (initlen != ClassModels.length()) {
    initlen = ClassModels.length();
    // build map
    for (int f = 0; f < ClassModels.length(); ++f) {
      VClassModelScript *mdl = ClassModels[f];
      if (mdl->Name == NAME_None || !mdl->Model || mdl->Frames.length() == 0) continue;
      mdlmap.put(mdl->Name, mdl);
    }
  }
  auto mp = mdlmap.find(clsName);
  return (mp ? *mp : nullptr);
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
    if (GMeshModels[i]->Data) Z_Free(GMeshModels[i]->Data);
    delete GMeshModels[i];
    GMeshModels[i] = nullptr;
  }
  GMeshModels.Clear();

  for (int i = 0; i < ClassModels.Num(); ++i) {
    delete ClassModels[i];
    ClassModels[i] = nullptr;
  }
  ClassModels.Clear();
}


//==========================================================================
//
//  Mod_FindMeshModel
//
//==========================================================================
static VMeshModel *Mod_FindMeshModel (const VStr &name) {
  if (name.IsEmpty()) Sys_Error("Mod_ForName: nullptr name");

  // search the currently loaded models
  for (int i = 0; i < GMeshModels.Num(); ++i) {
    if (GMeshModels[i]->Name == name) return GMeshModels[i];
  }

  VMeshModel *mod = new VMeshModel();
  mod->Name = name;
  mod->Data = nullptr;
  GMeshModels.Append(mod);

  return mod;
}


//==========================================================================
//
//  ParseModelScript
//
//==========================================================================
static void ParseModelScript (VModel *Mdl, VStream &Strm) {
  // parse xml file
  VXmlDocument *Doc = new VXmlDocument();
  Doc->Parse(Strm, Mdl->Name);

  // verify that it's a model definition file
  if (Doc->Root.Name != "vavoom_model_definition") Sys_Error("%s is not a valid model definition file", *Mdl->Name);

  Mdl->DefaultClass = nullptr;

  // process model definitions
  for (VXmlNode *N = Doc->Root.FindChild("model"); N; N = N->FindNext()) {
    VScriptModel &SMdl = Mdl->Models.Alloc();
    SMdl.Name = *N->GetAttribute("name");

    // process model parts
    for (VXmlNode *SN = N->FindChild("md2"); SN; SN = SN->FindNext()) {
      VScriptSubModel &Md2 = SMdl.SubModels.Alloc();
      Md2.Model = Mod_FindMeshModel(SN->GetAttribute("file").ToLower().FixFileSlashes());

      // version
      Md2.Version = -1;
      if (SN->HasAttribute("version")) Md2.Version = atoi(*SN->GetAttribute("version"));

      // position model
      Md2.PositionModel = nullptr;
      if (SN->HasAttribute("position_file")) {
        Md2.PositionModel = Mod_FindMeshModel(SN->GetAttribute("position_file").ToLower().FixFileSlashes());
      }

      // skin animation
      Md2.SkinAnimSpeed = 0;
      Md2.SkinAnimRange = 0;
      if (SN->HasAttribute("skin_anim_speed")) {
        Md2.SkinAnimSpeed = atoi(*SN->GetAttribute("skin_anim_speed"));
        Md2.SkinAnimRange = atoi(*SN->GetAttribute("skin_anim_range"));
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
      Md2.FullBright = false;
      if (SN->HasAttribute("fullbright")) Md2.FullBright = !SN->GetAttribute("fullbright").ICmp("true");

      // no shadow flag
      Md2.NoShadow = false;
      if (SN->HasAttribute("noshadow")) Md2.NoShadow = !SN->GetAttribute("noshadow").ICmp("true");

      // force depth test flag (for things like monsters with alpha transaparency)
      Md2.UseDepth = false;
      if (SN->HasAttribute("usedepth")) Md2.UseDepth = !SN->GetAttribute("usedepth").ICmp("true");

      // allow transparency in skin files
      // for skins that are transparent in solid models (Alpha = 1.0f)
      Md2.AllowTransparency = false;
      if (SN->HasAttribute("allowtransparency")) {
        Md2.AllowTransparency = !SN->GetAttribute("allowtransparency").ICmp("true");
      }

      // process frames
      for (VXmlNode *FN = SN->FindChild("frame"); FN; FN = FN->FindNext()) {
        VScriptSubModel::VFrame &F = Md2.Frames.Alloc();
        F.Index = atoi(*FN->GetAttribute("index"));

        // position model frame index
        F.PositionIndex = 0;
        if (FN->HasAttribute("position_index")) F.PositionIndex = atoi(*FN->GetAttribute("position_index"));

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
        if (FN->HasAttribute("skin_index")) F.SkinIndex = atoi(*FN->GetAttribute("skin_index"));
      }

      // process skins
      for (VXmlNode *SkN = SN->FindChild("skin"); SkN; SkN = SkN->FindNext()) {
        VStr sfl = SkN->GetAttribute("file").ToLower().FixFileSlashes();
        if (sfl.length()) {
          if (mdl_verbose_loading > 2) GCon->Logf("model '%s': skin file '%s'", *SMdl.Name, *sfl);
          Md2.Skins.Append(*sfl);
          int shade = -1;
          if (SkN->HasAttribute("shade")) {
            sfl = SkN->GetAttribute("shade");
            shade = parseHexRGB(sfl);
          }
          Md2.SkinShades.Append(shade);
        }
      }
    }
  }

  bool ClassDefined = false;
  for (VXmlNode *CN = Doc->Root.FindChild("class"); CN; CN = CN->FindNext()) {
    VClassModelScript *Cls = new VClassModelScript();
    Cls->Model = Mdl;
    Cls->Name = *CN->GetAttribute("name");
    if (!Mdl->DefaultClass) Mdl->DefaultClass = Cls;
    ClassModels.Append(Cls);
    ClassDefined = true;
    //GCon->Logf("found model for class '%s'", *Cls->Name);

    // process frames
    for (VXmlNode *N = CN->FindChild("state"); N; N = N->FindNext()) {
      VScriptedModelFrame &F = Cls->Frames.Alloc();

      F.hasYaw = N->HasAttribute("angle_yaw");
      F.hasPitch = N->HasAttribute("angle_pitch");
      F.hasRoll = N->HasAttribute("angle_roll");
      if (F.hasYaw && N->GetAttribute("angle_yaw") == "random") F.angleYaw = AngleMod(360.0f*Random());
      else F.angleYaw = AngleMod(F.hasYaw ? VStr::atof(*N->GetAttribute("angle_yaw")) : 0.0f);
      if (F.hasPitch && N->GetAttribute("angle_pitch") == "random") F.anglePitch = AngleMod(360.0f*Random());
      else F.anglePitch = AngleMod(F.hasPitch ? VStr::atof(*N->GetAttribute("angle_pitch")) : 0.0f);
      if (F.hasRoll && N->GetAttribute("angle_roll") == "random") F.angleRoll = AngleMod(360.0f*Random());
      else F.angleRoll = AngleMod(F.hasRoll ? VStr::atof(*N->GetAttribute("angle_roll")) : 0.0f);

      int lastIndex = -666;
      if (N->HasAttribute("index")) {
        F.Number = atoi(*N->GetAttribute("index"));
        if (N->HasAttribute("last_index")) lastIndex = atoi(*N->GetAttribute("last_index"));
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

      F.FrameIndex = atoi(*N->GetAttribute("frame_index"));

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
          ffr.Number = cfidx;
          ffr.FrameIndex = F.FrameIndex;
          ffr.ModelIndex = F.ModelIndex;
          ffr.Inter = F.Inter;
          ffr.AngleStart = F.AngleStart;
          ffr.AngleEnd = F.AngleEnd;
          ffr.AlphaStart = F.AlphaStart;
          ffr.AlphaEnd = F.AlphaEnd;
          ffr.hasYaw = F.hasYaw;
          ffr.hasPitch = F.hasPitch;
          ffr.hasRoll = F.hasRoll;
          ffr.angleYaw = F.angleYaw;
          ffr.anglePitch = F.anglePitch;
          ffr.angleRoll = F.angleRoll;
        }
      } else {
        if (F.Number < 0 && F.sprite == NAME_None) F.Number = -666;
      }
    }
    if (!Cls->Frames.Num()) Sys_Error("%s class %s has no states defined", *Mdl->Name, *Cls->Name);
  }
  if (!ClassDefined) Sys_Error("%s defined no classes", *Mdl->Name);

  // we don't need the xml file anymore
  delete Doc;
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
//  AddEdge
//
//==========================================================================
static void AddEdge(TArray<VTempEdge> &Edges, int Vert1, int Vert2, int Tri) {
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
//  Mod_SwapAliasModel
//
//==========================================================================
static void Mod_BuildFrames (VMeshModel *mod) {
  mmdl_t *pmodel;
  mstvert_t *pstverts;
  mtriangle_t *ptri;
  mframe_t *pframe;
  vint32 *pcmds;

  pmodel = mod->Data;
  mod->Uploaded = false;
  mod->VertsBuffer = 0;
  mod->IndexBuffer = 0;

  // endian-adjust and swap the data, starting with the alias model header
  for (unsigned i = 0; i < sizeof(mmdl_t)/4; ++i) ((vint32 *)pmodel)[i] = LittleLong(((vint32 *)pmodel)[i]);

  if (pmodel->version != ALIAS_VERSION) Sys_Error("%s has wrong version number (%i should be %i)", *mod->Name, pmodel->version, ALIAS_VERSION);
  if (pmodel->numverts <= 0) Sys_Error("model %s has no vertices", *mod->Name);
  if (pmodel->numverts > MAXALIASVERTS) Sys_Error("model %s has too many vertices", *mod->Name);
  if (pmodel->numstverts <= 0) Sys_Error("model %s has no texture vertices", *mod->Name);
  if (pmodel->numstverts > MAXALIASSTVERTS) Sys_Error("model %s has too many texture vertices", *mod->Name);
  if (pmodel->numtris <= 0) Sys_Error("model %s has no triangles", *mod->Name);
  if (pmodel->skinwidth&0x03) Sys_Error("Mod_LoadAliasModel: skinwidth not multiple of 4");
  if (pmodel->numskins < 1) Sys_Error("Mod_LoadAliasModel: Invalid # of skins: %d\n", pmodel->numskins);
  if (pmodel->numframes < 1) Sys_Error("Mod_LoadAliasModel: Invalid # of frames: %d\n", pmodel->numframes);

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
      /*k8: who cares?
      bool found = false;
      for (int vi = 0; vi < VertMap.Num(); ++vi) {
        if (VertMap[vi].VertIndex == ptri[i].vertindex[j] &&
            VertMap[vi].STIndex == ptri[i].stvertindex[j])
        {
          found = true;
          mod->Tris[i].VertIndex[j] = vi;
          break;
        }
      }
      if (!found) {
        mod->Tris[i].VertIndex[j] = VertMap.Num();
        TVertMap &V = VertMap.Alloc();
        V.VertIndex = ptri[i].vertindex[j];
        V.STIndex = ptri[i].stvertindex[j];
      }
      */
      mod->Tris[i].VertIndex[j] = VertMap.length();
      TVertMap &v = VertMap.alloc();
      v.VertIndex = ptri[i].vertindex[j];
      v.STIndex = ptri[i].stvertindex[j];
    }
    for (unsigned j = 0; j < 3; ++j) {
      //AddEdge(Edges, mod->Tris[i].VertIndex[j], ptri[i].vertindex[j], mod->Tris[i].VertIndex[(j+1)%3], ptri[i].vertindex[(j+1)%3], i);
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

  int triIgonded = 0;
  for (unsigned i = 0; i < pmodel->numframes; ++i) {
    pframe->scale[0] = LittleFloat(pframe->scale[0]);
    pframe->scale[1] = LittleFloat(pframe->scale[1]);
    pframe->scale[2] = LittleFloat(pframe->scale[2]);
    pframe->scale_origin[0] = LittleFloat(pframe->scale_origin[0]);
    pframe->scale_origin[1] = LittleFloat(pframe->scale_origin[1]);
    pframe->scale_origin[2] = LittleFloat(pframe->scale_origin[2]);

    VMeshFrame &Frame = mod->Frames[i];
    Frame.Verts = &mod->AllVerts[i*VertMap.Num()];
    Frame.Normals = &mod->AllNormals[i*VertMap.Num()];
    Frame.Planes = &mod->AllPlanes[i*pmodel->numtris];
    Frame.VertsOffset = 0;
    Frame.NormalsOffset = 0;
    Frame.TriCount = pmodel->numtris;
    Frame.ValidTris.setLength((int)pmodel->numtris);

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
        ++triIgonded;
        if (mdl_report_errors) GCon->Logf("  triangle #%u is ignored", j);
        Frame.ValidTris[j] = 0;
      } else {
        Frame.ValidTris[j] = 1;
      }
    }
    pframe = (mframe_t *)((vuint8 *)pframe+pmodel->framesize);
  }

  if (pmodel->numframes == 1) {
    // rebuild triangle indicies, why not
    if (hadError) {
      VMeshFrame &Frame = mod->Frames[0];
      TArray<VMeshTri> NewTris; // vetex indicies
      Frame.TriCount = 0;
      for (unsigned j = 0; j < pmodel->numtris; ++j) {
        if (Frame.ValidTris[j]) {
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
        GCon->Logf(NAME_Warning, "Alias model '%s' has %d degenerate triangles out of %u! model rebuilt.", *mod->Name, triIgonded, pmodel->numtris);
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
      GCon->Logf(NAME_Warning, "Alias model '%s' has %d degenerate triangles out of %u!", *mod->Name, triIgonded, pmodel->numtris);
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
  pcmds = (vint32 *)((vuint8 *)pmodel+pmodel->ofscmds);
  for (unsigned i = 0; i < pmodel->numcmds; ++i) pcmds[i] = LittleLong(pcmds[i]);

  // skins
  mskin_t *pskindesc = (mskin_t *)((vuint8 *)pmodel+pmodel->ofsskins);
  for (unsigned i = 0; i < pmodel->numskins; ++i) mod->Skins.Append(*VStr(pskindesc[i].name).ToLower());
}


//==========================================================================
//
//  Mod_ParseModel
//
//  Loads the data if needed
//
//==========================================================================
static mmdl_t *Mod_ParseModel (VMeshModel *mod) {
  if (mod->Data) return mod->Data;

  // load the file
  VStream *Strm = FL_OpenFileRead(mod->Name);
  if (!Strm) Sys_Error("Couldn't load %s", *mod->Name);

  mod->Data = (mmdl_t *)Z_Malloc(Strm->TotalSize());
  Strm->Serialise(mod->Data, Strm->TotalSize());
  delete Strm;
  Strm = nullptr;

  if (LittleLong(*(vuint32 *)mod->Data) != IDPOLY2HEADER) {
    Z_Free(mod->Data);
    Sys_Error("model %s is not a md2 model", *mod->Name);
  }

  // swap model
  Mod_BuildFrames(mod);

  return mod->Data;
}


//==========================================================================
//
//  PositionModel
//
//==========================================================================
static void PositionModel (TVec &Origin, TAVec &Angles, VMeshModel *wpmodel, int InFrame) {
  mmdl_t *pmdl = (mmdl_t *)Mod_ParseModel(wpmodel);
  unsigned frame = (unsigned)InFrame;
  if (frame >= pmdl->numframes) frame = 0;
  mtriangle_t *ptris = (mtriangle_t *)((vuint8 *)pmdl+pmdl->ofstris);
  mframe_t *pframe = (mframe_t *)((vuint8 *)pmdl+pmdl->ofsframes+frame*pmdl->framesize);
  trivertx_t *pverts = (trivertx_t *)(pframe+1);
  TVec p[3];
  for (int vi = 0; vi < 3; ++vi) {
    p[vi].x = pverts[ptris[0].vertindex[vi]].v[0]*pframe->scale[0]+pframe->scale_origin[0];
    p[vi].y = pverts[ptris[0].vertindex[vi]].v[1]*pframe->scale[1]+pframe->scale_origin[1];
    p[vi].z = pverts[ptris[0].vertindex[vi]].v[2]*pframe->scale[2]+pframe->scale_origin[2];
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
static int FindFrame (const VClassModelScript &Cls, const VAliasModelFrameInfo &Frame, float Inter) {
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
}


//==========================================================================
//
//  FindNextFrame
//
//==========================================================================
static int FindNextFrame (const VClassModelScript &Cls, int FIdx, const VAliasModelFrameInfo &Frame, float Inter, float &InterpFrac) {
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
//  DrawModel
//
//  FIXME: make this faster -- stop looping, cache data!
//
//==========================================================================
static void DrawModel (VLevel *Level, const TVec &Org, const TAVec &Angles,
  float ScaleX, float ScaleY, VClassModelScript &Cls, int FIdx, int NFIdx,
  VTextureTranslation *Trans, int ColourMap, int Version, vuint32 Light,
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
    mmdl_t *pmdl = (mmdl_t *)Mod_ParseModel(SubMdl.Model);
    //FIXME: this should be done earilier
    if (SubMdl.Model->HadErrors) SubMdl.NoShadow = true;

    // skin aniations
    int Md2SkinIdx = 0;
    if (F.SkinIndex >= 0) {
      Md2SkinIdx = F.SkinIndex;
    } else if (SubMdl.SkinAnimSpeed) {
      Md2SkinIdx = int((Level ? Level->Time : 0)*SubMdl.SkinAnimSpeed)%SubMdl.SkinAnimRange;
    }

    // get the proper skin texture ID
    int SkinID;
    if (SubMdl.Skins.Num()) {
      // skins defined in definition file override all skins in MD2 file
      if (Md2SkinIdx < 0 || Md2SkinIdx >= SubMdl.Skins.Num()) {
        SkinID = GTextureManager.AddFileTextureShaded(SubMdl.Skins[0], TEXTYPE_Skin, SubMdl.SkinShades[0]);
      } else {
        SkinID = GTextureManager.AddFileTextureShaded(SubMdl.Skins[Md2SkinIdx], TEXTYPE_Skin, SubMdl.SkinShades[Md2SkinIdx]);
      }
    } else {
      if ((unsigned)Md2SkinIdx >= pmdl->numskins) {
        SkinID = GTextureManager.AddFileTexture(SubMdl.Model->Skins[0], TEXTYPE_Skin);
      } else {
        SkinID = GTextureManager.AddFileTexture(SubMdl.Model->Skins[Md2SkinIdx], TEXTYPE_Skin);
      }
    }

    // get and verify frame number
    int Md2Frame = F.Index;
    if ((unsigned)Md2Frame >= pmdl->numframes) {
      GCon->Logf(NAME_Dev, "no such frame %d in %s", Md2Frame, *SubMdl.Model->Name);
      Md2Frame = 0;
      // stop further warnings
      F.Index = 0;
    }

    // get and verify next frame number
    int Md2NextFrame = NF.Index;
    if ((unsigned)Md2NextFrame >= pmdl->numframes) {
      GCon->Logf(NAME_Dev, "no such next frame %d in %s", Md2NextFrame, *SubMdl.Model->Name);
      Md2NextFrame = 0;
      // stop further warnings
      NF.Index = 0;
    }

    // position
    TVec Md2Org = Org;

    // angle
    TAVec Md2Angle = Angles;
    if (FDef.AngleStart || FDef.AngleEnd != 1.0f) {
      Md2Angle.yaw = AngleMod(Md2Angle.yaw+FDef.AngleStart+(FDef.AngleEnd-FDef.AngleStart)*Inter);
    }

    if (FDef.hasYaw) Md2Angle.yaw = FDef.angleYaw;
    if (FDef.hasPitch) Md2Angle.pitch = FDef.anglePitch;
    if (FDef.hasRoll) Md2Angle.roll = FDef.angleRoll;

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
        // noshadow model is rendered as "normal", so it doesn't need fog
        if (Md2Alpha <= getAlphaThreshold() || SubMdl.NoShadow) continue;
        break;
      case RPASS_NonShadow:
        //if (Md2Alpha >= 1.0f && !Additive && !SubMdl.NoShadow) continue;
        if (Md2Alpha < 1.0f || Additive || SubMdl.NoShadow) continue;
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
            Trans, ColourMap, Md2Light, Fade, Md2Alpha, Additive,
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
          Trans, ColourMap, Md2Alpha, smooth_inter, Interpolate, SubMdl.UseDepth,
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
bool VRenderLevelShared::DrawAliasModel (const TVec &Org, const TAVec &Angles,
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
  DrawModel(Level, Org, Angles, ScaleX, ScaleY, *Mdl->DefaultClass, FIdx,
    NFIdx, Trans, ColourMap, Version, Light, Fade, Alpha, Additive,
    IsViewModel, InterpFrac, Interpolate, CurrLightPos, CurrLightRadius,
    Pass, IsAdvancedRenderer());
  return true;
}


//==========================================================================
//
//  VRenderLevelShared::DrawAliasModel
//
//==========================================================================
bool VRenderLevelShared::DrawAliasModel (VName clsName, const TVec &Org, const TAVec &Angles,
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

  DrawModel(Level, Org, Angles, ScaleX, ScaleY, *Cls, FIdx, NFIdx, Trans,
    ColourMap, Version, Light, Fade, Alpha, Additive, IsViewModel,
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
  // check if we want to interpolate model frames
  const bool Interpolate = r_interpolate_frames;
  if (Ent->EntityFlags&VEntity::EF_FixedModel) {
    if (!FL_FileExists(VStr("models/")+Ent->FixedModelName)) {
      GCon->Logf("Can't find %s", *Ent->FixedModelName);
      return false;
    }
    VModel *Mdl = Mod_FindName(VStr("models/")+Ent->FixedModelName);
    if (!Mdl) return false;
    return DrawAliasModel(Ent->Origin-TVec(0, 0, Ent->FloorClip),
      Ent->Angles, Ent->ScaleX, Ent->ScaleY, Mdl,
      Ent->getMFI(), Ent->getNextMFI(),
      GetTranslation(Ent->Translation),
      Ent->ModelVersion, Light, Fade, Alpha, Additive, false, Inter,
      Interpolate, Pass);
  } else {
    return DrawAliasModel(Ent->GetClass()->Name, Ent->Origin-TVec(0, 0, Ent->FloorClip),
      Ent->Angles, Ent->ScaleX, Ent->ScaleY,
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
  int TranslEnd, int Colour, float Inter)
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

  DrawModel(nullptr, Origin, Angles, 1.0f, 1.0f, *Model->DefaultClass, FIdx,
    NFIdx, R_GetCachedTranslation(R_SetMenuPlayerTrans(TranslStart,
    TranslEnd, Colour), nullptr), 0, 0, 0xffffffff, 0, 1.0f, false, false,
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
  VClassModelScript *Cls = FindClassModelByName(State->Outer->Name);
  if (!Cls) return false;
  if (!State) return false;
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

  DrawModel(nullptr, Origin, Angles, 1.0f, 1.0f, *Cls, FIdx, NFIdx, nullptr, 0, 0,
    0xffffffff, 0, 1.0f, false, false, InterpFrac, Interpolate,
    TVec(), 0, RPASS_Normal, true); // force draw

  Drawer->EndView();
  return true;
}
