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
#include "gamedefs.h"
#include "network.h"
#include "net_message.h"
#include "cl_local.h"
#include "sv_local.h"


// ////////////////////////////////////////////////////////////////////////// //
enum {
  CMD_Side,
  CMD_SideTexture,
  CMD_SideTOffset,
  CMD_SideROffset,
  CMD_SideScale,
  CMD_Sector,
  CMD_SectorTexture,
  CMD_SectorLight,
  CMD_PolyObj,
  CMD_StaticLight,
  CMD_NewLevel,
  CMD_ServerInfo,
  CMD_ServerInfoEnd,
  CMD_PreRender,
  CMD_Line,
  CMD_CamTex,
  CMD_LevelTrans,
  CMD_BodyQueueTrans,

  CMD_ResetStaticLights,
  CMD_ResetLevel,

  CMD_MAX
};


//==========================================================================
//
//  VLevelChannel::VLevelChannel
//
//==========================================================================
VLevelChannel::VLevelChannel (VNetConnection *AConnection, vint32 AIndex, vuint8 AOpenedLocally)
  : VChannel(AConnection, CHANNEL_Level, AIndex, AOpenedLocally)
  , Level(nullptr)
  , Lines(nullptr)
  , Sides(nullptr)
  , Sectors(nullptr)
{
  OpenAcked = true; // this channel is pre-opened
  serverInfoBuf.clear();
  csi.mapname.clear();
  csi.sinfo.clear();
  csi.maxclients = 1;
  csi.deathmatch = 0;
  StaticLightsNext = 0;
  Phase = PhaseDone;
}


//==========================================================================
//
//  VLevelChannel::~VLevelChannel
//
//==========================================================================
VLevelChannel::~VLevelChannel () {
  if (Connection) SetLevel(nullptr);
}


//==========================================================================
//
//  VLevelChannel::GetName
//
//==========================================================================
VStr VLevelChannel::GetName () const noexcept {
  return VStr(va("lvlchan #%d(%s)", Index, GetTypeName()));
}


//==========================================================================
//
//  VLevelChannel::SetLevel
//
//==========================================================================
void VLevelChannel::SetLevel (VLevel *ALevel) {
  if (Level) {
    delete[] Lines;
    delete[] Sides;
    delete[] Sectors;
    delete[] PolyObjs;
    Lines = nullptr;
    Sides = nullptr;
    Sectors = nullptr;
    PolyObjs = nullptr;
    CameraTextures.Clear();
    Translations.Clear();
    BodyQueueTrans.Clear();
  }

  Level = ALevel;
  StaticLightsNext = 0;
  Phase = PhaseServerInfo;

  if (Level) {
    Lines = new rep_line_t[Level->NumLines];
    memcpy(Lines, Level->BaseLines, sizeof(rep_line_t)*Level->NumLines);
    Sides = new rep_side_t[Level->NumSides];
    memcpy(Sides, Level->BaseSides, sizeof(rep_side_t)*Level->NumSides);
    Sectors = new rep_sector_t[Level->NumSectors];
    memcpy(Sectors, Level->BaseSectors, sizeof(rep_sector_t)*Level->NumSectors);
    PolyObjs = new rep_polyobj_t[Level->NumPolyObjs];
    memcpy(PolyObjs, Level->BasePolyObjs, sizeof(rep_polyobj_t)*Level->NumPolyObjs);
  }
}


//==========================================================================
//
//  VLevelChannel::ResetLevel
//
//==========================================================================
void VLevelChannel::ResetLevel () {
  if (Level) {
    delete[] Lines;
    delete[] Sides;
    delete[] Sectors;
    delete[] PolyObjs;
    Lines = nullptr;
    Sides = nullptr;
    Sectors = nullptr;
    PolyObjs = nullptr;
    CameraTextures.Clear();
    Translations.Clear();
    BodyQueueTrans.Clear();
    Level = nullptr;
    serverInfoBuf.clear();
    csi.mapname.clear();
    csi.sinfo.clear();
    csi.maxclients = 1;
    csi.deathmatch = 0;
    StaticLightsNext = 0;
    Phase = PhaseServerInfo;
  }
}


//==========================================================================
//
//  VLevelChannel::SendLevelData
//
//==========================================================================
bool VLevelChannel::SendLevelData () {
  switch (Phase) {
    case PhaseServerInfo: SendServerInfo(); break;
    case PhaseStaticLights: SendStaticLights(); break;
    case PhasePrerender: SendPreRender(); break;
    case PhaseDone: break;
    default: abort(); // the thing that should not be
  }
  return (Phase != PhaseDone);
}


//==========================================================================
//
//  VLevelChannel::SendServerInfo
//
//==========================================================================
void VLevelChannel::SendServerInfo () {
  GCon->Logf(NAME_DevNet, "sending initial level data to %s", *Connection->GetAddress());

  //FIXME: fragment overlong server info
  VStr sinfo = svs.serverinfo;
  {
    VMessageOut Msg(this);
    Msg.WriteUInt(CMD_NewLevel);
    VStr MapName = *Level->MapName;
    VStr MapHash = *Level->MapHash;
    vuint32 modhash = FL_GetNetWadsHash();
    vassert(!Msg.IsLoading());
    Msg << MapName;
    Msg << MapHash;
    Msg << modhash;
    Msg.WriteUInt(svs.max_clients);
    Msg.WriteUInt(deathmatch);
    SendMessage(&Msg);
  }

  // send server info
  if (!sinfo.isEmpty()) {
    VMessageOut Msg(this);
    Msg.WriteUInt(CMD_ServerInfo);
    for (int f = 0; f < sinfo.length(); ++f) {
      if (WillOverflowMsg(&Msg, 8)) {
        FlushMsg(&Msg);
        Msg.WriteUInt(CMD_ServerInfo);
      }
      vuint8 ch = (vuint8)(sinfo[f]&0xff);
      Msg << ch;
    }
    FlushMsg(&Msg);
    Msg.WriteUInt(CMD_ServerInfoEnd);
    SendMessage(&Msg);
  }

  Phase = PhaseStaticLights;
}


//==========================================================================
//
//  VLevelChannel::SendStaticLights
//
//==========================================================================
void VLevelChannel::SendStaticLights () {
  // just in case
  if (StaticLightsNext >= Level->NumStaticLights) {
    SendPreRender();
    return;
  }

  GCon->Logf(NAME_DevNet, "sending static lights to %s", *Connection->GetAddress());
  VMessageOut Msg(this);
  VBitStreamWriter strm(MAX_MSG_SIZE_BITS+64, false); // no expand

  if (StaticLightsNext == 0) strm.WriteUInt(CMD_ResetStaticLights);

  for (int i = 0; i < Level->NumStaticLights; ++i) {
    StaticLightsNext = i+1;
    // forced update
    if (UpdateStaticLight(Msg, strm, i, true)) {
      PutStream(&Msg, strm);
      if (!CanSendData()) { FlushMsg(&Msg); return; }
    }
  }

  FlushMsg(&Msg);

  Phase = PhasePrerender;
}


//==========================================================================
//
//  VLevelChannel::SendPreRender
//
//==========================================================================
void VLevelChannel::SendPreRender () {
  GCon->Logf(NAME_DevNet, "sending prerender to %s (%d of %d static lights sent)", *Connection->GetAddress(), StaticLightsNext, Level->NumStaticLights);
  VMessageOut Msg(this);
  Msg.WriteUInt(CMD_PreRender);
  SendMessage(&Msg);

  Phase = PhaseDone;
}


//==========================================================================
//
//  VLevelChannel::UpdateLine
//
//==========================================================================
int VLevelChannel::UpdateLine (VMessageOut &Msg, VBitStreamWriter &strm, int lidx) {
  vassert(lidx >= 0 && lidx < Level->NumLines);

  line_t *Line = &Level->Lines[lidx];

  rep_line_t *RepLine = &Lines[lidx];
  if (Line->alpha == RepLine->alpha) return 0;

  strm.WriteUInt(CMD_Line);
  strm.WriteUInt((vuint32)lidx);
  strm.WriteBit(Line->alpha != RepLine->alpha);
  if (Line->alpha != RepLine->alpha) {
    strm << Line->alpha;
    strm.WriteBit(!!(Line->flags&ML_ADDITIVE));
    RepLine->alpha = Line->alpha;
  }

  return 1;
}


