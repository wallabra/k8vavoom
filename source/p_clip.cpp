//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "gamedefs.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

struct VViewClipper::VClipNode
{
  float   From;
  float   To;
  VClipNode *Prev;
  VClipNode *Next;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

#ifdef CLIENT
extern VCvarF r_lights_radius;
#endif

static VCvarB clip_bsp("clip_bsp", true, "Clip geometry behind some BSP nodes?"/*, CVAR_Archive*/);
static VCvarB clip_enabled("clip_enabled", true, "Do geometry cliping optimizations?"/*, CVAR_Archive*/);
static VCvarB clip_trans_hack("clip_trans_hack", true, "Do translucent clipping hack?"/*, CVAR_Archive*/);

// CODE --------------------------------------------------------------------

//==========================================================================
//
//  VViewClipper::VViewClipper
//
//==========================================================================

VViewClipper::VViewClipper()
  : FreeClipNodes(nullptr)
  , ClipHead(nullptr)
  , ClipTail(nullptr)
{
}

//==========================================================================
//
//  VViewClipper::~VViewClipper
//
//==========================================================================

VViewClipper::~VViewClipper()
{
  //guard(VViewClipper::~VViewClipper);
  ClearClipNodes(TVec(), nullptr);
  VClipNode *Node = FreeClipNodes;
  while (Node)
  {
    VClipNode *Next = Node->Next;

    delete Node;
    Node = nullptr;

    if (Next)
    {
      Node = Next;
    }
  }
  //unguard;
}

//==========================================================================
//
//  VViewClipper::NewClipNode
//
//==========================================================================

VViewClipper::VClipNode *VViewClipper::NewClipNode()
{
  guard(VViewClipper::NewClipNode);
  VClipNode *Ret = FreeClipNodes;

  if (Ret)
  {
    FreeClipNodes = Ret->Next;
  }
  else
  {
    Ret = new VClipNode();
  }

  return Ret;
  unguard;
}

//==========================================================================
//
//  VViewClipper::RemoveClipNode
//
//==========================================================================

void VViewClipper::RemoveClipNode(VViewClipper::VClipNode *Node)
{
  guard(VViewClipper::RemoveClipNode);
  if (Node->Next)
  {
    Node->Next->Prev = Node->Prev;
  }

  if (Node->Prev)
  {
    Node->Prev->Next = Node->Next;
  }

  if (Node == ClipHead)
  {
    ClipHead = Node->Next;
  }

  if (Node == ClipTail)
  {
    ClipTail = Node->Prev;
  }
  Node->Next = FreeClipNodes;
  FreeClipNodes = Node;
  unguard;
}

//==========================================================================
//
//  VViewClipper::ClearClipNodes
//
//==========================================================================

void VViewClipper::ClearClipNodes(const TVec &AOrigin, VLevel *ALevel)
{
  guard(VViewClipper::ClearClipNodes);
  if (ClipHead)
  {
    ClipTail->Next = FreeClipNodes;
    FreeClipNodes = ClipHead;
  }
  ClipHead = nullptr;
  ClipTail = nullptr;
  Origin = AOrigin;
  Level = ALevel;
  unguard;
}

//==========================================================================
//
//  VViewClipper::ClipInitFrustrumRange
//
//==========================================================================

void VViewClipper::ClipInitFrustrumRange(const TAVec &viewangles,
  const TVec &viewforward, const TVec &viewright, const TVec &viewup,
  float fovx, float fovy)
{
  guard(VViewClipper::ClipInitFrustrumRange);
  check(!ClipHead);

  if (viewforward.z > 0.9 || viewforward.z < -0.9)
  {
    //  Looking up or down, can see behind.
    return;
  }

  TVec Pts[4];
  TVec TransPts[4];
  Pts[0] = TVec(1, fovx, fovy);
  Pts[1] = TVec(1, fovx, -fovy);
  Pts[2] = TVec(1, -fovx, fovy);
  Pts[3] = TVec(1, -fovx, -fovy);
  TVec clipforward = Normalise(TVec(viewforward.x, viewforward.y, 0.0));
  float d1 = 0;
  float d2 = 0;

  for (int i = 0; i < 4; i++)
  {
    TransPts[i].x = Pts[i].y * viewright.x + Pts[i].z * viewup.x + Pts[i].x * viewforward.x;
    TransPts[i].y = Pts[i].y * viewright.y + Pts[i].z * viewup.y + Pts[i].x * viewforward.y;
    TransPts[i].z = Pts[i].y * viewright.z + Pts[i].z * viewup.z + Pts[i].x * viewforward.z;

    if (DotProduct(TransPts[i], clipforward) <= 0)
    {
      //  Player can see behind.
      return;
    }
    float a = matan(TransPts[i].y, TransPts[i].x);

    if (a < 0.0)
      a += 360.0;
    float d = AngleMod180(a - viewangles.yaw);

    if (d1 > d)
      d1 = d;

    if (d2 < d)
      d2 = d;
  }
  float a1 = AngleMod(viewangles.yaw + d1);
  float a2 = AngleMod(viewangles.yaw + d2);

  if (a1 > a2)
  {
    ClipHead = NewClipNode();
    ClipTail = ClipHead;
    ClipHead->From = a2;
    ClipHead->To = a1;
    ClipHead->Prev = nullptr;
    ClipHead->Next = nullptr;
  }
  else
  {
    ClipHead = NewClipNode();
    ClipHead->From = 0.0;
    ClipHead->To = a1;
    ClipTail = NewClipNode();
    ClipTail->From = a2;
    ClipTail->To = 360.0;
    ClipHead->Prev = nullptr;
    ClipHead->Next = ClipTail;
    ClipTail->Prev = ClipHead;
    ClipTail->Next = nullptr;
  }
  unguard;
}

//==========================================================================
//
//  VViewClipper::ClipToRanges
//
//==========================================================================

void VViewClipper::ClipToRanges(const VViewClipper &Range)
{
  guard(VViewClipper::ClipToRanges);
  if (!Range.ClipHead)
  {
    //  No ranges, everything is clipped away.
    DoAddClipRange(0.0, 360.0);
    return;
  }

  //  Add head and tail ranges.
  if (Range.ClipHead->From > 0.0)
  {
    DoAddClipRange(0.0, Range.ClipHead->From);
  }

  if (Range.ClipTail->To < 360.0)
  {
    DoAddClipRange(Range.ClipTail->To, 360.0);
  }

  //  Add middle ranges.
  for (VClipNode *N = Range.ClipHead; N->Next; N = N->Next)
  {
    DoAddClipRange(N->To, N->Next->From);
  }
  unguard;
}

//==========================================================================
//
//  VViewClipper::DoAddClipRange
//
//==========================================================================

void VViewClipper::DoAddClipRange(float From, float To)
{
  guard(VViewClipper::DoAddClipRange);
  if (!ClipHead)
  {
    ClipHead = NewClipNode();
    ClipTail = ClipHead;
    ClipHead->From = From;
    ClipHead->To = To;
    ClipHead->Prev = nullptr;
    ClipHead->Next = nullptr;

    return;
  }

  for (VClipNode *Node = ClipHead; Node; Node = Node->Next)
  {
    if (Node->To < From)
    {
      //  Before this range.
      continue;
    }

    if (To < Node->From)
    {
      //  Insert a new clip range before current one.
      VClipNode *N = NewClipNode();
      N->From = From;
      N->To = To;
      N->Prev = Node->Prev;
      N->Next = Node;

      if (Node->Prev)
      {
        Node->Prev->Next = N;
      }
      else
      {
        ClipHead = N;
      }
      Node->Prev = N;

      return;
    }

    if (Node->From <= From && Node->To >= To)
    {
      //  It contains this range.
      return;
    }

    if (From < Node->From)
    {
      //  Extend start of the current range.
      Node->From = From;
    }

    if (To <= Node->To)
    {
      //  End is included, so we are done here.
      return;
    }

    //  Merge with following nodes if needed.
    while (Node->Next && Node->Next->From <= To)
    {
      Node->To = Node->Next->To;
      RemoveClipNode(Node->Next);
    }

    if (To > Node->To)
    {
      //  Extend end.
      Node->To = To;
    }

    //  We are done here.
    return;
  }

  //  If we are here it means it's a new range at the end.
  VClipNode *NewTail = NewClipNode();
  NewTail->From = From;
  NewTail->To = To;
  NewTail->Prev = ClipTail;
  NewTail->Next = nullptr;
  ClipTail->Next = NewTail;
  ClipTail = NewTail;
  unguard;
}

//==========================================================================
//
//  VViewClipper::AddClipRange
//
//==========================================================================

void VViewClipper::AddClipRange(float From, float To)
{
  guard(VViewClipper::AddClipRange);
  if (From > To)
  {
    DoAddClipRange(0.0, To);
    DoAddClipRange(From, 360.0);
  }
  else
  {
    DoAddClipRange(From, To);
  }
  unguard;
}

//==========================================================================
//
//  VViewClipper::DoIsRangeVisible
//
//==========================================================================

bool VViewClipper::DoIsRangeVisible(float From, float To)
{
  guard(VViewClipper::DoIsRangeVisible);
  for (VClipNode *N = ClipHead; N; N = N->Next)
  {
    if (From >= N->From && To <= N->To)
    {
      return false;
    }
  }
  return true;
  unguard;
}

//==========================================================================
//
//  VViewClipper::IsRangeVisible
//
//==========================================================================

bool VViewClipper::IsRangeVisible(float From, float To)
{
  guard(VViewClipper::IsRangeVisible);
  if (From > To)
  {
    return DoIsRangeVisible(0.0, To) || DoIsRangeVisible(From, 360.0);
  }
  else
  {
    return DoIsRangeVisible(From, To);
  }
  unguard;
}

//==========================================================================
//
//  VViewClipper::ClipIsFull
//
//==========================================================================

bool VViewClipper::ClipIsFull()
{
  guard(VViewClipper::ClipIsFull);
  return ClipHead && ClipHead->From == 0.0 && ClipHead->To == 360.0;
  unguard;
}

//==========================================================================
//
//  VViewClipper::PointToClipAngle
//
//==========================================================================

float VViewClipper::PointToClipAngle(const TVec &Pt)
{
  float Ret = matan(Pt.y - Origin.y, Pt.x - Origin.x);

  if (Ret < 0.0)
    Ret += 360.0;
  return Ret;
}

//==========================================================================
//
//  VViewClipper::ClipIsBBoxVisible
//
//==========================================================================

bool VViewClipper::ClipIsBBoxVisible(const float *BBox, bool shadowslight, const TVec &CurrLightPos,
  float CurrLightRadius)
{
  guard(VViewClipper::ClipIsBBoxVisible);
  if (!ClipHead)
  {
    //  No clip nodes yet.
    return true;
  }

  if (!clip_enabled)
    return true;

#ifdef CLIENT
  if (shadowslight)
  {
    if (BBox[0] <= CurrLightPos.x && BBox[3] >= CurrLightPos.x &&
        BBox[1] <= CurrLightPos.y && BBox[4] >= CurrLightPos.y)
    {
      //  Viewer is inside the box.
      return true;
    }
  }
  else
#endif
  if (BBox[0] <= Origin.x && BBox[3] >= Origin.x &&
      BBox[1] <= Origin.y && BBox[4] >= Origin.y)
  {
    //  Viewer is inside the box.
    return true;
  }

  TVec v1;
  TVec v2;
  v1.z = v2.z = 0;
#ifdef CLIENT
  if (shadowslight)
  {
    if (BBox[0] > CurrLightPos.x)
    {
      if (BBox[1] > CurrLightPos.y)
      {
        v1.x = BBox[3];
        v1.y = BBox[1];
        v2.x = BBox[0];
        v2.y = BBox[4];
      }
      else if (BBox[4] < CurrLightPos.y)
      {
        v1.x = BBox[0];
        v1.y = BBox[1];
        v2.x = BBox[3];
        v2.y = BBox[4];
      }
      else
      {
        v1.x = BBox[0];
        v1.y = BBox[1];
        v2.x = BBox[0];
        v2.y = BBox[4];
      }
    }
    else if (BBox[3] < CurrLightPos.x)
    {
      if (BBox[1] > CurrLightPos.y)
      {
        v1.x = BBox[3];
        v1.y = BBox[4];
        v2.x = BBox[0];
        v2.y = BBox[1];
      }
      else if (BBox[4] < CurrLightPos.y)
      {
        v1.x = BBox[0];
        v1.y = BBox[4];
        v2.x = BBox[3];
        v2.y = BBox[1];
      }
      else
      {
        v1.x = BBox[3];
        v1.y = BBox[4];
        v2.x = BBox[3];
        v2.y = BBox[1];
      }
    }
    else
    {
      if (BBox[1] > CurrLightPos.y)
      {
        v1.x = BBox[3];
        v1.y = BBox[1];
        v2.x = BBox[0];
        v2.y = BBox[1];
      }
      else
      {
        v1.x = BBox[0];
        v1.y = BBox[4];
        v2.x = BBox[3];
        v2.y = BBox[4];
      }
    }
  }
  else
#endif
  if (BBox[0] > Origin.x)
  {
    if (BBox[1] > Origin.y)
    {
      v1.x = BBox[3];
      v1.y = BBox[1];
      v2.x = BBox[0];
      v2.y = BBox[4];
    }
    else if (BBox[4] < Origin.y)
    {
      v1.x = BBox[0];
      v1.y = BBox[1];
      v2.x = BBox[3];
      v2.y = BBox[4];
    }
    else
    {
      v1.x = BBox[0];
      v1.y = BBox[1];
      v2.x = BBox[0];
      v2.y = BBox[4];
    }
  }
  else if (BBox[3] < Origin.x)
  {
    if (BBox[1] > Origin.y)
    {
      v1.x = BBox[3];
      v1.y = BBox[4];
      v2.x = BBox[0];
      v2.y = BBox[1];
    }
    else if (BBox[4] < Origin.y)
    {
      v1.x = BBox[0];
      v1.y = BBox[4];
      v2.x = BBox[3];
      v2.y = BBox[1];
    }
    else
    {
      v1.x = BBox[3];
      v1.y = BBox[4];
      v2.x = BBox[3];
      v2.y = BBox[1];
    }
  }
  else
  {
    if (BBox[1] > Origin.y)
    {
      v1.x = BBox[3];
      v1.y = BBox[1];
      v2.x = BBox[0];
      v2.y = BBox[1];
    }
    else
    {
      v1.x = BBox[0];
      v1.y = BBox[4];
      v2.x = BBox[3];
      v2.y = BBox[4];
    }
  }

  // Clip sectors that are behind rendered segs
  TVec r1 = Origin - v1;
  TVec r2 = Origin - v2;
  float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), Origin);
  float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), Origin);

