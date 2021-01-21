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
//**  Copyright (C) 2018-2021 Ketmar Dark
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
#include "../gamedefs.h"
#include "r_local.h"
#include "../server/sv_local.h"


extern VCvarF fov_main;
extern VCvarF cl_fov;
extern VCvarB gl_pic_filtering;
extern VCvarB r_draw_psprites;
extern VCvarB r_chasecam;
extern VCvarI gl_release_ram_textures_mode;

static VCvarI crosshair("crosshair", "2", "Crosshair type (0-2).", CVAR_Archive);
static VCvarF crosshair_alpha("crosshair_alpha", "0.6", "Crosshair opacity.", CVAR_Archive);
static VCvarI r_crosshair_yofs("r_crosshair_yofs", "0", "Crosshair y offset (>0: down).", CVAR_Archive);
static VCvarF crosshair_scale("crosshair_scale", "1", "Crosshair scale.", CVAR_Archive);

int VRenderLevelShared::prevCrosshairPic = -666;


static int cli_WarnSprites = 0;
/*static*/ bool cliRegister_rsprites_args =
  VParsedArgs::RegisterFlagSet("-Wpsprite", "!show some warnings about sprites", &cli_WarnSprites);


//==========================================================================
//
//  showPSpriteWarnings
//
//==========================================================================
static inline bool showPSpriteWarnings () {
  return (cli_WAll > 0 || cli_WarnSprites > 0);
}


//==========================================================================
//
//  VRenderLevelShared::RenderPSprite
//
//==========================================================================
void VRenderLevelShared::RenderPSprite (float SX, float SY, const VAliasModelFrameInfo &mfi, float PSP_DIST, const RenderStyleInfo &ri) {
  spritedef_t *sprdef;
  spriteframe_t *sprframe;
  int lump;
  bool flip;

  if (ri.alpha < 0.004f) return; // no reason to render it, it is invisible
  //if (ri.alpha > 1.0f) ri.alpha = 1.0f;

  // decide which patch to use
  if ((unsigned)mfi.spriteIndex >= (unsigned)sprites.length()) {
    if (showPSpriteWarnings()) {
      GCon->Logf(NAME_Warning, "R_ProjectSprite: invalid sprite number %d", mfi.spriteIndex);
    }
    return;
  }
  sprdef = &sprites.ptr()[mfi.spriteIndex];

  if (mfi.frame >= sprdef->numframes) {
    if (showPSpriteWarnings()) {
      GCon->Logf(NAME_Warning, "R_ProjectSprite: invalid sprite frame %d : %d (max is %d)", mfi.spriteIndex, mfi.frame, sprdef->numframes);
    }
    return;
  }
  sprframe = &sprdef->spriteframes[mfi.frame];

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

  // do not release psprite textures
  int oldRelease = gl_release_ram_textures_mode.asInt();
  gl_release_ram_textures_mode = 0;
  Tex->CropTexture();

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

  float sprx = 160.0f-SX+TexSOffset*invScaleX;
  float spry = 100.0f-SY+TexTOffset*invScaleY;

  spry -= cl->PSpriteSY;
  spry -= PSpriteOfsAspect;

  //k8: this is not right, but meh...
  /*
  if (fov > 90) {
    // this moves sprx to the left screen edge
    //sprx += (AspectEffectiveFOVX-1.0f)*160.0f;
  }
  */

  float ourfov = clampval(fov_main.asFloat(), 1.0f, 170.0f);
  // apply client fov
  if (cl_fov > 1) ourfov = clampval(cl_fov.asFloat(), 1.0f, 170.0f);

  //k8: don't even ask me!
  const float sxymul = (1.0f+(ourfov != 90 ? AspectEffectiveFOVX-1.0f : 0.0f))/160.0f;

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

  Drawer->PushDepthMaskSlow();
  Drawer->GLDisableDepthWriteSlow();
  Drawer->GLDisableDepthTestSlow();
  Drawer->DrawSpritePolygon((Level ? Level->Time : 0.0f), dv, GTextureManager[lump], ri,
    nullptr, ColorMap, -Drawer->viewforward,
    DotProduct(dv[0], -Drawer->viewforward), saxis, taxis, texorg);
  Drawer->PopDepthMaskSlow();
  Drawer->GLEnableDepthTestSlow();

  gl_release_ram_textures_mode = oldRelease;
}


