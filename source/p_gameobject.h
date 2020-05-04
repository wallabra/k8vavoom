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
//**
//**  INTERNAL DATA TYPES
//**  used by play and refresh
//**
//**************************************************************************
#include "textures/r_tex_id.h"

class VRenderLevelPublic;
class VTextureTranslation;
class VAcsLevel;
class VNetContext;

class VThinker;
class VLevelInfo;
class VEntity;
class VBasePlayer;
class VWorldInfo;
class VGameInfo;

struct VMapInfo;

struct sector_t;
struct fakefloor_t;
struct seg_t;
struct subsector_t;
struct node_t;
struct surface_t;
struct segpart_t;
struct drawseg_t;
struct sec_surface_t;
struct subregion_t;

struct decal_t;
struct opening_t;


// line specials that are used by the loader
enum {
  LNSPEC_PolyStartLine        = 1,
  LNSPEC_PolyExplicitLine     = 5,
  LNSPEC_LineHorizon          = 9,
  LNSPEC_DoorLockedRaise      = 13,
  LNSPEC_ACSLockedExecute     = 83,
  LNSPEC_ACSLockedExecuteDoor = 85,
  LNSPEC_LineMirror           = 182,
  LNSPEC_StaticInit           = 190,
  LNSPEC_LineTranslucent      = 208,
  LNSPEC_TransferHeights      = 209,

  LNSPEC_ScrollTextureLeft = 100,// 100
  LNSPEC_ScrollTextureRight,
  LNSPEC_ScrollTextureUp,
  LNSPEC_ScrollTextureDown,

  LNSPEC_PlaneCopy = 118,
  LNSPEC_PlaneAlign = 181,

  /*
  LNSPEC_FloorLowerToNearest = 22,
  LNSPEC_FloorLowerToLowestChange = 241,
  LNSPEC_FloorLowerToHighest = 242,
  LNSPEC_ElevatorLowerToNearest = 247,
  */
};


enum {
  SPF_NOBLOCKING   = 1u, // Not blocking
  SPF_NOBLOCKSIGHT = 2u, // Do not block sight
  SPF_NOBLOCKSHOOT = 4u, // Do not block shooting
  SPF_ADDITIVE     = 8u, // Additive translucency

  SPF_MAX_FLAG     = 16u,
  SPF_FLAG_MASK    = 15u,

  // used only as tracer flags
  SPF_IGNORE_FAKE_FLOORS = 1u<<6, //64u,
  SPF_IGNORE_BASE_REGION = 1u<<7, //128u

  // for BSP tracer (can be used for additional blocking flags)
  SPF_PLAYER = 1u<<8, //256u
  SPF_MONSTER = 1u<<9, //512u
};


//==========================================================================
//
//  DrawSeg
//
//  TODO: document this!
//
//==========================================================================
struct drawseg_t {
  seg_t *seg;
  drawseg_t *next;

  segpart_t *top;
  segpart_t *mid;
  segpart_t *bot;
  segpart_t *topsky;
  segpart_t *extra;

  surface_t *HorizonTop;
  surface_t *HorizonBot;
};


//==========================================================================
//
//  LineDef
//
//==========================================================================

// move clipping aid for LineDefs
enum {
  ST_HORIZONTAL,
  ST_VERTICAL,
  ST_POSITIVE,
  ST_NEGATIVE,
};

// If a texture is pegged, the texture will have the end exposed to air held
// constant at the top or bottom of the texture (stairs or pulled down
// things) and will move with a height change of one of the neighbor sectors.
// Unpegged textures allways have the first row of the texture at the top
// pixel of the line for both top and bottom textures (use next to windows).

// LineDef attributes
enum {
  ML_BLOCKING            = 0x00000001u, // solid, is an obstacle
  ML_BLOCKMONSTERS       = 0x00000002u, // blocks monsters only
  ML_TWOSIDED            = 0x00000004u, // backside will not be present at all
  ML_DONTPEGTOP          = 0x00000008u, // upper texture unpegged
  ML_DONTPEGBOTTOM       = 0x00000010u, // lower texture unpegged
  ML_SECRET              = 0x00000020u, // don't map as two sided: IT'S A SECRET!
  ML_SOUNDBLOCK          = 0x00000040u, // don't let sound cross two of these
  ML_DONTDRAW            = 0x00000080u, // don't draw on the automap
  ML_MAPPED              = 0x00000100u, // set if already drawn in automap
  ML_REPEAT_SPECIAL      = 0x00000200u, // special is repeatable
  ML_ADDITIVE            = 0x00000400u, // additive translucency
  ML_MONSTERSCANACTIVATE = 0x00002000u, // monsters (as well as players) can activate the line
  ML_BLOCKPLAYERS        = 0x00004000u, // blocks players only
  ML_BLOCKEVERYTHING     = 0x00008000u, // line blocks everything
  ML_ZONEBOUNDARY        = 0x00010000u, // boundary of reverb zones
  ML_RAILING             = 0x00020000u,
  ML_BLOCK_FLOATERS      = 0x00040000u,
  ML_CLIP_MIDTEX         = 0x00080000u, // automatic for every Strife line
  ML_WRAP_MIDTEX         = 0x00100000u,
  ML_3DMIDTEX            = 0x00200000u, // not implemented
  ML_CHECKSWITCHRANGE    = 0x00400000u, // not implemented
  ML_FIRSTSIDEONLY       = 0x00800000u, // actiavte only when crossed from front side
  ML_BLOCKPROJECTILE     = 0x01000000u,
  ML_BLOCKUSE            = 0x02000000u, // blocks all use actions through this line
  ML_BLOCKSIGHT          = 0x04000000u, // blocks monster line of sight
  ML_BLOCKHITSCAN        = 0x08000000u, // blocks hitscan attacks
  ML_3DMIDTEX_IMPASS     = 0x10000000u, // (not implemented) [TP] if 3D midtex, behaves like a height-restricted ML_BLOCKING
  ML_KEEPDATA            = 0x20000000u, // keep FloorData or CeilingData after activating them
                                        // used to simulate original Heretic behaviour
  ML_NODECAL             = 0x40000000u, // don't spawn decals on this linedef
};