#ifdef CLIENT
  if (shadowslight)
  {
    TVec rLight1 = CurrLightPos - v1;
    TVec rLight2 = CurrLightPos - v2;
    float DLight1 = DotProduct(Normalise(CrossProduct(rLight1, rLight2)), CurrLightPos);
    float DLight2 = DotProduct(Normalise(CrossProduct(rLight2, rLight1)), CurrLightPos);

    TVec rView1 = Origin-v1 - CurrLightPos;
    TVec rView2 = Origin-v2 - CurrLightPos;
    float DView1 = DotProduct(Normalise(CrossProduct(rView1, rView2)), Origin);
    float DView2 = DotProduct(Normalise(CrossProduct(rView2, rView1)), Origin);

    if (D1 < 0.0 && D2 < 0.0 &&
      DView1 < -CurrLightRadius && DView2 < -CurrLightRadius)
    {
      return false;
    }

    if (D1 > r_lights_radius && D2 > r_lights_radius)
    {
      return false;
    }

    if ((DLight1 > CurrLightRadius && DLight2 > CurrLightRadius) ||
        (DLight1 < -CurrLightRadius && DLight2 < -CurrLightRadius))
    {
      return false;
    }

    // There might be a better method of doing this, but
    // this one works for now...
    if (DLight1 > 0.0 && DLight2 <= 0.0)
    {
      v2 += (v2 - v1) * D1 / (D1 - D2);
    }
    else if (DLight2 > 0.0 && DLight1 <= 0.0)
    {
      v1 += (v1 - v2) * D2 / (D2 - D1);
    }
  }
  else
