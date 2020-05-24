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
//**
//**  Portals.
//**
//**************************************************************************
#include "../gamedefs.h"
#include "r_local.h"

//#define VV_OLD_SKYPORTAL_CODE


extern VCvarB gl_dbg_wireframe;


// "autosave" struct to avoid some pasta
struct AutoSavedView {
  VRenderLevelShared *RLev;
  TVec SavedViewOrg;
  TAVec SavedViewAngles;
  TVec SavedViewForward;
  TVec SavedViewRight;
  TVec SavedViewUp;

  TClipPlane SavedClip;
  unsigned planeCount;
  bool SavedMirrorClip;

  AutoSavedView () = delete;
  AutoSavedView (const AutoSavedView &) = delete;
  AutoSavedView &operator = (const AutoSavedView &) = delete;

  inline AutoSavedView (VRenderLevelShared *ARLev) noexcept {
    vassert(ARLev);
    vassert(Drawer);
    RLev = ARLev;

    SavedViewOrg = Drawer->vieworg;
    SavedViewAngles = Drawer->viewangles;
    SavedViewForward = Drawer->viewforward;
    SavedViewRight = Drawer->viewright;
    SavedViewUp = Drawer->viewup;
    SavedMirrorClip = Drawer->MirrorClip;

    SavedClip = Drawer->viewfrustum.planes[TFrustum::Forward]; // save far/mirror plane
    planeCount = Drawer->viewfrustum.planeCount;
  }

  inline ~AutoSavedView () noexcept {
    // restore render settings
    Drawer->vieworg = SavedViewOrg;
    Drawer->viewangles = SavedViewAngles;
    Drawer->viewforward = SavedViewForward;
    Drawer->viewright = SavedViewRight;
    Drawer->viewup = SavedViewUp;
    Drawer->MirrorClip = SavedMirrorClip;

    // restore original frustum
    RLev->CallTransformFrustum();
    Drawer->viewfrustum.planes[TFrustum::Forward] = SavedClip; // restore far/mirror plane
    Drawer->viewfrustum.planeCount = planeCount;

    // resetup view origin
    Drawer->SetupViewOrg();
  }
};


//==========================================================================
//
//  VPortal::VPortal
//
//==========================================================================
VPortal::VPortal (VRenderLevelShared *ARLev)
  : RLev(ARLev)
  , stackedSector(false)
{
  Level = RLev->PortalLevel+1;
}


//==========================================================================
//
//  VPortal::~VPortal
//
//==========================================================================
VPortal::~VPortal () {
}


//==========================================================================
//
//  VPortal::NeedsDepthBuffer
//
//==========================================================================
bool VPortal::NeedsDepthBuffer () const {
  return true;
}


//==========================================================================
//
//  VPortal::IsSky
//
//==========================================================================
bool VPortal::IsSky () const {
  return false;
}


//==========================================================================
//
//  VPortal::IsMirror
//
//==========================================================================
bool VPortal::IsMirror () const {
  return false;
}


//==========================================================================
//
//  VPortal::IsStack
//
//==========================================================================
bool VPortal::IsStack () const {
  return false;
}


//==========================================================================
//
//  VPortal::MatchSky
//
//==========================================================================
bool VPortal::MatchSky (VSky *) const {
  return false;
}


//==========================================================================
//
//  VPortal::MatchSkyBox
//
//==========================================================================
bool VPortal::MatchSkyBox (VEntity *) const {
  return false;
}


//==========================================================================
//
//  VPortal::MatchMirror
//
//==========================================================================
bool VPortal::MatchMirror (TPlane *) const {
  return false;
}


