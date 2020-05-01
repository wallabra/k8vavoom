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
#include "drawer.h"
#include "ui.h"

extern VCvarB gl_pic_filtering;
extern VCvarB gl_font_filtering;


static VCvarF ui_msgxbox_wrap_trigger("ui_msgxbox_wrap_trigger", "0.9", "Maximum width (1 means whole screen) before message box will start wrapping; <=0 means \"don't\".", CVAR_Archive);
static VCvarF ui_msgxbox_wrap_width("ui_msgxbox_wrap_width", "0.7", "Width (1 means whole screen) for message box wrapping; <=0 means \"don't\".", CVAR_Archive);

static TMapNC<VName, bool> reportedMissingFonts;

TMapNC<VWidget *, bool> VWidget::AllWidgets;
static TArray<VWidget *> DyingWidgets;


IMPLEMENT_CLASS(V, Widget);


//==========================================================================
//
//  VWidget::PostCtor
//
//  this is called after defaults were blit
//
//==========================================================================
void VWidget::PostCtor () {
  //GCon->Logf(NAME_Debug, "created widget %s:%u:%p", GetClass()->GetName(), GetUniqueId(), (void *)this);
  AllWidgets.put(this, true);
  Super::PostCtor();
}


//==========================================================================
//
//  VWidget::CreateNewWidget
//
//==========================================================================
VWidget *VWidget::CreateNewWidget (VClass *AClass, VWidget *AParent) {
  VWidget *W = (VWidget *)StaticSpawnWithReplace(AClass);
  W->OfsX = W->OfsY = 0;
  W->Init(AParent);
  return W;
}


//==========================================================================
//
//  VWidget::CleanupWidgets
//
//  called by root widget in responder
//
//==========================================================================
void VWidget::CleanupWidgets () {
  if (GRoot && GRoot->IsDelayedDestroy()) {
    if (!GRoot->IsDestroyed()) GRoot->ConditionalDestroy();
    GRoot = nullptr;
  }

  // collect all orphans and dead widgets
  DyingWidgets.reset();
  for (auto &&it : AllWidgets.first()) {
    VWidget *w = it.getKey();
    if (w == GRoot) continue;
    //if (!w->ParentWidget && !w->IsGoingToDie()) w->MarkDead();
    if (!w->IsDestroyed() && w->IsDelayedDestroy()) DyingWidgets.append(w);
  }

  if (DyingWidgets.length() == 0) return;
  //GCon->Logf(NAME_Debug, "=== found %d dead widgets ===", DyingWidgets.length());

  // destroy all dead widgets
  for (auto &&it : DyingWidgets) {
    VWidget *w = it;
    if (!AllWidgets.has(w)) {
      //for (auto &&ww : AllWidgets.first()) GCon->Logf("  %p : %p : %d", w, ww.getKey(), (int)(w == ww.getKey()));
      // already destroyed
      //GCon->Logf(NAME_Debug, "(0)skipping already destroyed widget %s:%u:%p : %d", w->GetClass()->GetName(), w->GetUniqueId(), (void *)w, (AllWidgets.find(w) ? 1 : 0));
      continue;
    }
    if (w->IsDestroyed()) {
      //GCon->Logf(NAME_Debug, "(1)skipping already destroyed widget %s:%u:%p", w->GetClass()->GetName(), w->GetUniqueId(), (void *)w);
      continue;
    }
    vassert(it->IsDelayedDestroy());
    for (;;) {
      if (!w->ParentWidget) break;
      if (w->ParentWidget == GRoot) break;
      if (w->ParentWidget->IsDestroyed()) break;
      if (!w->ParentWidget->IsDelayedDestroy()) break;
      w = w->ParentWidget;
    }
    vassert(!w->IsDestroyed());
    vassert(w->IsDelayedDestroy());
    //GCon->Logf(NAME_Debug, "going to destroy widget %s:%u:%p", w->GetClass()->GetName(), w->GetUniqueId(), (void *)w);
    w->ConditionalDestroy();
  }
}


//==========================================================================
//
//  VWidget::MarkChildrenDead
//
//==========================================================================
void VWidget::MarkChildrenDead () {
  for (VWidget *w = FirstChildWidget; w; w = w->NextWidget) w->MarkDead();
}


//==========================================================================
//
//  VWidget::MarkDead
//
//==========================================================================
void VWidget::MarkDead () {
  MarkChildrenDead();
  if (ParentWidget && IsChildAdded()) ParentWidget->RemoveChild(this);
  SetDelayedDestroy();
}


//==========================================================================
//
//  VWidget::Close
//
//  don't delete it, GC will do
//
//==========================================================================
void VWidget::Close () {
  MarkDead();
}


//==========================================================================
//
//  VWidget::Init
//
//==========================================================================
void VWidget::Init (VWidget *AParent) {
  // set default values
  SetFont(SmallFont);
  SetTextAlign(hleft, vtop);

  ParentWidget = AParent;
  if (ParentWidget) ParentWidget->AddChild(this);
  ClipTree();
  OnCreate();
}


//==========================================================================
//
//  VWidget::Destroy
//
//==========================================================================
void VWidget::Destroy () {
  //GCon->Logf(NAME_Debug, "destroying widget %s:%u:%p", GetClass()->GetName(), GetUniqueId(), (void *)this);
  AllWidgets.remove(this);
  MarkChildrenDead();
  if (ParentWidget && IsChildAdded()) ParentWidget->RemoveChild(this);
  OnDestroy();
  DestroyAllChildren();
  Super::Destroy();
}


//==========================================================================
//
//  VWidget::AddChild
//
//==========================================================================
void VWidget::AddChild (VWidget *NewChild) {
  if (!NewChild || NewChild->IsGoingToDie()) return;
  if (IsGoingToDie()) return;
  if (NewChild == this) Sys_Error("VWidget::AddChild: trying to add `this` to `this`");
  if (!NewChild->ParentWidget) Sys_Error("VWidget::AddChild: trying to adopt a child without any official papers");
  if (NewChild->ParentWidget != this) Sys_Error("VWidget::AddChild: trying to adopt an alien child");
  if (NewChild->IsChildAdded()) { GCon->Log(NAME_Error, "VWidget::AddChild: adopting already adopted child"); return; }

  // link as last widget
  NewChild->PrevWidget = LastChildWidget;
  NewChild->NextWidget = nullptr;
  if (LastChildWidget) LastChildWidget->NextWidget = NewChild; else FirstChildWidget = NewChild;
  LastChildWidget = NewChild;

  // raise it (this fixes normal widgets when there are "on top" ones)
  NewChild->Raise();

  //GCon->Logf(NAME_Debug, "NewChild:%s:%u", NewChild->GetClass()->GetName(), NewChild->GetUniqueId());

  OnChildAdded(NewChild);
  if (NewChild->CanBeFocused()) {
    if (!CurrentFocusChild || NewChild->IsCloseOnBlurFlag()) SetCurrentFocusChild(NewChild);
  }
  if (NewChild->IsCloseOnBlurFlag() && CurrentFocusChild != NewChild) NewChild->Close(); // just in case
}


//==========================================================================
//
//  VWidget::RemoveChild
//
//==========================================================================
void VWidget::RemoveChild (VWidget *InChild) {
  if (!InChild) return;
  if (!InChild->IsChildAdded()) { /*GCon->Log(NAME_Error, "VWidget::RemoveChild: trying to orphan already orphaned child");*/ return; }
  if (InChild->ParentWidget != this) Sys_Error("VWidget::RemoveChild: trying to orphan an alien child");
  //if (InChild->IsCloseOnBlurFlag()) GCon->Logf(NAME_Debug, "VWidget::RemoveChild: removing %u (focus=%u)", InChild->GetUniqueId(), (CurrentFocusChild ? CurrentFocusChild->GetUniqueId() : 0));
  //if (InChild->IsCloseOnBlurFlag()) GCon->Logf(NAME_Debug, "VWidget::RemoveChild: removed %u (focus=%u)", InChild->GetUniqueId(), (CurrentFocusChild ? CurrentFocusChild->GetUniqueId() : 0));
  // remove "close on blur", because we're going to close it anyway
  //const bool oldCOB = InChild->IsCloseOnBlurFlag();
  InChild->SetCloseOnBlurFlag(false);
  // remove from parent list
  if (InChild->PrevWidget) {
    InChild->PrevWidget->NextWidget = InChild->NextWidget;
  } else {
    FirstChildWidget = InChild->NextWidget;
  }
  if (InChild->NextWidget) {
    InChild->NextWidget->PrevWidget = InChild->PrevWidget;
  } else {
    LastChildWidget = InChild->PrevWidget;
  }
  // fix focus
  if (CurrentFocusChild == InChild) FindNewFocus();
  //GCon->Logf(NAME_Debug, "%u: OnClose(); parent=%u", InChild->GetUniqueId(), GetUniqueId());
  InChild->OnClose();
  // mark as removed
  InChild->PrevWidget = nullptr;
  InChild->NextWidget = nullptr;
  InChild->ParentWidget = nullptr;
  OnChildRemoved(InChild);
}


//==========================================================================
//
//  VWidget::DestroyAllChildren
//
//==========================================================================
void VWidget::DestroyAllChildren () {
  while (FirstChildWidget) FirstChildWidget->ConditionalDestroy();
}


//==========================================================================
//
//  VWidget::GetRootWidget
//
//==========================================================================
VRootWidget *VWidget::GetRootWidget () noexcept {
  VWidget *W = this;
  while (W->ParentWidget) W = W->ParentWidget;
  return (VRootWidget *)W;
}


//==========================================================================
//
//  VWidget::FindFirstOnTopChild
//
//==========================================================================
VWidget *VWidget::FindFirstOnTopChild () noexcept {
  // start from the last widget, because it is likely to be on top
  for (VWidget *w = LastChildWidget; w; w = w->PrevWidget) {
    if (!w->IsOnTopFlag()) return w->NextWidget;
  }
  return nullptr;
}