#endif
  {
    //k8: this glitches E1M2 (due to uninitialized `z` component above, it seems; so no more)
    if (D1 < 0.0 && D2 < 0.0) return false;

    // There might be a better method of doing this, but
    // this one works for now...
    if (D1 > 0.0 && D2 <= 0.0)
    {
      v2 += (v2 - v1) * D1 / (D1 - D2);
    }
    else if (D2 > 0.0 && D1 <= 0.0)
    {
      v1 += (v1 - v2) * D2 / (D2 - D1);
    }
  }

  return IsRangeVisible(PointToClipAngle(v1), PointToClipAngle(v2));
  unguard;
}

//==========================================================================
//
//  VViewClipper::ClipCheckRegion
//
//==========================================================================

bool VViewClipper::ClipCheckRegion(subregion_t *region, subsector_t *sub, bool shadowslight,
  const TVec &CurrLightPos, float CurrLightRadius)
{
  guard(VViewClipper::ClipCheckRegion);
  if (!ClipHead)
  {
    return true;
  }

  if (!clip_enabled)
    return true;

  vint32 count = sub->numlines;
  drawseg_t *ds = region->lines;

  while (count--)
  {
    TVec v1 = *ds->seg->v1;
    TVec v2 = *ds->seg->v2;

    if (!ds->seg->linedef)
    {
      //  Miniseg.
      if (!IsRangeVisible(PointToClipAngle(v2),
        PointToClipAngle(v1)))
      {
        ds++;
        continue;
      }
    }

    // Clip sectors that are behind rendered segs
    TVec rLight1;
    TVec rLight2;
    TVec r1 = Origin - v1;
    TVec r2 = Origin - v2;
    float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), Origin);
    float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), Origin);

#ifdef CLIENT
    float DLight1;
    float DLight2;

    if (shadowslight)
    {
      rLight1 = CurrLightPos - v1;
      rLight2 = CurrLightPos - v2;
      DLight1 = DotProduct(Normalise(CrossProduct(rLight1, rLight2)), CurrLightPos);
      DLight2 = DotProduct(Normalise(CrossProduct(rLight2, rLight1)), CurrLightPos);

      TVec rView1 = Origin-v1 - CurrLightPos;
      TVec rView2 = Origin-v2 - CurrLightPos;
      float DView1 = DotProduct(Normalise(CrossProduct(rView1, rView2)), Origin);
      float DView2 = DotProduct(Normalise(CrossProduct(rView2, rView1)), Origin);

      if (D1 <= 0.0 && D2 <= 0.0 && //k8: i commented this for some reason; wtf?!
        DView1 < -CurrLightRadius && DView2 < -CurrLightRadius)
      {
        ds++;
        continue;
      }

      if (D1 > r_lights_radius && D2 > r_lights_radius)
      {
        ds++;
        continue;
      }

      if ((DLight1 > CurrLightRadius && DLight2 > CurrLightRadius) ||
          (DLight1 < -CurrLightRadius && DLight2 < -CurrLightRadius))
      {
        ds++;
        continue;
      }
    }
    else
