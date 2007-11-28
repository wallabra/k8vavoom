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
//**	Refresh of things, i.e. objects represented by sprites.
//**
//** 	Sprite rotation 0 is facing the viewer, rotation 1 is one angle turn
//**  CLOCKWISE around the axis. This is not the same as the angle, which
//**  increases counter clockwise (protractor). There was a lot of stuff
//**  grabbed wrong, so I changed it...
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "gamedefs.h"
#include "r_local.h"

// MACROS ------------------------------------------------------------------

#define MAX_SPRITE_MODELS	512

// TYPES -------------------------------------------------------------------

enum
{
	SPR_VP_PARALLEL_UPRIGHT,
	SPR_FACING_UPRIGHT,
	SPR_VP_PARALLEL,
	SPR_ORIENTED,
	SPR_VP_PARALLEL_ORIENTED,
	SPR_VP_PARALLEL_UPRIGHT_ORIENTED,
};

//
//  Sprites are patches with a special naming convention
// so they can be recognized by R_InitSprites.
//  The base name is NNNNFx or NNNNFxFx, with
// x indicating the rotation, x = 0, 1-7.
//  The sprite and frame specified by a thing_t
// is range checked at run time.
//  A sprite is a patch_t that is assumed to represent
// a three dimensional object and may have multiple
// rotations pre drawn.
//  Horizontal flipping is used to save space,
// thus NNNNF2F5 defines a mirrored patch.
//  Some sprites will only have one picture used
// for all views: NNNNF0
//
struct spriteframe_t
{
	// If false use 0 for any position.
	// Note: as eight entries are available,
	//  we might as well insert the same name eight times.
	bool		rotate;

	// Lump to use for view angles 0-7.
	short		lump[16];

	// Flip bit (1 = flip) to use for view angles 0-7.
	bool		flip[16];
};

