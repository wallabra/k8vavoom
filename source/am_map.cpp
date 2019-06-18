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
//**  The automap code
//**
//**************************************************************************
#include "gamedefs.h"
#include "cl_local.h"
#include "drawer.h"
#ifdef CLIENT
# include "neoui/neoui.h"
#endif

// there is no need to do this anymore: OpenGL will do it for us
//#define AM_DO_CLIPPING


// player radius for movement checking
#define PLAYERRADIUS   (16.0f)
#define MAPBLOCKUNITS  (128)

/*
#define AM_W  (640)
#define AM_H  (480-sb_height)
*/

// scale on entry
#define INITSCALEMTOF  (0.2f)

#define AMSTR_FOLLOWON   "Follow Mode ON"
#define AMSTR_FOLLOWOFF  "Follow Mode OFF"

#define AMSTR_GRIDON   "Grid ON"
#define AMSTR_GRIDOFF  "Grid OFF"

#define AMSTR_MARKEDSPOT    "Marked Spot"
#define AMSTR_MARKSCLEARED  "All Marks Cleared"

//#define AM_STARTKEY      K_TAB
#define AM_PANUPKEY      K_UPARROW
#define AM_PANDOWNKEY    K_DOWNARROW
#define AM_PANLEFTKEY    K_LEFTARROW
#define AM_PANRIGHTKEY   K_RIGHTARROW
#define AM_ZOOMINKEY     '='
#define AM_ZOOMOUTKEY    '-'
//#define AM_ENDKEY        K_TAB
#define AM_GOBIGKEY      '0'
#define AM_FOLLOWKEY     'h'
#define AM_GRIDKEY       'g'
#define AM_MARKKEY       'm'
#define AM_CLEARMARKKEY  'c'

// how much the automap moves window per tic in frame-buffer coordinates
// moves 140 pixels in 1 second
#define F_PANINC  (4)
// how much zoom-in per tic
// goes to 2x in 1 second
#define M_ZOOMIN  (1.02f)
// how much zoom-out per tic
// pulls out to 0.5x in 1 second
#define M_ZOOMOUT  (1.0f/1.02f)

#define AM_NUMMARKPOINTS  (10)

// translates between frame-buffer and map distances
#define FTOM(x)   ((float)(x)*scale_ftom)
#define MTOF(x)   ((int)((x)*scale_mtof))
#define MTOFF(x)  ((x)*scale_mtof)
// translates between frame-buffer and map coordinates
#define CXMTOF(x)   (MTOF((x)-m_x)-f_x)
#define CYMTOF(y)   (f_h-MTOF((y)-m_y)+f_y)
#define CXMTOFF(x)  (MTOFF((x)-m_x)-f_x)
#define CYMTOFF(y)  (f_h-MTOFF((y)-m_y)+f_y)

// the following is crap
#define LINE_NEVERSEE  ML_DONTDRAW

#define NUMALIAS  (3) // number of antialiased lines

#define FRACBITS  (16)
#define FRACUNIT  (1<<FRACBITS)

#define FL(x)  ((float)(x)/(float)FRACUNIT)
#define FX(x)  (fixed_t)((x)*FRACUNIT)


typedef int fixed_t;

struct fpoint_t {
  int x;
  int y;
};

struct fline_t {
  fpoint_t a;
  fpoint_t b;
};

struct mpoint_t {
  float x;
  float y;
};

struct mline_t {
  mpoint_t a;
  mpoint_t b;
};


int automapactive = 0;
static VCvarB am_active("am_active", false, "Is automap active?", 0);
extern VCvarI screen_size;

VCvarB am_always_update("am_always_update", true, "Update non-overlay automap?", CVAR_Archive);


/*
#define AM_W  (640)
#define AM_H  (480-sb_height)
*/
static inline int getAMWidth () {
  if (VirtualWidth < 320) return 320;
  return VirtualWidth;
}

static inline int getAMHeight () {
  int res = VirtualWidth;
  if (res < 240) res = 240;
  if (screen_size < 11) res -= sb_height;
  return res;
}


static VCvarB am_overlay("am_overlay", true, "Show automap in overlay mode?", CVAR_Archive);
static VCvarB am_full_lines("am_full_lines", false, "Render full line even if only some parts of it were seen?", CVAR_Archive);

// automap colors
static vuint32 WallColor;
static vuint32 TSWallColor;
static vuint32 FDWallColor;
static vuint32 CDWallColor;
static vuint32 EXWallColor;
static vuint32 SecretWallColor;
static vuint32 PowerWallColor;
static vuint32 GridColor;
static vuint32 ThingColor;
static vuint32 PlayerColor;
static vuint32 MinisegColor;

static VCvarS am_color_wall("am_color_wall", "d0 b0 85", "Automap color: normal walls.", CVAR_Archive);
static VCvarS am_color_tswall("am_color_tswall", "61 64 5f", "Automap color: same-height two-sided walls.", CVAR_Archive);
static VCvarS am_color_fdwall("am_color_fdwall", "a0 6c 40", "Automap color: floor level change.", CVAR_Archive);
static VCvarS am_color_cdwall("am_color_cdwall", "94 94 ac", "Automap color: ceiling level change.", CVAR_Archive);
static VCvarS am_color_exwall("am_color_exwall", "7b 4b 27", "Automap color: walls with extra floors.", CVAR_Archive);
static VCvarS am_color_secretwall("am_color_secretwall", "ff 7f 00", "Automap color: secret walls.", CVAR_Archive);
//static VCvarS am_color_power("am_color_power", "7d 83 79", "Automap color: autorevealed walls.", CVAR_Archive);
static VCvarS am_color_power("am_color_power", "2f 4f 9f", "Automap color: autorevealed walls.", CVAR_Archive);
static VCvarS am_color_grid("am_color_grid", "4d 9d 42", "Automap color: grid.", CVAR_Archive);
static VCvarS am_color_thing("am_color_thing", "cf 4f 00", "Automap color: thing.", CVAR_Archive);
static VCvarS am_color_player("am_color_player", "e6 e6 e6", "Automap color: player.", CVAR_Archive);
static VCvarS am_color_miniseg("am_color_miniseg", "7f 00 7f", "Automap color: minisegs.", CVAR_Archive);

static VCvarI am_player_arrow("am_player_arrow", 0, "Type of player arrow.", CVAR_Archive);
static VCvarB am_follow_player("am_follow_player", true, "Should automap follow player?", CVAR_Archive);
static VCvarB am_rotate("am_rotate", false, "Should automap rotate?", CVAR_Archive);
static VCvarB am_show_stats("am_show_stats", false, "Show stats on automap?", CVAR_Archive);

static VCvarI am_cheating("am_cheating", "0", "Oops! Automap cheats!", CVAR_Cheat);
static VCvarB am_show_secrets("am_show_secrets", false, "Show secret walls on automap!", CVAR_Cheat);
static VCvarB am_show_minisegs("am_show_minisegs", false, "Show minisegs on automap (cheating should be turned on).", CVAR_Cheat);
static VCvarB am_show_static_lights("am_show_static_lights", false, "Show static lights on automap (cheating should be turned on).", CVAR_Cheat);
static VCvarB am_show_dynamic_lights("am_show_dynamic_lights", false, "Show static lights on automap (cheating should be turned on).", CVAR_Cheat);
static VCvarB am_show_rendered_nodes("am_show_rendered_nodes", false, "Show rendered BSP nodes on automap (cheating should be turned on).", CVAR_Cheat);
static VCvarB am_show_rendered_subs("am_show_rendered_subs", false, "Show rendered subsectors on automap (cheating should be turned on).", CVAR_Cheat);

static VCvarF am_overlay_alpha("am_overlay_alpha", "0.4", "Automap overlay alpha", CVAR_Archive);
static VCvarB am_show_parchment("am_show_parchment", true, "Show automap parchment?", CVAR_Archive);

static VCvarB am_default_whole("am_default_whole", true, "Default scale is \"show all\"?", CVAR_Archive);

static int grid = 0;