//==========================================================================
//
//  VWidget::FindLastNormalChild
//
//==========================================================================
VWidget *VWidget::FindLastNormalChild () noexcept {
  // start from the last widget, because it is likely to be on top
  for (VWidget *w = LastChildWidget; w; w = w->PrevWidget) {
    if (!w->IsOnTopFlag()) return w;
  }
  return nullptr;
}


//==========================================================================
//
//  VWidget::UnlinkFromParent
//
//==========================================================================
void VWidget::UnlinkFromParent () noexcept {
  if (!ParentWidget || !IsChildAdded()) return;
  // unlink from current location
  if (PrevWidget) PrevWidget->NextWidget = NextWidget; else ParentWidget->FirstChildWidget = NextWidget;
  if (NextWidget) NextWidget->PrevWidget = PrevWidget; else ParentWidget->LastChildWidget = PrevWidget;
  PrevWidget = NextWidget = nullptr;
}


//==========================================================================
//
//  VWidget::LinkToParentBefore
//
//  if `w` is null, link as first
//
//==========================================================================
void VWidget::LinkToParentBefore (VWidget *w) noexcept {
  if (!ParentWidget || w == this) return;
  if (!w) {
    if (ParentWidget->FirstChildWidget == this) return; // already there
    // unlink from current location
    UnlinkFromParent();
    // link to bottom (i.e. as first child)
    PrevWidget = nullptr;
    NextWidget = ParentWidget->FirstChildWidget;
    ParentWidget->FirstChildWidget->PrevWidget = this;
    ParentWidget->FirstChildWidget = this;
  } else {
    if (w->PrevWidget == this) return; // already there
    // unlink from current location
    UnlinkFromParent();
    // link before `w`
    PrevWidget = w->PrevWidget;
    NextWidget = w;
    w->PrevWidget = this;
    if (PrevWidget) PrevWidget->NextWidget = this; else ParentWidget->FirstChildWidget = this;
  }
}


//==========================================================================
//
//  VWidget::LinkToParentAfter
//
//  if `w` is null, link as last
//
//==========================================================================
void VWidget::LinkToParentAfter (VWidget *w) noexcept {
  if (!ParentWidget || w == this) return;
  if (!w) {
    //GCon->Logf(NAME_Debug, "LinkToParentAfter:(nullptr):%u (%d)", GetUniqueId(), (int)(ParentWidget->LastChildWidget == this));
    if (ParentWidget->LastChildWidget == this) return; // already there
    // unlink from current location
    UnlinkFromParent();
    // link to top (i.e. as last child)
    PrevWidget = ParentWidget->LastChildWidget;
    NextWidget = nullptr;
    ParentWidget->LastChildWidget->NextWidget = this;
    ParentWidget->LastChildWidget = this;
  } else {
    if (w->NextWidget == this) return; // already there
    // unlink from current location
    UnlinkFromParent();
    // link after `w`
    PrevWidget = w;
    NextWidget = w->NextWidget;
    w->NextWidget = this;
    if (NextWidget) NextWidget->PrevWidget = this; else ParentWidget->LastChildWidget = this;
  }
}


//==========================================================================
//
//  VWidget::Lower
//
//==========================================================================
void VWidget::Lower () {
  if (!ParentWidget) { GCon->Log(NAME_Error, "Can't lower root window"); return; }
  LinkToParentBefore(IsOnTopFlag() ? ParentWidget->FindFirstOnTopChild() : nullptr);
}


//==========================================================================
//
//  VWidget::Raise
//
//==========================================================================
void VWidget::Raise () {
  if (!ParentWidget) { GCon->Log(NAME_Error, "Can't raise root window"); return; }
  //GCon->Logf(NAME_Debug, "raising %u (ontop=%d)", GetUniqueId(), (int)IsOnTopFlag());
  LinkToParentAfter(IsOnTopFlag() ? nullptr : ParentWidget->FindLastNormalChild());
}


//==========================================================================
//
//  VWidget::MoveBefore
//
//==========================================================================
void VWidget::MoveBefore (VWidget *Other) {
  if (!Other || Other == this) return;
  if (ParentWidget != Other->ParentWidget) { GCon->Log(NAME_Error, "Must have the same parent widget"); return; }

  if (IsOnTopFlag() != Other->IsOnTopFlag()) {
    if (IsOnTopFlag()) {
      // self is "on top", other is "normal": just lower it
      Lower();
    } else {
      // other is "on top", self is "normal": just raise it
      Raise();
    }
    return;
  }

  // link in new position
  LinkToParentBefore(Other);
}


//==========================================================================
//
//  VWidget::MoveAfter
//
//==========================================================================
void VWidget::MoveAfter (VWidget *Other) {
  if (!Other || Other == this) return;
  if (ParentWidget != Other->ParentWidget) { GCon->Log(NAME_Error, "Must have the same parent widget"); return; }

  if (IsOnTopFlag() != Other->IsOnTopFlag()) {
    if (IsOnTopFlag()) {
      // self is "on top", other is "normal": just lower it
      Lower();
    } else {
      // other is "on top", self is "normal": just raise it
      Raise();
    }
    return;
  }

  // link in new position
  LinkToParentAfter(Other);
}


//==========================================================================
//
//  VWidget::SetOnTop
//
//==========================================================================
void VWidget::SetOnTop (bool v) noexcept {
  if (v == IsOnTopFlag()) return;
  if (v) WidgetFlags |= WF_OnTop; else WidgetFlags &= ~WF_OnTop;
  if (!IsChildAdded()) return; // nothing to do yet
  Raise(); // this fixes position
}


//==========================================================================
//
//  VWidget::SetCloseOnBlur
//
//==========================================================================
void VWidget::SetCloseOnBlur (bool v) noexcept {
  if (v == IsCloseOnBlurFlag()) return;
  if (v) WidgetFlags |= WF_CloseOnBlur; else WidgetFlags &= ~WF_CloseOnBlur;
  if (!v || !IsChildAdded()) return; // nothing to do
  // if "close on blur" and not focused, close it
  if (v && !IsFocused()) Close();
}


//==========================================================================
//
//  VWidget::ClipTree
//
//==========================================================================
void VWidget::ClipTree () noexcept {
  // set up clipping rectangle
  if (ParentWidget) {
    // clipping rectangle is relative to the parent widget
    ClipRect.OriginX = ParentWidget->ClipRect.OriginX+ParentWidget->ClipRect.ScaleX*(PosX+ParentWidget->OfsX);
    ClipRect.OriginY = ParentWidget->ClipRect.OriginY+ParentWidget->ClipRect.ScaleY*(PosY+ParentWidget->OfsY);
    ClipRect.ScaleX = ParentWidget->ClipRect.ScaleX*SizeScaleX;
    ClipRect.ScaleY = ParentWidget->ClipRect.ScaleY*SizeScaleY;
    ClipRect.ClipX1 = ClipRect.OriginX;
    ClipRect.ClipY1 = ClipRect.OriginY;
    ClipRect.ClipX2 = ClipRect.OriginX+ClipRect.ScaleX*SizeWidth;
    ClipRect.ClipY2 = ClipRect.OriginY+ClipRect.ScaleY*SizeHeight;

    // clip against the parent widget's clipping rectangle
    if (ClipRect.ClipX1 < ParentWidget->ClipRect.ClipX1) ClipRect.ClipX1 = ParentWidget->ClipRect.ClipX1;
    if (ClipRect.ClipY1 < ParentWidget->ClipRect.ClipY1) ClipRect.ClipY1 = ParentWidget->ClipRect.ClipY1;
    if (ClipRect.ClipX2 > ParentWidget->ClipRect.ClipX2) ClipRect.ClipX2 = ParentWidget->ClipRect.ClipX2;
    if (ClipRect.ClipY2 > ParentWidget->ClipRect.ClipY2) ClipRect.ClipY2 = ParentWidget->ClipRect.ClipY2;
  } else {
    // this is the root widget
    ClipRect.OriginX = PosX;
    ClipRect.OriginY = PosY;
    ClipRect.ScaleX = SizeScaleX;
    ClipRect.ScaleY = SizeScaleY;
    ClipRect.ClipX1 = ClipRect.OriginX;
    ClipRect.ClipY1 = ClipRect.OriginY;
    ClipRect.ClipX2 = ClipRect.OriginX+ClipRect.ScaleX*SizeWidth;
    ClipRect.ClipY2 = ClipRect.OriginY+ClipRect.ScaleY*SizeHeight;
  }

  // set up clipping rectangles in child widgets
  for (VWidget *W = FirstChildWidget; W; W = W->NextWidget) W->ClipTree();
}


//==========================================================================
//
//  VWidget::SetConfiguration
//
//==========================================================================
void VWidget::SetConfiguration (int NewX, int NewY, int NewWidth, int HewHeight, float NewScaleX, float NewScaleY) {
  PosX = NewX;
  PosY = NewY;
  //OfsX = OfsY = 0;
  SizeWidth = NewWidth;
  SizeHeight = HewHeight;
  SizeScaleX = NewScaleX;
  SizeScaleY = NewScaleY;
  ClipTree();
  OnConfigurationChanged();
}


//==========================================================================
//
//  VWidget::SetVisibility
//
//==========================================================================
void VWidget::SetVisibility (bool NewVisibility) {
  if (IsGoingToDie()) return;
  if (IsVisibleFlag() != NewVisibility) {
    if (NewVisibility) {
      WidgetFlags |= WF_IsVisible;
      if (IsChildAdded() && ParentWidget) {
        // re-raise it, why not
        if (IsOnTopFlag()) {
          if (PrevWidget && !PrevWidget->IsOnTopFlag()) Raise();
        } else {
          if (PrevWidget && PrevWidget->IsOnTopFlag()) Raise();
        }
        if (!ParentWidget->CurrentFocusChild) ParentWidget->SetCurrentFocusChild(this);
      }
    } else {
      WidgetFlags &= ~WF_IsVisible;
      if (ParentWidget && ParentWidget->CurrentFocusChild == this) ParentWidget->FindNewFocus();
    }
    OnVisibilityChanged(NewVisibility);
  }
}


