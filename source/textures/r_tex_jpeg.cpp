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
#include "gamedefs.h"
#include "r_tex.h"


#ifdef VAVOOM_DISABLE_STB_IMAGE_JPEG
extern "C" {
# ifdef VAVOOM_USE_LIBJPG
#  include <jpeglib.h>
# else
#  include "../../libs/jpeg/jpeglib.h"
# endif
}
#else
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-function"
# define STB_IMAGE_IMPLEMENTATION
//# define STB_IMAGE_STATIC
# define STBIDEF  __attribute__((unused)) static
# define STBI_ONLY_JPEG
# define STBI_NO_STDIO
# define STBI_MALLOC   Z_Malloc
# define STBI_REALLOC  Z_Realloc
# define STBI_FREE     Z_Free
# include "stb_image.h"
# define STB_IMAGE_WRITE_IMPLEMENTATION
//# define STB_IMAGE_WRITE_STATIC
# define STBIWDEF  __attribute__((unused)) static
# define STBI_WRITE_NO_STDIO
# define STBIW_MALLOC   Z_Malloc
# define STBIW_REALLOC  Z_Realloc
# define STBIW_FREE     Z_Free
# include "stb_image_write.h"
# pragma GCC diagnostic pop
#endif


// ////////////////////////////////////////////////////////////////////////// //
#ifdef VAVOOM_DISABLE_STB_IMAGE_JPEG
struct VJpegClientData {
  VStream *Strm;
  JOCTET Buffer[4096];
};
#endif

static VCvarI jpeg_quality("jpeg_quality", "80", "Jpeg screenshot quality.", CVAR_Archive);


//==========================================================================
//
//  VJpegTexture::Create
//
//==========================================================================
VTexture *VJpegTexture::Create (VStream &Strm, int LumpNum) {
  if (Strm.TotalSize() < 11) return nullptr; // file is too small

  vuint8 Buf[8];

  // check header
  Strm.Seek(0);
  Strm.Serialise(Buf, 2);
  if (Buf[0] != 0xff || Buf[1] != 0xd8) return nullptr;

  // find SOFn marker to get the image dimensions
  int Len;
  do {
    if (Strm.TotalSize()-Strm.Tell() < 4) return nullptr;

    // read marker
    Strm.Serialise(Buf, 2);
    if (Buf[0] != 0xff) return nullptr; // missing identifier of a marker

    // skip padded 0xff-s
    while (Buf[1] == 0xff) {
      if (Strm.TotalSize()-Strm.Tell() < 3) return nullptr;
      Strm.Serialise(Buf+1, 1);
    }

    // read length
    Strm.Serialise(Buf+2, 2);
    Len = Buf[3]+(Buf[2]<<8);
    if (Len < 2) return nullptr;
    if (Strm.Tell()+Len-2 >= Strm.TotalSize()) return nullptr;

    // if it's not a SOFn marker, then skip it
    if (Buf[1] != 0xc0 && Buf[1] != 0xc1 && Buf[1] != 0xc2) Strm.Seek(Strm.Tell()+Len-2);
  } while (Buf[1] != 0xc0 && Buf[1] != 0xc1 && Buf[1] != 0xc2);

  if (Len < 7) return nullptr;
  Strm.Serialise(Buf, 5);
  vint32 Width = Buf[4]+(Buf[3]<<8);
  vint32 Height = Buf[2]+(Buf[1]<<8);
  return new VJpegTexture(LumpNum, Width, Height);
}


//==========================================================================
//
//  VJpegTexture::VJpegTexture
//
//==========================================================================
VJpegTexture::VJpegTexture (int ALumpNum, int AWidth, int AHeight)
  : VTexture()
{
  SourceLump = ALumpNum;
  Name = W_LumpName(SourceLump);
  Width = AWidth;
  Height = AHeight;
  mFormat = TEXFMT_RGBA;
}


//==========================================================================
//
//  VJpegTexture::~VJpegTexture
//
//==========================================================================
VJpegTexture::~VJpegTexture () {
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
}