static int leveljuststarted = 1; // kluge until AM_LevelInit() is called
static int amWholeScale = -1; // -1: unknown

// location of window on screen
static int f_x;
static int f_y;

// size of window on screen
static int f_w;
static int f_h;

static mpoint_t m_paninc; // how far the window pans each tic (map coords)
static float mtof_zoommul; // how far the window zooms in each tic (map coords)
static float ftom_zoommul; // how far the window zooms in each tic (fb coords)

static float m_x; // LL x,y where the window is on the map (map coords)
static float m_y;
static float m_x2; // UR x,y where the window is on the map (map coords)
static float m_y2;

// width/height of window on map (map coords)
static float m_w;
static float m_h;

// based on level size
static float min_x;
static float min_y;
static float max_x;
static float max_y;

static float max_w; // max_x-min_x,
static float max_h; // max_y-min_y

// based on player size
static float min_w;
static float min_h;


static float min_scale_mtof; // used to tell when to stop zooming out
static float max_scale_mtof; // used to tell when to stop zooming in

// old stuff for recovery later
static float old_m_x;
static float old_m_y;
static float old_m_w;
static float old_m_h;

// old location used by the Follower routine
static mpoint_t f_oldloc;

// used by MTOF to scale from map-to-frame-buffer coords
static float scale_mtof = INITSCALEMTOF;
// used by FTOM to scale from frame-buffer-to-map coords (=1/scale_mtof)
static float scale_ftom;

static float start_scale_mtof = INITSCALEMTOF;

static mpoint_t oldplr;

static bool use_marks = false;
static int marknums[10]; // numbers used for marking by the automap
static mpoint_t markpoints[AM_NUMMARKPOINTS]; // where the points are
static int markpointnum = 0; // next point to be assigned

static int mappic = 0;
static int mapheight = 64;
static short mapystart = 0; // y-value for the start of the map bitmap...used in the paralax stuff.
static short mapxstart = 0; //x-value for the bitmap.

static bool stopped = true;

static VName lastmap;


//  The vector graphics for the automap.

//  A line drawing of the player pointing right, starting from the middle.
#define R  (8.0f*PLAYERRADIUS/7.0f)
static const mline_t player_arrow1[] = {
  { { -R+R/8.0f, 0.0f }, { R, 0.0f } }, // -----
  { { R, 0.0f }, { R-R/2.0f, R/4.0f } },  // ----->
  { { R, 0.0f }, { R-R/2.0f, -R/4.0f } },
  { { -R+R/8.0f, 0.0f }, { -R-R/8.0f, R/4.0f } }, // >---->
  { { -R+R/8.0f, 0.0f }, { -R-R/8.0f, -R/4.0f } },
  { { -R+3.0f*R/8.0f, 0.0f }, { -R+R/8.0f, R/4.0f } }, // >>--->
  { { -R+3.0f*R/8.0f, 0.0f }, { -R+R/8.0f, -R/4.0f } }
};
#define NUMPLYRLINES1  (sizeof(player_arrow1)/sizeof(mline_t))


static const mline_t player_arrow2[] = {
  { { -R+R/4.0f, 0.0f }, { 0.0f, 0.0f} }, // centre line.
  { { -R+R/4.0f, R/8.0f }, { R, 0.0f} }, // blade
  { { -R+R/4.0f, -R/8.0f }, { R, 0.0f } },
  { { -R+R/4.0f, -R/4.0f }, { -R+R/4.0f, R/4.0f } }, // crosspiece
  { { -R+R/8.0f, -R/4.0f }, { -R+R/8.0f, R/4.0f } },
  { { -R+R/8.0f, -R/4.0f }, { -R+R/4.0f, -R/4.0f } }, //crosspiece connectors
  { { -R+R/8.0f, R/4.0f }, { -R+R/4.0f, R/4.0f } },
  { { -R-R/4.0f, R/8.0f }, { -R-R/4.0f, -R/8.0f } }, //pommel
  { { -R-R/4.0f, R/8.0f }, { -R+R/8.0f, R/8.0f } },
  { { -R-R/4.0f, -R/8 }, { -R+R/8.0f, -R/8.0f } }
};
#define NUMPLYRLINES2  (sizeof(player_arrow2)/sizeof(mline_t))


static const mline_t player_arrow_ddt[] = {
  { { -R+R/8, 0 }, { R, 0 } }, // -----
  { { R, 0 }, { R-R/2, R/6 } },  // ----->
  { { R, 0 }, { R-R/2, -R/6 } },
  { { -R+R/8, 0 }, { -R-R/8, R/6 } }, // >----->
  { { -R+R/8, 0 }, { -R-R/8, -R/6 } },
  { { -R+3*R/8, 0 }, { -R+R/8, R/6 } }, // >>----->
  { { -R+3*R/8, 0 }, { -R+R/8, -R/6 } },
  { { -R/2, 0 }, { -R/2, -R/6 } }, // >>-d--->
  { { -R/2, -R/6 }, { -R/2+R/6, -R/6 } },
  { { -R/2+R/6, -R/6 }, { -R/2+R/6, R/4 } },
  { { -R/6, 0 }, { -R/6, -R/6 } }, // >>-dd-->
  { { -R/6, -R/6 }, { 0, -R/6 } },
  { { 0, -R/6 }, { 0, R/4 } },
  { { R/6, R/4 }, { R/6, -R/7 } }, // >>-ddt->
  { { R/6, -R/7 }, { R/6+R/32, -R/7-R/32 } },
  { { R/6+R/32, -R/7-R/32 }, { R/6+R/10, -R/7 } }
};
#define NUMPLYRLINES3  (sizeof(player_arrow_ddt)/sizeof(mline_t))

#undef R

#define R (1.0f)
static const mline_t thintriangle_guy[] =
{
  { { -0.5f*R, -0.7f*R }, { R, 0.0f } },
  { { R, 0.0f }, { -0.5f*R, 0.7f*R } },
  { { -0.5f*R, 0.7f*R }, { -0.5f*R, -0.7f*R } }
};
#undef R
#define NUMTHINTRIANGLEGUYLINES  (sizeof(thintriangle_guy)/sizeof(mline_t))


//==========================================================================
//
//  AM_Init
//
//==========================================================================
void AM_Init () {
}


//==========================================================================
//
//  AM_activateNewScale
//
//==========================================================================
static void AM_activateNewScale () {
  m_x += m_w/2.0f;
  m_y += m_h/2.0f;
  m_w = FTOM(f_w);
  m_h = FTOM(f_h);
  m_x -= m_w/2.0f;
  m_y -= m_h/2.0f;
  m_x2 = m_x+m_w;
  m_y2 = m_y+m_h;
}


//==========================================================================
//
//  AM_saveScaleAndLoc
//
//==========================================================================
static void AM_saveScaleAndLoc () {
  old_m_x = m_x;
  old_m_y = m_y;
  old_m_w = m_w;
  old_m_h = m_h;
}


//==========================================================================
//
//  AM_restoreScaleAndLoc
//
//==========================================================================
static void AM_restoreScaleAndLoc () {
  m_w = old_m_w;
  m_h = old_m_h;
  if (!am_follow_player) {
    m_x = old_m_x;
    m_y = old_m_y;
  } else {
    m_x = cl->ViewOrg.x-m_w/2.0f;
    m_y = cl->ViewOrg.y-m_h/2.0f;
  }
  m_x2 = m_x+m_w;
  m_y2 = m_y+m_h;

  // change the scaling multipliers
  scale_mtof = (float)f_w/m_w;
  scale_ftom = 1.0f/scale_mtof;
}


//==========================================================================
//
//  AM_minOutWindowScale
//
//  set the window scale to the maximum size
//
//==========================================================================
static void AM_minOutWindowScale () {
  scale_mtof = min_scale_mtof;
  scale_ftom = 1.0f/scale_mtof;
  AM_activateNewScale();
}


