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
#include "../gamedefs.h"


// ////////////////////////////////////////////////////////////////////////// //
class DebugExportError : public VavoomError {
public:
  explicit DebugExportError (const char *text) : VavoomError(text) {}
};


//==========================================================================
//
//  writef
//
//==========================================================================
static __attribute__((format(printf, 2, 3))) void writef (VStream &strm, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char *res = vavarg(fmt, ap);
  va_end(ap);
  if (res && res[0]) {
    strm.Serialise(res, (int)strlen(res));
    if (strm.IsError()) throw DebugExportError("write error");
  }
}


// ////////////////////////////////////////////////////////////////////////// //
struct VertexPool {
public:
  TMapNC<vuint64, vint32> map; // key: two floats; value: index
  TArray<TVec> list;

public:
  VV_DISABLE_COPY(VertexPool)
  VertexPool () {}

  void clear () {
    map.clear();
    list.clear();
  }

  // returns index
  vint32 put (const TVec v) {
    union __attribute__((packed)) {
      struct __attribute__((packed)) { float f1, f2; };
      vuint64 i64;
    } u;
    static_assert(sizeof(u) == sizeof(vuint64), "oops");
    u.f1 = v.x;
    u.f2 = v.y;
    auto ip = map.find(u.i64);
    if (ip) {
      /*
      union __attribute__((packed)) {
        struct __attribute__((packed)) { float f1, f2; };
        vuint64 i64;
      } u1;
      u1.f1 = list[*ip].x;
      u1.f2 = list[*ip].y;
      GCon->Logf("looking for (%g,%g); found (%g,%g) at %d (0x%08llx  0x%08llx)", v.x, v.y, list[*ip].x, list[*ip].y, *ip, u.i64, u1.i64);
      */
      return *ip;
    }
    vint32 idx = list.length();
    list.append(TVec(v.x, v.y));
    map.put(u.i64, idx);
    return idx;
  }
};