#endif
    {
      //k8: dunno, seems to be the same bug as above?
      if (D1 <= 0.0 && D2 <= 0.0) {
        ds++;
        continue;
      }
    }

    if (!ds->seg->backsector)
    {
#ifdef CLIENT
      if (shadowslight)
      {
        // There might be a better method of doing this, but
        // this one works for now...
        if (DLight1 > CurrLightRadius && DLight2 < -CurrLightRadius)
        {
          v2 += (v2 - v1) * DLight1 / (DLight1 - DLight2);
        }
        else if (DLight2 > CurrLightRadius && DLight1 < -CurrLightRadius)
        {
          v1 += (v1 - v2) * DLight2 / (DLight2 - DLight1);
        }
      }
      else
#endif
      {
        if (D1 > 0.0 && D2 <= 0.0)
        {
          v2 += (v2 - v1) * D1 / (D1 - D2);
        }
        else if (D2 > 0.0 && D1 <= 0.0)
        {
          v1 += (v1 - v2) * D2 / (D2 - D1);
        }
      }

      if (IsRangeVisible(PointToClipAngle(v2),
        PointToClipAngle(v1)))
      {
        return true;
      }
    }
    else if (ds->seg->linedef)
    {
      TVec vv1 = *ds->seg->linedef->v1;
      TVec vv2 = *ds->seg->linedef->v2;

      if (ds->seg->backsector)
      {
        // 2-sided line, determine if it can be skipped
        if (ds->seg->linedef->backsector)
        {
          // Just apply this to sectors without slopes
          if (ds->seg->linedef->frontsector->floor.normal.z == 1.0 && ds->seg->linedef->backsector->floor.normal.z == 1.0 &&
            ds->seg->linedef->frontsector->ceiling.normal.z == -1.0 && ds->seg->linedef->backsector->ceiling.normal.z == -1.0)
          {
            //
            // Check for doors
            //

            // A line without top texture isn't a door
            if (ds->seg->sidedef->TopTexture != -1 && clip_bsp)
            {
              float frontcz1 = ds->seg->linedef->frontsector->ceiling.GetPointZ(vv1);
              float frontcz2 = ds->seg->linedef->frontsector->ceiling.GetPointZ(vv2);
              float frontfz1 = ds->seg->linedef->frontsector->floor.GetPointZ(vv1);
              float frontfz2 = ds->seg->linedef->frontsector->floor.GetPointZ(vv2);

              float backcz1 = ds->seg->linedef->backsector->ceiling.GetPointZ(vv1);
              float backcz2 = ds->seg->linedef->backsector->ceiling.GetPointZ(vv2);
              float backfz1 = ds->seg->linedef->backsector->floor.GetPointZ(vv1);
              float backfz2 = ds->seg->linedef->backsector->floor.GetPointZ(vv2);

              if ((backcz2 <= frontfz2 && backcz2 <= frontfz1 && backcz1 <= frontfz2 && backcz1 <= frontfz1) &&
                (frontcz2 <= backfz2 && frontcz2 <= backfz1 && frontcz1 <= backfz2 && frontcz1 <= backfz1))
              {
                // It's a closed door
                ds++;
                continue;
              }
            }

            //
            // Check for elevators/plats
            //

            // A line without bottom texture isn't an elevator/plat
            if (ds->seg->sidedef->BottomTexture != -1 && clip_bsp)
            {
              float frontcz1 = ds->seg->linedef->frontsector->ceiling.GetPointZ(vv1);
              float frontcz2 = ds->seg->linedef->frontsector->ceiling.GetPointZ(vv2);
              float frontfz1 = ds->seg->linedef->frontsector->floor.GetPointZ(vv1);
              float frontfz2 = ds->seg->linedef->frontsector->floor.GetPointZ(vv2);

              float backcz1 = ds->seg->linedef->backsector->ceiling.GetPointZ(vv1);
              float backcz2 = ds->seg->linedef->backsector->ceiling.GetPointZ(vv2);
              float backfz1 = ds->seg->linedef->backsector->floor.GetPointZ(vv1);
              float backfz2 = ds->seg->linedef->backsector->floor.GetPointZ(vv2);

              if ((backcz2 <= frontfz2 && backcz2 <= frontfz1 && backcz1 <= frontfz2 && backcz1 <= frontfz1) &&
                (frontcz2 <= backfz2 && frontcz2 <= backfz1 && frontcz1 <= backfz2 && frontcz1 <= backfz1))
              {
                // It's an enclosed elevator/plat
                ds++;
                continue;
              }
            }

            //
            // Check for polyobjs
            //

            // A line without mid texture isn't a polyobj door
            if (ds->seg->sidedef->MidTexture != -1 && clip_bsp)
            {
              float frontcz1 = ds->seg->linedef->frontsector->ceiling.GetPointZ(vv1);
              float frontcz2 = ds->seg->linedef->frontsector->ceiling.GetPointZ(vv2);
              float frontfz1 = ds->seg->linedef->frontsector->floor.GetPointZ(vv1);
              float frontfz2 = ds->seg->linedef->frontsector->floor.GetPointZ(vv2);

              float backcz1 = ds->seg->linedef->backsector->ceiling.GetPointZ(vv1);
              float backcz2 = ds->seg->linedef->backsector->ceiling.GetPointZ(vv2);
              float backfz1 = ds->seg->linedef->backsector->floor.GetPointZ(vv1);
              float backfz2 = ds->seg->linedef->backsector->floor.GetPointZ(vv2);

              if ((backcz2 <= frontfz2 && backcz2 <= frontfz1 && backcz1 <= frontfz2 && backcz1 <= frontfz1) &&
                (frontcz2 <= backfz2 && frontcz2 <= backfz1 && frontcz1 <= backfz2 && frontcz1 <= backfz1))
              {
                // It's a closed polyobj door
                ds++;
                continue;
              }
            }
          }
          else
          {
            if (((ds->seg->linedef->frontsector->floor.maxz > ds->seg->linedef->backsector->ceiling.minz &&
              ds->seg->linedef->frontsector->ceiling.maxz < ds->seg->linedef->backsector->floor.minz) ||
              (ds->seg->linedef->frontsector->floor.minz > ds->seg->linedef->backsector->ceiling.maxz &&
                ds->seg->linedef->frontsector->ceiling.minz < ds->seg->linedef->backsector->floor.maxz)) ||
                ((ds->seg->linedef->backsector->floor.maxz > ds->seg->linedef->frontsector->ceiling.minz &&
                  ds->seg->linedef->backsector->ceiling.maxz < ds->seg->linedef->frontsector->floor.minz) ||
                  (ds->seg->linedef->backsector->floor.minz > ds->seg->linedef->frontsector->ceiling.maxz &&
                    ds->seg->linedef->backsector->ceiling.minz < ds->seg->linedef->frontsector->floor.maxz)))
            {
              ds++;
              continue;
            }
          }
        }
      }

      // Clip sectors that are behind rendered segs
      TVec rr1 = Origin - vv1;
      TVec rr2 = Origin - vv2;
      float DD1 = DotProduct(Normalise(CrossProduct(rr1, rr2)), Origin);
      float DD2 = DotProduct(Normalise(CrossProduct(rr2, rr1)), Origin);

#ifdef CLIENT
      //TVec rLight1;
      //TVec rLight2;
      //float DLight1;
      //float DLight2;

      if (shadowslight) {
        rLight1 = CurrLightPos - vv1;
        rLight2 = CurrLightPos - vv2;
        DLight1 = DotProduct(Normalise(CrossProduct(rLight1, rLight2)), CurrLightPos);
        DLight2 = DotProduct(Normalise(CrossProduct(rLight2, rLight1)), CurrLightPos);

        TVec rView1 = Origin-v1 - CurrLightPos;
        TVec rView2 = Origin-v2 - CurrLightPos;
        float DView1 = DotProduct(Normalise(CrossProduct(rView1, rView2)), Origin);
        float DView2 = DotProduct(Normalise(CrossProduct(rView2, rView1)), Origin);

        if (D1 <= 0.0 && D2 <= 0.0 && //k8: i commented this for some reason; wtf?!
          DView1 < -CurrLightRadius && DView2 < -CurrLightRadius)
        {
          ds++;
          continue;
        }

        if (DD1 > r_lights_radius && DD2 > r_lights_radius)
        {
          ds++;
          continue;
        }

        if ((DLight1 > CurrLightRadius && DLight2 > CurrLightRadius) ||
            (DLight1 < -CurrLightRadius && DLight2 < -CurrLightRadius))
        {
          ds++;
          continue;
        }
      }
      else
#endif
      {
        if (DD1 <= 0.0 && DD2 <= 0.0)
        {
          ds++;
          continue;
        }
      }

#ifdef CLIENT
      if (shadowslight)
      {
        // There might be a better method of doing this, but
        // this one works for now...
        if (DLight1 > CurrLightRadius && DLight2 < -CurrLightRadius)
        {
          vv2 += (vv2 - vv1) * DLight1 / (DLight1 - DLight2);
        }
        else if (DLight2 > CurrLightRadius && DLight1 < -CurrLightRadius)
        {
          vv1 += (vv1 - vv2) * DLight2 / (DLight2 - DLight1);
        }
      }
      else
#endif
      {
        if (DD1 > 0.0 && DD2 <= 0.0)
        {
          vv2 += (vv2 - vv1) * DD1 / (DD1 - DD2);
        }
        else if (DD2 > 0.0 && DD1 <= 0.0)
        {
          vv1 += (vv1 - vv2) * DD2 / (DD2 - DD1);
        }
      }

      if (!ds->seg->linedef->backsector)
      {
        if (IsRangeVisible(PointToClipAngle(vv2),
          PointToClipAngle(vv1)))
        {
          return true;
        }
      }
      else
      {
        return true;
      }
    }
    ds++;
  }
  return false;
  unguard;
}

