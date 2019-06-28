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
//**    Build rough PVS
//**
//**************************************************************************

static int numOfThreads = -1;


// ////////////////////////////////////////////////////////////////////////// //
// 2d vector
class TVec2D {
public:
  double x;
  double y;

  TVec2D () {}
  TVec2D (double Ax, double Ay) { x = Ax; y = Ay; }
  TVec2D (const double f[2]) { x = f[0]; y = f[1]; }
  TVec2D (const TVec &v) { x = v.x; y = v.y; }

  inline const double &operator [] (int i) const { return (&x)[i]; }
  inline double &operator [] (int i) { return (&x)[i]; }

  inline TVec2D &operator += (const TVec2D &v) { x += v.x; y += v.y; return *this; }
  inline TVec2D &operator -= (const TVec2D &v) { x -= v.x; y -= v.y; return *this; }
  inline TVec2D &operator *= (double scale) { x *= scale; y *= scale; return *this; }
  inline TVec2D &operator /= (double scale) { x /= scale; y /= scale; return *this; }
  inline TVec2D operator + () const { return *this; }
  inline TVec2D operator - () const { return TVec2D(-x, -y); }
  inline double Length () const { return sqrt(x*x+y*y); }
};

static inline TVec2D operator + (const TVec2D &v1, const TVec2D &v2) { return TVec2D(v1.x+v2.x, v1.y+v2.y); }
static inline TVec2D operator - (const TVec2D &v1, const TVec2D &v2) { return TVec2D(v1.x-v2.x, v1.y-v2.y); }
static inline TVec2D operator * (const TVec2D &v, double s) { return TVec2D(s*v.x, s*v.y); }
static inline TVec2D operator * (double s, const TVec2D &v) { return TVec2D(s*v.x, s*v.y); }
static inline TVec2D operator / (const TVec2D &v, double s) { return TVec2D(v.x/s, v.y/s); }
static inline bool operator == (const TVec2D &v1, const TVec2D &v2) { return (v1.x == v2.x && v1.y == v2.y); }
static inline bool operator != (const TVec2D &v1, const TVec2D &v2) { return (v1.x != v2.x || v1.y != v2.y); }
static inline double Length (const TVec2D &v) { return sqrt(v.x*v.x+v.y*v.y); }
static inline TVec2D Normalise (const TVec2D &v) { return v/v.Length(); }
static inline double DotProduct (const TVec2D &v1, const TVec2D &v2) { return v1.x*v2.x+v1.y*v2.y; }


// ////////////////////////////////////////////////////////////////////////// //
// 2d plane (lol)
class TPlane2D {
public:
  TVec2D normal;
  double dist;

  inline void Set (const TVec2D &Anormal, double Adist) { normal = Anormal; dist = Adist; }

  // Initialises vertical plane from point and direction
  inline void SetPointDirXY (const TVec2D &point, const TVec2D &dir) {
    normal = Normalise(TVec2D(dir.y, -dir.x));
    dist = DotProduct(point, normal);
  }

  // Initialises vertical plane from 2 points
  inline void Set2Points (const TVec2D &v1, const TVec2D &v2) {
    SetPointDirXY(v1, v2-v1);
  }
};



// ////////////////////////////////////////////////////////////////////////// //
// simple PVS
#define MAX_PORTALS_ON_LEAF   (512*4)
#define ON_EPSILON  (0.001)


struct winding_t {
  bool original; // don't free, it's part of the portal
  TVec2D points[2];

  inline bool checkPositiveDist (const TPlane2D &p) const {
    for (int k = 0; k < 2; ++k) {
      const double d = DotProduct(points[k], p.normal)-p.dist;
      if (d > ON_EPSILON) return true;
    }
    return false; // no points on front
  }

  inline bool checkNegativeDist (const TPlane2D &p) const {
    for (int k = 0; k < 2; ++k) {
      const double d = DotProduct(points[k], p.normal)-p.dist;
      if (d < -ON_EPSILON) return true;
    }
    return false; // no points on front
  }
};

// normal pointing into neighbor
struct portal_t : TPlane2D {
  int leaf; // neighbor
  winding_t winding;
  vuint8 *visbits;
  vuint8 *mightsee;
};

struct subsec_extra_t {
  portal_t *portals[MAX_PORTALS_ON_LEAF];
  int numportals;
};