//==========================================================================
//
//  AM_maxOutWindowScale
//
//  set the window scale to the minimum size
//
//==========================================================================
static void AM_maxOutWindowScale () {
  scale_mtof = max_scale_mtof;
  scale_ftom = 1.0f/scale_mtof;
  AM_activateNewScale();
}


//==========================================================================
//
//  AM_findMinMaxBoundaries
//
//  Determines bounding box of all vertices, sets global variables
//  controlling zoom range.
//
//==========================================================================
static void AM_findMinMaxBoundaries () {
  min_x = min_y =  99999.0f;
  max_x = max_y = -99999.0f;

  for (int i = 0; i < GClLevel->NumVertexes; ++i) {
         if (GClLevel->Vertexes[i].x < min_x) min_x = GClLevel->Vertexes[i].x;
    else if (GClLevel->Vertexes[i].x > max_x) max_x = GClLevel->Vertexes[i].x;

         if (GClLevel->Vertexes[i].y < min_y) min_y = GClLevel->Vertexes[i].y;
    else if (GClLevel->Vertexes[i].y > max_y) max_y = GClLevel->Vertexes[i].y;
  }

  max_w = max_x-min_x;
  max_h = max_y-min_y;

  min_w = 2.0f*PLAYERRADIUS; // const? never changed?
  min_h = 2.0f*PLAYERRADIUS;

  float a = (float)f_w/max_w;
  float b = (float)f_h/max_h;

  min_scale_mtof = (a < b ? a : b);
  max_scale_mtof = (float)f_h/(2.0f*PLAYERRADIUS);
}


//==========================================================================
//
//  AM_ScrollParchment
//
//==========================================================================
static void AM_ScrollParchment (float dmapx, float dmapy) {
  mapxstart -= (short)(dmapx*scale_mtof/12)>>12;
  mapystart -= (short)(dmapy*scale_mtof/12)>>12;

  if (mappic > 0) {
    int pwidth = (int)GTextureManager.TextureWidth(mappic); //320;
    int pheight = (int)GTextureManager.TextureHeight(mappic);
    if (pwidth < 1) pwidth = 1;
    if (pheight < 1) pheight = 1;

    while (mapxstart > 0) mapxstart -= pwidth;
    while (mapxstart <= -pwidth) mapxstart += pwidth;
    while (mapystart > 0) mapystart -= pheight;
    while (mapystart <= -pheight) mapystart += pheight;
  }
}


//==========================================================================
//
//  AM_changeWindowLoc
//
//==========================================================================
static void AM_changeWindowLoc () {
  if (m_paninc.x || m_paninc.y) {
    am_follow_player = 0;
    f_oldloc.x = 99999.0f;
  }

  float oldmx = m_x, oldmy = m_y;

  m_x += m_paninc.x;
  m_y += m_paninc.y;

       if (m_x+m_w/2.0f > max_x) m_x = max_x-m_w/2.0f;
  else if (m_x+m_w/2.0f < min_x) m_x = min_x-m_w/2.0f;

       if (m_y+m_h/2.0f > max_y) m_y = max_y-m_h/2.0f;
  else if (m_y+m_h/2.0f < min_y) m_y = min_y-m_h/2.0f;

  m_x2 = m_x+m_w;
  m_y2 = m_y+m_h;

  AM_ScrollParchment (m_x-oldmx, oldmy-m_y);
}


//==========================================================================
//
//  AM_addMark
//
//  adds a marker at the current location
//
//==========================================================================
static bool AM_addMark () {
  if (marknums[0] != -1) {
    markpoints[markpointnum].x = m_x+m_w/2.0f;
    markpoints[markpointnum].y = m_y+m_h/2.0f;
    markpointnum = (markpointnum+1)%AM_NUMMARKPOINTS;
    return true;
  }
  return false;
}


//==========================================================================
//
//  AM_clearMarks
//
//==========================================================================
static bool AM_clearMarks () {
  for (int i = AM_NUMMARKPOINTS-1; i >= 0; --i) markpoints[i].x = -1.0f; //FIXME: means empty
  markpointnum = 0;
  return marknums[0] != -1;
}


//==========================================================================
//
//  AM_initVariables
//
//==========================================================================
static void AM_initVariables () {
  automapactive = (am_overlay ? -1 : 1);

  f_oldloc.x = 99999.0f;

  m_paninc.x = m_paninc.y = 0.0f;
  ftom_zoommul = 1.0f;
  mtof_zoommul = 1.0f;

  m_w = FTOM(f_w);
  m_h = FTOM(f_h);

  oldplr.x = cl->ViewOrg.x;
  oldplr.y = cl->ViewOrg.y;
  m_x = cl->ViewOrg.x-m_w/2.0f;
  m_y = cl->ViewOrg.y-m_h/2.0f;
  AM_changeWindowLoc();

  // for saving & restoring
  old_m_x = m_x;
  old_m_y = m_y;
  old_m_w = m_w;
  old_m_h = m_h;
}


//==========================================================================
//
//  AM_loadPics
//
//==========================================================================
static void AM_loadPics () {
  if (W_CheckNumForName(NAME_ammnum0) >= 0) {
    for (int i = 0; i < 10; ++i) {
      marknums[i] = GTextureManager.AddPatch(va("ammnum%d", i), TEXTYPE_Pic, true); // silent
    }
    use_marks = true;
  }
  mappic = GTextureManager.AddPatch(NAME_autopage, TEXTYPE_Autopage, true); // silent
  mapheight = (mappic > 0 ? (int)GTextureManager.TextureHeight(mappic) : 64);
  if (mapheight < 1) mapheight = 1;
}


//==========================================================================
//
//  AM_LevelInit
//
//  should be called at the start of every level
//  right now, i figure it out myself
//
//==========================================================================
static void AM_LevelInit () {
  leveljuststarted = 0;
  amWholeScale = -1;

  f_x = f_y = 0;
  f_w = ScreenWidth;
  f_h = ScreenHeight-(screen_size < 11 ? SB_RealHeight() : 0);

  AM_clearMarks();
  mapxstart = mapystart = 0;

  AM_findMinMaxBoundaries();
  scale_mtof = min_scale_mtof/0.7f;
  if (scale_mtof > max_scale_mtof) scale_mtof = min_scale_mtof;
  scale_ftom = 1.0f/scale_mtof;
  start_scale_mtof = scale_mtof;
}


//==========================================================================
//
//  AM_Stop
//
//==========================================================================
void AM_Stop () {
  automapactive = 0;
  stopped = true;
  am_active = false;
}


//==========================================================================
//
//  AM_Start
//
//==========================================================================
static void AM_Start () {
  //if (!stopped) AM_Stop();
  stopped = false;
  if (lastmap != GClLevel->MapName) {
    AM_LevelInit();
    lastmap = GClLevel->MapName;
  }
  AM_initVariables(); // this sets `automapactive`
  AM_loadPics();
  if (amWholeScale < 0) {
    amWholeScale = (am_default_whole ? 1 : 0);
    if (amWholeScale) {
      AM_saveScaleAndLoc();
      AM_minOutWindowScale();
    }
  }
  mtof_zoommul = 1.0f;
  ftom_zoommul = 1.0f;
  am_active = true;
}


//==========================================================================
//
//  AM_Check
//
//==========================================================================
static void AM_Check () {
  if (am_active != !!automapactive) {
    if (am_active) AM_Start(); else AM_Stop();
  }
  if (automapactive) automapactive = (am_overlay ? -1 : 1);
}