//==========================================================================
//
//  VWidget::SetEnabled
//
//==========================================================================
void VWidget::SetEnabled (bool NewEnabled) {
  if (IsGoingToDie()) return;
  if (IsEnabledFlag() != NewEnabled) {
    if (NewEnabled) {
      WidgetFlags |= WF_IsEnabled;
      if (IsChildAdded() && ParentWidget && !ParentWidget->CurrentFocusChild) ParentWidget->SetCurrentFocusChild(this);
    } else {
      WidgetFlags &= ~WF_IsEnabled;
      if (ParentWidget && ParentWidget->CurrentFocusChild == this) ParentWidget->FindNewFocus();
    }
    OnEnableChanged(NewEnabled);
  }
}


//==========================================================================
//
//  VWidget::SetFocusable
//
//==========================================================================
void VWidget::SetFocusable (bool NewFocusable) {
  if (IsGoingToDie()) return;
  if (IsFocusableFlag() != NewFocusable) {
    if (NewFocusable) {
      WidgetFlags |= WF_IsFocusable;
      if (IsChildAdded() && ParentWidget && !ParentWidget->CurrentFocusChild) ParentWidget->SetCurrentFocusChild(this);
    } else {
      WidgetFlags &= ~WF_IsFocusable;
      if (IsChildAdded() && ParentWidget && ParentWidget->CurrentFocusChild == this) ParentWidget->FindNewFocus();
    }
    OnFocusableChanged(NewFocusable);
  }
}


//==========================================================================
//
//  VWidget::SetCurrentFocusChild
//
//==========================================================================
void VWidget::SetCurrentFocusChild (VWidget *NewFocus) {
  // check if it's already focused
  if (CurrentFocusChild == NewFocus) {
    if (!CurrentFocusChild) return;
    if (!CurrentFocusChild->IsGoingToDie()) return;
    FindNewFocus();
    return;
  }

  // make sure it's visible, enabled and focusable
  if (NewFocus) {
    if (NewFocus->IsGoingToDie()) return;
    if (!NewFocus->IsChildAdded()) return; // if it is not added, get out of here
    if (!NewFocus->CanBeFocused()) return;
  }

  // if we have a focused child, send focus lost event
  VWidget *fcc = (CurrentFocusChild && CurrentFocusChild->IsCloseOnBlurFlag() ? CurrentFocusChild : nullptr);
  if (CurrentFocusChild) CurrentFocusChild->OnFocusLost();

  if (NewFocus) {
    // make it the current focus
    if (!NewFocus->IsGoingToDie()) {
      CurrentFocusChild = NewFocus;
      CurrentFocusChild->OnFocusReceived();
    } else {
      FindNewFocus();
    }
  } else {
    CurrentFocusChild = NewFocus;
  }

  if (fcc && fcc != CurrentFocusChild) fcc->Close();
}


//==========================================================================
//
//  VWidget::FindNewFocus
//
//==========================================================================
void VWidget::FindNewFocus () {
  if (CurrentFocusChild) {
    for (VWidget *W = CurrentFocusChild->NextWidget; W; W = W->NextWidget) {
      if (W->CanBeFocused()) {
        SetCurrentFocusChild(W);
        return;
      }
    }

    for (VWidget *W = CurrentFocusChild->PrevWidget; W; W = W->PrevWidget) {
      if (W->CanBeFocused()) {
        SetCurrentFocusChild(W);
        return;
      }
    }
  } else {
    for (VWidget *W = FirstChildWidget; W; W = W->NextWidget) {
      if (W->CanBeFocused()) {
        SetCurrentFocusChild(W);
        return;
      }
    }
  }

  SetCurrentFocusChild(nullptr);
}


//==========================================================================
//
//  VWidget::GetWidgetAt
//
//==========================================================================
VWidget *VWidget::GetWidgetAt (float X, float Y, bool allowDisabled) noexcept {
  if (!allowDisabled && !IsEnabled(false)) return nullptr; // don't perform recursive check
  for (VWidget *W = LastChildWidget; W; W = W->PrevWidget) {
    if (!IsVisibleFlag()) continue;
    if (W->IsGoingToDie()) continue;
    if (X >= W->ClipRect.ClipX1 && X < W->ClipRect.ClipX2 &&
        Y >= W->ClipRect.ClipY1 && Y < W->ClipRect.ClipY2)
    {
      // this can return `nullptr` for disabled widgets
      VWidget *res = W->GetWidgetAt(X, Y, allowDisabled);
      if (res) return res;
    }
  }
  return this;
}


//==========================================================================
//
//  VWidget::DrawTree
//
//==========================================================================
void VWidget::DrawTree () {
  if (IsGoingToDie()) return;
  if (!IsVisibleFlag() || !ClipRect.HasArea()) return; // not visible or clipped away

  // main draw event for this widget
  OnDraw();

  // draw chid widgets
  for (VWidget *c = FirstChildWidget; c; c = c->NextWidget) {
    if (c->IsGoingToDie()) continue;
    c->DrawTree();
  }

  // do any drawing after child wigets have been drawn
  OnPostDraw();
}


//==========================================================================
//
//  VWidget::TickTree
//
//==========================================================================
void VWidget::TickTree (float DeltaTime) {
  if (IsGoingToDie()) return;
  if (IsTickEnabledFlag()) Tick(DeltaTime);
  for (VWidget *c = FirstChildWidget; c; c = c->NextWidget) {
    if (c->IsGoingToDie()) continue;
    c->TickTree(DeltaTime);
  }
}


//==========================================================================
//
//  VWidget::ToDrawerCoords
//
//==========================================================================
void VWidget::ToDrawerCoords (float &x, float &y) const noexcept {
  x = ClipRect.ScaleX*x+ClipRect.OriginX;
  y = ClipRect.ScaleY*y+ClipRect.OriginY;
}


//==========================================================================
//
//  VWidget::ToDrawerCoords
//
//==========================================================================
void VWidget::ToDrawerCoords (int &x, int &y) const noexcept {
  x = ClipRect.ScaleX*x+ClipRect.OriginX;
  y = ClipRect.ScaleY*y+ClipRect.OriginY;
}


//==========================================================================
//
//  VWidget::TransferAndClipRect
//
//==========================================================================
bool VWidget::TransferAndClipRect (float &X1, float &Y1, float &X2, float &Y2,
  float &S1, float &T1, float &S2, float &T2) const noexcept
{
  X1 = ClipRect.ScaleX*X1+ClipRect.OriginX;
  Y1 = ClipRect.ScaleY*Y1+ClipRect.OriginY;
  X2 = ClipRect.ScaleX*X2+ClipRect.OriginX;
  Y2 = ClipRect.ScaleY*Y2+ClipRect.OriginY;
  if (X1 < ClipRect.ClipX1) {
    S1 = S1+(X1-ClipRect.ClipX1)/(X1-X2)*(S2-S1);
    X1 = ClipRect.ClipX1;
  }
  if (X2 > ClipRect.ClipX2) {
    S2 = S2+(X2-ClipRect.ClipX2)/(X1-X2)*(S2-S1);
    X2 = ClipRect.ClipX2;
  }
  if (Y1 < ClipRect.ClipY1) {
    T1 = T1+(Y1-ClipRect.ClipY1)/(Y1-Y2)*(T2-T1);
    Y1 = ClipRect.ClipY1;
  }
  if (Y2 > ClipRect.ClipY2) {
    T2 = T2+(Y2-ClipRect.ClipY2)/(Y1-Y2)*(T2-T1);
    Y2 = ClipRect.ClipY2;
  }
  return (X1 < X2 && Y1 < Y2);
}


//==========================================================================
//
//  VWidget::DrawPic
//
//==========================================================================
void VWidget::DrawPic (int X, int Y, int Handle, float Alpha, int Trans) {
  DrawPic(X, Y, GTextureManager(Handle), Alpha, Trans);
}


//==========================================================================
//
//  VWidget::DrawPic
//
//==========================================================================
void VWidget::DrawPicScaled (int X, int Y, int Handle, float scaleX, float scaleY, float Alpha, int Trans) {
  DrawPicScaled(X, Y, GTextureManager(Handle), scaleX, scaleY, Alpha, Trans);
}


//==========================================================================
//
//  VWidget::DrawPic
//
//==========================================================================
void VWidget::DrawPicScaled (int X, int Y, VTexture *Tex, float scaleX, float scaleY, float Alpha, int Trans) {
  if (!Tex || Alpha <= 0.0f || Tex->Type == TEXTYPE_Null) return;

  X -= (int)(Tex->GetScaledSOffset()*scaleX);
  Y -= (int)(Tex->GetScaledTOffset()*scaleY);
  float X1 = X;
  float Y1 = Y;
  float X2 = (int)(X+Tex->GetScaledWidth()*scaleX);
  float Y2 = (int)(Y+Tex->GetScaledHeight()*scaleY);
  float S1 = 0;
  float T1 = 0;
  float S2 = Tex->GetWidth();
  float T2 = Tex->GetHeight();
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    //fprintf(stderr, "X=%d; Y=%d; X1=%f; Y1=%f; X2=%f; Y2=%f; w1=%f; tw=%d; S1=%f; T1=%f; S2=%f; T2=%f\n", X, Y, X1, Y1, X2, Y2, X2-X1, Tex->GetWidth(), S1, T1, S2, T2);
    Drawer->DrawPic(X1, Y1, X2, Y2, S1, T1, S2, T2, Tex, R_GetCachedTranslation(Trans, nullptr), Alpha);
  }
}