//==========================================================================
//
//  VViewClipper::ClipCheckSubsector
//
//==========================================================================

bool VViewClipper::ClipCheckSubsector(subsector_t *Sub, bool shadowslight, const TVec &CurrLightPos,
  float CurrLightRadius)
{
  guard(VViewClipper::ClipCheckSubsector);
  if (!ClipHead)
  {
    return true;
  }

  if (!clip_enabled)
    return true;

  for (int i = 0; i < Sub->numlines; i++)
  {
    seg_t *line = &Level->Segs[Sub->firstline + i];

    TVec v1 = *line->v1;
    TVec v2 = *line->v2;

    if (!line->linedef)
    {
      //  Miniseg.
      if (!IsRangeVisible(PointToClipAngle(v2),
        PointToClipAngle(v1)))
      {
        continue;
      }
    }

    // Clip sectors that are behind rendered segs
    TVec rLight1;
    TVec rLight2;
#ifdef CLIENT
    float DLight1;
    float DLight2;
#endif

    TVec r1 = Origin - v1;
    TVec r2 = Origin - v2;
    float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), Origin);
    float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), Origin);

#ifdef CLIENT
    if (shadowslight)
    {
      rLight1 = CurrLightPos - v1;
      rLight2 = CurrLightPos - v2;
      DLight1 = DotProduct(Normalise(CrossProduct(rLight1, rLight2)), CurrLightPos);
      DLight2 = DotProduct(Normalise(CrossProduct(rLight2, rLight1)), CurrLightPos);

      TVec rView1 = Origin-v1 - CurrLightPos;
      TVec rView2 = Origin-v2 - CurrLightPos;
      float DView1 = DotProduct(Normalise(CrossProduct(rView1, rView2)), Origin);
      float DView2 = DotProduct(Normalise(CrossProduct(rView2, rView1)), Origin);

      if (D1 < 0.0 && D2 < 0.0 && //k8: i commented this for some reason; wtf?!
        DView1 < -CurrLightRadius && DView2 < -CurrLightRadius)
      {
        continue;
      }

      if (D1 > r_lights_radius && D2 > r_lights_radius)
      {
        continue;
      }

      if ((DLight1 > CurrLightRadius && DLight2 > CurrLightRadius) ||
          (DLight1 < -CurrLightRadius && DLight2 < -CurrLightRadius))
      {
        continue;
      }
    }
    else
