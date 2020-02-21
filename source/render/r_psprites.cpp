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
//**  Refresh of things, i.e. objects represented by sprites.
//**
//**  Sprite rotation 0 is facing the viewer, rotation 1 is one angle turn
//**  CLOCKWISE around the axis. This is not the same as the angle, which
//**  increases counter clockwise (protractor). There was a lot of stuff
//**  grabbed wrong, so I changed it...
//**
//**************************************************************************
#include "gamedefs.h"
#include "r_local.h"
#include "sv_local.h"


extern VCvarF fov;
extern VCvarB gl_pic_filtering;
extern VCvarB r_draw_psprites;
extern VCvarB r_chasecam;
extern VCvarB r_drawfuzz;
extern VCvarF r_transsouls;

static VCvarI crosshair("crosshair", "2", "Crosshair type (0-2).", CVAR_Archive);
static VCvarF crosshair_alpha("crosshair_alpha", "0.6", "Crosshair opacity.", CVAR_Archive);
static VCvarI r_crosshair_yofs("r_crosshair_yofs", "0", "Crosshair y offset (>0: down).", CVAR_Archive);
static VCvarF crosshair_scale("crosshair_scale", "1", "Crosshair scale.", CVAR_Archive);


static int cli_WarnSprites = 0;
/*static*/ bool cliRegister_rsprites_args =
  VParsedArgs::RegisterFlagSet("-Wpsprite", "!show some warnings about sprites", &cli_WarnSprites);


//==========================================================================
//
//  showPSpriteWarnings
//
//==========================================================================
static bool showPSpriteWarnings () { return (cli_WAll > 0 || cli_WarnSprites > 0); }