struct PVSInfo {
  int numportals;
  int NumSegs;
  portal_t *portals;
  int bitbytes;
  int bitlongs;
  int rowbytes;
  int *leaves; // NumSegs
  subsec_extra_t *ssex; // NumSubsectors
  // temp, don't free; bitmap
private:
  vuint8 *portalsee;

public:
  PVSInfo ()
    : numportals(0)
    , NumSegs(0)
    , portals(nullptr)
    , bitbytes(0)
    , bitlongs(0)
    , rowbytes(0)
    , leaves(nullptr)
    , ssex(nullptr)
    , portalsee(nullptr)
  {}

  PVSInfo (const PVSInfo &a)
    : numportals(a.numportals)
    , NumSegs(0)
    , portals(a.portals)
    , bitbytes(a.bitbytes)
    , bitlongs(a.bitlongs)
    , rowbytes(a.rowbytes)
    , leaves(a.leaves)
    , ssex(a.ssex)
    , portalsee(nullptr)
  {}

  inline void operator = (const PVSInfo &a) {
    numportals = a.numportals;
    NumSegs = a.NumSegs;
    portals = a.portals;
    bitbytes = a.bitbytes;
    bitlongs = a.bitlongs;
    rowbytes = a.rowbytes;
    leaves = a.leaves;
    ssex = a.ssex;
    portalsee = nullptr;
  }

  void createPortalSee () {
    check(!portalsee);
    portalsee = new vuint8[(numportals+7)/8];
    memset(portalsee, 0, (numportals+7)/8);
  }

  void deletePortalSee () {
    delete [] portalsee;
    portalsee = nullptr;
  }

  void clearPortalSee () {
    check(portalsee);
    memset(portalsee, 0, (numportals+7)/8);
  }

  inline void setPortalSee (int pidx) {
    check(pidx >= 0 && pidx < numportals);
    check(portalsee);
    portalsee[pidx/8] |= (vuint8)(1u<<(pidx&7));
  }

  inline bool getPortalSee (int pidx) const {
    check(pidx >= 0 && pidx < numportals);
    check(portalsee);
    return !!(portalsee[pidx/8]&(vuint8)(1u<<(pidx&7)));
  }
};


enum { PVSThreadMax = 64 };

struct PVSThreadInfo {
  PVSInfo nfo;
  mythread trd;
  bool created;
};

static PVSThreadInfo pvsTreadList[PVSThreadMax];
static int pvsNextPortal, pvsMaxPortals, pvsLastReport, pvsNumSegs; // next portal to process
static mythread_mutex pvsNPLock;
static double pvsLastReportTime;
static double pvsReportTimeout;
static int pvsReportPNum;

static mythread_mutex pvsPingLock;
static mythread_cond pvsPingCond;
static int pvsPBarCur, pvsPBarMax;
static bool pvsPBarDone;


static int getNextPortalNum () {
  mythread_mutex_lock(&pvsNPLock);
  int res = pvsNextPortal++;
  if (res-pvsLastReport >= pvsReportPNum) {
    pvsLastReport = res;
    const double tt = Sys_Time();
    if (tt-pvsLastReportTime >= pvsReportTimeout) {
      pvsLastReportTime = tt;
      mythread_mutex_lock(&pvsPingLock);
      pvsPBarCur = res;
      if (pvsPBarCur >= pvsMaxPortals) {
        pvsPBarCur = pvsMaxPortals;
        pvsPBarDone = true;
      }
      pvsPBarMax = pvsMaxPortals;
      // signal...
      mythread_cond_signal(&pvsPingCond);
      // ...and unlock
      mythread_mutex_unlock(&pvsPingLock);
    }
  } else {
    // we HAVE to report "completion", or main thread will hang
    if (res >= pvsMaxPortals) {
      mythread_mutex_lock(&pvsPingLock);
      pvsPBarDone = true;
      pvsPBarCur = res;
      if (pvsPBarCur > pvsMaxPortals) pvsPBarCur = pvsMaxPortals;
      pvsPBarMax = pvsMaxPortals;
      // signal...
      mythread_cond_signal(&pvsPingCond);
      // ...and unlock
      mythread_mutex_unlock(&pvsPingLock);
    }
  }
  mythread_mutex_unlock(&pvsNPLock);
  return res;
}