//==========================================================================
//
//  VLevelChannel::ParseLine
//
//==========================================================================
bool VLevelChannel::ParseLine (VMessageIn &Msg) {
  int lidx = (int)Msg.ReadUInt();
  if (Msg.IsError()) {
    GCon->Logf(NAME_DevNet, "%s: cannot read line index", *GetDebugName());
    return false;
  }
  if (lidx < 0 || lidx >= Level->NumLines) {
    GCon->Logf(NAME_DevNet, "%s: got invalid line index %d (max is %d)", *GetDebugName(), lidx, Level->NumLines-1);
    return false;
  }

  line_t *Line = &Level->Lines[lidx];
  if (Msg.ReadBit()) {
    Msg << Line->alpha;
    if (Msg.ReadBit()) Line->flags |= ML_ADDITIVE; else Line->flags &= ~ML_ADDITIVE;
  }

  return !Msg.IsError();
}


//==========================================================================
//
//  VLevelChannel::UpdateSide
//
//==========================================================================
int VLevelChannel::UpdateSide (VMessageOut &Msg, VBitStreamWriter &strm, int sidx) {
  vassert(sidx >= 0 && sidx < Level->NumSides);

  side_t *Side = &Level->Sides[sidx];
  if (!Connection->SecCheckFatPVS(Side->Sector)) return 0;

  rep_side_t *RepSide = &Sides[sidx];

  const bool texturesChanged =
    Side->TopTexture == RepSide->TopTexture &&
    Side->BottomTexture == RepSide->BottomTexture &&
    Side->MidTexture == RepSide->MidTexture;

  const bool tofsChanged =
    Side->Top.TextureOffset == RepSide->Top.TextureOffset &&
    Side->Bot.TextureOffset == RepSide->Bot.TextureOffset &&
    Side->Mid.TextureOffset == RepSide->Mid.TextureOffset;

  const bool rofsChanged =
    Side->Top.RowOffset == RepSide->Top.RowOffset &&
    Side->Bot.RowOffset == RepSide->Bot.RowOffset &&
    Side->Mid.RowOffset == RepSide->Mid.RowOffset;

  const bool scaleChanged =
    Side->Top.ScaleX == RepSide->Top.ScaleX &&
    Side->Top.ScaleY == RepSide->Top.ScaleY &&
    Side->Bot.ScaleX == RepSide->Bot.ScaleX &&
    Side->Bot.ScaleY == RepSide->Bot.ScaleY &&
    Side->Mid.ScaleX == RepSide->Mid.ScaleX &&
    Side->Mid.ScaleY == RepSide->Mid.ScaleY;

  const bool otherChanged =
    Side->Flags == RepSide->Flags &&
    Side->Light == RepSide->Light;

  if (!texturesChanged && !tofsChanged && !rofsChanged && !scaleChanged && !otherChanged) return 0;

  //GCon->Logf(NAME_DevNet, "%s:Update:000: side #%d (strmlen=%d/%d)", *GetDebugName(), i, strm.GetNumBits(), MAX_MSG_SIZE_BITS);

  // textures
  if (texturesChanged) {
    if (Side->TopTexture != RepSide->TopTexture) {
      strm.WriteUInt(CMD_SideTexture);
      strm.WriteUInt((vuint32)sidx);
      // 0
      strm.WriteBit(false);
      strm.WriteBit(false);
      // data
      Side->TopTexture.Serialise(strm);
      RepSide->TopTexture = Side->TopTexture;

      PutStream(&Msg, strm);
      if (!CanSendData()) { FlushMsg(&Msg); Connection->NeedsUpdate = true; return -1; }
    }

    if (Side->BottomTexture != RepSide->BottomTexture) {
      strm.WriteUInt(CMD_SideTexture);
      strm.WriteUInt((vuint32)sidx);
      // 1
      strm.WriteBit(true);
      strm.WriteBit(false);
      // data
      Side->BottomTexture.Serialise(strm);
      RepSide->BottomTexture = Side->BottomTexture;

      PutStream(&Msg, strm);
      if (!CanSendData()) { FlushMsg(&Msg); Connection->NeedsUpdate = true; return -1; }
    }

    if (Side->MidTexture != RepSide->MidTexture) {
      strm.WriteUInt(CMD_SideTexture);
      strm.WriteUInt((vuint32)sidx);
      // 2
      strm.WriteBit(false);
      strm.WriteBit(true);
      // data
      Side->MidTexture.Serialise(strm);
      RepSide->MidTexture = Side->MidTexture;

      PutStream(&Msg, strm);
      if (!CanSendData()) { FlushMsg(&Msg); Connection->NeedsUpdate = true; return -1; }
    }
  }

  if (tofsChanged) {
    strm.WriteUInt(CMD_SideTOffset);
    strm.WriteUInt((vuint32)sidx);

    strm.WriteBit(Side->Top.TextureOffset != RepSide->Top.TextureOffset);
    if (Side->Top.TextureOffset != RepSide->Top.TextureOffset) {
      strm << Side->Top.TextureOffset;
      RepSide->Top.TextureOffset = Side->Top.TextureOffset;
    }

    strm.WriteBit(Side->Bot.TextureOffset != RepSide->Bot.TextureOffset);
    if (Side->Bot.TextureOffset != RepSide->Bot.TextureOffset) {
      strm << Side->Bot.TextureOffset;
      RepSide->Bot.TextureOffset = Side->Bot.TextureOffset;
    }

    strm.WriteBit(Side->Mid.TextureOffset != RepSide->Mid.TextureOffset);
    if (Side->Mid.TextureOffset != RepSide->Mid.TextureOffset) {
      strm << Side->Mid.TextureOffset;
      RepSide->Mid.TextureOffset = Side->Mid.TextureOffset;
    }

    PutStream(&Msg, strm);
    if (!CanSendData()) { FlushMsg(&Msg); Connection->NeedsUpdate = true; return -1; }
  }

  if (rofsChanged) {
    strm.WriteUInt(CMD_SideROffset);
    strm.WriteUInt((vuint32)sidx);

    strm.WriteBit(Side->Top.RowOffset != RepSide->Top.RowOffset);
    if (Side->Top.RowOffset != RepSide->Top.RowOffset) {
      strm << Side->Top.RowOffset;
      RepSide->Top.RowOffset = Side->Top.RowOffset;
    }

    strm.WriteBit(Side->Bot.RowOffset != RepSide->Bot.RowOffset);
    if (Side->Bot.RowOffset != RepSide->Bot.RowOffset) {
      strm << Side->Bot.RowOffset;
      RepSide->Bot.RowOffset = Side->Bot.RowOffset;
    }

    strm.WriteBit(Side->Mid.RowOffset != RepSide->Mid.RowOffset);
    if (Side->Mid.RowOffset != RepSide->Mid.RowOffset) {
      strm << Side->Mid.RowOffset;
      RepSide->Mid.RowOffset = Side->Mid.RowOffset;
    }

    PutStream(&Msg, strm);
    if (!CanSendData()) { FlushMsg(&Msg); Connection->NeedsUpdate = true; return -1; }
  }

  if (scaleChanged) {
    strm.WriteUInt(CMD_SideScale);
    strm.WriteUInt((vuint32)sidx);

    strm.WriteBit(Side->Top.ScaleX != RepSide->Top.ScaleX);
    if (Side->Top.ScaleX != RepSide->Top.ScaleX) {
      strm << Side->Top.ScaleX;
      RepSide->Top.ScaleX = Side->Top.ScaleX;
    }

    strm.WriteBit(Side->Top.ScaleY != RepSide->Top.ScaleY);
    if (Side->Top.ScaleY != RepSide->Top.ScaleY) {
      strm << Side->Top.ScaleY;
      RepSide->Top.ScaleY = Side->Top.ScaleY;
    }

    strm.WriteBit(Side->Bot.ScaleX != RepSide->Bot.ScaleX);
    if (Side->Bot.ScaleX != RepSide->Bot.ScaleX) {
      strm << Side->Bot.ScaleX;
      RepSide->Bot.ScaleX = Side->Bot.ScaleX;
    }

    strm.WriteBit(Side->Bot.ScaleY != RepSide->Bot.ScaleY);
    if (Side->Bot.ScaleY != RepSide->Bot.ScaleY) {
      strm << Side->Bot.ScaleY;
      RepSide->Bot.ScaleY = Side->Bot.ScaleY;
    }

    strm.WriteBit(Side->Mid.ScaleX != RepSide->Mid.ScaleX);
    if (Side->Mid.ScaleX != RepSide->Mid.ScaleX) {
      strm << Side->Mid.ScaleX;
      RepSide->Mid.ScaleX = Side->Mid.ScaleX;
    }

    strm.WriteBit(Side->Mid.ScaleY != RepSide->Mid.ScaleY);
    if (Side->Mid.ScaleY != RepSide->Mid.ScaleY) {
      strm << Side->Mid.ScaleY;
      RepSide->Mid.ScaleY = Side->Mid.ScaleY;
    }

    PutStream(&Msg, strm);
    if (!CanSendData()) { FlushMsg(&Msg); Connection->NeedsUpdate = true; return -1; }
  }

  if (otherChanged) {
    strm.WriteUInt(CMD_Side);
    strm.WriteUInt((vuint32)sidx);

    strm.WriteBit(Side->Flags != RepSide->Flags);
    if (Side->Flags != RepSide->Flags) {
      strm.WriteUInt((vuint32)Side->Flags);
      RepSide->Flags = Side->Flags;
    }

    strm.WriteBit(Side->Light != RepSide->Light);
    if (Side->Light != RepSide->Light) {
      strm << Side->Light;
      RepSide->Light = Side->Light;
    }

    PutStream(&Msg, strm);
    if (!CanSendData()) { FlushMsg(&Msg); Connection->NeedsUpdate = true; return -1; }
  }

  return 0;
}


