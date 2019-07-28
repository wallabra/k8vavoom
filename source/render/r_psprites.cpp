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
extern VCvarB r_view_models;
extern VCvarB r_draw_psprites;
extern VCvarB r_chasecam;

static VCvarI crosshair("crosshair", "2", "Crosshair type (0-2).", CVAR_Archive);
static VCvarF crosshair_alpha("crosshair_alpha", "0.6", "Crosshair opacity.", CVAR_Archive);
static VCvarI r_crosshair_yofs("r_crosshair_yofs", "0", "Crosshair y offset (>0: down).", CVAR_Archive);


//==========================================================================
//
//  showPSpriteWarnings
//
//==========================================================================
static bool showPSpriteWarnings () {
  static int flag = -1;
  if (flag < 0) flag = (GArgs.CheckParm("-Wpsprite") || GArgs.CheckParm("-Wall") ? 1 : 0);
  return !!flag;
}


//==========================================================================
//
//  VRenderLevelShared::RenderPSprite
//
//==========================================================================
void VRenderLevelShared::RenderPSprite (VViewState *VSt, const VAliasModelFrameInfo mfi,
  float PSP_DIST, vuint32 light, vuint32 Fade, float Alpha, bool Additive)
{
  spritedef_t *sprdef;
  spriteframe_t *sprframe;
  int lump;
  bool flip;

  if (Alpha <= 0.0002f) return; // no reason to render it, it is invisible
  if (Alpha > 1.0f) Alpha = 1.0f;

  // decide which patch to use
  if ((vuint32)mfi.spriteIndex/*VSt->State->SpriteIndex*/ >= MAX_SPRITE_MODELS) {
    if (showPSpriteWarnings()) {
      GCon->Logf(NAME_Warning, "R_ProjectSprite: invalid sprite number %d", mfi.spriteIndex);
    }
    return;
  }
  sprdef = &sprites[mfi.spriteIndex/*VSt->State->SpriteIndex*/];

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
      GCon->Logf(NAME_Warning, "R_ProjectSprite: invalid sprite texture id %d in frame %d : %d (the thing that should not be)", lump, mfi.spriteIndex, mfi.frame);
    }
    return;
  }

  int TexWidth = Tex->GetWidth();
  int TexHeight = Tex->GetHeight();
  int TexSOffset = Tex->SOffset;
  int TexTOffset = Tex->TOffset;

  //GCon->Logf("PSPRITE: '%s'; size=(%d,%d); ofs=(%d,%d)", *Tex->Name, TexWidth, TexHeight, TexSOffset, TexTOffset);

  TVec dv[4];

  float PSP_DISTI = 1.0f/PSP_DIST;
  TVec sprorigin = vieworg+PSP_DIST*viewforward;

  float sprx = 160.0f-VSt->SX+TexSOffset;
  float spry = 100.0f-VSt->SY*R_GetAspectRatio()+TexTOffset;

  spry -= cl->PSpriteSY;
  //k8: this is not right, but meh...
  if (fov > 90) spry -= (refdef.fovx-1.0f)*(aspect_ratio != 0 ? 100.0f : 110.0f);

  //  1 / 160 = 0.00625f
  TVec start = sprorigin-(sprx*PSP_DIST*0.00625f)*viewright;
  TVec end = start+(TexWidth*PSP_DIST*0.00625f)*viewright;

  //  1 / 160.0f * 120 / 100 = 0.0075f
  const float symul = 1.0f/160.0f*120.0f/100.0f;
  TVec topdelta = (spry*PSP_DIST*symul)*viewup;
  TVec botdelta = topdelta-(TexHeight*PSP_DIST*symul)*viewup;
  if (aspect_ratio != 1) {
    topdelta *= 100.0f/120.0f;
    botdelta *= 100.0f/120.0f;
  }

  dv[0] = start+botdelta;
  dv[1] = start+topdelta;
  dv[2] = end+topdelta;
  dv[3] = end+botdelta;

  TVec saxis(0, 0, 0);
  TVec taxis(0, 0, 0);
  TVec texorg(0, 0, 0);
  if (flip) {
    saxis = -(viewright*160*PSP_DISTI);
    texorg = dv[2];
  } else {
    saxis = viewright*160*PSP_DISTI;
    texorg = dv[1];
  }
       if (aspect_ratio == 0) taxis = -(viewup*160*PSP_DISTI);
  else if (aspect_ratio == 1) taxis = -(viewup*100*4/3*PSP_DISTI);
  else if (aspect_ratio == 2) taxis = -(viewup*100*16/9*PSP_DISTI);
  else if (aspect_ratio > 2) taxis = -(viewup*100*16/10*PSP_DISTI);

  Drawer->DrawSpritePolygon(dv, GTextureManager[lump], Alpha, Additive,
    0, ColorMap, light, Fade, -viewforward,
    DotProduct(dv[0], -viewforward), saxis, taxis, texorg, false);
}