extern "C" {
  static void SimpleFlood (portal_t *srcportal, int leafnum, PVSInfo *nfo) {
    if (srcportal->mightsee[leafnum>>3]&(1<<(leafnum&7))) return;
    srcportal->mightsee[leafnum>>3] |= (1<<(leafnum&7));

    subsec_extra_t *leaf = &nfo->ssex[leafnum];
    for (int i = 0; i < leaf->numportals; ++i) {
      portal_t *p = leaf->portals[i];
      if (!nfo->getPortalSee((int)(ptrdiff_t)(p-nfo->portals))) continue;
      SimpleFlood(srcportal, p->leaf, nfo);
    }
  }


  static MYTHREAD_RET_TYPE pvsThreadWorker (void *aarg) {
    PVSInfo *nfo = (PVSInfo *)aarg;
    nfo->createPortalSee();
    for (;;) {
      int pnum = getNextPortalNum();
      if (pnum >= nfo->numportals) break;
      portal_t *p = &nfo->portals[pnum];

      p->mightsee = new vuint8[nfo->bitbytes];
      memset(p->mightsee, 0, nfo->bitbytes);

      nfo->clearPortalSee();
      portal_t *tp = nfo->portals;
      for (int j = 0; j < nfo->numportals; ++j, ++tp) {
        if (j == pnum) continue;
        if (!tp->winding.checkPositiveDist(*p)) continue;
        if (!p->winding.checkNegativeDist(*tp)) continue;
        nfo->setPortalSee(j);
      }

      SimpleFlood(p, p->leaf, nfo);

      p->visbits = p->mightsee;
    }
    nfo->deletePortalSee();
    Z_ThreadDone();
    return MYTHREAD_RET_VALUE;
  }
}


static void pvsStartThreads (const PVSInfo &anfo) {
  pvsNextPortal = 0;
  pvsLastReport = 0;
  pvsMaxPortals = anfo.numportals;
  pvsNumSegs = anfo.NumSegs;
  pvsPBarDone = false;
  int pvsThreadsToUse = /*loader_pvs_builder_threads*/numOfThreads;
  if (pvsThreadsToUse < 1) pvsThreadsToUse = 1; else if (pvsThreadsToUse > PVSThreadMax) pvsThreadsToUse = PVSThreadMax;

#ifdef CLIENT
  pvsReportTimeout = (Drawer && Drawer->IsInited() ? 0.05 : 2.5);
  pvsReportPNum = (Drawer && Drawer->IsInited() ? 32 : 512);
#else
  pvsReportTimeout = 2.5;
  pvsReportPNum = 512;
#endif

  mythread_mutex_init(&pvsNPLock);
  mythread_mutex_init(&pvsPingLock);
  mythread_cond_init(&pvsPingCond);

  // lock it here, so we won't miss any updates
  mythread_mutex_lock(&pvsPingLock);

  int ccount = 0;
  pvsLastReportTime = Sys_Time();
  for (int f = 0; f < pvsThreadsToUse; ++f) {
    pvsTreadList[f].nfo = anfo;
    pvsTreadList[f].created = (mythread_create(&pvsTreadList[f].trd, &pvsThreadWorker, &pvsTreadList[f].nfo) == 0);
    if (pvsTreadList[f].created) ++ccount;
  }
  if (ccount == 0) Sys_Error("Cannot create PVS worker threads");

  // loop until we'll get at least one complete progress
  bool wasProgress = false;
  for (;;) {
    mythread_cond_wait(&pvsPingCond, &pvsPingLock);
    // got one!
    bool done = pvsPBarDone;
    if (done && !wasProgress) break; // don't spam with progress messages
    //pvsDrawPBar(pvsPBarCur, pvsPBarMax);
#ifdef CLIENT
    R_PBarUpdate("PVS", pvsNumSegs+pvsPBarCur, pvsNumSegs+pvsPBarMax);
#endif
    wasProgress = true;
    if (done) break;
  }
  mythread_mutex_unlock(&pvsPingLock);

#ifdef CLIENT
  //if (wasProgress && Drawer && Drawer->IsInited()) pvsDrawPBar(42, 42);
  R_PBarUpdate("PVS", 42, 42, true); // final update
#endif

  // wait for all threads to complete
  for (int f = 0; f < pvsThreadsToUse; ++f) {
    if (pvsTreadList[f].created) mythread_join(pvsTreadList[f].trd);
  }

  mythread_mutex_destroy(&pvsNPLock);
  mythread_mutex_destroy(&pvsPingLock);
  mythread_cond_destroy(&pvsPingCond);

  if (pvsNextPortal < anfo.numportals) Sys_Error("PVS worker threads gone ape");
}


