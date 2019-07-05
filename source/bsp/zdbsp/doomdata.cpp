// ////////////////////////////////////////////////////////////////////////// //
FLevel::FLevel () {
  memset((void *)this, 0, sizeof(*this)); // shut the fuck up, gshitcc
}


FLevel::~FLevel () {
  if (Vertices) delete[] Vertices;
  if (Subsectors) delete[] Subsectors;
  if (Segs) delete[] Segs;
  if (Nodes) delete[] Nodes;
  if (Blockmap) delete[] Blockmap;
  if (Reject) delete[] Reject;
  if (GLSubsectors) delete[] GLSubsectors;
  if (GLSegs) delete[] GLSegs;
  if (GLNodes) delete[] GLNodes;
  //if (GLPVS) delete[] GLPVS;
  if (OrgSectorMap) delete[] OrgSectorMap;
}


void FLevel::FindMapBounds () {
  fixed_t minx, maxx, miny, maxy;

  minx = maxx = Vertices[0].x;
  miny = maxy = Vertices[0].y;

  for (int i = 1; i < NumVertices; ++i) {
         if (Vertices[i].x < minx) minx = Vertices[i].x;
    else if (Vertices[i].x > maxx) maxx = Vertices[i].x;
         if (Vertices[i].y < miny) miny = Vertices[i].y;
    else if (Vertices[i].y > maxy) maxy = Vertices[i].y;
  }

  MinX = minx;
  MinY = miny;
  MaxX = maxx;
  MaxY = maxy;
}


void FLevel::RemoveExtraLines () {
  int i, newNumLines;

  // Extra lines are those with 0 length. Collision detection against
  // one of those could cause a divide by 0, so it's best to remove them.

  for (i = newNumLines = 0; i < NumLines(); ++i) {
    if (Vertices[Lines[i].v1].x != Vertices[Lines[i].v2].x ||
        Vertices[Lines[i].v1].y != Vertices[Lines[i].v2].y)
    {
      if (i != newNumLines) Lines[newNumLines] = Lines[i];
      ++newNumLines;
    }
  }
  if (newNumLines < NumLines()) {
    int diff = NumLines() - newNumLines;
    ZDWarn("   Removed %d line%s with 0 length.\n", diff, diff > 1 ? "s" : "");
  }
  Lines.Resize(newNumLines);
}


void FLevel::RemoveExtraSides () {
  BYTE *used;
  int *remap;
  int i, newNumSides;

  // Extra sides are those that aren't referenced by any lines.
  // They just waste space, so get rid of them.
  int NumSides = this->NumSides();

  used = new BYTE[NumSides];
  memset (used, 0, NumSides*sizeof(*used));
  remap = new int[NumSides];

  // Mark all used sides
  for (i = 0; i < NumLines(); ++i) {
    if (Lines[i].sidenum[0] != NO_INDEX) {
      used[Lines[i].sidenum[0]] = 1;
    } else {
      ZDWarn("   Line %d needs a front sidedef.\n", i);
    }
    if (Lines[i].sidenum[1] != NO_INDEX) used[Lines[i].sidenum[1]] = 1;
  }

  // Shift out any unused sides
  for (i = newNumSides = 0; i < NumSides; ++i) {
    if (used[i]) {
      if (i != newNumSides) Sides[newNumSides] = Sides[i];
      remap[i] = newNumSides++;
    } else {
      remap[i] = NO_INDEX;
    }
  }

  if (newNumSides < NumSides) {
    int diff = NumSides - newNumSides;

    ZDWarn("   Removed %d unused sidedef%s.\n", diff, diff > 1 ? "s" : "");
    Sides.Resize(newNumSides);

    // Renumber side references in lines
    for (i = 0; i < NumLines(); ++i) {
      if (Lines[i].sidenum[0] != NO_INDEX) Lines[i].sidenum[0] = remap[Lines[i].sidenum[0]];
      if (Lines[i].sidenum[1] != NO_INDEX) Lines[i].sidenum[1] = remap[Lines[i].sidenum[1]];
    }
  }
  delete[] used;
  delete[] remap;
}


void FLevel::RemoveExtraSectors () {
  BYTE *used;
  DWORD *remap;
  int i, newNumSectors;

  // Extra sectors are those that aren't referenced by any sides.
  // They just waste space, so get rid of them.

  NumOrgSectors = NumSectors();
  used = new BYTE[NumSectors()];
  memset (used, 0, NumSectors()*sizeof(*used));
  remap = new DWORD[NumSectors()];

  // Mark all used sectors
  for (i = 0; i < NumSides(); ++i) {
    if ((DWORD)Sides[i].sector != NO_INDEX) {
      used[Sides[i].sector] = 1;
    } else {
      ZDWarn("   Sidedef %d needs a front sector.\n", i);
    }
  }

  // Shift out any unused sides
  for (i = newNumSectors = 0; i < NumSectors(); ++i) {
    if (used[i]) {
      if (i != newNumSectors) Sectors[newNumSectors] = Sectors[i];
      remap[i] = newNumSectors++;
    } else {
      remap[i] = NO_INDEX;
    }
  }

  if (newNumSectors < NumSectors()) {
    int diff = NumSectors() - newNumSectors;
    ZDWarn("   Removed %d unused sector%s.\n", diff, diff > 1 ? "s" : "");

    // Renumber sector references in sides
    for (i = 0; i < NumSides(); ++i) {
      if ((DWORD)Sides[i].sector != NO_INDEX) Sides[i].sector = remap[Sides[i].sector];
    }
    // Make a reverse map for fixing reject lumps
    OrgSectorMap = new DWORD[newNumSectors];
    for (i = 0; i < NumSectors(); ++i) {
      if (remap[i] != NO_INDEX) OrgSectorMap[remap[i]] = i;
    }

    Sectors.Resize(newNumSectors);
  }

  delete[] used;
  delete[] remap;
}