//==========================================================================
//
//  VRenderLevelShared::RenderViewModel
//
//  FIXME: this doesn't work with "----" and "####" view states
//
//==========================================================================
bool VRenderLevelShared::RenderViewModel (VViewState *VSt, float SX, float SY, const RenderStyleInfo &ri) {
  if (!r_models_view) return false;
  if (!R_HaveClassModelByName(VSt->State->Outer->Name)) return false;

  VMatrix4 oldProjMat;
  TClipBase old_clip_base = clip_base;

  // remporarily set FOV to 90
  const bool restoreFOV = (fov_main.asFloat() != 90.0f || cl_fov.asFloat() >= 1.0f);

  if (restoreFOV) {
    refdef_t newrd = refdef;
    const float fov90 = CalcEffectiveFOV(90.0f, newrd);
    SetupRefdefWithFOV(&newrd, fov90);

    VMatrix4 newProjMat;
    Drawer->CalcProjectionMatrix(newProjMat, /*this,*/ &newrd);

    Drawer->GetProjectionMatrix(oldProjMat);
    Drawer->SetProjectionMatrix(newProjMat);
  }

  TVec origin = Drawer->vieworg+(SX-1.0f)*Drawer->viewright/8.0f-(SY-32.0f)*Drawer->viewup/6.0f;

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
//  getVSOffset
//
//==========================================================================
static inline float getVSOffset (const float ofs, const float stofs) noexcept {
  if (stofs <= -10000.0f) return stofs+10000.0f;
  if (stofs >=  10000.0f) return stofs-10000.0f;
  return ofs;
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
    // check if we have any light at player's origin (rough), and owned by player
    const dlight_t *dl = DLights;
    for (int dlcount = MAX_DLIGHTS; dlcount--; ++dl) {
      if (dl->die < Level->Time || dl->radius < 1.0f) continue;
      //if (!dl->Owner || dl->Owner->IsGoingToDie() || !dl->Owner->IsA(eclass)) continue;
      //VEntity *e = (VEntity *)dl->Owner;
      VEntity *e;
      if (dl->ownerUId) {
        auto ownpp = suid2ent.find(dl->ownerUId);
        if (!ownpp) continue;
        e = *ownpp;
      } else {
        continue;
      }
      if (!e->IsPlayer()) continue;
      if (e != cl->MO) continue;
      if ((e->Origin-dl->origin).length() > dl->radius*0.75f) continue;
      ltxr += (dl->color>>16)&0xff;
      ltxg += (dl->color>>8)&0xff;
      ltxb += dl->color&0xff;
    }
  }

  RenderStyleInfo mdri = ri;

  // calculate interpolation
  const float currSX = cl->ViewStates[PS_WEAPON].SX;
  const float currSY = cl->ViewStates[PS_WEAPON].SY;

  float SX = currSX;
  float SY = currSY;

  const float dur = cl->PSpriteWeaponLoweringDuration;
  if (dur > 0.0f) {
    float stt = cl->PSpriteWeaponLoweringStartTime;
    float currt = Level->Time;
    float t = currt-stt;
    float prevSY = cl->PSpriteWeaponLowerPrev;
    if (t >= 0.0f && t < dur) {
      const float ydelta = fabsf(prevSY-currSY)*(t/dur);
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
    //cl->ViewStates[i].SY = prevSY;
    SY = prevSY;
  }

       if (cl->ViewStates[PS_WEAPON].OfsX <= -10000.0f) SX += cl->ViewStates[PS_WEAPON].OfsX+10000.0f;
  else if (cl->ViewStates[PS_WEAPON].OfsX >= 10000.0f) SX += cl->ViewStates[PS_WEAPON].OfsX-10000.0f;
  else SX += cl->ViewStates[PS_WEAPON].OfsX;

       if (cl->ViewStates[PS_WEAPON].OfsY <= -10000.0f) SY += cl->ViewStates[PS_WEAPON].OfsY+10000.0f;
  else if (cl->ViewStates[PS_WEAPON].OfsY >= 10000.0f) SY += cl->ViewStates[PS_WEAPON].OfsY-10000.0f;
  else SY += cl->ViewStates[PS_WEAPON].OfsY;

  //GCon->Logf(NAME_Debug, "weapon offset:(%g,%g):(%g,%g)  flash offset:(%g,%g)", cl->ViewStates[PS_WEAPON].OfsX, cl->ViewStates[PS_WEAPON].OfsY, SX, SY, cl->ViewStates[PS_FLASH].OfsX, cl->ViewStates[PS_FLASH].OfsY);

  SX += cl->ViewStates[PS_WEAPON].BobOfsX;
  SY += cl->ViewStates[PS_WEAPON].BobOfsY;

  // calculate base light and fade
  vuint32 baselight;
  if (RendStyle == STYLE_Fuzzy) {
    baselight = 0;
  } else {
    if (ltxr|ltxg|ltxb) {
      baselight = (0xff000000u)|(((vuint32)clampToByte(ltxr))<<16)|(((vuint32)clampToByte(ltxg))<<8)|((vuint32)clampToByte(ltxb));
    } else {
      //const TVec lpos = Drawer->vieworg-TVec(0.0f, 0.0f, cl->MO->Height);
      const TVec lpos = cl->MO->Origin;
      baselight = LightPoint(nullptr, lpos, cl->MO->Radius, cl->MO->Height, r_viewleaf);
    }
  }

  const vuint32 basefase = GetFade(SV_PointRegionLight(r_viewleaf->sector, cl->ViewOrg));

  // draw all active psprites
  for (int ii = 0; ii < NUMPSPRITES; ++ii) {
    const int i = VPSpriteRenderOrder[ii];

    VState *vst = cl->ViewStates[i].State;
    if (!vst) continue;
    //GCon->Logf(NAME_Debug, "PSPRITE #%d is %d: %s", ii, i, *vst->Loc.toStringNoCol());

    const vuint32 light = (RendStyle != STYLE_Fuzzy && (vst->Frame&VState::FF_FULLBRIGHT) ? 0xffffffff : baselight);

    ri.light = ri.seclight = light;
    ri.fade = basefase;

    mdri.light = mdri.seclight = light;
    mdri.fade = ri.fade;

    //GCon->Logf(NAME_Debug, "PSPRITE #%d is %d: sx=%g; sy=%g; %s", ii, i, cl->ViewStates[i].SX, cl->ViewStates[i].SY, *vst->Loc.toStringNoCol());

    const float nSX = (i != PS_WEAPON ? getVSOffset(SX, cl->ViewStates[i].OfsX) : SX);
    const float nSY = (i != PS_WEAPON ? getVSOffset(SY, cl->ViewStates[i].OfsY) : SY);

    if (!RenderViewModel(&cl->ViewStates[i], nSX, nSY, mdri)) {
      RenderPSprite(nSX, nSY, cl->getMFI(i), 3.0f/*NUMPSPRITES-ii*/, ri);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::RenderCrosshair
//
//==========================================================================
void VRenderLevelShared::RenderCrosshair () {
  int ch = (int)crosshair;
  const float scale = crosshair_scale.asFloat();
  float alpha = crosshair_alpha.asFloat();
  if (!isFiniteF(alpha)) alpha = 0;
  if (ch > 0 && ch < 16 && alpha > 0.0f && isFiniteF(scale) && scale > 0.0f) {
    static int handle = 0;
    if (!handle || prevCrosshairPic != ch) {
      prevCrosshairPic = ch;
      handle = GTextureManager.AddPatch(VName(va("croshai%x", ch), VName::AddLower8), TEXTYPE_Pic, true); //silent
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