//==========================================================================
//
//  VRenderLevelShared::RenderPSprite
//
//==========================================================================
void VRenderLevelShared::RenderPSprite (VViewState *VSt, const VAliasModelFrameInfo &mfi, float PSP_DIST, const RenderStyleInfo &ri) {
  spritedef_t *sprdef;
  spriteframe_t *sprframe;
  int lump;
  bool flip;

  if (ri.alpha < 0.004f) return; // no reason to render it, it is invisible
  //if (ri.alpha > 1.0f) ri.alpha = 1.0f;

  // decide which patch to use
  if ((unsigned)mfi.spriteIndex/*VSt->State->SpriteIndex*/ >= (unsigned)sprites.length()) {
    if (showPSpriteWarnings()) {
      GCon->Logf(NAME_Warning, "R_ProjectSprite: invalid sprite number %d", mfi.spriteIndex);
    }
    return;
  }
  sprdef = &sprites.ptr()[mfi.spriteIndex/*VSt->State->SpriteIndex*/];

  if (mfi.frame/*(VSt->State->Frame & VState::FF_FRAMEMASK)*/ >= sprdef->numframes) {
    if (showPSpriteWarnings()) {
      GCon->Logf(NAME_Warning, "R_ProjectSprite: invalid sprite frame %d : %d (max is %d)", mfi.spriteIndex, mfi.frame, sprdef->numframes);
    }
    return;
  }
  sprframe = &sprdef->spriteframes[mfi.frame/*VSt->State->Frame & VState::FF_FRAMEMASK*/];

  lump = sprframe->lump[0];
  if (lump < 0) {
    if (showPSpriteWarnings()) {
      GCon->Logf(NAME_Warning, "R_ProjectSprite: invalid sprite texture id %d in frame %d : %d", lump, mfi.spriteIndex, mfi.frame);
    }
    return;
  }
  flip = sprframe->flip[0];

  VTexture *Tex = GTextureManager[lump];
  if (!Tex) {
    if (showPSpriteWarnings()) {
      GCon->Logf(NAME_Warning, "RenderPSprite: invalid sprite texture id %d in frame %d : %d (the thing that should not be)", lump, mfi.spriteIndex, mfi.frame);
    }
    return;
  }

  const int TexWidth = Tex->GetWidth();
  const int TexHeight = Tex->GetHeight();
  const int TexSOffset = Tex->SOffset;
  const int TexTOffset = Tex->TOffset;

  //GCon->Logf(NAME_Debug, "PSPRITE: '%s'; size=(%d,%d); ofs=(%d,%d); scale=(%g,%g)", *Tex->Name, TexWidth, TexHeight, TexSOffset, TexTOffset, Tex->SScale, Tex->TScale);

  const float scaleX = max2(0.001f, Tex->SScale);
  const float scaleY = max2(0.001f, Tex->TScale);

  const float invScaleX = 1.0f/scaleX;
  const float invScaleY = 1.0f/scaleY;

  const float PSP_DISTI = 1.0f/PSP_DIST;
  TVec sprorigin = Drawer->vieworg+PSP_DIST*Drawer->viewforward;

  float sprx = 160.0f-VSt->SX+TexSOffset*invScaleX;
  float spry = 100.0f-VSt->SY+TexTOffset*invScaleY;

  spry -= cl->PSpriteSY;
  spry -= PSpriteOfsAspect;

  //k8: this is not right, but meh...
  if (fov > 90) {
    // this moves sprx to the left screen edge
    //sprx += (AspectEffectiveFOVX-1.0f)*160.0f;
  }

  //k8: don't even ask me!
  const float sxymul = (1.0f+(fov != 90 ? AspectEffectiveFOVX-1.0f : 0.0f))/160.0f;

  // horizontal
  TVec start = sprorigin-(sprx*PSP_DIST*sxymul)*Drawer->viewright;
  TVec end = start+(TexWidth*invScaleX*PSP_DIST*sxymul)*Drawer->viewright;

  TVec topdelta = (spry*PSP_DIST*sxymul)*Drawer->viewup;
  TVec botdelta = topdelta-(TexHeight*invScaleY*PSP_DIST*sxymul)*Drawer->viewup;

  // this puts psprite at the fixed screen position (only for horizontal FOV)
  if (!r_vertical_fov) {
    topdelta /= PixelAspect;
    botdelta /= PixelAspect;
  }

  TVec dv[4];
  dv[0] = start+botdelta;
  dv[1] = start+topdelta;
  dv[2] = end+topdelta;
  dv[3] = end+botdelta;

  TVec saxis(0, 0, 0);
  TVec taxis(0, 0, 0);
  TVec texorg(0, 0, 0);

  // texture scale
  const float axismul = 1.0f/160.0f/sxymul;

  const float saxmul = 160.0f*axismul;
  if (flip) {
    saxis = -(Drawer->viewright*saxmul*PSP_DISTI);
    texorg = dv[2];
  } else {
    saxis = Drawer->viewright*saxmul*PSP_DISTI;
    texorg = dv[1];
  }

  taxis = -(Drawer->viewup*100.0f*axismul*(320.0f/200.0f)*PSP_DISTI);

  saxis *= scaleX;
  taxis *= scaleY;

  Drawer->DrawSpritePolygon(dv, GTextureManager[lump], ri,
    nullptr, ColorMap, -Drawer->viewforward,
    DotProduct(dv[0], -Drawer->viewforward), saxis, taxis, texorg);
}


//==========================================================================
//
//  VRenderLevelShared::RenderViewModel
//
//  FIXME: this doesn't work with "----" and "####" view states
//
//==========================================================================
bool VRenderLevelShared::RenderViewModel (VViewState *VSt, const RenderStyleInfo &ri) {
  if (!r_models_view) return false;
  if (!R_HaveClassModelByName(VSt->State->Outer->Name)) return false;

  VMatrix4 oldProjMat;
  TClipBase old_clip_base = clip_base;

  // remporarily set FOV to 90
  const bool restoreFOV = (fov.asFloat() != 90.0f);

  if (restoreFOV) {
    refdef_t newrd = refdef;
    const float fov90 = CalcEffectiveFOV(90.0f, newrd);
    SetupRefdefWithFOV(&newrd, fov90);

    VMatrix4 newProjMat;
    Drawer->CalcProjectionMatrix(newProjMat, this, &newrd);

    Drawer->GetProjectionMatrix(oldProjMat);
    Drawer->SetProjectionMatrix(newProjMat);
  }

  TVec origin = Drawer->vieworg+(VSt->SX-1.0f)*Drawer->viewright/8.0f-(VSt->SY-32.0f)*Drawer->viewup/6.0f;

  float TimeFrac = 0;
  if (VSt->State->Time > 0) {
    TimeFrac = 1.0f-(VSt->StateTime/VSt->State->Time);
    TimeFrac = midval(0.0f, TimeFrac, 1.0f);
  }

  const bool res = DrawAliasModel(nullptr, VSt->State->Outer->Name, origin, cl->ViewAngles, 1.0f, 1.0f,
    VSt->State->getMFI(), (VSt->State->NextState ? VSt->State->NextState->getMFI() : VSt->State->getMFI()),
    nullptr, 0, ri, true, TimeFrac, r_interpolate_frames,
    RPASS_Normal);

  if (restoreFOV) {
    // restore original FOV (just in case)
    Drawer->SetProjectionMatrix(oldProjMat);
    clip_base = old_clip_base;
  }

  return res;
}