//==========================================================================
//
//  VLevelChannel::ParseSideTexture
//
//==========================================================================
bool VLevelChannel::ParseSideTexture (VMessageIn &Msg) {
  int sidx = (int)Msg.ReadUInt();
  if (Msg.IsError()) {
    GCon->Logf(NAME_DevNet, "%s: cannot read side index", *GetDebugName());
    return false;
  }
  if (sidx < 0 || sidx >= Level->NumSides) {
    GCon->Logf(NAME_DevNet, "%s: got invalid side index %d (max is %d)", *GetDebugName(), sidx, Level->NumSides-1);
    return false;
  }

  // get texture number
  vuint8 tnum = 0;
  if (Msg.ReadBit()) tnum |= 1u;
  if (Msg.ReadBit()) tnum |= 2u;
  if (Msg.IsError() || tnum > 2) {
    GCon->Logf(NAME_DevNet, "%s: got invalid side texture index %u", *GetDebugName(), tnum);
    return false;
  }

  side_t *Side = &Level->Sides[sidx];
  switch (tnum) {
    case 0: Side->TopTexture.Serialise(Msg); break; // top
    case 1: Side->BottomTexture.Serialise(Msg); break; // bottom
    case 2: Side->MidTexture.Serialise(Msg); break; // middle
    default: abort();
  }

  return !Msg.IsError();
}


//==========================================================================
//
//  VLevelChannel::ParseSideTOffset
//
//==========================================================================
bool VLevelChannel::ParseSideTOffset (VMessageIn &Msg) {
  int sidx = (int)Msg.ReadUInt();
  if (Msg.IsError()) {
    GCon->Logf(NAME_DevNet, "%s: cannot read side index", *GetDebugName());
    return false;
  }
  if (sidx < 0 || sidx >= Level->NumSides) {
    GCon->Logf(NAME_DevNet, "%s: got invalid side index %d (max is %d)", *GetDebugName(), sidx, Level->NumSides-1);
    return false;
  }

  side_t *Side = &Level->Sides[sidx];
  if (Msg.ReadBit()) Msg << Side->Top.TextureOffset;
  if (Msg.ReadBit()) Msg << Side->Bot.TextureOffset;
  if (Msg.ReadBit()) Msg << Side->Mid.TextureOffset;

  return !Msg.IsError();
}


//==========================================================================
//
//  VLevelChannel::ParseSideROffset
//
//==========================================================================
bool VLevelChannel::ParseSideROffset (VMessageIn &Msg) {
  int sidx = (int)Msg.ReadUInt();
  if (Msg.IsError()) {
    GCon->Logf(NAME_DevNet, "%s: cannot read side index", *GetDebugName());
    return false;
  }
  if (sidx < 0 || sidx >= Level->NumSides) {
    GCon->Logf(NAME_DevNet, "%s: got invalid side index %d (max is %d)", *GetDebugName(), sidx, Level->NumSides-1);
    return false;
  }

  side_t *Side = &Level->Sides[sidx];
  if (Msg.ReadBit()) Msg << Side->Top.RowOffset;
  if (Msg.ReadBit()) Msg << Side->Bot.RowOffset;
  if (Msg.ReadBit()) Msg << Side->Mid.RowOffset;

  return !Msg.IsError();
}


//==========================================================================
//
//  VLevelChannel::ParseSideScale
//
//==========================================================================
bool VLevelChannel::ParseSideScale (VMessageIn &Msg) {
  int sidx = (int)Msg.ReadUInt();
  if (Msg.IsError()) {
    GCon->Logf(NAME_DevNet, "%s: cannot read side index", *GetDebugName());
    return false;
  }
  if (sidx < 0 || sidx >= Level->NumSides) {
    GCon->Logf(NAME_DevNet, "%s: got invalid side index %d (max is %d)", *GetDebugName(), sidx, Level->NumSides-1);
    return false;
  }

  side_t *Side = &Level->Sides[sidx];
  if (Msg.ReadBit()) Msg << Side->Top.ScaleX;
  if (Msg.ReadBit()) Msg << Side->Top.ScaleY;
  if (Msg.ReadBit()) Msg << Side->Bot.ScaleX;
  if (Msg.ReadBit()) Msg << Side->Bot.ScaleY;
  if (Msg.ReadBit()) Msg << Side->Mid.ScaleX;
  if (Msg.ReadBit()) Msg << Side->Mid.ScaleY;

  return !Msg.IsError();
}



//==========================================================================
//
//  VLevelChannel::ParseSide
//
//==========================================================================
bool VLevelChannel::ParseSide (VMessageIn &Msg) {
  int sidx = (int)Msg.ReadUInt();
  if (Msg.IsError()) {
    GCon->Logf(NAME_DevNet, "%s: cannot read side index", *GetDebugName());
    return false;
  }
  if (sidx < 0 || sidx >= Level->NumSides) {
    GCon->Logf(NAME_DevNet, "%s: got invalid side index %d (max is %d)", *GetDebugName(), sidx, Level->NumSides-1);
    return false;
  }

  side_t *Side = &Level->Sides[sidx];
  if (Msg.ReadBit()) Side->Flags = Msg.ReadUInt();
  if (Msg.ReadBit()) Msg << Side->Light;

  return !Msg.IsError();
}


