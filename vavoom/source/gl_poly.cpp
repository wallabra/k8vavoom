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

// HEADER FILES ------------------------------------------------------------

#include "gl_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
static float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anorm_dots.h"
;

// CODE --------------------------------------------------------------------

//==========================================================================
//
//	TOpenGLDrawer::FlushCaches
//
//==========================================================================

void TOpenGLDrawer::FlushCaches(bool free_blocks)
{
	guard(TOpenGLDrawer::FlushCaches);
	int				i;
	surfcache_t		*blines;
	surfcache_t		*block;

	if (free_blocks)
	{
		for (i = 0; i < NUM_BLOCK_SURFS; i++)
		{
			for (blines = cacheblocks[i]; blines; blines = blines->bnext)
			{
				for (block = blines; block; block = block->lnext)
				{
					if (block->owner)
						*block->owner = NULL;
				}
			}
		}
	}

	memset(blockbuf, 0, sizeof(blockbuf));
	freeblocks = NULL;
	for (i = 0; i < NUM_CACHE_BLOCKS; i++)
	{
		blockbuf[i].chain = freeblocks;
		freeblocks = &blockbuf[i];
	}
	for (i = 0; i < NUM_BLOCK_SURFS; i++)
	{
		cacheblocks[i] = freeblocks;
		freeblocks = freeblocks->chain;
		cacheblocks[i]->width = BLOCK_WIDTH;
		cacheblocks[i]->height = BLOCK_HEIGHT;
		cacheblocks[i]->blocknum = i;
	}
	unguard;
}

//==========================================================================
//
//	TOpenGLDrawer::AllocBlock
//
//==========================================================================

surfcache_t	*TOpenGLDrawer::AllocBlock(int width, int height)
{
	guard(TOpenGLDrawer::AllocBlock);
	int				i;
	surfcache_t		*blines;
	surfcache_t		*block;
	surfcache_t		*other;

	for (i = 0; i < NUM_BLOCK_SURFS; i++)
	{
		for (blines = cacheblocks[i]; blines; blines = blines->bnext)
		{
			if (blines->height != height)
			{
				continue;
			}
			for (block = blines; block; block = block->lnext)
			{
				if (block->owner)
					continue;
				if (block->width < width)
					continue;
				if (block->width > width)
				{
					if (!freeblocks)
						Sys_Error("No more free blocks");
					other = freeblocks;
					freeblocks = other->chain;
					other->s = block->s + width;
					other->t = block->t;
					other->width = block->width - width;
					other->height = block->height;
					other->lnext = block->lnext;
					if (other->lnext)
						other->lnext->lprev = other;
					block->lnext = other;
					other->lprev = block;
					block->width = width;
					other->owner = NULL;
					other->blocknum = i;
				}
				return block;
			}
		}
	}

	for (i = 0; i < NUM_BLOCK_SURFS; i++)
	{
		for (blines = cacheblocks[i]; blines; blines = blines->bnext)
		{
			if (blines->height < height)
			{
				continue;
			}
			if (blines->lnext)
			{
				continue;
			}

			block = blines;
			if (block->height > height)
			{
				if (!freeblocks)
					Sys_Error("No more free blocks");
				other = freeblocks;
				freeblocks = other->chain;
				other->s = 0;
				other->t = block->t + height;
				other->width = block->width;
				other->height = block->height - height;
				other->lnext = NULL;
				other->lprev = NULL;
				other->bnext = block->bnext;
				if (other->bnext)
					other->bnext->bprev = other;
				block->bnext = other;
				other->bprev = block;
				block->height = height;
				other->owner = NULL;
				other->blocknum = i;
			}

			if (!freeblocks)
				Sys_Error("No more free blocks");
			other = freeblocks;
			freeblocks = other->chain;
			other->s = block->s + width;
			other->t = block->t;
			other->width = block->width - width;
			other->height = block->height;
			other->lnext = NULL;
			block->lnext = other;
			other->lprev = block;
			block->width = width;
			other->owner = NULL;
			other->blocknum = i;

			return block;
		}
	}

	Sys_Error("overflow");
	return NULL;
	unguard;
}

//==========================================================================
//
//	TOpenGLDrawer::FreeSurfCache
//
//==========================================================================

