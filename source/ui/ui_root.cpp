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
#include "ui.h"

IMPLEMENT_CLASS(V, RootWidget);

VRootWidget *GRoot;


//==========================================================================
//
//  VRootWidget::Init
//
//==========================================================================
void VRootWidget::Init () {
  Super::Init(nullptr);
  SetSize(640, 480);

  MouseCursorPic = GTextureManager.AddPatch("mc_arrow", TEXTYPE_Pic, true); // silent
}


//==========================================================================
//
//  VRootWidget::DrawWidgets
//
//==========================================================================
void VRootWidget::DrawWidgets () {
  CleanupWidgets();
  if (IsGoingToDie()) return;
  DrawTree();
  // draw message box
  if (GClGame) GClGame->eventMessageBoxDrawer();
  // draw mouse cursor
  if (RootFlags&RWF_MouseEnabled) DrawPic(MouseX, MouseY, MouseCursorPic);
}


//==========================================================================
//
//  VRootWidget::RefreshScale
//
//==========================================================================
void VRootWidget::RefreshScale () {
  //fprintf(stderr, "new scale: %g, %g\n", (double)fScaleX, (double)fScaleY);
  SizeScaleX = fScaleX;
  SizeScaleY = fScaleY;
  SizeWidth = VirtualWidth;
  SizeHeight = VirtualHeight;
  ClipRect.ClipX2 = VirtualWidth-1;
  ClipRect.ClipY2 = VirtualHeight-1;
  ClipTree();
}


//==========================================================================
//
//  VRootWidget::TickWidgets
//
//==========================================================================
void VRootWidget::TickWidgets (float DeltaTime) {
  CleanupWidgets();
  if (IsGoingToDie()) return;
  if (SizeScaleX != fScaleX || SizeScaleY != fScaleY) RefreshScale();
  TickTree(DeltaTime);
}


//==========================================================================
//
//  VRootWidget::BuildEventPath
//
//==========================================================================
void VRootWidget::BuildEventPath () {
  EventPath.reset();
  for (VWidget *w = CurrentFocusChild; w; w = w->CurrentFocusChild) {
    if (w->IsGoingToDie()) break;
    EventPath.append(w);
  }
}


//==========================================================================
//
//  VRootWidget::InternalResponder
//
//  this is called by the engine to dispatch the event
//
//==========================================================================
bool VRootWidget::InternalResponder (event_t *evt) {
  if (IsGoingToDie()) return false;
  if (evt->isEatenOrCancelled()) return evt->isEaten();

  BuildEventPath();
  if (EventPath.length() == 0) return false;

  // first, sink
  evt->resetBubbling();
  evt->dest = EventPath[EventPath.length()-1];
  // do not process deepest child yet
  for (int f = 0; f < EventPath.length()-1; ++f) {
    VWidget *w = EventPath[f];
    if (w->IsGoingToDie()) return false;
    if (w->OnEvent(evt)) { evt->setEaten(); return true; }
    if (evt->isEatenOrCancelled()) return true;
  }

  // now, bubble
  evt->setBubbling();
  for (int f = EventPath.length()-1; f >= 0; --f) {
    VWidget *w = EventPath[f];
    if (w->IsGoingToDie()) return false;
    if (w->OnEvent(evt)) { evt->setEaten(); return true; }
    if (evt->isEatenOrCancelled()) return true;
  }

  // process mouse by root
  if (RootFlags&RWF_MouseEnabled) {
    // handle mouse movement
    if (evt->type == ev_mouse) {
      MouseMoveEvent(MouseX+evt->data2, MouseY-evt->data3);
      return true;
    }
    // handle mouse buttons
    if ((evt->type == ev_keydown || evt->type == ev_keyup) &&
        evt->data1 >= K_MOUSE1 && evt->data1 <= K_MOUSE3)
    {
      return MouseButtonEvent(evt->data1, evt->type == ev_keydown);
    }
  }

  return false;
}


//==========================================================================
//
//  VRootWidget::Responder
//
//  this is called by the engine to dispatch the event
//
//==========================================================================
bool VRootWidget::Responder (event_t *evt) {
  const bool res = (evt ? InternalResponder(evt) : false);
  CleanupWidgets();
  return res;
}


//==========================================================================
//
//  VRootWidget::SetMouse
//
//==========================================================================
void VRootWidget::SetMouse (bool MouseOn) {
  if (MouseOn) {
    RootFlags |= RWF_MouseEnabled;
  } else {
    RootFlags &= ~RWF_MouseEnabled;
  }
  if (!MouseOn && GInput) GInput->RegrabMouse();
}


