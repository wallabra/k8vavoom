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
#include "../../gamedefs.h"


static VCvarB loader_force_fix_2s("loader_force_fix_2s", false, "Force-fix invalid two-sided flags? (non-persistent)", CVAR_PreInit/*|CVAR_Archive*/);


//==========================================================================
//
//  VLevel::CreateSides
//
//==========================================================================
void VLevel::CreateSides () {
  // perform side index and two-sided flag checks and count number of sides needed
  int dummySideCount = 0;
  int NumNewSides = 0;

  line_t *Line = Lines;
  for (int i = 0; i < NumLines; ++i, ++Line) {
    if (Line->sidenum[0] == -1) {
      if (Line->sidenum[1] == -1) {
        // UDMF control line
        GCon->Logf(NAME_Debug, "Line %d is sideless control line", i);
        Line->flags &= ~ML_TWOSIDED; // just in case
        continue;
      }
      GCon->Logf(NAME_Error, "Line %d has no front side", i);
      //++dummySideCount;
    } else {
      if (Line->sidenum[0] < 0 || Line->sidenum[0] >= NumSides) {
        //Host_Error("Bad sidedef index %d", Line->sidenum[0]);
        GCon->Logf(NAME_Error, "Bad sidedef index %d for linedef #%d", Line->sidenum[0], i);
        Line->sidenum[0] = -1;
        ++dummySideCount;
      } else {
        ++NumNewSides;
      }
    }

    if (Line->sidenum[1] != -1 && (Line->sidenum[1] < 0 || Line->sidenum[1] >= NumSides)) {
      //Host_Error("Bad sidedef index %d for linedef #%d", Line->sidenum[1], i);
      GCon->Logf(NAME_Error, "Bad second sidedef index %d for linedef #%d", Line->sidenum[1], i);
      Line->sidenum[1] = -1;
    }

    if (Line->sidenum[1] != -1) {
      // has second side
      // just a warning (and a fix)
      if ((Line->flags&ML_TWOSIDED) == 0) {
        if (loader_force_fix_2s) {
          GCon->Logf(NAME_Warning, "Linedef #%d marked as two-sided but has no TWO-SIDED flag set", i);
          Line->flags |= ML_TWOSIDED; //k8: we need to set this, or clipper will glitch
        }
      }
      ++NumNewSides;
    } else {
      // no second side, but marked as two-sided
      if (Line->flags&ML_TWOSIDED) {
        //if (strict_level_errors) Host_Error("Bad WAD: Line %d is marked as TWO-SIDED but has only one side", i);
        GCon->Logf(NAME_Warning, "Linedef #%d is marked as TWO-SIDED but has only one side", i);
        Line->flags &= ~ML_TWOSIDED;
      }
    }
    //fprintf(stderr, "linedef #%d: sides=(%d,%d); two-sided=%s\n", i, Line->sidenum[0], Line->sidenum[1], (Line->flags&ML_TWOSIDED ? "tan" : "ona"));
  }


  // allocate memory for sidedefs
  Sides = new side_t[NumNewSides+dummySideCount+1];
  memset((void *)Sides, 0, sizeof(side_t)*(NumNewSides+dummySideCount+1));

  for (int f = 0; f < NumNewSides+dummySideCount+1; ++f) {
    Sides[f].Top.ScaleX = Sides[f].Top.ScaleY = 1.0f;
    Sides[f].Bot.ScaleX = Sides[f].Bot.ScaleY = 1.0f;
    Sides[f].Mid.ScaleX = Sides[f].Mid.ScaleY = 1.0f;
  }

  // create dummy sidedef
  if (dummySideCount) {
    for (int f = NumNewSides; f < NumNewSides+dummySideCount+1; ++f) {
      side_t *ds = &Sides[f];
      ds->TopTexture = ds->BottomTexture = ds->MidTexture = GTextureManager.DefaultTexture;
      ds->Flags = SDF_ABSLIGHT;
      ds->Light = 255;
      // `Sector` and `LineNum` will be set later
    }
  }

  int CurrentSide = 0;
  int CurrentDummySide = NumNewSides;
  Line = Lines;
  for (int i = 0; i < NumLines; ++i, ++Line) {
    if (Line->sidenum[0] == -1 && Line->sidenum[1] == -1) continue; // skip control lines

    vassert(Line->sidenum[0] >= -1 && Line->sidenum[0] < NumSides);
    vassert(Line->sidenum[1] >= -1 && Line->sidenum[1] < NumSides);

    side_t *fside;
    if (Line->sidenum[0] != -1) {
      fside = &Sides[CurrentSide];
      fside->BottomTexture = Line->sidenum[0]; //k8: this is for UDMF
      fside->LineNum = i;
      Line->sidenum[0] = CurrentSide++;
    } else {
      // let it glitch...
      fside = nullptr;
      Line->sidenum[0] = CurrentDummySide;
      side_t *ds = &Sides[CurrentDummySide++];
      ds->LineNum = i;
      ds->Sector = &Sectors[0]; //FIXME
      GCon->Logf(NAME_Debug, "Linedef #%d front side is dummy side #%d", i, CurrentDummySide-NumNewSides);
    }

    if (Line->sidenum[1] != -1) {
      side_t *ds = &Sides[CurrentSide];
      ds->BottomTexture = Line->sidenum[1]; //k8: this is for UDMF
      ds->LineNum = i;
      Line->sidenum[1] = CurrentSide++;
    }

    // assign line specials to sidedefs midtexture and arg1 to toptexture
    if (Line->special == LNSPEC_StaticInit && Line->arg2 != 1) continue;

    if (fside) {
      fside->MidTexture = Line->special;
      fside->TopTexture = Line->arg1;
    }
  }

  vassert(CurrentSide == NumNewSides);
  vassert(CurrentDummySide == NumNewSides+dummySideCount);

  NumSides = NumNewSides+dummySideCount;
}


