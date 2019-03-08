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
    int wdt = cur*(ScreenWidth-PBarHPad*2)/max;
    if (cur < max && wdt == lastPBarWdt) return;
    lastPBarWdt = wdt;
    Drawer->StartUpdate(false); // don't clear
    Drawer->FillRect(PBarHPad-2, ScreenHeight-PBarVPad-PBarHeight-2, ScreenWidth-PBarHPad+2, ScreenHeight-PBarVPad+2, 0xffffffff);
    Drawer->FillRect(PBarHPad-1, ScreenHeight-PBarVPad-PBarHeight-1, ScreenWidth-PBarHPad+1, ScreenHeight-PBarVPad+1, 0xff000000);
    Drawer->FillRect(PBarHPad, ScreenHeight-PBarVPad-PBarHeight, ScreenWidth-PBarHPad, ScreenHeight-PBarVPad, 0xff8f0f00);
    if (wdt > 0) Drawer->FillRect(PBarHPad, ScreenHeight-PBarVPad-PBarHeight, PBarHPad+wdt, ScreenHeight-PBarVPad, 0xffff7f00);
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
    int wdt = cur*(ScreenWidth-PBarHPad*2)/max;
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
        PBarHPad-8, ScreenHeight-PBarVPad-PBarHeight, PBarHPad, ScreenHeight-PBarVPad-PBarHeight+32,
        0, 0, tex->GetWidth(), tex->GetHeight(), tex, nullptr, 1.0f);
      // right end
      tex = GTextureManager(right);
      Drawer->DrawPic(
        ScreenWidth-PBarHPad, ScreenHeight-PBarVPad-PBarHeight, ScreenWidth-PBarHPad+8, ScreenHeight-PBarVPad-PBarHeight+32,
        0, 0, tex->GetWidth(), tex->GetHeight(), tex, nullptr, 1.0f);
      // middle
      tex = GTextureManager(mid);
      Drawer->FillRectWithFlatRepeat(
        PBarHPad, ScreenHeight-PBarVPad-PBarHeight, ScreenWidth-PBarHPad, ScreenHeight-PBarVPad-PBarHeight+32,
        0, 0, /*tex->GetWidth()*/(ScreenWidth-PBarHPad)*2, tex->GetHeight(), tex);
      // fill
      if (wdt > 0) {
        tex = GTextureManager(fill);
        Drawer->FillRectWithFlatRepeat(
          PBarHPad, ScreenHeight-PBarVPad-PBarHeight, PBarHPad+wdt, ScreenHeight-PBarVPad-PBarHeight+32,
          0, 0, /*tex->GetWidth()*/wdt, tex->GetHeight(), tex);
      }
    } else {
      Drawer->FillRect(PBarHPad-2, ScreenHeight-PBarVPad-PBarHeight-2, ScreenWidth-PBarHPad+2, ScreenHeight-PBarVPad+2, 0xffffffff);
      Drawer->FillRect(PBarHPad-1, ScreenHeight-PBarVPad-PBarHeight-1, ScreenWidth-PBarHPad+1, ScreenHeight-PBarVPad+1, 0xff000000);
      Drawer->FillRect(PBarHPad, ScreenHeight-PBarVPad-PBarHeight, ScreenWidth-PBarHPad, ScreenHeight-PBarVPad, 0xff8f0f00);
      if (wdt > 0) Drawer->FillRect(PBarHPad, ScreenHeight-PBarVPad-PBarHeight, PBarHPad+wdt, ScreenHeight-PBarVPad, 0xffff7f00);
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