enum {
  ML_SPAC_SHIFT = 10,
  ML_SPAC_MASK = 0x00001c00u,
};

// extra flags
enum {
  ML_EX_PARTIALLY_MAPPED = 1u<<0, // some segs are visible, but not all
  ML_EX_CHECK_MAPPED     = 1u<<1, // check if all segs are mapped (done in automap drawer
};

// Special activation types
enum {
  SPAC_Cross      = 0x0001u, // when player crosses line
  SPAC_Use        = 0x0002u, // when player uses line
  SPAC_MCross     = 0x0004u, // when monster crosses line
  SPAC_Impact     = 0x0008u, // when projectile hits line
  SPAC_Push       = 0x0010u, // when player pushes line
  SPAC_PCross     = 0x0020u, // when projectile crosses line
  SPAC_UseThrough = 0x0040u, // SPAC_USE, but passes it through
  // SPAC_PTouch is remapped as (SPAC_Impact|SPAC_PCross)
  SPAC_AnyCross   = 0x0080u,
  SPAC_MUse       = 0x0100u, // when monster uses line
  SPAC_MPush      = 0x0200u, // when monster pushes line
  SPAC_UseBack    = 0x0400u, // can be used from the backside
};


struct TagHash;

/*
struct TagHashIter {
  TagHash *th;
  int tag;
  int index;
};
*/

TagHash *tagHashAlloc ();
void tagHashClear (TagHash *th);
void tagHashFree (TagHash *&th);
void tagHashPut (TagHash *th, int tag, void *ptr);
bool tagHashCheckTag (const TagHash *th, int tag, const void *ptr);
/*
bool tagHashFirst (TagHashIter *it, TagHash *th, int tag);
bool tagHashNext (TagHashIter *it);
void *tagHashCurrent (const TagHashIter *it);
*/
// returns -1 when finished
int tagHashFirst (const TagHash *th, int tag);
int tagHashNext (const TagHash *th, int index, int tag);
void *tagHashPtr (const TagHash *th, int index);
int tagHashTag (const TagHash *th, int index);


struct line_t : public TPlane {
  // vertices, from v1 to v2
  TVec *v1;
  TVec *v2;

  // precalculated *2D* (v2-v1) for side checking (i.e. z is zero)
  TVec dir;
  // normalised dir
  TVec ndir;

  vuint32 flags;
  vuint32 SpacFlags;
  vuint32 exFlags; //ML_EX_xxx

  // visual appearance: SideDefs
  // sidenum[1] will be -1 if one sided
  vint32 sidenum[2];

  // neat. another bounding box, for the extent of the LineDef
  float bbox2d[4];

  // to aid move clipping
  vint32 slopetype;

  // front and back sector
  // note: redundant? can be retrieved from SideDefs
  sector_t *frontsector;
  sector_t *backsector;

  // if == validcount, already checked (used in various traversing, like LOS, and other line tracing)
  vint32 validcount;

  float alpha;

  vint32 special;
  vint32 arg1;
  vint32 arg2;
  vint32 arg3;
  vint32 arg4;
  vint32 arg5;

  vint32 locknumber;

  TArray<vint32> moreTags; // except `lineTag`
  vint32 lineTag;

  inline bool IsTagEqual (int tag) const {
    if (!tag || tag == -1) return false;
    if (lineTag == tag) return true;
    //return tagHashCheckTag(tagHash, tag, this);
    for (int f = 0; f < moreTags.length(); ++f) if (moreTags[f] == tag) return true;
    return false;
  }

  seg_t *firstseg; // linked by lsnext

  vint32 decalMark; // uid of current decal placement loop, to avoid endless looping

  // lines connected to `v1`
  line_t **v1lines;
  vint32 v1linesCount;

  // lines connected to `v2`
  line_t **v2lines;
  vint32 v2linesCount;

  // collision detection planes
  // first plane is usually a duplicate of a normal line plane, but idc
  TPlane *cdPlanes;

