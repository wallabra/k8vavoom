//------------------------------------------------------------------------
//
//  AJ-BSP  Copyright (C) 2000-2018  Andrew Apted, et al
//          Copyright (C) 1994-1998  Colin Reed
//          Copyright (C) 1997-1998  Lee Killough
//
//  Originally based on the program 'BSP', version 2.3.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#include "main.h"
#include "../../libs/core/timsort.h"

namespace ajbsp
{

#define DEBUG_BLOCKMAP  0

nodebuildinfo_t * cur_info = NULL;

static int block_x, block_y;
static int block_w, block_h;
static int block_count;

static int block_mid_x = 0;
static int block_mid_y = 0;

static u16_t ** block_lines;

static u16_t *block_ptrs;
static u16_t *block_dups;

static int block_compression;
static int block_overflowed;

#define BLOCK_LIMIT  16000

#define DUMMY_DUP  0xFFFF


void GetBlockmapBounds(int *x, int *y, int *w, int *h)
{
	*x = block_x; *y = block_y;
	*w = block_w; *h = block_h;
}


int CheckLinedefInsideBox(int xmin, int ymin, int xmax, int ymax,
		int x1, int y1, int x2, int y2)
{
	int count = 2;
	int tmp;

	for (;;)
	{
		if (y1 > ymax)
		{
			if (y2 > ymax)
				return false;

			x1 = x1 + (int) ((x2-x1) * (double)(ymax-y1) / (double)(y2-y1));
			y1 = ymax;

			count = 2;
			continue;
		}

		if (y1 < ymin)
		{
			if (y2 < ymin)
				return false;

			x1 = x1 + (int) ((x2-x1) * (double)(ymin-y1) / (double)(y2-y1));
			y1 = ymin;

			count = 2;
			continue;
		}

		if (x1 > xmax)
		{
			if (x2 > xmax)
				return false;

			y1 = y1 + (int) ((y2-y1) * (double)(xmax-x1) / (double)(x2-x1));
			x1 = xmax;

			count = 2;
			continue;
		}

		if (x1 < xmin)
		{
			if (x2 < xmin)
				return false;

			y1 = y1 + (int) ((y2-y1) * (double)(xmin-x1) / (double)(x2-x1));
			x1 = xmin;

			count = 2;
			continue;
		}

		count--;

		if (count == 0)
			break;

		// swap end points
		tmp=x1;  x1=x2;  x2=tmp;
		tmp=y1;  y1=y2;  y2=tmp;
	}

	// linedef touches block
	return true;
}


/* ----- create blockmap ------------------------------------ */

#define BK_NUM    0
#define BK_MAX    1
#define BK_XOR    2
#define BK_FIRST  3

#define BK_QUANTUM  32

static void BlockAdd(int blk_num, int line_index)
{
	u16_t *cur = block_lines[blk_num];

# if DEBUG_BLOCKMAP
	ajbsp_DebugPrintf("Block %d has line %d\n", blk_num, line_index);
# endif

	if (blk_num < 0 || blk_num >= block_count)
		ajbsp_BugError("BlockAdd: bad block number %d\n", blk_num);

	if (! cur)
	{
		// create empty block
		block_lines[blk_num] = cur = (u16_t *)UtilCalloc(BK_QUANTUM * sizeof(u16_t));
		cur[BK_NUM] = 0;
		cur[BK_MAX] = BK_QUANTUM;
		cur[BK_XOR] = 0x1234;
	}

	if (BK_FIRST + cur[BK_NUM] == cur[BK_MAX])
	{
		// no more room, so allocate some more...
		cur[BK_MAX] += BK_QUANTUM;

		block_lines[blk_num] = cur = (u16_t *)UtilRealloc(cur, cur[BK_MAX] * sizeof(u16_t));
	}

	// compute new checksum
	cur[BK_XOR] = ((cur[BK_XOR] << 4) | (cur[BK_XOR] >> 12)) ^ line_index;

	cur[BK_FIRST + cur[BK_NUM]] = LE_U16(line_index);
	cur[BK_NUM]++;
}


static void BlockAddLine(linedef_t *L)
{
	int x1 = (int) L->start->x;
	int y1 = (int) L->start->y;
	int x2 = (int) L->end->x;
	int y2 = (int) L->end->y;

	int bx1 = (MIN2(x1,x2) - block_x) / 128;
	int by1 = (MIN2(y1,y2) - block_y) / 128;
	int bx2 = (MAX2(x1,x2) - block_x) / 128;
	int by2 = (MAX2(y1,y2) - block_y) / 128;

	int bx, by;
	int line_index = L->index;

# if DEBUG_BLOCKMAP
	ajbsp_DebugPrintf("BlockAddLine: %d (%d,%d) -> (%d,%d)\n", line_index,
			x1, y1, x2, y2);
# endif

	// handle truncated blockmaps
	if (bx1 < 0) bx1 = 0;
	if (by1 < 0) by1 = 0;
	if (bx2 >= block_w) bx2 = block_w - 1;
	if (by2 >= block_h) by2 = block_h - 1;

	if (bx2 < bx1 || by2 < by1)
		return;

	// handle simple case #1: completely horizontal
	if (by1 == by2)
	{
		for (bx=bx1 ; bx <= bx2 ; bx++)
		{
			int blk_num = by1 * block_w + bx;
			BlockAdd(blk_num, line_index);
		}
		return;
	}

	// handle simple case #2: completely vertical
	if (bx1 == bx2)
	{
		for (by=by1 ; by <= by2 ; by++)
		{
			int blk_num = by * block_w + bx1;
			BlockAdd(blk_num, line_index);
		}
		return;
	}

	// handle the rest (diagonals)

	for (by=by1 ; by <= by2 ; by++)
	for (bx=bx1 ; bx <= bx2 ; bx++)
	{
		int blk_num = by * block_w + bx;

		int minx = block_x + bx * 128;
		int miny = block_y + by * 128;
		int maxx = minx + 127;
		int maxy = miny + 127;

		if (CheckLinedefInsideBox(minx, miny, maxx, maxy, x1, y1, x2, y2))
		{
			BlockAdd(blk_num, line_index);
		}
	}
}


static void CreateBlockmap(void)
{
	int i;

	block_lines = (u16_t **) UtilCalloc(block_count * sizeof(u16_t *));

	for (i=0 ; i < num_linedefs ; i++)
	{
		linedef_t *L = LookupLinedef(i);

		// ignore zero-length lines
		if (L->zero_len)
			continue;

		BlockAddLine(L);
	}
}


extern "C" {
static int BlockCompare(const void *p1, const void *p2, void *udata)
{
	int blk_num1 = ((const u16_t *) p1)[0];
	int blk_num2 = ((const u16_t *) p2)[0];

	const u16_t *A = block_lines[blk_num1];
	const u16_t *B = block_lines[blk_num2];

	if (A == B)
		return 0;

	if (A == NULL) return -1;
	if (B == NULL) return +1;

	if (A[BK_NUM] != B[BK_NUM])
	{
		return A[BK_NUM] - B[BK_NUM];
	}

	if (A[BK_XOR] != B[BK_XOR])
	{
		return A[BK_XOR] - B[BK_XOR];
	}

	return memcmp(A+BK_FIRST, B+BK_FIRST, A[BK_NUM] * sizeof(u16_t));
}
}


static void CompressBlockmap(void)
{
	int i;
	int cur_offset;
	int dup_count=0;

	int orig_size, new_size;

	block_ptrs = (u16_t *)UtilCalloc(block_count * sizeof(u16_t));
	block_dups = (u16_t *)UtilCalloc(block_count * sizeof(u16_t));

	// sort duplicate-detecting array.  After the sort, all duplicates
	// will be next to each other.  The duplicate array gives the order
	// of the blocklists in the BLOCKMAP lump.

	for (i=0 ; i < block_count ; i++)
		block_dups[i] = i;

	//qsort(block_dups, block_count, sizeof(u16_t), BlockCompare);
	timsort_r(block_dups, block_count, sizeof(u16_t), &BlockCompare, nullptr);

	// scan duplicate array and build up offset array

	cur_offset = 4 + block_count + 2;

	orig_size = 4 + block_count;
	new_size  = cur_offset;

	for (i=0 ; i < block_count ; i++)
	{
		int blk_num = block_dups[i];
		int count;

		// empty block ?
		if (block_lines[blk_num] == NULL)
		{
			block_ptrs[blk_num] = 4 + block_count;
			block_dups[i] = DUMMY_DUP;

			orig_size += 2;
			continue;
		}

		count = 2 + block_lines[blk_num][BK_NUM];

		// duplicate ?  Only the very last one of a sequence of duplicates
		// will update the current offset value.

		if (i+1 < block_count &&
				BlockCompare(block_dups + i, block_dups + i+1, nullptr) == 0)
		{
			block_ptrs[blk_num] = cur_offset;
			block_dups[i] = DUMMY_DUP;

			// free the memory of the duplicated block
			UtilFree(block_lines[blk_num]);
			block_lines[blk_num] = NULL;

			dup_count++;

			orig_size += count;
			continue;
		}

		// OK, this block is either the last of a series of duplicates, or
		// just a singleton.

		block_ptrs[blk_num] = cur_offset;

		cur_offset += count;

		orig_size += count;
		new_size  += count;
	}

	if (cur_offset > 65535)
	{
		block_overflowed = true;
		return;
	}

# if DEBUG_BLOCKMAP
	ajbsp_DebugPrintf("Blockmap: Last ptr = %d  duplicates = %d\n",
			cur_offset, dup_count);
# endif

	block_compression = (orig_size - new_size) * 100 / orig_size;

	// there's a tiny chance of new_size > orig_size
	if (block_compression < 0)
		block_compression = 0;
}


static __attribute__((unused)) int CalcBlockmapSize()
{
	// compute size of final BLOCKMAP lump.
	// it does not need to be exact, but it *does* need to be bigger
	// (or equal) to the actual size of the lump.

	// header + null_block + a bit extra
	int size = 20;

	// the pointers (offsets to the line lists)
	size = size + block_count * 2;

	// add size of each block
	for (int i=0 ; i < block_count ; i++)
	{
		int blk_num = block_dups[i];

		// ignore duplicate or empty blocks
		if (blk_num == DUMMY_DUP)
			continue;

		u16_t *blk = block_lines[blk_num];
		SYS_ASSERT(blk);

		size += (1 + (int)(blk[BK_NUM]) + 1) * 2;
	}

	return size;
}


static void WriteBlockmap (VStream &strm)
{
	int i;

	//int max_size = CalcBlockmapSize();

	//Lump_c *lump = CreateLevelLump("BLOCKMAP", max_size);

	u16_t null_block[2] = { 0x0000, 0xFFFF };
	u16_t m_zero = 0x0000;
	u16_t m_neg1 = 0xFFFF;

	// fill in header
	raw_blockmap_header_t header;

	header.x_origin = LE_U16(block_x);
	header.y_origin = LE_U16(block_y);
	header.x_blocks = LE_U16(block_w);
	header.y_blocks = LE_U16(block_h);

	strm.Serialise(&header, sizeof(header));

	// handle pointers
	for (i=0 ; i < block_count ; i++)
	{
		u16_t ptr = LE_U16(block_ptrs[i]);

		if (ptr == 0)
			ajbsp_BugError("WriteBlockmap: offset %d not set.\n", i);

		strm.Serialise(&ptr, sizeof(u16_t));
	}

	// add the null block which *all* empty blocks will use
	strm.Serialise(null_block, sizeof(null_block));

	// handle each block list
	for (i=0 ; i < block_count ; i++)
	{
		int blk_num = block_dups[i];

		// ignore duplicate or empty blocks
		if (blk_num == DUMMY_DUP)
			continue;

		u16_t *blk = block_lines[blk_num];
		SYS_ASSERT(blk);

		strm.Serialise(&m_zero, sizeof(u16_t));
		strm.Serialise(blk + BK_FIRST, blk[BK_NUM] * sizeof(u16_t));
		strm.Serialise(&m_neg1, sizeof(u16_t));
	}
}


static void FreeBlockmap(void)
{
	for (int i=0 ; i < block_count ; i++)
	{
		if (block_lines[i])
			UtilFree(block_lines[i]);
	}

	UtilFree(block_lines);
	UtilFree(block_ptrs);
	UtilFree(block_dups);
}


static void FindBlockmapLimits(bbox_t *bbox)
{
	int i;

	int mid_x = 0;
	int mid_y = 0;

	bbox->minx = bbox->miny = SHRT_MAX;
	bbox->maxx = bbox->maxy = SHRT_MIN;

	for (i=0 ; i < num_linedefs ; i++)
	{
		linedef_t *L = LookupLinedef(i);

		if (! L->zero_len)
		{
			double x1 = L->start->x;
			double y1 = L->start->y;
			double x2 = L->end->x;
			double y2 = L->end->y;

			int lx = (int)floor(MIN2(x1, x2));
			int ly = (int)floor(MIN2(y1, y2));
			int hx = (int)ceil(MAX2(x1, x2));
			int hy = (int)ceil(MAX2(y1, y2));

			if (lx < bbox->minx) bbox->minx = lx;
			if (ly < bbox->miny) bbox->miny = ly;
			if (hx > bbox->maxx) bbox->maxx = hx;
			if (hy > bbox->maxy) bbox->maxy = hy;

			// compute middle of cluster (roughly, so we don't overflow)
			mid_x += (lx + hx) / 32;
			mid_y += (ly + hy) / 32;
		}
	}

	if (num_linedefs > 0)
	{
		block_mid_x = (mid_x / num_linedefs) * 16;
		block_mid_y = (mid_y / num_linedefs) * 16;
	}

# if DEBUG_BLOCKMAP
	ajbsp_DebugPrintf("Blockmap lines centered at (%d,%d)\n", block_mid_x, block_mid_y);
# endif
}


void InitBlockmap()
{
	bbox_t map_bbox;

	// find limits of linedefs, and store as map limits
	FindBlockmapLimits(&map_bbox);

	ajbsp_PrintDetail("Map limits: (%d,%d) to (%d,%d)\n",
			map_bbox.minx, map_bbox.miny, map_bbox.maxx, map_bbox.maxy);

	block_x = map_bbox.minx - (map_bbox.minx & 0x7);
	block_y = map_bbox.miny - (map_bbox.miny & 0x7);

	block_w = ((map_bbox.maxx - block_x) / 128) + 1;
	block_h = ((map_bbox.maxy - block_y) / 128) + 1;

	block_count = block_w * block_h;
}


bool PutBlockmap (VStream &strm)
{
	if (! cur_info->do_blockmap || num_linedefs == 0)
	{
		// just create an empty blockmap lump
		//CreateLevelLump("BLOCKMAP")->Finish();
		return true;
	}

	block_overflowed = false;

	// initial phase: create internal blockmap containing the index of
	// all lines in each block.

	CreateBlockmap();

	// -AJA- second phase: compress the blockmap.  We do this by sorting
	//       the blocks, which is a typical way to detect duplicates in
	//       a large list.  This also detects BLOCKMAP overflow.

	CompressBlockmap();

	// final phase: write it out in the correct format

	if (block_overflowed)
	{
		// leave an empty blockmap lump
		//CreateLevelLump("BLOCKMAP")->Finish();

		Warning("Blockmap overflowed\n");
		FreeBlockmap();
		return true;
	}

	WriteBlockmap(strm);

	ajbsp_PrintDetail("Blockmap size: %dx%d (compression: %d%%)\n",
			block_w, block_h, block_compression);

	FreeBlockmap();

  return false;
}


//------------------------------------------------------------------------
// REJECT : Generate the reject table
//------------------------------------------------------------------------


#define DEBUG_REJECT  0

static u8_t *rej_matrix;
static int   rej_total_size;	// in bytes


//
// Allocate the matrix, init sectors into individual groups.
//
static void Reject_Init()
{
	rej_total_size = (num_sectors * num_sectors + 7) / 8;

	rej_matrix = new u8_t[rej_total_size];

	memset(rej_matrix, 0, rej_total_size);


	for (int i=0 ; i < num_sectors ; i++)
	{
		sector_t *sec = LookupSector(i);

		sec->rej_group = i;
		sec->rej_next = sec->rej_prev = sec;
	}
}


static void Reject_Free()
{
	delete[] rej_matrix;
	rej_matrix = NULL;
}


//
// Algorithm: Initially all sectors are in individual groups.  Now we
// scan the linedef list.  For each 2-sectored line, merge the two
// sector groups into one.  That's it !
//
static void Reject_GroupSectors()
{
	int i;

	for (i=0 ; i < num_linedefs ; i++)
	{
		linedef_t *line = LookupLinedef(i);
		sector_t *sec1, *sec2, *tmp;

		if (! line->right || ! line->left)
			continue;

		// the standard DOOM engine will not allow sight past lines
		// lacking the TWOSIDED flag, so we can skip them here too.
		if (! line->two_sided)
			continue;

		sec1 = line->right->sector;
		sec2 = line->left->sector;

		if (! sec1 || ! sec2 || sec1 == sec2)
			continue;

		// already in the same group ?
		if (sec1->rej_group == sec2->rej_group)
			continue;

		// swap sectors so that the smallest group is added to the biggest
		// group.  This is based on the assumption that sector numbers in
		// wads will generally increase over the set of linedefs, and so
		// (by swapping) we'll tend to add small groups into larger
		// groups, thereby minimising the updates to 'rej_group' fields
		// that is required when merging.

		if (sec1->rej_group > sec2->rej_group)
		{
			tmp = sec1; sec1 = sec2; sec2 = tmp;
		}

		// update the group numbers in the second group

		sec2->rej_group = sec1->rej_group;

		for (tmp=sec2->rej_next ; tmp != sec2 ; tmp=tmp->rej_next)
			tmp->rej_group = sec1->rej_group;

		// merge 'em baby...

		sec1->rej_next->rej_prev = sec2;
		sec2->rej_next->rej_prev = sec1;

		tmp = sec1->rej_next;
		sec1->rej_next = sec2->rej_next;
		sec2->rej_next = tmp;
	}
}


#if DEBUG_REJECT
static void Reject_DebugGroups()
{
	// Note: this routine is destructive to the group numbers

	int i;

	for (i=0 ; i < num_sectors ; i++)
	{
		sector_t *sec = LookupSector(i);
		sector_t *tmp;

		int group = sec->rej_group;
		int num = 0;

		if (group < 0)
			continue;

		sec->rej_group = -1;
		num++;

		for (tmp=sec->rej_next ; tmp != sec ; tmp=tmp->rej_next)
		{
			tmp->rej_group = -1;
			num++;
		}

		ajbsp_DebugPrintf("Group %d  Sectors %d\n", group, num);
	}
}
#endif


static void Reject_ProcessSectors()
{
	for (int view=0 ; view < num_sectors ; view++)
	{
		for (int target=0 ; target < view ; target++)
		{
			sector_t *view_sec = LookupSector(view);
			sector_t *targ_sec = LookupSector(target);

			int p1, p2;

			if (view_sec->rej_group == targ_sec->rej_group)
				continue;

			// for symmetry, do both sides at same time

			p1 = view * num_sectors + target;
			p2 = target * num_sectors + view;

			rej_matrix[p1 >> 3] |= (1 << (p1 & 7));
			rej_matrix[p2 >> 3] |= (1 << (p2 & 7));
		}
	}
}


static void Reject_WriteLump(VStream &strm)
{
	//Lump_c *lump = CreateLevelLump("REJECT", rej_total_size);
	strm.Serialise(rej_matrix, rej_total_size);
}


//
// For now we only do very basic reject processing, limited to
// determining all isolated groups of sectors (islands that are
// surrounded by void space).
//
void PutReject(VStream &strm)
{
	if (! cur_info->do_reject || num_sectors == 0)
	{
		// just create an empty reject lump
		//CreateLevelLump("REJECT")->Finish();
		return;
	}

	Reject_Init();
	Reject_GroupSectors();
	Reject_ProcessSectors();

# if DEBUG_REJECT
	Reject_DebugGroups();
# endif

	Reject_WriteLump(strm);
	Reject_Free();

	ajbsp_PrintDetail("Reject size: %d\n", rej_total_size);
}


//------------------------------------------------------------------------
// LEVEL : Level structure read/write functions.
//------------------------------------------------------------------------


// Note: ZDoom format support based on code (C) 2002,2003 Randy Heit


#define DEBUG_LOAD      0
#define DEBUG_BSP       0

#define ALLOC_BLKNUM  1024


// per-level variables

const char *lev_current_name;

short lev_current_idx;
short lev_current_start;

//bool lev_doing_hexen;

bool lev_force_v5;
bool lev_force_xnod;

bool lev_long_name;

int lev_overflows;


#define LEVELARRAY(TYPE, BASEVAR, NUMVAR)  \
	TYPE ** BASEVAR = NULL;  \
	int NUMVAR = 0;


LEVELARRAY(vertex_t,  lev_vertices,   num_vertices)
LEVELARRAY(linedef_t, lev_linedefs,   num_linedefs)
LEVELARRAY(sidedef_t, lev_sidedefs,   num_sidedefs)
LEVELARRAY(sector_t,  lev_sectors,    num_sectors)
LEVELARRAY(thing_t,   lev_things,     num_things)

static LEVELARRAY(seg_t,     segs,       num_segs)
static LEVELARRAY(subsec_t,  subsecs,    num_subsecs)
static LEVELARRAY(node_t,    nodes,      num_nodes)
static LEVELARRAY(wall_tip_t,wall_tips,  num_wall_tips)


int num_old_vert = 0;
int num_new_vert = 0;
int num_complete_seg = 0;
int num_real_lines = 0;


/* ----- allocation routines ---------------------------- */

#define ALLIGATOR(TYPE, BASEVAR, NUMVAR)  \
{  \
	if ((NUMVAR % ALLOC_BLKNUM) == 0)  \
	{  \
		BASEVAR = (TYPE **) UtilRealloc(BASEVAR, (NUMVAR + ALLOC_BLKNUM) * sizeof(TYPE *));  \
	}  \
	BASEVAR[NUMVAR] = (TYPE *) UtilCalloc(sizeof(TYPE));  \
	NUMVAR += 1;  \
	return BASEVAR[NUMVAR - 1];  \
}


vertex_t *NewVertex(void)
	ALLIGATOR(vertex_t, lev_vertices, num_vertices)

linedef_t *NewLinedef(void)
	ALLIGATOR(linedef_t, lev_linedefs, num_linedefs)

sidedef_t *NewSidedef(void)
	ALLIGATOR(sidedef_t, lev_sidedefs, num_sidedefs)

sector_t *NewSector(void)
	ALLIGATOR(sector_t, lev_sectors, num_sectors)

thing_t *NewThing(void)
	ALLIGATOR(thing_t, lev_things, num_things)

seg_t *NewSeg(void) {
	++cur_info->totalsegs;
	ALLIGATOR(seg_t, segs, num_segs)
}

subsec_t *NewSubsec(void)
	ALLIGATOR(subsec_t, subsecs, num_subsecs)

node_t *NewNode(void)
	ALLIGATOR(node_t, nodes, num_nodes)

wall_tip_t *NewWallTip(void)
	ALLIGATOR(wall_tip_t, wall_tips, num_wall_tips)


/* ----- free routines ---------------------------- */

#define FREEMASON(TYPE, BASEVAR, NUMVAR)  \
{  \
	int i;  \
	for (i=0 ; i < NUMVAR ; i++)  \
		UtilFree(BASEVAR[i]);  \
	if (BASEVAR)  \
		UtilFree(BASEVAR);  \
	BASEVAR = NULL; NUMVAR = 0;  \
}


void FreeVertices(void)
	FREEMASON(vertex_t, lev_vertices, num_vertices)

void FreeLinedefs(void)
	FREEMASON(linedef_t, lev_linedefs, num_linedefs)

void FreeSidedefs(void)
	FREEMASON(sidedef_t, lev_sidedefs, num_sidedefs)

void FreeSectors(void)
	FREEMASON(sector_t, lev_sectors, num_sectors)

void FreeThings(void)
	FREEMASON(thing_t, lev_things, num_things)

void FreeSegs(void)
	FREEMASON(seg_t, segs, num_segs)

void FreeSubsecs(void)
	FREEMASON(subsec_t, subsecs, num_subsecs)

void FreeNodes(void)
	FREEMASON(node_t, nodes, num_nodes)

void FreeWallTips(void)
	FREEMASON(wall_tip_t, wall_tips, num_wall_tips)


/* ----- lookup routines ------------------------------ */

#define LOOKERUPPER(BASEVAR, NUMVAR, NAMESTR)  \
{  \
	if (index < 0 || index >= NUMVAR)  \
		ajbsp_BugError("No such %s number #%d\n", NAMESTR, index);  \
	return BASEVAR[index];  \
}

vertex_t *LookupVertex(int index)
	LOOKERUPPER(lev_vertices, num_vertices, "vertex")

linedef_t *LookupLinedef(int index)
	LOOKERUPPER(lev_linedefs, num_linedefs, "linedef")

sidedef_t *LookupSidedef(int index)
	LOOKERUPPER(lev_sidedefs, num_sidedefs, "sidedef")

sector_t *LookupSector(int index)
	LOOKERUPPER(lev_sectors, num_sectors, "sector")

thing_t *LookupThing(int index)
	LOOKERUPPER(lev_things, num_things, "thing")

seg_t *LookupSeg(int index)
	LOOKERUPPER(segs, num_segs, "seg")

subsec_t *LookupSubsec(int index)
	LOOKERUPPER(subsecs, num_subsecs, "subsector")

node_t *LookupNode(int index)
	LOOKERUPPER(nodes, num_nodes, "node")

int GetVertexIndex (const vertex_t *v) {
  if (!v) return -1;
  for (int f = 0; f < num_vertices; ++f) if (v == lev_vertices[f]) return f;
  return -666;
}

int GetLinedefIndex (const linedef_t *ld) {
  if (!ld) return -1;
  for (int f = 0; f < num_linedefs; ++f) if (ld == lev_linedefs[f]) return f;
  return -666;
}



/*
static inline int VanillaSegDist(const seg_t *seg)
{
	double lx = seg->side ? seg->linedef->end->x : seg->linedef->start->x;
	double ly = seg->side ? seg->linedef->end->y : seg->linedef->start->y;

	// use the "true" starting coord (as stored in the wad)
	double sx = I_ROUND(seg->start->x);
	double sy = I_ROUND(seg->start->y);

	return (int) floor(UtilComputeDist(sx - lx, sy - ly) + 0.5);
}

static inline int VanillaSegAngle(const seg_t *seg)
{
	// compute the "true" delta
	double dx = I_ROUND(seg->end->x) - I_ROUND(seg->start->x);
	double dy = I_ROUND(seg->end->y) - I_ROUND(seg->start->y);

	double angle = UtilComputeAngle(dx, dy);

	if (angle < 0)
		angle += 360.0;

	int result = (int) floor(angle * 65536.0 / 360.0 + 0.5);

	return (result & 0xFFFF);
}
*/

extern "C" {
static int SegCompare(const void *p1, const void *p2, void *udata)
{
	const seg_t *A = ((const seg_t **) p1)[0];
	const seg_t *B = ((const seg_t **) p2)[0];

	if (A->index < 0)
		ajbsp_BugError("Seg %p never reached a subsector !\n", A);

	if (B->index < 0)
		ajbsp_BugError("Seg %p never reached a subsector !\n", B);

	return (A->index - B->index);
}
}


/* ----- writing routines ------------------------------ */

void MarkOverflow(int flags)
{
	// flags are ignored

	lev_overflows++;
}


void CheckLimits()
{
	if (num_sectors > 65534)
	{
		Failure("Map has too many sectors.\n");
		MarkOverflow(LIMIT_SECTORS);
	}

	if (num_sidedefs > 65534)
	{
		Failure("Map has too many sidedefs.\n");
		MarkOverflow(LIMIT_SIDEDEFS);
	}

	if (num_linedefs > 65534)
	{
		Failure("Map has too many linedefs.\n");
		MarkOverflow(LIMIT_LINEDEFS);
	}

	if (cur_info->gl_nodes && !cur_info->force_v5)
	{
		if (num_old_vert > 32767 ||
			num_new_vert > 32767 ||
			num_segs > 65534 ||
			num_nodes > 32767)
		{
			//!Warning("Forcing V5 of GL-Nodes due to overflows.\n");
			lev_force_v5 = true;
		}
	}

	if (! cur_info->force_xnod)
	{
		if (num_old_vert > 32767 ||
			num_new_vert > 32767 ||
			num_segs > 32767 ||
			num_nodes > 32767)
		{
			//!Warning("Forcing XNOD format nodes due to overflows.\n");
			lev_force_xnod = true;
		}
	}
}


void SortSegs()
{
	// sort segs into ascending index
	//qsort(segs, num_segs, sizeof(seg_t *), SegCompare);
	timsort_r(segs, num_segs, sizeof(seg_t *), &SegCompare, nullptr);
}


void FreeLevel(void)
{
	FreeVertices();
	FreeSidedefs();
	FreeLinedefs();
	FreeSectors();
	FreeThings();
	FreeSegs();
	FreeSubsecs();
	FreeNodes();
	FreeWallTips();
}


} // namespace
