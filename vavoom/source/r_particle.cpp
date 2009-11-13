//**************************************************************************
//**
//**	##   ##    ##    ##   ##   ####     ####   ###     ###
//**	##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**	 ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**	 ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**	  ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**	   #    ##    ##    #      ####     ####   ##       ##
//**
//**	$Id$
//**
//**	Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**	This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**	This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
//**
//**	Rendering of particles.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "gamedefs.h"
#include "r_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

VCvarI				r_draw_particles("r_draw_particles", "1", CVAR_Archive);

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
//	VRenderLevel::InitParticles
//
//==========================================================================

void VRenderLevel::InitParticles()
{
	guard(VRenderLevel::InitParticles);
	const char* p = GArgs.CheckValue("-particles");

	if (p)
	{
		NumParticles = atoi(p);
		if (NumParticles < ABSOLUTE_MIN_PARTICLES)
			NumParticles = ABSOLUTE_MIN_PARTICLES;
	}
	else
	{
		NumParticles = MAX_PARTICLES;
	}

	Particles = new particle_t[NumParticles];
	unguard;
}

//==========================================================================
//
//	VRenderLevel::ClearParticles
//
//==========================================================================

void VRenderLevel::ClearParticles()
{
	guard(VRenderLevel::ClearParticles);
	FreeParticles = &Particles[0];
	ActiveParticles = NULL;

	for (int i = 0; i < NumParticles; i++)
		Particles[i].next = &Particles[i + 1];
	Particles[NumParticles - 1].next = NULL;
	unguard;
}

//==========================================================================
//
//	VRenderLevel::NewParticle
//
//==========================================================================

particle_t* VRenderLevel::NewParticle()
{
	guard(VRenderLevel::NewParticle);
	if (!FreeParticles)
	{
		//	No free particles
		return NULL;
	}
	//	Remove from list of free particles
	particle_t* p = FreeParticles;
	FreeParticles = p->next;
	//	Clean
	memset(p, 0, sizeof(*p));
	//	Add to active particles
	p->next = ActiveParticles;
	ActiveParticles = p;
	return p;
	unguard;
}

//==========================================================================
//
//	VRenderLevel::UpdateParticles
//
//==========================================================================

void VRenderLevel::UpdateParticles(float frametime)
{
	guard(VRenderLevel::UpdateParticles);
	particle_t		*p, *kill;

	if (cl->ClGame->ClientFlags & VClientGameBase::CF_Paused ||
		(GGameInfo->NetMode == NM_Standalone && (MN_Active() || C_Active())))
	{
		return;
	}

	kill = ActiveParticles;
	while (kill && kill->die < Level->Time)
	{
		ActiveParticles = kill->next;
		kill->next = FreeParticles;
		FreeParticles = kill;
		kill = ActiveParticles;
	}

	for (p = ActiveParticles; p; p = p->next)
	{
		kill = p->next;
		while (kill && kill->die < Level->Time)
		{
			p->next = kill->next;
			kill->next = FreeParticles;
			FreeParticles = kill;
			kill = p->next;
		}

		p->org += (p->vel * frametime);
		Level->LevelInfo->eventUpdateParticle(p, frametime);
	}
	unguard;
}

//==========================================================================
//
//	VRenderLevel::DrawParticles
//
//==========================================================================

void VRenderLevel::DrawParticles()
{
	guard(VRenderLevel::DrawParticles);
	if (!r_draw_particles)
	{
		return;
	}
	Drawer->StartParticles();
	for (particle_t* p = ActiveParticles; p; p = p->next)
	{
		if (ColourMap)
		{
			vuint32 Col = p->colour;
			rgba_t TmpCol = ColourMaps[ColourMap].GetPalette()[R_LookupRGB(
				(Col >> 16) & 0xff, (Col >> 8) & 0xff, Col & 0xff)];
			p->colour = (Col & 0xff000000) | (TmpCol.r << 16) |
				(TmpCol.g << 8) | TmpCol.b;
			Drawer->DrawParticle(p);
			p->colour = Col;
		}
		else
		{
			Drawer->DrawParticle(p);
		}
	}
	Drawer->EndParticles();
	unguard;
}