//==========================================================================
//
//  AM_Responder
//
//  Handle events (user inputs) in automap mode
//
//==========================================================================
bool AM_Responder (event_t *ev) {
  AM_Check();
  if (!automapactive) return false;
  bool rc = false;
  if (ev->type == ev_keydown) {
    rc = true;
    switch (ev->data1) {
      case AM_PANRIGHTKEY: // pan right
        if (!am_follow_player) m_paninc.x = FTOM(F_PANINC/2.0f); else rc = false;
        break;
      case AM_PANLEFTKEY: // pan left
        if (!am_follow_player) m_paninc.x = -FTOM(F_PANINC/2.0f); else rc = false;
        break;
      case AM_PANUPKEY: // pan up
        if (!am_follow_player) m_paninc.y = FTOM(F_PANINC/2.0f); else rc = false;
        break;
      case AM_PANDOWNKEY: // pan down
        if (!am_follow_player) m_paninc.y = -FTOM(F_PANINC/2.0f); else rc = false;
        break;
      case AM_ZOOMOUTKEY: // zoom out
        if (!amWholeScale) {
          mtof_zoommul = M_ZOOMOUT;
          ftom_zoommul = M_ZOOMIN;
        }
        break;
      case AM_ZOOMINKEY: // zoom in
        if (!amWholeScale) {
          mtof_zoommul = M_ZOOMIN;
          ftom_zoommul = M_ZOOMOUT;
        }
        break;
      /*
      case AM_ENDKEY:
        AM_Stop();
        break;
      */
      case AM_GOBIGKEY:
        mtof_zoommul = 1.0f;
        ftom_zoommul = 1.0f;
        amWholeScale = (amWholeScale ? 0 : 1);
        if (amWholeScale) {
          AM_saveScaleAndLoc();
          AM_minOutWindowScale();
        } else {
          AM_restoreScaleAndLoc();
        }
        break;
      case AM_FOLLOWKEY:
        am_follow_player = !am_follow_player;
        f_oldloc.x = 99999.0f;
        cl->Printf(am_follow_player ? AMSTR_FOLLOWON : AMSTR_FOLLOWOFF);
        break;
      case AM_GRIDKEY:
        grid = !grid;
        cl->Printf(grid ? AMSTR_GRIDON : AMSTR_GRIDOFF);
        break;
      case AM_MARKKEY:
        if (use_marks && AM_addMark()) cl->Printf("%s %d", AMSTR_MARKEDSPOT, markpointnum); else rc = false;
        break;
      case AM_CLEARMARKKEY:
        if (use_marks && AM_clearMarks()) cl->Printf(AMSTR_MARKSCLEARED); else rc = false;
        break;
      default:
        rc = false;
        break;
    }
  } else if (ev->type == ev_keyup) {
    rc = false;
    switch (ev->data1) {
      case AM_PANRIGHTKEY:
        if (!am_follow_player) m_paninc.x = 0.0f;
        break;
      case AM_PANLEFTKEY:
        if (!am_follow_player) m_paninc.x = 0.0f;
        break;
      case AM_PANUPKEY:
        if (!am_follow_player) m_paninc.y = 0.0f;
        break;
      case AM_PANDOWNKEY:
        if (!am_follow_player) m_paninc.y = 0.0f;
        break;
      case AM_ZOOMOUTKEY:
      case AM_ZOOMINKEY:
        mtof_zoommul = 1.0f;
        ftom_zoommul = 1.0f;
        break;
    }
  }
  return rc;
}


//==========================================================================
//
//  AM_changeWindowScale
//
//  Zooming
//
//==========================================================================
static void AM_changeWindowScale () {
  // change the scaling multipliers
  scale_mtof = scale_mtof*mtof_zoommul;
  scale_ftom = 1.0f/scale_mtof;
       if (scale_mtof < min_scale_mtof) AM_minOutWindowScale();
  else if (scale_mtof > max_scale_mtof) AM_maxOutWindowScale();
  else AM_activateNewScale();
}


//==========================================================================
//
//  AM_rotate
//
//  Rotation in 2D. Used to rotate player arrow line character.
//
//==========================================================================
static void AM_rotate (float *x, float *y, float a) {
  float tmpx = *x*mcos(a)-*y*msin(a);
  *y = *x*msin(a)+*y*mcos(a);
  *x = tmpx;
}


//==========================================================================
//
//  AM_rotatePoint
//
//==========================================================================
void AM_rotatePoint (float *x, float *y) {
  *x -= FTOM(MTOF(cl->ViewOrg.x));
  *y -= FTOM(MTOF(cl->ViewOrg.y));
  AM_rotate(x, y, 90.0f-cl->ViewAngles.yaw);
  *x += FTOM(MTOF(cl->ViewOrg.x));
  *y += FTOM(MTOF(cl->ViewOrg.y));
}


//==========================================================================
//
//  AM_doFollowPlayer
//
//==========================================================================
static void AM_doFollowPlayer () {
  if (f_oldloc.x != cl->ViewOrg.x || f_oldloc.y != cl->ViewOrg.y) {
    m_x = FTOM(MTOF(cl->ViewOrg.x))-m_w/2.0f;
    m_y = FTOM(MTOF(cl->ViewOrg.y))-m_h/2.0f;
    m_x2 = m_x+m_w;
    m_y2 = m_y+m_h;
    // do the parallax parchment scrolling
    float sx = FTOM(MTOF(cl->ViewOrg.x-f_oldloc.x));
    float sy = FTOM(MTOF(f_oldloc.y-cl->ViewOrg.y));
    if (am_rotate) AM_rotate(&sx, &sy, cl->ViewAngles.yaw-90.0f);
    AM_ScrollParchment(sx, sy);
    f_oldloc.x = cl->ViewOrg.x;
    f_oldloc.y = cl->ViewOrg.y;
  }
}


//==========================================================================
//
//  AM_Ticker
//
//  Updates on Game Tick
//
//==========================================================================
void AM_Ticker () {
  AM_Check();
  if (!automapactive) return;

  if (am_follow_player) AM_doFollowPlayer();

  // change the zoom if necessary
  if (ftom_zoommul != 1.0f) AM_changeWindowScale();

  // change x,y location
  if (m_paninc.x || m_paninc.y) AM_changeWindowLoc();
}


//==========================================================================
//
//  AM_clearFB
//
//  Clear automap frame buffer.
//
//==========================================================================
static void AM_clearFB () {
  if (am_follow_player) {
    int dmapx = MTOF(cl->ViewOrg.x)-MTOF(oldplr.x);
    int dmapy = MTOF(oldplr.y)-MTOF(cl->ViewOrg.y);

    oldplr.x = cl->ViewOrg.x;
    oldplr.y = cl->ViewOrg.y;
    mapxstart -= dmapx>>1;
    mapystart -= dmapy>>1;

    while (mapxstart >= getAMWidth()) mapxstart -= getAMWidth();
    while (mapxstart < 0) mapxstart += getAMWidth();
    while (mapystart >= mapheight) mapystart -= mapheight;
    while (mapystart < 0) mapystart += mapheight;
  } else {
    mapxstart -= MTOF(m_paninc.x)>>1;
    mapystart += MTOF(m_paninc.y)>>1;
    if (mapxstart >= getAMWidth()) mapxstart -= getAMWidth();
    if (mapxstart < 0) mapxstart += getAMWidth();
    if (mapystart >= mapheight) mapystart -= mapheight;
    if (mapystart < 0) mapystart += mapheight;
  }

  // blit the automap background to the screen
  if (automapactive > 0) {
    if (mappic > 0 && am_show_parchment) {
      for (int y = mapystart-mapheight; y < getAMHeight(); y += mapheight) {
        for (int x = mapxstart-getAMWidth(); x < getAMWidth(); x += 320) {
          R_DrawPic(x, y, mappic);
        }
      }
    } else {
      Drawer->FillRect(0, 0, ScreenWidth, ScreenHeight, 0);
    }
  }
}


#ifdef AM_DO_CLIPPING
//==========================================================================
//
//  AM_clipMline
//
//  Automap clipping of lines.
//
//  Based on Cohen-Sutherland clipping algorithm but with a slightly faster
//  reject and precalculated slopes. If the speed is needed, use a hash
//  algorithm to handle the common cases.
//
//==========================================================================
#define DOOUTCODE(oc, mx, my) \
  (oc) = 0; \
  if ((my) < 0) (oc) |= TOP; \
  else if ((my) >= f_h) (oc) |= BOTTOM; \
  if ((mx) < 0) (oc) |= LEFT; \
  else if ((mx) >= f_w) (oc) |= RIGHT;