  vint32 cdPlanesCount;
  TPlane cdPlanesArray[6];
};


//==========================================================================
//
//  SideDef
//
//==========================================================================
enum {
  SDF_ABSLIGHT   = 1u<<0, // light is absolute value
  SDF_WRAPMIDTEX = 1u<<1,
  SDF_CLIPMIDTEX = 1u<<2,
  SDF_NOFAKECTX  = 1u<<3, // no fake contrast
  SDF_SMOOTHLIT  = 1u<<4, // smooth lighting, not implemented yet
  SDF_NODECAL    = 1u<<5,
  // was the corresponding textures "AASHITTY"?
  // this is required to fix bridges
  SDF_AAS_TOP    = 1u<<6,
  SDF_AAS_BOT    = 1u<<7,
  SDF_AAS_MID    = 1u<<8,
};


struct side_tex_params_t {
  float TextureOffset; // x, s axis, column
  float RowOffset; // y, t axis, top
  float ScaleX, ScaleY;
};


struct side_t {
  side_tex_params_t Top;
  side_tex_params_t Bot;
  side_tex_params_t Mid;

  // texture indices: we do not maintain names here
  // 0 means "no texture"; -1 means "i forgot what it is"
  VTextureID TopTexture;
  VTextureID BottomTexture;
  VTextureID MidTexture;

  // sector the SideDef is facing
  sector_t *Sector;

  vint32 LineNum; // line index in `Lines`

  vuint32 Flags; // SDF_XXX

  vint32 Light;
};


//==========================================================================
//
//  Sector
//
//==========================================================================
enum {
  SKY_FROM_SIDE = 0x40000000u,
};


struct sec_plane_t : public TPlane {
  float minz;
  float maxz;

  // use for wall texture mapping
  float TexZ;

  VTextureID pic;

  float xoffs;
  float yoffs;

  float XScale;
  float YScale;

  float Angle;

  float BaseAngle;
  float BaseYOffs;

  vuint32 flags; // SPF_xxx
  float Alpha;
  float MirrorAlpha;

  vint32 LightSourceSector;
  VEntity *SkyBox;

  //sector_t *parent; // can be `nullptr`, has meaning only for `SPF_ALLOCATED` planes
  //vuint32 exflags; // SPF_EX_xxx

  inline VVA_CHECKRESULT float GetPointZClamped (float x, float y) const {
    return clampval(GetPointZ(x, y), minz, maxz);
  }

  inline VVA_CHECKRESULT float GetPointZRevClamped (float x, float y) const {
    //FIXME: k8: should min and max be switched here?
    return clampval(GetPointZRev(x, y), minz, maxz);
  }

  inline VVA_CHECKRESULT float GetPointZClamped (const TVec &v) const {
    return GetPointZClamped(v.x, v.y);
  }

  inline VVA_CHECKRESULT float GetPointZRevClamped (const TVec &v) const {
    return GetPointZRevClamped(v.x, v.y);
  }
};


//==========================================================================
//
//  sector plane reference with flip flag
//
//==========================================================================
struct TSecPlaneRef {
  // WARNING! do not change this values, they are used to index things in other code!
  enum Type { Unknown = -1, Floor = 0, Ceiling = 1 };

  sec_plane_t *splane;
  /*bool*/vint32 flipped; // actually, bit flags

  TSecPlaneRef () : splane(nullptr), flipped(false) {}
  TSecPlaneRef (const TSecPlaneRef &sp) : splane(sp.splane), flipped(sp.flipped) {}
  explicit TSecPlaneRef (sec_plane_t *aplane, bool arev) : splane(aplane), flipped(arev) {}

  inline TSecPlaneRef &operator = (const TSecPlaneRef &sp) {
    if (this != &sp) { splane = sp.splane; flipped = sp.flipped; }
    return *this;
  }

  inline bool isValid () const { return !!splane; }

  inline bool isFloor () const { return (GetNormalZSafe() > 0.0f); }
  inline bool isCeiling () const { return (GetNormalZSafe() < 0.0f); }

  inline bool isSlope () const { return (fabsf(GetNormalZSafe()) != 1.0f); }

  // see enum at the top
  inline Type classify () const { const float z = GetNormalZSafe(); return (z < 0.0f ? Ceiling : z > 0.0f ? Floor : Unknown); }

  inline void set (sec_plane_t *aplane, bool arev) { splane = aplane; flipped = arev; }

  inline TVec GetNormal () const { return (!flipped ? splane->normal : -splane->normal); }
  inline float GetNormalZ () const { return (!flipped ? splane->normal.z : -splane->normal.z); }
  inline float GetNormalZSafe () const { return (splane ? (!flipped ? splane->normal.z : -splane->normal.z) : 0.0f); }
  inline float GetDist () const { return (!flipped ? splane->dist : -splane->dist); }
  inline TPlane GetPlane () const { TPlane res; res.normal = (!flipped ? splane->normal : -splane->normal); res.dist = (!flipped ? splane->dist : -splane->dist); return res; }

  inline float PointDistance (const TVec &p) const { return (!flipped ? DotProduct(p, splane->normal)-splane->dist : DotProductV2Neg(p, splane->normal)-(-splane->dist)); }

