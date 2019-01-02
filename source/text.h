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

// horisontal alignement
enum halign_e {
  hleft,   //Left
  hcentre, //Centred
  hright,  //Right
  hcenter = hcentre,
};

// vertical alignement
enum valign_e {
  vtop,    //Top
  vcentre, //Centre
  vbottom, //Bottom
  vcenter = vcentre,
};

// text colours, these must match the constants used in ACS
enum {
  CR_UNDEFINED = -1,
  CR_BRICK, //A
  CR_TAN, //B
  CR_GRAY, //C
  CR_GREEN, //D
  CR_BROWN, //E
  CR_GOLD, //F
  CR_RED, //G
  CR_BLUE, //H
  CR_ORANGE, //I
  CR_WHITE, //J
  CR_YELLOW, //K
  CR_UNTRANSLATED, //L
  CR_BLACK, //M
  CR_LIGHTBLUE, //N
  CR_CREAM, //O
  CR_OLIVE, //P
  CR_DARKGREEN, //Q
  CR_DARKRED, //R
  CR_DARKBROWN, //S
  CR_PURPLE, //T
  CR_DARKGRAY, //U
  CR_CYAN, //V
  CR_ICE, //W
  CR_FIRE, //X
  CR_SAPPHIRE, //Y
  CR_TEAL, //Z
  NUM_TEXT_COLOURS
};

class VFont;

void T_Init ();
void T_Shutdown ();

void T_SetFont (VFont *);
void T_SetAlign (halign_e, valign_e);

void T_DrawText (int, int, const VStr &, int);

void T_DrawCursor ();
void T_DrawCursorAt (int, int);

extern VFont *SmallFont;
extern VFont *ConFont;