static bool AM_clipMline (mline_t *ml, fline_t *fl) {
  enum {
    LEFT   = 1,
    RIGHT  = 2,
    BOTTOM = 4,
    TOP    = 8,
  };

  int outcode1 = 0;
  int outcode2 = 0;
  int outside;

  fpoint_t tmp = {0, 0};
  int dx, dy;

  // do trivial rejects and outcodes
  if (ml->a.y > m_y2) outcode1 = TOP; else if (ml->a.y < m_y) outcode1 = BOTTOM;
  if (ml->b.y > m_y2) outcode2 = TOP; else if (ml->b.y < m_y) outcode2 = BOTTOM;
  if (outcode1&outcode2) return false; // trivially outside

  if (ml->a.x < m_x) outcode1 |= LEFT; else if (ml->a.x > m_x2) outcode1 |= RIGHT;
  if (ml->b.x < m_x) outcode2 |= LEFT; else if (ml->b.x > m_x2) outcode2 |= RIGHT;
  if (outcode1&outcode2) return false; // trivially outside

  // transform to frame-buffer coordinates.
  fl->a.x = CXMTOF(ml->a.x);
  fl->a.y = CYMTOF(ml->a.y);
  fl->b.x = CXMTOF(ml->b.x);
  fl->b.y = CYMTOF(ml->b.y);

  DOOUTCODE(outcode1, fl->a.x, fl->a.y);
  DOOUTCODE(outcode2, fl->b.x, fl->b.y);

  if (outcode1&outcode2) return false;

  while (outcode1|outcode2) {
    // may be partially inside box
    // find an outside point
    if (outcode1) outside = outcode1; else outside = outcode2;

    // clip to each side
    if (outside&TOP) {
      dy = fl->a.y-fl->b.y;
      dx = fl->b.x-fl->a.x;
      tmp.x = fl->a.x+(dx*(fl->a.y))/dy;
      tmp.y = 0;
    } else if (outside&BOTTOM) {
      dy = fl->a.y-fl->b.y;
      dx = fl->b.x-fl->a.x;
      tmp.x = fl->a.x+(dx*(fl->a.y-f_h))/dy;
      tmp.y = f_h-1;
    } else if (outside&RIGHT) {
      dy = fl->b.y-fl->a.y;
      dx = fl->b.x-fl->a.x;
      tmp.y = fl->a.y+(dy*(f_w-1-fl->a.x))/dx;
      tmp.x = f_w-1;
    } else if (outside&LEFT) {
      dy = fl->b.y-fl->a.y;
      dx = fl->b.x-fl->a.x;
      tmp.y = fl->a.y+(dy*(-fl->a.x))/dx;
      tmp.x = 0;
    }

    if (outside == outcode1) {
      fl->a = tmp;
      DOOUTCODE(outcode1, fl->a.x, fl->a.y);
    } else {
      fl->b = tmp;
      DOOUTCODE(outcode2, fl->b.x, fl->b.y);
    }

    if (outcode1&outcode2) return false; // trivially outside
  }

  return true;
}
#undef DOOUTCODE


//==========================================================================
//
//  AM_drawFline
//
//  Classic Bresenham w/ whatever optimizations needed for speed
//
//==========================================================================
static void AM_drawFline (fline_t *fl, vuint32 color) {
  Drawer->DrawLine(fl->a.x, fl->a.y, color, fl->b.x, fl->b.y, color);
}
#endif


//==========================================================================
//
//  AM_drawMline
//
//  Clip lines, draw visible part sof lines.
//
//==========================================================================
static void AM_drawMline (mline_t *ml, vuint32 color) {
#ifdef AM_DO_CLIPPING
  fline_t fl;
  if (AM_clipMline(ml, &fl)) AM_drawFline(&fl, color); // draws it on frame buffer using fb coords
#else
  Drawer->DrawLine(CXMTOFF(ml->a.x), CYMTOFF(ml->a.y), color, CXMTOFF(ml->b.x), CYMTOFF(ml->b.y), color);
#endif
}


//==========================================================================
//
//  AM_drawGrid
//
//  Draws flat (floor/ceiling tile) aligned grid lines.
//
//==========================================================================
static void AM_drawGrid (vuint32 color) {
  float x, y;
  float start, end;
  mline_t ml;
  float minlen, extx, exty;
  float minx, miny;

  // calculate a minimum for how long the grid lines should be, so they
  // cover the screen at any rotation
  minlen = sqrtf (m_w*m_w+m_h*m_h);
  extx = (minlen-m_w)/2;
  exty = (minlen-m_h)/2;

  minx = m_x;
  miny = m_y;

  // figure out start of vertical gridlines
  start = m_x-extx;
  if ((FX(start-GClLevel->BlockMapOrgX))%(MAPBLOCKUNITS<<FRACBITS)) {
    start += FL((MAPBLOCKUNITS<<FRACBITS)-((FX(start-GClLevel->BlockMapOrgX))%(MAPBLOCKUNITS<<FRACBITS)));
  }
  end = minx+minlen-extx;

  // draw vertical gridlines
  for (x = start; x < end; x += (float)MAPBLOCKUNITS) {
    ml.a.x = x;
    ml.b.x = x;
    ml.a.y = miny-exty;
    ml.b.y = ml.a.y+minlen;
    if (am_rotate) {
      AM_rotatePoint(&ml.a.x, &ml.a.y);
      AM_rotatePoint(&ml.b.x, &ml.b.y);
    }
    AM_drawMline(&ml, color);
  }

  // figure out start of horizontal gridlines
  start = m_y-exty;
  if ((FX(start-GClLevel->BlockMapOrgY))%(MAPBLOCKUNITS<<FRACBITS)) {
    start += FL((MAPBLOCKUNITS<<FRACBITS)-((FX(start-GClLevel->BlockMapOrgY))%(MAPBLOCKUNITS<<FRACBITS)));
  }
  end = miny+minlen-exty;

  // draw horizontal gridlines
  for (y = start; y < end; y += (float)MAPBLOCKUNITS) {
    ml.a.x = minx-extx;
    ml.b.x = ml.a.x+minlen;
    ml.a.y = y;
    ml.b.y = y;
    if (am_rotate) {
      AM_rotatePoint(&ml.a.x, &ml.a.y);
      AM_rotatePoint(&ml.b.x, &ml.b.y);
    }
    AM_drawMline(&ml, color);
  }
}


//==========================================================================
//
//  AM_getLineColor
//
//==========================================================================
static vuint32 AM_getLineColor (const line_t *line, bool *cheatOnly) {
  *cheatOnly = false;
  // locked door
  if (line->special == LNSPEC_DoorLockedRaise && GetLockDef(line->arg4) && GetLockDef(line->arg4)->MapColor) {
    return GetLockDef(line->arg4)->MapColor;
  }
  // locked ACS special
  if ((line->special == LNSPEC_ACSLockedExecute || line->special == LNSPEC_ACSLockedExecuteDoor) &&
      GetLockDef(line->arg5) && GetLockDef(line->arg5)->MapColor)
  {
    return GetLockDef(line->arg5)->MapColor;
  }
  // unseen automap walls
  if (!am_cheating && !(line->flags&ML_MAPPED) && !(line->exFlags&ML_EX_PARTIALLY_MAPPED) &&
      (cl->PlayerFlags&VBasePlayer::PF_AutomapRevealed))
  {
    return PowerWallColor;
  }
  // normal wall
  if (!line->backsector) {
    return WallColor;
  }
  // secret door
  if (line->flags&ML_SECRET) {
    return (am_cheating || am_show_secrets ? SecretWallColor : WallColor);
  }
  // floor level change
  if (line->backsector->floor.minz != line->frontsector->floor.minz) {
    return FDWallColor;
  }
  // ceiling level change
  if (line->backsector->ceiling.maxz != line->frontsector->ceiling.maxz) {
    return CDWallColor;
  }
  // show extra floors
  if (line->backsector->SectorFlags&sector_t::SF_HasExtrafloors ||
      line->frontsector->SectorFlags&sector_t::SF_HasExtrafloors)
  {
    return EXWallColor;
  }
  // something other
  *cheatOnly = true;
  return TSWallColor;
}