#ifdef VAVOOM_DISABLE_STB_IMAGE_JPEG
//==========================================================================
//
//  my_init_source
//
//==========================================================================
static void my_init_source (j_decompress_ptr cinfo) {
  cinfo->src->next_input_byte = nullptr;
  cinfo->src->bytes_in_buffer = 0;
}


//==========================================================================
//
//  my_fill_input_buffer
//
//==========================================================================
static boolean my_fill_input_buffer (j_decompress_ptr cinfo) {
  VJpegClientData *cdata = (VJpegClientData*)cinfo->client_data;
  if (cdata->Strm->AtEnd()) {
    // insert a fake EOI marker
    cdata->Buffer[0] = 0xff;
    cdata->Buffer[1] = JPEG_EOI;
    cinfo->src->next_input_byte = cdata->Buffer;
    cinfo->src->bytes_in_buffer = 2;
    return FALSE;
  }

  int Count = 4096;
  if (Count > cdata->Strm->TotalSize()-cdata->Strm->Tell()) {
    Count = cdata->Strm->TotalSize()-cdata->Strm->Tell();
  }
  cdata->Strm->Serialise(cdata->Buffer, Count);
  cinfo->src->next_input_byte = cdata->Buffer;
  cinfo->src->bytes_in_buffer = Count;
  return TRUE;
}


//==========================================================================
//
//  my_skip_input_data
//
//==========================================================================
static void my_skip_input_data (j_decompress_ptr cinfo, long num_bytes) {
  if (num_bytes <= 0) return;
  if ((long)cinfo->src->bytes_in_buffer > num_bytes) {
    cinfo->src->bytes_in_buffer -= num_bytes;
    cinfo->src->next_input_byte += num_bytes;
  } else {
    VJpegClientData *cdata = (VJpegClientData*)cinfo->client_data;
    int Pos = cdata->Strm->Tell()+num_bytes-cinfo->src->bytes_in_buffer;
    if (Pos > cdata->Strm->TotalSize()) Pos = cdata->Strm->TotalSize();
    cdata->Strm->Seek(Pos);
    cinfo->src->bytes_in_buffer = 0;
  }
}


//==========================================================================
//
//  my_term_source
//
//==========================================================================
static void my_term_source (j_decompress_ptr) {
}


//==========================================================================
//
//  my_error_exit
//
//==========================================================================
static void my_error_exit (j_common_ptr cinfo) {
  (*cinfo->err->output_message)(cinfo);
  throw -1;
}


//==========================================================================
//
//  my_output_message
//
//==========================================================================
static void my_output_message (j_common_ptr cinfo) {
  char Msg[JMSG_LENGTH_MAX];
  cinfo->err->format_message(cinfo, Msg);
  GCon->Log(Msg);
}