//==========================================================================
//
//  VLevelChannel::UpdateSector
//
//==========================================================================
int VLevelChannel::UpdateSector (VMessageOut &Msg, VBitStreamWriter &strm, int sidx) {
  vassert(sidx >= 0 && sidx < Level->NumSectors);

  sector_t *Sec = &Level->Sectors[sidx];
  //bool forced = false;

  if (!Connection->SecCheckFatPVS(Sec) &&
      !(Sec->SectorFlags&sector_t::SF_ExtrafloorSource) &&
      !(Sec->SectorFlags&sector_t::SF_TransferSource))
  {
    return 0;
  }

  VEntity *FloorSkyBox = Sec->floor.SkyBox;
  if (FloorSkyBox && !Connection->ObjMap->CanSerialiseObject(FloorSkyBox)) FloorSkyBox = nullptr;

  VEntity *CeilSkyBox = Sec->ceiling.SkyBox;
  if (CeilSkyBox && !Connection->ObjMap->CanSerialiseObject(CeilSkyBox)) CeilSkyBox = nullptr;

  rep_sector_t *RepSec = &Sectors[sidx];

  const bool FloorChanged = RepSec->floor_dist != Sec->floor.dist ||
    mround(RepSec->floor_xoffs) != mround(Sec->floor.xoffs) ||
    mround(RepSec->floor_yoffs) != mround(Sec->floor.yoffs) ||
    RepSec->floor_XScale != Sec->floor.XScale ||
    RepSec->floor_YScale != Sec->floor.YScale ||
    mround(RepSec->floor_Angle) != mround(Sec->floor.Angle) ||
    mround(RepSec->floor_BaseAngle) != mround(Sec->floor.BaseAngle) ||
    mround(RepSec->floor_BaseYOffs) != mround(Sec->floor.BaseYOffs) ||
    RepSec->floor_SkyBox != FloorSkyBox ||
    RepSec->floor_MirrorAlpha != Sec->floor.MirrorAlpha;

  const bool CeilChanged = RepSec->ceil_dist != Sec->ceiling.dist ||
    mround(RepSec->ceil_xoffs) != mround(Sec->ceiling.xoffs) ||
    mround(RepSec->ceil_yoffs) != mround(Sec->ceiling.yoffs) ||
    RepSec->ceil_XScale != Sec->ceiling.XScale ||
    RepSec->ceil_YScale != Sec->ceiling.YScale ||
    mround(RepSec->ceil_Angle) != mround(Sec->ceiling.Angle) ||
    mround(RepSec->ceil_BaseAngle) != mround(Sec->ceiling.BaseAngle) ||
    mround(RepSec->ceil_BaseYOffs) != mround(Sec->ceiling.BaseYOffs) ||
    RepSec->ceil_SkyBox != CeilSkyBox ||
    RepSec->ceil_MirrorAlpha != Sec->ceiling.MirrorAlpha;

  const bool LightChanged = (abs(RepSec->lightlevel-Sec->params.lightlevel) >= 4);
  const bool FadeChanged = (RepSec->Fade != Sec->params.Fade);
  const bool SkyChanged = (RepSec->Sky != Sec->Sky);

  if (RepSec->floor_pic == Sec->floor.pic &&
      RepSec->ceil_pic == Sec->ceiling.pic &&
      !FloorChanged && !CeilChanged && !LightChanged && !FadeChanged && !SkyChanged)
  {
    return 0;
  }

  // floor texture
  if (RepSec->floor_pic != Sec->floor.pic) {
    strm.WriteUInt(CMD_SectorTexture);
    strm.WriteUInt((vuint32)sidx);
    strm.WriteBit(false); // floor
    Sec->floor.pic.Serialise(strm);

    RepSec->floor_pic = Sec->floor.pic;

    PutStream(&Msg, strm);
    if (!CanSendData()) { FlushMsg(&Msg); Connection->NeedsUpdate = true; return -1; }
  }

  // ceiling texture
  if (RepSec->ceil_pic == Sec->ceiling.pic) {
    strm.WriteUInt(CMD_SectorTexture);
    strm.WriteUInt((vuint32)sidx);
    strm.WriteBit(true); // ceiling
    Sec->floor.pic.Serialise(strm);

    RepSec->ceil_pic = Sec->ceiling.pic;

    PutStream(&Msg, strm);
    if (!CanSendData()) { FlushMsg(&Msg); Connection->NeedsUpdate = true; return -1; }
  }

  // floor changes
  if (FloorChanged) {
    const bool MirrorChanged = (RepSec->floor_MirrorAlpha != Sec->floor.MirrorAlpha);

    strm.WriteUInt(CMD_Sector);
    strm.WriteUInt((vuint32)sidx);
    strm.WriteBit(false); // floor

    strm.WriteBit(RepSec->floor_dist != Sec->floor.dist);
    if (RepSec->floor_dist != Sec->floor.dist) {
      strm << Sec->floor.dist;
      strm << Sec->floor.TexZ;
    }
    strm.WriteBit(mround(RepSec->floor_xoffs) != mround(Sec->floor.xoffs));
    if (mround(RepSec->floor_xoffs) != mround(Sec->floor.xoffs)) strm << Sec->floor.xoffs;
    strm.WriteBit(mround(RepSec->floor_yoffs) != mround(Sec->floor.yoffs));
    if (mround(RepSec->floor_yoffs) != mround(Sec->floor.yoffs)) strm << Sec->floor.yoffs;
    strm.WriteBit(RepSec->floor_XScale != Sec->floor.XScale);
    if (RepSec->floor_XScale != Sec->floor.XScale) strm << Sec->floor.XScale;
    strm.WriteBit(RepSec->floor_YScale != Sec->floor.YScale);
    if (RepSec->floor_YScale != Sec->floor.YScale) strm << Sec->floor.YScale;
    strm.WriteBit(mround(RepSec->floor_Angle) != mround(Sec->floor.Angle));
    if (mround(RepSec->floor_Angle) != mround(Sec->floor.Angle)) strm << Sec->floor.Angle;
    strm.WriteBit(mround(RepSec->floor_BaseAngle) != mround(Sec->floor.BaseAngle));
    if (mround(RepSec->floor_BaseAngle) != mround(Sec->floor.BaseAngle)) strm << Sec->floor.BaseAngle;
    strm.WriteBit(mround(RepSec->floor_BaseYOffs) != mround(Sec->floor.BaseYOffs));
    if (mround(RepSec->floor_BaseYOffs) != mround(Sec->floor.BaseYOffs)) strm << Sec->floor.BaseYOffs;
    strm.WriteBit(RepSec->floor_SkyBox != FloorSkyBox);
    if (RepSec->floor_SkyBox != FloorSkyBox) strm << FloorSkyBox;
    strm.WriteBit(MirrorChanged);
    if (MirrorChanged) strm << Sec->floor.MirrorAlpha;

    RepSec->floor_dist = Sec->floor.dist;
    RepSec->floor_xoffs = Sec->floor.xoffs;
    RepSec->floor_yoffs = Sec->floor.yoffs;
    RepSec->floor_XScale = Sec->floor.XScale;
    RepSec->floor_YScale = Sec->floor.YScale;
    RepSec->floor_Angle = Sec->floor.Angle;
    RepSec->floor_BaseAngle = Sec->floor.BaseAngle;
    RepSec->floor_BaseYOffs = Sec->floor.BaseYOffs;
    RepSec->floor_SkyBox = FloorSkyBox;
    RepSec->floor_MirrorAlpha = Sec->floor.MirrorAlpha;

    PutStream(&Msg, strm);
    if (!CanSendData()) { FlushMsg(&Msg); Connection->NeedsUpdate = true; return -1; }
  }

  if (CeilChanged) {
    const bool MirrorChanged = (RepSec->ceil_MirrorAlpha != Sec->ceiling.MirrorAlpha);

    strm.WriteUInt(CMD_Sector);
    strm.WriteUInt((vuint32)sidx);
    strm.WriteBit(true); // ceiling

    strm.WriteBit(RepSec->ceil_dist != Sec->ceiling.dist);
    if (RepSec->ceil_dist != Sec->ceiling.dist) {
      strm << Sec->ceiling.dist;
      strm << Sec->ceiling.TexZ;
    }
    strm.WriteBit(mround(RepSec->ceil_xoffs) != mround(Sec->ceiling.xoffs));
    if (mround(RepSec->ceil_xoffs) != mround(Sec->ceiling.xoffs)) strm << Sec->ceiling.xoffs;
    strm.WriteBit(mround(RepSec->ceil_yoffs) != mround(Sec->ceiling.yoffs));
    if (mround(RepSec->ceil_yoffs) != mround(Sec->ceiling.yoffs)) strm << Sec->ceiling.yoffs;
    strm.WriteBit(RepSec->ceil_XScale != Sec->ceiling.XScale);
    if (RepSec->ceil_XScale != Sec->ceiling.XScale) strm << Sec->ceiling.XScale;
    strm.WriteBit(RepSec->ceil_YScale != Sec->ceiling.YScale);
    if (RepSec->ceil_YScale != Sec->ceiling.YScale) strm << Sec->ceiling.YScale;
    strm.WriteBit(mround(RepSec->ceil_Angle) != mround(Sec->ceiling.Angle));
    if (mround(RepSec->ceil_Angle) != mround(Sec->ceiling.Angle)) strm << Sec->ceiling.Angle;
    strm.WriteBit(mround(RepSec->ceil_BaseAngle) != mround(Sec->ceiling.BaseAngle));
    if (mround(RepSec->ceil_BaseAngle) != mround(Sec->ceiling.BaseAngle)) strm << Sec->ceiling.BaseAngle;
    strm.WriteBit(mround(RepSec->ceil_BaseYOffs) != mround(Sec->ceiling.BaseYOffs));
    if (mround(RepSec->ceil_BaseYOffs) != mround(Sec->ceiling.BaseYOffs)) strm << Sec->ceiling.BaseYOffs;
    strm.WriteBit(RepSec->ceil_SkyBox != CeilSkyBox);
    if (RepSec->ceil_SkyBox != CeilSkyBox) strm << CeilSkyBox;
    strm.WriteBit(MirrorChanged);
    if (MirrorChanged) strm << Sec->ceiling.MirrorAlpha;

    RepSec->ceil_dist = Sec->ceiling.dist;
    RepSec->ceil_xoffs = Sec->ceiling.xoffs;
    RepSec->ceil_yoffs = Sec->ceiling.yoffs;
    RepSec->ceil_XScale = Sec->ceiling.XScale;
    RepSec->ceil_YScale = Sec->ceiling.YScale;
    RepSec->ceil_Angle = Sec->ceiling.Angle;
    RepSec->ceil_BaseAngle = Sec->ceiling.BaseAngle;
    RepSec->ceil_BaseYOffs = Sec->ceiling.BaseYOffs;
    RepSec->ceil_SkyBox = CeilSkyBox;
    RepSec->ceil_MirrorAlpha = Sec->ceiling.MirrorAlpha;

    PutStream(&Msg, strm);
    if (!CanSendData()) { FlushMsg(&Msg); Connection->NeedsUpdate = true; return -1; }
  }

  if (LightChanged || FadeChanged || SkyChanged) {
    strm.WriteUInt(CMD_SectorLight);
    strm.WriteUInt((vuint32)sidx);

    strm.WriteBit(LightChanged);
    if (LightChanged) strm.WriteUInt(((vuint32)Sec->params.lightlevel)>>2); // 256

    strm.WriteBit(FadeChanged);
    if (FadeChanged) strm << Sec->params.Fade;

    strm.WriteBit(SkyChanged);
    if (SkyChanged) strm.WriteInt(Sec->Sky);

    RepSec->lightlevel = Sec->params.lightlevel;
    RepSec->Fade = Sec->params.Fade;
    RepSec->Sky = Sec->Sky;

    PutStream(&Msg, strm);
    if (!CanSendData()) { FlushMsg(&Msg); Connection->NeedsUpdate = true; return -1; }
  }

  return 0;
}


