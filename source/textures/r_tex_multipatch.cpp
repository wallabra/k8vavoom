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
#include "r_tex.h"
#include "render/r_local.h"


//==========================================================================
//
//  VMultiPatchTexture::VMultiPatchTexture
//
//==========================================================================
VMultiPatchTexture::VMultiPatchTexture (VStream &Strm, int DirectoryIndex,
    TArray<VTextureManager::WallPatchInfo> &patchlist, int FirstTex, bool IsStrife)
  : VTexture()
{
  Type = TEXTYPE_Wall;
  mFormat = TEXFMT_8;

  // read offset and seek to the starting position
  Strm.Seek(4+DirectoryIndex*4);
  vint32 Offset = Streamer<vint32>(Strm);
  if (Offset < 0 || Offset >= Strm.TotalSize()) Sys_Error("InitTextures: bad texture directory");
  Strm.Seek(Offset);

  // read name
  char TmpName[12];
  Strm.Serialise(TmpName, 8);
  TmpName[8] = 0;
  Name = VName(TmpName, VName::AddLower8);

  // in Doom, textures were searched from the beginning, so to avoid
  // problems, especially with animated textures, set name to a blank one
  // if this one is a duplicate
  /*
  if (GTextureManager.CheckNumForName(Name, TEXTYPE_Wall, false, false) >= FirstTex) {
    GCon->Logf(NAME_Warning, "duplicate multipatch texture '%s'", *Name);
    //Name = NAME_None;
  }
  */

  // skip unused value
  Streamer<vint16>(Strm); // masked, unused

  // read scaling
  vuint8 TmpSScale = Streamer<vuint8>(Strm);
  vuint8 TmpTScale = Streamer<vuint8>(Strm);
  SScale = (TmpSScale ? TmpSScale/8.0 : 1.0);
  TScale = (TmpTScale ? TmpTScale/8.0 : 1.0);

  // read dimensions
  Width = Streamer<vint16>(Strm);
  Height = Streamer<vint16>(Strm);

  // skip unused value
  if (!IsStrife) Streamer<vint32>(Strm); // ColumnDirectory, unused

  // create list of patches
  PatchCount = Streamer<vint16>(Strm);
  Patches = new VTexPatch[PatchCount];
  memset((void *)Patches, 0, sizeof(VTexPatch)*PatchCount);

  static int dumpMPT = -1;
  if (dumpMPT < 0) dumpMPT = (GArgs.CheckParm("-dump-multipatch-texture-data") ? 1 : 0);

  if (dumpMPT > 0) GCon->Logf("=== MULTIPATCH DATA FOR '%s' (%s) (size:%dx%d, scale:%g/%g) ===", *Strm.GetName(), TmpName, Width, Height, SScale, TScale);

  // read patches
  bool warned = false;
  VTexPatch *patch = Patches;
  for (int i = 0; i < PatchCount; ++i, ++patch) {
    // read origin
    patch->XOrigin = Streamer<vint16>(Strm);
    patch->YOrigin = Streamer<vint16>(Strm);
    patch->Rot = 0;
    patch->Trans = nullptr;
    patch->bOwnTrans = false;
    patch->Blend.r = 0;
    patch->Blend.g = 0;
    patch->Blend.b = 0;
    patch->Blend.a = 0;
    patch->Style = STYLE_Copy;
    patch->Alpha = 1.0;

    // read patch index and find patch texture
    vint16 PatchIdx = Streamer<vint16>(Strm);
    // one patch with index `-1` means "use the same named texture, and get out"
    if (PatchIdx == -1 && PatchCount == 1) {
      int tid = GTextureManager.CheckNumForNameAndForce(Name, TEXTYPE_Any, true, true, false);
      patch->Tex = (tid >= 0 ? GTextureManager[tid] : nullptr);
    } else if (PatchIdx < 0 || PatchIdx >= patchlist.length()) {
      //Sys_Error("InitTextures: Bad patch index in texture %s (%d/%d)", *Name, PatchIdx, NumPatchLookup-1);
      if (!warned) { warned = true; GCon->Logf(NAME_Warning, "InitTextures: Bad %spatch index (%d/%d) in texture \"%s\" (%d/%d)", (IsStrife ? "Strife " : ""), PatchIdx, patchlist.length()-1, *Name, i, PatchCount-1); }
      patch->Tex = nullptr;
    } else {
      patch->Tex = patchlist[PatchIdx].tx;
      if (!patch->Tex) {
        if (!warned) { warned = true; GCon->Logf(NAME_Warning, "InitTextures: Missing patch \"%s\" (%d) in texture \"%s\" (%d/%d)", *patchlist[PatchIdx].name, patchlist[PatchIdx].index, *Name, i, PatchCount-1); }
      }
    }

    if (dumpMPT > 0) {
      GCon->Logf("  patch #%d/%d: xorg=%d; yorg=%d; patch=%s", i, PatchCount-1, patch->XOrigin, patch->YOrigin, (patch->Tex ? *patch->Tex->Name : "(none)"));
    }

    if (!patch->Tex) {
      --i;
      --PatchCount;
      --patch;
    }

    // skip unused values
    if (!IsStrife) {
      Streamer<vint16>(Strm); // Step dir, unused
      Streamer<vint16>(Strm); // Colour map, unused
    }
  }

  // fix sky texture heights for Heretic, but it can also be used for Doom and Strife
  if (!VStr::NICmp(*Name, "sky", 3) && Height == 128) {
    if (Patches[0].Tex && Patches[0].Tex->GetHeight() > Height) {
      Height = Patches[0].Tex->GetHeight();
    }
  }
}