  // valid only for horizontal planes!
  inline float GetRealDist () const { return (!flipped ? splane->dist*splane->normal.z : (-splane->dist)*(-splane->normal.z)); }

  inline void Flip () { flipped = !flipped; }

  // get z of point with given x and y coords
  // don't try to use it on a vertical plane
  inline VVA_CHECKRESULT float GetPointZ (float x, float y) const {
    return (!flipped ? splane->GetPointZ(x, y) : splane->GetPointZRev(x, y));
  }

  inline VVA_CHECKRESULT float GetPointZClamped (float x, float y) const {
    //return clampval((!flipped ? splane->GetPointZ(x, y) : splane->GetPointZRev(x, y)), splane->minz, splane->maxz);
    return (!flipped ? splane->GetPointZClamped(x, y) : splane->GetPointZRevClamped(x, y));
  }

  inline VVA_CHECKRESULT float DotPoint (const TVec &point) const {
    if (!flipped) {
      return DotProduct(point, splane->normal);
    } else {
      // gozzo shit, idc
      return DotProduct(point, -splane->normal);
    }
  }

  inline VVA_CHECKRESULT float DotPointDist (const TVec &point) const {
    if (!flipped) {
      return DotProduct(point, splane->normal)-splane->dist;
    } else {
      // gozzo shit, idc
      return DotProduct(point, -splane->normal)+splane->dist;
    }
  }

  inline VVA_CHECKRESULT float GetPointZ (const TVec &v) const {
    return GetPointZ(v.x, v.y);
  }

  inline VVA_CHECKRESULT float GetPointZClamped (const TVec &v) const {
    return GetPointZClamped(v.x, v.y);
  }

  // returns side 0 (front) or 1 (back, or on plane)
  inline VVA_CHECKRESULT int PointOnSide (const TVec &point) const {
    return (DotPointDist(point) <= 0.0f);
  }

  // returns side 0 (front) or 1 (back, or on plane)
  inline VVA_CHECKRESULT int PointOnSideThreshold (const TVec &point) const {
    return (DotPointDist(point) < 0.1f);
  }

  // returns side 0 (front, or on plane) or 1 (back)
  // "fri" means "front inclusive"
  inline VVA_CHECKRESULT int PointOnSideFri (const TVec &point) const {
    return (DotPointDist(point) < 0.0f);
  }

  // returns side 0 (front), 1 (back), or 2 (on)
  // used in line tracing (only)
  inline VVA_CHECKRESULT int PointOnSide2 (const TVec &point) const {
    const float dot = DotPointDist(point);
    return (dot < -0.1f ? 1 : dot > 0.1f ? 0 : 2);
  }

  // returns side 0 (front), 1 (back)
  // if at least some part of the sphere is on a front side, it means "front"
  inline VVA_CHECKRESULT int SphereOnSide (const TVec &center, float radius) const {
    return (DotPointDist(center) <= -radius);
  }

  inline VVA_CHECKRESULT bool SphereTouches (const TVec &center, float radius) const {
    return (fabsf(DotPointDist(center)) < radius);
  }

  // returns side 0 (front), 1 (back), or 2 (collides)
  inline VVA_CHECKRESULT int SphereOnSide2 (const TVec &center, float radius) const {
    const float d = DotPointDist(center);
    return (d < -radius ? 1 : d > radius ? 0 : 2);
  }

  // distance from point to plane
  // plane must be normalized
  inline VVA_CHECKRESULT float Distance (const TVec &p) const {
    //return (cast(double)normal.x*p.x+cast(double)normal.y*p.y+cast(double)normal.z*cast(double)p.z)/normal.dbllength;
    //return VSUM3(normal.x*p.x, normal.y*p.y, normal.z*p.z); // plane normal has length 1
    return DotPointDist(p);
  }
};


struct sec_params_t {
  enum {
    LFC_FloorLight_Abs    = 1u<<0,
    LFC_CeilingLight_Abs  = 1u<<1,
    LFC_FloorLight_Glow   = 1u<<2,
    LFC_CeilingLight_Glow = 1u<<3,
  };

  vuint32 lightlevel;
  vuint32 LightColor;
  vuint32 Fade;
  vuint32 contents;
  // bit0: floor light is absolute; bit1: the same for ceiling
  // bit2: has floor glow; bit3: has ceiling glow
  vuint32 lightFCFlags;
  // light levels
  vuint32 lightFloor;
  vuint32 lightCeiling;
  // glow colors
  vuint32 glowFloor;
  vuint32 glowCeiling;
  float glowFloorHeight;
  float glowCeilingHeight;
};


// "regions" keeps 3d floors for sector
// region inside may be non-solid, if the corresponding flag is set
struct sec_region_t {
  sec_region_t *next;
  // planes
  // floor is bottom plane (and points bottom, like normal ceiling, if not base region)
  TSecPlaneRef efloor;
  // ceiling is top plane (and points top, like normal froor, if not base region)
  TSecPlaneRef eceiling;
  // contents and lighting
  sec_params_t *params;

