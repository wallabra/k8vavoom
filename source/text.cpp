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
#include "gamedefs.h"
#include "cl_local.h"
#include "ui/ui.h"
#include "neoui/neoui.h"


//==========================================================================
//
//  T_Init
//
//==========================================================================
void T_Init () {
  VFont::StaticInit();
}


//==========================================================================
//
//  T_Shutdown
//
//==========================================================================
void T_Shutdown () {
  VFont::StaticShutdown();
}


//==========================================================================
//
//  T_SetFont
//
//==========================================================================
void T_SetFont (VFont *AFont) {
  GRoot->SetFont(AFont);
}


//==========================================================================
//
//  T_SetAlign
//
//==========================================================================
void T_SetAlign (halign_e NewHAlign, valign_e NewVAlign) {
  GRoot->SetTextAlign(NewHAlign, NewVAlign);
}


//==========================================================================
//
//  T_DrawText
//
//==========================================================================
void T_DrawText (int x, int y, const VStr &String, int col) {
  GRoot->DrawText(x, y, String, col, CR_YELLOW, 1.0f);
}


//==========================================================================
//
//  T_TextWidth
//
//==========================================================================
int T_TextWidth (const VStr &s) {
  return GRoot->TextWidth(s);
}


//==========================================================================
//
//  T_StringWidth
//
//==========================================================================
int T_StringWidth (const VStr &s) {
  return GRoot->StringWidth(s);
}


//==========================================================================
//
//  T_TextHeight
//
//==========================================================================
int T_TextHeight (const VStr &s) {
  return GRoot->TextHeight(s);
}


//==========================================================================
//
//  T_FontHeight
//
//==========================================================================
int T_FontHeight () {
  return GRoot->FontHeight();
}


//==========================================================================
//
//  T_DrawCursor
//
//==========================================================================
void T_DrawCursor () {
  GRoot->DrawCursor();
}


//==========================================================================
//
//  T_DrawCursorAt
//
//==========================================================================
void T_DrawCursorAt (int x, int y) {
  GRoot->DrawCursorAt(x, y);
}