//==========================================================================
//
//  VMultiPatchTexture::VMultiPatchTexture
//
//==========================================================================
VMultiPatchTexture::VMultiPatchTexture (VScriptParser *sc, int AType)
  : PatchCount(0)
  , Patches(nullptr)
{
  Type = AType;
  mFormat = TEXFMT_8;

  sc->SetCMode(true);

  sc->ResetQuoted();
  sc->ExpectString();
  if (!sc->QuotedString && sc->String.ICmp("optional") == 0) {
    //FIXME!
    GCon->Logf(NAME_Warning, "%s: 'optional' is doing the opposite of what it should, lol", *sc->GetLoc().toStringNoCol());
    sc->ExpectString();
  }

  Name = VName(*sc->String, VName::AddLower8);
  sc->Expect(",");
  sc->ExpectNumber();
  Width = sc->Number;
  sc->Expect(",");
  sc->ExpectNumber();
  Height = sc->Number;

  if (sc->Check("{")) {
    TArray<VTexPatch> Parts;
    while (!sc->Check("}")) {
      if (sc->Check("offset")) {
        sc->ExpectNumberWithSign();
        SOffset = sc->Number;
        sc->Expect(",");
        sc->ExpectNumberWithSign();
        TOffset = sc->Number;
      } else if (sc->Check("xscale")) {
        sc->ExpectFloatWithSign();
        SScale = sc->Float;
      } else if (sc->Check("yscale")) {
        sc->ExpectFloatWithSign();
        TScale = sc->Float;
      } else if (sc->Check("worldpanning")) {
        bWorldPanning = true;
      } else if (sc->Check("nulltexture")) {
        Type = TEXTYPE_Null;
      } else if (sc->Check("nodecals")) {
        noDecals = true;
        staticNoDecals = true;
        animNoDecals = true;
      } else if (sc->Check("patch") || sc->Check("graphic")) {
        VTexPatch &P = Parts.Alloc();
        sc->ExpectString();
        VName PatchName = VName(*sc->String.ToLower());
        int Tex = GTextureManager.CheckNumForName(PatchName, TEXTYPE_WallPatch, false, false);
        // try other texture types, why not?
        if (Tex < 0) GTextureManager.CheckNumForName(PatchName, TEXTYPE_Flat, false, false);
        if (Tex < 0) GTextureManager.CheckNumForName(PatchName, TEXTYPE_Wall, false, false);
        if (Tex < 0) GTextureManager.CheckNumForName(PatchName, TEXTYPE_Sprite, false, false);
        if (Tex < 0) {
          int LumpNum = W_CheckNumForTextureFileName(sc->String);
          if (LumpNum >= 0) {
            Tex = GTextureManager.FindTextureByLumpNum(LumpNum);
            if (Tex < 0) {
              VTexture *T = CreateTexture(TEXTYPE_WallPatch, LumpNum);
              if (!T) T = CreateTexture(TEXTYPE_Any, LumpNum);
              if (T) {
                Tex = GTextureManager.AddTexture(T);
                T->Name = NAME_None;
              }
            }
          } else if (sc->String.Length() <= 8) {
            //if (warn) fprintf(stderr, "*********************\n");
            LumpNum = W_CheckNumForName(PatchName, WADNS_Patches);
            if (LumpNum < 0) LumpNum = W_CheckNumForTextureFileName(*PatchName);
            if (LumpNum >= 0) {
              Tex = GTextureManager.AddTexture(CreateTexture(TEXTYPE_WallPatch, LumpNum));
            } else {
              // DooM:Complete has some patches in "sprites/"
              LumpNum = W_CheckNumForName(PatchName, WADNS_Sprites);
              if (LumpNum < 0) LumpNum = W_CheckNumForName(PatchName, WADNS_Graphics);
              if (LumpNum < 0) LumpNum = W_CheckNumForName(PatchName, WADNS_Flats); //k8: why not?
              if (LumpNum >= 0) {
                Tex = GTextureManager.AddTexture(CreateTexture(TEXTYPE_Any, LumpNum));
              }
            }
          }
        }
        if (Tex < 0) {
          GCon->Logf(NAME_Warning, "%s: Unknown patch '%s' in texture '%s'", *sc->GetLoc().toStringNoCol(), *sc->String, *Name);
          //int LumpNum = W_CheckNumForTextureFileName("-noflat-");
          //if (LumpNum >= 0) Tex = GTextureManager.AddTexture(CreateTexture(TEXTYPE_WallPatch, LumpNum));
          P.Tex = nullptr;
        } else {
          P.Tex = GTextureManager[Tex];
        }

        // parse origin
        sc->Expect(",");
        sc->ExpectNumberWithSign();
        P.XOrigin = sc->Number;
        sc->Expect(",");
        sc->ExpectNumberWithSign();
        P.YOrigin = sc->Number;

        // initialise parameters
        int Flip = 0;
        P.Rot = 0;
        P.Trans = nullptr;
        P.bOwnTrans = false;
        P.Blend.r = 0;
        P.Blend.g = 0;
        P.Blend.b = 0;
        P.Blend.a = 0;
        P.Style = STYLE_Copy;
        P.Alpha = 1.0;

        if (sc->Check("{")) {
          while (!sc->Check("}")) {
            bool expectStyle = false;
            if (sc->Check("flipx")) {
              Flip |= 1;
            } else if (sc->Check("flipy")) {
              Flip |= 2;
            } else if (sc->Check("useoffsets")) {
              //FIXME!
              GCon->Logf(NAME_Warning, "%s: 'UseOffsets' is not supported yet", *sc->GetLoc().toStringNoCol());
            } else if (sc->Check("rotate")) {
              sc->ExpectNumberWithSign();
              int Rot = ((sc->Number+90)%360)-90;
              if (Rot != 0 && Rot != 90 && Rot != 180 && Rot != -90) sc->Error("Rotation must be a multiple of 90 degrees.");
              P.Rot = (Rot/90)&3;
            } else if (sc->Check("translation")) {
              mFormat = TEXFMT_RGBA;
              if (P.bOwnTrans) {
                delete P.Trans;
                P.Trans = nullptr;
                P.bOwnTrans = false;
              }
              P.Trans = nullptr;
              P.Blend.r = 0;
              P.Blend.g = 0;
              P.Blend.b = 0;
              P.Blend.a = 0;

                   if (sc->Check("inverse")) P.Trans = &ColourMaps[CM_Inverse];
              else if (sc->Check("gold")) P.Trans = &ColourMaps[CM_Gold];
              else if (sc->Check("red")) P.Trans = &ColourMaps[CM_Red];
              else if (sc->Check("green")) P.Trans = &ColourMaps[CM_Green];
              else if (sc->Check("ice")) P.Trans = &IceTranslation;
              else if (sc->Check("desaturate")) { sc->Expect(","); sc->ExpectNumber(); }
              else {
                P.Trans = new VTextureTranslation();
                P.bOwnTrans = true;
                do {
                  sc->ExpectString();
                  P.Trans->AddTransString(sc->String);
                } while (sc->Check(","));
              }
            } else if (sc->Check("blend")) {
              mFormat = TEXFMT_RGBA;
              if (P.bOwnTrans) {
                delete P.Trans;
                P.Trans = nullptr;
                P.bOwnTrans = false;
              }
              P.Trans = nullptr;
              P.Blend.r = 0;
              P.Blend.g = 0;
              P.Blend.b = 0;
              P.Blend.a = 0;

              if (!sc->CheckNumber()) {
                sc->ExpectString();
                vuint32 Col = M_ParseColour(*sc->String);
                P.Blend.r = (Col>>16)&0xff;
                P.Blend.g = (Col>>8)&0xff;
                P.Blend.b = Col&0xff;
              } else {
                P.Blend.r = clampToByte((int)sc->Number);
                sc->Expect(",");
                sc->ExpectNumber();
                P.Blend.g = clampToByte((int)sc->Number);
                sc->Expect(",");
                sc->ExpectNumber();
                P.Blend.b = clampToByte((int)sc->Number);
                //sc->Expect(",");
              }
              if (sc->Check(",")) {
                sc->ExpectFloat();
                P.Blend.a = clampToByte((int)(sc->Float*255));
              } else {
                P.Blend.a = 255;
              }
            }
            else if (sc->Check("alpha")) {
              sc->ExpectFloat();
              P.Alpha = MID(0.0f, (float)sc->Float, 1.0f);
            } else {
              if (sc->Check("style")) expectStyle = true;
                   if (sc->Check("copy")) P.Style = STYLE_Copy;
              else if (sc->Check("translucent")) P.Style = STYLE_Translucent;
              else if (sc->Check("add")) P.Style = STYLE_Add;
              else if (sc->Check("subtract")) P.Style = STYLE_Subtract;
              else if (sc->Check("reversesubtract")) P.Style = STYLE_ReverseSubtract;
              else if (sc->Check("modulate")) P.Style = STYLE_Modulate;
              else if (sc->Check("copyalpha")) P.Style = STYLE_CopyAlpha;
              // Overlay: This is the same as CopyAlpha, except it only copies the patch's alpha channel where it has a higher alpha than what's underneath.
              else if (sc->Check("overlay")) {
                //FIXME
                GCon->Logf(NAME_Warning, "%s: unsupported texture style 'Overlay', approximated with 'CopyAlpha'", *sc->GetLoc().toStringNoCol());
                P.Style = STYLE_CopyAlpha;
              } else {
                if (expectStyle) {
                  sc->Error(va("Bad style: '%s'", *sc->String));
                } else {
                  sc->Error(va("Bad texture patch command '%s'", *sc->String));
                }
              }
              if (P.Style != STYLE_Copy) mFormat = TEXFMT_RGBA;
            }
          }
        }

        if (Flip&2) {
          P.Rot = (P.Rot+2)&3;
          Flip ^= 1;
        }
        if (Flip&1) {
          P.Rot |= 4;
        }
      } else {
        sc->Error(va("Bad texture command '%s'", *sc->String));
      }
    }

    PatchCount = Parts.Num();
    Patches = new VTexPatch[PatchCount];
    memcpy(Patches, Parts.Ptr(), sizeof(VTexPatch)*PatchCount);
  }

  sc->SetCMode(false);
}


