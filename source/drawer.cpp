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
//**  Game completion, final screen animation.
//**
//**************************************************************************
#include "gamedefs.h"
#include "cl_local.h"
#include "drawer.h"


int R_LdrMsgColorMain = CR_FIRE;
int R_LdrMsgColorSecondary = CR_ORANGE /*CR_PURPLE*/ /*CR_TEAL*/;


enum {
  PBarHPad = 20+16,
  PBarVPad = 20+16,
  PBarHeight = 20,
};


static int currMsgNumber = 0;

static int lastPBarWdt = -666;
static double pbarStartTime = 0;
static double pbarLastUpdateTime = 0;

TArray<void (*) (int phase)> VDrawer::cbInitDeinit;


//**************************************************************************
//
// VDrawer methods
//
//**************************************************************************

//==========================================================================
//
//  VDrawer::VDrawer
//
//==========================================================================
VDrawer::VDrawer () noexcept
  : mInitialized(false)
  , isShittyGPU(false)
  , shittyGPUCheckDone(false)
  , useReverseZ(false)
  , RendLev(nullptr)
{
  ScrWdt = max2(1, ScreenWidth);
  ScrHgt = max2(1, ScreenHeight);
}


//==========================================================================
//
//  VDrawer::~VDrawer
//
//==========================================================================
VDrawer::~VDrawer () {
}


//==========================================================================
//
//  VDrawer::RegisterICB
//
//==========================================================================
void VDrawer::RegisterICB (void (*cb) (int phase)) {
  if (!cb) return;
  for (int f = 0; f < cbInitDeinit.length(); ++f) {
    if (cbInitDeinit[f] == cb) return;
  }
  cbInitDeinit.append(cb);
}


//==========================================================================
//
//  VDrawer::callICB
//
//==========================================================================
void VDrawer::callICB (int phase) {
  for (int f = 0; f < cbInitDeinit.length(); ++f) cbInitDeinit[f](phase);
}


//==========================================================================
//
//  VDrawer::ResetTextureUpdateFrames
//
//==========================================================================
void VDrawer::ResetTextureUpdateFrames () noexcept {
  for (int i = 0; i < GTextureManager.GetNumTextures(); ++i) {
    VTexture *tex = GTextureManager[i];
    if (tex) tex->lastUpdateFrame = 0;
  }
}


//**************************************************************************
//
// VRenderLevelDrawer methods
//
//**************************************************************************

//==========================================================================
//
//  VRenderLevelDrawer::ResetDrawStack
//
//  should be called before rendering a frame
//  (i.e. in initial render, `RenderPlayerView()`)
//  creates 0th element of the stack
//
//==========================================================================
void VRenderLevelDrawer::ResetDrawStack () {
  DrawListStack.reset();
  DrawLists &dls = DrawListStack.alloc();
  dls.resetAll();
  vassert(DrawListStack.length() > 0);
}


//==========================================================================
//
//  VRenderLevelDrawer::PushDrawLists
//
//==========================================================================
void VRenderLevelDrawer::PushDrawLists () {
  DrawLists &dls = DrawListStack.alloc();
  dls.resetAll();
}


//==========================================================================
//
//  VRenderLevelDrawer::PopDrawLists
//
//==========================================================================
void VRenderLevelDrawer::PopDrawLists () {
  vensure(DrawListStack.length() != 0);
  GetCurrentDLS().resetAll();
  DrawListStack.setLength(DrawListStack.length()-1, false); // don't resize
  vassert(DrawListStack.length() > 0);
}


//**************************************************************************
//
// loader messages and progress bar
//
//**************************************************************************

//==========================================================================
//
//  R_LdrMsgReset
//
//==========================================================================
void R_LdrMsgReset () {
  currMsgNumber = 0;
}


void R_LdrMsgShow (const char *msg, int clr) {
#ifdef CLIENT
  if (!msg || !msg[0]) return;
  if (Drawer && Drawer->IsInited()) {
    T_SetFont(SmallFont);
    Drawer->StartUpdate(false); // don't clear
    T_SetAlign(hcentre, vcentre);
    // slightly off vcenter
    T_DrawText(VirtualWidth/2, VirtualHeight/2+64+10*currMsgNumber, msg, clr/*CR_TAN*/);
    Drawer->Update();
    ++currMsgNumber;
  }
#endif
}


//==========================================================================
//
//  R_PBarReset
//
//==========================================================================
bool R_PBarReset () {
  lastPBarWdt = -666;
  pbarStartTime = Sys_Time();
  pbarLastUpdateTime = 0;
#ifdef CLIENT
  return (Drawer && Drawer->IsInited());
#else
  return false;
#endif
}


