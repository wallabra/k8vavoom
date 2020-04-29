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
#ifdef CLIENT
# include "drawer.h"
# include "net/network.h" /* sorry */
#endif


int NetLagChart[NETLAG_CHART_ITEMS] = {0};
unsigned NetLagChartPos = 0;


#ifdef CLIENT
extern VCvarB draw_lag;


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
//  T_IsFontExists
//
//==========================================================================
bool T_IsFontExists (VName fontname) {
  if (fontname == NAME_None) return false;
  return !!VFont::GetFont(VStr(fontname)); // this doesn't allocate
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
//  T_CursorWidth
//
//==========================================================================
int T_CursorWidth () {
  return GRoot->CursorWidth();
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
  GRoot->DrawCursorAt(x, y, GRoot->GetCursorChar());
}


//==========================================================================
//
//  T_DrawCursorAt
//
//==========================================================================
void T_DrawCursorAt (int x, int y, int cursorChar, int cursorColor) {
  GRoot->DrawCursorAt(x, y, cursorChar, cursorColor);
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
static double pbarLastLagUpdateTime = 0;
static int pbarMaxLagWidth = 0;

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
void R_OSDMsgReset (int type, bool sendKeepalives) {
  currMsgNumber = 0;
  currMsgType = type;
  if (sendKeepalives) NET_SendNetworkHeartbeat();
}


//==========================================================================
//
//  R_OSDMsgShow
//
//==========================================================================
void R_OSDMsgShow (const char *msg, int clr, bool sendKeepalives) {
  if (sendKeepalives) NET_SendNetworkHeartbeat();
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
    if (currMsgNumber == 0) {
      GRoot->ShadeRect(96, y-T_FontHeight(), VirtualWidth-96*2, T_FontHeight()*3, 0.666f);
    } else {
      GRoot->ShadeRect(96, y+T_FontHeight(), VirtualWidth-96*2, T_FontHeight(), 0.666f);
    }
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
bool R_PBarReset (bool sendKeepalives) {
  lastPBarWdt = -666;
  pbarStartTime = Sys_Time();
  pbarLastUpdateTime = 0;
  pbarLastLagUpdateTime = 0;
  pbarMaxLagWidth = 0;
#ifdef CLIENT
  return (Drawer && Drawer->IsInited());
#else
  return false;
#endif
}


#ifdef CLIENT
//==========================================================================
//
//  RenderLag
//
//==========================================================================
static bool RenderLag () {
  if (!cl || !cl->Net) return false;
  double ctt = Sys_Time();
  if (ctt-pbarLastLagUpdateTime <= 1.0) return false;
  pbarLastLagUpdateTime = ctt;
  const int nlag = clampval((int)((cl->Net->PrevLag+1.2*(max2(cl->Net->InLoss, cl->Net->OutLoss)*0.01))*1000), 0, 999);
  //const int lag0 = clampval((int)(cl->Net->PrevLag*1000), 0, 999);
  //const int lag1 = clampval((int)(cl->Net->AvgLag*1000), 0, 999);
  T_SetFont(ConFont);
  T_SetAlign(hleft, vtop);
  int xpos = 4;
  int ypos = 22;
  VStr s0(va("NET LAG:%3d", nlag));
  //VStr s1(va("LAGS   :%3d %3d", lag0, lag1));
  //pbarMaxLagWidth = max(pbarMaxLagWidth, max2(T_TextWidth(s0), T_TextWidth(s1))+4*2);
  pbarMaxLagWidth = max2(pbarMaxLagWidth, T_TextWidth(s0)+4*2);
  //GRoot->FillRect(xpos-4, ypos-4, pbarMaxLagWidth, T_FontHeight()*2+4*2, 0, 1.0f);
  GRoot->FillRect(xpos-4, ypos-4, pbarMaxLagWidth, T_FontHeight()*1+4*2, 0, 1.0f);
  T_DrawText(xpos, ypos, s0, CR_DARKBROWN); ypos += 9;
  //T_DrawText(xpos, ypos, s1, CR_DARKBROWN); ypos += 9;
  return true;
}
#endif


//==========================================================================
//
//  R_PBarUpdate
//
//==========================================================================
bool R_PBarUpdate (const char *message, int cur, int max, bool forced, bool sendKeepalives) {
  if (sendKeepalives) NET_SendNetworkHeartbeat();

  if (forced && cur >= max && lastPBarWdt == -666) return false; // nothing was drawn at all

  // check if we need to update pbar
  // when we have drawer, delay first update by 800 msec, otherwise don't delay it
  #ifdef CLIENT
  if (!forced) {
    if (Drawer && Drawer->IsInited()) {
      if (lastPBarWdt == -666) {
        if (Sys_Time()-pbarStartTime < 0.8) return false;
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
    if (!forced && wdt == lastPBarWdt) {
      if (draw_lag && cl && cl->Net) {
        if (RenderLag()) Drawer->Update();
      }
      return false;
    }
    // delay update if it is too often
    const double currt = Sys_Time();
    if (!forced && currt-pbarLastUpdateTime < 1.0/30.0) return false;
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
    if (draw_lag && cl && cl->Net) RenderLag();
    Drawer->Update();
  } else
  #endif
  // textual
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