void TOpenGLDrawer::FreeSurfCache(surfcache_t *block)
{
	guard(TOpenGLDrawer::FreeSurfCache);
	surfcache_t		*other;

	*block->owner = NULL;
	block->owner = NULL;
	if (block->lnext && !block->lnext->owner)
	{
		other = block->lnext;
		block->width += other->width;
		block->lnext = other->lnext;
		if (block->lnext)
			block->lnext->lprev = block;
		other->chain = freeblocks;
		freeblocks = other;
	}
	if (block->lprev && !block->lprev->owner)
	{
		other = block;
		block = block->lprev;
		block->width += other->width;
		block->lnext = other->lnext;
		if (block->lnext)
			block->lnext->lprev = block;
		other->chain = freeblocks;
		freeblocks = other;
	}

	if (block->lprev || block->lnext)
	{
		return;
	}

	if (block->bnext && !block->bnext->lnext)
	{
		other = block->bnext;
		block->height += other->height;
		block->bnext = other->bnext;
		if (block->bnext)
			block->bnext->bprev = block;
		other->chain = freeblocks;
		freeblocks = other;
	}
	if (block->bprev && !block->bprev->lnext)
	{
		other = block;
		block = block->bprev;
		block->height += other->height;
		block->bnext = other->bnext;
		if (block->bnext)
			block->bnext->bprev = block;
		other->chain = freeblocks;
		freeblocks = other;
	}
	unguard;
}

//==========================================================================
//
//	TOpenGLDrawer::CacheSurface
//
//==========================================================================

void TOpenGLDrawer::CacheSurface(surface_t *surface)
{
	guard(TOpenGLDrawer::CacheSurface);
	surfcache_t     *cache;
	int				smax, tmax;
	int				i, j, bnum;

	//
	// see if the cache holds apropriate data
	//
	cache = surface->cachespots[0];

	if (cache && !cache->dlight && surface->dlightframe != r_dlightframecount
			&& cache->lightlevel == surface->lightlevel)
	{
		bnum = cache->blocknum;
		cache->chain = light_chain[bnum];
		light_chain[bnum] = cache;
		return;
	}

	//
	// determine shape of surface
	//
	smax = (surface->extents[0] >> 4) + 1;
	tmax = (surface->extents[1] >> 4) + 1;
	
	//
	// allocate memory if needed
	//
	if (!cache)     // if a texture just animated, don't reallocate it
	{
		cache = AllocBlock(smax, tmax);
		surface->cachespots[0] = cache;
		cache->owner = &surface->cachespots[0];
		cache->surf = surface;
	}
	
	if (surface->dlightframe == r_dlightframecount)
		cache->dlight = 1;
	else
		cache->dlight = 0;
	cache->lightlevel = surface->lightlevel;

	// calculate the lightings
	R_BuildLightMap(surface, 0);
	bnum = cache->blocknum;
	block_changed[bnum] = true;

	for (j = 0; j < tmax; j++)
	{
		for (i = 0; i < smax; i++)
		{
			rgba_t &lb = light_block[bnum][(j + cache->t) * BLOCK_WIDTH +
				i + cache->s];
			lb.r = byte(blocklightsr[j * smax + i] >> 8);
			lb.g = byte(blocklightsg[j * smax + i] >> 8);
			lb.b = byte(blocklightsb[j * smax + i] >> 8);
			lb.a = 255;
		}
	}
	cache->chain = light_chain[bnum];
	light_chain[bnum] = cache;

	// specular highlights
	for (j = 0; j < tmax; j++)
	{
		for (i = 0; i < smax; i++)
		{
			rgba_t &lb = add_block[bnum][(j + cache->t) * BLOCK_WIDTH +
				i + cache->s];
			lb.r = byte(blockaddlightsr[j * smax + i] >> 8);
			lb.g = byte(blockaddlightsg[j * smax + i] >> 8);
			lb.b = byte(blockaddlightsb[j * smax + i] >> 8);
			lb.a = 255;
		}
	}
	if (r_light_add)
	{
		cache->addchain = add_chain[bnum];
		add_chain[bnum] = cache;
		add_changed[bnum] = true;
	}
	unguard;
}

//==========================================================================
//
//	glVertex
//
//==========================================================================

inline void	glVertex(const TVec &v)
{
	glVertex3f(v.x, v.y, v.z);
}

//==========================================================================
//
//	TOpenGLDrawer::DrawPolygon
//
//==========================================================================