//==========================================================================
//
//  VLevel::DebugSaveLevel
//
//  this saves everything except thinkers, so i can load it for
//  further experiments
//
//==========================================================================
void VLevel::DebugSaveLevel (VStream &strm) {
  writef(strm, "Namespace = \"VavoomDebug\";\n");

  VertexPool vpool;

  // collect vertices
  for (int f = 0; f < NumLines; ++f) {
    const line_t *line = &Lines[f];
    vpool.put(*line->v1);
    vpool.put(*line->v2);
  }

  // write vertices
  writef(strm, "\n");
  for (int f = 0; f < vpool.list.length(); ++f) {
    writef(strm, "\nvertex // %d\n", f);
    writef(strm, "{\n");
    if ((int)vpool.list[f].x == vpool.list[f].x) {
      writef(strm, "  x = %g.0;\n", vpool.list[f].x);
    } else {
      writef(strm, "  x = %g;\n", vpool.list[f].x);
    }
    if ((int)vpool.list[f].y == vpool.list[f].y) {
      writef(strm, "  y = %g.0;\n", vpool.list[f].y);
    } else {
      writef(strm, "  y = %g;\n", vpool.list[f].y);
    }
    writef(strm, "}\n");
  }

  // write lines
  writef(strm, "\n");
  for (int f = 0; f < NumLines; ++f) {
    const line_t *line = &Lines[f];
    writef(strm, "\nlinedef // %d\n", f);
    writef(strm, "{\n");
    if (line->lineTag && line->lineTag != -1) writef(strm, "  id = %d;\n", line->lineTag);
    writef(strm, "  v1 = %d;\n", vpool.put(*line->v1));
    writef(strm, "  v2 = %d;\n", vpool.put(*line->v2));
    //vassert(line->sidenum[0] >= 0);
    if (line->sidenum[0] >= 0) writef(strm, "  sidefront = %d;\n", line->sidenum[0]);
    if (line->sidenum[1] >= 0) writef(strm, "  sideback = %d;\n", line->sidenum[1]);
    // flags
    if (line->flags&ML_BLOCKING) writef(strm, "  blocking = true;\n");
    if (line->flags&ML_BLOCKMONSTERS) writef(strm, "  blockmonsters = true;\n");
    if (line->flags&ML_TWOSIDED) writef(strm, "  twosided = true;\n");
    if (line->flags&ML_DONTPEGTOP) writef(strm, "  dontpegtop = true;\n");
    if (line->flags&ML_DONTPEGBOTTOM) writef(strm, "  dontpegbottom = true;\n");
    if (line->flags&ML_SECRET) writef(strm, "  secret = true;\n");
    if (line->flags&ML_SOUNDBLOCK) writef(strm, "  blocksound = true;\n");
    if (line->flags&ML_DONTDRAW) writef(strm, "  dontdraw = true;\n");
    if (line->flags&ML_MAPPED) writef(strm, "  mapped = true;\n");
    if (line->flags&ML_REPEAT_SPECIAL) writef(strm, "  repeatspecial = true;\n");
    if (line->flags&ML_MONSTERSCANACTIVATE) writef(strm, "  monsteractivate = true;\n");
    if (line->flags&ML_BLOCKPLAYERS) writef(strm, "  blockplayers = true;\n");
    if (line->flags&ML_BLOCKEVERYTHING) writef(strm, "  blockeverything = true;\n");
    if (line->flags&ML_ZONEBOUNDARY) writef(strm, "  zoneboundary = true;\n");
    if (line->flags&ML_ADDITIVE) writef(strm, "  renderstyle = \"add\";\n");
    if (line->flags&ML_RAILING) writef(strm, "  jumpover = true;\n");
    if (line->flags&ML_BLOCK_FLOATERS) writef(strm, "  blockfloaters = true;\n");
    if (line->flags&ML_CLIP_MIDTEX) writef(strm, "  clipmidtex = true;\n");
    if (line->flags&ML_WRAP_MIDTEX) writef(strm, "  wrapmidtex = true;\n");
    if (line->flags&ML_3DMIDTEX) writef(strm, "  midtex3d = true;\n");
    if (line->flags&ML_3DMIDTEX_IMPASS) writef(strm, "  midtex3dimpassible = true;\n");
    if (line->flags&ML_CHECKSWITCHRANGE) writef(strm, "  checkswitchrange = true;\n");
    if (line->flags&ML_FIRSTSIDEONLY) writef(strm, "  firstsideonly = true;\n");
    if (line->flags&ML_BLOCKPROJECTILE) writef(strm, "  blockprojectiles = true;\n");
    if (line->flags&ML_BLOCKUSE) writef(strm, "  blockuse = true;\n");
    if (line->flags&ML_BLOCKSIGHT) writef(strm, "  blocksight = true;\n");
    if (line->flags&ML_BLOCKHITSCAN) writef(strm, "  blockhitscan = true;\n");
    if (line->flags&ML_KEEPDATA) writef(strm, "  keepdata = true;\n"); // k8vavoom
    if (line->flags&ML_NODECAL) writef(strm, "  nodecal = true;\n"); // k8vavoom
    // spac flags
    if (line->SpacFlags&SPAC_Cross) writef(strm, "  playercross = true;\n");
    if (line->SpacFlags&SPAC_Use) writef(strm, "  playeruse = true;\n");
    if (line->SpacFlags&SPAC_MCross) writef(strm, "  monstercross = true;\n");
    if (line->SpacFlags&SPAC_Impact) writef(strm, "  impact = true;\n");
    if (line->SpacFlags&SPAC_Push) writef(strm, "  playerpush = true;\n");
    if (line->SpacFlags&SPAC_PCross) writef(strm, "  missilecross = true;\n");
    if (line->SpacFlags&SPAC_UseThrough) writef(strm, "  usethrough = true;\n"); // k8vavoom
    if (line->SpacFlags&SPAC_AnyCross) writef(strm, "  anycross = true;\n");
    if (line->SpacFlags&SPAC_MUse) writef(strm, "  monsteruse = true;\n");
    if (line->SpacFlags&SPAC_MPush) writef(strm, "  monsterpush = true;\n"); // k8vavoom
    if (line->SpacFlags&SPAC_UseBack) writef(strm, "  playeruseback = true;\n"); // k8vavoom
    // other
    if (line->alpha < 1.0f) writef(strm, "  alpha = %g;\n", line->alpha);
    // special
    if (line->special) writef(strm, "  special = %d;\n", line->special);
    if (line->arg1) writef(strm, "  arg1 = %d;\n", line->arg1);
    if (line->arg2) writef(strm, "  arg2 = %d;\n", line->arg2);
    if (line->arg3) writef(strm, "  arg3 = %d;\n", line->arg3);
    if (line->arg4) writef(strm, "  arg4 = %d;\n", line->arg4);
    if (line->arg5) writef(strm, "  arg5 = %d;\n", line->arg5);
    if (line->locknumber) writef(strm, "  locknumber = %d;\n", line->locknumber);
    writef(strm, "}\n");
  }

  // write sides
  writef(strm, "\n");
  for (int f = 0; f < NumSides; ++f) {
    const side_t *side = &Sides[f];
    writef(strm, "\nsidedef // %d\n", f);
    writef(strm, "{\n");
    if (side->Sector) writef(strm, "  sector = %d;\n", (int)(ptrdiff_t)(side->Sector-&Sectors[0]));
    if (side->TopTexture.id > 0) writef(strm, "  texturetop = \"%s\";\n", *VStr(GTextureManager.GetTextureName(side->TopTexture.id)).quote());
    if (side->BottomTexture.id > 0) writef(strm, "  texturebottom = \"%s\";\n", *VStr(GTextureManager.GetTextureName(side->BottomTexture.id)).quote());
    if (side->MidTexture.id > 0) writef(strm, "  texturemiddle = \"%s\";\n", *VStr(GTextureManager.GetTextureName(side->MidTexture.id)).quote());
    // offset
    if (side->Top.TextureOffset == side->Bot.TextureOffset && side->Top.TextureOffset == side->Mid.TextureOffset) {
      if (side->Top.TextureOffset) writef(strm, "  offsetx = %g;\n", side->Top.TextureOffset);
    } else {
      if (side->Top.TextureOffset) writef(strm, "  offsetx_top = %g;\n", side->Top.TextureOffset);
      if (side->Bot.TextureOffset) writef(strm, "  offsetx_bottom = %g;\n", side->Bot.TextureOffset);
      if (side->Mid.TextureOffset) writef(strm, "  offsetx_mid = %g;\n", side->Mid.TextureOffset);
    }
    if (side->Top.RowOffset == side->Bot.RowOffset && side->Top.RowOffset == side->Mid.RowOffset) {
      if (side->Top.RowOffset) writef(strm, "  offsety = %g;\n", side->Top.RowOffset);
    } else {
      if (side->Top.RowOffset) writef(strm, "  offsety_top = %g;\n", side->Top.RowOffset);
      if (side->Bot.RowOffset) writef(strm, "  offsety_bottom = %g;\n", side->Bot.RowOffset);
      if (side->Mid.RowOffset) writef(strm, "  offsety_mid = %g;\n", side->Mid.RowOffset);
    }
    // scale
    if (side->Top.ScaleX != 1.0f) writef(strm, "  scaley_top = %g;\n", side->Top.ScaleX);
    if (side->Top.ScaleY != 1.0f) writef(strm, "  scaley_top = %g;\n", side->Top.ScaleY);
    if (side->Bot.ScaleX != 1.0f) writef(strm, "  scaley_bottom = %g;\n", side->Bot.ScaleX);
    if (side->Bot.ScaleY != 1.0f) writef(strm, "  scaley_bottom = %g;\n", side->Bot.ScaleY);
    if (side->Mid.ScaleX != 1.0f) writef(strm, "  scaley_mid = %g;\n", side->Mid.ScaleX);
    if (side->Mid.ScaleY != 1.0f) writef(strm, "  scaley_mid = %g;\n", side->Mid.ScaleY);
    // other
    writef(strm, "  nofakecontrast = true;\n"); // k8vavoom, not right
    if (side->Light) writef(strm, "  light = %d;\n", side->Light); // k8vavoom, not right
    // flags
    if (side->Flags&SDF_ABSLIGHT) writef(strm, "  lightabsolute = true;\n");
    if (side->Flags&SDF_WRAPMIDTEX) writef(strm, "  wrapmidtex = true;\n");
    if (side->Flags&SDF_CLIPMIDTEX) writef(strm, "  clipmidtex = true;\n");
    writef(strm, "}\n");
  }

  // sectors
  writef(strm, "\n");
  for (int f = 0; f < NumSectors; ++f) {
    const sector_t *sector = &Sectors[f];
    writef(strm, "\nsector // %d\n", f);
    writef(strm, "{\n");
    if (sector->sectorTag) writef(strm, "  id = %d;\n", sector->sectorTag);
    if (sector->special) writef(strm, "  special = %d;\n", sector->special);
    if (sector->floor.normal.z == 1.0f) {
      // normal
      writef(strm, "  heightfloor = %g;\n", sector->floor.minz);
    } else {
      // slope
      writef(strm, "  floornormal_x = %g;\n", sector->floor.normal.x); // k8vavoom
      writef(strm, "  floornormal_y = %g;\n", sector->floor.normal.y); // k8vavoom
      writef(strm, "  floornormal_z = %g;\n", sector->floor.normal.z); // k8vavoom
      writef(strm, "  floordist = %g;\n", sector->floor.dist); // k8vavoom
    }
    if (sector->ceiling.normal.z == -1.0f) {
      // normal
      writef(strm, "  heightceiling = %g;\n", sector->ceiling.minz);
    } else {
      // slope
      writef(strm, "  ceilingnormal_x = %g;\n", sector->ceiling.normal.x); // k8vavoom
      writef(strm, "  ceilingnormal_y = %g;\n", sector->ceiling.normal.y); // k8vavoom
      writef(strm, "  ceilingnormal_z = %g;\n", sector->ceiling.normal.z); // k8vavoom
      writef(strm, "  ceilingdist = %g;\n", sector->ceiling.dist); // k8vavoom
    }
    // textures
    writef(strm, "  texturefloor = \"%s\";\n", (sector->floor.pic.id > 0 ? *VStr(GTextureManager.GetTextureName(sector->floor.pic.id)).quote() : "-"));
    writef(strm, "  textureceiling = \"%s\";\n", (sector->ceiling.pic.id > 0 ? *VStr(GTextureManager.GetTextureName(sector->ceiling.pic.id)).quote() : "-"));
    //TODO: write other floor/ceiling parameters
    // light
    writef(strm, "  lightlevel = %d;\n", sector->params.lightlevel);
    if ((sector->params.LightColor&0xffffff) != 0xffffff) writef(strm, "  lightcolor = 0x%06x;\n", sector->params.LightColor);
    if (sector->params.Fade) writef(strm, "  fadecolor = 0x%08x;\n", sector->params.Fade);
    // other
    if (sector->Damage) {
      writef(strm, "  damageamount = %d;\n", sector->Damage);
      if (sector->DamageType != NAME_None) writef(strm, "  damagetype = \"%s\";\n", *VStr(sector->DamageType).quote());
      if (sector->DamageInterval != 32) writef(strm, "  damageinterval = %d;\n", sector->DamageInterval);
      if (sector->DamageLeaky != 0) writef(strm, "  leakiness = %d;\n", sector->DamageLeaky);
    }
    // write other crap
    writef(strm, "}\n");
  }

  //*// non-standard sections //*//
  /*
  // seg vertices
  // collect
  vpool.clear();
  for (int f = 0; f < NumSegs; ++f) {
    const seg_t *seg = &Segs[f];
    vpool.put(*seg->v1);
    vpool.put(*seg->v2);
  }
  // write
  writef(strm, "\n");
  for (int f = 0; f < vpool.list.length(); ++f) {
    writef(strm, "\nsegvertex // %d\n", f);
    writef(strm, "{\n");
    if ((int)vpool.list[f].x == vpool.list[f].x) {
      writef(strm, "  x = %g.0;\n", vpool.list[f].x);
    } else {
      writef(strm, "  x = %g;\n", vpool.list[f].x);
    }
    if ((int)vpool.list[f].y == vpool.list[f].y) {
      writef(strm, "  y = %g.0;\n", vpool.list[f].y);
    } else {
      writef(strm, "  y = %g;\n", vpool.list[f].y);
    }
    writef(strm, "}\n");
  }
  */

  // segs
  writef(strm, "\n");
  for (int f = 0; f < NumSegs; ++f) {
    const seg_t *seg = &Segs[f];
    writef(strm, "\nseg // %d\n", f);
    writef(strm, "{\n");
    /*
    writef(strm, "  v1 = %d;\n", vpool.put(*seg->v1));
    writef(strm, "  v2 = %d;\n", vpool.put(*seg->v2));
    */
    writef(strm, "  v1_x = %g;\n", seg->v1->x);
    writef(strm, "  v1_y = %g;\n", seg->v1->y);
    writef(strm, "  v2_x = %g;\n", seg->v2->x);
    writef(strm, "  v2_y = %g;\n", seg->v2->y);
    writef(strm, "  offset = %g;\n", seg->offset);
    writef(strm, "  length = %g;\n", seg->length);
    if (seg->linedef) {
      writef(strm, "  side = %d;\n", seg->side);
      // not a miniseg
      vassert(seg->sidedef);
      writef(strm, "  sidedef = %d;\n", (int)(ptrdiff_t)(seg->sidedef-&Sides[0]));
      vassert(seg->linedef);
      writef(strm, "  linedef = %d;\n", (int)(ptrdiff_t)(seg->linedef-&Lines[0]));
    }
    if (seg->partner) writef(strm, "  partner = %d;\n", (int)(ptrdiff_t)(seg->partner-&Segs[0]));
    vassert(seg->frontsub);
    writef(strm, "  frontsub = %d;\n", (int)(ptrdiff_t)(seg->frontsub-&Subsectors[0]));
    writef(strm, "}\n");
  }

  // subsectors
  writef(strm, "\n");
  for (int f = 0; f < NumSubsectors; ++f) {
    const subsector_t *sub = &Subsectors[f];
    writef(strm, "\nsubsector // %d\n", f);
    writef(strm, "{\n");
    vassert(sub->sector);
    writef(strm, "  sector = %d;\n", (int)(ptrdiff_t)(sub->sector-&Sectors[0]));
    writef(strm, "  firstseg = %d;\n", sub->firstline);
    writef(strm, "  numsegs = %d;\n", sub->numlines);
    vassert(sub->parent);
    writef(strm, "  bspnode = %d;\n", (int)(ptrdiff_t)(sub->parent-&Nodes[0]));
    writef(strm, "}\n");
  }

  // bsp nodes
  writef(strm, "\n");
  for (int f = 0; f < NumNodes; ++f) {
    const node_t *node = &Nodes[f];
    writef(strm, "\nbspnode // %d\n", f);
    writef(strm, "{\n");
    // plane
    writef(strm, "  plane_normal_x = %g;\n", node->normal.x);
    writef(strm, "  plane_normal_y = %g;\n", node->normal.y);
    writef(strm, "  plane_normal_z = %g;\n", node->normal.z);
    writef(strm, "  plane_dist = %g;\n", node->dist);
    // child 0
    writef(strm, "  bbox_child0_min_x = %g;\n", node->bbox[0][0]);
    writef(strm, "  bbox_child0_min_y = %g;\n", node->bbox[0][1]);
    writef(strm, "  bbox_child0_min_z = %g;\n", node->bbox[0][2]);
    writef(strm, "  bbox_child0_max_x = %g;\n", node->bbox[0][3]);
    writef(strm, "  bbox_child0_max_y = %g;\n", node->bbox[0][4]);
    writef(strm, "  bbox_child0_max_z = %g;\n", node->bbox[0][5]);
    // child 1
    writef(strm, "  bbox_child1_min_x = %g;\n", node->bbox[1][0]);
    writef(strm, "  bbox_child1_min_y = %g;\n", node->bbox[1][1]);
    writef(strm, "  bbox_child1_min_z = %g;\n", node->bbox[1][2]);
    writef(strm, "  bbox_child1_max_x = %g;\n", node->bbox[1][3]);
    writef(strm, "  bbox_child1_max_y = %g;\n", node->bbox[1][4]);
    writef(strm, "  bbox_child1_max_z = %g;\n", node->bbox[1][5]);
    // children
    if (node->children[0]&NF_SUBSECTOR) {
      writef(strm, "  subsector0 = %d;\n", node->children[0]&(~NF_SUBSECTOR));
    } else {
      writef(strm, "  node0 = %d;\n", node->children[0]);
    }
    if (node->children[1]&NF_SUBSECTOR) {
      writef(strm, "  subsector1 = %d;\n", node->children[1]&(~NF_SUBSECTOR));
    } else {
      writef(strm, "  node1 = %d;\n", node->children[1]);
    }
    // parent (if any)
    if (node->parent) writef(strm, "  parent = %d;\n", (int)(ptrdiff_t)(node->parent-&Nodes[0]));
    writef(strm, "}\n");
  }

  // write player starts
  int psidx[8];
  for (int f = 0; f < 8; ++f) psidx[f] = -1;
  bool foundStart = false;
  for (int f = 0; f < NumThings; ++f) {
    const mthing_t *thing = &Things[f];
    int idx = -1;
    if (thing->type >= 1 && thing->type <= 4) idx = (thing->type-1);
    if (thing->type >= 4001 && thing->type <= 4004) idx = (thing->type-4001+4);
    if (idx >= 0) {
      foundStart = true;
      psidx[idx] = f;
    }
  }
  if (foundStart) {
    writef(strm, "\n");
    for (int f = 0; f < 8; ++f) {
      int idx = psidx[f];
      if (idx < 0) continue;
      writef(strm, "\nplayerstart\n");
      writef(strm, "{\n");
      writef(strm, "  player = %d;\n", idx);
      writef(strm, "  x = %g;\n", Things[idx].x);
      writef(strm, "  y = %g;\n", Things[idx].y);
      writef(strm, "  angle = %d;\n", (int)Things[idx].angle);
      if (Things[idx].pitch) writef(strm, "  pitch = %g;\n", Things[idx].pitch);
      if (Things[idx].roll) writef(strm, "  roll = %g;\n", Things[idx].roll);
      writef(strm, "}\n");
    }
  }
}


COMMAND(DebugExportLevel) {
  if (Args.length() != 2) {
    GCon->Log("(only) file name expected!");
    return;
  }

  if (!GLevel) {
    GCon->Log("no level loaded");
    return;
  }

  // find a file name to save it to
  if (!FL_IsSafeDiskFileName(Args[1])) {
    GCon->Logf(NAME_Error, "unsafe file name '%s'", *Args[1]);
    return;
  }

  VStr fname = va("%s.udmf", *Args[1]);
  auto strm = FL_OpenFileWrite(fname, true); // as full name
  if (!strm) {
    GCon->Logf(NAME_Error, "cannot create file '%s'", *fname);
    return;
  }

  try {
    GLevel->DebugSaveLevel(*strm);
    delete strm;
    GCon->Logf("Level exported to '%s'", *fname);
  } catch (DebugExportError &werr) {
    delete strm;
    GCon->Logf(NAME_Error, "Cannot write level to '%s'", *fname);
  } catch (...) {
    delete strm;
    throw;
  }
}