//==========================================================================
//
//  VJpegTexture::GetPixels
//
//==========================================================================
vuint8 *VJpegTexture::GetPixels () {
  // if we already have loaded pixels, return them
  if (Pixels) return Pixels;
  transparent = false;
  translucent = false;

  mFormat = TEXFMT_RGBA;
  Pixels = new vuint8[Width*Height*4];
  memset(Pixels, 0, Width*Height*4);

  jpeg_decompress_struct cinfo;
  jpeg_source_mgr smgr;
  jpeg_error_mgr jerr;
  VJpegClientData cdata;

  // open stream
  VStream *Strm = W_CreateLumpReaderNum(SourceLump);
  if (!Strm) Sys_Error("cannot load jpeg texture from '%s'", *W_FullLumpName(SourceLump));

  try {
    // set up the JPEG error routines
    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = my_error_exit;
    jerr.output_message = my_output_message;

    // set client data pointer
    cinfo.client_data = &cdata;
    cdata.Strm = Strm;

    // initialise the JPEG decompression object
    jpeg_create_decompress(&cinfo);

    // specify data source
    smgr.init_source = my_init_source;
    smgr.fill_input_buffer = my_fill_input_buffer;
    smgr.skip_input_data = my_skip_input_data;
    smgr.resync_to_restart = jpeg_resync_to_restart;
    smgr.term_source = my_term_source;
    cinfo.src = &smgr;

    // read file parameters with jpeg_read_header()
    jpeg_read_header(&cinfo, TRUE);

    if (!((cinfo.out_color_space == JCS_RGB && cinfo.num_components == 3) ||
        (cinfo.out_color_space == JCS_CMYK && cinfo.num_components == 4) ||
        (cinfo.out_color_space == JCS_GRAYSCALE && cinfo.num_components == 1)))
    {
      GCon->Log(NAME_Warning, "Unsupported JPEG file format");
      throw -1;
    }

    // start decompressor
    jpeg_start_decompress(&cinfo);

    // JSAMPLEs per row in output buffer
    int row_stride = cinfo.output_width*cinfo.output_components;
    // make a one-row-high sample array that will go away when done with image
    JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

    // read image
    vuint8 *pDst = Pixels;
    while (cinfo.output_scanline < cinfo.output_height) {
      jpeg_read_scanlines(&cinfo, buffer, 1);
      JOCTET *pSrc = buffer[0];
      switch (cinfo.out_color_space) {
        case JCS_RGB:
          for (int x = 0; x < Width; ++x) {
            pDst[0] = pSrc[0];
            pDst[1] = pSrc[1];
            pDst[2] = pSrc[2];
            pDst[3] = 0xff;
            pSrc += 3;
            pDst += 4;
          }
          break;
        case JCS_GRAYSCALE:
          for (int x = 0; x < Width; ++x) {
            pDst[0] = pSrc[0];
            pDst[1] = pSrc[0];
            pDst[2] = pSrc[0];
            pDst[3] = 0xff;
            pSrc++;
            pDst += 4;
          }
          break;
        case JCS_CMYK:
          for (int x = 0; x < Width; ++x) {
            pDst[0] = (255-pSrc[0])*(255-pSrc[3])/255;
            pDst[1] = (255-pSrc[1])*(255-pSrc[3])/255;
            pDst[2] = (255-pSrc[2])*(255-pSrc[3])/255;
            pDst[3] = 0xff;
            pSrc += 4;
            pDst += 4;
          }
          break;
        default:
          break;
      }
    }

    // finish decompression
    jpeg_finish_decompress(&cinfo);
  } catch (int) {
  }

  // release JPEG decompression object
  jpeg_destroy_decompress(&cinfo);

  //TODO: check for errors

  // free memory
  delete Strm;

  ConvertPixelsToShaded();
  return Pixels;
}

#else /* stb_image interface */

struct CBReadInfo {
  VStream *strm;
  int strmStart;
  int strmSize;
  int strmPos;
};


static const stbi_io_callbacks stbcbacks = {
  // fill 'data' with 'size' bytes; return number of bytes actually read
  .read = [](void *user, char *data, int size) -> int {
    if (size <= 0) return 0; // just in case
    CBReadInfo *nfo = (CBReadInfo *)user;
    int left = nfo->strmSize-nfo->strmPos;
    if (size > left) size = left;
    if (size) nfo->strm->Serialise(data, size);
    nfo->strmPos += size;
    return size;
  },
  // skip the next 'n' bytes, or 'unget' the last -n bytes if negative
  .skip = [](void *user, int n) -> void {
    if (n == 0) return;
    CBReadInfo *nfo = (CBReadInfo *)user;
    n = nfo->strmPos+n; // ignore overflow, meh
    if (n < 0 || n > nfo->strmSize) abort(); // this should not happen
    nfo->strmPos = n;
    nfo->strm->Seek(n);
  },
  // returns nonzero if we are at end of file/data
  .eof = [](void *user) -> int {
    CBReadInfo *nfo = (CBReadInfo *)user;
    return !!(nfo->strmPos >= nfo->strmSize);
  },
};


