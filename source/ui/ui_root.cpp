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


static VCvarI ui_click_threshold("ui_click_threshold", "6", "Amount of pixels mouse cursor can move before aborting a click event.", CVAR_Archive);
static VCvarF ui_click_timeout("ui_click_timeout", "0.3", "Click timeout in seconds.", CVAR_Archive);


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

  //MouseCursorPic = GTextureManager.AddPatch("mc_arrow", TEXTYPE_Pic, true); // silent
  MouseCursorPic = GTextureManager.AddFileTextureChecked("graphics/ui/default_cursor.png", TEXTYPE_Pic);
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
//  VRootWidget::FixEventCoords
//
//==========================================================================
void VRootWidget::FixEventCoords (VWidget *w, event_t *evt) {
  if (!evt || !evt->isAnyMouseEvent()) return;
  if (!w) w = this;
  if (evt->type != ev_mouse) {
    evt->x = (int)w->ScaledXToLocal(MouseX*SizeScaleX);
    evt->y = (int)w->ScaledYToLocal(MouseY*SizeScaleX);
  } else {
    evt->dx = (int)(evt->dx*SizeScaleX/w->ClipRect.ScaleX);
    evt->dy = (int)(evt->dy*SizeScaleY/w->ClipRect.ScaleY);
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
  //GCon->Logf(NAME_Debug, "evt=%d; len=%d", evt->type, EventPath.length());
  if (EventPath.length() == 0) return false;

  const bool mouseAllowed = (RootFlags&RWF_MouseEnabled);

  // remember old mouse coordinates
  const int OldMouseX = MouseX;
  const int OldMouseY = MouseY;
  const bool doMouseMoved = (evt->type == ev_mouse ? UpdateMousePosition(MouseX+evt->dx, MouseY-evt->dy) : false);

  // do not send mouse events if UI mouse is disabled
  if (mouseAllowed || !evt->isAnyMouseEvent()) {
    // first, sink
    evt->resetBubbling();
    evt->dest = EventPath[EventPath.length()-1];
    const int oldx = evt->x;
    const int oldy = evt->y;

    // do not process deepest child yet
    for (int f = 0; f < EventPath.length()-1; ++f) {
      VWidget *w = EventPath[f];
      if (w->IsGoingToDie()) return false;
      FixEventCoords(w, evt);
      const bool done = w->OnEvent(evt);
      evt->x = oldx;
      evt->y = oldy;
      if (done) { evt->setEaten(); return true; }
      if (evt->isEatenOrCancelled()) return true;
    }

    // now, bubble
    evt->setBubbling();
    for (int f = EventPath.length()-1; f >= 0; --f) {
      VWidget *w = EventPath[f];
      if (w->IsGoingToDie()) return false;
      FixEventCoords(w, evt);
      const bool done = w->OnEvent(evt);
      evt->x = oldx;
      evt->y = oldy;
      if (done) { evt->setEaten(); return true; }
      if (evt->isEatenOrCancelled()) return true;
    }
  }

  // process mouse by root
  if (mouseAllowed) {
    // handle mouse movement
    if (evt->type == ev_mouse) {
      if (doMouseMoved) MouseMoveEvent(OldMouseX, OldMouseY);
      return true;
    }
    // handle mouse buttons
    if ((evt->type == ev_keydown || evt->type == ev_keyup) &&
        evt->keycode >= K_MOUSE1 && evt->keycode <= K_MOUSE9)
    {
      return MouseButtonEvent(evt->keycode, (evt->type == ev_keydown));
    }
  } else {
    if (evt->type == ev_mouse) (void)UpdateMousePosition(MouseX+evt->dx, MouseY-evt->dy);
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
  if (MouseOn) RootFlags |= RWF_MouseEnabled; else RootFlags &= ~RWF_MouseEnabled;
  if (!MouseOn && GInput) GInput->RegrabMouse();
}


//==========================================================================
//
//  VRootWidget::UpdateMousePosition
//
//  returns `true` if the mouse was moved
//
//==========================================================================
bool VRootWidget::UpdateMousePosition (int NewX, int NewY) {
  const int OldMouseX = MouseX;
  const int OldMouseY = MouseY;

  // update and clip mouse coordinates against window boundaries
  MouseX = clampval(NewX, 0, SizeWidth-1);
  MouseY = clampval(NewY, 0, SizeHeight-1);

  return (OldMouseX != MouseX || OldMouseY != MouseY);
}


//==========================================================================
//
//  VRootWidget::MouseMoveEvent
//
//==========================================================================
void VRootWidget::MouseMoveEvent (int OldMouseX, int OldMouseY) {
  if (IsGoingToDie()) return;

  // find widget under old position
  const float ScaledOldX = OldMouseX*SizeScaleX;
  const float ScaledOldY = OldMouseY*SizeScaleY;
  const float ScaledNewX = MouseX*SizeScaleX;
  const float ScaledNewY = MouseY*SizeScaleY;

  VWidget *OldFocus = GetWidgetAt(ScaledOldX, ScaledOldY);

  // only bubble
  EventPath.reset();
  for (VWidget *W = OldFocus; W; W = W->ParentWidget) EventPath.append(W);

  for (auto &&W : EventPath) {
    if (W->IsGoingToDie()) break;
    if (W->OnMouseMove(
      (int)((ScaledOldX-W->ClipRect.OriginX)/W->ClipRect.ScaleX),
      (int)((ScaledOldY-W->ClipRect.OriginY)/W->ClipRect.ScaleY),
      (int)((ScaledNewX-W->ClipRect.OriginX)/W->ClipRect.ScaleX),
      (int)((ScaledNewY-W->ClipRect.OriginY)/W->ClipRect.ScaleY), OldFocus))
    {
      break;
    }
  }

  VWidget *NewFocus = GetWidgetAt(ScaledNewX, ScaledNewY);
  if (OldFocus != NewFocus) {
    if (OldFocus) OldFocus->OnMouseLeave();
    if (NewFocus) NewFocus->OnMouseEnter();
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
  const float ScaledX = MouseX*SizeScaleX;
  const float ScaledY = MouseY*SizeScaleY;
  VWidget *Focus = GetWidgetAt(ScaledX, ScaledY);

  if (Focus && Button >= K_MOUSE1 && Button <= K_MOUSE9) {
    const float ct = Sys_Time();
    const unsigned mnum = (unsigned)(Button-K_MOUSE1);
    const float msx = Focus->ScaledXToLocal(ScaledX);
    const float msy = Focus->ScaledYToLocal(ScaledY);
    if (Down) {
      Focus->MouseDownState[mnum].time = ct;
      Focus->MouseDownState[mnum].x = MouseX;
      Focus->MouseDownState[mnum].y = MouseY;
      Focus->MouseDownState[mnum].localx = msx;
      Focus->MouseDownState[mnum].localy = msy;
    } else {
      const float otime = Focus->MouseDownState[mnum].time;
      Focus->MouseDownState[mnum].time = 0;
      if (otime > 0 && (ui_click_timeout.asFloat() <= 0 || ct-otime < ui_click_timeout.asFloat())) {
        const int th = ui_click_threshold.asInt();
        if (th < 0 || (abs(Focus->MouseDownState[mnum].x-MouseX) <= th && abs(Focus->MouseDownState[mnum].y-MouseY) <= th)) {
          Focus->OnMouseClick((int)msx, (int)msy, Button, Focus);
        }
      }
    }
  }

  // only bubble
  EventPath.reset();
  for (VWidget *W = Focus; W; W = W->ParentWidget) EventPath.append(W);

  for (auto &&W : EventPath) {
    if (W->IsGoingToDie()) break;
    int LocalX = (int)W->ScaledXToLocal(ScaledX);
    int LocalY = (int)W->ScaledYToLocal(ScaledY);
    if (Down) {
      if (W->OnMouseDown(LocalX, LocalY, Button, Focus)) return true;
    } else {
      if (W->OnMouseUp(LocalX, LocalY, Button, Focus)) return true;
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