//==========================================================================
//
//  VRenderLevelShared::DrawPlayerSprites
//
//==========================================================================
void VRenderLevelShared::DrawPlayerSprites () {
  if (!r_draw_psprites || r_chasecam) return;
  if (!cl || !cl->MO) return;

  int RendStyle = STYLE_Normal;
  float Alpha = 1.0f;
  cl->MO->eventGetViewEntRenderParams(Alpha, RendStyle);

  RenderStyleInfo ri;
  if (!CalculateRenderStyleInfo(ri, RendStyle, Alpha)) return;

  int ltxr = 0, ltxg = 0, ltxb = 0;
  {
    static VClass *eclass = nullptr;
    if (!eclass) eclass = VClass::FindClass("Entity");
    // check if we have any light at player's origin (rough), and owned by player
    const dlight_t *dl = DLights;
    for (int dlcount = MAX_DLIGHTS; dlcount--; ++dl) {
      if (dl->die < Level->Time || dl->radius < 1.0f) continue;
      if (!dl->Owner || (dl->Owner->GetFlags()&(_OF_Destroyed|_OF_DelayedDestroy)) || !dl->Owner->IsA(eclass)) continue;
      VEntity *e = (VEntity *)dl->Owner;
      if ((e->EntityFlags&VEntity::EF_IsPlayer) == 0) continue;
      if (e != cl->MO) continue;
      if ((e->Origin-dl->origin).length() > dl->radius*0.75f) continue;
      ltxr += (dl->color>>16)&0xff;
      ltxg += (dl->color>>8)&0xff;
      ltxb += dl->color&0xff;
    }
  }

  // add all active psprites
  for (int i = 0; i < NUMPSPRITES; ++i) {
    if (!cl->ViewStates[i].State) continue;

    vuint32 light;
    if (RendStyle == STYLE_Fuzzy) {
      light = 0;
    } else if (cl->ViewStates[i].State->Frame&VState::FF_FULLBRIGHT) {
      light = 0xffffffff;
    } else {
      /*
      light = LightPoint(Drawer->vieworg, cl->MO->Radius, -1);
      if (ltxr|ltxg|ltxb) {
        //GCon->Logf("ltx=(%d,%d,%d)", ltxr, ltxg, ltxb);
        int r = max2(ltxr, (int)((light>>16)&0xff));
        int g = max2(ltxg, (int)((light>>8)&0xff));
        int b = max2(ltxb, (int)(light&0xff));
        light = (light&0xff000000u)|(((vuint32)clampToByte(r))<<16)|(((vuint32)clampToByte(g))<<8)|((vuint32)clampToByte(b));
      }
      */
      if (ltxr|ltxg|ltxb) {
        light = (0xff000000u)|(((vuint32)clampToByte(ltxr))<<16)|(((vuint32)clampToByte(ltxg))<<8)|((vuint32)clampToByte(ltxb));
      } else {
        light = LightPoint(Drawer->vieworg, cl->MO->Radius, -1);
      }
      //GCon->Logf("ltx=(%d,%d,%d)", ltxr, ltxg, ltxb);
      //light = (0xff000000u)|(((vuint32)clampToByte(ltxr))<<16)|(((vuint32)clampToByte(ltxg))<<8)|((vuint32)clampToByte(ltxb));
    }

    ri.light = ri.seclight = light;
    ri.fade = GetFade(SV_PointRegionLight(r_viewleaf->sector, cl->ViewOrg));

    const float currSX = cl->ViewStates[i].SX;
    const float currSY = cl->ViewStates[i].SY;

    float dur = cl->PSpriteWeaponLoweringDuration;
    if (dur > 0.0f) {
      float stt = cl->PSpriteWeaponLoweringStartTime;
      float currt = Level->Time;
      float t = currt-stt;
      float prevSY = cl->PSpriteWeaponLowerPrev;
      if (t >= 0.0f && t < dur) {
        float ydelta = fabs(prevSY-currSY)*(t/dur);
        //GCon->Logf("prev=%f; end=%f; curr=%f; dur=%f; t=%f; mul=%f; ydelta=%f", cl->PSpriteWeaponLowerPrev, currSY, prevSY+ydelta, dur, t, t/dur, ydelta);
        if (prevSY < currSY) {
          prevSY += ydelta;
          //GCon->Logf("DOWN: prev=%f; end=%f; curr=%f; dur=%f; t=%f; mul=%f; ydelta=%f", cl->PSpriteWeaponLowerPrev, currSY, prevSY, dur, t, t/dur, ydelta);
          if (prevSY >= currSY) {
            prevSY = currSY;
            cl->PSpriteWeaponLoweringDuration = 0.0f;
          }
        } else {
          prevSY -= ydelta;
          //GCon->Logf("UP: prev=%f; end=%f; curr=%f; dur=%f; t=%f; mul=%f; ydelta=%f", cl->PSpriteWeaponLowerPrev, currSY, prevSY, dur, t, t/dur, ydelta);
          if (prevSY <= currSY) {
            prevSY = currSY;
            cl->PSpriteWeaponLoweringDuration = 0.0f;
          }
        }
      } else {
        prevSY = currSY;
        cl->PSpriteWeaponLoweringDuration = 0.0f;
      }
      cl->ViewStates[i].SY = prevSY;
    }

    cl->ViewStates[i].SX += cl->ViewStates[i].BobOfsX;
    cl->ViewStates[i].SY += cl->ViewStates[i].OfsY+cl->ViewStates[i].BobOfsY;

    if (!RenderViewModel(&cl->ViewStates[i], ri)) {
      RenderPSprite(&cl->ViewStates[i], cl->getMFI(i), 3-i, ri);
    }

    cl->ViewStates[i].SX = currSX;
    cl->ViewStates[i].SY = currSY;
  }
}