#endif
    {
      //k8: dunno, seems to be the same bug as above?
      if (D1 < 0.0 && D2 < 0.0)
      {
        continue;
      }
    }

    if (!line->backsector)
    {
#ifdef CLIENT
      if (shadowslight)
      {
/*
#ifndef CLIENT
        rLight1 = CurrLightPos - v1;
        rLight2 = CurrLightPos - v2;
        DLight1 = DotProduct(Normalise(CrossProduct(rLight1, rLight2)), CurrLightPos);
        DLight2 = DotProduct(Normalise(CrossProduct(rLight2, rLight1)), CurrLightPos);
#endif
*/
        // There might be a better method of doing this, but
        // this one works for now...
        if (DLight1 > CurrLightRadius && DLight2 < -CurrLightRadius)
        {
          v2 += (v2 - v1) * DLight1 / (DLight1 - DLight2);
        }
        else if (DLight2 > CurrLightRadius && DLight1 < -CurrLightRadius)
        {
          v1 += (v1 - v2) * DLight2 / (DLight2 - DLight1);
        }
      }
      else
#endif
      {
        if (D1 > 0.0 && D2 < 0.0)
        {
          v2 += (v2 - v1) * D1 / (D1 - D2);
        }
        else if (D2 > 00 && D1 < 0.0)
        {
          v1 += (v1 - v2) * D2 / (D2 - D1);
        }
      }

      // Just apply this to sectors without slopes
      if (line->frontsector && line->frontsector->floor.normal.z == 1.0 && line->frontsector->ceiling.normal.z == -1.0)
      {
        if (IsRangeVisible(PointToClipAngle(v2),
          PointToClipAngle(v1)))
        {
          return true;
        }
      }
      else
      {
        return true;
      }
    } else if (line->linedef) {
      TVec vv1 = *line->linedef->v1;
      TVec vv2 = *line->linedef->v2;

      // 2-sided line, determine if it can be skipped
      if (line->backsector)
      {
        // Just apply this to sectors without slopes
        if (line->linedef->frontsector->floor.normal.z == 1.0 && line->linedef->backsector->floor.normal.z == 1.0 &&
          line->linedef->frontsector->ceiling.normal.z == -1.0 && line->linedef->backsector->ceiling.normal.z == -1.0)
        {
          //
          // Check for doors
          //

          // A line without top texture isn't a door
          if (line->sidedef->TopTexture != -1)
          {
            float frontcz1 = line->linedef->frontsector->ceiling.GetPointZ(vv1);
            float frontcz2 = line->linedef->frontsector->ceiling.GetPointZ(vv2);
            float frontfz1 = line->linedef->frontsector->floor.GetPointZ(vv1);
            float frontfz2 = line->linedef->frontsector->floor.GetPointZ(vv2);

            float backcz1 = line->linedef->backsector->ceiling.GetPointZ(vv1);
            float backcz2 = line->linedef->backsector->ceiling.GetPointZ(vv2);
            float backfz1 = line->linedef->backsector->floor.GetPointZ(vv1);
            float backfz2 = line->linedef->backsector->floor.GetPointZ(vv2);

            if ((backcz2 <= frontfz2 && backcz2 <= frontfz1 && backcz1 <= frontfz2 && backcz1 <= frontfz1) &&
              (frontcz2 <= backfz2 && frontcz2 <= backfz1 && frontcz1 <= backfz2 && frontcz1 <= backfz1))
            {
              // It's a closed door
              continue;
            }
          }

          //
          // Check for elevators/plats
          //

          // A line without bottom texture isn't an elevator/plat
          if (line->sidedef->BottomTexture != -1 && clip_bsp)
          {
            float frontcz1 = line->linedef->frontsector->ceiling.GetPointZ(vv1);
            float frontcz2 = line->linedef->frontsector->ceiling.GetPointZ(vv2);
            float frontfz1 = line->linedef->frontsector->floor.GetPointZ(vv1);
            float frontfz2 = line->linedef->frontsector->floor.GetPointZ(vv2);

            float backcz1 = line->linedef->backsector->ceiling.GetPointZ(vv1);
            float backcz2 = line->linedef->backsector->ceiling.GetPointZ(vv2);
            float backfz1 = line->linedef->backsector->floor.GetPointZ(vv1);
            float backfz2 = line->linedef->backsector->floor.GetPointZ(vv2);

            if ((backcz2 <= frontfz2 && backcz2 <= frontfz1 && backcz1 <= frontfz2 && backcz1 <= frontfz1) &&
              (frontcz2 <= backfz2 && frontcz2 <= backfz1 && frontcz1 <= backfz2 && frontcz1 <= backfz1))
            {
              // It's an enclosed elevator/plat
              continue;
            }
          }

          //
          // Check for polyobjs
          //

          // A line without mid texture isn't a polyobj door
          if (line->sidedef->MidTexture != -1 && clip_bsp)
          {
            float frontcz1 = line->linedef->frontsector->ceiling.GetPointZ(vv1);
            float frontcz2 = line->linedef->frontsector->ceiling.GetPointZ(vv2);
            float frontfz1 = line->linedef->frontsector->floor.GetPointZ(vv1);
            float frontfz2 = line->linedef->frontsector->floor.GetPointZ(vv2);

            float backcz1 = line->linedef->backsector->ceiling.GetPointZ(vv1);
            float backcz2 = line->linedef->backsector->ceiling.GetPointZ(vv2);
            float backfz1 = line->linedef->backsector->floor.GetPointZ(vv1);
            float backfz2 = line->linedef->backsector->floor.GetPointZ(vv2);

            if ((backcz2 <= frontfz2 && backcz2 <= frontfz1 && backcz1 <= frontfz2 && backcz1 <= frontfz1) &&
              (frontcz2 <= backfz2 && frontcz2 <= backfz1 && frontcz1 <= backfz2 && frontcz1 <= backfz1))
            {
              // It's a closed polyobj door
              continue;
            }
          }
        }
        else
        {
          if (((line->linedef->frontsector->floor.maxz > line->linedef->backsector->ceiling.minz &&
            line->linedef->frontsector->ceiling.maxz < line->linedef->backsector->floor.minz) ||
            (line->linedef->frontsector->floor.minz > line->linedef->backsector->ceiling.maxz &&
              line->linedef->frontsector->ceiling.minz < line->linedef->backsector->floor.maxz)) ||
              ((line->linedef->backsector->floor.maxz > line->linedef->frontsector->ceiling.minz &&
                line->linedef->backsector->ceiling.maxz < line->linedef->frontsector->floor.minz) ||
                (line->linedef->backsector->floor.minz > line->linedef->frontsector->ceiling.maxz &&
                  line->linedef->backsector->ceiling.minz < line->linedef->frontsector->floor.maxz)))
          {
            continue;
          }
        }
      }

      // Clip sectors that are behind rendered segs
      //TVec rLight1;
      //TVec rLight2;

      /*TVec*/ r1 = Origin - vv1;
      /*TVec*/ r2 = Origin - vv2;
      /*float*/ D1 = DotProduct(Normalise(CrossProduct(r1, r2)), Origin);
      /*float*/ D2 = DotProduct(Normalise(CrossProduct(r2, r1)), Origin);

#ifdef CLIENT
      //float DLight1;
      //float DLight2;

      if (shadowslight)
      {
        rLight1 = CurrLightPos - vv1;
        rLight2 = CurrLightPos - vv2;
        DLight1 = DotProduct(Normalise(CrossProduct(rLight1, rLight2)), CurrLightPos);
        DLight2 = DotProduct(Normalise(CrossProduct(rLight2, rLight1)), CurrLightPos);

        TVec rView1 = Origin-v1 - CurrLightPos;
        TVec rView2 = Origin-v2 - CurrLightPos;
        float DView1 = DotProduct(Normalise(CrossProduct(rView1, rView2)), Origin);
        float DView2 = DotProduct(Normalise(CrossProduct(rView2, rView1)), Origin);

        if (D1 < 0.0 && D2 < 0.0 && //k8: i commented this for some reason; wtf?!
          DView1 < -CurrLightRadius && DView2 < -CurrLightRadius)
        {
          continue;
        }

        if (D1 > r_lights_radius && D2 > r_lights_radius)
        {
          continue;
        }

        if ((DLight1 > CurrLightRadius && DLight2 > CurrLightRadius) ||
            (DLight1 < -CurrLightRadius && DLight2 < -CurrLightRadius))
        {
          continue;
        }
      }
      else
#endif
      {
        //k8: dunno, seems to be the same bug as above?
        if (D1 < 0.0 && D2 < 0.0)
        {
          continue;
        }
      }

#ifdef CLIENT
      if (shadowslight)
      {
        // There might be a better method of doing this, but
        // this one works for now...
        if (DLight1 > CurrLightRadius && DLight2 < -CurrLightRadius)
        {
          vv2 += (vv2 - vv1) * DLight1 / (DLight1 - DLight2);
        }
        else if (DLight2 > CurrLightRadius && DLight1 < -CurrLightRadius)
        {
          vv1 += (vv1 - vv2) * DLight2 / (DLight2 - DLight1);
        }
      }
      else
#endif
      {
        if (D1 > 0.0 && D2 < 0.0)
        {
          vv2 += (vv2 - vv1) * D1 / (D1 - D2);
        }
        else if (D2 > 0.0 && D1 < 0.0)
        {
          vv1 += (vv1 - vv2) * D2 / (D2 - D1);
        }
      }

      if (!line->linedef->backsector)
      {
        if (IsRangeVisible(PointToClipAngle(vv2),
          PointToClipAngle(vv1)))
        {
          return true;
        }
      }
      else
      {
        return true;
      }
    }
  }
  return false;
  unguard;
}

//==========================================================================
//
//  VViewClipper::ClipAddSubsectorSegs
//
//==========================================================================

void VViewClipper::ClipAddSubsectorSegs(subsector_t *Sub, bool shadowslight, TPlane *Mirror,
  const TVec &CurrLightPos, float CurrLightRadius)
{
  guard(VViewClipper::ClipAddSubsectorSegs);

  if (!clip_enabled)
    return;

  for (int i = 0; i < Sub->numlines; i++)
  {
    seg_t *line = &Level->Segs[Sub->firstline + i];

    TVec v1 = *line->v1;
    TVec v2 = *line->v2;

    if (!line->linedef)
    {
      //  Miniseg.
      continue;
    }

    if (line->PointOnSide(Origin))
    {
      //  Viewer is in back side or on plane
      continue;
    }

    TVec r1 = Origin - v1;
    TVec r2 = Origin - v2;
    float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), Origin);
    float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), Origin);

#ifdef CLIENT
    if (shadowslight)
    {
      TVec rLight1 = CurrLightPos - v1;
      TVec rLight2 = CurrLightPos - v2;
      float DLight1 = DotProduct(Normalise(CrossProduct(rLight1, rLight2)), CurrLightPos);
      float DLight2 = DotProduct(Normalise(CrossProduct(rLight2, rLight1)), CurrLightPos);

      TVec rView1 = Origin-v1 - CurrLightPos;
      TVec rView2 = Origin-v2 - CurrLightPos;
      float DView1 = DotProduct(Normalise(CrossProduct(rView1, rView2)), Origin);
      float DView2 = DotProduct(Normalise(CrossProduct(rView2, rView1)), Origin);

      if (D1 <= 0.0 && D2 <= 0.0 && //k8: i commented this for some reason; wtf?!
        DView1 < -CurrLightRadius && DView2 < -CurrLightRadius)
      {
        continue;
      }

      if (D1 > r_lights_radius && D2 > r_lights_radius)
      {
        continue;
      }

      if ((DLight1 > CurrLightRadius && DLight2 > CurrLightRadius) ||
          (DLight1 < -CurrLightRadius && DLight2 < -CurrLightRadius))
      {
        continue;
      }
    }
    else