//==========================================================================
//
//  VPortal::Draw
//
//==========================================================================
void VPortal::Draw (bool UseStencil) {
  if (gl_dbg_wireframe) return;

  if (!Drawer->StartPortal(this, UseStencil)) {
    // all portal polygons are clipped away
    //GCon->Logf("portal is clipped away (stencil:%d)", (int)UseStencil);
    return;
  }
  //GCon->Logf("doing portal (stencil:%d)", (int)UseStencil);

  {
    // save renderer settings
    AutoSavedView guard(RLev);

    VEntity *SavedViewEnt = RLev->ViewEnt;
    const int SavedExtraLight = RLev->ExtraLight;
    const int SavedFixedLight = RLev->FixedLight;
    vuint8 *SavedBspVis = RLev->BspVis;
    vuint8 *SavedBspVisSector = RLev->BspVisSector;

    VRenderLevelShared::PPMark pmark;
    VRenderLevelShared::MarkPortalPool(&pmark);

    bool restoreVis = false;
    {
      VRenderLevelDrawer::DrawListStackMark dlsmark(RLev);
      vassert(dlsmark.level > 0);
      RLev->PushDrawLists();

      if (NeedsDepthBuffer()) {
        // set up BSP visibility table and translated sprites
        // this has to be done only for portals that do rendering of view

        // notify allocator about minimal node size
        VRenderLevelShared::SetMinPoolNodeSize(RLev->VisSize+RLev->SecVisSize+128);
        // allocate new bsp vis
        RLev->BspVis = VRenderLevelShared::AllocPortalPool(RLev->VisSize+RLev->SecVisSize+128);
        RLev->BspVisSector = RLev->BspVis+RLev->VisSize;
        if (RLev->VisSize) {
          memset(RLev->BspVis, 0, RLev->VisSize);
        }
        if (RLev->SecVisSize) {
          memset(RLev->BspVisSector, 0, RLev->SecVisSize);
        }
        //fprintf(stderr, "BSPVIS: size=%d\n", RLev->VisSize);

        // allocate new transsprites list
        restoreVis = true;
      }

      DrawContents();
    }

    // restore render settings
    RLev->ViewEnt = SavedViewEnt;
    RLev->ExtraLight = SavedExtraLight;
    RLev->FixedLight = SavedFixedLight;
    if (restoreVis) {
      RLev->BspVis = SavedBspVis;
      RLev->BspVisSector = SavedBspVisSector;
    }

    // restore ppol
    VRenderLevelShared::RestorePortalPool(&pmark);
  }

  Drawer->EndPortal(this, UseStencil);
}


//==========================================================================
//
//  VPortal::SetupRanges
//
//==========================================================================
void VPortal::SetupRanges (const refdef_t &refdef, VViewClipper &Range, bool Revert, bool SetFrustum) {
  Range.ClearClipNodes(Drawer->vieworg, RLev->Level);
  if (SetFrustum) {
    Range.ClipInitFrustumRange(Drawer->viewangles, Drawer->viewforward, Drawer->viewright, Drawer->viewup, refdef.fovx, refdef.fovy);
    //GCon->Logf("SURFS: %d", Surfs.Num());
    //return;
  }
  for (int i = 0; i < Surfs.Num(); ++i) {
    if (Surfs[i]->GetNormalZ() == 0) {
      // wall
      //seg_t *Seg = (seg_t *)Surfs[i]->eplane;
      seg_t *Seg = Surfs[i]->seg;
      vassert(Seg);
      vassert(Seg >= RLev->Level->Segs);
      vassert(Seg < RLev->Level->Segs+RLev->Level->NumSegs);
      if (Revert) {
        Range.AddClipRange(*Seg->v1, *Seg->v2);
      } else {
        Range.AddClipRange(*Seg->v2, *Seg->v1);
      }
    } else {
      // floor/ceiling
      for (int j = 0; j < Surfs[i]->count; ++j) {
        TVec v1, v2;
        if ((Surfs[i]->GetNormalZ() < 0) != Revert) {
          v1 = Surfs[i]->verts[j < Surfs[i]->count-1 ? j+1 : 0].vec();
          v2 = Surfs[i]->verts[j].vec();
        } else {
          v1 = Surfs[i]->verts[j].vec();
          v2 = Surfs[i]->verts[j < Surfs[i]->count-1 ? j+1 : 0].vec();
        }
        TVec Dir = v2-v1;
        Dir.z = 0;
        if (Dir.x > -0.01f && Dir.x < 0.01f && Dir.y > -0.01f && Dir.y < 0.01f) continue; // too short
        TPlane P;
        P.SetPointDirXY(v1, Dir);
        if ((DotProduct(Drawer->vieworg, P.normal)-P.dist < 0.01f) != Revert) continue; // view origin is on the back side
        Range.AddClipRange(v2, v1);
      }
    }
  }
}