//==========================================================================
//
//  AM_drawWalls
//
//  Determines visible lines, draws them.
//
//==========================================================================
static void AM_drawWalls () {
  line_t *line = &GClLevel->Lines[0];
  for (unsigned i = GClLevel->NumLines; i--; ++line) {
    if (!am_cheating) {
      if (line->flags&LINE_NEVERSEE) continue;
      if (!(line->flags&ML_MAPPED) && !(line->exFlags&ML_EX_PARTIALLY_MAPPED)) {
        if (!(cl->PlayerFlags&VBasePlayer::PF_AutomapRevealed)) continue;
      }
    }

    bool cheatOnly = false;
    vuint32 clr = AM_getLineColor(line, &cheatOnly);
    if (cheatOnly && !am_cheating) continue; //FIXME: should we draw this lines if automap powerup is active?

    // check if we need to re-evaluate line visibility, and do it
    if (line->exFlags&ML_EX_CHECK_MAPPED) {
      line->exFlags &= ~(ML_EX_PARTIALLY_MAPPED|ML_EX_CHECK_MAPPED);
      if (!(line->flags&ML_MAPPED)) {
        int unseenSides[2] = {0, 0};
        int seenSides[2] = {0, 0};
        do {
          int defSide;
               if (line->sidenum[0] >= 0) defSide = (line->sidenum[1] >= 0 ? -1 : 0);
          else if (line->sidenum[1] >= 0) defSide = 1;
          else break;
          for (const seg_t *seg = line->firstseg; seg; seg = seg->lsnext) {
            int side = defSide;
            if (side < 0) side = (int)(seg->sidedef == &GClLevel->Sides[line->sidenum[1]]);
            if (seg->flags&SF_MAPPED) ++seenSides[side]; else ++unseenSides[side];
          }
        } while (0);
        // if any line side is fully seen, this line is fully mapped
        // that is, we should have some seen segs, and no unseen segs for a side to consider it "fully seen"
        if ((unseenSides[0] == 0 && seenSides[0] != 0) || (unseenSides[1] == 0 && seenSides[1] != 0)) {
          // fully mapped
          line->flags |= ML_MAPPED;
        } else if (seenSides[0]|seenSides[1]) { // not a typo!
          // partially mapped, because some segs were seen
          line->exFlags |= ML_EX_PARTIALLY_MAPPED;
        }
      }
    }

    // just in case
    if (line->flags&ML_MAPPED) line->exFlags &= ~(ML_EX_PARTIALLY_MAPPED|ML_EX_CHECK_MAPPED);

    // fully mapped or automap revealed?
    if (am_full_lines || am_cheating || (line->flags&ML_MAPPED) || (cl->PlayerFlags&VBasePlayer::PF_AutomapRevealed)) {
      mline_t l;
      l.a.x = line->v1->x;
      l.a.y = line->v1->y;
      l.b.x = line->v2->x;
      l.b.y = line->v2->y;

      if (am_rotate) {
        AM_rotatePoint(&l.a.x, &l.a.y);
        AM_rotatePoint(&l.b.x, &l.b.y);
      }

      AM_drawMline(&l, clr);
    } else {
      // render segments
      for (const seg_t *seg = line->firstseg; seg; seg = seg->lsnext) {
        if (!(seg->flags&SF_MAPPED)) continue;

        mline_t l;
        l.a.x = seg->v1->x;
        l.a.y = seg->v1->y;
        l.b.x = seg->v2->x;
        l.b.y = seg->v2->y;

        if (am_rotate) {
          AM_rotatePoint(&l.a.x, &l.a.y);
          AM_rotatePoint(&l.b.x, &l.b.y);
        }

        AM_drawMline(&l, clr);
      }
    }
  }
}


//==========================================================================
//
//  AM_DrawSimpleLine
//
//==========================================================================
static void AM_DrawSimpleLine (float x0, float y0, float x1, float y1, vuint32 color) {
  mline_t l;
  l.a.x = x0;
  l.a.y = y0;
  l.b.x = x1;
  l.b.y = y1;
  if (am_rotate) {
    AM_rotatePoint(&l.a.x, &l.a.y);
    AM_rotatePoint(&l.b.x, &l.b.y);
  }
  AM_drawMline(&l, color);
}


//==========================================================================
//
//  AM_DrawBox
//
//==========================================================================
static void AM_DrawBox (float x0, float y0, float x1, float y1, vuint32 color) {
  AM_DrawSimpleLine(x0, y0, x1, y0, color);
  AM_DrawSimpleLine(x1, y0, x1, y1, color);
  AM_DrawSimpleLine(x1, y1, x0, y1, color);
  AM_DrawSimpleLine(x0, y1, x0, y0, color);
}


//==========================================================================
//
//  AM_DrawMinisegs
//
//==========================================================================
static void AM_DrawMinisegs () {
  const seg_t *seg = &GClLevel->Segs[0];
  for (unsigned i = GClLevel->NumSegs; i--; ++seg) {
    if (seg->linedef) continue; // not a miniseg
    if (seg->front_sub->sector->linecount == 0) continue; // original polyobj sector
    AM_DrawSimpleLine(seg->v1->x, seg->v1->y, seg->v2->x, seg->v2->y, MinisegColor);
  }
}


//==========================================================================
//
//  AM_DrawRenderedNodes
//
//==========================================================================
static void AM_DrawRenderedNodes () {
  const node_t *node = &GClLevel->Nodes[0];
  for (unsigned i = GClLevel->NumNodes; i--; ++node) {
    if (!Drawer->RendLev->IsNodeRendered(node)) continue;
    AM_DrawBox(node->bbox[0][0], node->bbox[0][1], node->bbox[0][3], node->bbox[0][4], 0xffff7f00);
    AM_DrawBox(node->bbox[1][0], node->bbox[1][1], node->bbox[1][3], node->bbox[1][4], 0xffffffff);
  }
}


//==========================================================================
//
//  AM_DrawRenderedSubs
//
//==========================================================================
static void AM_DrawRenderedSubs (const subsector_t *sub, vuint32 color, bool drawMinisegs) {
  if (!sub) return;
  const seg_t *seg = &GClLevel->Segs[sub->firstline];
  for (unsigned i = sub->numlines; i--; ++seg) {
    if (!drawMinisegs && !seg->linedef) continue;
    AM_DrawSimpleLine(seg->v1->x, seg->v1->y, seg->v2->x, seg->v2->y, color);
  }
}


//==========================================================================
//
//  AM_DrawRenderedSubs
//
//==========================================================================
static void AM_DrawRenderedSubs () {
  subsector_t *mysub = GClLevel->PointInSubsector(cl->ViewOrg);
  check(mysub);

  const subsector_t *sub = &GClLevel->Subsectors[0];
  for (unsigned scount = GClLevel->NumSubsectors; scount--; ++sub) {
    if (mysub == sub) continue;
    if (!Drawer->RendLev->IsSubsectorRendered(sub)) continue;
    AM_DrawRenderedSubs(sub, 0xff00ffff, true);
  }

  if (mysub) AM_DrawRenderedSubs(mysub, (Drawer->RendLev->IsSubsectorRendered(mysub) ? 0xff00ff00 : 0xffff0000), true);
}


//==========================================================================
//
//  AM_drawLineCharacter
//
//==========================================================================
static void AM_drawLineCharacter (const mline_t *lineguy, int lineguylines,
  float scale, float angle, vuint32 color, float x, float y)
{
  float msinAngle = msin(angle);
  float mcosAngle = mcos(angle);

  for (int i = 0; i < lineguylines; ++i) {
    mline_t l = lineguy[i];

    if (scale) {
      l.a.x = scale*l.a.x;
      l.a.y = scale*l.a.y;
      l.b.x = scale*l.b.x;
      l.b.y = scale*l.b.y;
    }

    if (angle) {
      float oldax = l.a.x;
      float olday = l.a.y;
      float oldbx = l.b.x;
      float oldby = l.b.y;

      l.a.x = oldax*mcosAngle-olday*msinAngle;
      l.a.y = oldax*msinAngle+olday*mcosAngle;
      l.b.x = oldbx*mcosAngle-oldby*msinAngle;
      l.b.y = oldbx*msinAngle+oldby*mcosAngle;
    }

    l.a.x += x;
    l.a.y += y;
    l.b.x += x;
    l.b.y += y;

    AM_drawMline(&l, color);
  }
}