  // side of this region
  // can be `nullptr`, cannot be changed after surface creation
  // this is from our floor to our ceiling
  line_t *extraline;

  // flags are here to implement various kinds of 3d floors
  enum {
    RF_BaseRegion    = 1u<<0, // sane k8vavoom-style region; only one, only first
    RF_NonSolid      = 1u<<1,
    // ignore this region in gap/opening processing
    // this flags affects collision detection
    RF_OnlyVisual    = 1u<<2,
    // the following flags are only for renderer, collision detection igonres them
    RF_SkipFloorSurf = 1u<<3, // do not create floor surface for this region
    RF_SkipCeilSurf  = 1u<<4, // do not create ceiling surface for this region
    RF_SaneRegion    = 1u<<5, // k8vavoom-style 3d floor region
  };
  vuint32 regflags;
};


//
// phares 3/14/98
//
// Sector list node showing all sectors an object appears in.
//
// There are two threads that flow through these nodes. The first thread
// starts at TouchingThingList in a sector_t and flows through the SNext
// links to find all mobjs that are entirely or partially in the sector.
// The second thread starts at TouchingSectorList in a VEntity and flows
// through the TNext links to find all sectors a thing touches. This is
// useful when applying friction or push effects to sectors. These effects
// can be done as thinkers that act upon all objects touching their sectors.
// As an mobj moves through the world, these nodes are created and
// destroyed, with the links changed appropriately.
//
// k8: note that sector cannot appear in a list twice, so it is safe to
//     rely on this in various effectors.
//
// For the links, nullptr means top or end of list.
//
struct msecnode_t {
  sector_t *Sector; // a sector containing this object
  VEntity *Thing; // this object
  msecnode_t *TNext; // next msecnode_t for this thing
  msecnode_t *TPrev; // prev msecnode_t for this thing
  msecnode_t *SNext; // next msecnode_t for this sector (also, link for `HeadSecNode` aka "free nodes list")
  msecnode_t *SPrev; // prev msecnode_t for this sector
  vint32 Visited; // killough 4/4/98, 4/7/98: used in search algorithms
};


// the SECTORS record, at runtime
// stores things/mobjs
struct sector_t {
  sec_plane_t floor;
  sec_plane_t ceiling;
  sec_params_t params;

  // floor/ceiling info for sector
  // first region is always base sector region that keeps "emptyness"
  // other regions are "filled space" (yet that space can be non-solid)
  // region floor and ceiling are better have the same set of `SPF_NOxxx` flags.
  sec_region_t *eregions;

  // this is cached planes for thing gap determination
  // planes are sorted by... something
  //TArray<TSecPlaneRef> regplanes;

  sector_t *deepref; // deep water hack

  vint32 special;

  TArray<vint32> moreTags; // except `lineTag`
  vint32 sectorTag;

  inline bool IsTagEqual (int tag) const {
    if (!tag || tag == -1) return false;
    if (sectorTag == tag) return true;
    //return tagHashCheckTag(tagHash, tag, this);
    for (int f = 0; f < moreTags.length(); ++f) if (moreTags[f] == tag) return true;
    return false;
  }

  float skyheight;

  // stone, metal, heavy, etc...
  vint32 seqType;

  // mapblock bounding box for height changes
  vint32 blockbox[4];

  // origin for any sounds played by the sector
  TVec soundorg;

  // if == validcount, already checked (used in various traversing, like LOS, and other line tracing)
  vint32 validcount;

  // list of subsectors in sector
  // used to check if client can see this sector
  // build in map loader; subsectors are linked with `seclink`
  subsector_t *subsectors;

  // list of things in sector
  VEntity *ThingList;
  msecnode_t *TouchingThingList;

  line_t **lines; // [linecount] size
  vint32 linecount;

  // neighbouring sectors
  sector_t **nbsecs;
  vint32 nbseccount;

  // Boom's fake floors (and deepwater)
  sector_t *heightsec;
  fakefloor_t *fakefloors; // info for rendering

  // flags
  enum {
    SF_HasExtrafloors   = 0x0001u, // this sector has extrafloors
    SF_ExtrafloorSource = 0x0002u, // this sector is a source of an extrafloor
    SF_TransferSource   = 0x0004u, // source of an heightsec or transfer light
    SF_FakeFloorOnly    = 0x0008u, // do not draw fake ceiling
    SF_ClipFakePlanes   = 0x0010u, // as a heightsec, clip planes to target sector's planes
    SF_NoFakeLight      = 0x0020u, // heightsec does not change lighting
    SF_IgnoreHeightSec  = 0x0040u, // heightsec is only for triggering sector actions (i.e. don't draw them)
    SF_UnderWater       = 0x0080u, // sector is underwater
    SF_Silent           = 0x0100u, // actors don't make noise in this sector
    SF_NoFallingDamage  = 0x0200u, // no falling damage in this sector
    SF_FakeCeilingOnly  = 0x0400u, // when used as heightsec in R_FakeFlat, only copies ceiling
    SF_HangingBridge    = 0x0800u, // fake hanging bridge
    SF_Has3DMidTex      = 0x1000u, // has any 3dmidtex linedef?
    // mask with this to check if this is "arg1==0" Boom crap
    SF_FakeBoomMask     = SF_FakeFloorOnly|SF_ClipFakePlanes|/*SF_UnderWater|*/SF_IgnoreHeightSec/*|SF_NoFakeLight*/,
  };
  vuint32 SectorFlags;