//==========================================================================
//
//  VWidget::DrawPicScaledIgnoreOffset
//
//==========================================================================
void VWidget::DrawPicScaledIgnoreOffset (int X, int Y, int Handle, float scaleX, float scaleY, float Alpha, int Trans) {
  if (Alpha <= 0.0f) return;
  VTexture *Tex = GTextureManager(Handle);
  if (!Tex || Tex->Type == TEXTYPE_Null) return;

  float X1 = X;
  float Y1 = Y;
  float X2 = (int)(X+Tex->GetScaledWidth()*scaleX);
  float Y2 = (int)(Y+Tex->GetScaledHeight()*scaleY);
  float S1 = 0;
  float T1 = 0;
  float S2 = Tex->GetWidth();
  float T2 = Tex->GetHeight();
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    //fprintf(stderr, "X=%d; Y=%d; X1=%f; Y1=%f; X2=%f; Y2=%f; w1=%f; tw=%d; S1=%f; T1=%f; S2=%f; T2=%f\n", X, Y, X1, Y1, X2, Y2, X2-X1, Tex->GetWidth(), S1, T1, S2, T2);
    Drawer->DrawPic(X1, Y1, X2, Y2, S1, T1, S2, T2, Tex, R_GetCachedTranslation(Trans, nullptr), Alpha);
  }
}


//==========================================================================
//
//  VWidget::DrawPic
//
//==========================================================================
void VWidget::DrawPic (int X, int Y, VTexture *Tex, float Alpha, int Trans) {
  if (!Tex || Alpha <= 0.0f || Tex->Type == TEXTYPE_Null) return;

  X -= Tex->GetScaledSOffset();
  Y -= Tex->GetScaledTOffset();
  float X1 = X;
  float Y1 = Y;
  float X2 = X+Tex->GetScaledWidth();
  float Y2 = Y+Tex->GetScaledHeight();
  float S1 = 0;
  float T1 = 0;
  float S2 = Tex->GetWidth();
  float T2 = Tex->GetHeight();
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    //fprintf(stderr, "X=%d; Y=%d; X1=%f; Y1=%f; X2=%f; Y2=%f; w1=%f; tw=%d; S1=%f; T1=%f; S2=%f; T2=%f\n", X, Y, X1, Y1, X2, Y2, X2-X1, Tex->GetWidth(), S1, T1, S2, T2);
    Drawer->DrawPic(X1, Y1, X2, Y2, S1, T1, S2, T2, Tex, R_GetCachedTranslation(Trans, nullptr), Alpha);
  }
}


//==========================================================================
//
//  VWidget::DrawCharPic
//
//==========================================================================
void VWidget::DrawCharPic (int X, int Y, VTexture *Tex, float Alpha, bool shadowed) {
  if (!Tex || Alpha <= 0.0f || Tex->Type == TEXTYPE_Null) return;

  //GCon->Logf(NAME_Debug, "%s: pos=(%d,%d); size=(%d,%d); scale=(%g,%g); ssize=(%d,%d)", *Tex->Name, X, Y, Tex->GetWidth(), Tex->GetHeight(), Tex->SScale, Tex->TScale, Tex->GetScaledWidth(), Tex->GetScaledHeight());

  X -= Tex->GetScaledSOffset();
  Y -= Tex->GetScaledTOffset();
  float X1 = X;
  float Y1 = Y;
  float X2 = X+Tex->GetScaledWidth();
  float Y2 = Y+Tex->GetScaledHeight();
  float S1 = 0;
  float T1 = 0;
  float S2 = Tex->GetWidth();
  float T2 = Tex->GetHeight();
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    //fprintf(stderr, "X=%d; Y=%d; X1=%f; Y1=%f; X2=%f; Y2=%f; w1=%f; tw=%d; S1=%f; T1=%f; S2=%f; T2=%f\n", X, Y, X1, Y1, X2, Y2, X2-X1, Tex->GetWidth(), S1, T1, S2, T2);
    if (shadowed) Drawer->DrawPicShadow(X1, Y1, X2, Y2, S1, T1, S2, T2, Tex, 0.625f*Alpha);
    Drawer->DrawPic(X1, Y1, X2, Y2, S1, T1, S2, T2, Tex, nullptr, Alpha);
  }
}


//==========================================================================
//
//  VWidget::DrawShadowedPic
//
//==========================================================================
void VWidget::DrawShadowedPic (int X, int Y, int Handle) {
  DrawShadowedPic(X, Y, GTextureManager(Handle));
}


//==========================================================================
//
//  VWidget::DrawShadowedPic
//
//==========================================================================
void VWidget::DrawShadowedPic (int X, int Y, VTexture *Tex) {
  if (!Tex || Tex->Type == TEXTYPE_Null) return;

  float X1 = X-Tex->GetScaledSOffset()+2;
  float Y1 = Y-Tex->GetScaledTOffset()+2;
  float X2 = X-Tex->GetScaledSOffset()+2+Tex->GetScaledWidth();
  float Y2 = Y-Tex->GetScaledTOffset()+2+Tex->GetScaledHeight();
  float S1 = 0;
  float T1 = 0;
  float S2 = Tex->GetWidth();
  float T2 = Tex->GetHeight();
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->DrawPicShadow(X1, Y1, X2, Y2, S1, T1, S2, T2, Tex, 0.625f);
  }

  DrawPic(X, Y, Tex);
}


//==========================================================================
//
//  VWidget::FillRectWithFlat
//
//==========================================================================
void VWidget::FillRectWithFlat (int X, int Y, int Width, int Height, VName Name) {
  if (Name == NAME_None) return;
  VTexture *tex;
  if (VStr::strEquCI(*Name, "ScreenBackPic")) {
    if (screenBackTexNum < 1) return;
    tex = GTextureManager.getIgnoreAnim(screenBackTexNum);
  } else {
    tex = GTextureManager.getIgnoreAnim(GTextureManager.NumForName(Name, TEXTYPE_Flat, true));
  }
  if (!tex || tex->Type == TEXTYPE_Null) return;
  float X1 = X;
  float Y1 = Y;
  float X2 = X+Width;
  float Y2 = Y+Height;
  float S1 = 0;
  float T1 = 0;
  float S2 = Width;
  float T2 = Height;
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->FillRectWithFlat(X1, Y1, X2, Y2, S1, T1, S2, T2, tex);
  }
}


//==========================================================================
//
//  VWidget::FillRectWithFlatHandle
//
//==========================================================================
void VWidget::FillRectWithFlatHandle (int X, int Y, int Width, int Height, int Handle) {
  if (Handle <= 0) return;
  VTexture *tex = GTextureManager.getIgnoreAnim(Handle);
  if (!tex || tex->Type == TEXTYPE_Null) return;
  float X1 = X;
  float Y1 = Y;
  float X2 = X+Width;
  float Y2 = Y+Height;
  float S1 = 0;
  float T1 = 0;
  float S2 = Width;
  float T2 = Height;
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->FillRectWithFlat(X1, Y1, X2, Y2, S1, T1, S2, T2, tex);
  }
}


//==========================================================================
//
//  VWidget::FillRectWithFlatRepeat
//
//==========================================================================
void VWidget::FillRectWithFlatRepeat (int X, int Y, int Width, int Height, VName Name) {
  if (Name == NAME_None) return;
  VTexture *tex;
  if (VStr::strEquCI(*Name, "ScreenBackPic")) {
    if (screenBackTexNum < 1) return;
    tex = GTextureManager.getIgnoreAnim(screenBackTexNum);
  } else {
    int nn = GTextureManager.CheckNumForName(Name, TEXTYPE_Flat, true);
    tex = (nn >= 0 ? GTextureManager.getIgnoreAnim(GTextureManager.NumForName(Name, TEXTYPE_Flat, true)) : nullptr);
  }
  if (!tex) {
    // no flat: fill rect with gray color
    FillRect(X, Y, Width, Height, 0x222222, 1.0f);
    return;
  }
  if (tex->Type == TEXTYPE_Null) return;
  float X1 = X;
  float Y1 = Y;
  float X2 = X+Width;
  float Y2 = Y+Height;
  float S1 = 0;
  float T1 = 0;
  float S2 = Width;
  float T2 = Height;
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->FillRectWithFlatRepeat(X1, Y1, X2, Y2, S1, T1, S2, T2, tex);
  }
}


//==========================================================================
//
//  VWidget::FillRectWithFlatRepeatHandle
//
//==========================================================================
void VWidget::FillRectWithFlatRepeatHandle (int X, int Y, int Width, int Height, int Handle) {
  if (Handle <= 0) {
    // no flat: fill rect with gray color
    FillRect(X, Y, Width, Height, 0x222222, 1.0f);
  }
  VTexture *tex = GTextureManager.getIgnoreAnim(Handle);
  if (!tex || tex->Type == TEXTYPE_Null) {
    // no flat: fill rect with gray color
    FillRect(X, Y, Width, Height, 0x222222, 1.0f);
    return;
  }
  float X1 = X;
  float Y1 = Y;
  float X2 = X+Width;
  float Y2 = Y+Height;
  float S1 = 0;
  float T1 = 0;
  float S2 = Width;
  float T2 = Height;
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->FillRectWithFlatRepeat(X1, Y1, X2, Y2, S1, T1, S2, T2, tex);
  }
}


//==========================================================================
//
//  VWidget::FillRect
//
//==========================================================================
void VWidget::FillRect (int X, int Y, int Width, int Height, int color, float alpha) {
  float X1 = X;
  float Y1 = Y;
  float X2 = X+Width;
  float Y2 = Y+Height;
  float S1 = 0;
  float T1 = 0;
  float S2 = Width;
  float T2 = Height;
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->FillRect(X1, Y1, X2, Y2, color, alpha);
  }
}