//==========================================================================
//
//  VSkyPortal::NeedsDepthBuffer
//
//==========================================================================
bool VSkyPortal::NeedsDepthBuffer () const {
  return false;
}


//==========================================================================
//
//  VSkyPortal::IsSky
//
//==========================================================================
bool VSkyPortal::IsSky () const {
  return true;
}


//==========================================================================
//
//  VSkyPortal::MatchSky
//
//==========================================================================
bool VSkyPortal::MatchSky (VSky *ASky) const {
  return (Level == RLev->PortalLevel+1 && Sky == ASky);
}


//==========================================================================
//
//  VSkyPortal::Draw
//
//==========================================================================
void VSkyPortal::Draw (bool UseStencil) {
  #ifdef VV_OLD_SKYPORTAL_CODE
  VPortal::Draw(UseStencil);
  #else
  vassert(!UseStencil);

  if (!Drawer->StartPortal(this, UseStencil)) {
    // all portal polygons are clipped away
    //GCon->Logf("portal is clipped away (stencil:%d)", (int)UseStencil);
    return;
  }

  // there is no need to mess with BSP here, we will never touch it
  {
    AutoSavedView guard(RLev);
    DrawContents();
  }

  Drawer->EndPortal(this, UseStencil);
  #endif
}


//==========================================================================
//
//  VSkyPortal::DrawContents
//
//==========================================================================
void VSkyPortal::DrawContents () {
  Drawer->vieworg = TVec(0, 0, 0);
  RLev->TransformFrustum();
  Drawer->SetupViewOrg();

  Sky->Draw(RLev->ColorMap);

  // this should not be the case (render lists should be empty)
  #ifdef VV_OLD_SKYPORTAL_CODE
  Drawer->DrawLightmapWorld();
  #endif
}



//==========================================================================
//
//  VSkyBoxPortal::IsSky
//
//==========================================================================
bool VSkyBoxPortal::IsSky () const {
  return true;
}


//==========================================================================
//
//  VSkyBoxPortal::MatchSkyBox
//
//==========================================================================
bool VSkyBoxPortal::MatchSkyBox (VEntity *AEnt) const {
  return (Level == RLev->PortalLevel+1 && Viewport == AEnt);
}


//==========================================================================
//
//  VSkyBoxPortal::DrawContents
//
//==========================================================================
void VSkyBoxPortal::DrawContents () {
  if (gl_dbg_wireframe) return;

  // set view origin to be sky view origin
  RLev->ViewEnt = Viewport;
  Drawer->vieworg = Viewport->Origin;
  Drawer->viewangles.yaw += Viewport->Angles.yaw;
  AngleVectors(Drawer->viewangles, Drawer->viewforward, Drawer->viewright, Drawer->viewup);

  // no light flashes in the sky
  RLev->ExtraLight = 0;
  if (RLev->ColorMap == CM_Default) RLev->FixedLight = 0;

  // prevent recursion
  VEntity::AutoPortalDirty guard(Viewport);
  refdef_t rd = RLev->refdef;
  RLev->RenderScene(&rd, nullptr);
}



//==========================================================================
//
//  VSectorStackPortal::IsStack
//
//==========================================================================
bool VSectorStackPortal::IsStack () const {
  return true;
}