  // 0 = untraversed, 1,2 = sndlines -1
  vint32 soundtraversed;

  // thing that made a sound (or null)
  VEntity *SoundTarget;

  // thinker for reversable actions
  VThinker *FloorData;
  VThinker *CeilingData;
  VThinker *LightingData;
  VThinker *AffectorData;

  // sector action triggers
  VEntity *ActionList;

  vint32 Damage;
  VName DamageType; // can be empty for "standard/unknown"
  vint32 DamageInterval; // in ticks; zero means `32` (Doom default)
  vint32 DamageLeaky; // <0: none; 0: default (5); 256 is 100% (i.e. it is actually a byte)

  float Friction;
  float MoveFactor;
  float Gravity; // Sector gravity (1.0 is normal)

  vint32 Sky; // if SKY_FROM_SIDE set, this is replaced sky; sky2 for index 0, or top texture from side[index-1]

  vint32 Zone; // reverb zone id

  vint32 ZExtentsCacheId;
  float LastMinZ, LastMaxZ;

  // this is used to check for "floor holes" that should be filled to emulate original flat floodfill bug
  // if sector has more than one neighbour, this is `nullptr`
  sector_t *othersecFloor;
  sector_t *othersecCeiling;


  inline bool Has3DFloors () const noexcept { return !!eregions->next; }
  inline bool HasAnyExtraFloors () const noexcept { return (!!eregions->next) || (!!heightsec); }

  inline bool Has3DSlopes () const noexcept {
    for (const sec_region_t *reg = eregions->next; reg; reg = reg->next) {
      if (reg->regflags&(sec_region_t::RF_NonSolid|sec_region_t::RF_OnlyVisual)) continue;
      if (reg->efloor.isSlope() || reg->eceiling.isSlope()) return true;
    }
    return false;
  }

  // should be called for new sectors to setup base region
  void CreateBaseRegion ();
  void DeleteAllRegions ();

  // `next` is set, everything other is zeroed
  sec_region_t *AllocRegion ();
};


//==========================================================================
//
//  Polyobject
//
//==========================================================================

// polyobj data
struct polyobj_t {
  friend class PolySubIter;

  seg_t **segs;
  vint32 numsegs;
  TVec startSpot;
  TVec *originalPts; // used as the base for the rotations
  TVec *prevPts; // use to restore the old point values
  sector_t *originalSector; // used to get height
  float angle;
  vint32 tag; // reference tag assigned in HereticEd
  vint32 bbox2d[4];
  vint32 validcount;
  enum {
    PF_Crush       = 0x01u, // should the polyobj attempt to crush mobjs?
    PF_HurtOnTouch = 0x02u,
  };
  vuint32 PolyFlags;
  vint32 seqType;
  VThinker *SpecialData; // pointer a thinker, if the poly is moving
  vint32 index; // required for LevelInfo sound sequences

private:
  subsector_t *sub;
  // we can have more than one pobj in a subsector, and they form a doubly-linked list
  polyobj_t *subprev;
  polyobj_t *subnext;

public:
  //WARNING! polyobj can be in only one subsector at a time, so calling this
  //         will remove pobj from its current subsector, and put it into a new one!
  // it is safe to call this with `nullptr`
  void RelinkToSubsector (subsector_t *asub);
  void UnlinkFromSubsector ();

  inline subsector_t *GetSubsector () const { return sub; }

  inline polyobj_t *GetPrev () { return subprev; }
  inline const polyobj_t *GetPrev () const { return subprev; }

  inline polyobj_t *GetNext () { return subnext; }
  inline const polyobj_t *GetNext () const { return subnext; }

public:
  class PolySubIter {
  private:
    polyobj_t *pobjp;
  public:
    PolySubIter (polyobj_t *pstart) : pobjp(pstart) {}
    inline PolySubIter begin () { return PolySubIter(pobjp); }
    inline PolySubIter end () { return PolySubIter(nullptr); }
    inline bool operator == (const PolySubIter &b) const { return (pobjp == b.pobjp); }
    inline bool operator != (const PolySubIter &b) const { return (pobjp != b.pobjp); }
    inline PolySubIter operator * () const { return PolySubIter(this->pobjp); } /* required for iterator */
    inline void operator ++ () { if (pobjp) pobjp = pobjp->GetNext(); } /* this is enough for iterator */
    // accessors
    inline polyobj_t *pobj () const { return pobjp; }
    inline polyobj_t *value () const { return pobjp; }
  };
};


struct polyblock_t {
  polyobj_t *polyobj;
  polyblock_t *prev;
  polyblock_t *next;
};


struct PolyAnchorPoint_t {
  float x;
  float y;
  vint32 tag;
};