//
// 	A sprite definition:
// a number of animation frames.
//
struct spritedef_t
{
	int					numframes;
	spriteframe_t		*spriteframes;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern VCvarI		r_chasecam;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

//	Temporary variables for sprite installing
static spriteframe_t	sprtemp[30];
static int				maxframe;
static const char*		spritename;

//	variables used to look up
// and range check thing_t sprites patches
static spritedef_t		sprites[MAX_SPRITE_MODELS];

static VCvarI			r_draw_mobjs("r_draw_mobjs", "1", CVAR_Archive);
static VCvarI			r_draw_psprites("r_draw_psprites", "1", CVAR_Archive);
static VCvarI			r_models("r_models", "1", CVAR_Archive);
static VCvarI			r_view_models("r_view_models", "1", CVAR_Archive);
static VCvarI			r_sort_sprites("r_sort_sprites", "0");
static VCvarI			r_fix_sprite_offsets("r_fix_sprite_offsets", "1", CVAR_Archive);
static VCvarI			r_sprite_fix_delta("r_sprite_fix_delta", "-3", CVAR_Archive);
static VCvarI			r_drawfuzz("r_drawfuzz", "1", CVAR_Archive);
static VCvarF			transsouls("transsouls", "1.0", CVAR_Archive);
static VCvarI			croshair("croshair", "0", CVAR_Archive);
static VCvarF			croshair_alpha("croshair_alpha", "1", CVAR_Archive);

// CODE --------------------------------------------------------------------

//==========================================================================
//
//	InstallSpriteLump
//
//	Local function for R_InitSprites.
//
//==========================================================================

static void InstallSpriteLump(int lumpnr, int frame, char Rot, bool flipped)
{
	guard(InstallSpriteLump);
	int			r;
	int			rotation;

	if (Rot >= '0' && Rot <= '9')
	{
		rotation = Rot - '0';
	}
	else if (Rot >= 'a')
	{
		rotation = Rot - 'a' + 10;
	}
	else
	{
		rotation = 17;
	}

	VTexture* Tex = GTextureManager.Textures[lumpnr];
	if ((vuint32)frame >= 30 || (vuint32)rotation > 16)
	{
		Sys_Error("InstallSpriteLump: Bad frame characters in lump %s",
			*Tex->Name);
	}

	if (frame > maxframe)
		maxframe = frame;

	if (rotation == 0)
	{
		// the lump should be used for all rotations
		sprtemp[frame].rotate = false;
		for (r = 0; r < 16; r++)
		{
			sprtemp[frame].lump[r] = lumpnr;
			sprtemp[frame].flip[r] = flipped;
		}
		return;
	}

	if (rotation <= 8)
	{
		rotation = (rotation - 1) * 2;
	}
	else
	{
		rotation = (rotation - 9) * 2 + 1;
	}

	// the lump is only used for one rotation
	if (sprtemp[frame].rotate == false)
	{
		for (r = 0; r < 16; r++)
		{
			sprtemp[frame].lump[r] = -1;
			sprtemp[frame].flip[r] = false;
		}
	}

	sprtemp[frame].rotate = true;
	sprtemp[frame].lump[rotation] = lumpnr;
	sprtemp[frame].flip[rotation] = flipped;
	unguard;
}

//==========================================================================
//
//	R_InstallSprite
//
// 	Builds the sprite rotation matrixes to account for horizontally flipped
// sprites. Will report an error if the lumps are inconsistant.
// 	Sprite lump names are 4 characters for the actor, a letter for the frame,
// and a number for the rotation. A sprite that is flippable will have an
// additional letter/number appended. The rotation character can be 0 to
// signify no rotations.
//
//==========================================================================

void R_InstallSprite(const char *name, int index)
{
	guard(R_InstallSprite);
	if ((vuint32)index >= MAX_SPRITE_MODELS)
	{
		Host_Error("Invalid sprite index %d for sprite %s", index, name);
	}
	spritename = name;
	memset(sprtemp, -1, sizeof(sprtemp));
	maxframe = -1;

	// scan all the lump names for each of the names,
	//  noting the highest frame letter.
	// Just compare 4 characters as ints
	int intname = *(int*)*VName(spritename, VName::AddLower8);

	// scan the lumps, filling in the frames for whatever is found
	for (int l = 0; l < GTextureManager.Textures.Num(); l++)
	{
		if (GTextureManager.Textures[l]->Type == TEXTYPE_Sprite)
		{
			const char* lumpname = *GTextureManager.Textures[l]->Name;
			if (*(int*)lumpname == intname)
			{
				InstallSpriteLump(l, VStr::ToUpper(lumpname[4]) - 'A',
					lumpname[5], false);

				if (lumpname[6])
				{
					InstallSpriteLump(l, VStr::ToUpper(lumpname[6]) - 'A',
						lumpname[7], true);
				}
			}
		}
	}

	// check the frames that were found for completeness
	if (maxframe == -1)
	{
		sprites[index].numframes = 0;
		return;
	}

	maxframe++;

	for (int frame = 0 ; frame < maxframe ; frame++)
	{
		switch ((int)sprtemp[frame].rotate)
		{
		case -1:
			// no rotations were found for that frame at all
			Sys_Error("R_InstallSprite: No patches found "
					"for %s frame %c", spritename, frame + 'A');
			break;

		case 0:
			// only the first rotation is needed
			break;

		case 1:
			//	Copy missing frames for 16-angle rotation.
			for (int rotation = 0; rotation < 8; rotation++)
			{
				if (sprtemp[frame].lump[rotation * 2 + 1] == -1)
				{
					sprtemp[frame].lump[rotation * 2 + 1] =
						sprtemp[frame].lump[rotation * 2];
					sprtemp[frame].flip[rotation * 2 + 1] =
						sprtemp[frame].flip[rotation * 2];
				}
				if (sprtemp[frame].lump[rotation * 2] == -1)
				{
					sprtemp[frame].lump[rotation * 2] =
						sprtemp[frame].lump[rotation * 2 + 1];
					sprtemp[frame].flip[rotation * 2] =
						sprtemp[frame].flip[rotation * 2 + 1];
				}
			}
			// must have all 8 frames
			for (int rotation = 0; rotation < 8; rotation++)
			{
				if (sprtemp[frame].lump[rotation] == -1)
				{
					Sys_Error("R_InstallSprite: Sprite %s frame %c "
							"is missing rotations", spritename, frame + 'A');
				}
			}
			break;
		}
	}

	if (sprites[index].spriteframes)
	{
		Z_Free(sprites[index].spriteframes);
		sprites[index].spriteframes = NULL;
	}
	// allocate space for the frames present and copy sprtemp to it
	sprites[index].numframes = maxframe;
	sprites[index].spriteframes = (spriteframe_t*)
		Z_Malloc(maxframe * sizeof(spriteframe_t));
	memcpy(sprites[index].spriteframes, sprtemp, maxframe * sizeof(spriteframe_t));
	unguard;
}

//==========================================================================
//
//	R_FreeSpriteData
//
//==========================================================================

void R_FreeSpriteData()
{
	guard(R_FreeSpriteData);
	for (int i = 0; i < MAX_SPRITE_MODELS; i++)
		if (sprites[i].spriteframes)
			Z_Free(sprites[i].spriteframes);
	unguard;
}

//==========================================================================
//
//	VRenderLevel::DrawTranslucentPoly
//
//==========================================================================

void VRenderLevel::DrawTranslucentPoly(surface_t* surf, TVec* sv, int count,
	int lump, float Alpha, bool Additive, int translation, bool type,
	vuint32 light, vuint32 Fade, const TVec& normal, float pdist,
	const TVec& saxis, const TVec& taxis, const TVec& texorg)
{
	guard(VRenderLevel::DrawTranslucentPoly);
	int i;

	TVec mid(0, 0, 0);
	for (i = 0; i < count; i++)
	{
		mid += sv[i];
	}
	mid /= count;
	float dist = fabs(DotProduct(mid - vieworg, viewforward));
//	float dist = Length(mid - vieworg);
	int found = -1;
	float best_dist = -1;
	for (i = 0; i < MAX_TRANS_SPRITES; i++)
	{
		trans_sprite_t &spr = trans_sprites[i];
		if (!spr.Alpha)
		{
			if (type)
				memcpy(spr.Verts, sv, sizeof(TVec) * 4);
			spr.dist = dist;
			spr.lump = lump;
			spr.normal = normal;
			spr.pdist = pdist;
			spr.saxis = saxis;
			spr.taxis = taxis;
			spr.texorg = texorg;
			spr.surf = surf;
			spr.Alpha = Alpha;
			spr.Additive = Additive;
			spr.translation = translation;
			spr.type = type;
			spr.light = light;
			spr.Fade = Fade;
			return;
		}
		if (spr.dist > best_dist)
		{
			found = i;
			best_dist = spr.dist;
		}
	}
	if (best_dist > dist)
	{
		//	All slots are full, draw and replace a far away sprite
		trans_sprite_t &spr = trans_sprites[found];
		if (spr.type == 2)
		{
			DrawEntityModel(spr.Ent, spr.light, spr.Fade, spr.Alpha,
				spr.Additive, spr.TimeFrac);
		}
		else if (spr.type)
		{
			Drawer->DrawSpritePolygon(spr.Verts, GTextureManager[spr.lump],
				spr.Alpha, spr.Additive, spr.translation, spr.light, spr.Fade,
				spr.normal, spr.pdist, spr.saxis, spr.taxis, spr.texorg);
		}
		else
		{
			Drawer->DrawMaskedPolygon(spr.surf, spr.Alpha, spr.Additive);
		}
		if (type)
			memcpy(spr.Verts, sv, sizeof(TVec) * 4);
		spr.dist = dist;
		spr.lump = lump;
		spr.normal = normal;
		spr.pdist = pdist;
		spr.saxis = saxis;
		spr.taxis = taxis;
		spr.texorg = texorg;
		spr.surf = surf;
		spr.Alpha = Alpha;
		spr.Additive = Additive;
		spr.translation = translation;
		spr.light = light;
		spr.Fade = Fade;
		return;
	}

	//	All slots are full and are nearer to current sprite so draw it
	if (type)
	{
		Drawer->DrawSpritePolygon(sv, GTextureManager[lump], Alpha,
			Additive, translation, light, Fade, normal, pdist, saxis,
			taxis, texorg);
	}
	else
	{
		Drawer->DrawMaskedPolygon(surf, Alpha, Additive);
	}
	unguard;
}

//==========================================================================
//
//	VRenderLevel::RenderSprite
//
//==========================================================================

void VRenderLevel::RenderSprite(VEntity* thing, vuint32 light, vuint32 Fade,
	float Alpha, bool Additive)
{
	guard(VRenderLevel::RenderSprite);
	int spr_type = thing->SpriteType;

	TVec sprorigin = thing->Origin;
	sprorigin.z -= thing->FloorClip;
	TVec sprforward;
	TVec sprright;
	TVec sprup;

	float		dot;
	TVec		tvec;
	float		sr;
	float		cr;

	switch (spr_type)
	{
	case SPR_VP_PARALLEL_UPRIGHT:
		//	Generate the sprite's axes, with sprup straight up in worldspace,
		// and sprright parallel to the viewplane. This will not work if the
		// view direction is very close to straight up or down, because the
		// cross product will be between two nearly parallel vectors and
		// starts to approach an undefined state, so we don't draw if the two
		// vectors are less than 1 degree apart
		dot = viewforward.z;	//	same as DotProduct(viewforward, sprup)
								// because sprup is 0, 0, 1
		if ((dot > 0.999848) || (dot < -0.999848))	// cos(1 degree) = 0.999848
			return;

		sprup = TVec(0, 0, 1);
		//	CrossProduct(sprup, viewforward)
		sprright = Normalise(TVec(viewforward.y, -viewforward.x, 0));
		//	CrossProduct(sprright, sprup)
		sprforward = TVec(-sprright.y, sprright.x, 0);
		break;

	case SPR_FACING_UPRIGHT:
		//	Generate the sprite's axes, with sprup straight up in worldspace,
		// and sprright perpendicular to sprorigin. This will not work if the
		// view direction is very close to straight up or down, because the
		// cross product will be between two nearly parallel vectors and
		// starts to approach an undefined state, so we don't draw if the two
		// vectors are less than 1 degree apart
		tvec = Normalise(sprorigin - vieworg);
		dot = tvec.z;	//	same as DotProduct (tvec, sprup) because
						// sprup is 0, 0, 1
		if ((dot > 0.999848) || (dot < -0.999848))	// cos(1 degree) = 0.999848
			return;
		sprup = TVec(0, 0, 1);
		//	CrossProduct(sprup, -sprorigin)
		sprright = Normalise(TVec(tvec.y, -tvec.x, 0));
		//	CrossProduct(sprright, sprup)
		sprforward = TVec(-sprright.y, sprright.x, 0);
		break;

	case SPR_VP_PARALLEL:
		//	Generate the sprite's axes, completely parallel to the viewplane.
		// There are no problem situations, because the sprite is always in
		// the same position relative to the viewer
		sprup = viewup;
		sprright = viewright;
		sprforward = viewforward;
		break;

	case SPR_ORIENTED:
		//	Generate the sprite's axes, according to the sprite's world
		// orientation
		AngleVectors(thing->Angles, sprforward, sprright, sprup);
		break;

	case SPR_VP_PARALLEL_ORIENTED:
		//	Generate the sprite's axes, parallel to the viewplane, but
		// rotated in that plane around the centre according to the sprite
		// entity's roll angle. So sprforward stays the same, but sprright
		// and sprup rotate
		sr = msin(thing->Angles.roll);
		cr = mcos(thing->Angles.roll);

		sprforward = viewforward;
		sprright = TVec(viewright.x * cr + viewup.x * sr, viewright.y * cr +
			viewup.y * sr, viewright.z * cr + viewup.z * sr);
		sprup = TVec(viewright.x * -sr + viewup.x * cr, viewright.y * -sr +
			viewup.y * cr, viewright.z * -sr + viewup.z * cr);
		break;

	case SPR_VP_PARALLEL_UPRIGHT_ORIENTED:
		//	Generate the sprite's axes, with sprup straight up in worldspace,
		// and sprright parallel to the viewplane and then rotated in that
		// plane around the centre according to the sprite entity's roll
		// angle. So sprforward stays the same, but sprright and sprup rotate
		// This will not work if the view direction is very close to straight
		// up or down, because the cross product will be between two nearly
		// parallel vectors and starts to approach an undefined state, so we
		// don't draw if the two vectors are less than 1 degree apart
		dot = viewforward.z;	//	same as DotProduct(viewforward, sprup)
								// because sprup is 0, 0, 1
		if ((dot > 0.999848) || (dot < -0.999848))	// cos(1 degree) = 0.999848
			return;

		sr = msin(thing->Angles.roll);
		cr = mcos(thing->Angles.roll);

		//	CrossProduct(TVec(0, 0, 1), viewforward)
		tvec = Normalise(TVec(viewforward.y, -viewforward.x, 0));
		//	CrossProduct(tvec, TVec(0, 0, 1))
		sprforward = TVec(-tvec.y, tvec.x, 0);
		//	Rotate
		sprright = TVec(tvec.x * cr, tvec.y * cr, tvec.z * cr + sr);
		sprup = TVec(tvec.x * -sr, tvec.y * -sr, tvec.z * -sr + cr);
		break;

	default:
		Sys_Error("RenderSprite: Bad sprite type %d", spr_type);
	}

	spritedef_t*	sprdef;
	spriteframe_t*	sprframe;

	// decide which patch to use for sprite relative to player
	if ((unsigned)thing->State->SpriteIndex >= MAX_SPRITE_MODELS)
	{
#ifdef PARANOID
		GCon->Logf(NAME_Dev, "Invalid sprite number %d",
			thing->State->SpriteIndex);
#endif
		return;
	}
	sprdef = &sprites[thing->State->SpriteIndex];
	if ((thing->State->Frame & FF_FRAMEMASK) >= sprdef->numframes)
	{
#ifdef PARANOID
		GCon->Logf(NAME_Dev, "Invalid sprite frame %d : %d",
			thing->State->SpriteIndex, thing->State->Frame);
#endif
		return;
	}
	sprframe = &sprdef->spriteframes[thing->State->Frame & FF_FRAMEMASK];

	int			lump;
	bool		flip;

	if (sprframe->rotate)
	{
		// choose a different rotation based on player view
		//FIXME must use sprforward here?
		float ang = matan(thing->Origin.y - vieworg.y,
			thing->Origin.x - vieworg.x);
		if (sprframe->lump[0] == sprframe->lump[1])
		{
			ang = AngleMod(ang - thing->Angles.yaw + 180.0 + 45.0 / 2.0);
		}
		else
		{
			ang = AngleMod(ang - thing->Angles.yaw + 180.0 + 45.0 / 4.0);
		}
		vuint32 rot = (vuint32)(ang * 16 / 360.0) & 15;
		lump = sprframe->lump[rot];
		flip = sprframe->flip[rot];
	}
	else
	{
		// use single rotation for all views
		lump = sprframe->lump[0];
		flip = sprframe->flip[0];
	}

	VTexture* Tex = GTextureManager.Textures[lump];
	int TexWidth = Tex->GetWidth();
	int TexHeight = Tex->GetHeight();
	int TexSOffset = Tex->SOffset;
	int TexTOffset = Tex->TOffset;

	TVec	sv[4];

	TVec start = -TexSOffset * sprright;
	TVec end = (TexWidth - TexSOffset) * sprright;

	if (r_fix_sprite_offsets && TexTOffset < TexHeight &&
		2 * TexTOffset + r_sprite_fix_delta >= TexHeight)
	{
		TexTOffset = TexHeight;
	}
	TVec topdelta = TexTOffset * sprup;
	TVec botdelta = (TexTOffset - TexHeight) * sprup;

	sv[0] = sprorigin + start + botdelta;
	sv[1] = sprorigin + start + topdelta;
	sv[2] = sprorigin + end + topdelta;
	sv[3] = sprorigin + end + botdelta;

	if (Alpha < 1.0 || Additive || r_sort_sprites)
	{
		DrawTranslucentPoly(NULL, sv, 4, lump, Alpha, Additive,
			thing->Translation, true, light, Fade, -sprforward, DotProduct(
			sprorigin, -sprforward), flip ? -sprright : sprright, -sprup,
			flip ? sv[2] : sv[1]);
	}
	else
	{
		Drawer->DrawSpritePolygon(sv, GTextureManager[lump], Alpha,
			Additive, thing->Translation, light, Fade, -sprforward,
			DotProduct(sprorigin, -sprforward), flip ? -sprright : sprright,
			-sprup, flip ? sv[2] : sv[1]);
	}
	unguard;
}

//==========================================================================
//
//	VRenderLevel::RenderTranslucentAliasModel
//
//==========================================================================

void VRenderLevel::RenderTranslucentAliasModel(VEntity* mobj, vuint32 light,
	vuint32 Fade, float Alpha, bool Additive, float TimeFrac)
{
	guard(VRenderLevel::RenderTranslucentAliasModel);
	int i;

	float dist = fabs(DotProduct(mobj->Origin - vieworg, viewforward));
	int found = -1;
	float best_dist = -1;
	for (i = 0; i < MAX_TRANS_SPRITES; i++)
	{
		trans_sprite_t &spr = trans_sprites[i];
		if (!spr.Alpha)
		{
			spr.Ent = mobj;
			spr.light = light;
			spr.Fade = Fade;
			spr.Alpha = Alpha;
			spr.Additive = Additive;
			spr.dist = dist;
			spr.type = 2;
			spr.TimeFrac = TimeFrac;
			return;
		}
		if (spr.dist > best_dist)
		{
			found = i;
			best_dist = spr.dist;
		}
	}
	if (best_dist > dist)
	{
		//	All slots are full, draw and replace a far away sprite
		trans_sprite_t &spr = trans_sprites[found];
		if (spr.type == 2)
		{
			DrawEntityModel(spr.Ent, spr.light, spr.Fade, spr.Alpha,
				spr.Additive, spr.TimeFrac);
		}
		else if (spr.type)
		{
			Drawer->DrawSpritePolygon(spr.Verts, GTextureManager[spr.lump],
				spr.Alpha, spr.Additive, spr.translation, spr.light, spr.Fade,
				spr.normal, spr.pdist, spr.saxis, spr.taxis, spr.texorg);
		}
		else
		{
			Drawer->DrawMaskedPolygon(spr.surf, spr.Alpha, spr.Additive);
		}
		spr.Ent = mobj;
		spr.light = light;
		spr.Fade = Fade;
		spr.Alpha = Alpha;
		spr.Additive = Additive;
		spr.dist = dist;
		spr.type = 2;
		spr.TimeFrac = TimeFrac;
		return;
	}
	DrawEntityModel(mobj, light, Fade, Alpha, Additive, TimeFrac);
	unguard;
}

//==========================================================================
//
//	VRenderLevel::RenderAliasModel
//
//==========================================================================

bool VRenderLevel::RenderAliasModel(VEntity* mobj, vuint32 light,
	vuint32 Fade, float Alpha, bool Additive)
{
	guard(VRenderLevel::RenderAliasModel);
	if (!r_models)
	{
		return false;
	}

	float TimeFrac = 0;
	if (mobj->State->Time > 0)
	{
		TimeFrac = 1.0 - (mobj->StateTime / mobj->State->Time);
		TimeFrac = MID(0.0, TimeFrac, 1.0);
	}

	//	Draw it
	if (Alpha < 1.0 || Additive)
	{
		if (!CheckAliasModelFrame(mobj, TimeFrac))
		{
			return false;
		}
		RenderTranslucentAliasModel(mobj, light, Fade, Alpha, Additive,
			TimeFrac);
		return true;
	}
	else
	{
		return DrawEntityModel(mobj, light, Fade, 1.0, false, TimeFrac);
	}
	unguard;
}

//==========================================================================
//
//	VRenderLevel::RenderThing
//
//==========================================================================

void VRenderLevel::RenderThing(VEntity* mobj)
{
	guard(VRenderLevel::RenderThing);
	if (ViewEnt)
	{
		if (mobj == ViewEnt)
		{
			return;
		}
	}
	else
	{
		if (!r_chasecam && (mobj->EntityFlags & VEntity::EF_NetLocalPlayer))
		{
			//	Don't draw client's mobj
			return;
		}
	}

	if (mobj->EntityFlags & VEntity::EF_Hidden)
	{
		return;
	}

	if (mobj->SubSector->VisFrame != r_visframecount)
	{
		return;
	}

	int RendStyle = mobj->RenderStyle;
	float Alpha = mobj->Alpha;
	bool Additive = false;

	if (RendStyle == STYLE_SoulTrans)
	{
		RendStyle = STYLE_Translucent;
		Alpha = transsouls;
	}
	else if (RendStyle == STYLE_OptFuzzy)
	{
		RendStyle = r_drawfuzz ? STYLE_Fuzzy : STYLE_Translucent;
	}

	switch (RendStyle)
	{
	case STYLE_None:
		return;

	case STYLE_Normal:
		Alpha = 1.0;
		break;

	case STYLE_Fuzzy:
		Alpha = 0.1;
		break;

	case STYLE_Add:
		Additive = true;
		break;
	}
	Alpha = MID(0.0, Alpha, 1.0);

	if (!Alpha)
	{
		// Never make a vissprite when MF2_DONTDRAW is flagged.
		return;
	}

	//	Setup lighting
	vuint32 light;
	if (RendStyle == STYLE_Fuzzy)
	{
		light = 0;
	}
	else if ((mobj->State->Frame & FF_FULLBRIGHT) ||
		(mobj->EntityFlags & VEntity::EF_FullBright))
	{
		light = 0xffffffff;
	}
	else
	{
		light = LightPoint(mobj->Origin);
	}
	vuint32 Fade = GetFade(mobj->SubSector);

	//	Try to draw a model. If it's a script and it doesn't
	// specify model for this frame, draw sprite instead.
	if (!RenderAliasModel(mobj, light, Fade, Alpha, Additive))
	{
		RenderSprite(mobj, light, Fade, Alpha, Additive);
	}
	unguard;
}

//==========================================================================
//
//	VRenderLevel::RenderMobjs
//
//==========================================================================

void VRenderLevel::RenderMobjs()
{
	guard(VRenderLevel::RenderMobjs);
	if (!r_draw_mobjs)
	{
		return;
	}

	for (TThinkerIterator<VEntity> Ent(Level); Ent; ++Ent)
	{
		RenderThing(*Ent);
	}
	unguard;
}

//==========================================================================
//
//	VRenderLevel::DrawTranslucentPolys
//
//==========================================================================

void VRenderLevel::DrawTranslucentPolys()
{
	guard(VRenderLevel::DrawTranslucentPolys);
	int i, found;
	do
	{
		found = -1;
		float best_dist = -1;
		for (i = 0; i < MAX_TRANS_SPRITES; i++)
		{
			trans_sprite_t &spr = trans_sprites[i];
			if (!spr.Alpha)
			{
				continue;
			}
			if (spr.dist > best_dist)
			{
				found = i;
				best_dist = spr.dist;
			}
		}
		if (found != -1)
		{
			trans_sprite_t &spr = trans_sprites[found];
			if (spr.type == 2)
			{
				DrawEntityModel(spr.Ent, spr.light, spr.Fade, spr.Alpha,
					spr.Additive, spr.TimeFrac);
			}
			else if (spr.type)
			{
				Drawer->DrawSpritePolygon(spr.Verts, GTextureManager[spr.lump],
					spr.Alpha, spr.Additive, spr.translation, spr.light,
					spr.Fade, spr.normal, spr.pdist, spr.saxis, spr.taxis,
					spr.texorg);
			}
			else
			{
				Drawer->DrawMaskedPolygon(spr.surf, spr.Alpha, spr.Additive);
			}
			spr.Alpha = 0;
		}
	} while (found != -1);
	unguard;
}

//==========================================================================
//
//	VRenderLevel::RenderPSprite
//
//==========================================================================

void VRenderLevel::RenderPSprite(VViewState* VSt, float PSP_DIST,
	vuint32 light, vuint32 Fade, float Alpha, bool Additive)
{
	guard(VRenderLevel::RenderPSprite);
	spritedef_t*		sprdef;
	spriteframe_t*		sprframe;
	int					lump;
	bool				flip;

	// decide which patch to use
	if ((vuint32)VSt->State->SpriteIndex >= MAX_SPRITE_MODELS)
	{
#ifdef PARANOID
		GCon->Logf("R_ProjectSprite: invalid sprite number %d",
			VSt->State->SpriteIndex);
#endif
		return;
	}
	sprdef = &sprites[VSt->State->SpriteIndex];
	if ((VSt->State->Frame & FF_FRAMEMASK) >= sprdef->numframes)
	{
#ifdef PARANOID
		GCon->Logf("R_ProjectSprite: invalid sprite frame %d : %d",
			VSt->State->SpriteIndex, VSt->State->Frame);
#endif
		return;
	}
	sprframe = &sprdef->spriteframes[VSt->State->Frame & FF_FRAMEMASK];

	lump = sprframe->lump[0];
	flip = sprframe->flip[0];
	VTexture* Tex = GTextureManager.Textures[lump];

	int TexWidth = Tex->GetWidth();
	int TexHeight = Tex->GetHeight();
	int TexSOffset = Tex->SOffset;
	int TexTOffset = Tex->TOffset;

	TVec	dv[4];

	float PSP_DISTI = 1.0 / PSP_DIST;
	TVec sprorigin = vieworg + PSP_DIST * viewforward;

	float sprx = 160.0 - VSt->SX + TexSOffset;
	float spry = 100.0 - VSt->SY + TexTOffset;

	spry -= cl->PSpriteSY;

	//	1 / 160 = 0.00625
	TVec start = sprorigin - (sprx * PSP_DIST * 0.00625) * viewright;
	TVec end = start + (TexWidth * PSP_DIST * 0.00625) * viewright;

	//	1 / 160.0 * 120 / 100 =	0.0075
	TVec topdelta = (spry * PSP_DIST * 0.0075) * viewup;
	TVec botdelta = topdelta - (TexHeight * PSP_DIST * 0.0075) * viewup;
	if (old_aspect)
	{
		topdelta *= 100.0 / 120.0;
		botdelta *= 100.0 / 120.0;
	}

	dv[0] = start + botdelta;
	dv[1] = start + topdelta;
	dv[2] = end + topdelta;
	dv[3] = end + botdelta;

	TVec saxis;
	TVec taxis;
	TVec texorg;
	if (flip)
	{
		saxis = -(viewright * 160 * PSP_DISTI);
		texorg = dv[2];
	}
	else
	{
		saxis = viewright * 160 * PSP_DISTI;
		texorg = dv[1];
	}
	if (old_aspect)
		taxis = -(viewup * 160 * PSP_DISTI);
	else
		taxis = -(viewup * 100 * 4 / 3 * PSP_DISTI);

	Drawer->DrawSpritePolygon(dv, GTextureManager[lump], Alpha, Additive,
		0, light, Fade, -viewforward, DotProduct(dv[0], -viewforward),
		saxis, taxis, texorg);
	unguard;
}

//==========================================================================
//
//	VRenderLevel::RenderViewModel
//
//==========================================================================

bool VRenderLevel::RenderViewModel(VViewState* VSt, vuint32 light,
	vuint32 Fade, float Alpha, bool Additive)
{
	guard(VRenderLevel::RenderViewModel);
	if (!r_view_models)
	{
		return false;
	}

	TVec origin = vieworg + (VSt->SX - 1.0) * viewright / 8.0 -
		(VSt->SY - 32.0) * viewup / 6.0;

	float TimeFrac = 0;
	if (VSt->State->Time > 0)
	{
		TimeFrac = 1.0 - (VSt->StateTime / VSt->State->Time);
		TimeFrac = MID(0.0, TimeFrac, 1.0);
	}

	return DrawAliasModel(origin, cl->ViewAngles, VSt->State,
		NULL, 0, light, Fade, Alpha, Additive, true, TimeFrac);
	unguard;
}

//==========================================================================
//
//	VRenderLevel::DrawPlayerSprites
//
//==========================================================================

void VRenderLevel::DrawPlayerSprites()
{
	guard(VRenderLevel::DrawPlayerSprites);
	if (!r_draw_psprites || r_chasecam)
	{
		return;
	}

	// add all active psprites
	for (int i = 0; i < NUMPSPRITES; i++)
	{
		if (!cl->ViewStates[i].State)
		{
			continue;
		}

		vuint32 light;
		if (cl->ViewStates[i].State->Frame & FF_FULLBRIGHT)
		{
			light = 0xffffffff;
		}
		else
		{
			light = LightPoint(vieworg);
		}
		vuint32 Fade = GetFade(Level->PointInSubsector(cl->ViewOrg));
		float Alpha = cl->ViewEntAlpha;
		bool Additive = false;

		if (!RenderViewModel(&cl->ViewStates[i], light, Fade, Alpha,
			Additive))
		{
			RenderPSprite(&cl->ViewStates[i], 3 - i, light, Fade, Alpha,
				Additive);
		}
	}
	unguard;
}

//==========================================================================
//
//	VRenderLevel::DrawCroshair
//
//==========================================================================

void VRenderLevel::DrawCroshair()
{
	guard(VRenderLevel::DrawCroshair);
	if (croshair)
	{
		if (croshair_alpha < 0.0)	croshair_alpha = 0.0;
		if (croshair_alpha > 1.0)	croshair_alpha = 1.0;

		int			cy;
		if (screenblocks < 11)
			cy = (480 - sb_height) / 2;
		else
			cy = 240;
		int handle = GTextureManager.AddPatch(VName(va("CROSHAI%i",
			(int)croshair), VName::AddLower8), TEXTYPE_Pic);
		R_DrawPic(320, cy, handle, croshair_alpha);
	}
	unguard;
}

//==========================================================================
//
//  R_DrawSpritePatch
//
//==========================================================================

void R_DrawSpritePatch(int x, int y, int sprite, int frame, int rot, int translation)
{
	guard(R_DrawSpritePatch);
	bool			flip;
	int				lump;

	spriteframe_t *sprframe = &sprites[sprite].spriteframes[frame & FF_FRAMEMASK];
	flip = sprframe->flip[rot];
	lump = sprframe->lump[rot];
	VTexture* Tex = GTextureManager.Textures[lump];

	Tex->GetWidth();

	float x1 = x - Tex->SOffset;
	float y1 = y - Tex->TOffset;
	float x2 = x1 + Tex->GetWidth();
	float y2 = y1 + Tex->GetHeight();

	x1 *= fScaleX;
	y1 *= fScaleY;
	x2 *= fScaleX;
	y2 *= fScaleY;

	Drawer->DrawSpriteLump(x1, y1, x2, y2, Tex, translation, flip);
	unguard;
}