//==========================================================================
//
//  R_PBarUpdate
//
//==========================================================================
bool R_PBarUpdate (const char *message, int cur, int max, bool forced) {
/*
#ifdef CLIENT
  if (Drawer && Drawer->IsInited()) {
    int wdt = cur*(Drawer->getWidth()-PBarHPad*2)/max;
    if (cur < max && wdt == lastPBarWdt) return;
    lastPBarWdt = wdt;
    Drawer->StartUpdate(false); // don't clear
    Drawer->FillRect(PBarHPad-2, Drawer->getHeight()-PBarVPad-PBarHeight-2, Drawer->getWidth()-PBarHPad+2, Drawer->getHeight()-PBarVPad+2, 0xffffffff);
    Drawer->FillRect(PBarHPad-1, Drawer->getHeight()-PBarVPad-PBarHeight-1, Drawer->getWidth()-PBarHPad+1, Drawer->getHeight()-PBarVPad+1, 0xff000000);
    Drawer->FillRect(PBarHPad, Drawer->getHeight()-PBarVPad-PBarHeight, Drawer->getWidth()-PBarHPad, Drawer->getHeight()-PBarVPad, 0xff8f0f00);
    if (wdt > 0) Drawer->FillRect(PBarHPad, Drawer->getHeight()-PBarVPad-PBarHeight, PBarHPad+wdt, Drawer->getHeight()-PBarVPad, 0xffff7f00);
    Drawer->Update();
  } else
#endif
  {
    int prc = cur*100/max;
    GCon->Logf("PVS: %02d%% done (%d of %d)", prc, cur-1, max);
  }
*/

  if (forced && cur >= max && lastPBarWdt == -666) return false; // nothing was drawn at all

  // check if we need to update pbar
  // when we have drawer, delay first update by 800 msec, otherwise don't delay it
#ifdef CLIENT
  if (!forced) {
    if (Drawer && Drawer->IsInited()) {
      double currt = Sys_Time();
      if (lastPBarWdt == -666 && currt-pbarStartTime < 0.8) {
        if (currt-pbarStartTime > 0.033) CL_KeepaliveMessageEx(currt); // ~30 FPS
        return false;
      }
    }
  }
#endif

  if (max < 1) return false; // alas
  if (cur < 0) cur = 0;
  if (cur > max) cur = max;

#ifdef CLIENT
  if (Drawer && Drawer->IsInited()) {
    int wdt = cur*(Drawer->getWidth()-PBarHPad*2)/max;
    if (!forced && wdt == lastPBarWdt) return false;
    // delay update if it is too often
    double currt = Sys_Time();
    if (!forced && currt-pbarLastUpdateTime < 0.033) return false; // ~30 FPS
    CL_KeepaliveMessageEx(currt);
    pbarLastUpdateTime = currt;
    lastPBarWdt = wdt;
    Drawer->StartUpdate(false); // don't clear
    // load progressbar textures
    static bool texturesLoaded = false;
    static int left = -1, right = -1, mid = -1, fill = -1;
    if (!texturesLoaded) {
      texturesLoaded = true;
      left = GTextureManager.AddFileTextureChecked("graphics/progbar/progbar_left.png", TEXTYPE_Pic);
      if (left > 0) right = GTextureManager.AddFileTextureChecked("graphics/progbar/progbar_right.png", TEXTYPE_Pic);
      if (right > 0) mid = GTextureManager.AddFileTextureChecked("graphics/progbar/progbar_middle.png", TEXTYPE_Pic);
      if (mid > 0) fill = GTextureManager.AddFileTextureChecked("graphics/progbar/progbar_marker.png", TEXTYPE_Pic);
    }
    // which kind of progress bar to draw?
    if (left > 0) {
      VTexture *tex;
      // left end
      tex = GTextureManager(left);
      Drawer->DrawPic(
        PBarHPad-8, Drawer->getHeight()-PBarVPad-PBarHeight, PBarHPad, Drawer->getHeight()-PBarVPad-PBarHeight+32,
        0, 0, tex->GetWidth(), tex->GetHeight(), tex, nullptr, 1.0f);
      // right end
      tex = GTextureManager(right);
      Drawer->DrawPic(
        Drawer->getWidth()-PBarHPad, Drawer->getHeight()-PBarVPad-PBarHeight, Drawer->getWidth()-PBarHPad+8, Drawer->getHeight()-PBarVPad-PBarHeight+32,
        0, 0, tex->GetWidth(), tex->GetHeight(), tex, nullptr, 1.0f);
      // middle
      tex = GTextureManager(mid);
      Drawer->FillRectWithFlatRepeat(
        PBarHPad, Drawer->getHeight()-PBarVPad-PBarHeight, Drawer->getWidth()-PBarHPad, Drawer->getHeight()-PBarVPad-PBarHeight+32,
        0, 0, /*tex->GetWidth()*/(Drawer->getWidth()-PBarHPad)*2, tex->GetHeight(), tex);
      // fill
      if (wdt > 0) {
        tex = GTextureManager(fill);
        Drawer->FillRectWithFlatRepeat(
          PBarHPad, Drawer->getHeight()-PBarVPad-PBarHeight, PBarHPad+wdt, Drawer->getHeight()-PBarVPad-PBarHeight+32,
          0, 0, /*tex->GetWidth()*/wdt, tex->GetHeight(), tex);
      }
    } else {
      Drawer->FillRect(PBarHPad-2, Drawer->getHeight()-PBarVPad-PBarHeight-2, Drawer->getWidth()-PBarHPad+2, Drawer->getHeight()-PBarVPad+2, 0xffffffff);
      Drawer->FillRect(PBarHPad-1, Drawer->getHeight()-PBarVPad-PBarHeight-1, Drawer->getWidth()-PBarHPad+1, Drawer->getHeight()-PBarVPad+1, 0xff000000);
      Drawer->FillRect(PBarHPad, Drawer->getHeight()-PBarVPad-PBarHeight, Drawer->getWidth()-PBarHPad, Drawer->getHeight()-PBarVPad, 0xff8f0f00);
      if (wdt > 0) Drawer->FillRect(PBarHPad, Drawer->getHeight()-PBarVPad-PBarHeight, PBarHPad+wdt, Drawer->getHeight()-PBarVPad, 0xffff7f00);
    }
    Drawer->Update();
  } else
#endif
  {
    double currt = Sys_Time();
    if (!forced && currt-pbarLastUpdateTime < 2) return false;
    pbarLastUpdateTime = currt;
    int prc = cur*100/max;
    if (!message) message = "PROCESSING";
    GCon->Logf("%s: %02d%% done (%d of %d)", message, prc, cur-1, max);
  }
  return true;
}