//==========================================================================
//
//  VLevelChannel::ParseSectorTexture
//
//==========================================================================
bool VLevelChannel::ParseSectorTexture (VMessageIn &Msg) {
  int sidx = (int)Msg.ReadUInt();
  if (Msg.IsError()) {
    GCon->Logf(NAME_DevNet, "%s: cannot read sector index", *GetDebugName());
    return false;
  }
  if (sidx < 0 || sidx >= Level->NumSectors) {
    GCon->Logf(NAME_DevNet, "%s: got invalid sector index %d (max is %d)", *GetDebugName(), sidx, Level->NumSectors-1);
    return false;
  }

  sector_t *Sec = &Level->Sectors[sidx];

  if (Msg.ReadBit()) {
    Sec->ceiling.pic.Serialise(Msg);
  } else {
    Sec->floor.pic.Serialise(Msg);
  }

  return !Msg.IsError();
}


//==========================================================================
//
//  VLevelChannel::ParseSectorLight
//
//==========================================================================
bool VLevelChannel::ParseSectorLight (VMessageIn &Msg) {
  int sidx = (int)Msg.ReadUInt();
  if (Msg.IsError()) {
    GCon->Logf(NAME_DevNet, "%s: cannot read sector index", *GetDebugName());
    return false;
  }
  if (sidx < 0 || sidx >= Level->NumSectors) {
    GCon->Logf(NAME_DevNet, "%s: got invalid sector index %d (max is %d)", *GetDebugName(), sidx, Level->NumSectors-1);
    return false;
  }

  sector_t *Sec = &Level->Sectors[sidx];

  if (Msg.ReadBit()) Sec->params.lightlevel = Msg.ReadUInt()<<2;
  if (Msg.ReadBit()) Msg << Sec->params.Fade;
  if (Msg.ReadBit()) Sec->Sky = Msg.ReadInt();

  return !Msg.IsError();
}


//==========================================================================
//
//  VLevelChannel::ParseSector
//
//==========================================================================
bool VLevelChannel::ParseSector (VMessageIn &Msg) {
  int sidx = (int)Msg.ReadUInt();
  if (Msg.IsError()) {
    GCon->Logf(NAME_DevNet, "%s: cannot read sector index", *GetDebugName());
    return false;
  }
  if (sidx < 0 || sidx >= Level->NumSectors) {
    GCon->Logf(NAME_DevNet, "%s: got invalid sector index %d (max is %d)", *GetDebugName(), sidx, Level->NumSectors-1);
    return false;
  }

  sector_t *Sec = &Level->Sectors[sidx];

  const float PrevFloorDist = Sec->floor.dist;
  const float PrevCeilDist = Sec->ceiling.dist;

  // `false` is floor
  if (Msg.ReadBit()) {
    // ceiling
    if (Msg.ReadBit()) {
      Msg << Sec->ceiling.dist;
      Msg << Sec->ceiling.TexZ;
    }
    if (Msg.ReadBit()) Sec->ceiling.xoffs = Msg.ReadInt();
    if (Msg.ReadBit()) Sec->ceiling.yoffs = Msg.ReadInt();
    if (Msg.ReadBit()) Msg << Sec->ceiling.XScale;
    if (Msg.ReadBit()) Msg << Sec->ceiling.YScale;
    if (Msg.ReadBit()) Sec->ceiling.Angle = Msg.ReadInt();
    if (Msg.ReadBit()) Sec->ceiling.BaseAngle = Msg.ReadInt();
    if (Msg.ReadBit()) Sec->ceiling.BaseYOffs = Msg.ReadInt();
    if (Msg.ReadBit()) Msg << Sec->ceiling.SkyBox;
    if (Msg.ReadBit()) Msg << Sec->ceiling.MirrorAlpha;
  } else {
    // floor
    if (Msg.ReadBit()) {
      Msg << Sec->floor.dist;
      Msg << Sec->floor.TexZ;
    }
    if (Msg.ReadBit()) Sec->floor.xoffs = Msg.ReadInt();
    if (Msg.ReadBit()) Sec->floor.yoffs = Msg.ReadInt();
    if (Msg.ReadBit()) Msg << Sec->floor.XScale;
    if (Msg.ReadBit()) Msg << Sec->floor.YScale;
    if (Msg.ReadBit()) Sec->floor.Angle = Msg.ReadInt();
    if (Msg.ReadBit()) Sec->floor.BaseAngle = Msg.ReadInt();
    if (Msg.ReadBit()) Sec->floor.BaseYOffs = Msg.ReadInt();
    if (Msg.ReadBit()) Msg << Sec->floor.SkyBox;
    if (Msg.ReadBit()) Msg << Sec->floor.MirrorAlpha;
  }

  if (Msg.IsError()) return false;

  if (PrevFloorDist != Sec->floor.dist || PrevCeilDist != Sec->ceiling.dist) {
    //GCon->Logf("updating sector #%d", (int)(ptrdiff_t)(Sec-&GClLevel->Sectors[0]));
    Level->CalcSecMinMaxs(Sec);
  }

  return true;
}


