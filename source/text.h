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

// horisontal alignement
enum halign_e {
  hleft,   //Left
  hcenter, //Centered
  hright,  //Right
};

// vertical alignement
enum valign_e {
  vtop,    //Top
  vcenter, //Center
  vbottom, //Bottom
};

// text colors, these must match the constants used in ACS
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
  NUM_TEXT_COLORS
};

class VFont;

void T_Init ();
void T_Shutdown ();

void T_SetFont (VFont *);
void T_SetAlign (halign_e, valign_e);

void T_DrawText (int, int, VStr, int);
int T_TextWidth (VStr);
int T_TextHeight (VStr);
int T_StringWidth (VStr);
int T_FontHeight ();

int T_CursorWidth ();
void T_DrawCursor ();
void T_DrawCursorAt (int, int);
void T_SetCursorPos (int cx, int cy);
int T_GetCursorX ();
int T_GetCursorY ();

extern VFont *SmallFont;
extern VFont *ConFont;


// ////////////////////////////////////////////////////////////////////////// //
// fancyprogress bar and OSD

bool R_IsDrawerInited ();

// reset progress bar, setup initial timing and so on
// returns `false` if graphics is not initialized
bool R_PBarReset (bool sendKeepalives=true);
// this doesn't send keepalives
VVA_OKUNUSED inline bool RNet_PBarReset () { return R_PBarReset(false); }

// update progress bar, return `true` if something was displayed.
// it is safe to call this even if graphics is not initialized.
// without graphics, it will print occasionally console messages.
// you can call this as often as you want, it will take care of
// limiting output to reasonable amounts.
// `cur` must be zero or positive, `max` must be positive
bool R_PBarUpdate (const char *message, int cur, int max, bool forced=false, bool sendKeepalives=true);
// this doesn't send keepalives
VVA_OKUNUSED inline bool RNet_PBarUpdate (const char *message, int cur, int max, bool forced=false) { return R_PBarUpdate(message, cur, max, forced, false); }


// on-screen messages type
enum {
  OSD_MapLoading,
  OSD_Network,
};

// iniit on-screen messages system
void R_OSDMsgReset (int type, bool sendKeepalives=true);
VVA_OKUNUSED inline void RNet_OSDMsgReset (int type) { R_OSDMsgReset(type, false); }

// show loader message
void R_OSDMsgShow (const char *msg, int clr=-666, bool sendKeepalives=true);
VVA_OKUNUSED inline void RNet_OSDMsgShow (const char *msg, int clr=-666) { R_OSDMsgShow(msg, clr, false); }

extern int R_OSDMsgColorMain;
extern int R_OSDMsgColorSecondary;

static inline VVA_OKUNUSED void R_OSDMsgShowMain (const char *msg) { R_OSDMsgShow(msg, R_OSDMsgColorMain); }
static inline VVA_OKUNUSED void R_OSDMsgShowSecondary (const char *msg) { R_OSDMsgShow(msg, R_OSDMsgColorSecondary); }
