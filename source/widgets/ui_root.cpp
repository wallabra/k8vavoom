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
#include "../gamedefs.h"
#include "../client/cl_local.h"
#include "ui.h"


static VCvarI ui_click_threshold("ui_click_threshold", "6", "Amount of pixels mouse cursor can move before aborting a click event.", CVAR_Archive);
static VCvarF ui_click_timeout("ui_click_timeout", "0.3", "Click timeout in seconds.", CVAR_Archive);

extern VCvarB ui_mouse_forced;


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
//  VRootWidget::UpdateMouseForced
//
//==========================================================================
void VRootWidget::UpdateMouseForced () {
  //FIXME: make this faster!
  if (IsWantMouseInput()) RootFlags |= RWF_MouseForced; else RootFlags &= ~RWF_MouseForced;
  ui_mouse_forced = !!(RootFlags&RWF_MouseForced);
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
  UpdateMouseForced();
  if (MouseCursorPic > 0) {
    if (RootFlags&(RWF_MouseEnabled|RWF_MouseForced)) DrawPic(MouseX, MouseY, MouseCursorPic);
  }
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
//  VRootWidget::UpdateMousePosition
//
//==========================================================================
void VRootWidget::UpdateMousePosition (int NewX, int NewY) noexcept {
  // update and clip mouse coordinates against window boundaries
  MouseX = clampval(NewX, 0, SizeWidth-1);
  MouseY = clampval(NewY, 0, SizeHeight-1);
}


//==========================================================================
//
//  VRootWidget::GetWidgetAtScreenXY
//
//==========================================================================
VWidget *VRootWidget::GetWidgetAtScreenXY (int x, int y, bool allowDisabled) noexcept {
  return GetWidgetAt(x*SizeScaleX, y*SizeScaleY, allowDisabled);
}


//==========================================================================
//
//  VRootWidget::BuildEventPath
//
//==========================================================================
void VRootWidget::BuildEventPath (VWidget *lastOne) noexcept {
  if (lastOne && (lastOne->IsGoingToDie() || !lastOne->IsEnabled())) lastOne = nullptr;
  EventPath.resetNoDtor();
  for (VWidget *w = CurrentFocusChild; w; w = w->CurrentFocusChild) {
    if (w->IsGoingToDie() || !w->IsEnabled(false)) break;
    EventPath.append(w);
  }
  if (lastOne && (EventPath.length() == 0 || EventPath[EventPath.length()-1] != lastOne)) {
    EventPath.append(lastOne);
  }
}


//==========================================================================
//
//  VRootWidget::FixEventCoords
//
//==========================================================================
void VRootWidget::FixEventCoords (VWidget *w, event_t *evt, SavedEventParts &svparts) noexcept {
  svparts.type = 0;
  if (!evt || !evt->isAnyMouseEvent()) return;
  if (!w) w = this;
  if (evt->type != ev_mouse) {
    svparts.type = 1;
    svparts.x = evt->x;
    svparts.y = evt->y;
    evt->x = (int)w->ScaledXToLocal(MouseX*SizeScaleX);
    evt->y = (int)w->ScaledYToLocal(MouseY*SizeScaleY);
  } else {
    svparts.type = 2;
    svparts.dx = evt->dx;
    svparts.dy = evt->dy;
    svparts.msx = evt->msx;
    svparts.msy = evt->msy;
    evt->dx = (int)(evt->dx*SizeScaleX/w->ClipRect.ScaleX);
    evt->dy = (int)(evt->dy*SizeScaleY/w->ClipRect.ScaleY);
    evt->msx = (int)w->ScaledXToLocal(MouseX*SizeScaleX);
    evt->msy = (int)w->ScaledYToLocal(MouseY*SizeScaleY);
  }
}


//==========================================================================
//
//  VRootWidget::RestoreEventCoords
//
//==========================================================================
void VRootWidget::RestoreEventCoords (event_t *evt, const SavedEventParts &svparts) noexcept {
  if (!evt) return;
  switch (svparts.type) {
    case 1:
      evt->x = svparts.x;
      evt->y = svparts.y;
      break;
    case 2:
      evt->dx = svparts.dx;
      evt->dy = svparts.dy;
      evt->msx = svparts.msx;
      evt->msy = svparts.msy;
      break;
  }
}


//==========================================================================
//
//  VRootWidget::DispatchEvent
//
//==========================================================================
bool VRootWidget::DispatchEvent (event_t *evt) {
  if (!evt || evt->isEatenOrCancelled() || EventPath.length() == 0) return false;

  SavedEventParts svparts;

  // sink down to the destination parent, then bubble up from the destination
  // i.e. destination widget will get only "bubbling" event

  int dir = 1; // 1: sinking; -1: bubbling
  int widx = 0;
  while (widx >= 0 && widx < EventPath.length()) {
    if (dir == 1 && widx == EventPath.length()-1) dir = -1; // stop sinking, start bubbling
    VWidget *w = EventPath[widx];
    if (w->IsGoingToDie()) {
      // if sinking, don't sink furhter
      // if bubbling, do nothing
      dir = -1;
    } else {
      // protect from accidental changes
      if (dir == 1) evt->setSinking(); else evt->setBubbling();
      evt->dest = EventPath[EventPath.length()-1];
      // fix mouse coords
      FixEventCoords(w, evt, svparts);
      // call event handler
      if (w->OnEvent(evt)) evt->setEaten();
      // restore modified event fields
      RestoreEventCoords(evt, svparts);
      // get out if event is consumed or cancelled
      if (evt->isEatenOrCancelled()) return true;
    }
    widx += dir;
  }

  return false;
}


//==========================================================================
//
//  VRootWidget::MouseMoveEvent
//
//==========================================================================
void VRootWidget::MouseMoveEvent (const event_t *evt, int OldMouseX, int OldMouseY) {
  if (IsGoingToDie()) return;

  // check if this is a mouse movement event
  if (evt->type != ev_mouse) return;

  // check if mouse really moved
  if (OldMouseX == MouseX && OldMouseY == MouseY) return;

  // find widgets under old and new positions
  VWidget *OldFocus = GetWidgetAtScreenXY(OldMouseX, OldMouseY);
  VWidget *NewFocus = GetWidgetAtScreenXY(MouseX, MouseY);

  //GCon->Logf(NAME_Debug, "mmoved");

  if (OldFocus == NewFocus) {
    //GCon->Logf(NAME_Debug, "mmoved; old=(%d,%d); new=(%d,%d)", OldMouseX, OldMouseY, MouseX, MouseY);
    //if (OldFocus) GCon->Logf(NAME_Debug, "  ow:%u: size=(%d,%d)", OldFocus->GetUniqueId(), OldFocus->SizeWidth, OldFocus->SizeHeight);
    return;
  }

  if (OldFocus && !OldFocus->IsGoingToDie()) {
    //OldFocus->OnMouseLeave();
    // generate leave event
    event_t cev;
    cev.clear();
    cev.type = ev_leave;
    cev.dest = OldFocus;
    // the event is dispatched through the whole chain down to the current focused widget, and then to the leaved one
    BuildEventPath(OldFocus);
    // dispatch it
    DispatchEvent(&cev);
  }

  if (NewFocus && !NewFocus->IsGoingToDie()) {
    //NewFocus->OnMouseEnter();
    // generate enter event
    event_t cev;
    cev.clear();
    cev.type = ev_enter;
    cev.dest = NewFocus;
    // the event is dispatched through the whole chain down to the current focused widget, and then to the entered one
    BuildEventPath(NewFocus);
    // dispatch it
    DispatchEvent(&cev);
  }

  /*
  // only bubble
  EventPath.resetNoDtor();
  for (VWidget *W = OldFocus; W; W = W->ParentWidget) EventPath.append(W);

  const float ScaledOldX = OldMouseX*SizeScaleX;
  const float ScaledOldY = OldMouseY*SizeScaleY;

  const float ScaledNewX = MouseX*SizeScaleX;
  const float ScaledNewY = MouseY*SizeScaleY;

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
  */
}


//==========================================================================
//
//  VRootWidget::MouseClickEvent
//
//==========================================================================
void VRootWidget::MouseClickEvent (const event_t *evt) {
  if (IsGoingToDie()) return;

  // check if this is a mouse button event
  if (evt->type != ev_keydown && evt->type != ev_keyup) return;
  if (evt->keycode < K_MOUSE1 || evt->keycode > K_MOUSE9) return;

  // find widget under mouse
  VWidget *Focus = GetWidgetAtScreenXY(MouseX, MouseY);

  if (!Focus) return; // oopsie

  const float ct = Sys_Time();
  const unsigned mnum = (unsigned)(evt->keycode-K_MOUSE1);
  if (evt->type == ev_keydown) {
    Focus->MouseDownState[mnum].time = ct;
    Focus->MouseDownState[mnum].x = MouseX;
    Focus->MouseDownState[mnum].y = MouseY;
    Focus->MouseDownState[mnum].localx = Focus->ScaledXToLocal(MouseX*SizeScaleX);
    Focus->MouseDownState[mnum].localy = Focus->ScaledYToLocal(MouseY*SizeScaleY);
  } else {
    const float otime = Focus->MouseDownState[mnum].time;
    Focus->MouseDownState[mnum].time = 0;
    if (otime > 0 && (ui_click_timeout.asFloat() <= 0 || ct-otime < ui_click_timeout.asFloat())) {
      const int th = ui_click_threshold.asInt();
      if (th < 0 || (abs(Focus->MouseDownState[mnum].x-MouseX) <= th && abs(Focus->MouseDownState[mnum].y-MouseY) <= th)) {
        //Focus->OnMouseClick((int)msx, (int)msy, evt->keycode, Focus);
        // generate click event
        event_t cev;
        cev.clear();
        cev.type = ev_click;
        cev.keycode = evt->keycode;
        cev.clickcnt = 1;
        cev.dest = Focus;
        // the event is dispatched through the whole chain down to the current focused widget, and then to the clicked one
        BuildEventPath(Focus);
        // dispatch it
        DispatchEvent(&cev);
      }
    }
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
  if (!IsEnabled(false)) return false;
  if (evt->isEatenOrCancelled()) return evt->isEaten();

  // process broadcast event
  if (evt->type == ev_broadcast) {
    BroadcastEvent(evt);
    return false;
  }

  UpdateMouseForced();

  // remember old mouse coordinates (we may need it for enter/leave events)
  const int oldMouseX = MouseX;
  const int oldMouseY = MouseY;

  // update mouse position
  if (evt->type == ev_mouse) UpdateMousePosition(MouseX+evt->dx, MouseY-evt->dy);

  const bool mouseAllowed = (RootFlags&(RWF_MouseEnabled|RWF_MouseForced));

  // mouse down/up should still be processed to generate click events
  // also, "click" event should be delivered *before* "mouse up"
  // do it here and now, why not?
  if (mouseAllowed) {
    // those method will take care of everything
    MouseMoveEvent(evt, oldMouseX, oldMouseY);
    MouseClickEvent(evt);
  }

  // do not send mouse events if UI mouse is disabled
  if (mouseAllowed || !evt->isAnyMouseEvent()) {
    VWidget *Focus = nullptr;
    // special processing for mouse events
    if (evt->type == ev_mouse ||
        evt->type == ev_uimouse ||
        ((evt->type == ev_keydown || evt->type == ev_keyup) && (evt->keycode >= K_MOUSE_FIRST && evt->keycode <= K_MOUSE_LAST)))
    {
      // find widget under mouse
      Focus = GetWidgetAtScreenXY(MouseX, MouseY);
    }
    BuildEventPath(Focus);
    DispatchEvent(evt);
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
  UpdateMouseForced();
  if (GInput && (RootFlags&(RWF_MouseEnabled|RWF_MouseForced))) GInput->RegrabMouse();
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

// native final Widget GetWidgetAtScreenXY (int x, int y, optional bool allowDisabled/*=false*/);
IMPLEMENT_FUNCTION(VRootWidget, GetWidgetAtScreenXY) {
  int x, y;
  VOptParamBool allowDisabled(false);
  vobjGetParamSelf(x, y, allowDisabled);
  RET_REF(Self ? Self->GetWidgetAtScreenXY(x, y, allowDisabled) : nullptr);
}