//==========================================================================
//
//  VLevelChannel::UpdatePolyObj
//
//==========================================================================
int VLevelChannel::UpdatePolyObj (VMessageOut &Msg, VBitStreamWriter &strm, int oidx) {
  vassert(oidx >= 0 && oidx < Level->NumPolyObjs);

  polyobj_t *Po = Level->PolyObjs[oidx];
  if (!Connection->CheckFatPVS(Po->GetSubsector())) return 0;

  rep_polyobj_t *RepPo = &PolyObjs[oidx];
  if (RepPo->startSpot.x == Po->startSpot.x &&
      RepPo->startSpot.y == Po->startSpot.y &&
      RepPo->angle == Po->angle)
  {
    return 0;
  }

  strm.WriteUInt(CMD_PolyObj);
  strm.WriteUInt((vuint32)oidx);

  strm.WriteBit(RepPo->startSpot.x != Po->startSpot.x);
  if (RepPo->startSpot.x != Po->startSpot.x) strm << Po->startSpot.x;
  strm.WriteBit(RepPo->startSpot.y != Po->startSpot.y);
  if (RepPo->startSpot.y != Po->startSpot.y) strm << Po->startSpot.y;
  strm.WriteBit(RepPo->angle != Po->angle);
  if (RepPo->angle != Po->angle) strm << Po->angle;

  RepPo->startSpot = Po->startSpot;
  RepPo->angle = Po->angle;

  return 1;
}


//==========================================================================
//
//  VLevelChannel::ParsePolyObj
//
//==========================================================================
bool VLevelChannel::ParsePolyObj (VMessageIn &Msg) {
  int oidx = (int)Msg.ReadUInt();
  if (Msg.IsError()) {
    GCon->Logf(NAME_DevNet, "%s: cannot read polyobject index", *GetDebugName());
    return false;
  }
  if (oidx < 0 || oidx >= Level->NumPolyObjs) {
    GCon->Logf(NAME_DevNet, "%s: got invalid polyobject index %d (max is %d)", *GetDebugName(), oidx, Level->NumPolyObjs-1);
    return false;
  }

  polyobj_t *Po = Level->PolyObjs[oidx];
  TVec Pos = Po->startSpot;
  if (Msg.ReadBit()) Msg << Pos.x;
  if (Msg.ReadBit()) Msg << Pos.y;
  if (Msg.IsError()) return false;
  if (Pos != Po->startSpot) Level->MovePolyobj(Po->tag, Pos.x-Po->startSpot.x, Pos.y-Po->startSpot.y);
  if (Msg.ReadBit()) {
    float a = 0;
    Msg << a;
    if (Msg.IsError()) return false;
    Level->RotatePolyobj(Po->tag, a-Po->angle);
  }

  return !Msg.IsError();
}


//==========================================================================
//
//  VLevelChannel::UpdateCameraTexture
//
//==========================================================================
int VLevelChannel::UpdateCameraTexture (VMessageOut &Msg, VBitStreamWriter &strm, int idx) {
  if (idx > 255) return 0; // oops
  vassert(idx >= 0 && idx < Level->CameraTextures.length());

  // grow replication array if needed
  if (CameraTextures.Num() == idx) {
    VCameraTextureInfo &C = CameraTextures.Alloc();
    C.Camera = nullptr;
    C.TexNum = -1;
    C.FOV = 0;
  }

  VCameraTextureInfo &Cam = Level->CameraTextures[idx];
  VCameraTextureInfo &RepCam = CameraTextures[idx];
  VEntity *CamEnt = Cam.Camera;

  if (CamEnt && !Connection->ObjMap->CanSerialiseObject(CamEnt)) CamEnt = nullptr;
  if (CamEnt == RepCam.Camera && Cam.TexNum == RepCam.TexNum && Cam.FOV == RepCam.FOV) return 0;

  // send message
  strm.WriteUInt(CMD_CamTex);
  strm.WriteUInt((vuint32)idx);

  Connection->ObjMap->SerialiseObject(strm, *(VObject **)&CamEnt);
  strm.WriteInt(Cam.TexNum);
  strm.WriteInt(Cam.FOV);

  // update replication info
  RepCam.Camera = CamEnt;
  RepCam.TexNum = Cam.TexNum;
  RepCam.FOV = Cam.FOV;

  return 1;
}


//==========================================================================
//
//  VLevelChannel::ParseCameraTexture
//
//==========================================================================
bool VLevelChannel::ParseCameraTexture (VMessageIn &Msg) {
  int idx = (int)Msg.ReadUInt();
  if (Msg.IsError()) {
    GCon->Logf(NAME_DevNet, "%s: cannot read camtex index", *GetDebugName());
    return false;
  }
  if (idx < 0 || idx > 255) {
    GCon->Logf(NAME_DevNet, "%s: got invalid camtex index %d (max is %d)", *GetDebugName(), idx, 1023);
    return false;
  }

  while (Level->CameraTextures.Num() <= idx) {
    VCameraTextureInfo &C = Level->CameraTextures.Alloc();
    C.Camera = nullptr;
    C.TexNum = -1;
    C.FOV = 0;
  }
  VCameraTextureInfo &Cam = Level->CameraTextures[idx];
  Connection->ObjMap->SerialiseObject(Msg, *(VObject **)&Cam.Camera);
  Cam.TexNum = Msg.ReadInt();
  Cam.FOV = Msg.ReadInt();

  return !Msg.IsError();
}


//==========================================================================
//
//  VLevelChannel::UpdateTranslation
//
//==========================================================================
int VLevelChannel::UpdateTranslation (VMessageOut &Msg, VBitStreamWriter &strm, int idx) {
  if (idx > 4095) return 0; // artificial limit
  vassert(idx >= 0 && idx < Level->Translations.length());

  // grow replication array if needed
  if (Translations.Num() == idx) Translations.Alloc();
  if (!Level->Translations[idx]) return 0;

  VTextureTranslation *Tr = Level->Translations[idx];
  TArray<VTextureTranslation::VTransCmd> &Rep = Translations[idx];
  bool Eq = (Tr->Commands.Num() == Rep.Num());
  if (Eq) {
    for (int j = 0; j < Rep.Num(); ++j) {
      if (memcmp(&Tr->Commands[j], &Rep[j], sizeof(Rep[j]))) {
        Eq = false;
        break;
      }
    }
  }
  if (Eq) return 0;

  // send message
  strm.WriteUInt(CMD_LevelTrans);
  strm.WriteUInt((vuint32)idx);

  strm.WriteUInt((vuint32)Tr->Commands.Num());
  Rep.SetNum(Tr->Commands.Num());
  for (int j = 0; j < Tr->Commands.Num(); ++j) {
    VTextureTranslation::VTransCmd &C = Tr->Commands[j];
    strm.WriteUInt(C.Type);
         if (C.Type == 0) strm << C.Start << C.End << C.R1 << C.R2;
    else if (C.Type == 1) strm << C.Start << C.End << C.R1 << C.G1 << C.B1 << C.R2 << C.G2 << C.B2;
    else if (C.Type == 2) strm << C.Start << C.End << C.R1 << C.G1 << C.B1 << C.R2 << C.G2 << C.B2;
    else if (C.Type == 3) strm << C.Start << C.End << C.R1 << C.G1 << C.B1;
    else if (C.Type == 4) strm << C.Start << C.End << C.R1 << C.G1 << C.B1 << C.R2;
    Rep[j] = C;
  }

  return 1;
}