//==========================================================================
//
//  AM_drawPlayers
//
//==========================================================================
static void AM_drawPlayers () {
  const mline_t *player_arrow;
  float angle;
  int NUMPLYRLINES;

  if (am_cheating) {
    player_arrow = player_arrow_ddt;
    NUMPLYRLINES = NUMPLYRLINES3;
  } else if (am_player_arrow == 1) {
    player_arrow = player_arrow2;
    NUMPLYRLINES = NUMPLYRLINES2;
  } else {
    player_arrow = player_arrow1;
    NUMPLYRLINES = NUMPLYRLINES1;
  }

  if (am_rotate) {
    angle = 90.0f;
  } else {
    angle = cl->ViewAngles.yaw;
  }

  AM_drawLineCharacter(player_arrow, NUMPLYRLINES, 0.0f, angle,
    PlayerColor, FTOM(MTOF(cl->ViewOrg.x)), FTOM(MTOF(cl->ViewOrg.y)));
  return;
}


//==========================================================================
//
//  AM_drawThings
//
//==========================================================================
static void AM_drawThings (vuint32 color) {
  for (TThinkerIterator<VEntity> Ent(GClLevel); Ent; ++Ent) {
    VEntity *mobj = *Ent;
    if (!mobj->State || (mobj->GetFlags()&(_OF_Destroyed|_OF_DelayedDestroy))) continue;
    if (am_cheating <= 2) {
      if (mobj->EntityFlags&(VEntity::EF_NoSector|VEntity::EF_Invisible|VEntity::EF_NoBlockmap)) continue;
      if (mobj->RenderStyle == STYLE_None) continue;
    }

    float x = FTOM(MTOF(mobj->Origin.x));
    float y = FTOM(MTOF(mobj->Origin.y));
    float angle = mobj->/*Angles*/GetInterpolatedDrawAngles().yaw; // anyway

    if (am_rotate) {
      AM_rotatePoint(&x, &y);
      angle += 90.0f-cl->ViewAngles.yaw;
    }

    AM_drawLineCharacter(thintriangle_guy, NUMTHINTRIANGLEGUYLINES, 16.0f, angle, color, x, y);
  }
}


//==========================================================================
//
//  AM_drawOneLight
//
//==========================================================================
static void AM_drawOneLight (const VRenderLevelPublic::LightInfo &lt) {
  if (!lt.active || lt.radius < 1) return;
  float x = lt.origin.x;
  float y = lt.origin.y;
  if (am_rotate) AM_rotatePoint(&x, &y);
  Drawer->DrawLine(CXMTOFF(x-2), CYMTOFF(y-2), lt.color, CXMTOFF(x+2), CYMTOFF(y-2), lt.color);
  Drawer->DrawLine(CXMTOFF(x+2), CYMTOFF(y-2), lt.color, CXMTOFF(x+2), CYMTOFF(y+2), lt.color);
  Drawer->DrawLine(CXMTOFF(x+2), CYMTOFF(y+2), lt.color, CXMTOFF(x-2), CYMTOFF(y+2), lt.color);
  Drawer->DrawLine(CXMTOFF(x-2), CYMTOFF(y+2), lt.color, CXMTOFF(x-2), CYMTOFF(y-2), lt.color);
  // draw circle
  float px = 0, py = 0;
  for (float angle = 0; angle <= 360; angle += 10) {
    float x0 = x+msin(angle)*lt.radius;
    float y0 = y-mcos(angle)*lt.radius;
    if (angle) {
      Drawer->DrawLine(CXMTOFF(px), CYMTOFF(py), lt.color, CXMTOFF(x0), CYMTOFF(y0), lt.color);
    }
    px = x0;
    py = y0;
  }
}


//==========================================================================
//
//  AM_drawStaticLights
//
//==========================================================================
static void AM_drawStaticLights (vuint32 color) {
  if (!GClLevel->RenderData) return;
  int count = GClLevel->RenderData->GetStaticLightCount();
  for (int f = 0; f < count; ++f) {
    auto lt = GClLevel->RenderData->GetStaticLight(f);
    AM_drawOneLight(lt);
  }
}


//==========================================================================
//
//  AM_drawDynamicLights
//
//==========================================================================
static void AM_drawDynamicLights (vuint32 color) {
  if (!GClLevel->RenderData) return;
  int count = GClLevel->RenderData->GetDynamicLightCount();
  for (int f = 0; f < count; ++f) {
    auto lt = GClLevel->RenderData->GetDynamicLight(f);
    AM_drawOneLight(lt);
  }
}


//==========================================================================
//
//  AM_drawMarks
//
//==========================================================================
static void AM_drawMarks () {
  for (int i = 0; i < AM_NUMMARKPOINTS; ++i) {
    if (markpoints[i].x == -1.0f) continue; // FIXME
    int w = LittleShort(GTextureManager.TextureWidth(marknums[i]));
    int h = LittleShort(GTextureManager.TextureHeight(marknums[i]));

    mpoint_t pt;
    pt.x = markpoints[i].x;
    pt.y = markpoints[i].y;

    if (am_rotate) AM_rotatePoint(&pt.x, &pt.y);

    int fx = (int)(CXMTOF(pt.x)/fScaleX);
    int fy = (int)((CYMTOF(pt.y)-3.0f)/fScaleX);
    //fx = (int)(CXMTOF(markpoints[i].x)*fScaleXI);
    //fy = (int)(CYMTOF(markpoints[i].y)*fScaleXI);
    if (fx >= f_x && fx <= f_w-w && fy >= f_y && fy <= f_h-h && marknums[i] != -1) {
      R_DrawPic(fx, fy, marknums[i]);
    }
  }
}


//===========================================================================
//
//  AM_DrawWorldTimer
//
//===========================================================================
void AM_DrawWorldTimer () {
  int days;
  int hours;
  int minutes;
  int seconds;
  int worldTimer;
  char timeBuffer[64];
  char dayBuffer[20];

  if (!cl) return;

  worldTimer = (int)cl->WorldTimer;
  if (worldTimer < 0) worldTimer = 0;
  //if (!worldTimer) return;

  days = worldTimer/86400;
  worldTimer -= days*86400;
  hours = worldTimer/3600;
  worldTimer -= hours*3600;
  minutes = worldTimer/60;
  worldTimer -= minutes*60;
  seconds = worldTimer;

  T_SetFont(SmallFont);
  T_SetAlign(hleft, vtop);
  snprintf(timeBuffer, sizeof(timeBuffer), "%.2d : %.2d : %.2d", hours, minutes, seconds);
  T_DrawText(560, 8, timeBuffer, CR_UNTRANSLATED);

  if (days) {
    if (days == 1) {
      snprintf(dayBuffer, sizeof(dayBuffer), "%.2d DAY", days);
    } else {
      snprintf(dayBuffer, sizeof(dayBuffer), "%.2d DAYS", days);
    }
    T_DrawText(560, 18, dayBuffer, CR_UNTRANSLATED);
    if (days >= 5) T_DrawText(550, 28, "YOU FREAK!!!", CR_UNTRANSLATED);
  }

  /*
  worldTimer = (int)GLevel->Time;
  days = worldTimer/86400;
  worldTimer -= days*86400;
  hours = worldTimer/3600;
  worldTimer -= hours*3600;
  minutes = worldTimer/60;
  worldTimer -= minutes*60;
  seconds = worldTimer;

  T_SetFont(SmallFont);
  T_SetAlign(hleft, vtop);
  snprintf(timeBuffer, sizeof(timeBuffer), "%.2d : %.2d : %.2d", hours, minutes, seconds);
  T_DrawText(560, 8+10, timeBuffer, CR_UNTRANSLATED);
  */
}