//==========================================================================
//
//  VJpegTexture::GetPixels
//
//==========================================================================
vuint8 *VJpegTexture::GetPixels () {
  // if we already have loaded pixels, return them
  if (Pixels) return Pixels;
  transparent = false;
  translucent = false;

  mFormat = TEXFMT_RGBA;
  Pixels = new vuint8[Width*Height*4];
  memset(Pixels, 0, Width*Height*4);

  // open stream
  VStream *Strm = W_CreateLumpReaderNum(SourceLump);
  if (!Strm) Sys_Error("cannot load jpeg texture from '%s'", *W_FullLumpName(SourceLump));

  CBReadInfo nfo;
  nfo.strm = Strm;
  nfo.strmStart = 0;
  nfo.strmSize = Strm->TotalSize();
  nfo.strmPos = 0;
  if (Strm->IsError()) {
    delete Strm;
    Sys_Error("error reading jpeg texture from '%s'", *W_FullLumpName(SourceLump));
  }

  int imgwidth = 0, imgheight = 0, imgchans = 0;
  vuint8 *data = (vuint8 *)stbi_load_from_callbacks(&stbcbacks, (void *)&nfo, &imgwidth, &imgheight, &imgchans, 4); // request RGBA
  if (Strm->IsError()) {
    if (data) stbi_image_free(data);
    delete Strm;
    Sys_Error("error reading jpeg texture from '%s'", *W_FullLumpName(SourceLump));
  }
  delete Strm;

  if (!data) Sys_Error("cannot load jpeg texture from '%s'", *W_FullLumpName(SourceLump));

  if (Width != imgwidth || Height != imgheight) {
    stbi_image_free(data);
    Sys_Error("cannot load jpeg texture from '%s' (detected dims are %dx%d, loaded dims are %dx%d", *W_FullLumpName(SourceLump), Width, Height, imgwidth, imgheight);
  }

  // copy image
  vuint8 *pDst = Pixels;
  const vuint8 *pSrc = data;
  for (int y = 0; y < imgheight; ++y) {
    for (int x = 0; x < imgwidth; ++x) {
      pDst[0] = pSrc[0];
      pDst[1] = pSrc[1];
      pDst[2] = pSrc[2];
      pDst[3] = 0xff;
      pSrc += 4;
      pDst += 4;
    }
  }

  // free memory
  stbi_image_free(data);

  ConvertPixelsToShaded();
  return Pixels;
}
#endif


//==========================================================================
//
//  VJpegTexture::Unload
//
//==========================================================================
void VJpegTexture::Unload () {
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
}


#ifdef CLIENT

#ifdef VAVOOM_DISABLE_STB_IMAGE_JPEG
# ifdef VAVOOM_USE_LIBJPG
//==========================================================================
//
//  my_init_destination
//
//==========================================================================
static void my_init_destination (j_compress_ptr cinfo) {
  VJpegClientData *cdata = (VJpegClientData*)cinfo->client_data;
  cinfo->dest->next_output_byte = cdata->Buffer;
  cinfo->dest->free_in_buffer = 4096;
}


//==========================================================================
//
//  my_empty_output_buffer
//
//==========================================================================
static boolean my_empty_output_buffer (j_compress_ptr cinfo) {
  VJpegClientData *cdata = (VJpegClientData*)cinfo->client_data;
  cdata->Strm->Serialise(cdata->Buffer, 4096);
  cinfo->dest->next_output_byte = cdata->Buffer;
  cinfo->dest->free_in_buffer = 4096;
  return TRUE;
}


//==========================================================================
//
//  my_term_destination
//
//==========================================================================
static void my_term_destination (j_compress_ptr cinfo) {
  VJpegClientData *cdata = (VJpegClientData*)cinfo->client_data;
  cdata->Strm->Serialise(cdata->Buffer, 4096-cinfo->dest->free_in_buffer);
}


