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
//**	Copyright (C) 1999-2002 J�nis Legzdi��
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

#ifndef GLVISINT_H
#define GLVISINT_H

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include "cmdlib.h"
#include "wadlib.h"
#include "glvis.h"

namespace VavoomUtils {

#include "vector.h"

// MACROS ------------------------------------------------------------------

#define	ON_EPSILON	0.1

//
// Indicate a leaf.
//
#define	NF_SUBSECTOR	0x80000000

// TYPES -------------------------------------------------------------------

struct winding_t
{
	bool	original;			// don't free, it's part of the portal
	TVec	points[2];
};

enum vstatus_t { stat_none, stat_working, stat_done };
struct portal_t : TPlane	// normal pointing into neighbor
{
	int			leaf;		// neighbor
	winding_t	winding;
	vstatus_t	status;
	byte		*visbits;
	byte		*mightsee;
	int			nummightsee;
	int			numcansee;
};

#define	MAX_PORTALS_ON_LEAF		128
struct leaf_t
{
	int			secnum;
	int			numportals;
	portal_t	*portals[MAX_PORTALS_ON_LEAF];
};

struct pstack_t
{
	pstack_t	*next;
	leaf_t		*leaf;
	portal_t	*portal;	// portal exiting
	winding_t	*source, *pass;
	TPlane		portalplane;
	byte		*mightsee;		// bit string
};

struct threaddata_t
{
	byte		*leafvis;		// bit string
	portal_t	*base;
	pstack_t	pstack_head;
};

typedef TVec vertex_t;

typedef leaf_t subsector_t;

struct seg_t : public TPlane
{
	vertex_t	*v1;
	vertex_t	*v2;

	seg_t		*partner;
	int			leaf;
	int			secnum;
};

struct line_t
{
	int secnum[2];
};

class TVisBuilder
{
 public:
	TVisBuilder(TGLVis &);
	void Run(const char *srcfile);

 private:
	TGLVis &Owner;

	TIWadFile inwad;
	TIWadFile gwa;
	TOWadFile outwad;

	TIWadFile *mainwad;
	TIWadFile *glwad;

	bool GLNodesV5;

	int numvertexes;
	vertex_t *vertexes;
	vertex_t *gl_vertexes;

	int numsectors;
	line_t *lines;
	int numlines;
	int *sidesecs;
	int numsides;

	int numsegs;
	seg_t *segs;

	int numsubsectors;
	subsector_t *subsectors;

	int numportals;
	portal_t *portals;

	int vissize;
	byte *vis;

	int rejectsize;
	byte *reject;

	int bitbytes;				// (portalleafs+63)>>3
	int bitlongs;

	byte *portalsee;
	int c_leafsee, c_portalsee;

	int c_chains;
	int c_portalskip, c_leafskip;
	int c_vistest, c_mighttest;
	int c_portaltest, c_portalpass, c_portalcheck;

	int totalvis;
	int rowbytes;

	bool IsLevelName(int lump);
	bool IsGLLevelName(int lump);

	void LoadVertexes(int lump, int gl_lump);
	void LoadSectors(int lump);
	void LoadSideDefs(int lump);
	void LoadLineDefs1(int lump);
	void LoadLineDefs2(int lump);
	void LoadSegs(int lump);
	void LoadSubsectors(int lump);
	void CreatePortals(void);
	void CopyLump(int i);
	void LoadLevel(int lumpnum, int gl_lumpnum);
	void FreeLevel(void);
	void BuildGWA(void);
	void BuildWAD(void);

	void SimpleFlood(portal_t *srcportal, int leafnum);
	void BasePortalVis(void);
	void CheckStack(leaf_t *leaf, threaddata_t *thread);
	void FreeWinding(winding_t *w);
	winding_t *CopyWinding(winding_t *w);
	winding_t *ClipWinding(winding_t *in, TPlane *split);
	winding_t *ClipToSeperators(winding_t *source, winding_t *pass,
		winding_t *target);
	void RecursiveLeafFlow(int leafnum, threaddata_t *thread,
		pstack_t *prevstack, int StackDepth);
	void PortalFlow(portal_t *p);
	portal_t *GetNextPortal(void);
	void CalcPortalVis(void);
	void LeafFlow(int leafnum);
	void BuildPVS(void);
	void BuildReject(void);
};

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PUBLIC DATA DECLARATIONS ------------------------------------------------

} // namespace VavoomUtils

#endif

//**************************************************************************
//
//	$Log$
//	Revision 1.5  2005/11/14 19:03:18  dj_jl
//	Added support for version 5 GL nodes.
//
//	Revision 1.4  2004/12/27 12:23:18  dj_jl
//	Multiple small changes for version 1.16
//	
//	Revision 1.3  2002/01/07 12:30:05  dj_jl
//	Changed copyright year
//	
//	Revision 1.2  2001/10/18 17:41:47  dj_jl
//	Added reject building
//	
//	Revision 1.1  2001/09/12 17:28:38  dj_jl
//	Created glVIS plugin
//	
//	Revision 1.3  2001/08/24 17:08:34  dj_jl
//	Beautification
//	
//	Revision 1.2  2001/07/27 14:27:55  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************