//==========================================================================
//
//  VWidget::ShadeRect
//
//==========================================================================
void VWidget::ShadeRect (int X, int Y, int Width, int Height, float Shade) {
  float X1 = X;
  float Y1 = Y;
  float X2 = X+Width;
  float Y2 = Y+Height;
  float S1 = 0;
  float T1 = 0;
  float S2 = 0;
  float T2 = 0;
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->ShadeRect(X1, Y1, X2, Y2, Shade);
  }
}


//==========================================================================
//
//  VWidget::DrawRect
//
//==========================================================================
void VWidget::DrawRect (int X, int Y, int Width, int Height, int color, float alpha) {
  float X1 = X;
  float Y1 = Y;
  float X2 = X+Width;
  float Y2 = Y+Height;
  float S1 = 0;
  float T1 = 0;
  float S2 = Width;
  float T2 = Height;
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->DrawRect(X1, Y1, X2, Y2, color, alpha);
  }
}


//==========================================================================
//
//  VWidget::DrawLine
//
//==========================================================================
void VWidget::DrawLine (int aX0, int aY0, int aX1, int aY1, int color, float alpha) {
  float X1 = aX0;
  float Y1 = aY0;
  float X2 = aX1;
  float Y2 = aY1;
  float S1 = 0;
  float T1 = 0;
  float S2 = 1;
  float T2 = 1;
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->DrawLine(X1, Y1, X2, Y2, color, alpha);
  }
}


//==========================================================================
//
//  VWidget::GetFont
//
//==========================================================================
VFont *VWidget::GetFont () noexcept {
  return Font;
}


//==========================================================================
//
//  VWidget::SetFont
//
//==========================================================================
void VWidget::SetFont (VFont *AFont) noexcept {
  if (!AFont) Sys_Error("VWidget::SetFont: cannot set `nullptr` font");
  Font = AFont;
}


//==========================================================================
//
//  VWidget::SetFont
//
//==========================================================================
void VWidget::SetFont (VName FontName) {
  VFont *F = VFont::GetFont(VStr(FontName)); // this doesn't allocate
  if (F) {
    Font = F;
  } else {
    if (!reportedMissingFonts.put(FontName, true)) {
      GCon->Logf(NAME_Warning, "No such font '%s'", *FontName);
    }
  }
}


//==========================================================================
//
//  VWidget::SetTextAlign
//
//==========================================================================
void VWidget::SetTextAlign (halign_e NewHAlign, valign_e NewVAlign) {
  HAlign = NewHAlign;
  VAlign = NewVAlign;
}


//==========================================================================
//
//  VWidget::SetTextShadow
//
//==========================================================================
void VWidget::SetTextShadow (bool State) {
  if (State) {
    WidgetFlags |= WF_TextShadowed;
  } else {
    WidgetFlags &= ~WF_TextShadowed;
  }
}


//==========================================================================
//
//  VWidget::DrawString
//
//==========================================================================
void VWidget::DrawString (int x, int y, VStr String, int NormalColor, int BoldColor, float Alpha) {
  if (String.isEmpty() || !Font) return;

  int cx = x;
  int cy = y;
  int Kerning = Font->GetKerning();
  int Color = NormalColor;

  if (HAlign == hcenter) cx -= Font->StringWidth(String)/2;
  if (HAlign == hright) cx -= Font->StringWidth(String);

  bool oldflt = gl_pic_filtering;
  gl_pic_filtering = gl_font_filtering;

  for (const char *SPtr = *String; *SPtr; ) {
    int c = VStr::Utf8GetChar(SPtr);

    // check for color escape.
    if (c == TEXT_COLOR_ESCAPE) {
      Color = VFont::ParseColorEscape(SPtr, NormalColor, BoldColor);
      continue;
    }

    int w;
    VTexture *Tex = Font->GetChar(c, &w, Color);
    if (Tex) {
      if (WidgetFlags&WF_TextShadowed) DrawCharPicShadowed(cx, cy, Tex); else DrawCharPic(cx, cy, Tex, Alpha);
    }
    cx += w+Kerning;
  }

  gl_pic_filtering = oldflt;

  LastX = cx;
  LastY = cy;
}


//==========================================================================
//
//  VWidget::TextBounds
//
//  returns text bounds with respect to the current text align
//
//==========================================================================
void VWidget::TextBounds (int x, int y, VStr String, int *x0, int *y0, int *width, int *height, bool trimTrailNL) {
  if (x0 || y0) {
    if (x0) {
      int cx = x;
      if (HAlign == hcenter) cx -= Font->StringWidth(String)/2;
      if (HAlign == hright) cx -= Font->StringWidth(String);
      *x0 = cx;
    }

    if (y0) {
      int cy = y;
      if (VAlign == vcenter) cy -= Font->TextHeight(String)/2;
      if (VAlign == vbottom) cy -= Font->TextHeight(String);
      *y0 = cy;
    }
  }

  if (width || height) {
    if (trimTrailNL) {
      int count = 0;
      for (int pos = String.length()-1; pos >= 0 && String[pos] == '\n'; --pos) ++count;
      String.chopRight(count);
    }

    if (width) *width = Font->TextWidth(String);
    if (height) *height = Font->TextHeight(String);
  }
}


//==========================================================================
//
//  VWidget::DrawText
//
//==========================================================================
void VWidget::DrawText (int x, int y, VStr String, int NormalColor, int BoldColor, float Alpha) {
  int start = 0;
  int cx = x;
  int cy = y;

  if (VAlign == vcenter) cy -= Font->TextHeight(String)/2;
  if (VAlign == vbottom) cy -= Font->TextHeight(String);

  // need this for correct cursor position with empty strings
  LastX = cx;
  LastY = cy;

  for (int i = 0; i < String.length(); ++i) {
    if (String[i] == '\n') {
      VStr cs(String, start, i-start);
      DrawString(cx, cy, cs, NormalColor, BoldColor, Alpha);
      cy += Font->GetHeight();
      start = i+1;
    }
    if (i == String.length()-1) {
      DrawString(cx, cy, VStr(String, start, String.Length()-start), NormalColor, BoldColor, Alpha);
    }
  }
}


//==========================================================================
//
//  VWidget::TextWidth
//
//==========================================================================
int VWidget::TextWidth (VStr s) {
  return (Font ? Font->TextWidth(s) : 0);
}


//==========================================================================
//
//  VWidget::StringWidth
//
//==========================================================================
int VWidget::StringWidth (VStr s) {
  return (Font ? Font->StringWidth(s) : 0);
}


//==========================================================================
//
//  VWidget::TextHeight
//
//==========================================================================
int VWidget::TextHeight (VStr s) {
  return (Font ? Font->TextHeight(s) : 0);
}


//==========================================================================
//
//  VWidget::FontHeight
//
//==========================================================================
int VWidget::FontHeight () {
  return (Font ? Font->GetHeight() : 0);
}


//==========================================================================
//
//  VWidget::CursorWidth
//
//==========================================================================
int VWidget::CursorWidth () {
  //return TextWidth("_");
  return (Font ? Font->GetCharWidth(CursorChar) : 8);
}


//==========================================================================
//
//  VWidget::DrawCursor
//
//==========================================================================
void VWidget::DrawCursor (int cursorColor) {
  DrawCursorAt(LastX, LastY, CursorChar, cursorColor);
}


//==========================================================================
//
//  VWidget::DrawCursorAt
//
//==========================================================================
void VWidget::DrawCursorAt (int x, int y, int cursorChar, int cursorColor) {
  if ((int)(host_time*4)&1) {
    int w;
    bool oldflt = gl_pic_filtering;
    gl_pic_filtering = gl_font_filtering;
    DrawPic(x, y, Font->GetChar(cursorChar, &w, cursorColor/*CR_UNTRANSLATED*/));
    gl_pic_filtering = oldflt;
  }
}


//==========================================================================
//
//  VWidget::BroadcastEvent
//
//  to self, then to children
//  ignores cancel/consume, will prevent event modification
//
//==========================================================================
void VWidget::BroadcastEvent (event_t *evt) {
  if (!evt || IsGoingToDie()) return;
  event_t evsaved = *evt;
  OnEvent(evt);
  *evt = evsaved;
  for (VWidget *w = LastChildWidget; w; w = w->PrevWidget) {
    if (w->IsGoingToDie()) continue;
    w->BroadcastEvent(evt);
    *evt = evsaved;
  }
}


//==========================================================================
//
//  TranslateCoords
//
//==========================================================================
static bool TranslateCoords (const VClipRect &ClipRect, float &x, float &y) {
  x = ClipRect.ScaleX*x+ClipRect.OriginX;
  y = ClipRect.ScaleY*y+ClipRect.OriginY;
  return
    x >= ClipRect.ClipX1 &&
    y >= ClipRect.ClipY1 &&
    x <= ClipRect.ClipX2 &&
    y <= ClipRect.ClipY2;
}


//==========================================================================
//
//  VWidget::DrawHex
//
//==========================================================================
void VWidget::DrawHex (float x0, float y0, float w, float h, vuint32 color, float alpha) {
  if (alpha <= 0.0f || w <= 0.0f || h <= 0.0f) return;
  w *= ClipRect.ScaleX;
  h *= ClipRect.ScaleY;
  TranslateCoords(ClipRect, x0, y0);
  Drawer->DrawHex(x0, y0, w, h, color, alpha);
}


//==========================================================================
//
//  VWidget::FillHex
//
//==========================================================================
void VWidget::FillHex (float x0, float y0, float w, float h, vuint32 color, float alpha) {
  if (alpha <= 0.0f || w <= 0.0f || h <= 0.0f) return;
  w *= ClipRect.ScaleX;
  h *= ClipRect.ScaleY;
  TranslateCoords(ClipRect, x0, y0);
  Drawer->FillHex(x0, y0, w, h, color, alpha);
}