//==========================================================================
//
//  VRootWidget::MouseMoveEvent
//
//==========================================================================
void VRootWidget::MouseMoveEvent (int NewX, int NewY) {
  if (IsGoingToDie()) return;

  // remember old mouse coordinates
  int OldMouseX = MouseX;
  int OldMouseY = MouseY;

  // update mouse position
  MouseX = NewX;
  MouseY = NewY;

  // clip mouse coordinates against window boundaries
  if (MouseX < 0) MouseX = 0; else if (MouseX >= SizeWidth) MouseX = SizeWidth-1;
  if (MouseY < 0) MouseY = 0; else if (MouseY >= SizeHeight) MouseY = SizeHeight-1;

  // check if mouse position has changed
  if (MouseX == OldMouseX && MouseY == OldMouseY) return;

  // find widget under old position
  float ScaledOldX = OldMouseX*SizeScaleX;
  float ScaledOldY = OldMouseY*SizeScaleY;
  float ScaledNewX = MouseX*SizeScaleX;
  float ScaledNewY = MouseY*SizeScaleY;
  VWidget *OldFocus = GetWidgetAt(ScaledOldX, ScaledOldY);
  for (VWidget *W = OldFocus; W != nullptr; W = W->ParentWidget) {
    if (W->OnMouseMove(
      (int)((ScaledOldX-W->ClipRect.OriginX)/W->ClipRect.ScaleX),
      (int)((ScaledOldY-W->ClipRect.OriginY)/W->ClipRect.ScaleY),
      (int)((ScaledNewX-W->ClipRect.OriginX)/W->ClipRect.ScaleX),
      (int)((ScaledNewY-W->ClipRect.OriginY)/W->ClipRect.ScaleY)))
    {
      break;
    }
  }
  VWidget *NewFocus = GetWidgetAt(ScaledNewX, ScaledNewY);
  if (OldFocus != nullptr && OldFocus != NewFocus) {
    OldFocus->WidgetFlags &= ~(WF_LMouseDown|WF_MMouseDown|WF_RMouseDown);
    OldFocus->OnMouseLeave();
    NewFocus->OnMouseEnter();
  }
}


//==========================================================================
//
//  VRootWidget::MouseButtonEvent
//
//==========================================================================
bool VRootWidget::MouseButtonEvent (int Button, bool Down) {
  if (IsGoingToDie()) return false;

  // find widget under mouse
  float ScaledX = MouseX*SizeScaleX;
  float ScaledY = MouseY*SizeScaleY;
  VWidget *Focus = GetWidgetAt(ScaledX, ScaledY);

  if (Down) {
         if (Button == K_MOUSE1) Focus->WidgetFlags |= WF_LMouseDown;
    else if (Button == K_MOUSE3) Focus->WidgetFlags |= WF_MMouseDown;
    else if (Button == K_MOUSE2) Focus->WidgetFlags |= WF_RMouseDown;
  } else {
    int LocalX = (int)((ScaledX-Focus->ClipRect.OriginX)/Focus->ClipRect.ScaleX);
    int LocalY = (int)((ScaledY-Focus->ClipRect.OriginY)/Focus->ClipRect.ScaleY);
    if (Button == K_MOUSE1 && (Focus->WidgetFlags&WF_LMouseDown)) {
      Focus->WidgetFlags &= ~WF_LMouseDown;
      Focus->OnMouseClick(LocalX, LocalY);
    }
    if (Button == K_MOUSE3 && (Focus->WidgetFlags&WF_MMouseDown)) {
      Focus->WidgetFlags &= ~WF_MMouseDown;
      Focus->OnMMouseClick(LocalX, LocalY);
    }
    if (Button == K_MOUSE2 && (Focus->WidgetFlags&WF_RMouseDown)) {
      Focus->WidgetFlags &= ~WF_RMouseDown;
      Focus->OnRMouseClick(LocalX, LocalY);
    }
  }

  for (VWidget *W = Focus; W; W = W->ParentWidget) {
    int LocalX = (int)((ScaledX-W->ClipRect.OriginX)/W->ClipRect.ScaleX);
    int LocalY = (int)((ScaledY-W->ClipRect.OriginY)/W->ClipRect.ScaleY);
    if (Down) {
      if (W->OnMouseDown(LocalX, LocalY, Button)) return true;
    } else {
      if (W->OnMouseUp(LocalX, LocalY, Button)) return true;
    }
  }
  return false;
}


//==========================================================================
//
//  VRootWidget::StaticInit
//
//==========================================================================
void VRootWidget::StaticInit () {
  GRoot = SpawnWithReplace<VRootWidget>();
  GRoot->Init();
  GClGame->GRoot = GRoot;
}


//==========================================================================
//
//  Natives
//
//==========================================================================
IMPLEMENT_FUNCTION(VRootWidget, SetMouse) {
  P_GET_BOOL(MouseOn);
  P_GET_SELF;
  Self->SetMouse(MouseOn);
}