void TOpenGLDrawer::DrawPolygon(TVec *cv, int count, int texture, int)
{
	guard(TOpenGLDrawer::DrawPolygon);
	int			i;
	bool		lightmaped;
	surface_t	*surf = r_surface;

	lightmaped = surf->lightmap != NULL ||
		surf->dlightframe == r_dlightframecount;

	if (lightmaped)
	{
		CacheSurface(surf);
		if (mtexable)
		{
			return;
		}
	}

	SetTexture(texture);

	if (lightmaped)
	{
		glColor4f(1, 1, 1, 1);
	}
	else
	{
		float lev = float(surf->lightlevel) / 255.0;
		glColor4f(lev, lev, lev, 1.0);
	}

	texinfo_t *tex = r_surface->texinfo;
	glBegin(GL_POLYGON);
	for (i = 0; i < count; i++)
	{
		glTexCoord2f((DotProduct(cv[i], tex->saxis) + tex->soffs) * tex_iw,
			(DotProduct(cv[i], tex->taxis) + tex->toffs) * tex_ih);
		glVertex(cv[i]);
	}
	glEnd();
	unguard;
}

//==========================================================================
//
//	TOpenGLDrawer::WorldDrawing
//
//==========================================================================

void TOpenGLDrawer::WorldDrawing(void)
{
	guard(TOpenGLDrawer::WorldDrawing);
	int			lb, i;
	surfcache_t	*cache;
	GLfloat		s, t;
	GLfloat		lights, lightt;
	surface_t	*surf;
	texinfo_t	*tex;

	if (mtexable)
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		SelectTexture(1);
		glEnable(GL_TEXTURE_2D);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
		SelectTexture(0);
		glColor4f(1, 1, 1, 1);

		for (lb = 0; lb < NUM_BLOCK_SURFS; lb++)
		{
			if (!light_chain[lb])
			{
				continue;
			}

			SelectTexture(1);
			glBindTexture(GL_TEXTURE_2D, lmap_id[lb]);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			if (block_changed[lb])
			{
				block_changed[lb] = false;
				glTexImage2D(GL_TEXTURE_2D, 0, 4, BLOCK_WIDTH, BLOCK_HEIGHT,
					0, GL_RGBA, GL_UNSIGNED_BYTE, light_block[lb]);
			}
			SelectTexture(0);

			for (cache = light_chain[lb]; cache; cache = cache->chain)
			{
				surf = cache->surf;
				tex = surf->texinfo;

				SetTexture(tex->pic);
				glBegin(GL_POLYGON);
				for (i = 0; i < surf->count; i++)
				{
					s = DotProduct(surf->verts[i], tex->saxis) + tex->soffs;
					t = DotProduct(surf->verts[i], tex->taxis) + tex->toffs;
					lights = (s - surf->texturemins[0] +
						cache->s * 16 + 8) / (BLOCK_WIDTH * 16);
					lightt = (t - surf->texturemins[1] +
						cache->t * 16 + 8) / (BLOCK_HEIGHT * 16);
					MultiTexCoord(0, s * tex_iw, t * tex_ih);
					MultiTexCoord(1, lights, lightt);
					glVertex(surf->verts[i]);
				}
				glEnd();
			}
		}

		SelectTexture(1);
		glDisable(GL_TEXTURE_2D);
		SelectTexture(0);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
	else
	{
		glDepthMask(0);		// don't bother writing Z
		glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
		glEnable(GL_BLEND);
		glColor4f(1, 1, 1, 1);

		for (lb = 0; lb < NUM_BLOCK_SURFS; lb++)
		{
			if (!light_chain[lb])
			{
				continue;
			}

			glBindTexture(GL_TEXTURE_2D, lmap_id[lb]);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			if (block_changed[lb])
			{
				block_changed[lb] = false;
				glTexImage2D(GL_TEXTURE_2D, 0, 4, BLOCK_WIDTH, BLOCK_HEIGHT,
					0, GL_RGBA, GL_UNSIGNED_BYTE, light_block[lb]);
			}

			for (cache = light_chain[lb]; cache; cache = cache->chain)
			{
				surf = cache->surf;
				tex = surf->texinfo;

				glBegin(GL_POLYGON);
				for (i = 0; i < surf->count; i++)
				{
					s = (DotProduct(surf->verts[i], tex->saxis) + tex->soffs -
						surf->texturemins[0] + cache->s * 16 + 8) / (BLOCK_WIDTH * 16);
					t = (DotProduct(surf->verts[i], tex->taxis) + tex->toffs -
						surf->texturemins[1] + cache->t * 16 + 8) / (BLOCK_HEIGHT * 16);
					glTexCoord2f(s, t);
					glVertex(surf->verts[i]);
				}
				glEnd();
			}
		}

		glDisable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(1);		// back to normal Z buffering
	}

	//
	//	Add specular lights
	//
	if (specular_highlights)
	{
		glDepthMask(0);		// don't bother writing Z
		glBlendFunc(GL_ONE, GL_ONE);
		glEnable(GL_BLEND);
		glColor4f(1, 1, 1, 1);
		if (r_fog)
		{
			glDisable(GL_FOG);
		}

		for (lb = 0; lb < NUM_BLOCK_SURFS; lb++)
		{
			if (!add_chain[lb])
			{
				continue;
			}

			glBindTexture(GL_TEXTURE_2D, addmap_id[lb]);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			if (add_changed[lb])
			{
				add_changed[lb] = false;
				glTexImage2D(GL_TEXTURE_2D, 0, 4, BLOCK_WIDTH, BLOCK_HEIGHT,
					0, GL_RGBA, GL_UNSIGNED_BYTE, add_block[lb]);
			}

			for (cache = add_chain[lb]; cache; cache = cache->addchain)
			{
				surf = cache->surf;
				tex = surf->texinfo;

				glBegin(GL_POLYGON);
				for (i = 0; i < surf->count; i++)
				{
					s = (DotProduct(surf->verts[i], tex->saxis) + tex->soffs -
						surf->texturemins[0] + cache->s * 16 + 8) / (BLOCK_WIDTH * 16);
					t = (DotProduct(surf->verts[i], tex->taxis) + tex->toffs -
						surf->texturemins[1] + cache->t * 16 + 8) / (BLOCK_HEIGHT * 16);
					glTexCoord2f(s, t);
					glVertex(surf->verts[i]);
				}
				glEnd();
			}
		}

		glDisable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(1);		// back to normal Z buffering
		if (r_fog)
		{
			glEnable(GL_FOG);
		}
	}
	unguard;
}