//==========================================================================
//
//  VWidget::ShadeHex
//
//==========================================================================
void VWidget::ShadeHex (float x0, float y0, float w, float h, float darkening) {
  if (darkening <= 0.0f || w <= 0.0f || h <= 0.0f) return;
  w *= ClipRect.ScaleX;
  h *= ClipRect.ScaleY;
  TranslateCoords(ClipRect, x0, y0);
  Drawer->ShadeHex(x0, y0, w, h, darkening);
}


//==========================================================================
//
//  VWidget::CalcHexHeight
//
//==========================================================================
float VWidget::CalcHexHeight (float h) {
  if (h <= 0.0f) return 0.0f;
  return VDrawer::CalcRealHexHeight(h*ClipRect.ScaleY)/ClipRect.ScaleY;
}


//==========================================================================
//
//  VWidget::CalcHexColorPatternDims
//
//==========================================================================
void VWidget::CalcHexColorPatternDims (float *w, float *h, int radius, float cellW, float cellH) {
  if (!w && !h) return;
  if (radius < 1) radius = 1;
  if (cellW < 1.0f) cellW = 1.0f;
  if (cellH < 1.0f) cellH = 1.0f;
  const int diameter = radius*2;
  // width
  if (w) {
    const float scrW = cellW*ClipRect.ScaleX;
    *w = scrW*(diameter+1)/ClipRect.ScaleX;
  }
  // height
  if (h) {
    const float scrH = cellH*ClipRect.ScaleY;
    // calculate real hex height
    const float realScrH = VDrawer::CalcRealHexHeight(scrH);
    //*h = (realScrH*(radius*2+1)+scrH/3.0f)/ClipRect.ScaleY;
    *h = realScrH*(diameter+1)/ClipRect.ScaleY+cellH/3.0f*2.0f/3.0f;
  }
}


// returns `true` if the given "internal" hex pattern
// coordinates are valid
bool VWidget::IsValidHexColorPatternCoords (int hpx, int hpy, int radius) {
  if (radius < 1) radius = 1;
  hpx *= 2;
  hpy *= 2;
  const int diameter = radius*2;
  if (hpy < -diameter || hpy > diameter) return false;
  const int rowSize = diameter-abs(hpy/2);
  return (hpx >= -rowSize && hpx <= rowSize);
 }


// calcs "internal" hex pattern coordinates,
// returns `false` if passed coords are outside any hex
bool VWidget::CalcHexColorPatternCoords (int *hpx, int *hpy, float x, float y, float x0, float y0, int radius, float cellW, float cellH) {
  int tmpx, tmpy;
  if (!hpx) hpx = &tmpx;
  if (!hpy) hpy = &tmpy;
  if (x < x0 || y < y0) { *hpx = -1; *hpy = -1; return false; }
  if (radius < 1) radius = 1;
  if (cellW < 1.0f) cellW = 1.0f;
  if (cellH < 1.0f) cellH = 1.0f;
  const int diameter = radius*2;
  // width
  const float scrW = cellW*ClipRect.ScaleX;
  const float scrH = cellH*ClipRect.ScaleY;
  // calculate real hex height
  const float realScrH = VDrawer::CalcRealHexHeight(scrH);
  const float w = scrW*(diameter+1)/ClipRect.ScaleX;
  const float h = realScrH*(diameter+1)/ClipRect.ScaleY+cellH/3.0f*2.0f/3.0f;
  if (x >= x0+w || y >= y0+h) { *hpx = -1; *hpy = -1; return false; }
  int tx = (int)((x-x0)/cellW/2.0f);
  int ty = (int)((y-y0)/VDrawer::CalcRealHexHeight(cellH)/2.0f);
  *hpx = tx;
  *hpy = ty;
  tx *= 2;
  ty *= 2;
  if (ty < -diameter || ty > diameter) return false;
  const int rowSize = diameter-abs(ty/2);
  return (tx >= -rowSize && tx <= rowSize);
}


// calcs coordinates of the individual hex;
// returns `false` if the given "internal" hp coordinates are not valid
// those coords can be used in `*Hex()` methods
bool VWidget::CalcHexColorPatternHexCoordsAt (float *hx, float *hy, int hpx, int hpy, float x0, float y0, int radius, float cellW, float cellH) {
  if (radius < 1) radius = 1;
  if (cellW < 1.0f) cellW = 1.0f;
  if (cellH < 1.0f) cellH = 1.0f;
  if (hx) {
    *hx = x0+cellW*radius+(float)hpx*cellW;
  }
  if (hy) {
    const float realCH = VDrawer::CalcRealHexHeight(cellH);
    *hy = y0+realCH*radius+(float)hpy*realCH;
  }
  return IsValidHexColorPatternCoords(hpx, hpy, radius);
}


//==========================================================================
//
//  CalcHexColorByCoords
//
//==========================================================================
static vuint32 CalcHexColorByCoords (int x, int y, int radius) {
  const float diameter = radius*2;
  const float h = 360.0f*(0.5f-0.5f*atan2f(y, -x)/(float)(M_PI));
  const float s = sqrtf(x*x+y*y)/diameter;
  const float v = 1.0f;
  float r, g, b;
  M_HsvToRgb(h, s, v, r, g, b);
  return PackRGBf(r, g, b);
}


// returns opaque color at the given "internal" hex pattern
// coordinates (with high byte set), or 0
int VWidget::GetHexColorPatternColorAt (int hpx, int hpy, int radius) {
  if (!IsValidHexColorPatternCoords(hpx, hpy, radius)) return 0;
  return CalcHexColorByCoords(hpx*2, hpy*2, radius);
}


//==========================================================================
//
//  VWidget::DrawHexColorPattern
//
//==========================================================================
void VWidget::DrawHexColorPattern (float x0, float y0, int radius, float cellW, float cellH) {
  if (radius < 1) radius = 1;
  if (cellW < 1.0f) cellW = 1.0f;
  if (cellH < 1.0f) cellH = 1.0f;
  const float scrW = cellW*ClipRect.ScaleX;
  const float scrH = cellH*ClipRect.ScaleY;
  // calculate real hex height
  const float realScrH = VDrawer::CalcRealHexHeight(scrH);

  TranslateCoords(ClipRect, x0, y0);
  x0 += scrW*radius;
  y0 += realScrH*radius;

  const int diameter = radius*2;
  for (int y = -diameter; y <= diameter; y += 2) {
    const int rowSize = diameter-abs(y/2);
    for (int x = -rowSize; x <= rowSize; x += 2) {
      vuint32 clr = CalcHexColorByCoords(x, y, radius);
      float xc = (float)x/2.0f;
      float yc = (float)y/2.0f;
      xc = x0+xc*scrW;
      yc = y0+yc*realScrH;
      Drawer->FillHex(xc, yc, scrW, scrH, clr);
    }
  }
}


//==========================================================================
//
//  Natives
//
//==========================================================================
IMPLEMENT_FUNCTION(VWidget, NewChild) {
  VClass *ChildClass;
  vobjGetParamSelf(ChildClass);
  RET_REF(CreateNewWidget(ChildClass, Self));
}

IMPLEMENT_FUNCTION(VWidget, Destroy) {
  vobjGetParamSelf();
  //k8: don't delete it, GC will do
  if (Self) Self->Close();
}

IMPLEMENT_FUNCTION(VWidget, DestroyAllChildren) {
  vobjGetParamSelf();
  if (Self) Self->MarkChildrenDead();
}

IMPLEMENT_FUNCTION(VWidget, GetRootWidget) {
  vobjGetParamSelf();
  RET_REF(Self ? Self->GetRootWidget() : nullptr);
}

IMPLEMENT_FUNCTION(VWidget, Lower) {
  vobjGetParamSelf();
  if (Self) Self->Lower();
}

IMPLEMENT_FUNCTION(VWidget, Raise) {
  vobjGetParamSelf();
  if (Self) Self->Raise();
}

IMPLEMENT_FUNCTION(VWidget, MoveBefore) {
  VWidget *Other;
  vobjGetParamSelf(Other);
  if (Self) Self->MoveBefore(Other);
}

IMPLEMENT_FUNCTION(VWidget, MoveAfter) {
  VWidget *Other;
  vobjGetParamSelf(Other);
  if (Self) Self->MoveAfter(Other);
}

IMPLEMENT_FUNCTION(VWidget, SetPos) {
  int NewX, NewY;
  vobjGetParamSelf(NewX, NewY);
  if (Self) Self->SetPos(NewX, NewY);
}

IMPLEMENT_FUNCTION(VWidget, SetX) {
  int NewX;
  vobjGetParamSelf(NewX);
  if (Self) Self->SetX(NewX);
}

IMPLEMENT_FUNCTION(VWidget, SetY) {
  int NewY;
  vobjGetParamSelf(NewY);
  if (Self) Self->SetY(NewY);
}

IMPLEMENT_FUNCTION(VWidget, SetOfsX) {
  int NewX;
  vobjGetParamSelf(NewX);
  if (Self) Self->SetOfsX(NewX);
}

IMPLEMENT_FUNCTION(VWidget, SetOfsY) {
  int NewY;
  vobjGetParamSelf(NewY);
  if (Self) Self->SetOfsY(NewY);
}

IMPLEMENT_FUNCTION(VWidget, SetSize) {
  int NewWidth, NewHeight;
  vobjGetParamSelf(NewWidth, NewHeight);
  if (Self) Self->SetSize(NewWidth, NewHeight);
}

IMPLEMENT_FUNCTION(VWidget, SetWidth) {
  int NewWidth;
  vobjGetParamSelf(NewWidth);
  if (Self) Self->SetWidth(NewWidth);
}

IMPLEMENT_FUNCTION(VWidget, SetHeight) {
  int NewHeight;
  vobjGetParamSelf(NewHeight);
  if (Self) Self->SetHeight(NewHeight);
}

IMPLEMENT_FUNCTION(VWidget, SetScale) {
  float NewScaleX, NewScaleY;
  vobjGetParamSelf(NewScaleX, NewScaleY);
  if (Self) Self->SetScale(NewScaleX, NewScaleY);
}

