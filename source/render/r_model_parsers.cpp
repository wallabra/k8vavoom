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
// included from "r_model.cpp"


//==========================================================================
//
//  GZModelDefEx::ParseMD2Frames
//
//  return `true` if model was succesfully found and parsed, or
//  false if model wasn't found or in invalid format
//  WARNING: don't clear `names` array!
//
//==========================================================================
bool GZModelDefEx::ParseMD2Frames (VStr mdpath, TArray<VStr> &names) {
  return VMeshModel::LoadMD2Frames(mdpath, names);
}


bool IsKnownModelFormat (VStream *strm);


//==========================================================================
//
//  GZModelDefEx::IsModelFileExists
//
//==========================================================================
bool GZModelDefEx::IsModelFileExists (VStr mdpath) {
  if (mdpath.length() == 0) return false;
  VStream *strm = FL_OpenFileRead(mdpath);
  if (!strm) return false;
  bool okfmt = VMeshModel::IsKnownModelFormat(strm);
  strm->Close();
  delete strm;
  return okfmt;
}


//==========================================================================
//
//  VMeshModel::AddEdge
//
//==========================================================================
void VMeshModel::AddEdge (TArray<VTempEdge> &Edges, int Vert1, int Vert2, int Tri) {
  // check for a match
  // compare original vertex indices since texture coordinates are not important here
  for (auto &&E : Edges) {
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
//  VMeshModel::CopyEdgesTo
//
//  store edges
//
//==========================================================================
void VMeshModel::CopyEdgesTo (TArray<VMeshEdge> &dest, TArray<VTempEdge> &src) {
  const int len = src.length();
  dest.setLength(len);
  for (int i = 0; i < len; ++i) {
    dest[i].Vert1 = src[i].Vert1;
    dest[i].Vert2 = src[i].Vert2;
    dest[i].Tri1 = src[i].Tri1;
    dest[i].Tri2 = src[i].Tri2;
  }
}


//==========================================================================
//
//  VMeshModel::getStrZ
//
//==========================================================================
VStr VMeshModel::getStrZ (const char *s, unsigned maxlen) {
  if (!s || maxlen == 0 || !s[0]) return VStr::EmptyString;
  const char *se = s;
  while (maxlen-- && *se) ++se;
  return VStr(s, (int)(ptrdiff_t)(se-s));
}


//==========================================================================
//
//  VMeshModel::LoadMD2Frames
//
//  return `true` if model was succesfully found and parsed, or
//  false if model wasn't found or in invalid format
//  WARNING: don't clear `names` array!
//
//==========================================================================
bool VMeshModel::LoadMD2Frames (VStr mdpath, TArray<VStr> &names) {
  // load the file
  VStream *strm = FL_OpenFileRead(mdpath);
  if (!strm) return false;

  TArray<vuint8> data;
  data.setLength(strm->TotalSize());
  if (data.length() < 4) {
    delete strm;
    return false;
  }
  strm->Serialise(data.ptr(), data.length());
  bool wasError = strm->IsError();
  delete strm;
  if (wasError) return false;

  // is this MD2 model?
  if (LittleLong(*(vuint32 *)data.ptr()) != IDPOLY2HEADER) return false;

  mmdl_t *pmodel = (mmdl_t *)data.ptr();

  // endian-adjust and swap the data, starting with the alias model header
  for (unsigned i = 0; i < sizeof(mmdl_t)/4; ++i) ((vint32 *)pmodel)[i] = LittleLong(((vint32 *)pmodel)[i]);

  if (pmodel->version != ALIAS_VERSION) return false;
  if (pmodel->numverts <= 0 || pmodel->numverts > MAXALIASVERTS) return false;
  if (pmodel->numstverts <= 0 || pmodel->numstverts > MAXALIASSTVERTS) return false;
  if (pmodel->numtris <= 0 || pmodel->numtris > 65536) return false;
  if (pmodel->numskins > 1024) return false;
  if (pmodel->numframes < 1 || pmodel->numframes > 1024) return false;

  mframe_t *pframe = (mframe_t *)((vuint8 *)pmodel+pmodel->ofsframes);

  for (unsigned i = 0; i < pmodel->numframes; ++i) {
    VStr frname = getStrZ(pframe->name, 16);
    names.append(frname);
    pframe = (mframe_t *)((vuint8 *)pframe+pmodel->framesize);
  }

  return true;
}


//==========================================================================
//
//  VMeshModel::Load_MD2
//
//==========================================================================
void VMeshModel::Load_MD2 (vuint8 *Data, int DataSize) {
  mmdl_t *pmodel;
  mstvert_t *pstverts;
  mtriangle_t *ptri;
  mframe_t *pframe;

  pmodel = (mmdl_t *)Data;
  this->Uploaded = false;
  this->VertsBuffer = 0;
  this->IndexBuffer = 0;

  // endian-adjust and swap the data, starting with the alias model header
  for (unsigned i = 0; i < sizeof(mmdl_t)/4; ++i) ((vint32 *)pmodel)[i] = LittleLong(((vint32 *)pmodel)[i]);

  if (pmodel->version != ALIAS_VERSION) Sys_Error("model '%s' has wrong version number (%i should be %i)", *this->Name, pmodel->version, ALIAS_VERSION);
  if (pmodel->numverts <= 0) Sys_Error("model '%s' has no vertices", *this->Name);
  if (pmodel->numverts > MAXALIASVERTS) Sys_Error("model '%s' has too many vertices", *this->Name);
  if (pmodel->numstverts <= 0) Sys_Error("model '%s' has no texture vertices", *this->Name);
  if (pmodel->numstverts > MAXALIASSTVERTS) Sys_Error("model '%s' has too many texture vertices", *this->Name);
  if (pmodel->numtris <= 0) Sys_Error("model '%s' has no triangles", *this->Name);
  if (pmodel->numtris > 65536) Sys_Error("model '%s' has too many triangles", *this->Name);
  //if (pmodel->skinwidth&0x03) Sys_Error("Mod_LoadAliasModel: skinwidth not multiple of 4");
  if (pmodel->numskins < 0 || pmodel->numskins > 1024) Sys_Error("model '%s' has invalid number of skins: %u", *this->Name, pmodel->numskins);
  if (pmodel->numframes < 1 || pmodel->numframes > 1024) Sys_Error("model '%s' has invalid numebr of frames: %u", *this->Name, pmodel->numframes);

  // base s and t vertices
  pstverts = (mstvert_t *)((vuint8 *)pmodel+pmodel->ofsstverts);
  for (unsigned i = 0; i < pmodel->numstverts; ++i) {
    pstverts[i].s = LittleShort(pstverts[i].s);
    pstverts[i].t = LittleShort(pstverts[i].t);
  }

  // triangles
  //k8: this tried to collapse same vertices, but meh
  TArray<TVertMap> VertMap;
  TArray<VTempEdge> Edges;
  this->Tris.SetNum(pmodel->numtris);
  ptri = (mtriangle_t *)((vuint8 *)pmodel+pmodel->ofstris);
  for (unsigned i = 0; i < pmodel->numtris; ++i) {
    for (unsigned j = 0; j < 3; ++j) {
      ptri[i].vertindex[j] = LittleShort(ptri[i].vertindex[j]);
      ptri[i].stvertindex[j] = LittleShort(ptri[i].stvertindex[j]);
      this->Tris[i].VertIndex[j] = VertMap.length();
      TVertMap &v = VertMap.alloc();
      v.VertIndex = ptri[i].vertindex[j];
      v.STIndex = ptri[i].stvertindex[j];
    }
    for (unsigned j = 0; j < 3; ++j) {
      AddEdge(Edges, this->Tris[i].VertIndex[j], this->Tris[i].VertIndex[(j+1)%3], i);
    }
  }

  // calculate remapped ST verts
  this->STVerts.SetNum(VertMap.length());
  for (int i = 0; i < VertMap.length(); ++i) {
    this->STVerts[i].S = (float)pstverts[VertMap[i].STIndex].s/(float)pmodel->skinwidth;
    this->STVerts[i].T = (float)pstverts[VertMap[i].STIndex].t/(float)pmodel->skinheight;
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

  this->Frames.SetNum(pmodel->numframes);
  this->AllVerts.SetNum(pmodel->numframes*VertMap.length());
  this->AllNormals.SetNum(pmodel->numframes*VertMap.length());
  this->AllPlanes.SetNum(pmodel->numframes*pmodel->numtris);
  pframe = (mframe_t *)((vuint8 *)pmodel+pmodel->ofsframes);

  int triIgnored = 0;
  for (unsigned i = 0; i < pmodel->numframes; ++i) {
    pframe->scale[0] = LittleFloat(pframe->scale[0]);
    pframe->scale[1] = LittleFloat(pframe->scale[1]);
    pframe->scale[2] = LittleFloat(pframe->scale[2]);
    pframe->scale_origin[0] = LittleFloat(pframe->scale_origin[0]);
    pframe->scale_origin[1] = LittleFloat(pframe->scale_origin[1]);
    pframe->scale_origin[2] = LittleFloat(pframe->scale_origin[2]);

    VMeshFrame &Frame = this->Frames[i];
    Frame.Name = getStrZ(pframe->name, 16);
    Frame.Scale = TVec(pframe->scale[0], pframe->scale[1], pframe->scale[2]);
    Frame.Origin = TVec(pframe->scale_origin[0], pframe->scale_origin[1], pframe->scale_origin[2]);
    Frame.Verts = &this->AllVerts[i*VertMap.length()];
    Frame.Normals = &this->AllNormals[i*VertMap.length()];
    Frame.Planes = &this->AllPlanes[i*pmodel->numtris];
    Frame.VertsOffset = 0;
    Frame.NormalsOffset = 0;
    Frame.TriCount = pmodel->numtris;
    //Frame.ValidTris.setLength((int)pmodel->numtris);

    trivertx_t *Verts = (trivertx_t *)(pframe+1);
    for (int j = 0; j < VertMap.length(); ++j) {
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
        TVec v1 = Frame.Verts[this->Tris[j].VertIndex[(vnn+0)%3]];
        TVec v2 = Frame.Verts[this->Tris[j].VertIndex[(vnn+1)%3]];
             v3 = Frame.Verts[this->Tris[j].VertIndex[(vnn+2)%3]];

        TVec d1 = v2-v3;
        TVec d2 = v1-v3;
        PlaneNormal = CrossProduct(d1, d2);
        if (lengthSquared(PlaneNormal) == 0) {
          //k8:hack!
          if (mdl_report_errors && !reported) {
            GCon->Logf("Alias model '%s' has degenerate triangle %d; v1=(%f,%f,%f), v2=(%f,%f,%f); v3=(%f,%f,%f); d1=(%f,%f,%f); d2=(%f,%f,%f); cross=(%f,%f,%f)",
              *this->Name, j, v1.x, v1.y, v1.z, v2.x, v2.y, v2.z, v3.x, v3.y, v3.z, d1.x, d1.y, d1.z, d2.x, d2.y, d2.z, PlaneNormal.x, PlaneNormal.y, PlaneNormal.z);
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
      VMeshFrame &Frame = this->Frames[0];
      TArray<VMeshTri> NewTris; // vetex indicies
      Frame.TriCount = 0;
      for (unsigned j = 0; j < pmodel->numtris; ++j) {
        if (validTri[j]) {
          NewTris.append(this->Tris[j]);
          ++Frame.TriCount;
        }
      }
      if (Frame.TriCount == 0) Sys_Error("model %s has no valid triangles", *this->Name);
      // replace index array
      this->Tris.setLength(NewTris.length());
      memcpy(this->Tris.ptr(), NewTris.ptr(), NewTris.length()*sizeof(VMeshTri));
      pmodel->numtris = Frame.TriCount;
      if (showError) {
        GCon->Logf(NAME_Warning, "Alias model '%s' has %d degenerate triangles out of %u! model rebuilt.", *this->Name, triIgnored, pmodel->numtris);
      }
      // rebuild edges
      this->Edges.SetNum(0);
      for (unsigned i = 0; i < pmodel->numtris; ++i) {
        for (unsigned j = 0; j < 3; ++j) {
          //AddEdge(Edges, this->Tris[i].VertIndex[j], ptri[i].vertindex[j], this->Tris[i].VertIndex[(j+1)%3], ptri[i].vertindex[(j+1)%3], i);
          AddEdge(Edges, this->Tris[i].VertIndex[j], this->Tris[i].VertIndex[(j+1)%3], i);
        }
      }
    }
  } else {
    if (hadError && showError) {
      GCon->Logf(NAME_Warning, "Alias model '%s' has %d degenerate triangles out of %u!", *this->Name, triIgnored, pmodel->numtris);
    }
  }

  // if there were some errors, disable shadows for this model, it is probably broken anyway
  this->HadErrors = hadError;

  // store edges
  CopyEdgesTo(this->Edges, Edges);

  // skins
  mskin_t *pskindesc = (mskin_t *)((vuint8 *)pmodel+pmodel->ofsskins);
  for (unsigned i = 0; i < pmodel->numskins; ++i) {
    //this->Skins.Append(*getStrZ(pskindesc[i].name, 64).ToLower());
    VStr name = getStrZ(pskindesc[i].name, 64).toLowerCase();
    // prepend model path
    if (!name.isEmpty()) name = this->Name.ExtractFilePath()+name.ExtractFileBaseName();
    //GCon->Logf("model '%s' has skin #%u '%s'", *this->Name, i, *name);
    VMeshModel::SkinInfo &si = this->Skins.alloc();
    si.fileName = *name;
    si.textureId = -1;
    si.shade = -1;
  }

  this->loaded = true;
}


//==========================================================================
//
//  VMeshModel::Load_MD3
//
//==========================================================================
void VMeshModel::Load_MD3 (vuint8 *Data, int DataSize) {
  this->Uploaded = false;
  this->VertsBuffer = 0;
  this->IndexBuffer = 0;

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

  if (pmodel->ver != MD3_VERSION) Sys_Error("model '%s' has wrong version number (%u should be %i)", *this->Name, pmodel->ver, MD3_VERSION);
  if (pmodel->frameNum < 1 || pmodel->frameNum > 1024) Sys_Error("model '%s' has invalid numebr of frames: %u", *this->Name, pmodel->frameNum);
  if (pmodel->surfaceNum < 1) Sys_Error("model '%s' has no meshes", *this->Name);
  if ((unsigned)this->MeshIndex >= pmodel->surfaceNum) Sys_Error("model '%s' has no mesh with index %d", *this->Name, this->MeshIndex);
  //if (pmodel->surfaceNum > 1) GCon->Logf(NAME_Warning, "model '%s' has more than one mesh (%u); ignoring extra meshes", *this->Name, pmodel->surfaceNum);

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
  for (int f = 0; f < this->MeshIndex; ++f) {
    if (memcmp(pmesh->sign, "IDP3", 4) != 0) Sys_Error("model '%s' has invalid mesh signature", *this->Name);
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

    if (pmesh->shaderNum < 0 || pmesh->shaderNum > 1024) Sys_Error("model '%s' has invalid number of shaders: %u", *this->Name, pmesh->shaderNum);
    if (pmesh->frameNum != pmodel->frameNum) Sys_Error("model '%s' has mismatched number of frames in mesh", *this->Name);
    if (pmesh->vertNum < 1) Sys_Error("model '%s' has no vertices", *this->Name);
    if (pmesh->vertNum > MAXALIASVERTS) Sys_Error("model '%s' has too many vertices", *this->Name);
    if (pmesh->triNum < 1) Sys_Error("model '%s' has no triangles", *this->Name);
    if (pmesh->triNum > 65536) Sys_Error("model '%s' has too many triangles", *this->Name);

    pmesh = (MD3Surface *)((vuint8 *)pmesh+pmesh->endOfs);
  }

  // convert and copy shader data
  MD3Shader *pshader = (MD3Shader *)((vuint8 *)pmesh+pmesh->shaderOfs);
  for (unsigned i = 0; i < pmesh->shaderNum; ++i) {
    pshader[i].index = LittleLong(pshader[i].index);
    VStr name = getStrZ(pshader[i].name, 64).toLowerCase();
    // prepend model path
    if (!name.isEmpty()) name = this->Name.ExtractFilePath()+name.ExtractFileBaseName();
    //GCon->Logf("SKIN: %s", *name);
    VMeshModel::SkinInfo &si = this->Skins.alloc();
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
    if (ptri[i].v0 >= pmesh->vertNum || ptri[i].v1 >= pmesh->vertNum || ptri[i].v2 >= pmesh->vertNum) Sys_Error("model '%s' has invalid vertex index in triangle #%u", *this->Name, i);
  }

  // copy texture coordinates
  this->STVerts.setLength((int)pmesh->vertNum);
  for (unsigned i = 0; i < pmesh->vertNum; ++i) {
    this->STVerts[i].S = pstverts[i].s;
    this->STVerts[i].T = pstverts[i].t;
  }

  // copy triangles, create edges
  TArray<VTempEdge> Edges;
  this->Tris.setLength(pmesh->triNum);
  for (unsigned i = 0; i < pmesh->triNum; ++i) {
    this->Tris[i].VertIndex[0] = ptri[i].v0;
    this->Tris[i].VertIndex[1] = ptri[i].v1;
    this->Tris[i].VertIndex[2] = ptri[i].v2;
    for (unsigned j = 0; j < 3; ++j) {
      AddEdge(Edges, this->Tris[i].VertIndex[j], this->Tris[i].VertIndex[(j+1)%3], i);
    }
  }

  // copy vertices
  this->AllVerts.setLength(pmodel->frameNum*pmesh->vertNum);
  this->AllNormals.setLength(pmodel->frameNum*pmesh->vertNum);
  for (unsigned i = 0; i < pmesh->vertNum*pmodel->frameNum; ++i) {
    this->AllVerts[i] = md3vert(pverts+i);
    this->AllNormals[i] = md3vertNormal(pverts+i);
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

  this->Frames.setLength(pmodel->frameNum);
  this->AllPlanes.setLength(pmodel->frameNum*pmesh->triNum);

  int triIgnored = 0;
  for (unsigned i = 0; i < pmodel->frameNum; ++i, ++pframe) {
    VMeshFrame &Frame = this->Frames[i];
    Frame.Name = getStrZ(pframe->name, 16);
    Frame.Scale = TVec(1.0f, 1.0f, 1.0f);
    Frame.Origin = TVec(pframe->origin[0], pframe->origin[1], pframe->origin[2]);
    Frame.Verts = &this->AllVerts[i*pmesh->vertNum];
    Frame.Normals = &this->AllNormals[i*pmesh->vertNum];
    Frame.Planes = &this->AllPlanes[i*pmesh->triNum];
    Frame.VertsOffset = 0;
    Frame.NormalsOffset = 0;
    Frame.TriCount = pmesh->triNum;

    // process triangles
    for (unsigned j = 0; j < pmesh->triNum; ++j) {
      TVec PlaneNormal;
      TVec v3(0, 0, 0);
      bool reported = false, hacked = false;
      for (int vnn = 0; vnn < 3; ++vnn) {
        TVec v1 = Frame.Verts[this->Tris[j].VertIndex[(vnn+0)%3]];
        TVec v2 = Frame.Verts[this->Tris[j].VertIndex[(vnn+1)%3]];
             v3 = Frame.Verts[this->Tris[j].VertIndex[(vnn+2)%3]];

        TVec d1 = v2-v3;
        TVec d2 = v1-v3;
        PlaneNormal = CrossProduct(d1, d2);
        if (lengthSquared(PlaneNormal) == 0) {
          //k8:hack!
          if (mdl_report_errors && !reported) {
            GCon->Logf("Alias model '%s' has degenerate triangle %d; v1=(%f,%f,%f), v2=(%f,%f,%f); v3=(%f,%f,%f); d1=(%f,%f,%f); d2=(%f,%f,%f); cross=(%f,%f,%f)",
              *this->Name, j, v1.x, v1.y, v1.z, v2.x, v2.y, v2.z, v3.x, v3.y, v3.z, d1.x, d1.y, d1.z, d2.x, d2.y, d2.z, PlaneNormal.x, PlaneNormal.y, PlaneNormal.z);
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
      VMeshFrame &Frame = this->Frames[0];
      TArray<VMeshTri> NewTris; // vetex indicies
      Frame.TriCount = 0;
      for (unsigned j = 0; j < pmesh->triNum; ++j) {
        if (validTri[j]) {
          NewTris.append(this->Tris[j]);
          ++Frame.TriCount;
        }
      }
      if (Frame.TriCount == 0) Sys_Error("model %s has no valid triangles", *this->Name);
      // replace index array
      this->Tris.setLength(NewTris.length());
      memcpy(this->Tris.ptr(), NewTris.ptr(), NewTris.length()*sizeof(VMeshTri));
      pmesh->triNum = Frame.TriCount;
      if (showError) {
        GCon->Logf(NAME_Warning, "Alias model '%s' has %d degenerate triangles out of %u! model rebuilt.", *this->Name, triIgnored, pmesh->triNum);
      }
      // rebuild edges
      this->Edges.setLength(0);
      for (unsigned i = 0; i < pmesh->triNum; ++i) {
        for (unsigned j = 0; j < 3; ++j) {
          //AddEdge(Edges, this->Tris[i].VertIndex[j], ptri[i].vertindex[j], this->Tris[i].VertIndex[(j+1)%3], ptri[i].vertindex[(j+1)%3], i);
          AddEdge(Edges, this->Tris[i].VertIndex[j], this->Tris[i].VertIndex[(j+1)%3], i);
        }
      }
    }
  } else {
    if (hadError && showError) {
      GCon->Logf(NAME_Warning, "Alias model '%s' has %d degenerate triangles out of %u!", *this->Name, triIgnored, pmesh->triNum);
    }
  }

  // if there were some errors, disable shadows for this model, it is probably broken anyway
  this->HadErrors = hadError;

  // store edges
  CopyEdgesTo(this->Edges, Edges);

  this->loaded = true;
}


//==========================================================================
//
//  VMeshModel::LoadFromData
//
//==========================================================================
void VMeshModel::LoadFromData (vuint8 *Data, int DataSize) {
  if (loaded) return;
  if (!Data || DataSize < 4) Sys_Error("Too small data for model '%s'", *Name);

  if (LittleLong(*(vuint32 *)Data) == IDPOLY2HEADER) {
    // MD2
    GCon->Logf(NAME_Debug, "loading MD2 model '%s'...", *Name);
    Load_MD2(Data, DataSize);
  } else if (LittleLong(*(vuint32 *)Data) == IDPOLY3HEADER) {
    // MD3
    GCon->Logf(NAME_Debug, "loading MD3 model '%s'...", *Name);
    Load_MD3(Data, DataSize);
  } else {
    Sys_Error("model '%s' is in unknown format", *Name);
  }
}


//==========================================================================
//
//  VMeshModel::IsKnownModelFormat
//
//==========================================================================
bool VMeshModel::IsKnownModelFormat (VStream *strm) {
  if (!strm || strm->IsError()) return false;
  strm->Seek(0);
  if (strm->IsError() || strm->TotalSize() < 4) return false;
  vuint32 sign = 0;
  strm->Serialise(&sign, sizeof(sign));
  if (strm->IsError()) return false;
  if (LittleLong(*(vuint32 *)&sign) == IDPOLY2HEADER ||
      LittleLong(*(vuint32 *)&sign) == IDPOLY3HEADER)
  {
    return true;
  }
  return false;
}


//==========================================================================
//
//  VMeshModel::LoadFromWad
//
//==========================================================================
void VMeshModel::LoadFromWad () {
  if (loaded) return;

  // load the file
  VStream *Strm = FL_OpenFileRead(Name);
  if (!Strm) Sys_Error("Couldn't open model data '%s'", *Name);

  int DataSize = Strm->TotalSize();
  if (DataSize < 1) Sys_Error("Cannot read model data '%s'", *Name);

  vuint8 *Data = (vuint8 *)Z_Malloc(DataSize);
  Strm->Serialise(Data, DataSize);
  bool wasError = Strm->IsError();
  delete Strm;
  if (wasError) Sys_Error("Error loading model data '%s'", *Name);

  LoadFromData(Data, DataSize);

  Z_Free(Data);
}