//===========================================================================
//
//  AM_DrawDeathmatchStats
//
//===========================================================================
#if 0
static void AM_DrawDeathmatchStats () {
/*
  int i, k, m;
  int order[MAXPLAYERS];
  char textBuffer[80];
  int yPosition;

  for (i = 0; i < MAXPLAYERS; i++)
  {
    order[i] = -1;
  }
  for (i = 0; i < MAXPLAYERS; i++)
  {
    if (!GPlayersBase[i])
    {
      continue;
    }
    for (k = 0; k < MAXPLAYERS; k++)
    {
      if (order[k] == -1)
      {
        order[k] = i;
        break;
      }
      else if (GPlayersBase[i]->Frags > GPlayersBase[order[k]]->Frags)
      {
        for (m = MAXPLAYERS - 1; m > k; m--)
        {
          order[m] = order[m-1];
        }
        order[k] = i;
        break;
      }
    }
  }
  yPosition = 15;
  for (i = 0; i < MAXPLAYERS; i++)
  {
    if (order[i] == -1)
    {
      break;
    }
    T_SetFont(font_small);
    T_SetAlign(hleft, vtop);
    T_DrawText(8, yPosition, *GPlayersBase[order[i]]->PlayerName);
    snprintf(textBuffer, sizeof(textBuffer), "%d", GPlayersBase[order[i]]->Frags);
    T_DrawText(80, yPosition, textBuffer);
    yPosition += 10;
  }
*/
}
#endif


//===========================================================================
//
//  AM_DrawLevelStats
//
//===========================================================================
static void AM_DrawLevelStats () {
  int kills;
  int totalkills;
  int items;
  int totalitems;
  int secrets;
  int totalsecrets;
  char kill[80];
  char secret[80];
  char item[80];

  kills = cl->KillCount;
  items = cl->ItemCount;
  secrets = cl->SecretCount;
  totalkills = GClLevel->LevelInfo->TotalKills;
  totalitems = GClLevel->LevelInfo->TotalItems;
  totalsecrets = GClLevel->LevelInfo->TotalSecret;

  T_SetFont(SmallFont);
  T_SetAlign(hleft, vtop);
  snprintf(kill, sizeof(kill), "Kills: %.2d / %.2d", kills, totalkills);
  T_DrawText(8, 390, kill, CR_RED);
  snprintf(item, sizeof(item), "Items: %.2d / %.2d", items, totalitems);
  T_DrawText(8, 400, item, CR_GREEN);
  snprintf(secret, sizeof(secret), "Secrets: %.2d / %.2d", secrets, totalsecrets);
  T_DrawText(8, 410, secret, CR_GOLD);
}


//==========================================================================
//
//  StringToColor
//
//==========================================================================
static vuint32 StringToColor (const char *str) {
/*
  int r, g, b;
  char *p;

  vuint32 alpha = clampToByte((int)((am_overlay_alpha < 0 ? 0.1f : am_overlay_alpha > 1 ? 1.0f : am_overlay_alpha)*255));
  alpha <<= 24;
  //const vuint32 alpha = 0xff000000U;

  r = strtol(str, &p, 16)&0xff;
  g = strtol(p, &p, 16)&0xff;
  b = strtol(p, &p, 16)&0xff;
  return alpha|(r<<16)|(g<<8)|b;
  */
  vuint32 clr = M_ParseColor(str)&0xffffffu;
  clr |= ((vuint32)((am_overlay_alpha < 0.0f ? 0.1f : am_overlay_alpha > 1.0f ? 1.0f : am_overlay_alpha)*255))<<24;
  return clr;
}


//==========================================================================
//
//  AM_CheckVariables
//
//==========================================================================
static void AM_CheckVariables () {
  // check for screen resolution change
  if (f_w != ScreenWidth || f_h != ScreenHeight-SB_RealHeight()) {
    float old_mtof_zoommul = mtof_zoommul;
    mtof_zoommul = scale_mtof/start_scale_mtof;

    f_w = ScreenWidth;
    f_h = ScreenHeight-(screen_size < 11 ? SB_RealHeight() : 0);

    float a = (float)f_w/max_w;
    float b = (float)f_h/max_h;

    min_scale_mtof = (a < b ? a : b);
    max_scale_mtof = (float)f_h/(2.0f*PLAYERRADIUS);

    scale_mtof = min_scale_mtof/0.7f;
    if (scale_mtof > max_scale_mtof) scale_mtof = min_scale_mtof;
    scale_ftom = 1.0f/scale_mtof;
    start_scale_mtof = scale_mtof;

    AM_changeWindowScale();

    mtof_zoommul = old_mtof_zoommul;
  }

  WallColor = StringToColor(am_color_wall);
  TSWallColor = StringToColor(am_color_tswall);
  FDWallColor = StringToColor(am_color_fdwall);
  CDWallColor = StringToColor(am_color_cdwall);
  EXWallColor = StringToColor(am_color_exwall);
  SecretWallColor = StringToColor(am_color_secretwall);
  PowerWallColor = StringToColor(am_color_power);
  GridColor = StringToColor(am_color_grid);
  ThingColor = StringToColor(am_color_thing);
  PlayerColor = StringToColor(am_color_player);
  MinisegColor = StringToColor(am_color_miniseg);
}


//==========================================================================
//
//  AM_Drawer
//
//==========================================================================
void AM_Drawer () {
  AM_Check();
  if (!automapactive) return;

  //if (am_overlay) glColor4f(1, 1, 1, (am_overlay_alpha < 0.1f ? 0.1f : am_overlay_alpha > 1.0f ? 1.0f : am_overlay_alpha));

  AM_CheckVariables();
  AM_clearFB();

  Drawer->StartAutomap(am_overlay);
  if (grid) AM_drawGrid(GridColor);
  AM_drawWalls();
  AM_drawPlayers();
  if (am_cheating >= 2 || (cl->PlayerFlags&VBasePlayer::PF_AutomapShowThings)) AM_drawThings(ThingColor);
  if (am_cheating && am_show_static_lights) AM_drawStaticLights(ThingColor);
  if (am_cheating && am_show_dynamic_lights) AM_drawDynamicLights(ThingColor);
  if (am_cheating && am_show_minisegs) AM_DrawMinisegs();
  if (am_cheating && am_show_rendered_nodes) AM_DrawRenderedNodes();
  if (am_cheating && am_show_rendered_subs) AM_DrawRenderedSubs();
  Drawer->EndAutomap();
  AM_DrawWorldTimer();
  T_SetFont(SmallFont);
  T_SetAlign(hleft, vbottom);
  T_DrawText(20, 480-sb_height-7-9, va("%s (n%d:c%d)", *GClLevel->MapName, GClLevel->LevelInfo->LevelNum, GClLevel->LevelInfo->Cluster), CR_UNTRANSLATED);
  T_DrawText(20, 480-sb_height-7, *GClLevel->LevelInfo->GetLevelName(), CR_UNTRANSLATED);
  if (am_show_stats) AM_DrawLevelStats();
  //if (am_show_stats == 2 && GClGame->maxclients > 1 && GClGame->deathmatch) AM_DrawDeathmatchStats();
  if (use_marks) AM_drawMarks();

  //if (am_overlay) glColor4f(1, 1, 1, 1);
}


COMMAND(Iddt) {
  am_cheating = (am_cheating+1)%3;
}


COMMAND(toggle_automap) {
  if (!cl || !GClGame || !GGameInfo || GClGame->intermission || GGameInfo->NetMode <= NM_TitleMap) {
    return;
  }
#ifdef CLIENT
  //if (MN_Active() || C_Active() || NUI_IsPaused()) return;
  if (GGameInfo->IsPaused()) return;
#endif
  am_active = !am_active;
}
