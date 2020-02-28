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
#include "cl_local.h"
#include "ui/ui.h"
#include "neoui/neoui.h"
#ifdef CLIENT
# include "drawer.h"
#endif


#ifdef CLIENT
//==========================================================================
//
//  T_Init
//
//==========================================================================
void T_Init () {
  VFont::StaticInit();
}


//==========================================================================
//
//  T_Shutdown
//
//==========================================================================
void T_Shutdown () {
  VFont::StaticShutdown();
}


//==========================================================================
//
//  T_SetFont
//
//==========================================================================
void T_SetFont (VFont *AFont) {
  GRoot->SetFont(AFont);
}


//==========================================================================
//
//  T_SetAlign
//
//==========================================================================
void T_SetAlign (halign_e NewHAlign, valign_e NewVAlign) {
  GRoot->SetTextAlign(NewHAlign, NewVAlign);
}


//==========================================================================
//
//  T_DrawText
//
//==========================================================================
void T_DrawText (int x, int y, VStr String, int col) {
  GRoot->DrawText(x, y, String, col, CR_YELLOW, 1.0f);
}


//==========================================================================
//
//  T_TextWidth
//
//==========================================================================
int T_TextWidth (VStr s) {
  return GRoot->TextWidth(s);
}


//==========================================================================
//
//  T_StringWidth
//
//==========================================================================
int T_StringWidth (VStr s) {
  return GRoot->StringWidth(s);
}


//==========================================================================
//
//  T_TextHeight
//
//==========================================================================
int T_TextHeight (VStr s) {
  return GRoot->TextHeight(s);
}


//==========================================================================
//
//  T_FontHeight
//
//==========================================================================
int T_FontHeight () {
  return GRoot->FontHeight();
}


//==========================================================================
//
//  T_DrawCursor
//
//==========================================================================
void T_DrawCursor () {
  GRoot->DrawCursor();
}


//==========================================================================
//
//  T_DrawCursorAt
//
//==========================================================================
void T_DrawCursorAt (int x, int y) {
  GRoot->DrawCursorAt(x, y);
}


//==========================================================================
//
//  T_SetCursorPos
//
//==========================================================================
void T_SetCursorPos (int cx, int cy) {
  GRoot->SetCursorPos(cx, cy);
}


//==========================================================================
//
//  T_GetCursorX
//
//==========================================================================
int T_GetCursorX () {
  return GRoot->GetCursorX();
}


//==========================================================================
//
//  T_GetCursorY
//
//==========================================================================
int T_GetCursorY () {
  return GRoot->GetCursorY();
}
#endif


//**************************************************************************
//
// loader messages and progress bar
//
//**************************************************************************
enum {
  PBarHPad = 20+16,
  PBarVPad = 20+16,
  PBarHeight = 20,
};


static int currMsgNumber = 0;
static int currMsgType = OSD_MapLoading;

static int lastPBarWdt = -666;
static double pbarStartTime = 0;
static double pbarLastUpdateTime = 0;

int R_OSDMsgColorMain = CR_FIRE;
int R_OSDMsgColorSecondary = CR_ORANGE /*CR_PURPLE*/ /*CR_TEAL*/;


//==========================================================================
//
//  R_IsDrawerInited
//
//==========================================================================
bool R_IsDrawerInited () {
  #ifdef CLIENT
  return (Drawer && Drawer->IsInited());
  #else
  return false;
  #endif
}


//==========================================================================
//
//  R_OSDMsgReset
//
//==========================================================================
void R_OSDMsgReset (int type) {
  currMsgNumber = 0;
  currMsgType = type;
  NET_SendNetworkHeartbeat();
}


//==========================================================================
//
//  R_OSDMsgShow
//
//==========================================================================
void R_OSDMsgShow (const char *msg, int clr) {
  NET_SendNetworkHeartbeat();
  #ifdef CLIENT
  if (!msg || !msg[0]) return;
  if (Drawer && Drawer->IsInited()) {
    T_SetFont(SmallFont);
    Drawer->StartUpdate();
    T_SetAlign(hcenter, /*vcenter*/vtop);
    // slightly off vcenter
    int y;
    switch (currMsgType) {
      case OSD_MapLoading:
        y = VirtualHeight/2+64;
        if (clr == -666) clr = CR_TAN;
        break;
      case OSD_Network:
        y = 8*T_FontHeight();
        if (clr == -666) clr = CR_ICE;
        //GRoot->FillRect(0, y+T_FontHeight()*currMsgNumber, VirtualWidth, T_FontHeight(), 0, 1.0f);
        break;
      default:
        if (clr == -666) clr = CR_TEAL;
        y = max2(4*T_FontHeight(), VirtualHeight/2-9*T_FontHeight());
        //GRoot->ShadeRect(96, y+T_FontHeight()*currMsgNumber, VirtualWidth-96*2, T_FontHeight(), 0.666f);
        break;
    }
    y += T_FontHeight()*currMsgNumber;
    GRoot->ShadeRect(96, y, VirtualWidth-96*2, T_FontHeight(), 0.666f);
    T_DrawText(VirtualWidth/2, y, msg, clr);
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
    Drawer->StartUpdate();
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

  NET_SendNetworkHeartbeat();

  if (forced && cur >= max && lastPBarWdt == -666) return false; // nothing was drawn at all

  // check if we need to update pbar
  // when we have drawer, delay first update by 800 msec, otherwise don't delay it
#ifdef CLIENT
  if (!forced) {
    if (Drawer && Drawer->IsInited()) {
      double currt = Sys_Time();
      if (lastPBarWdt == -666 && currt-pbarStartTime < 0.8) {
        //if (currt-pbarStartTime > 0.033) CL_NetworkHeartbeatEx(currt); // ~30 FPS
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
    pbarLastUpdateTime = currt;
    lastPBarWdt = wdt;
    Drawer->StartUpdate();
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