//==========================================================================
//
//  VSectorStackPortal::MatchSkyBox
//
//==========================================================================
bool VSectorStackPortal::MatchSkyBox (VEntity *AEnt) const {
  return (Level == RLev->PortalLevel+1 && Viewport == AEnt);
}


//==========================================================================
//
//  VSectorStackPortal::DrawContents
//
//==========================================================================
void VSectorStackPortal::DrawContents () {
  VViewClipper Range;
  refdef_t rd = RLev->refdef;
  VPortal::SetupRanges(rd, Range, false, true); //k8: after moving viewport?

  RLev->ViewEnt = Viewport;
  VEntity *Mate = Viewport->GetSkyBoxMate();

  //GCon->Logf("rendering portal contents; offset=(%f,%f)", Viewport->Origin.x-Mate->Origin.x, Viewport->Origin.y-Mate->Origin.y);

  Drawer->vieworg.x = Drawer->vieworg.x+Viewport->Origin.x-Mate->Origin.x;
  Drawer->vieworg.y = Drawer->vieworg.y+Viewport->Origin.y-Mate->Origin.y;

  //VPortal::SetupRanges(Range, false, true); //k8: after moving viewport?

  // prevent recursion
  VEntity::AutoPortalDirty guard(Viewport);
  RLev->RenderScene(&rd, &Range);
}



//==========================================================================
//
//  VMirrorPortal::IsMirror
//
//==========================================================================
bool VMirrorPortal::IsMirror () const {
  return true;
}


//==========================================================================
//
//  VMirrorPortal::MatchMirror
//
//==========================================================================
bool VMirrorPortal::MatchMirror (TPlane *APlane) const {
  return (Level == RLev->PortalLevel+1 && Plane->normal == APlane->normal && Plane->dist == APlane->dist);
}


//==========================================================================
//
//  VMirrorPortal::DrawContents
//
//==========================================================================
void VMirrorPortal::DrawContents () {
  RLev->ViewEnt = nullptr;

  ++RLev->MirrorLevel;
  Drawer->MirrorFlip = RLev->MirrorLevel&1;
  Drawer->MirrorClip = true;

  float Dist = DotProduct(Drawer->vieworg, Plane->normal)-Plane->dist;
  Drawer->vieworg -= 2*Dist*Plane->normal;

  Dist = DotProduct(Drawer->viewforward, Plane->normal);
  Drawer->viewforward -= 2*Dist*Plane->normal;
  Dist = DotProduct(Drawer->viewright, Plane->normal);
  Drawer->viewright -= 2*Dist*Plane->normal;
  Dist = DotProduct(Drawer->viewup, Plane->normal);
  Drawer->viewup -= 2*Dist*Plane->normal;

  // k8: i added this, but i don't know if it is required
  Drawer->viewforward.normaliseInPlace();
  Drawer->viewright.normaliseInPlace();
  Drawer->viewup.normaliseInPlace();

  VectorsAngles(Drawer->viewforward, (Drawer->MirrorFlip ? -Drawer->viewright : Drawer->viewright), Drawer->viewup, Drawer->viewangles);

  refdef_t rd = RLev->refdef;
  VViewClipper Range;
  SetupRanges(rd, Range, true, false);

  // use "far plane" (it is unused by default)
  const TClipPlane SavedClip = Drawer->viewfrustum.planes[TFrustum::Forward]; // save far/mirror plane
  const unsigned planeCount = Drawer->viewfrustum.planeCount;
  Drawer->viewfrustum.planes[TFrustum::Forward] = *Plane;
  Drawer->viewfrustum.planes[TFrustum::Forward].clipflag = TFrustum::ForwardBit; //0x20U;
  Drawer->viewfrustum.planeCount = 6;

  RLev->RenderScene(&rd, &Range);

  Drawer->viewfrustum.planes[TFrustum::Forward] = SavedClip;
  Drawer->viewfrustum.planeCount = planeCount;

  --RLev->MirrorLevel;
  Drawer->MirrorFlip = RLev->MirrorLevel&1;
}