//==========================================================================
//
//  VRenderLevelShared::RenderViewModel
//
//  FIXME: this doesn't work with "----" and "####" view states
//
//==========================================================================
bool VRenderLevelShared::RenderViewModel (VViewState *VSt, vuint32 light,
                                          vuint32 Fade, float Alpha, bool Additive)
{
  if (!r_view_models) return false;

  TVec origin = vieworg+(VSt->SX-1.0f)*viewright/8.0f-(VSt->SY-32.0f)*viewup/6.0f;

  float TimeFrac = 0;
  if (VSt->State->Time > 0) {
    TimeFrac = 1.0f-(VSt->StateTime/VSt->State->Time);
    TimeFrac = midval(0.0f, TimeFrac, 1.0f);
  }

  return DrawAliasModel(nullptr, VSt->State->Outer->Name, origin, cl->ViewAngles, 1.0f, 1.0f,
    VSt->State->getMFI(), (VSt->State->NextState ? VSt->State->NextState->getMFI() : VSt->State->getMFI()),
    nullptr, 0, light, Fade, Alpha, Additive, true, TimeFrac, r_interpolate_frames,
    RPASS_Normal);
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
  bool Additive = false;

  cl->MO->eventGetViewEntRenderParams(Alpha, RendStyle);

  if (RendStyle == STYLE_SoulTrans) {
    RendStyle = STYLE_Translucent;
    Alpha = r_transsouls;
  } else if (RendStyle == STYLE_OptFuzzy) {
    RendStyle = (r_drawfuzz ? STYLE_Fuzzy : STYLE_Translucent);
  }

  switch (RendStyle) {
    case STYLE_None: return;
    case STYLE_Normal: Alpha = 1.0f; break;
    case STYLE_Fuzzy: Alpha = FUZZY_ALPHA; break;
    case STYLE_Add: Additive = true; break;
    case STYLE_Stencil: break;
    case STYLE_AddStencil: Additive = true; break;
  }
  //Alpha = midval(0.0f, Alpha, 1.0f);
  if (Alpha <= 0.002f) return; // no reason to render it, it is invisible
  if (Alpha > 1.0f) Alpha = 1.0f;

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
      light = LightPoint(vieworg, cl->MO->Radius, -1);
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
        light = LightPoint(vieworg, cl->MO->Radius, -1);
      }
      //GCon->Logf("ltx=(%d,%d,%d)", ltxr, ltxg, ltxb);
      //light = (0xff000000u)|(((vuint32)clampToByte(ltxr))<<16)|(((vuint32)clampToByte(ltxg))<<8)|((vuint32)clampToByte(ltxb));
    }

    //FIXME: fake "solid color" with colored light for now
    if (RendStyle == STYLE_Stencil || RendStyle == STYLE_AddStencil) {
      light = (light&0xff000000u)|(cl->MO->StencilColor&0xffffffu);
    }

    vuint32 Fade = GetFade(SV_PointRegionLight(r_viewleaf->sector, cl->ViewOrg));

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

    cl->ViewStates[i].SY += cl->ViewStates[i].OfsY;

    if (!RenderViewModel(&cl->ViewStates[i], light, Fade, Alpha, Additive)) {
      RenderPSprite(&cl->ViewStates[i], cl->getMFI(i), 3-i, light, Fade, Alpha, Additive);
    }

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
  if (ch > 0 && ch < 10 && crosshair_alpha > 0.0f) {
    static int handle = 0;
    if (!handle || prevCH != ch) {
      prevCH = ch;
      handle = GTextureManager.AddPatch(VName(va("CROSHAI%i", ch), VName::AddLower8), TEXTYPE_Pic);
      if (handle < 0) handle = 0;
    }
    if (handle > 0) {
      //if (crosshair_alpha < 0.0f) crosshair_alpha = 0.0f;
      if (crosshair_alpha > 1.0f) crosshair_alpha = 1.0f;
      int cy = (screenblocks < 11 ? (VirtualHeight-sb_height)/2 : VirtualHeight/2);
      cy += r_crosshair_yofs;
      bool oldflt = gl_pic_filtering;
      gl_pic_filtering = false;
      R_DrawPic(VirtualWidth/2, cy, handle, crosshair_alpha);
      gl_pic_filtering = oldflt;
    }
  }
}