//==========================================================================
//
//  VLevel::BuildPVS
//
//==========================================================================
void VLevel::BuildPVS () {
  if (!loader_build_pvs || (nodes_builder && !loader_build_pvs_force)) {
    if (nodes_builder && loader_build_pvs) GCon->Logf(NAME_Warning, "skipped PVS building due to problems with node builder (only AJBSP is supported for now)");
    VisData = nullptr;
    NoVis = new vuint8[(NumSubsectors+7)/8];
    memset(NoVis, 0xff, (NumSubsectors+7)/8);
    return;
  }

  GCon->Logf("building PVS...");
#ifdef CLIENT
  R_LdrMsgShowSecondary("BUILDING PVS...");
  R_PBarReset();
#endif

  PVSInfo nfo;
  memset((void *)&nfo, 0, sizeof(nfo));

  nfo.bitbytes = ((NumSubsectors+63)&~63)>>3;
  nfo.bitlongs = nfo.bitbytes/sizeof(long);
  nfo.rowbytes = (NumSubsectors+7)>>3;

  //nfo.secnums = new int[NumSubsectors+1];
  nfo.leaves = new int[NumSegs+1];
  nfo.ssex = new subsec_extra_t[NumSubsectors+1];

  {
    subsector_t *ss = Subsectors;
    for (int i = 0; i < NumSubsectors; ++i, ++ss) {
      nfo.ssex[i].numportals = 0;
      // set seg subsector links
      int count = ss->numlines;
      int ln = ss->firstline;
      while (count--) nfo.leaves[ln++] = i;
    }
  }

  bool ok = CreatePortals(&nfo);

  if (ok) {
    numOfThreads = loader_pvs_builder_threads;
    if (numOfThreads <= 0) numOfThreads = Sys_GetCPUCount();
    if (numOfThreads > PVSThreadMax) numOfThreads = PVSThreadMax;
    GCon->Logf("using %d thread%s for PVS builder", numOfThreads, (numOfThreads != 1 ? "s" : ""));
    //if (numOfThreads < 2) numOfThreads = 2; // it looks better this way
    nfo.NumSegs = NumSegs/100;
    if (numOfThreads > 1) {
      pvsStartThreads(nfo);
    } else {
      BasePortalVis(&nfo);
    }
    // assemble the leaf vis lists by oring and compressing the portal lists
    //totalvis = 0;
    int vissize = nfo.rowbytes*NumSubsectors;
    //vis = new vuint8[vissize];
    VisData = new vuint8[vissize];
    memset(VisData, 0, vissize);
    for (int i = 0; i < NumSubsectors; ++i) {
      if (!LeafFlow(i, &nfo)) { ok = false; break; }
    }
    NoVis = nullptr;
    if (!ok) {
      delete [] VisData;
      VisData = nullptr;
    }
  }

  if (!ok) {
    GCon->Logf(NAME_Warning, "PVS building failed.");
    VisData = nullptr;
    NoVis = new vuint8[(NumSubsectors+7)/8];
    memset(NoVis, 0xff, (NumSubsectors+7)/8);
  } else {
    GCon->Logf("PVS building (rough) complete (%d bytes).", nfo.rowbytes*NumSubsectors);
  }

  for (int i = 0; i < nfo.numportals; ++i) {
    delete [] nfo.portals[i].mightsee;
  }
  //delete [] nfo.secnums;
  delete [] nfo.leaves;
  delete [] nfo.ssex;
  delete [] nfo.portals;

  // for zdbsp, we should check if both given subsectors are mutually visible
  // for some reason, zdbsp creates subsectors for "inner sectors" that
  // breaks PVS. i have to investigate the cause and write a real fix, but
  // for now, let's do it this way.
  if (ok && nodes_builder) {
    int mtfixcount = 0;
    GCon->Log("fixind pvs...");
    const int sslen = NumSubsectors;
    const int vslen = (sslen+7)>>3;
    vuint8 *vd0 = VisData;
    for (int s0idx = 0; s0idx < sslen; ++s0idx, vd0 += vslen) {
      vuint8 *vd1 = VisData;
      for (int s1idx = 0; s1idx < sslen; ++s1idx, vd1 += vslen) {
        if (vd0[s1idx>>3]&(1<<(s1idx&7))) {
          if ((vd1[s0idx>>3]&(1<<(s0idx&7))) == 0) {
            //check(s0idx != s1idx);
            //GCon->Logf("subsector #%d can see subsector #%d, but not vice versa", s0idx, s1idx);
            vd1[s0idx>>3] |= (1<<(s0idx&7));
            ++mtfixcount;
          }
        } else if (vd1[s0idx>>3]&(1<<(s0idx&7))) {
          if ((vd0[s1idx>>3]&(1<<(s1idx&7))) == 0) {
            vd0[s1idx>>3] |= (1<<(s1idx&7));
            ++mtfixcount;
          }
        }
      }
    }
    GCon->Logf("pvs fixing complete, %d fixes done", mtfixcount);
  }
}