//==========================================================================
//
//	TOpenGLDrawer::BeginSky
//
//==========================================================================

void TOpenGLDrawer::BeginSky(void)
{
	guard(TOpenGLDrawer::BeginSky);
	glDepthRange(0.99, 1.0);

	//	Sky polys are alredy translated
	glPushMatrix();
	glTranslatef(vieworg.x, vieworg.y, vieworg.z);

	if (r_fog)
	{
		glDisable(GL_FOG);
	}
	unguard;
}

//==========================================================================
//
//	TOpenGLDrawer::DrawSkyPolygon
//
//==========================================================================

void TOpenGLDrawer::DrawSkyPolygon(TVec *cv, int count,
	int texture1, float offs1, int texture2, float offs2)
{
	guard(TOpenGLDrawer::DrawSkyPolygon);
	int		i;

	texinfo_t *tex = r_surface->texinfo;
	if (mtexable && texture2)
	{
		SetSkyTexture(texture1, false);
		SelectTexture(1);
		glEnable(GL_TEXTURE_2D);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
		SetSkyTexture(texture2, true);
		SelectTexture(0);

		glColor4f(1, 1, 1, 1);
		glBegin(GL_POLYGON);
		for (i = 0; i < count; i++)
		{
			MultiTexCoord(0, 
				(DotProduct(cv[i], tex->saxis) + tex->soffs - offs1) * tex_iw,
				(DotProduct(cv[i], tex->taxis) + tex->toffs) * tex_ih);
			MultiTexCoord(1, 
				(DotProduct(cv[i], tex->saxis) + tex->soffs - offs2) * tex_iw,
				(DotProduct(cv[i], tex->taxis) + tex->toffs) * tex_ih);
			glVertex(cv[i]);
		}
		glEnd();

		SelectTexture(1);
		glDisable(GL_TEXTURE_2D);
		SelectTexture(0);
	}
	else
	{
		SetSkyTexture(texture1, false);
		glBegin(GL_POLYGON);
		glColor4f(1, 1, 1, 1);
		for (i = 0; i < count; i++)
		{
			glTexCoord2f(
				(DotProduct(cv[i], tex->saxis) + tex->soffs - offs1) * tex_iw,
				(DotProduct(cv[i], tex->taxis) + tex->toffs) * tex_ih);
			glVertex(cv[i]);
		}
		glEnd();

		if (texture2)
		{
			SetSkyTexture(texture2, true);
			glEnable(GL_BLEND);
			glBegin(GL_POLYGON);
			glColor4f(1, 1, 1, 1);
			for (i = 0; i < count; i++)
			{
				glTexCoord2f(
					(DotProduct(cv[i], tex->saxis) + tex->soffs - offs2) * tex_iw,
					(DotProduct(cv[i], tex->taxis) + tex->toffs) * tex_ih);
				glVertex(cv[i]);
			}
			glEnd();
			glDisable(GL_BLEND);
		}
	}
	unguard;
}