//==========================================================================
//
//  VLevelChannel::ParseTranslation
//
//==========================================================================
bool VLevelChannel::ParseTranslation (VMessageIn &Msg) {
  int idx = (int)Msg.ReadUInt();
  if (Msg.IsError()) {
    GCon->Logf(NAME_DevNet, "%s: cannot read translation index", *GetDebugName());
    return false;
  }
  if (idx < 0 || idx > 4095) {
    GCon->Logf(NAME_DevNet, "%s: got invalid translation index %d (max is %d)", *GetDebugName(), idx, 4095);
    return false;
  }

  while (Level->Translations.Num() <= idx) Level->Translations.Append(nullptr);
  VTextureTranslation *Tr = Level->Translations[idx];
  if (!Tr) {
    Tr = new VTextureTranslation;
    Level->Translations[idx] = Tr;
  }
  Tr->Clear();

  int Count = (int)Msg.ReadUInt();
  for (int j = 0; j < Count; ++j) {
    vuint8 Type = Msg.ReadUInt();
    if (Msg.IsError()) return false;
    if (Type == 0) {
      vuint8 Start;
      vuint8 End;
      vuint8 SrcStart;
      vuint8 SrcEnd;
      Msg << Start << End << SrcStart << SrcEnd;
      if (Msg.IsError()) return false;
      Tr->MapToRange(Start, End, SrcStart, SrcEnd);
    } else if (Type == 1) {
      vuint8 Start;
      vuint8 End;
      vuint8 R1;
      vuint8 G1;
      vuint8 B1;
      vuint8 R2;
      vuint8 G2;
      vuint8 B2;
      Msg << Start << End << R1 << G1 << B1 << R2 << G2 << B2;
      if (Msg.IsError()) return false;
      Tr->MapToColors(Start, End, R1, G1, B1, R2, G2, B2);
    } else if (Type == 2) {
      vuint8 Start;
      vuint8 End;
      vuint8 R1;
      vuint8 G1;
      vuint8 B1;
      vuint8 R2;
      vuint8 G2;
      vuint8 B2;
      Msg << Start << End << R1 << G1 << B1 << R2 << G2 << B2;
      if (Msg.IsError()) return false;
      Tr->MapDesaturated(Start, End, R1/128.0f, G1/128.0f, B1/128.0f, R2/128.0f, G2/128.0f, B2/128.0f);
    } else if (Type == 3) {
      vuint8 Start;
      vuint8 End;
      vuint8 R1;
      vuint8 G1;
      vuint8 B1;
      Msg << Start << End << R1 << G1 << B1;
      if (Msg.IsError()) return false;
      Tr->MapBlended(Start, End, R1, G1, B1);
    } else if (Type == 4) {
      vuint8 Start;
      vuint8 End;
      vuint8 R1;
      vuint8 G1;
      vuint8 B1;
      vuint8 Amount;
      Msg << Start << End << R1 << G1 << B1 << Amount;
      if (Msg.IsError()) return false;
      Tr->MapTinted(Start, End, R1, G1, B1, Amount);
    }
  }

  return !Msg.IsError();
}


//==========================================================================
//
//  VLevelChannel::UpdateBodyQueueTran
//
//==========================================================================
int VLevelChannel::UpdateBodyQueueTran (VMessageOut &Msg, VBitStreamWriter &strm, int idx) {
  if (idx > 8191) return 0; // artificial limit
  vassert(idx >= 0 && idx < Level->BodyQueueTrans.length());

  // grow replication array if needed
  if (BodyQueueTrans.Num() == idx) BodyQueueTrans.Alloc().TranslStart = 0;
  if (!Level->BodyQueueTrans[idx]) return 0;
  VTextureTranslation *Tr = Level->BodyQueueTrans[idx];
  if (!Tr->TranslStart) return 0;
  VBodyQueueTrInfo &Rep = BodyQueueTrans[idx];
  if (Tr->TranslStart == Rep.TranslStart && Tr->TranslEnd == Rep.TranslEnd && Tr->Color == Rep.Color) return 0;

  // send message
  strm.WriteUInt(CMD_BodyQueueTrans);
  strm.WriteUInt((vuint32)idx);
  strm << Tr->TranslStart << Tr->TranslEnd;
  strm.WriteUInt((vuint32)Tr->Color);

  Rep.TranslStart = Tr->TranslStart;
  Rep.TranslEnd = Tr->TranslEnd;
  Rep.Color = Tr->Color;

  return 1;
}


//==========================================================================
//
//  VLevelChannel::ParseBodyQueueTran
//
//==========================================================================
bool VLevelChannel::ParseBodyQueueTran (VMessageIn &Msg) {
  int idx = (int)Msg.ReadUInt();
  if (Msg.IsError()) {
    GCon->Logf(NAME_DevNet, "%s: cannot read body queue index", *GetDebugName());
    return false;
  }
  if (idx < 0 || idx > 8191) {
    GCon->Logf(NAME_DevNet, "%s: got invalid body queue index %d (max is %d)", *GetDebugName(), idx, 8191);
    return false;
  }

  while (Level->BodyQueueTrans.Num() <= idx) Level->BodyQueueTrans.Append(nullptr);
  VTextureTranslation *Tr = Level->BodyQueueTrans[idx];
  if (!Tr) {
    Tr = new VTextureTranslation;
    Level->BodyQueueTrans[idx] = Tr;
  }
  Tr->Clear();

  vuint8 TrStart = 0;
  vuint8 TrEnd = 0;
  Msg << TrStart << TrEnd;
  vint32 Col = Msg.ReadUInt();
  Tr->BuildPlayerTrans(TrStart, TrEnd, Col);

  return !Msg.IsError();
}


//==========================================================================
//
//  VLevelChannel::UpdateStaticLight
//
//==========================================================================
int VLevelChannel::UpdateStaticLight (VMessageOut &Msg, VBitStreamWriter &strm, int idx, bool forced) {
  if (idx > 65535) return 0; // arbitrary limit
  vassert(idx >= 0 && idx < Level->NumStaticLights);

  rep_light_t &L = Level->StaticLights[idx];
  if (!forced && !(L.Flags&rep_light_t::LightChanged)) return 0;

  strm.WriteUInt(CMD_StaticLight);

  TVec lOrigin = L.Origin;
  float lRadius = L.Radius;
  vuint32 lColor = L.Color;
  vuint32 ouid = L.OwnerUId;

  strm << STRM_INDEX_U(ouid) << lOrigin << lRadius << lColor;
  strm.WriteBit(!!L.ConeAngle);
  if (L.ConeAngle) {
    TVec lConeDir = L.ConeDir;
    float lConeAngle = L.ConeAngle;
    strm << lConeDir << lConeAngle;
  }

  if (!forced) L.Flags &= ~rep_light_t::LightChanged;

  return 1;
}


//==========================================================================
//
//  VLevelChannel::ParseStaticLight
//
//==========================================================================
bool VLevelChannel::ParseStaticLight (VMessageIn &Msg) {
  vuint32 owneruid;
  TVec Origin;
  float Radius;
  vuint32 Color;
  Msg << STRM_INDEX_U(owneruid) << Origin << Radius << Color;
  bool isCone = Msg.ReadBit();
  if (Msg.IsError()) {
    GCon->Logf(NAME_DevNet, "%s: cannot read static light header", *GetDebugName());
    return false;
  }
  if (isCone) {
    TVec ConeDir;
    float ConeAngle;
    Msg << ConeDir << ConeAngle;
    if (Msg.IsError()) {
      GCon->Logf(NAME_DevNet, "%s: cannot read static light cone data", *GetDebugName());
      return false;
    }
    #ifdef CLIENT
    Level->AddStaticLightRGB(owneruid, Origin, Radius, Color, ConeDir, ConeAngle);
    //Level->Renderer->AddStaticLightRGB(owneruid, Origin, Radius, Color, ConeDir, ConeAngle);
    #endif
  } else {
    #ifdef CLIENT
    Level->AddStaticLightRGB(owneruid, Origin, Radius, Color);
    //Level->Renderer->AddStaticLightRGB(owneruid, Origin, Radius, Color);
    #endif
  }

  return !Msg.IsError();
}


#define GEN_UPDATE(name_) do { \
  /*GCon->Log(NAME_DevNet, "VLevelChannel::Update -- " # name_ # "s");*/ \
  for (int i = 0; i < Level->Num ## name_ ## s; ++i) { \
    if (Update##name_(Msg, strm, i)) { \
      PutStream(&Msg, strm); \
      if (!CanSendData()) { FlushMsg(&Msg); Connection->NeedsUpdate = true; return; } \
    } \
  } \
} while (0)


#define GEN_UPDATE_ARR(name_) do { \
  /*GCon->Log(NAME_DevNet, "VLevelChannel::Update -- " # name_ # "s");*/ \
  for (int i = 0; i < name_ ## s.length(); ++i) { \
    if (Update##name_(Msg, strm, i)) { \
      PutStream(&Msg, strm); \
      if (!CanSendData()) { FlushMsg(&Msg); Connection->NeedsUpdate = true; return; } \
    } \
  } \
} while (0)