/* k8: no need to rebuild reject table, as PVS is used in every place where
       reject is checked, and it provides better granularity anyway.
 */

/*REJECT: rebuild it with PVS
    auto sub = Level->PointInSubsector(cl->ViewOrg);
    const vuint8 *dyn_facevis = Level->LeafPVS(sub);
    auto leafnum = Level->PointInSubsector(lorg)-Level->Subsectors;
    // check potential visibility
    if (!(dyn_facevis[leafnum>>3]&(1<<(leafnum&7)))) {
      //fprintf(stderr, "DYNLIGHT rejected by PVS\n");
      return nullptr;
    }


  if (XLevel->RejectMatrix)
  {
    int       s1;
    int       pnum;
    //  Determine subsector entries in REJECT table.
    //  We must do this because REJECT can have some special effects like
    // "safe sectors"
    s1 = Sector - XLevel->Sectors;
    s2 = Other->Sector - XLevel->Sectors;
    pnum = s1 * XLevel->NumSectors + s2;
    // Check in REJECT table.
    if (XLevel->RejectMatrix[pnum >> 3] & (1 << (pnum & 7)))
    {
      // can't possibly be connected
      return false;
    }
  }
*/


//==========================================================================
//
//  VLevel::CreatePortals
//
//==========================================================================
bool VLevel::CreatePortals (void *pvsinfo) {
  PVSInfo *nfo = (PVSInfo *)pvsinfo;

  nfo->numportals = 0;
  seg_t *seg = &Segs[0];
  for (int f = 0; f < NumSegs; ++f, ++seg) {
    if (seg->partner) ++nfo->numportals;
  }
  const int origpcount = nfo->numportals;

  if (nfo->numportals == 0) {
    GCon->Logf(NAME_Warning, "PVS: no possible portals found");
    return false;
  }
  //GCon->Logf("PVS: %d partners", nfo->numportals);

  nfo->portals = new portal_t[nfo->numportals];
  for (int i = 0; i < nfo->numportals; ++i) {
    nfo->portals[i].visbits = nullptr;
    nfo->portals[i].mightsee = nullptr;
  }

#ifdef CLIENT
  R_PBarUpdate("PVS", 0, NumSegs/100+nfo->numportals);
#endif

  portal_t *p = nfo->portals;
  seg = &Segs[0];
  for (int i = 0; i < NumSegs; ++i, ++seg) {
    //subsector_t *sub = &Subsectors[line->leaf];
    //subsector_t *sub = &Subsectors[nfo->leaves[i]];
    if (seg->partner) {
      subsec_extra_t *sub = &nfo->ssex[nfo->leaves[i]];
      int pnum = (int)(ptrdiff_t)(seg->partner-Segs);

      // skip self-referencing subsector segs
      if (/*line->leaf == line->partner->leaf*/nfo->leaves[i] == nfo->leaves[pnum]) {
        //GCon->Logf("Self-referencing subsector detected (%d)", i);
        --nfo->numportals;
      } else {
        // create portal
        if (sub->numportals == MAX_PORTALS_ON_LEAF) {
          //throw GLVisError("Leaf with too many portals");
          GCon->Logf(NAME_Warning, "PVS: Leaf with too many portals!");
          return false;
        }
        sub->portals[sub->numportals++] = p;

        p->winding.original = true;
        p->winding.points[0] = *seg->v1;
        p->winding.points[1] = *seg->v2;
        p->normal = seg->partner->normal;
        p->dist = seg->partner->dist;
        //p->leaf = line->partner->leaf;
        p->leaf = nfo->leaves[pnum];
        ++p;
      }
    }
#ifdef CLIENT
    R_PBarUpdate("PVS", (i+1)/100, NumSegs/100+nfo->numportals);
#endif
  }

  if (origpcount != nfo->numportals) {
    GCon->Logf("PVS: %d portals found (%d portals dropped)", nfo->numportals, origpcount-nfo->numportals);
  } else {
    GCon->Logf("PVS: %d portals found", nfo->numportals);
  }

  //if (p-portals != numportals) throw GLVisError("Portals miscounted");
  return (nfo->numportals > 0);
}