#endif
    {
      if (D1 <= 0.0 && D2 <= 0.0)
      {
        continue;
      }
    }

    if (Mirror)
    {
      //  Clip seg with mirror plane.
      float Dist1 = DotProduct(v1, Mirror->normal) - Mirror->dist;
      float Dist2 = DotProduct(v2, Mirror->normal) - Mirror->dist;

      if (Dist1 <= 0.0 && Dist2 <= 0.0)
      {
        continue;
      }
    }

    if (clip_trans_hack && line->linedef->alpha < 1.0) continue; //k8: skip translucent walls (for now)
    // 2-sided line, determine if it can be skipped
    if (line->backsector) {
      const TVec vv1 = *line->linedef->v1;
      const TVec vv2 = *line->linedef->v2;

      // Just apply this to sectors without slopes
      if (line->linedef->frontsector->floor.normal.z == 1.0 && line->linedef->backsector->floor.normal.z == 1.0 &&
        line->linedef->frontsector->ceiling.normal.z == -1.0 && line->linedef->backsector->ceiling.normal.z == -1.0)
      {
        //
        // Check for doors
        //

        // A line without top texture isn't a door
        if (line->sidedef->TopTexture != -1)
        {
          float frontcz1 = line->linedef->frontsector->ceiling.GetPointZ(vv1);
          float frontcz2 = line->linedef->frontsector->ceiling.GetPointZ(vv2);
          float frontfz1 = line->linedef->frontsector->floor.GetPointZ(vv1);
          float frontfz2 = line->linedef->frontsector->floor.GetPointZ(vv2);

          float backcz1 = line->linedef->backsector->ceiling.GetPointZ(vv1);
          float backcz2 = line->linedef->backsector->ceiling.GetPointZ(vv2);
          float backfz1 = line->linedef->backsector->floor.GetPointZ(vv1);
          float backfz2 = line->linedef->backsector->floor.GetPointZ(vv2);

          if ((backcz2 > frontfz2 && backcz2 > frontfz1 && backcz1 > frontfz2 && backcz1 > frontfz1) &&
            (frontcz2 > backfz2 && frontcz2 > backfz1 && frontcz1 > backfz2 && frontcz1 > backfz1))
          {
            // It's an opened door
            continue;
          }
        }

        //
        // Check for elevators/plats
        //

        // A line without bottom texture isn't an elevator/plat
        if (line->sidedef->BottomTexture != -1) {
          float frontcz1 = line->linedef->frontsector->ceiling.GetPointZ(vv1);
          float frontcz2 = line->linedef->frontsector->ceiling.GetPointZ(vv2);
          float frontfz1 = line->linedef->frontsector->floor.GetPointZ(vv1);
          float frontfz2 = line->linedef->frontsector->floor.GetPointZ(vv2);

          float backcz1 = line->linedef->backsector->ceiling.GetPointZ(vv1);
          float backcz2 = line->linedef->backsector->ceiling.GetPointZ(vv2);
          float backfz1 = line->linedef->backsector->floor.GetPointZ(vv1);
          float backfz2 = line->linedef->backsector->floor.GetPointZ(vv2);

          if ((backcz2 > frontfz2 && backcz2 > frontfz1 && backcz1 > frontfz2 && backcz1 > frontfz1) &&
            (frontcz2 > backfz2 && frontcz2 > backfz1 && frontcz1 > backfz2 && frontcz1 > backfz1))
          {
            // It's a lowered elevator/plat
            continue;
          }
        }

        //
        // Check for polyobjs
        //

        // A line without mid texture isn't a polyobj door
        if (line->sidedef->MidTexture != -1 && clip_bsp) {
          float frontcz1 = line->linedef->frontsector->ceiling.GetPointZ(vv1);
          float frontcz2 = line->linedef->frontsector->ceiling.GetPointZ(vv2);
          float frontfz1 = line->linedef->frontsector->floor.GetPointZ(vv1);
          float frontfz2 = line->linedef->frontsector->floor.GetPointZ(vv2);

          float backcz1 = line->linedef->backsector->ceiling.GetPointZ(vv1);
          float backcz2 = line->linedef->backsector->ceiling.GetPointZ(vv2);
          float backfz1 = line->linedef->backsector->floor.GetPointZ(vv1);
          float backfz2 = line->linedef->backsector->floor.GetPointZ(vv2);

          if ((backcz2 > frontfz2 && backcz2 > frontfz1 && backcz1 > frontfz2 && backcz1 > frontfz1) &&
            (frontcz2 > backfz2 && frontcz2 > backfz1 && frontcz1 > backfz2 && frontcz1 > backfz1))
          {
            // It's an opened polyobj door
            continue;
          }
        }
      } else {
        if (((line->linedef->frontsector->floor.maxz <= line->linedef->backsector->ceiling.minz &&
          line->linedef->frontsector->ceiling.maxz >= line->linedef->backsector->floor.minz) ||
          (line->linedef->frontsector->floor.minz <= line->linedef->backsector->ceiling.maxz &&
            line->linedef->frontsector->ceiling.minz >= line->linedef->backsector->floor.maxz)) ||
            ((line->linedef->backsector->floor.maxz <= line->linedef->frontsector->ceiling.minz &&
              line->linedef->backsector->ceiling.maxz >= line->linedef->frontsector->floor.minz) ||
              (line->linedef->backsector->floor.minz <= line->linedef->frontsector->ceiling.maxz &&
                line->linedef->backsector->ceiling.minz >= line->linedef->frontsector->floor.maxz)))
        {
          continue;
        }
      }
    }

    if (Mirror)
    {
      //  Clip seg with mirror plane.
      float Dist1 = DotProduct(v1, Mirror->normal) - Mirror->dist;
      float Dist2 = DotProduct(v2, Mirror->normal) - Mirror->dist;

      if (Dist1 > 0.0 && Dist2 <= 0.0)
      {
        v2 = v1 + (v2 - v1) * Dist1 / (Dist1 - Dist2);
      }
      else if (Dist2 > 0.0 && Dist1 <= 0.0)
      {
        v1 = v2 + (v1 - v2) * Dist2 / (Dist2 - Dist1);
      }
    }
    //k8: this is hack for boom translucency
    //    midtexture 0 *SHOULD* mean "transparent", but let's play safe
    if (clip_trans_hack && line->linedef && line->frontsector && line->backsector && line->sidedef->MidTexture <= 0) {
      // ok, we can possibly see thru it, now check the OPPOSITE sector ceiling and floor heights
      const sector_t *ops = (line->side ? line->frontsector : line->backsector);
      // ceiling is higher than the floor?
      if (ops->ceiling.minz > ops->floor.maxz) {
        // check if that sector has any translucent walls
        bool hasTrans = false;
        int lcount = ops->linecount;
        for (int lidx = 0; lidx < lcount; ++lidx) if (ops->lines[lidx]->alpha < 1.0) { hasTrans = true; break; }
        if (hasTrans) {
          //printf("!!! SKIPPED 2-SIDED LINE W/O MIDTEX (%d); side=%d; origz=%f!\n", line->sidedef->LineNum, line->side, Origin.z);
          //printf("  (f,c)=(%f:%f,%f:%f)\n", ops->floor.minz, ops->floor.maxz, ops->ceiling.minz, ops->ceiling.maxz);
          continue;
        } else {
          //printf("!!! ALLOWED 2-SIDED LINE W/O MIDTEX (%d); side=%d; origz=%f!\n", line->sidedef->LineNum, line->side, Origin.z);
        }
      }
    }
    AddClipRange(PointToClipAngle(v2), PointToClipAngle(v1));
  }

  if (Sub->poly) {
    seg_t **polySeg = Sub->poly->segs;

    for (int polyCount = Sub->poly->numsegs; polyCount--; polySeg++) {
      seg_t *line = *polySeg;

      if (!line->linedef) {
        //  Miniseg.
        continue;
      }

      if (line->PointOnSide(Origin)) {
        //  Viewer is in back side or on plane
        continue;
      }

      TVec v1 = *line->v1;
      TVec v2 = *line->v2;

      // Just apply this to sectors without slopes
      if (line->frontsector->floor.normal.z == 1.0 && line->frontsector->ceiling.normal.z == -1.0) { //k8: first was `10`; a typo?
        TVec r1 = Origin - v1;
        TVec r2 = Origin - v2;
        float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), Origin);
        float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), Origin);