//==========================================================================
//
//  VLevelChannel::Update
//
//==========================================================================
void VLevelChannel::Update () {
  if (Closing) return; // just in case
  // if network connection is saturated, do nothing
  if (!CanSendData()) { Connection->NeedsUpdate = true; return; }

  VMessageOut Msg(this);
  VBitStreamWriter strm(MAX_MSG_SIZE_BITS+64, false); // no expand

  GEN_UPDATE(Line);
  GEN_UPDATE(Side);
  GEN_UPDATE(Sector);
  GEN_UPDATE(PolyObj);
  GEN_UPDATE_ARR(CameraTexture);
  GEN_UPDATE_ARR(Translation);
  GEN_UPDATE_ARR(BodyQueueTran);

  // do not send static light updates yet
  // we need to move static light replication info here first
  /*
  for (int i = 0; i < Level->NumStaticLights; ++i) {
    const int oldsize = strm.GetNumBits();
    UpdateStaticLight(strm, i, false); // not forced
    if (strm.GetNumBits() != oldsize) {
      PutStream(&Msg, strm);
      if (!CanSendData()) { FlushMsg(&Msg); Connection->NeedsUpdate = true; return; }
    }
  }
  */

  FlushMsg(&Msg);
}


//==========================================================================
//
//  VLevelChannel::ParseMessage
//
//==========================================================================
void VLevelChannel::ParseMessage (VMessageIn &Msg) {
  if (!Connection->IsClient()) {
    GCon->Logf(NAME_DevNet, "%s: client sent some level updates, ignoring", *GetDebugName());
    return;
  }

  bool err = false;
  while (!err && !Msg.AtEnd()) {
    int Cmd = Msg.ReadUInt();
    if (Msg.IsError()) {
      GCon->Logf(NAME_DevNet, "%s: cannot read command", *GetDebugName());
      err = true;
      break;
    }
    switch (Cmd) {
      case CMD_Line: err = !ParseLine(Msg); if (err) GCon->Logf(NAME_DevNet, "%s: error reading line update", *GetDebugName()); break;
      case CMD_Side: err = !ParseSide(Msg); if (err) GCon->Logf(NAME_DevNet, "%s: error reading side update", *GetDebugName()); break;
      case CMD_SideTexture: err = !ParseSideTexture(Msg); if (err) GCon->Logf(NAME_DevNet, "%s: error reading side update", *GetDebugName()); break;
      case CMD_SideTOffset: err = !ParseSideTOffset(Msg); if (err) GCon->Logf(NAME_DevNet, "%s: error reading side update", *GetDebugName()); break;
      case CMD_SideROffset: err = !ParseSideROffset(Msg); if (err) GCon->Logf(NAME_DevNet, "%s: error reading side update", *GetDebugName()); break;
      case CMD_SideScale: err = !ParseSideScale(Msg); if (err) GCon->Logf(NAME_DevNet, "%s: error reading side update", *GetDebugName()); break;
      case CMD_Sector: err = !ParseSector(Msg); if (err) GCon->Logf(NAME_DevNet, "%s: error reading sector update", *GetDebugName()); break;
      case CMD_SectorTexture: err = !ParseSectorTexture(Msg); if (err) GCon->Logf(NAME_DevNet, "%s: error reading sector update", *GetDebugName()); break;
      case CMD_SectorLight: err = !ParseSectorLight(Msg); if (err) GCon->Logf(NAME_DevNet, "%s: error reading sector update", *GetDebugName()); break;
      case CMD_PolyObj: err = !ParsePolyObj(Msg); if (err) GCon->Logf(NAME_DevNet, "%s: error reading polyobject update", *GetDebugName()); break;
      case CMD_CamTex: err = !ParseCameraTexture(Msg); if (err) GCon->Logf(NAME_DevNet, "%s: error reading camtex update", *GetDebugName()); break;
      case CMD_LevelTrans: err = !ParseTranslation(Msg); if (err) GCon->Logf(NAME_DevNet, "%s: error reading translation update", *GetDebugName()); break;
      case CMD_BodyQueueTrans: err = !ParseBodyQueueTran(Msg); if (err) GCon->Logf(NAME_DevNet, "%s: error reading body queue update", *GetDebugName()); break;
      case CMD_StaticLight: err = !ParseStaticLight(Msg); if (err) GCon->Logf(NAME_DevNet, "%s: error reading static light update", *GetDebugName()); break;
      case CMD_ResetStaticLights: //TODO
        // currently, there is nothing to do here, because client level is spawned without entities anyway
        #ifdef CLIENT
        Level->ResetStaticLights();
        #endif
        break;
      case CMD_NewLevel:
        #ifdef CLIENT
        Msg << csi.mapname;
        Msg << csi.maphash;
        Msg << csi.modhash;
        csi.maxclients = Msg.ReadUInt();
        csi.deathmatch = Msg.ReadUInt();
        if (Msg.IsError() || csi.maxclients < 1 || csi.maxclients > MAXPLAYERS) {
          GCon->Logf(NAME_DevNet, "%s: invalid level handshake sequence", *GetDebugName());
          err = true;
          break;
        }
        if (csi.modhash != FL_GetNetWadsHash()) {
          GCon->Logf(NAME_DevNet, "%s: server has different wad set", *GetDebugName());
          err = true;
          break;
        }
        // prepare serveinfo buffer
        serverInfoBuf.clear();
        //CL_ParseServerInfo(Msg);
        GCon->Logf(NAME_DevNet, "%s: received new map request (map name is '%s')", *GetDebugName(), *csi.mapname);
        #else
        Host_Error("CMD_NewLevel from client");
        #endif
        break;
      case CMD_ServerInfo:
        #ifdef CLIENT
        while (!Msg.AtEnd()) {
          vuint8 ch = 0;
          Msg << ch;
          if (Msg.IsError()) {
            GCon->Logf(NAME_DevNet, "%s: invalid server info packet (got %d server info bytes so far)", *GetDebugName(), serverInfoBuf.length());
            err = true;
            break;
          }
          serverInfoBuf += (char)ch;
          if (serverInfoBuf.length() > 1024*1024) {
            GCon->Logf(NAME_DevNet, "%s: server info packet too long", *GetDebugName());
            err = true;
            break;
          }
        }
        #else
        Host_Error("CMD_ServerInfo from client");
        #endif
        break;
      case CMD_ServerInfoEnd:
        #ifdef CLIENT
        GCon->Logf(NAME_DevNet, "%s: received server info: \"%s\"", *GetDebugName(), *serverInfoBuf.quote());
        csi.sinfo = serverInfoBuf;
        serverInfoBuf.clear();
        CL_ParseServerInfo(&csi); // this loads map
        csi.mapname.clear();
        csi.sinfo.clear();
        #else
        Host_Error("CMD_ServerInfoEnd from client");
        #endif
        break;
      case CMD_PreRender: // sent by server, received by client
        #ifdef CLIENT
        GCon->Logf(NAME_DevNet, "%s: received prerender", *GetDebugName());
        if (Level->Renderer) Level->Renderer->PreRender();
        if (cls.signon) {
          GCon->Logf(NAME_DevNet, "%s: Client_Spawn command already sent", *GetDebugName());
          err = true;
          break;
        }
        if (!UserInfoSent) {
          cl->eventServerSetUserInfo(cls.userinfo);
          UserInfoSent = true;
        }
        cls.gotmap = 2;
        Connection->SendCommand("PreSpawn\n");
        GCmdBuf << "HideConsole\n";
        #else
        Host_Error("CMD_PreRender from client");
        #endif
        break;
      case CMD_ResetLevel:
        #ifdef CLIENT
        GCon->Logf(NAME_DevNet, "%s: received level reset", *GetDebugName());
        ResetLevel();
        #else
        Host_Error("CMD_PreRender from CMD_ResetLevel");
        #endif
        break;
      default:
        GCon->Logf(NAME_DevNet, "%s: received invalid level command from client (%d)", *GetDebugName(), Cmd);
        err = true;
        break;
    }
  }

  if (err) {
    GCon->Logf(NAME_DevNet, "%s: clsing connection due to level update errors", *GetDebugName());
    Connection->Close();
  }
}