//==========================================================================
//
//  VLevel::SimpleFlood
//
//==========================================================================
void VLevel::SimpleFlood (/*portal_t*/void *srcportalp, int leafnum, void *pvsinfo) {
  PVSInfo *nfo = (PVSInfo *)pvsinfo;
  portal_t *srcportal = (portal_t *)srcportalp;

  if (srcportal->mightsee[leafnum>>3]&(1<<(leafnum&7))) return;
  srcportal->mightsee[leafnum>>3] |= (1<<(leafnum&7));
  //++nfo->c_leafsee;

  //leaf_t *leaf = &subsectors[leafnum];
  subsec_extra_t *leaf = &nfo->ssex[leafnum];
  for (int i = 0; i < leaf->numportals; ++i) {
    portal_t *p = leaf->portals[i];
    if (!nfo->getPortalSee((int)(ptrdiff_t)(p-nfo->portals))) continue;
    SimpleFlood(srcportal, p->leaf, pvsinfo);
  }
}


//==========================================================================
//
//  VLevel::BasePortalVis
//
//  This is a rough first-order aproximation that is used to trivially
//  reject some of the final calculations.
//
//==========================================================================
void VLevel::BasePortalVis (void *pvsinfo) {
  PVSInfo *nfo = (PVSInfo *)pvsinfo;

  nfo->createPortalSee();
  portal_t *p = nfo->portals;
  for (int pnum = 0; pnum < nfo->numportals; ++pnum, ++p) {
    p->mightsee = new vuint8[nfo->bitbytes];
    memset(p->mightsee, 0, nfo->bitbytes);

    nfo->clearPortalSee();
    portal_t *tp = nfo->portals;
    for (int j = 0; j < nfo->numportals; ++j, ++tp) {
      if (j == pnum) continue;
      if (!tp->winding.checkPositiveDist(*p)) continue;
      if (!p->winding.checkNegativeDist(*tp)) continue;
      nfo->setPortalSee(j);
    }

    SimpleFlood(p, p->leaf, pvsinfo);

    p->visbits = p->mightsee;

#ifdef CLIENT
    R_PBarUpdate("PVS", NumSegs/100+pnum, NumSegs/100+nfo->numportals);
#endif
  }
  nfo->deletePortalSee();
}


//==========================================================================
//
//  VLevel::LeafFlow
//
//  Builds the entire visibility list for a leaf
//
//==========================================================================
bool VLevel::LeafFlow (int leafnum, void *pvsinfo) {
  PVSInfo *nfo = (PVSInfo *)pvsinfo;

  // flow through all portals, collecting visible bits
  vuint8 *outbuffer = VisData+leafnum*nfo->rowbytes;
  subsec_extra_t *leaf = &nfo->ssex[leafnum];
  for (int i = 0; i < leaf->numportals; ++i) {
    portal_t *p = leaf->portals[i];
    if (p == nullptr) continue;
    for (int j = 0; j < nfo->rowbytes; ++j) {
      if (p->visbits[j] == 0) continue;
      outbuffer[j] |= p->visbits[j];
    }
  }

  if (outbuffer[leafnum>>3]&(1<<(leafnum&7))) {
    //k8: so what?
    GCon->Logf(NAME_Warning, "Leaf %d portals saw into leaf", leafnum);
    //return false;
  }

  outbuffer[leafnum>>3] |= (vuint8)(1u<<(leafnum&7));

  return true;
}
