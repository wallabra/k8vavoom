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

#include "cmdlib.h"
#include "wadlib.h"
#include "scrlib.h"
#include "imglib.h"

using namespace VavoomUtils;

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// posts are runs of non masked source pixels
struct post_t
{
	byte		topdelta;       // -1 is the last post in a column
	byte		length;         // length data bytes follows
};

// column_t is a list of 0 or more post_t, (byte)-1 terminated
typedef post_t column_t;

//  Patches.
//  A patch holds one or more columns.
//  Patches are used for sprites and all masked pictures, and we compose
// textures from the TEXTURE1/2 lists of patches.
struct patch_t
{
	short		width;          // bounding box size
	short		height;
	short		leftoffset;     // pixels to the left of origin
	short		topoffset;      // pixels below the origin
	int			columnofs[8];   // only [width] used
	// the [0] is &columnofs[width]
};

struct vpic_t
{
	char		magic[4];
	short		width;
	short		height;
	byte		bpp;
	byte		reserved[7];
};

struct RGB_MAP
{
	byte		data[32][32][32];
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

TOWadFile		outwad;

char			basepath[256];
char			srcpath[256];

RGB_MAP			rgb_table;
bool			rgb_table_created;

char			lumpname[12];
char			destfile[1024];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
//	makecol8
//
//==========================================================================

int makecol8(int r, int g, int b)
{
	return rgb_table.data[r >> 3][g >> 3][b >> 3];
}

//==========================================================================
//
//	GetPixel
//
//==========================================================================

byte GetPixel(int x, int y)
{
	return ImgData[x + y * ImgWidth];
}

//==========================================================================
//
//	GetPixelRGB
//
//==========================================================================

rgba_t &GetPixelRGB(int x, int y)
{
	return ((rgba_t *)ImgData)[x + y * ImgWidth];
}

//==========================================================================
//
//	fn
//
//==========================================================================

char *fn(const char *name)
{
	static char filename[1024];
	if (name[0] == '/' || name[0] == '\\' || name[1] == ':')
	{
		//	Absolute path
		strcpy(filename, name);
	}
	else
	{
		sprintf(filename, "%s%s", srcpath, name);
	}
	return filename;
}

//==========================================================================
//
//	AddWadFile
//
//==========================================================================

static void AddWadFile(const char *name)
{
	TIWadFile	inwad;

	inwad.Open(name);
	for (int i = 0; i < inwad.numlumps; i++)
	{
		void *data = inwad.GetLump(i);
		outwad.AddLump(inwad.LumpName(i), data, inwad.LumpSize(i));
		Free(data);
	}
	inwad.Close();
}

//==========================================================================
//
//	AddWad
//
//	$WAD filename
//
//==========================================================================

static void AddWad(const char *name)
{
	char *filename = fn(name);
	DefaultExtension(filename, ".wad");
	AddWadFile(filename);
}

//==========================================================================
//
//	AddMap
//
//	$MAP filename
//
//==========================================================================

static void AddMap(const char *name)
{
	char	*filename;

	filename = fn(name);
	DefaultExtension(filename, ".wad");
	AddWadFile(filename);

	StripExtension(filename);
	strcat(filename, ".gwa");
	FILE *ff = fopen(filename, "rb");
	if (ff)
	{
		fclose(ff);
		AddWadFile(filename);
	}
}

//==========================================================================
//
//	LoadImage
//
//	$LOAD image
//
//==========================================================================

static void LoadImage(void)
{
	SC_MustGetString();
	DestroyImage();
	LoadImage(fn(sc_String));
	rgb_table_created = false;
}

//==========================================================================
//
//	my_bestfit_color
//
//==========================================================================

int my_bestfit_color(int r, int g, int b)
{
	int		best_color = 0;
	int		best_dist = 0x1000000;

	for (int i = 1; i < 256; i++)
	{
		int dist = (ImgPal[i].r - r) * (ImgPal[i].r - r) +
			(ImgPal[i].g - g) * (ImgPal[i].g - g) +
			(ImgPal[i].b - b) * (ImgPal[i].b - b);
		if (dist < best_dist)
		{
			best_color = i;
			best_dist = dist;
			if (!dist)
				break;
		}
	}
	return best_color;
}

//==========================================================================
//
//	SetupRGBTable
//
//==========================================================================

void SetupRGBTable(void)
{
	if (rgb_table_created)
	{
		return;
	}

	for (int r = 0; r < 32; r++)
	{
		for (int g = 0; g < 32; g++)
		{
			for (int b = 0; b < 32; b++)
			{
				rgb_table.data[r][g][b] = my_bestfit_color(
					(int)(r * 255.0 / 31.0 + 0.5),
					(int)(g * 255.0 / 31.0 + 0.5),
					(int)(b * 255.0 / 31.0 + 0.5));
			}
		}
	}
	rgb_table_created = true;
}

//==========================================================================
//
//	GrabRGBTable
//
//==========================================================================

void GrabRGBTable(void)
{
	byte	tmp[32 * 32 * 32 + 4];

	SetupRGBTable();
	memcpy(tmp, &rgb_table, 32 * 32 * 32);
	tmp[32 * 32 * 32] = 0;
	outwad.AddLump(lumpname, tmp, 32 * 32 * 32 + 1);
}

//==========================================================================
//
//	GrabTranslucencyTable
//
//	lumpname TINTTAB amount
//
//==========================================================================

void GrabTranslucencyTable(void)
{
	byte		table[256 * 256];
	byte		temp[768];
	int			i;
	int			j;
	int			r;
	int			g;
	int			b;
	byte*		p;
	byte*		q;
	int			transluc;

	SC_MustGetNumber();
	transluc = sc_Number;

	SetupRGBTable();

	p = table;
	for (i = 0; i < 256; i++)
	{
		temp[i * 3]     = ImgPal[i].r * transluc / 100;
		temp[i * 3 + 1] = ImgPal[i].g * transluc / 100;
		temp[i * 3 + 2] = ImgPal[i].b * transluc / 100;
	}
	for (i = 0; i < 256; i++)
	{
		r = ImgPal[i].r * (100 - transluc) / 100;
		g = ImgPal[i].g * (100 - transluc) / 100;
		b = ImgPal[i].b * (100 - transluc) / 100;
		q = temp;
		for (j = 0; j < 256; j++)
		{
			*(p++) = makecol8(r + q[0], g + q[1], b + q[2]);
			q += 3;
		}
	}
	outwad.AddLump(lumpname, table, 256 * 256);
}

//==========================================================================
//
//	GrabScaleMap
//
//	lumpname SCALEMAP r g b
//
//==========================================================================

void GrabScaleMap(void)
{
	byte		map[256];
	int			i;
	double		r;
	double		g;
	double		b;

	SC_MustGetFloat();
	r = sc_Float;
	SC_MustGetFloat();
	g = sc_Float;
	SC_MustGetFloat();
	b = sc_Float;

	SetupRGBTable();

	for (i = 0; i < 256; i++)
	{
		double col = (ImgPal[i].r * 0.3 + ImgPal[i].g * 0.5 + ImgPal[i].b * 0.2);
		map[i] = makecol8((int)(r * col), (int)(g * col), (int)(b * col));
	}

	outwad.AddLump(lumpname, map, 256);
}

//==========================================================================
//
//	GrabRaw
//
//	lumpname RAW x y width height
//
//==========================================================================

void GrabRaw(void)
{
	SC_MustGetNumber();
	int x1 = sc_Number;
	SC_MustGetNumber();
	int y1 = sc_Number;
	SC_MustGetNumber();
	int w = sc_Number;
	SC_MustGetNumber();
	int h = sc_Number;
	int x2 = x1 + w;
	int y2 = y1 + h;

	byte *data = (byte*)Malloc(w * h);
	byte *dst = data;
	for (int y = y1; y < y2; y++)
	{
		for (int x = x1; x < x2; x++)
		{
			*dst = GetPixel(x, y);
			dst++;
		}
	}

	outwad.AddLump(lumpname, data, w * h);
	Free(data);
}

//==========================================================================
//
//	GrabPatch
//
//	lumpname PATCH x y width height leftoffset topoffset
//
//==========================================================================

void GrabPatch(void)
{
	SC_MustGetNumber();
	int x1 = sc_Number;
	SC_MustGetNumber();
	int y1 = sc_Number;
	SC_MustGetNumber();
	int w = sc_Number;
	SC_MustGetNumber();
	int h = sc_Number;
	SC_MustGetNumber();
	int leftoffset = sc_Number;
	SC_MustGetNumber();
	int topoffset = sc_Number;

	int TransColor = 0;
	patch_t* Patch = (patch_t*)Malloc(8 + 4 * w + w * h * 4);
	Patch->width = LittleShort(w);
	Patch->height = LittleShort(h);
	Patch->leftoffset = LittleShort(leftoffset);
	Patch->topoffset = LittleShort(topoffset);
	column_t* Col = (column_t*)&Patch->columnofs[w];

	for (int x = 0; x < w; x++)
	{
		Patch->columnofs[x] = LittleLong((byte*)Col - (byte*)Patch);
		int y = 0;
		int PrevTop = -1;
		while (y < h)
		{
			//	Skip transparent pixels.
			if (GetPixel(x1 + x, y1 + y) == TransColor)
			{
				y++;
				continue;
			}
			//	Grab a post.
			if (y < 255)
			{
				Col->topdelta = y;
			}
			else
			{
				//	Tall patch.
				while (y - PrevTop > 254)
				{
					//	Insert empty post.
					Col->topdelta = 254;
					Col = (column_t*)((byte*)Col + 4);
					if (PrevTop < 254)
					{
						PrevTop = 254;
					}
					else
					{
						PrevTop += 254;
					}
				}
				Col->topdelta = y - PrevTop;
			}
			PrevTop = y;
			byte* Pixels = (byte*)Col + 3;
			while (y < h && Col->length < 255 &&
				GetPixel(x1 + x, y1 + y) != TransColor)
			{
				Pixels[Col->length] = GetPixel(x1 + x, y1 + y);
				Col->length++;
				y++;
			}
			Col = (column_t*)((byte*)Col + Col->length + 4);
		}

		//	Add terminating post.
		Col->topdelta = 0xff;
		Col = (column_t*)((byte*)Col + 4);
	}

	outwad.AddLump(lumpname, Patch, (byte*)Col - (byte*)Patch);
	Free(Patch);
}

//==========================================================================
//
//	GrabPic
//
//	lumpname PIC x y width height
//
//==========================================================================

void GrabPic(void)
{
	SC_MustGetNumber();
	int x1 = sc_Number;
	SC_MustGetNumber();
	int y1 = sc_Number;
	SC_MustGetNumber();
	int w = sc_Number;
	SC_MustGetNumber();
	int h = sc_Number;
	int x2 = x1 + w;
	int y2 = y1 + h;

	vpic_t *pic = (vpic_t*)Malloc(sizeof(vpic_t) + w * h);
	memcpy(pic->magic, "VPIC", 4);
	pic->width = LittleShort(w);
	pic->height = LittleShort(h);
	pic->bpp = 8;
	byte *dst = (byte *)(pic + 1);
	for (int y = y1; y < y2; y++)
	{
		for (int x = x1; x < x2; x++)
		{
			*dst = GetPixel(x, y);
			dst++;
		}
	}

	outwad.AddLump(lumpname, pic, sizeof(vpic_t) + w * h);
	Free(pic);
}

//==========================================================================
//
//	GrabPic15
//
//	lumpname PIC15 x y width height
//
//==========================================================================

void GrabPic15(void)
{
	SC_MustGetNumber();
	int x1 = sc_Number;
	SC_MustGetNumber();
	int y1 = sc_Number;
	SC_MustGetNumber();
	int w = sc_Number;
	SC_MustGetNumber();
	int h = sc_Number;
	int x2 = x1 + w;
	int y2 = y1 + h;

	ConvertImageTo32Bit();
	vpic_t *pic = (vpic_t*)Malloc(sizeof(vpic_t) + w * h * 2);
	memcpy(pic->magic, "VPIC", 4);
	pic->width = LittleShort(w);
	pic->height = LittleShort(h);
	pic->bpp = 15;
	byte *dst = (byte *)(pic + 1);
	for (int y = y1; y < y2; y++)
	{
		for (int x = x1; x < x2; x++)
		{
			const rgba_t &p = GetPixelRGB(x, y);
			int c;
			if (p.r == 0 && (p.g & 0xf8) == 0xf8 && (p.b & 0xf8) == 0xf8)
				c = 0x8000;
			else
				c = ((p.r << 7) & 0x7c00) | ((p.g << 2) & 0x03e0) | ((p.b >> 3) & 0x001f);
			dst[0] = byte(c);
			dst[1] = byte(c >> 8);
			dst += 2;
		}
	}

	outwad.AddLump(lumpname, pic, sizeof(vpic_t) + w * h * 2);
	Free(pic);
}

//==========================================================================
//
//	ParseScript
//
//==========================================================================

void ParseScript(const char *name)
{
	ExtractFilePath(name, basepath);
	strcpy(srcpath, basepath);

	strcpy(destfile, name);
	DefaultExtension(destfile, ".ls");
	SC_Open(destfile);
	StripExtension(destfile);

	bool OutputOpened = false;
	bool GrabMode = false;

	while (SC_GetString())
	{
		if (SC_Compare("$dest"))
		{
			if (OutputOpened)
				SC_ScriptError("Output already opened");
			SC_MustGetString();
			strcpy(destfile, fn(sc_String));
			continue;
		}

		if (SC_Compare("$srcdir"))
		{
			SC_MustGetString();
			sprintf(srcpath, "%s%s", basepath, sc_String);
			if (srcpath[strlen(srcpath) - 1] != '/')
			{
				strcat(srcpath, "/");
			}
			continue;
		}

		if (SC_Compare("$load"))
		{
			LoadImage();
			GrabMode = true;
			continue;
		}

		if (SC_Compare("$files"))
		{
			GrabMode = false;
			continue;
		}

		if (!OutputOpened)
		{
			DefaultExtension(destfile, ".wad");
			outwad.Open(destfile, "PWAD");
			OutputOpened = true;
		}

		if (SC_Compare("$wad"))
		{
			SC_MustGetString();
			AddWad(sc_String);
			continue;
		}

		if (SC_Compare("$map"))
		{
			SC_MustGetString();
			AddMap(sc_String);
			continue;
		}

		if (SC_Compare("$label"))
		{
			SC_MustGetString();
			outwad.AddLump(sc_String, NULL, 0);
			continue;
		}

		if (GrabMode)
		{
			if (strlen(sc_String) > 8)
			{
				SC_ScriptError("Lump name is too long.");
			}
			memset(lumpname, 0, sizeof(lumpname));
			strcpy(lumpname, sc_String);

			SC_MustGetString();
			if (SC_Compare("rgbtable"))
			{
				GrabRGBTable();
			}
			else if (SC_Compare("tinttab"))
			{
				GrabTranslucencyTable();
			}
			else if (SC_Compare("scalemap"))
			{
				GrabScaleMap();
			}
			else if (SC_Compare("raw"))
			{
				GrabRaw();
			}
			else if (SC_Compare("patch"))
			{
				GrabPatch();
			}
			else if (SC_Compare("pic"))
			{
				GrabPic();
			}
			else if (SC_Compare("pic15"))
			{
				GrabPic15();
			}
			else
			{
				SC_ScriptError(va("Unknown command %s", sc_String));
			}
		}
		else
		{
			ExtractFileBase(sc_String, lumpname);
			if (strlen(lumpname) > 8)
			{
				SC_ScriptError("File name too long");
			}
			void *data;
			int size = LoadFile(fn(sc_String), &data);
			outwad.AddLump(lumpname, data, size);
			Free(data);
		}
	}

	DestroyImage();
	if (outwad.handle)
	{
		outwad.Close();
	}
	SC_Close();
}

//==========================================================================
//
//	main
//
//==========================================================================

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "Usage: vlumpy <script1> [<script2> ...]\n");
		return 1;
	}

	try
	{
		for (int i = 1; i < argc; i++)
		{
			ParseScript(argv[i]);
		}
	}
	catch (WadLibError &E)
	{
		Error("%s", E.message);
	}
	return 0;
}

//**************************************************************************
//
//	$Log$
//	Revision 1.11  2004/12/27 12:21:19  dj_jl
//	Byte swapping for patches.
//
//	Revision 1.10  2003/11/03 07:20:30  dj_jl
//	Grabbing of patches
//	
//	Revision 1.9  2002/08/24 14:41:35  dj_jl
//	Removed usage of the iostream.
//	
//	Revision 1.8  2002/06/22 07:25:43  dj_jl
//	Added transparent pixels to VPIC.
//	
//	Revision 1.7  2002/04/11 16:54:01  dj_jl
//	Added support for 15-bit vpics.
//	
//	Revision 1.6  2002/03/20 19:12:56  dj_jl
//	Added catching of wad errors.
//	
//	Revision 1.5  2002/01/07 12:31:36  dj_jl
//	Changed copyright year
//	
//	Revision 1.4  2001/09/24 17:31:02  dj_jl
//	Beautification
//	
//	Revision 1.3  2001/08/31 17:19:53  dj_jl
//	Beautification
//
//	Revision 1.2  2001/07/27 14:27:56  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************