//==========================================================================
//
//  LineSeg
//
//==========================================================================
// seg flags
enum {
  SF_MAPPED  = 1u<<0, // some segs of this linedef are visible, but not all
  SF_ZEROLEN = 1u<<1, // zero-length seg (it has some fake length)
};

struct seg_t : public TPlane {
  TVec *v1;
  TVec *v2;

  float offset;
  float length;
  TVec dir; // precalced segment direction, so i don't have to do it again in surface creator

  side_t *sidedef;
  line_t *linedef;
  seg_t *lsnext; // next seg in linedef; set by `VLevel::PostProcessForDecals()`

  // sector references
  // could be retrieved from linedef, too
  // backsector is nullptr for one sided lines
  sector_t *frontsector;
  sector_t *backsector;

  seg_t *partner; // from glnodes
  subsector_t *frontsub; // front subsector (we need this for self-referencing deep water)

  // side of line (for light calculations: 0 or 1)
  vint32 side;

  vuint32 flags; // SF_xxx

  drawseg_t *drawsegs;

  // original polyobject, or `nullptr` for world seg
  polyobj_t *pobj;

  // decal list
  decal_t *decalhead;
  decal_t *decaltail;

  void appendDecal (decal_t *dc) noexcept;
  void removeDecal (decal_t *dc) noexcept; // will not delete it
};


//==========================================================================
//
//  SubRegion
//
//  TODO: document this!
//
//==========================================================================
struct subregion_t {
  sec_region_t *secregion;
  subregion_t *next;
  TSecPlaneRef floorplane;
  TSecPlaneRef ceilplane;
  sec_surface_t *realfloor;
  sec_surface_t *realceil;
  sec_surface_t *fakefloor; // can be `nullptr`
  sec_surface_t *fakeceil; // can be `nullptr`
  enum {
    SRF_ZEROSKY_FLOOR_HACK = 1u<<0,
  };
  vuint32 flags;
  vint32 count;
  drawseg_t *lines;
};


//==========================================================================
//
//  Subsector
//
//==========================================================================
// a subsector; references a sector
// basically, this is a list of LineSegs, indicating
// the visible walls that define (all or some) sides of a convex BSP leaf
struct subsector_t {
public:
  enum {
    SSMF_Rendered = 1u<<0u,
  };

public:
  sector_t *sector;
  subsector_t *seclink; // next subsector for this sector
  vint32 numlines;
  vint32 firstline;
  polyobj_t *polyfirst; // first pobj in the subsector

  node_t *parent;
  vuint32 parentChild; // our child index in parent node
  vuint32 VisFrame;
  vuint32 updateWorldFrame;

  sector_t *deepref; // for deepwater
  seg_t *firstseg;

  //k8: i love bounding boxes! (this one doesn't store z, though)
  float bbox2d[4];

  vuint32 dlightbits; // bitmask of active dynamic lights
  vuint32 dlightframe; // `dlightbits` validity counter
  subregion_t *regions;

  vuint32 miscFlags; // SSMF_xxx

  inline bool HasPObjs () const { return !!polyfirst; }
  inline polyobj_t::PolySubIter PObjFirst () const { return polyobj_t::PolySubIter(polyfirst); }
};


//==========================================================================
//
//  Node
//
//==========================================================================

// indicate a leaf
enum {
  NF_SUBSECTOR = 0x80000000,
};

// BSP node
struct node_t : public TPlane {
  // bounding box for each child
  // (x,y,z) triples (min and max)
  float bbox[2][6];

  // if NF_SUBSECTOR its a subsector
  vuint32 children[2];

  node_t *parent;
  vuint32 visframe;
  vuint32 index;
  // linedef used for this node (can be nullptr if nodes builder don't have this info)
  line_t *splitldef;
  // from the original nodes; needed to emulate original buggy "point in subsector"
  vint32 sx, sy, dx, dy; // 16.16
};


//==========================================================================
//
//  Thing
//
//==========================================================================

// map thing definition with initialised fields for global use
struct mthing_t {
  vint32 tid;
  float x;
  float y;
  float height;
  /*vint32 angle;*/
  float angle, pitch, roll;
  vint32 type;
  vint32 options;
  vint32 SkillClassFilter;
  vint32 special;
  vint32 args[5]; // was `arg1`..`arg5`
  float health; // initial health; 0 means "default"
  float scaleX, scaleY; // 0 means "default"
  VStr arg0str;
};


//==========================================================================
//
//  Strife conversation scripts
//
//==========================================================================
struct FRogueConChoice {
  vint32 GiveItem; // item given on success
  vint32 NeedItem1; // required item 1
  vint32 NeedItem2; // required item 2
  vint32 NeedItem3; // required item 3
  vint32 NeedAmount1; // amount of item 1
  vint32 NeedAmount2; // amount of item 2
  vint32 NeedAmount3; // amount of item 3
  VStr Text; // text of the answer
  VStr TextOK; // message displayed on success
  vint32 Next;  // dialog to go on success, negative values to go here immediately
  vint32 Objectives; // mission objectives, LOGxxxx lump
  VStr TextNo; // message displayed on failure (player doesn't
               // have needed thing, it haves enough health/ammo,
               // item is not ready, quest is not completed)
};


