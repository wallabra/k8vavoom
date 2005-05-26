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
//**
//**	Functions to draw patches (by post) directly to screen.
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

// CODE --------------------------------------------------------------------

//==========================================================================
//
//	VOpenGLDrawer::DrawPic
//
//==========================================================================

void VOpenGLDrawer::DrawPic(float x1, float y1, float x2, float y2,
	float s1, float t1, float s2, float t2, int handle, int trans)
{
	guard(VOpenGLDrawer::DrawPic);
	SetPic(handle);
	if (trans)
	{
		glDisable(GL_ALPHA_TEST);
		glEnable(GL_BLEND);
		glColor4f(1, 1, 1, (100.0 - trans) / 100.0);
	}
	else
	{
		glColor4f(1, 1, 1, 1);
	}
	glBegin(GL_QUADS);
	glTexCoord2f(s1 * tex_iw, t1 * tex_ih);
	glVertex2f(x1, y1);
	glTexCoord2f(s2 * tex_iw, t1 * tex_ih);
	glVertex2f(x2, y1);
	glTexCoord2f(s2 * tex_iw, t2 * tex_ih);
	glVertex2f(x2, y2);
	glTexCoord2f(s1 * tex_iw, t2 * tex_ih);
	glVertex2f(x1, y2);
	glEnd();
	if (trans)
	{
		glDisable(GL_BLEND);
		glEnable(GL_ALPHA_TEST);
	}
	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::DrawPicShadow
//
//==========================================================================

void VOpenGLDrawer::DrawPicShadow(float x1, float y1, float x2, float y2,
	float s1, float t1, float s2, float t2, int handle, int shade)
{
	guard(VOpenGLDrawer::DrawPicShadow);
	SetPic(handle);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_BLEND);
	glColor4f(0, 0, 0, (float)shade / 255.0);
	glBegin(GL_QUADS);
	glTexCoord2f(s1 * tex_iw, t1 * tex_ih);
	glVertex2f(x1, y1);
	glTexCoord2f(s2 * tex_iw, t1 * tex_ih);
	glVertex2f(x2, y1);
	glTexCoord2f(s2 * tex_iw, t2 * tex_ih);
	glVertex2f(x2, y2);
	glTexCoord2f(s1 * tex_iw, t2 * tex_ih);
	glVertex2f(x1, y2);
	glEnd();
	glDisable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	unguard;
}

//==========================================================================
//
//  VOpenGLDrawer::FillRectWithFlat
//
// 	Fills rectangle with flat.
//
//==========================================================================

void VOpenGLDrawer::FillRectWithFlat(float x1, float y1, float x2, float y2,
	float s1, float t1, float s2, float t2, const char* fname)
{
	guard(VOpenGLDrawer::FillRectWithFlat);
	SetTexture(GTextureManager.NumForName(FName(fname, FNAME_AddLower8),
		TEXTYPE_Flat, true, false));

	glColor4f(1, 1, 1, 1);
	glBegin(GL_QUADS);
	glTexCoord2f(s1 * tex_iw, t1 * tex_ih);
	glVertex2f(x1, y1);
	glTexCoord2f(s2 * tex_iw, t1 * tex_ih);
	glVertex2f(x2, y1);
	glTexCoord2f(s2 * tex_iw, t2 * tex_ih);
	glVertex2f(x2, y2);
	glTexCoord2f(s1 * tex_iw, t2 * tex_ih);
	glVertex2f(x1, y2);
	glEnd();
	unguard;
}

//==========================================================================
//
//  VOpenGLDrawer::FillRect
//
//==========================================================================

void VOpenGLDrawer::FillRect(float x1, float y1, float x2, float y2,
	dword color)
{
	guard(VOpenGLDrawer::FillRect);
	SetColor(color);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_ALPHA_TEST);
	glBegin(GL_QUADS);
	glVertex2f(x1, y1);
	glVertex2f(x2, y1);
	glVertex2f(x2, y2);
	glVertex2f(x1, y2);
	glEnd();
	glEnable(GL_ALPHA_TEST);
	glEnable(GL_TEXTURE_2D);
	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::ShadeRect
//
//  Fade all the screen buffer, so that the menu is more readable,
// especially now that we use the small hufont in the menus...
//
//==========================================================================

