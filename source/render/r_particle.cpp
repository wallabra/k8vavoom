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
//**
//**  Rendering of particles.
//**
//**************************************************************************
#include "gamedefs.h"
#include "r_local.h"


VCvarB r_draw_particles("r_draw_particles", true, "Draw particles?", CVAR_Archive);
static VCvarF r_particle_max_distance("r_particle_max_distance", "3072", "Max distance for alive particles.", CVAR_Archive);


//==========================================================================
//
//  VRenderLevelShared::InitParticles
//
//==========================================================================
void VRenderLevelShared::InitParticles () {
  const char *p = GArgs.CheckValue("-particles");

  if (p) {
    NumParticles = atoi(p);
    if (NumParticles < ABSOLUTE_MIN_PARTICLES) NumParticles = ABSOLUTE_MIN_PARTICLES;
  } else {
    NumParticles = MAX_PARTICLES;
  }

  Particles = new particle_t[NumParticles];
}


//==========================================================================
//
//  VRenderLevelShared::ClearParticles
//
//==========================================================================
void VRenderLevelShared::ClearParticles () {
  FreeParticles = &Particles[0];
  ActiveParticles = nullptr;
  for (int i = 0; i < NumParticles; ++i) Particles[i].next = &Particles[i+1];
  Particles[NumParticles-1].next = nullptr;
}


//==========================================================================
//
//  VRenderLevelShared::NewParticle
//
//==========================================================================
particle_t *VRenderLevelShared::NewParticle (const TVec &porg) {
  if (!FreeParticles) return nullptr; // no free particles
  if (!r_draw_particles) return nullptr; // save some resources

  // check distance and frustum
  //TODO: camera views can view far away from client
  if (cl && r_particle_max_distance > 0) {
    const float maxdistSq = r_particle_max_distance*r_particle_max_distance;
    const float distSq = lengthSquared(porg-cl->ViewOrg);
    if (distSq >= maxdistSq) return nullptr;
    if (distSq >= maxdistSq*0.25) {
      static TFrustum frustum;
      static TFrustumParam fp;
      if (fp.needUpdate(cl->ViewOrg, cl->ViewAngles)) {
        fp.setup(cl->ViewOrg, cl->ViewAngles);
        TClipBase cb(refdef.fovx, refdef.fovy);
        if (cb.isValid()) frustum.setup(cb, fp, true/*, r_particle_max_distance*/);
      }
      if (frustum.isValid() && !frustum.checkPoint(porg)) return nullptr;
    }
  }

  // remove from list of free particles
  particle_t *p = FreeParticles;
  FreeParticles = p->next;
  // clean
  memset((void *)p, 0, sizeof(*p));
  // add to active particles
  p->next = ActiveParticles;
  ActiveParticles = p;
  p->org = porg;
  return p;
}


//==========================================================================
//
//  VRenderLevelShared::UpdateParticles
//
//==========================================================================
void VRenderLevelShared::UpdateParticles (float frametime) {
  particle_t *p, *kill;

  if (GGameInfo->IsPaused() || (Level->LevelInfo->LevelInfoFlags2&VLevelInfo::LIF2_Frozen)) return;

  if (!r_draw_particles) {
    // save some resources (and remove all particles)
    kill = ActiveParticles;
    while (kill) {
      ActiveParticles = kill->next;
      kill->next = FreeParticles;
      FreeParticles = kill;
      kill = ActiveParticles;
    }
    return;
  }

  kill = ActiveParticles;
  while (kill && kill->die < Level->Time) {
    ActiveParticles = kill->next;
    kill->next = FreeParticles;
    FreeParticles = kill;
    kill = ActiveParticles;
  }

  for (p = ActiveParticles; p; p = p->next) {
    kill = p->next;
    while (kill && kill->die < Level->Time) {
      p->next = kill->next;
      kill->next = FreeParticles;
      FreeParticles = kill;
      kill = p->next;
    }

    p->org += (p->vel*frametime);
    Level->LevelInfo->eventUpdateParticle(p, frametime);
  }
}


//==========================================================================
//
//  VRenderLevelShared::DrawParticles
//
//==========================================================================
void VRenderLevelShared::DrawParticles () {
  if (!r_draw_particles) return;
  Drawer->StartParticles();
  for (particle_t *p = ActiveParticles; p; p = p->next) {
    if (ColourMap) {
      vuint32 Col = p->colour;
      rgba_t TmpCol = ColourMaps[ColourMap].GetPalette()[R_LookupRGB((Col>>16)&255, (Col>>8)&255, Col&255)];
      p->colour = (Col&0xff000000)|(TmpCol.r<<16)|(TmpCol.g<<8)|TmpCol.b;
      Drawer->DrawParticle(p);
      p->colour = Col;
    } else {
      Drawer->DrawParticle(p);
    }
  }
  Drawer->EndParticles();
}