//==========================================================================
//
//  WriteJPG
//
//==========================================================================
void WriteJPG (VStr FileName, const void *Data, int Width, int Height, int Bpp, bool Bot2top) {
  VStream *Strm = FL_OpenFileWrite(FileName, true);
  if (!Strm) {
    GCon->Log(NAME_Warning, "Couldn't write jpg");
    return;
  }

  jpeg_compress_struct cinfo;
  jpeg_destination_mgr dmgr;
  jpeg_error_mgr jerr;
  VJpegClientData cdata;

  // set up the JPEG error routines
  cinfo.err = jpeg_std_error(&jerr);
  jerr.error_exit = my_error_exit;
  jerr.output_message = my_output_message;

  // set client data pointer
  cinfo.client_data = &cdata;
  cdata.Strm = Strm;

  try {
    // initialise the JPEG decompression object
    jpeg_create_compress(&cinfo);

    // specify data source
    dmgr.init_destination = my_init_destination;
    dmgr.empty_output_buffer = my_empty_output_buffer;
    dmgr.term_destination = my_term_destination;
    cinfo.dest = &dmgr;

    // specify image data
    cinfo.image_width = Width;
    cinfo.image_height = Height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    // set up compression
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, clampval(jpeg_quality.asInt(), 1, 100), TRUE);

    // perform compression
    jpeg_start_compress(&cinfo, TRUE);
    TArray<JSAMPROW> RowPointers;
    TArray<vuint8> TmpData;
    RowPointers.SetNum(Height);
    if (Bpp == 8) {
      // convert image to 24 bit
      TmpData.SetNum(Width*Height*3);
      for (int i = 0; i < Width*Height; ++i) {
        int Col = ((vuint8 *)Data)[i];
        TmpData[i*3] = r_palette[Col].r;
        TmpData[i*3+1] = r_palette[Col].g;
        TmpData[i*3+2] = r_palette[Col].b;
      }
      for (int i = 0; i < Height; ++i) {
        RowPointers[i] = &TmpData[(Bot2top ? Height-i-1 : i)*Width*3];
      }
    } else {
      for (int i = 0; i < Height; ++i) {
        RowPointers[i] = ((vuint8 *)Data)+(Bot2top ? Height-i-1 : i)*Width*3;
      }
    }
    jpeg_write_scanlines(&cinfo, RowPointers.Ptr(), Height);
    jpeg_finish_compress(&cinfo);
  } catch (int) {
  }

  // finish with the image
  jpeg_destroy_compress(&cinfo);
  Strm->Close();
  delete Strm;
}
# endif /* VAVOOM_USE_LIBJPG */

#else /* !VAVOOM_DISABLE_STB_IMAGE_JPEG */

extern "C" {
  static void stbWriter (void *context, void *data, int size) {
    VStream *strm = (VStream *)context;
    if (size > 0) strm->Serialise(data, size);
  }
}

//==========================================================================
//
//  WriteJPG
//
//==========================================================================
void WriteJPG (VStr FileName, const void *Data, int Width, int Height, int Bpp, bool Bot2top) {
  if (Width < 1 || Height < 1 || (Bpp != 8 && Bpp != 24 && Bpp != 32)) {
    GCon->Log(NAME_Warning, "Couldn't write jpg (invalid parameters)");
    return;
  }

  VStream *Strm = FL_OpenFileWrite(FileName, true);
  if (!Strm) {
    GCon->Logf(NAME_Warning, "Couldn't write jpg (error creating output stream for '%s')", *FileName);
    return;
  }

  TArray<vuint8> imgdata;
  imgdata.setLength(Width*Height*3);
  const vuint8 *src = (const vuint8 *)Data;
  if (Bot2top) src += (Width*(Bpp/8))*(Height-1);
  vuint8 *dest = imgdata.ptr();
  for (int y = 0; y < Height; ++y) {
    const vuint8 *line = src;
    if (Bot2top) src -= Width*(Bpp/8); else src += Width*(Bpp/8);
    for (int x = 0; x < Width; ++x) {
      switch (Bpp) {
        case 8:
          {
            const vuint8 col = *line++;
            dest[0] = r_palette[col].r;
            dest[1] = r_palette[col].g;
            dest[2] = r_palette[col].b;
          }
          break;
        case 24:
          dest[0] = *line++;
          dest[1] = *line++;
          dest[2] = *line++;
          break;
        case 32:
          dest[0] = *line++;
          dest[1] = *line++;
          dest[2] = *line++;
          ++line; // skip alpha
          break;
        default: Sys_Error("the thing that should not be");
      }
      dest += 3;
    }
  }

  //stbi_flip_vertically_on_write(Bot2top ? 1 : 0);
  int res = stbi_write_jpg_to_func(&stbWriter, (void *)Strm, Width, Height, 3, imgdata.ptr(), clampval(jpeg_quality.asInt(), 1, 100));
  if (res && Strm->IsError()) res = 0;
  Strm->Close();
  delete Strm;

  if (!res) GCon->Logf(NAME_Error, "error writing '%s'", *FileName);
}

#endif


#endif