//==========================================================================
//
//	TOpenGLDrawer::EndSky
//
//==========================================================================

void TOpenGLDrawer::EndSky(void)
{
	guard(TOpenGLDrawer::EndSky);
	if (r_fog)
	{
		glEnable(GL_FOG);
	}

	glPopMatrix();
	glDepthRange(0.0, 1.0);
	unguard;
}

//==========================================================================
//
//	TOpenGLDrawer::DrawMaskedPolygon
//
//==========================================================================

void TOpenGLDrawer::DrawMaskedPolygon(TVec *cv, int count,
	int texture, int translucency)
{
	guard(TOpenGLDrawer::DrawMaskedPolygon);
	int			i, r, g, b, w, h, size;
	double		iscale;
	surface_t	*surf = r_surface;

	SetTexture(texture);
	glEnable(GL_ALPHA_TEST);
	if (blend_sprites || translucency)
	{
		glAlphaFunc(GL_GREATER, 0.0);
		glEnable(GL_BLEND);
	}

	R_BuildLightMap(surf, 0);
	w = (surf->extents[0] >> 4) + 1;
	h = (surf->extents[1] >> 4) + 1;
	size = w * h;
	r = 0;
	g = 0;
	b = 0;
	for (i = 0; i < size; i++)
	{
		r += 255 * 256 - blocklightsr[i];
		g += 255 * 256 - blocklightsg[i];
		b += 255 * 256 - blocklightsb[i];
	}
	float alpha = (100.0 - translucency) / 100.0;
	iscale = 1.0 / (size * 255 * 256);
	glColor4f(r * iscale, g * iscale, b * iscale, alpha);

	texinfo_t *tex = r_surface->texinfo;
	glBegin(GL_POLYGON);
	for (i = 0; i < count; i++)
	{
		glTexCoord2f((DotProduct(cv[i], tex->saxis) + tex->soffs) * tex_iw,
			(DotProduct(cv[i], tex->taxis) + tex->toffs) * tex_ih);
		glVertex(cv[i]);
	}
	glEnd();

	if (blend_sprites || translucency)
	{
		glAlphaFunc(GL_GREATER, 0.666);
		glDisable(GL_BLEND);
	}
	glDisable(GL_ALPHA_TEST);
	unguard;
}

//==========================================================================
//
//	TOpenGLDrawer::DrawSpritePolygon
//
//==========================================================================

void TOpenGLDrawer::DrawSpritePolygon(TVec *cv, int lump,
	int translucency, int translation, dword light)
{
	guard(TOpenGLDrawer::DrawSpritePolygon);
	TVec	texpt;

	SetSpriteLump(lump, translation);

	if (blend_sprites || translucency)
	{
		glAlphaFunc(GL_GREATER, 0.0);
		glEnable(GL_BLEND);
	}
	glEnable(GL_ALPHA_TEST);

	dword alpha = 255 * (100 - translucency) / 100;
	SetColor((light & 0x00ffffff) | (alpha << 24));

	glBegin(GL_QUADS);

	texpt = cv[0] - r_texorg;
	glTexCoord2f(DotProduct(texpt, r_saxis) * tex_iw,
		DotProduct(texpt, r_taxis) * tex_ih);
	glVertex(cv[0]);

	texpt = cv[1] - r_texorg;
	glTexCoord2f(DotProduct(texpt, r_saxis) * tex_iw,
		DotProduct(texpt, r_taxis) * tex_ih);
	glVertex(cv[1]);

	texpt = cv[2] - r_texorg;
	glTexCoord2f(DotProduct(texpt, r_saxis) * tex_iw,
		DotProduct(texpt, r_taxis) * tex_ih);
	glVertex(cv[2]);

	texpt = cv[3] - r_texorg;
	glTexCoord2f(DotProduct(texpt, r_saxis) * tex_iw,
		DotProduct(texpt, r_taxis) * tex_ih);
	glVertex(cv[3]);

	glEnd();

	if (blend_sprites || translucency)
	{
		glAlphaFunc(GL_GREATER, 0.666);
		glDisable(GL_BLEND);
	}
	glDisable(GL_ALPHA_TEST);
	unguard;
}