#ifdef CLIENT
        if (shadowslight) {
          TVec rLight1 = CurrLightPos - v1;
          TVec rLight2 = CurrLightPos - v2;
          float DLight1 = DotProduct(Normalise(CrossProduct(rLight1, rLight2)), CurrLightPos);
          float DLight2 = DotProduct(Normalise(CrossProduct(rLight2, rLight1)), CurrLightPos);

          TVec rView1 = Origin-v1 - CurrLightPos;
          TVec rView2 = Origin-v2 - CurrLightPos;
          float DView1 = DotProduct(Normalise(CrossProduct(rView1, rView2)), Origin);
          float DView2 = DotProduct(Normalise(CrossProduct(rView2, rView1)), Origin);

          if (D1 <= 0.0 && D2 <= 0.0 && //k8: i commented this for some reason; wtf?!
            DView1 < -CurrLightRadius && DView2 < -CurrLightRadius)
          {
            continue;
          }

          if (D1 > r_lights_radius && D2 > r_lights_radius)
          {
            continue;
          }

          if ((DLight1 > CurrLightRadius && DLight2 > CurrLightRadius) ||
              (DLight1 < -CurrLightRadius && DLight2 < -CurrLightRadius))
          {
            continue;
          }
        }
        else
#endif
        {
          if (D1 <= 0.0 && D2 <= 0.0) continue;
        }
      }

      // 2-sided line, determine if it can be skipped
      if (line->backsector) {
        const TVec vv1 = *line->linedef->v1;
        const TVec vv2 = *line->linedef->v2;

        // Just apply this to sectors without slopes
        if (line->linedef->frontsector->floor.normal.z == 1.0 && line->linedef->backsector->floor.normal.z == 1.0 &&
          line->linedef->frontsector->ceiling.normal.z == -1.0 && line->linedef->backsector->ceiling.normal.z == -1.0)
        {
          //
          // Check for doors
          //

          // A line without top texture isn't a door
          if (line->sidedef->TopTexture != -1) {
            float frontcz1 = line->linedef->frontsector->ceiling.GetPointZ(vv1);
            float frontcz2 = line->linedef->frontsector->ceiling.GetPointZ(vv2);
            float frontfz1 = line->linedef->frontsector->floor.GetPointZ(vv1);
            float frontfz2 = line->linedef->frontsector->floor.GetPointZ(vv2);

            float backcz1 = line->linedef->backsector->ceiling.GetPointZ(vv1);
            float backcz2 = line->linedef->backsector->ceiling.GetPointZ(vv2);
            float backfz1 = line->linedef->backsector->floor.GetPointZ(vv1);
            float backfz2 = line->linedef->backsector->floor.GetPointZ(vv2);

            if ((backcz2 > frontfz2 && backcz2 > frontfz1 && backcz1 > frontfz2 && backcz1 > frontfz1) &&
              (frontcz2 > backfz2 && frontcz2 > backfz1 && frontcz1 > backfz2 && frontcz1 > backfz1))
            {
              // It's an opened door
              continue;
            }
          }

          //
          // Check for elevators/plats
          //

          // A line without bottom texture isn't an elevator/plat
          if (line->sidedef->BottomTexture != -1) {
            float frontcz1 = line->linedef->frontsector->ceiling.GetPointZ(vv1);
            float frontcz2 = line->linedef->frontsector->ceiling.GetPointZ(vv2);
            float frontfz1 = line->linedef->frontsector->floor.GetPointZ(vv1);
            float frontfz2 = line->linedef->frontsector->floor.GetPointZ(vv2);

            float backcz1 = line->linedef->backsector->ceiling.GetPointZ(vv1);
            float backcz2 = line->linedef->backsector->ceiling.GetPointZ(vv2);
            float backfz1 = line->linedef->backsector->floor.GetPointZ(vv1);
            float backfz2 = line->linedef->backsector->floor.GetPointZ(vv2);

            if ((backcz2 > frontfz2 && backcz2 > frontfz1 && backcz1 > frontfz2 && backcz1 > frontfz1) &&
              (frontcz2 > backfz2 && frontcz2 > backfz1 && frontcz1 > backfz2 && frontcz1 > backfz1))
            {
              // It's a lowered elevator
              continue;
            }
          }

          //
          // Check for polyobjs
          //

          // A line without mid texture isn't a polyobj door
          if (line->sidedef->MidTexture != -1) {
            float frontcz1 = line->linedef->frontsector->ceiling.GetPointZ(vv1);
            float frontcz2 = line->linedef->frontsector->ceiling.GetPointZ(vv2);
            float frontfz1 = line->linedef->frontsector->floor.GetPointZ(vv1);
            float frontfz2 = line->linedef->frontsector->floor.GetPointZ(vv2);

            float backcz1 = line->linedef->backsector->ceiling.GetPointZ(vv1);
            float backcz2 = line->linedef->backsector->ceiling.GetPointZ(vv2);
            float backfz1 = line->linedef->backsector->floor.GetPointZ(vv1);
            float backfz2 = line->linedef->backsector->floor.GetPointZ(vv2);

            if ((backcz2 > frontfz2 && backcz2 > frontfz1 && backcz1 > frontfz2 && backcz1 > frontfz1) &&
              (frontcz2 > backfz2 && frontcz2 > backfz1 && frontcz1 > backfz2 && frontcz1 > backfz1))
            {
              // It's an opened polyobj door
              continue;
            }
          }
        }
        else {
          if (((line->linedef->frontsector->floor.maxz <= line->linedef->backsector->ceiling.minz &&
            line->linedef->frontsector->ceiling.maxz >= line->linedef->backsector->floor.minz) ||
            (line->linedef->frontsector->floor.minz <= line->linedef->backsector->ceiling.maxz &&
              line->linedef->frontsector->ceiling.minz >= line->linedef->backsector->floor.maxz)) ||
              ((line->linedef->backsector->floor.maxz <= line->linedef->frontsector->ceiling.minz &&
                line->linedef->backsector->ceiling.maxz >= line->linedef->frontsector->floor.minz) ||
                (line->linedef->backsector->floor.minz <= line->linedef->frontsector->ceiling.maxz &&
                  line->linedef->backsector->ceiling.minz >= line->linedef->frontsector->floor.maxz)))
          {
            continue;
          }
        }
      }

      AddClipRange(PointToClipAngle(v2), PointToClipAngle(v1));
    }
  }
  unguard;
}