//==========================================================================
//
//  VRenderLevelShared::DrawCrosshair
//
//==========================================================================
void VRenderLevelShared::DrawCrosshair () {
  static int prevCH = -666;
  int ch = (int)crosshair;
  const float scale = crosshair_scale.asFloat();
  float alpha = crosshair_alpha.asFloat();
  if (!isFiniteF(alpha)) alpha = 0;
  if (ch > 0 && ch < 16 && alpha > 0.0f && isFiniteF(scale) && scale > 0.0f) {
    static int handle = 0;
    if (!handle || prevCH != ch) {
      prevCH = ch;
      handle = GTextureManager.AddPatch(VName(va("croshai%x", ch), VName::AddLower8), TEXTYPE_Pic, true/*silent*/);
      if (handle < 0) handle = 0;
    }
    if (handle > 0) {
      //if (crosshair_alpha < 0.0f) crosshair_alpha = 0.0f;
      if (alpha > 1.0f) alpha = 1.0f;
      int cy = (screenblocks < 11 ? (VirtualHeight-sb_height)/2 : VirtualHeight/2);
      cy += r_crosshair_yofs;
      bool oldflt = gl_pic_filtering;
      gl_pic_filtering = false;
      R_DrawPicScaled(VirtualWidth/2, cy, handle, scale, alpha);
      gl_pic_filtering = oldflt;
    }
  }
}