//==========================================================================
//
//	TOpenGLDrawer::DrawAliasModel
//
//==========================================================================

void TOpenGLDrawer::DrawAliasModel(const TVec &origin, const TAVec &angles,
	model_t *model, int frame, const char *skin, dword light,
	int translucency, bool is_view_model)
{
	guard(TOpenGLDrawer::DrawAliasModel);
	mmdl_t		*pmdl;
	mframe_t	*framedesc;
	mskin_t		*pskindesc;
	float 		l;
	int			index;
	trivertx_t	*verts;
	int			*order;
	int			count;
	float		shadelightr;
	float		shadelightg;
	float		shadelightb;
	float		*shadedots;
	float		alpha;

	if (is_view_model)
	{
		// hack the depth range to prevent view model from poking into walls
		glDepthRange(0.0, 0.3);
	}

	//
	// get lighting information
	//
	shadelightr = ((light >> 16) & 0xff) / 510.0;
	shadelightg = ((light >> 8) & 0xff) / 510.0;
	shadelightb = (light & 0xff) / 510.0;
	shadedots = r_avertexnormal_dots[((int)(angles.yaw * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
	alpha = (100.0 - translucency) / 100.0;
	if (!model_lighting)
	{
		SetColor((light & 0x00ffffff) | ((255 * (100 - translucency) / 100) << 24));
	}
	
	//
	// locate the proper data
	//
	pmdl = (mmdl_t *)Mod_Extradata(model);

	// Hack to make sure that skin loading doesn't free model
	Z_ChangeTag(pmdl, PU_STATIC);

	//
	// draw all the triangles
	//

	glPushMatrix();
	glTranslatef(origin.x, origin.y, origin.z);

	glRotatef(angles.yaw,  0, 0, 1);
	glRotatef(angles.pitch,  0, 1, 0);
	glRotatef(angles.roll,  1, 0, 0);

	if ((frame >= pmdl->numframes) || (frame < 0))
	{
		cond << "no such frame " << frame << endl;
		frame = 0;
	}
	framedesc = (mframe_t*)((byte *)pmdl + pmdl->ofsframes + frame * pmdl->framesize);

	glTranslatef(framedesc->scale_origin[0], framedesc->scale_origin[1], framedesc->scale_origin[2]);
	glScalef(framedesc->scale[0], framedesc->scale[1], framedesc->scale[2]);

	if (skin && *skin)
	{
		SetSkin(skin);
	}
	else
	{
		pskindesc = (mskin_t *)((byte *)pmdl + pmdl->ofsskins);
		SetSkin(pskindesc->name);
	}

	glShadeModel(GL_SMOOTH);
	if (translucency)
	{
		glEnable(GL_BLEND);
	}

	verts = (trivertx_t *)(framedesc + 1);
	order = (int *)((byte *)pmdl + pmdl->ofscmds);

	while (*order)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (count < 0)
		{
			count = -count;
			glBegin(GL_TRIANGLE_FAN);
		}
		else
		{
			glBegin(GL_TRIANGLE_STRIP);
		}

		do
		{
			// texture coordinates come from the draw list
			glTexCoord2f(((float *)order)[0], ((float *)order)[1]);
			order += 2;

			// normals and vertexes come from the frame list
			index = *order++;
			if (model_lighting)
			{
				l = shadedots[verts[index].lightnormalindex];
				glColor4f(l * shadelightr, l * shadelightg, l * shadelightb, alpha);
			}
			glVertex3f(verts[index].v[0], verts[index].v[1], verts[index].v[2]);
		} while (--count);

		glEnd();
	}

	glShadeModel(GL_FLAT);
	if (translucency)
	{
		glDisable(GL_BLEND);
	}

	glPopMatrix();
	if (is_view_model)
	{
		glDepthRange(0.0, 1.0);
	}

	// Make it cachable again
	Z_ChangeTag(pmdl, PU_CACHE);
	unguard;
}

//==========================================================================
//
//	TOpenGLDrawer::StartParticles
//
//==========================================================================

void TOpenGLDrawer::StartParticles(void)
{
	guard(TOpenGLDrawer::StartParticles);
	glEnable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.0);
	if (pointparmsable)
	{
		GLfloat parms[3] = { 0.0, 1.0, 0.0 };
		p_PointParameterfv(GLenum(GL_DISTANCE_ATTENUATION_EXT), parms);
		p_PointParameterf(GLenum(GL_POINT_FADE_THRESHOLD_SIZE_EXT), 1.0);
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_POINT_SMOOTH);
		glBegin(GL_POINTS);
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D, particle_texture);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, maxfilter);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minfilter);
		glBegin(GL_QUADS);
	}
	unguard;
}