IMPLEMENT_FUNCTION(VWidget, SetConfiguration) {
  int NewX, NewY, NewWidth, NewHeight;
  VOptParamFloat NewScaleX(1.0f);
  VOptParamFloat NewScaleY(1.0f);
  vobjGetParamSelf(NewX, NewY, NewWidth, NewHeight, NewScaleX, NewScaleY);
  if (Self) Self->SetConfiguration(NewX, NewY, NewWidth, NewHeight, NewScaleX, NewScaleY);
}

IMPLEMENT_FUNCTION(VWidget, SetVisibility) {
  bool bNewVisibility;
  vobjGetParamSelf(bNewVisibility);
  if (Self) Self->SetVisibility(bNewVisibility);
}

IMPLEMENT_FUNCTION(VWidget, Show) {
  vobjGetParamSelf();
  if (Self) Self->Show();
}

IMPLEMENT_FUNCTION(VWidget, Hide) {
  vobjGetParamSelf();
  if (Self) Self->Hide();
}

IMPLEMENT_FUNCTION(VWidget, IsVisible) {
  VOptParamBool Recurse(true);
  vobjGetParamSelf(Recurse);
  RET_BOOL(Self ? Self->IsVisible(Recurse) : false);
}

IMPLEMENT_FUNCTION(VWidget, SetEnabled) {
  bool bNewEnabled;
  vobjGetParamSelf(bNewEnabled);
  if (Self) Self->SetEnabled(bNewEnabled);
}

IMPLEMENT_FUNCTION(VWidget, Enable) {
  vobjGetParamSelf();
  if (Self) Self->Enable();
}

IMPLEMENT_FUNCTION(VWidget, Disable) {
  vobjGetParamSelf();
  if (Self) Self->Disable();
}

IMPLEMENT_FUNCTION(VWidget, IsEnabled) {
  VOptParamBool Recurse(true);
  vobjGetParamSelf(Recurse);
  RET_BOOL(Self ? Self->IsEnabled(Recurse) : false);
}

IMPLEMENT_FUNCTION(VWidget, SetOnTop) {
  bool bNewOnTop;
  vobjGetParamSelf(bNewOnTop);
  if (Self) Self->SetOnTop(bNewOnTop);
}

IMPLEMENT_FUNCTION(VWidget, IsOnTop) {
  vobjGetParamSelf();
  RET_BOOL(Self ? Self->IsOnTop() : false);
}

IMPLEMENT_FUNCTION(VWidget, SetCloseOnBlur) {
  bool bNewCloseOnBlur;
  vobjGetParamSelf(bNewCloseOnBlur);
  if (Self) Self->SetCloseOnBlur(bNewCloseOnBlur);
}

IMPLEMENT_FUNCTION(VWidget, IsCloseOnBlur) {
  vobjGetParamSelf();
  RET_BOOL(Self ? Self->IsCloseOnBlur() : false);
}

IMPLEMENT_FUNCTION(VWidget, IsModal) {
  vobjGetParamSelf();
  RET_BOOL(Self ? Self->IsModal() : false);
}

IMPLEMENT_FUNCTION(VWidget, SetFocusable) {
  bool bNewFocusable;
  vobjGetParamSelf(bNewFocusable);
  if (Self) Self->SetFocusable(bNewFocusable);
}

IMPLEMENT_FUNCTION(VWidget, IsFocusable) {
  vobjGetParamSelf();
  RET_BOOL(Self ? Self->IsFocusable() : false);
}

IMPLEMENT_FUNCTION(VWidget, SetCurrentFocusChild) {
  VWidget *NewFocus;
  vobjGetParamSelf(NewFocus);
  if (Self) Self->SetCurrentFocusChild(NewFocus);
}

IMPLEMENT_FUNCTION(VWidget, GetCurrentFocus) {
  vobjGetParamSelf();
  RET_REF(Self ? Self->GetCurrentFocus() : nullptr);
}

IMPLEMENT_FUNCTION(VWidget, IsFocused) {
  VOptParamBool Recurse(true);
  vobjGetParamSelf(Recurse);
  RET_BOOL(Self ? Self->IsFocused(Recurse) : false);
}

IMPLEMENT_FUNCTION(VWidget, SetFocus) {
  VOptParamBool onlyInParent(false);
  vobjGetParamSelf(onlyInParent);
  if (Self) Self->SetFocus(onlyInParent);
}


IMPLEMENT_FUNCTION(VWidget, DrawPicScaledIgnoreOffset) {
  int X, Y, Handle;
  VOptParamFloat scaleX(1.0f);
  VOptParamFloat scaleY(1.0f);
  VOptParamFloat Alpha(1.0f);
  VOptParamInt Translation(0);
  vobjGetParamSelf(X, Y, Handle, scaleX, scaleY, Alpha, Translation);
  if (Self) Self->DrawPicScaledIgnoreOffset(X, Y, Handle, scaleX, scaleY, Alpha, Translation);
}

IMPLEMENT_FUNCTION(VWidget, DrawPic) {
  int X, Y, Handle;
  VOptParamFloat Alpha(1.0f);
  VOptParamInt Translation(0);
  vobjGetParamSelf(X, Y, Handle, Alpha, Translation);
  if (Self) Self->DrawPic(X, Y, Handle, Alpha, Translation);
}

IMPLEMENT_FUNCTION(VWidget, DrawPicScaled) {
  int X, Y, Handle;
  float scaleX, scaleY;
  VOptParamFloat Alpha(1.0f);
  VOptParamInt Translation(0);
  vobjGetParamSelf(X, Y, Handle, scaleX, scaleY, Alpha, Translation);
  if (Self) Self->DrawPicScaled(X, Y, Handle, scaleX, scaleY, Alpha, Translation);
}

IMPLEMENT_FUNCTION(VWidget, DrawShadowedPic) {
  int X, Y, Handle;
  vobjGetParamSelf(X, Y, Handle);
  if (Self) Self->DrawShadowedPic(X, Y, Handle);
}

IMPLEMENT_FUNCTION(VWidget, FillRectWithFlat) {
  int X, Y, Width, Height;
  VName Name;
  vobjGetParamSelf(X, Y, Width, Height, Name);
  if (Self) Self->FillRectWithFlat(X, Y, Width, Height, Name);
}

IMPLEMENT_FUNCTION(VWidget, FillRectWithFlatHandle) {
  int X, Y, Width, Height, Handle;
  vobjGetParamSelf(X, Y, Width, Height, Handle);
  if (Self) Self->FillRectWithFlatHandle(X, Y, Width, Height, Handle);
}

IMPLEMENT_FUNCTION(VWidget, FillRectWithFlatRepeat) {
  int X, Y, Width, Height;
  VName Name;
  vobjGetParamSelf(X, Y, Width, Height, Name);
  if (Self) Self->FillRectWithFlatRepeat(X, Y, Width, Height, Name);
}

IMPLEMENT_FUNCTION(VWidget, FillRectWithFlatRepeatHandle) {
  int X, Y, Width, Height, Handle;
  vobjGetParamSelf(X, Y, Width, Height, Handle);
  if (Self) Self->FillRectWithFlatRepeatHandle(X, Y, Width, Height, Handle);
}

IMPLEMENT_FUNCTION(VWidget, FillRect) {
  int X, Y, Width, Height;
  vuint32 color;
  VOptParamFloat alpha(1.0f);
  vobjGetParamSelf(X, Y, Width, Height, color, alpha);
  if (Self) Self->FillRect(X, Y, Width, Height, color, alpha);
}

IMPLEMENT_FUNCTION(VWidget, DrawRect) {
  int X, Y, Width, Height;
  vuint32 color;
  VOptParamFloat alpha(1.0f);
  vobjGetParamSelf(X, Y, Width, Height, color, alpha);
  if (Self) Self->DrawRect(X, Y, Width, Height, color, alpha);
}

IMPLEMENT_FUNCTION(VWidget, ShadeRect) {
  int X, Y, Width, Height;
  float Shade;
  vobjGetParamSelf(X, Y, Width, Height, Shade);
  if (Self) Self->ShadeRect(X, Y, Width, Height, Shade);
}

IMPLEMENT_FUNCTION(VWidget, DrawLine) {
  int X1, Y1, X2, Y2;
  vuint32 color;
  VOptParamFloat alpha(1.0f);
  vobjGetParamSelf(X1, Y1, X2, Y2, color, alpha);
  if (Self) Self->DrawLine(X1, Y1, X2, Y2, color, alpha);
}

IMPLEMENT_FUNCTION(VWidget, GetFont) {
  vobjGetParamSelf();
  if (Self) {
    VFont *font = Self->GetFont();
    if (font) { RET_NAME(font->GetFontName()); return; }
  }
  RET_NAME(NAME_None);
}

IMPLEMENT_FUNCTION(VWidget, SetFont) {
  VName FontName;
  vobjGetParamSelf(FontName);
  if (Self) Self->SetFont(FontName);
}

IMPLEMENT_FUNCTION(VWidget, SetTextAlign) {
  int halign, valign;
  vobjGetParamSelf(halign, valign);
  if (Self) Self->SetTextAlign((halign_e)halign, (valign_e)valign);
}

IMPLEMENT_FUNCTION(VWidget, SetTextShadow) {
  bool State;
  vobjGetParamSelf(State);
  if (Self) Self->SetTextShadow(State);
}

// native final void TextBounds (int x, int y, string text, out int x0, out int y0, out int width, out int height, optional bool trimTrailNL);
IMPLEMENT_FUNCTION(VWidget, TextBounds) {
  int x, y;
  VStr text;
  int *x0, *y0, *width, *height;
  VOptParamBool trimTrailNL(true);
  vobjGetParamSelf(x, y, text, x0, y0, width, height, trimTrailNL);
  if (Self) {
    Self->TextBounds(x, y, text, x0, y0, width, height, trimTrailNL);
  } else {
    if (x0) *x0 = x;
    if (y0) *y0 = y;
    if (width) *width = 0;
    if (height) *height = 0;
  }
}