struct FRogueConSpeech {
  vint32 SpeakerID; // type of the object (MT_xxx)
  vint32 DropItem; // item dropped when killed
  vint32 CheckItem1; // item 1 to check for jump
  vint32 CheckItem2; // item 2 to check for jump
  vint32 CheckItem3; // item 3 to check for jump
  vint32 JumpToConv; // jump to conversation if have certain item(s)
  VStr Name; // name of the character
  VName Voice; // voice to play
  VName BackPic; // picture of the speaker
  VStr Text; // message
  FRogueConChoice Choices[5]; // choices
};


//==========================================================================
//
//  Misc game structs
//
//==========================================================================
enum {
  PT_ADDLINES  = 1,
  PT_ADDTHINGS = 2,
  PT_EARLYOUT  = 4,
};


struct intercept_t {
  float frac; // along trace line
  enum {
    IF_IsALine  = 0x01,
    IF_BackSide = 0x02, // not yet
  };
  vuint32 Flags;
  VEntity *thing;
  line_t *line;
  // used in path traverser, absolutely unreliable!
  float tmpHitDist;
};


struct linetrace_t {
public:
  TVec Start;
  TVec End;
  TVec Delta;
  TVec LineStart;
  TVec LineEnd;
  vuint32 PlaneNoBlockFlags;
  // the following will be calculated from `PlaneNoBlockFlags`
  vuint32 LineBlockFlags;
  // subsector we ended in (can be arbitrary if trace doesn't end in map boundaries)
  // valid only for `TraceLine()` call (i.e. BSP trace)
  subsector_t *EndSubsector;
  // the following fields are valid only if trace was failed
  TVec HitPlaneNormal;
  TPlane HitPlane;
  line_t *HitLine; // can be `nullptr` if we hit a floor/ceiling
  enum {
    SightEarlyOut = 1u<<0, // set if hit line is not two-sided
  };
  vuint32 Flags;
//private:
  TPlane LinePlane; // vertical plane for (Start,End), used only for line checks; may be undefined
};


struct VStateCall {
  VEntity *Item;
  VState *State;
  vuint8 Result; // `0` means "don't change"; non-zero means "succeed"
};


struct VMapMarkerInfo {
  vint32 id; // for convenienience; -1 means "unused"
  float x, y;
  sector_t *sector; // marker sector
  vint32 thingTid; // follow this thing (0: none)
  enum {
    F_Visible = 1u<<0,
    F_Active  = 1u<<1,
  };
  vuint32 flags;
};



//==========================================================================
//
//  VGameObject
//
//==========================================================================
class VGameObject : public VObject {
  DECLARE_CLASS(VGameObject, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VGameObject)

public:
  VObject *_stateRouteSelf; // used to replace state self for uservars if not none

  // WARNING: keep in sync with VC code!
  enum UserVarFieldType {
    None, // field is missing, or type is invalid
    Int,
    Float,
    IntArray,
    FloatArray,
  };

  // -0 index is ok for non-arrays
  int _get_user_var_int (VName fldname, int index=0);
  float _get_user_var_float (VName fldname, int index=0);

  void _set_user_var_int (VName fldname, int value, int index=0);
  void _set_user_var_float (VName fldname, float value, int index=0);

  UserVarFieldType _get_user_var_type (VName fldname);
  int _get_user_var_dim (VName fldname); // array dimension; -1: not an array, or absent

  DECLARE_FUNCTION(_get_user_var_int)
  DECLARE_FUNCTION(_get_user_var_float)
  DECLARE_FUNCTION(_set_user_var_int)
  DECLARE_FUNCTION(_set_user_var_float)
  DECLARE_FUNCTION(_get_user_var_type)
  DECLARE_FUNCTION(_get_user_var_dim)

  DECLARE_FUNCTION(spGetNormal)
  DECLARE_FUNCTION(spGetNormalZ)
  DECLARE_FUNCTION(spGetDist)
  DECLARE_FUNCTION(spGetPointZ)
  DECLARE_FUNCTION(spDotPoint)
  DECLARE_FUNCTION(spDotPointDist)
  DECLARE_FUNCTION(spPointOnSide)
  DECLARE_FUNCTION(spPointOnSideThreshold)
  DECLARE_FUNCTION(spPointOnSideFri)
  DECLARE_FUNCTION(spPointOnSide2)
  DECLARE_FUNCTION(SphereOnSide)
  DECLARE_FUNCTION(spSphereTouches)
  DECLARE_FUNCTION(spSphereOnSide)
  DECLARE_FUNCTION(spSphereOnSide2)

  DECLARE_FUNCTION(GetPointZClamped)
  DECLARE_FUNCTION(GetPointZRevClamped)

  DECLARE_FUNCTION(GetSectorFloorPointZ)
  DECLARE_FUNCTION(SectorHas3DFloors)

  DECLARE_FUNCTION(CheckPlanePass)
  DECLARE_FUNCTION(CheckPassPlanes)
};