//==========================================================================
//
//  VLevel::LoadSideDefs
//
//==========================================================================
void VLevel::LoadSideDefs (int Lump) {
  NumSides = W_LumpLength(Lump)/30;
  if (NumSides <= 0) Host_Error("Map '%s' has no sides!", *MapName);
  CreateSides();

  // load data
  VStream *lumpstream = W_CreateLumpReaderNum(Lump);
  VCheckedStream Strm(lumpstream);
  side_t *sd = Sides;
  for (int i = 0; i < NumSides; ++i, ++sd) {
    if (sd->Flags == SDF_ABSLIGHT) continue; // dummy side

    Strm.Seek(sd->BottomTexture*30);
    vint16 textureoffset;
    vint16 rowoffset;
    char toptexture[9];
    char bottomtexture[9];
    char midtexture[9];
    memset(toptexture, 0, sizeof(toptexture));
    memset(bottomtexture, 0, sizeof(bottomtexture));
    memset(midtexture, 0, sizeof(midtexture));
    vint16 sector;
    Strm << textureoffset << rowoffset;
    Strm.Serialise(toptexture, 8);
    Strm.Serialise(bottomtexture, 8);
    Strm.Serialise(midtexture, 8);
    Strm << sector;

    if (sector < 0 || sector >= NumSectors) Host_Error("Bad sector index %d", sector);

    sd->Top.TextureOffset = textureoffset;
    sd->Bot.TextureOffset = textureoffset;
    sd->Mid.TextureOffset = textureoffset;
    sd->Top.RowOffset = rowoffset;
    sd->Bot.RowOffset = rowoffset;
    sd->Mid.RowOffset = rowoffset;
    sd->Sector = &Sectors[sector];

    switch (sd->MidTexture) {
      case LNSPEC_LineTranslucent:
        // in BOOM midtexture can be translucency table lump name
        sd->MidTexture = GTextureManager.CheckNumForName(VName(midtexture, VName::AddLower8), TEXTYPE_Wall, true);
        if (sd->MidTexture == -1) sd->MidTexture = 0;
        sd->TopTexture = TexNumForName(toptexture, TEXTYPE_Wall);
        sd->BottomTexture = TexNumForName(bottomtexture, TEXTYPE_Wall);
        break;

      case LNSPEC_TransferHeights:
        sd->MidTexture = TexNumForName(midtexture, TEXTYPE_Wall, true);
        sd->TopTexture = TexNumForName(toptexture, TEXTYPE_Wall, true);
        sd->BottomTexture = TexNumForName(bottomtexture, TEXTYPE_Wall, true);
        break;

      case LNSPEC_StaticInit:
        {
          bool HaveCol = false;
          bool HaveFade = false;
          vuint32 Col = 0;
          vuint32 Fade = 0;
          sd->MidTexture = TexNumForName(midtexture, TEXTYPE_Wall);
          int TmpTop = TexNumOrColor(toptexture, TEXTYPE_Wall, HaveCol, Col);
          sd->BottomTexture = TexNumOrColor(bottomtexture, TEXTYPE_Wall, HaveFade, Fade);
          if (HaveCol || HaveFade) {
            //GCon->Logf("*** fade=0x%08x (%s); light=0x%08x (%s)", Col, toptexture, Fade, bottomtexture);
            for (int j = 0; j < NumSectors; ++j) {
              if (Sectors[j].IsTagEqual(sd->TopTexture)) {
                if (HaveCol) {
                  //GCon->Logf("old sector light=0x%08x; new=0x%08x", (vuint32)Sectors[j].params.LightColor, Col);
                  Sectors[j].params.LightColor = Col;
                }
                if (HaveFade) Sectors[j].params.Fade = Fade;
              }
            }
          } else {
            //GCon->Logf("*** FUCKED fade=(%s); light=(%s)", toptexture, bottomtexture);
          }
          sd->TopTexture = TmpTop;
        }
        break;

      default:
        sd->MidTexture = TexNumForName(midtexture, TEXTYPE_Wall);
        sd->TopTexture = TexNumForName(toptexture, TEXTYPE_Wall);
        sd->BottomTexture = TexNumForName(bottomtexture, TEXTYPE_Wall);
        break;
    }

    if (sd->TopTexture == 0 && toptexture[0] != '-' && toptexture[1]) sd->Flags |= SDF_AAS_TOP;
    if (sd->BottomTexture == 0 && bottomtexture[0] != '-' && bottomtexture[1]) sd->Flags |= SDF_AAS_BOT;
    if (sd->MidTexture == 0 && midtexture[0] != '-' && midtexture[1]) sd->Flags |= SDF_AAS_MID;
  }
}