IMPLEMENT_FUNCTION(VWidget, TextWidth) {
  VStr text;
  vobjGetParamSelf(text);
  RET_INT(Self ? Self->TextWidth(text) : 0);
}

IMPLEMENT_FUNCTION(VWidget, StringWidth) {
  VStr text;
  vobjGetParamSelf(text);
  RET_INT(Self ? Self->TextWidth(text) : 0);
}

IMPLEMENT_FUNCTION(VWidget, TextHeight) {
  VStr text;
  vobjGetParamSelf(text);
  RET_INT(Self ? Self->TextHeight(text) : 0);
}

IMPLEMENT_FUNCTION(VWidget, FontHeight) {
  vobjGetParamSelf();
  RET_INT(Self ? Self->FontHeight() : 0);
}

IMPLEMENT_FUNCTION(VWidget, SplitText) {
  VStr Text;
  TArray<VSplitLine> *Lines;
  int MaxWidth;
  VOptParamBool trimRight(true);
  vobjGetParamSelf(Text, Lines, MaxWidth, trimRight);
  RET_INT(Self ? Self->Font->SplitText(Text, *Lines, MaxWidth, trimRight) : 0);
}

IMPLEMENT_FUNCTION(VWidget, SplitTextWithNewlines) {
  VStr Text;
  int MaxWidth;
  VOptParamBool trimRight(true);
  vobjGetParamSelf(Text, MaxWidth, trimRight);
  RET_STR(Self ? Self->Font->SplitTextWithNewlines(Text, MaxWidth, trimRight) : VStr::EmptyString);
}

IMPLEMENT_FUNCTION(VWidget, DrawText) {
  int X, Y;
  VStr String;
  VOptParamInt Color(CR_UNTRANSLATED);
  VOptParamInt BoldColor(CR_UNTRANSLATED);
  VOptParamFloat Alpha(1.0f);
  vobjGetParamSelf(X, Y, String, Color, BoldColor, Alpha);
  if (Self) Self->DrawText(X, Y, String, Color, BoldColor, Alpha);
}

IMPLEMENT_FUNCTION(VWidget, CursorWidth) {
  vobjGetParamSelf();
  RET_INT(Self ? Self->CursorWidth() : 0);
}

IMPLEMENT_FUNCTION(VWidget, DrawCursor) {
  VOptParamInt Color(CR_UNTRANSLATED);
  vobjGetParamSelf(Color);
  if (Self) Self->DrawCursor(Color);
}

IMPLEMENT_FUNCTION(VWidget, DrawCursorAt) {
  int cx, cy;
  VOptParamInt Color(CR_UNTRANSLATED);
  vobjGetParamSelf(cx, cy, Color);
  if (Self) Self->DrawCursorAt(cx, cy, Color);
}

IMPLEMENT_FUNCTION(VWidget, SetCursorPos) {
  int cx, cy;
  vobjGetParamSelf(cx, cy);
  Self->SetCursorPos(cx, cy);
}

IMPLEMENT_FUNCTION(VWidget, get_CursorX) {
  vobjGetParamSelf();
  RET_INT(Self ? Self->GetCursorX() : 0);
}

IMPLEMENT_FUNCTION(VWidget, get_CursorY) {
  vobjGetParamSelf();
  RET_INT(Self ? Self->GetCursorY() : 0);
}

IMPLEMENT_FUNCTION(VWidget, set_CursorX) {
  int v;
  vobjGetParamSelf(v);
  if (Self) Self->SetCursorX(v);
}

IMPLEMENT_FUNCTION(VWidget, set_CursorY) {
  int v;
  vobjGetParamSelf(v);
  if (Self) Self->SetCursorY(v);
}

IMPLEMENT_FUNCTION(VWidget, FindTextColor) {
  VStr Name;
  vobjGetParam(Name);
  RET_INT(VFont::FindTextColor(*Name.ToLower()));
}

IMPLEMENT_FUNCTION(VWidget, TranslateXY) {
  float *px, *py;
  vobjGetParamSelf(px, py);
  if (Self) {
    if (px) *px = (Self->ClipRect.ScaleX*(*px)+Self->ClipRect.OriginX)/fScaleX;
    if (py) *py = (Self->ClipRect.ScaleY*(*py)+Self->ClipRect.OriginY)/fScaleY;
  } else {
    if (px) *px = 0;
    if (py) *py = 0;
  }
}


// native final Widget GetWidgetAt (float x, float y, optional bool allowDisabled/*=false*/);
IMPLEMENT_FUNCTION(VWidget, GetWidgetAt) {
  float x, y;
  VOptParamBool allowDisabled(false);
  vobjGetParamSelf(x, y, allowDisabled);
  RET_REF(Self ? Self->GetWidgetAt(x, y, allowDisabled) : nullptr);
}


// native final float CalcHexHeight (float h);
IMPLEMENT_FUNCTION(VWidget, CalcHexHeight) {
  float h;
  vobjGetParamSelf(h);
  RET_FLOAT(Self ? Self->CalcHexHeight(h) : 0.0f);
}

// native final void DrawHex (float x0, float y0, float w, float h, int color, optional float alpha/*=1.0f*/);
IMPLEMENT_FUNCTION(VWidget, DrawHex) {
  float x0, y0, w, h;
  vuint32 color;
  VOptParamFloat alpha(1.0f);
  vobjGetParamSelf(x0, y0, w, h, color, alpha);
  if (Self) Self->DrawHex(x0, y0, w, h, color, alpha);
}

// native final void FillHex (float x0, float y0, float w, float h, int color, optional float alpha/*=1.0f*/);
IMPLEMENT_FUNCTION(VWidget, FillHex) {
  float x0, y0, w, h;
  vuint32 color;
  VOptParamFloat alpha(1.0f);
  vobjGetParamSelf(x0, y0, w, h, color, alpha);
  if (Self) Self->FillHex(x0, y0, w, h, color, alpha);
}

// native final void ShadeHex (float x0, float y0, float w, float h, float darkening);
IMPLEMENT_FUNCTION(VWidget, ShadeHex) {
  float x0, y0, w, h, darkening;
  vobjGetParamSelf(x0, y0, w, h, darkening);
  if (Self) Self->ShadeHex(x0, y0, w, h, darkening);
}


// native final void DrawHexColorPattern (float x0, float y0, int radius, float cellW, float cellH);
IMPLEMENT_FUNCTION(VWidget, DrawHexColorPattern) {
  float x0, y0;
  int radius;
  float cellW, cellH;
  vobjGetParamSelf(x0, y0, radius, cellW, cellH);
  if (Self) Self->DrawHexColorPattern(x0, y0, radius, cellW, cellH);
}


// native final float CalcHexColorPatternWidth (int radius, float cellW, float cellH);
IMPLEMENT_FUNCTION(VWidget, CalcHexColorPatternWidth) {
  int radius;
  float cellW, cellH;
  vobjGetParamSelf(radius, cellW, cellH);
  if (Self) {
    float w;
    Self->CalcHexColorPatternDims(&w, nullptr, radius, cellW, cellH);
    RET_FLOAT(w);
  } else {
    RET_FLOAT(0.0f);
  }
}

// native final float CalcHexColorPatternHeight (int radius, float cellW, float cellH);
IMPLEMENT_FUNCTION(VWidget, CalcHexColorPatternHeight) {
  int radius;
  float cellW, cellH;
  vobjGetParamSelf(radius, cellW, cellH);
  if (Self) {
    float h;
    Self->CalcHexColorPatternDims(nullptr, &h, radius, cellW, cellH);
    RET_FLOAT(h);
  } else {
    RET_FLOAT(0.0f);
  }
}

// native final bool IsValidHexColorPatternCoords (int hpx, int hpy, int radius);
IMPLEMENT_FUNCTION(VWidget, IsValidHexColorPatternCoords) {
  int hpx, hpy, radius;
  vobjGetParamSelf(hpx, hpy, radius);
  RET_BOOL(Self ? Self->IsValidHexColorPatternCoords(hpx, hpy, radius) : false);
}

// native final bool CalcHexColorPatternCoords (out int hpx, out int hpy, float x, float y, float x0, float y0, int radius, float cellW, float cellH);
IMPLEMENT_FUNCTION(VWidget, CalcHexColorPatternCoords) {
  int *hpx;
  int *hpy;
  float x, y, x0, y0;
  int radius;
  float cellW, cellH;
  vobjGetParamSelf(hpx, hpy, x, y, x0, y0, radius, cellW, cellH);
  RET_BOOL(Self ? Self->CalcHexColorPatternCoords(hpx, hpy, x, y, x0, y0, radius, cellW, cellH) : false);
}

// native final bool CalcHexColorPatternHexCoordsAt (out float hx, out float hy, int hpx, int hpy, float x0, float y0, int radius, float cellW, float cellH);
IMPLEMENT_FUNCTION(VWidget, CalcHexColorPatternHexCoordsAt) {
  float *hx;
  float *hy;
  int hpx, hpy;
  float x0, y0;
  int radius;
  float cellW, cellH;
  vobjGetParamSelf(hx, hy, hpx, hpy, x0, y0, radius, cellW, cellH);
  RET_BOOL(Self ? Self->CalcHexColorPatternHexCoordsAt(hx, hy, hpx, hpy, x0, y0, radius, cellW, cellH) : false);
}

// native final int GetHexColorPatternColorAt (int hpx, int hpy, int radius);
IMPLEMENT_FUNCTION(VWidget, GetHexColorPatternColorAt) {
  int hpx, hpy, radius;
  vobjGetParamSelf(hpx, hpy, radius);
  RET_BOOL(Self ? Self->GetHexColorPatternColorAt(hpx, hpy, radius) : 0);
}