//==========================================================================
//
//	TOpenGLDrawer::DrawParticle
//
//==========================================================================

void TOpenGLDrawer::DrawParticle(particle_t *p)
{
	guard(TOpenGLDrawer::DrawParticle);
	SetColor(p->color);
	if (pointparmsable)
	{
		glVertex(p->org);
	}
	else
	{
		glTexCoord2f(0, 0); glVertex(p->org - viewright + viewup);
		glTexCoord2f(1, 0); glVertex(p->org + viewright + viewup);
		glTexCoord2f(1, 1); glVertex(p->org + viewright - viewup);
		glTexCoord2f(0, 1); glVertex(p->org - viewright - viewup);
	}
	unguard;
}

//==========================================================================
//
//	TOpenGLDrawer::EndParticles
//
//==========================================================================

void TOpenGLDrawer::EndParticles(void)
{
	guard(TOpenGLDrawer::EndParticles);
	glEnd();
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.666);
	if (pointparmsable)
	{
		glDisable(GL_POINT_SMOOTH);
		glEnable(GL_TEXTURE_2D);
	}
	unguard;
}

//**************************************************************************
//
//	$Log$
//	Revision 1.20  2002/03/28 17:58:02  dj_jl
//	Added support for scaled textures.
//
//	Revision 1.19  2002/03/02 17:32:33  dj_jl
//	Fixed specular lights when fog is enabled.
//	
//	Revision 1.18  2002/01/11 18:24:44  dj_jl
//	Added guard macros
//	
//	Revision 1.17  2002/01/07 12:16:42  dj_jl
//	Changed copyright year
//	
//	Revision 1.16  2001/11/12 18:47:08  dj_jl
//	Some quick fixes
//	
//	Revision 1.15  2001/11/09 14:18:40  dj_jl
//	Added specular highlights
//	
//	Revision 1.14  2001/11/02 18:35:55  dj_jl
//	Sky optimizations
//	
//	Revision 1.13  2001/10/18 17:36:31  dj_jl
//	A lots of changes for Alpha 2
//	
//	Revision 1.12  2001/10/12 17:28:26  dj_jl
//	Blending of sprite borders
//	
//	Revision 1.11  2001/10/09 17:21:39  dj_jl
//	Added sky begining and ending functions
//	
//	Revision 1.10  2001/10/04 17:23:29  dj_jl
//	Got rid of some warnings
//	
//	Revision 1.9  2001/09/05 12:21:42  dj_jl
//	Release changes
//	
//	Revision 1.8  2001/08/31 17:27:15  dj_jl
//	Beautification
//	
//	Revision 1.7  2001/08/21 17:46:08  dj_jl
//	Added R_TextureAnimation, made SetTexture recognize flats
//	
//	Revision 1.6  2001/08/07 16:46:23  dj_jl
//	Added player models, skins and weapon
//	
//	Revision 1.5  2001/08/04 17:31:16  dj_jl
//	Added depth hack for weapon models
//	Added support for multitexture extensions
//	
//	Revision 1.4  2001/08/01 17:36:11  dj_jl
//	Added alpha test to the particle drawing
//	
//	Revision 1.3  2001/07/31 17:16:30  dj_jl
//	Just moved Log to the end of file
//	
//	Revision 1.2  2001/07/27 14:27:54  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************