//==========================================================================
//
//  VMultiPatchTexture::~VMultiPatchTexture
//
//==========================================================================
VMultiPatchTexture::~VMultiPatchTexture () {
  if (Patches) {
    for (int i = 0; i < PatchCount; ++i) {
      if (Patches[i].bOwnTrans) {
        delete Patches[i].Trans;
        Patches[i].Trans = nullptr;
      }
    }
    delete[] Patches;
    Patches = nullptr;
  }
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
}


//==========================================================================
//
//  VMultiPatchTexture::SetFrontSkyLayer
//
//==========================================================================
void VMultiPatchTexture::SetFrontSkyLayer () {
  for (int i = 0; i < PatchCount; ++i) Patches[i].Tex->SetFrontSkyLayer();
  bNoRemap0 = true;
}


//==========================================================================
//
//  VMultiPatchTexture::GetPixels
//
// using the texture definition, the composite texture is created from the
// patches, and each column is cached
//
//==========================================================================
vuint8 *VMultiPatchTexture::GetPixels () {
  // if already got pixels, then just return them.
  if (Pixels) return Pixels;
  transparent = false;

  static int dumpPng = -1;
  if (dumpPng < 0) dumpPng = (GArgs.CheckParm("-dump-multipatch-textures") ? 1 : 0);

  if (dumpPng > 0 && Name != "doortrak") dumpPng = -1;

  // load all patches, if any of them is not in standard palette, then switch to 32 bit mode
  for (int i = 0; i < PatchCount; ++i) {
    if (!Patches[i].Tex) continue;
    Patches[i].Tex->GetPixels();
    if (dumpPng > 0) {
      //Patches[i].Tex->ConvertPixelsToRGBA();
      VStream *strm = nullptr;
      VStr basename = VStr(Name != NAME_None ? *Name : "_untitled");
      basename += "_patches_";
      if (Patches[i].Tex->Name != NAME_None) basename += *Patches[i].Tex->Name; else basename += "untitled";
      for (int count = 0; ; ++count) {
        VStr fname = va("_texdump/%s_%04d_%04d.png", *basename, count, i);
        if (!Sys_FileExists(fname)) {
          strm = FL_OpenFileWrite(fname, true); // as full name
          if (strm) {
            WriteToPNG(strm);
            delete strm;
          }
          break;
        }
      }
    }
    if (Patches[i].Tex->Format != TEXFMT_8) mFormat = TEXFMT_RGBA;
  }

  if (Format == TEXFMT_8) {
    Pixels = new vuint8[Width*Height];
    memset(Pixels, 0, Width*Height);
  } else {
    Pixels = new vuint8[Width*Height*4];
    memset(Pixels, 0, Width*Height*4);
  }

  // composite the columns together
  VTexPatch *patch = Patches;
  for (int i = 0; i < PatchCount; ++i, ++patch) {
    VTexture *PatchTex = patch->Tex;
    if (!PatchTex) continue;
    vuint8 *PatchPixels = (patch->Trans ? PatchTex->GetPixels8() : PatchTex->GetPixels());
    int PWidth = PatchTex->GetWidth();
    int PHeight = PatchTex->GetHeight();
    int x1 = (patch->XOrigin < 0 ? 0 : patch->XOrigin);
    int x2 = x1+(patch->Rot&1 ? PHeight : PWidth);
    if (x2 > Width) x2 = Width;
    int y1 = (patch->YOrigin < 0 ? 0 : patch->YOrigin);
    int y2 = y1+(patch->Rot&1 ? PWidth : PHeight);
    if (y2 > Height) y2 = Height;
    float IAlpha = 1.0-patch->Alpha;
    if (IAlpha < 0) IAlpha = 0; else if (IAlpha > 1) IAlpha = 1;

    for (int y = y1 < 0 ? 0 : y1; y < y2; ++y) {
      int PIdxY;
      switch (patch->Rot) {
        case 0: case 4: PIdxY = (y-y1)*PWidth; break;
        case 1: case 7: PIdxY = y-y1; break;
        case 2: case 6: PIdxY = (PHeight-y+y1-1)*PWidth; break;
        case 3: case 5: PIdxY = PWidth-y+y1-1; break;
        default: Sys_Error("invalid `patch->Rot` in `VMultiPatchTexture::GetPixels()` (PIdxY)");
      }

      for (int x = x1 < 0 ? 0 : x1; x < x2; ++x) {
        int PIdx;
        switch (patch->Rot) {
          case 0: case 6: PIdx = (x-x1)+PIdxY; break;
          case 1: case 5: PIdx = (PHeight-x+x1-1)*PWidth+PIdxY; break;
          case 2: case 4: PIdx = (PWidth-x+x1-1)+PIdxY; break;
          case 3: case 7: PIdx = (x-x1)*PWidth+PIdxY; break;
          default: Sys_Error("invalid `patch->Rot` in `VMultiPatchTexture::GetPixels() (PIdx)`");
        }

        if (Format == TEXFMT_8) {
          // patch texture is guaranteed to be paletted
          if (PatchPixels[PIdx]) {
            Pixels[x+y*Width] = PatchPixels[PIdx];
          }
        } else {
          // get pixel
          rgba_t col = rgba_t(0, 0, 0, 0);

          if (patch->Trans) {
            col = patch->Trans->GetPalette()[PatchPixels[PIdx]];
          } else {
            switch (PatchTex->Format) {
              case TEXFMT_8: col = r_palette[PatchPixels[PIdx]]; break;
              case TEXFMT_8Pal: col = PatchTex->GetPalette()[PatchPixels[PIdx]]; break;
              case TEXFMT_RGBA: col = ((rgba_t *)PatchPixels)[PIdx]; break;
            }
          }
          if (patch->Blend.a == 255) {
            col.r = col.r*patch->Blend.r/255;
            col.g = col.g*patch->Blend.g/255;
            col.b = col.b*patch->Blend.b/255;
          } else if (patch->Blend.a) {
            col.r = col.r*(255-patch->Blend.a)/255+patch->Blend.r*patch->Blend.a/255;
            col.g = col.g*(255-patch->Blend.a)/255+patch->Blend.g*patch->Blend.a/255;
            col.b = col.b*(255-patch->Blend.a)/255+patch->Blend.b*patch->Blend.a/255;
          }

          // add to texture
          if (col.a) {
            rgba_t &Dst = ((rgba_t *)Pixels)[x+y*Width];
            switch (patch->Style) {
              case STYLE_Copy:
                Dst = col;
                break;
              case STYLE_Translucent:
                Dst.r = vuint8(Dst.r*IAlpha+col.r*patch->Alpha);
                Dst.g = vuint8(Dst.g*IAlpha+col.g*patch->Alpha);
                Dst.b = vuint8(Dst.b*IAlpha+col.b*patch->Alpha);
                Dst.a = col.a;
                break;
              case STYLE_Add:
                Dst.r = MIN(Dst.r+vuint8(col.r*patch->Alpha), 255);
                Dst.g = MIN(Dst.g+vuint8(col.g*patch->Alpha), 255);
                Dst.b = MIN(Dst.b+vuint8(col.b*patch->Alpha), 255);
                Dst.a = col.a;
                break;
              case STYLE_Subtract:
                Dst.r = MAX(Dst.r-vuint8(col.r*patch->Alpha), 0);
                Dst.g = MAX(Dst.g-vuint8(col.g*patch->Alpha), 0);
                Dst.b = MAX(Dst.b-vuint8(col.b*patch->Alpha), 0);
                Dst.a = col.a;
                break;
              case STYLE_ReverseSubtract:
                Dst.r = MAX(vuint8(col.r*patch->Alpha)-Dst.r, 0);
                Dst.g = MAX(vuint8(col.g*patch->Alpha)-Dst.g, 0);
                Dst.b = MAX(vuint8(col.b*patch->Alpha)-Dst.b, 0);
                Dst.a = col.a;
                break;
              case STYLE_Modulate:
                Dst.r = Dst.r*col.r/255;
                Dst.g = Dst.g*col.g/255;
                Dst.b = Dst.b*col.b/255;
                Dst.a = col.a;
                break;
              case STYLE_CopyAlpha:
                if (col.a == 255) {
                  Dst = col;
                } else {
                  float a = col.a/255.0;
                  float ia = (255.0-col.a)/255.0;
                  Dst.r = vuint8(Dst.r*ia+col.r*a);
                  Dst.g = vuint8(Dst.g*ia+col.g*a);
                  Dst.b = vuint8(Dst.b*ia+col.b*a);
                  Dst.a = col.a;
                }
                break;
            }
          }
        }
      }
    }
  }

  if (Width > 0 && Height > 0) {
    if (Format == TEXFMT_8) {
      const vuint8 *s = Pixels;
      for (int count = Width*Height; count--; ++s) {
        if (s[0] == 0) { transparent = true; break; }
      }
    } else {
      const rgba_t *s = (const rgba_t *)Pixels;
      for (int count = Width*Height; count--; ++s) {
        if (s->a != 255) { transparent = true; break; }
      }
    }
  }

  ConvertPixelsToShaded();

  if (dumpPng > 0) {
    VStream *strm = nullptr;
    VStr basename = VStr(Name != NAME_None ? *Name : "_untitled");
    for (int counter = 0; ; ++counter) {
      VStr fname = VStr(counter ? va("_texdump/%s_%04d.png", *basename, counter) : va("_texdump/%s.png", *basename));
      if (!Sys_FileExists(fname)) {
        strm = FL_OpenFileWrite(fname, true); // as full name
        break;
      }
    }
    if (strm) {
      WriteToPNG(strm);
      delete strm;
    }
  }

  return Pixels;
}


//==========================================================================
//
//  VMultiPatchTexture::Unload
//
//==========================================================================
void VMultiPatchTexture::Unload () {
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
}