void VOpenGLDrawer::ShadeRect(int x, int y, int w, int h, int darkening)
{
	guard(VOpenGLDrawer::ShadeRect);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);

	glColor4f(0, 0, 0, darkening / 32.0);
	glBegin(GL_QUADS);
	glVertex2f(x, y);
	glVertex2f(x + w, y);
	glVertex2f(x + w, y + h);
	glVertex2f(x, y + h);
	glEnd();

	glDisable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glEnable(GL_TEXTURE_2D);
	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::DrawConsoleBackground
//
//==========================================================================

void VOpenGLDrawer::DrawConsoleBackground(int h)
{
	guard(VOpenGLDrawer::DrawConsoleBackground);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);

	glColor4f(0, 0, 0.5, 0.75);
	glBegin(GL_QUADS);
	glVertex2f(0, 0);
	glVertex2f(ScreenWidth, 0);
	glVertex2f(ScreenWidth, h);
	glVertex2f(0, h);
	glEnd();

	glDisable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glEnable(GL_TEXTURE_2D);
	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::DrawSpriteLump
//
//==========================================================================

void VOpenGLDrawer::DrawSpriteLump(float x1, float y1, float x2, float y2,
	int lump, int translation, boolean flip)
{
	guard(VOpenGLDrawer::DrawSpriteLump);
	SetSpriteLump(lump, translation);

	TTexture* Tex = GTextureManager.Textures[lump];
	float s1, s2;
	if (flip)
	{
		s1 = Tex->GetWidth() * tex_iw;
		s2 = 0;
	}
	else
	{
		s1 = 0;
		s2 = Tex->GetWidth() * tex_iw;
	}
	float texh = Tex->GetHeight() * tex_ih;

	glColor4f(1, 1, 1, 1);
	glBegin(GL_QUADS);

	glTexCoord2f(s1, 0);
	glVertex2f(x1, y1);
	glTexCoord2f(s2, 0);
	glVertex2f(x2, y1);
	glTexCoord2f(s2, texh);
	glVertex2f(x2, y2);
	glTexCoord2f(s1, texh);
	glVertex2f(x1, y2);

	glEnd();
	unguard;
}


//==========================================================================
//
//	VOpenGLDrawer::StartAutomap
//
//==========================================================================

void VOpenGLDrawer::StartAutomap(void)
{
	guard(VOpenGLDrawer::StartAutomap);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);
	glBegin(GL_LINES);
	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::DrawLine
//
//==========================================================================

void VOpenGLDrawer::DrawLine(int x1, int y1, dword c1, int x2, int y2, dword c2)
{
	guard(VOpenGLDrawer::DrawLine);
	SetColor(c1);
	glVertex2f(x1, y1);
	SetColor(c2);
	glVertex2f(x2, y2);
	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::EndAutomap
//
//==========================================================================

void VOpenGLDrawer::EndAutomap(void)
{
	guard(VOpenGLDrawer::EndAutomap);
	glEnd();
	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
	glEnable(GL_ALPHA_TEST);
	glEnable(GL_TEXTURE_2D);
	unguard;
}

//**************************************************************************
//
//	$Log$
//	Revision 1.13  2005/05/26 16:50:14  dj_jl
//	Created texture manager class
//
//	Revision 1.12  2002/07/13 07:38:00  dj_jl
//	Added drawers to the object tree.
//	
//	Revision 1.11  2002/01/11 18:24:44  dj_jl
//	Added guard macros
//	
//	Revision 1.10  2002/01/07 12:16:42  dj_jl
//	Changed copyright year
//	
//	Revision 1.9  2001/10/04 17:23:29  dj_jl
//	Got rid of some warnings
//	
//	Revision 1.8  2001/09/12 17:31:27  dj_jl
//	Rectangle drawing and direct update for plugins
//	
//	Revision 1.7  2001/08/31 17:27:15  dj_jl
//	Beautification
//	
//	Revision 1.6  2001/08/29 17:49:01  dj_jl
//	Line colors in RGBA format
//	
//	Revision 1.5  2001/08/15 17:15:55  dj_jl
//	Drawer API changes, removed wipes
//	
//	Revision 1.4  2001/08/01 17:33:58  dj_jl
//	Fixed drawing of spite lump for player setup menu, beautification
//	
//	Revision 1.3  2001/07/31 17:16:30  dj_jl
//	Just moved Log to the end of file
//	
//	Revision 1.2  2001/07/27 14:27:54  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************
