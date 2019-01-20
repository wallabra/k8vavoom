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
#include "gamedefs.h"
#include "cl_local.h"
#include "drawer.h"
#include "ui.h"

extern VCvarB gl_pic_filtering;
extern VCvarB gl_font_filtering;


static VCvarF ui_msgxbox_wrap_trigger("ui_msgxbox_wrap_trigger", "0.9", "Maximum width (1 means whole screen) before message box will start wrapping; <=0 means \"don't\".", CVAR_Archive);
static VCvarF ui_msgxbox_wrap_width("ui_msgxbox_wrap_width", "0.7", "Width (1 means whole screen) for message box wrapping; <=0 means \"don't\".", CVAR_Archive);

static TMapNC<VName, bool> reportedMissingFonts;


IMPLEMENT_CLASS(V, Widget);


//==========================================================================
//
//  VWidget::CreateNewWidget
//
//==========================================================================
VWidget *VWidget::CreateNewWidget (VClass *AClass, VWidget *AParent) {
  guard(VWidget::CreateNewWidget);
  VWidget *W = (VWidget *)StaticSpawnWithReplace(AClass);
  W->OfsX = W->OfsY = 0;
  W->Init(AParent);
  return W;
  unguardf(("(%s)", AClass->GetName()));
}


//==========================================================================
//
//  VWidget::cleanupWidgets
//
//==========================================================================
void VWidget::cleanupWidgets () {
  //if ((GetFlags()&_OF_Destroyed) != 0) return;
  // if we're marked as dead, kill us and gtfo
  //if (IsDeadManWalking()) return;
  if (IsDeadManWalking()) { Destroy(); return; }
  // need cleanup?
  if (!IsNeedCleanup()) return;
  // do children cleanup
  VWidget *w = FirstChildWidget;
  while (w) {
    VWidget *next = w->NextWidget;
    // is child alive?
    if ((w->GetFlags()&_OF_Destroyed) == 0) {
      // is child marked as dead?
      if (w->IsDeadManWalking()) {
        // destroy it, and go on
        w->Destroy();
        w = next;
        continue;
      }
      // do child cleanup
      w->cleanupWidgets();
      // is child alive?
      /*
      if ((w->GetFlags()&_OF_Destroyed) != 0) {
        // nope
        w = next;
        continue;
      }
      */
      // is child marked as dead?
      if (w->IsDeadManWalking()) {
        // destroy it, and go on
        w->Destroy();
        w = next;
        continue;
      }
    }
    w = next;
  }
}


//==========================================================================
//
//  VWidget::MarkDead
//
//==========================================================================
void VWidget::MarkDead () {
  //if ((GetFlags()&_OF_Destroyed) != 0) return;
#if 1
  WidgetFlags |= WF_DeadManWalking;
  for (VWidget *w = FirstChildWidget; w; w = w->NextWidget) w->MarkDead();
  for (VWidget *w = this; w; w = w->ParentWidget) w->WidgetFlags |= WF_NeedCleanup;
#else
  DestroyAllChildren();
  ConditionalDestroy();
#endif
}


//==========================================================================
//
//  VWidget::Init
//
//==========================================================================
void VWidget::Init (VWidget *AParent) {
  guard(VWidget::Init);
  // set default values
  SetFont(SmallFont);
  SetTextAlign(hleft, vtop);

  ParentWidget = AParent;
  if (ParentWidget) ParentWidget->AddChild(this);
  ClipTree();
  OnCreate();
  unguard;
}


//==========================================================================
//
//  VWidget::Destroy
//
//==========================================================================
void VWidget::Destroy () {
  guard(VWidget::Destroy);
  /*
  if (ParentWidget) {
    GCon->Logf("%p: removing widget `%s` (parent is `%s`)", this, GetClass()->GetName(), ParentWidget->GetClass()->GetName());
  } else {
    GCon->Logf("%p: removing orphan widget `%s`", this, GetClass()->GetName());
  }
  */
  //if ((GetFlags()&_OF_Destroyed) != 0) return;
  OnDestroy();
  if (ParentWidget) ParentWidget->RemoveChild(this);
  DestroyAllChildren();
  Super::Destroy();
  unguard;
}


//==========================================================================
//
//  VWidget::AddChild
//
//==========================================================================
void VWidget::AddChild (VWidget *NewChild) {
  guard(VWidget::AddChild);
  if (!NewChild) return;
  if (NewChild->IsDeadManWalking() || (NewChild->GetFlags()&_OF_Destroyed) != 0) return;
  if (NewChild == this) Sys_Error("VWidget::AddChild: trying to add `this` to `this`");
  if (!NewChild->ParentWidget) Sys_Error("VWidget::AddChild: trying to adopt a child without any official papers");
  if (NewChild->ParentWidget != this) Sys_Error("VWidget::AddChild: trying to adopt an alien child");

  NewChild->PrevWidget = LastChildWidget;
  NewChild->NextWidget = nullptr;
  if (LastChildWidget) {
    LastChildWidget->NextWidget = NewChild;
  } else {
    FirstChildWidget = NewChild;
  }
  LastChildWidget = NewChild;
  OnChildAdded(NewChild);
  if (!CurrentFocusChild) SetCurrentFocusChild(NewChild);
  unguard;
}


//==========================================================================
//
//  VWidget::RemoveChild
//
//==========================================================================
void VWidget::RemoveChild (VWidget *InChild) {
  guard(VWidget::RemoveChild);
  if (!InChild) return;
  if (InChild->ParentWidget != this) Sys_Error("VWidget::AddChild: trying to orphan an alien child");
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
  InChild->PrevWidget = nullptr;
  InChild->NextWidget = nullptr;
  InChild->ParentWidget = nullptr;
  OnChildRemoved(InChild);
  if (CurrentFocusChild == InChild) FindNewFocus();
  unguard;
}


//==========================================================================
//
//  VWidget::DestroyAllChildren
//
//==========================================================================
void VWidget::DestroyAllChildren () {
  guard(VWidget::DestroyAllChildren);
  while (FirstChildWidget) FirstChildWidget->ConditionalDestroy();
  unguard;
}


//==========================================================================
//
//  VWidget::GetRootWidget
//
//==========================================================================
VRootWidget *VWidget::GetRootWidget () {
  guard(VWidget::GetRootWidget);
  VWidget *W = this;
  while (W->ParentWidget) W = W->ParentWidget;
  return (VRootWidget *)W;
  unguard;
}


//==========================================================================
//
//  VWidget::Lower
//
//==========================================================================
void VWidget::Lower () {
  guard(VWidget::Lower);
  if (!ParentWidget) Sys_Error("Can't lower root window");

  if (ParentWidget->FirstChildWidget == this) return; // already there

  // unlink from current location
  PrevWidget->NextWidget = NextWidget;
  if (NextWidget) {
    NextWidget->PrevWidget = PrevWidget;
  } else {
    ParentWidget->LastChildWidget = PrevWidget;
  }

  // link on bottom
  PrevWidget = nullptr;
  NextWidget = ParentWidget->FirstChildWidget;
  ParentWidget->FirstChildWidget->PrevWidget = this;
  ParentWidget->FirstChildWidget = this;
  unguard;
}


//==========================================================================
//
//  VWidget::Raise
//
//==========================================================================
void VWidget::Raise () {
  guard(VWidget::Raise);
  if (!ParentWidget) Sys_Error("Can't raise root window");

  if (ParentWidget->LastChildWidget == this) return; // already there

  // unlink from current location
  NextWidget->PrevWidget = PrevWidget;
  if (PrevWidget) {
    PrevWidget->NextWidget = NextWidget;
  } else {
    ParentWidget->FirstChildWidget = NextWidget;
  }

  // link on top
  PrevWidget = ParentWidget->LastChildWidget;
  NextWidget = nullptr;
  ParentWidget->LastChildWidget->NextWidget = this;
  ParentWidget->LastChildWidget = this;
  unguard;
}


//==========================================================================
//
//  VWidget::MoveBefore
//
//==========================================================================
void VWidget::MoveBefore (VWidget *Other) {
  guard(VWidget::MoveBefore);
  if (!Other) return;
  if (ParentWidget != Other->ParentWidget) Sys_Error("Must have the same parent widget");
  if (Other == this) Sys_Error("Can't move before self");

  if (Other->PrevWidget == this) return; // already there

  // unlink from current location
  if (PrevWidget) {
    PrevWidget->NextWidget = NextWidget;
  } else {
    ParentWidget->FirstChildWidget = NextWidget;
  }

  if (NextWidget) {
    NextWidget->PrevWidget = PrevWidget;
  } else {
    ParentWidget->LastChildWidget = PrevWidget;
  }

  // link in new position
  PrevWidget = Other->PrevWidget;
  NextWidget = Other;
  Other->PrevWidget = this;
  if (PrevWidget) {
    PrevWidget->NextWidget = this;
  } else {
    ParentWidget->FirstChildWidget = this;
  }
  unguard;
}


//==========================================================================
//
//  VWidget::MoveAfter
//
//==========================================================================
void VWidget::MoveAfter (VWidget *Other) {
  guard(VWidget::MoveAfter);
  if (!Other) return;
  if (ParentWidget != Other->ParentWidget) Sys_Error("Must have the same parent widget");
  if (Other == this) Sys_Error("Can't move after self");

  if (Other->NextWidget == this) return; // already there

  // unlink from current location
  if (PrevWidget) {
    PrevWidget->NextWidget = NextWidget;
  } else {
    ParentWidget->FirstChildWidget = NextWidget;
  }

  if (NextWidget) {
    NextWidget->PrevWidget = PrevWidget;
  } else {
    ParentWidget->LastChildWidget = PrevWidget;
  }

  // link in new position
  NextWidget = Other->NextWidget;
  PrevWidget = Other;
  Other->NextWidget = this;
  if (NextWidget) {
    NextWidget->PrevWidget = this;
  } else {
    ParentWidget->LastChildWidget = this;
  }
  unguard;
}


//==========================================================================
//
//  VWidget::ClipTree
//
//==========================================================================
void VWidget::ClipTree () {
  guard(VWidget::ClipTree);
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
  unguard;
}


//==========================================================================
//
//  VWidget::SetConfiguration
//
//==========================================================================
void VWidget::SetConfiguration (int NewX, int NewY, int NewWidth, int HewHeight, float NewScaleX, float NewScaleY) {
  guard(VWidget::SetConfiguration);
  PosX = NewX;
  PosY = NewY;
  //OfsX = OfsY = 0;
  SizeWidth = NewWidth;
  SizeHeight = HewHeight;
  SizeScaleX = NewScaleX;
  SizeScaleY = NewScaleY;
  ClipTree();
  OnConfigurationChanged();
  unguard;
}


//==========================================================================
//
//  VWidget::SetVisibility
//
//==========================================================================
void VWidget::SetVisibility (bool NewVisibility) {
  guard(VWidget::SetVisibility);
  if (!!(WidgetFlags&WF_IsVisible) != NewVisibility) {
    if (NewVisibility) {
      WidgetFlags |= WF_IsVisible;
      if (ParentWidget && !ParentWidget->CurrentFocusChild) ParentWidget->SetCurrentFocusChild(this);
    }
    else {
      WidgetFlags &= ~WF_IsVisible;
      if (ParentWidget && ParentWidget->CurrentFocusChild == this) ParentWidget->FindNewFocus();
    }
    OnVisibilityChanged(NewVisibility);
  }
  unguard;
}


//==========================================================================
//
//  VWidget::SetEnabled
//
//==========================================================================
void VWidget::SetEnabled (bool NewEnabled) {
  guard(VWidget::SetEnabled);
  if (!!(WidgetFlags&WF_IsEnabled) != NewEnabled) {
    if (NewEnabled) {
      WidgetFlags |= WF_IsEnabled;
      if (ParentWidget && !ParentWidget->CurrentFocusChild) ParentWidget->SetCurrentFocusChild(this);
    } else {
      WidgetFlags &= ~WF_IsEnabled;
      if (ParentWidget && ParentWidget->CurrentFocusChild == this) ParentWidget->FindNewFocus();
    }
    OnEnableChanged(NewEnabled);
  }
  unguard;
}


//==========================================================================
//
//  VWidget::SetFocusable
//
//==========================================================================
void VWidget::SetFocusable (bool NewFocusable) {
  guard(VWidget::SetFocusable);
  if (!!(WidgetFlags&WF_IsFocusable) != NewFocusable) {
    if (NewFocusable) {
      WidgetFlags |= WF_IsFocusable;
      if (ParentWidget && !ParentWidget->CurrentFocusChild) ParentWidget->SetCurrentFocusChild(this);
    } else {
      WidgetFlags &= ~WF_IsFocusable;
      if (ParentWidget && ParentWidget->CurrentFocusChild == this) ParentWidget->FindNewFocus();
    }
    OnFocusableChanged(NewFocusable);
  }
  unguard;
}


//==========================================================================
//
//  VWidget::SetCurrentFocusChild
//
//==========================================================================
void VWidget::SetCurrentFocusChild (VWidget *NewFocus) {
  guard(VWidget::SetCurrentFocusChild);
  // check f it's already focused
  if (CurrentFocusChild == NewFocus) return;

  // make sure it's visible, enabled and focusable
  if (NewFocus &&
      (!(NewFocus->WidgetFlags&WF_IsVisible) ||
       !(NewFocus->WidgetFlags&WF_IsEnabled) ||
       !(NewFocus->WidgetFlags&WF_IsFocusable)))
  {
    return;
  }

  // if we have a focused child, send focus lost event
  if (CurrentFocusChild) CurrentFocusChild->OnFocusLost();

  // make it the current focus
  CurrentFocusChild = NewFocus;
  if (CurrentFocusChild) CurrentFocusChild->OnFocusReceived();
  unguard;
}


//==========================================================================
//
//  VWidget::IsFocus
//
//==========================================================================
bool VWidget::IsFocus (bool Recurse) const {
  guard(VWidget::IsFocus);
  // root is always focused
  if (!ParentWidget) return true;
  if (Recurse) {
    const VWidget *W = this;
    while (W->ParentWidget && W->ParentWidget->CurrentFocusChild == W) W = W->ParentWidget;
    return !W->ParentWidget;
  } else {
    return (ParentWidget->CurrentFocusChild == this);
  }
  unguard;
}


//==========================================================================
//
//  VWidget::SetFocus
//
//==========================================================================
void VWidget::SetFocus () {
  guard(VWidget::SetFocus);
  if (ParentWidget) ParentWidget->SetCurrentFocusChild(this);
  unguard;
}


//==========================================================================
//
//  VWidget::FindNewFocus
//
//==========================================================================
void VWidget::FindNewFocus () {
  guard(VWidget::FindNewFocus);
  for (VWidget *W = CurrentFocusChild->NextWidget; W; W = W->NextWidget) {
    if ((W->WidgetFlags&WF_IsFocusable) &&
        (W->WidgetFlags&WF_IsVisible) &&
        (W->WidgetFlags&WF_IsEnabled))
    {
      SetCurrentFocusChild(W);
      return;
    }
  }

  for (VWidget *W = CurrentFocusChild->PrevWidget; W; W = W->PrevWidget) {
    if ((W->WidgetFlags&WF_IsFocusable) &&
        (W->WidgetFlags&WF_IsVisible) &&
        (W->WidgetFlags&WF_IsEnabled))
    {
      SetCurrentFocusChild(W);
      return;
    }
  }

  SetCurrentFocusChild(nullptr);
  unguard;
}


//==========================================================================
//
//  VWidget::GetWidgetAt
//
//==========================================================================
VWidget *VWidget::GetWidgetAt (float X, float Y) {
  guard(VWidget::GetWidgetAt);
  for (VWidget *W = LastChildWidget; W; W = W->PrevWidget) {
    if (!(W->WidgetFlags&WF_IsVisible)) continue;
    if (X >= W->ClipRect.ClipX1 && X < W->ClipRect.ClipX2 &&
        Y >= W->ClipRect.ClipY1 && Y < W->ClipRect.ClipY2)
    {
      return W->GetWidgetAt(X, Y);
    }
  }
  return this;
  unguard;
}


//==========================================================================
//
//  VWidget::DrawTree
//
//==========================================================================
void VWidget::DrawTree () {
  guard(VWidget::DrawTree);
  if (GetFlags()&_OF_Destroyed) return;
  if (!(WidgetFlags&WF_IsVisible) || !ClipRect.HasArea()) return; // not visible or clipped away

  // main draw event for this widget
  OnDraw();

  // draw chid widgets
  for (VWidget *c = FirstChildWidget; c; c = c->NextWidget) {
    if (c->GetFlags()&_OF_Destroyed) continue;
    c->DrawTree();
  }

  // do any drawing after child wigets have been drawn
  OnPostDraw();
  unguard;
}


//==========================================================================
//
//  VWidget::TickTree
//
//==========================================================================
void VWidget::TickTree (float DeltaTime) {
  guard(VWidget::TickTree);
  if (GetFlags()&_OF_Destroyed) return;
  if (WidgetFlags&WF_TickEnabled) Tick(DeltaTime);
  for (VWidget *c = FirstChildWidget; c; c = c->NextWidget) {
    if (c->GetFlags()&_OF_Destroyed) continue;
    c->TickTree(DeltaTime);
  }
  unguard;
}

//==========================================================================
//
//  VWidget::TransferAndClipRect
//
//==========================================================================
bool VWidget::TransferAndClipRect (float &X1, float &Y1, float &X2, float &Y2,
  float &S1, float &T1, float &S2, float &T2) const
{
  guard(VWidget::TransferAndClipRect);
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
  unguard;
}


//==========================================================================
//
//  VWidget::DrawPic
//
//==========================================================================
void VWidget::DrawPic (int X, int Y, int Handle, float Alpha, int Trans) {
  guard(VWidget::DrawPic);
  DrawPic(X, Y, GTextureManager(Handle), Alpha, Trans);
  unguard;
}


//==========================================================================
//
//  VWidget::DrawPic
//
//==========================================================================
void VWidget::DrawPic (int X, int Y, VTexture *Tex, float Alpha, int Trans) {
  guard(VWidget::DrawPic);
  if (!Tex) return;

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
  unguard;
}


//==========================================================================
//
//  VWidget::DrawShadowedPic
//
//==========================================================================
void VWidget::DrawShadowedPic (int X, int Y, int Handle) {
  guard(VWidget::DrawShadowedPic);
  DrawShadowedPic(X, Y, GTextureManager(Handle));
  unguard;
}


//==========================================================================
//
//  VWidget::DrawShadowedPic
//
//==========================================================================
void VWidget::DrawShadowedPic (int X, int Y, VTexture *Tex) {
  guard(VWidget::DrawShadowedPic);
  if (!Tex) return;

  float X1 = X-Tex->GetScaledSOffset()+2;
  float Y1 = Y-Tex->GetScaledTOffset()+2;
  float X2 = X-Tex->GetScaledSOffset()+2+Tex->GetScaledWidth();
  float Y2 = Y-Tex->GetScaledTOffset()+2+Tex->GetScaledHeight();
  float S1 = 0;
  float T1 = 0;
  float S2 = Tex->GetWidth();
  float T2 = Tex->GetHeight();
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->DrawPicShadow(X1, Y1, X2, Y2, S1, T1, S2, T2, Tex, 0.625);
  }

  DrawPic(X, Y, Tex);
  unguard;
}


//==========================================================================
//
//  VWidget::FillRectWithFlat
//
//==========================================================================
void VWidget::FillRectWithFlat (int X, int Y, int Width, int Height, VName Name) {
  guard(VWidget::FillRectWithFlat);
  if (Name == NAME_None) return;
  float X1 = X;
  float Y1 = Y;
  float X2 = X+Width;
  float Y2 = Y+Height;
  float S1 = 0;
  float T1 = 0;
  float S2 = Width;
  float T2 = Height;
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->FillRectWithFlat(X1, Y1, X2, Y2, S1, T1, S2, T2,
      GTextureManager(GTextureManager.NumForName(Name, TEXTYPE_Flat, true, true)));
  }
  unguard;
}


//==========================================================================
//
//  VWidget::FillRectWithFlatRepeat
//
//==========================================================================
void VWidget::FillRectWithFlatRepeat (int X, int Y, int Width, int Height, VName Name) {
  guard(VWidget::FillRectWithFlatRepeat);
  if (Name == NAME_None) return;
  float X1 = X;
  float Y1 = Y;
  float X2 = X+Width;
  float Y2 = Y+Height;
  float S1 = 0;
  float T1 = 0;
  float S2 = Width;
  float T2 = Height;
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->FillRectWithFlatRepeat(X1, Y1, X2, Y2, S1, T1, S2, T2,
      GTextureManager(GTextureManager.NumForName(Name, TEXTYPE_Flat, true, true)));
  }
  unguard;
}


//==========================================================================
//
//  VWidget::FillRect
//
//==========================================================================
void VWidget::FillRect (int X, int Y, int Width, int Height, int color) {
  guard(VWidget::FillRect);
  float X1 = X;
  float Y1 = Y;
  float X2 = X+Width;
  float Y2 = Y+Height;
  float S1 = 0;
  float T1 = 0;
  float S2 = Width;
  float T2 = Height;
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->FillRect(X1, Y1, X2, Y2, color);
  }
  unguard;
}


//==========================================================================
//
//  VWidget::ShadeRect
//
//==========================================================================
void VWidget::ShadeRect (int X, int Y, int Width, int Height, float Shade) {
  guard(VWidget::ShadeRect);
  float X1 = X;
  float Y1 = Y;
  float X2 = X+Width;
  float Y2 = Y+Height;
  float S1 = 0;
  float T1 = 0;
  float S2 = 0;
  float T2 = 0;
  if (TransferAndClipRect(X1, Y1, X2, Y2, S1, T1, S2, T2)) {
    Drawer->ShadeRect((int)X1, (int)Y1, (int)X2-(int)X1, (int)Y2-(int)Y1, Shade);
  }
  unguard;
}


//==========================================================================
//
//  VWidget::SetFont
//
//==========================================================================
void VWidget::SetFont (VFont *AFont) {
  guard(VWidget::SetFont);
  if (!AFont) Sys_Error("VWidget::SetFont: cannot set `nullptr` font");
  Font = AFont;
  unguard;
}


//==========================================================================
//
//  VWidget::SetFont
//
//==========================================================================
void VWidget::SetFont(VName FontName) {
  guard(VWidget::SetFont);
  VFont *F = VFont::GetFont(FontName, FontName);
  if (F) {
    Font = F;
  } else {
    if (!reportedMissingFonts.find(FontName)) {
      reportedMissingFonts.put(FontName, true);
      GCon->Logf("No such font '%s'", *FontName);
    }
  }
  unguard;
}


//==========================================================================
//
//  VWidget::SetTextAlign
//
//==========================================================================
void VWidget::SetTextAlign (halign_e NewHAlign, valign_e NewVAlign) {
  guard(VWidget::SetTextAlign);
  HAlign = NewHAlign;
  VAlign = NewVAlign;
  unguard;
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
void VWidget::DrawString (int x, int y, const VStr &String, int NormalColour, int BoldColour, float Alpha) {
  guard(VWidget::DrawNString);
  if (String.length() == 0) return;

  int cx = x;
  int cy = y;
  int Kerning = Font->GetKerning();
  int Colour = NormalColour;

  if (HAlign == hcentre) cx -= Font->StringWidth(String)/2;
  if (HAlign == hright) cx -= Font->StringWidth(String);

  bool oldflt = gl_pic_filtering;
  gl_pic_filtering = gl_font_filtering;

  for (const char *SPtr = *String; *SPtr; ) {
    int c = VStr::GetChar(SPtr);

    // check for colour escape.
    if (c == TEXT_COLOUR_ESCAPE) {
      Colour = VFont::ParseColourEscape(SPtr, NormalColour, BoldColour);
      continue;
    }

    int w;
    VTexture *Tex = Font->GetChar(c, &w, Colour);
    if (Tex) {
      //fprintf(stderr, "*CHAR: %c\n", (c >= 32 && c < 127 ? (char)c : '?'));
      if (WidgetFlags&WF_TextShadowed) DrawShadowedPic(cx, cy, Tex); else DrawPic(cx, cy, Tex, Alpha);
    }
    cx += w+Kerning;
  }

  gl_pic_filtering = oldflt;

  LastX = cx;
  LastY = cy;
  unguard;
}


//==========================================================================
//
//  VWidget::DrawText
//
//==========================================================================
void VWidget::DrawText (int x, int y, const VStr &String, int NormalColour, int BoldColour, float Alpha) {
  guard(VWidget::DrawText);
  int start = 0;
  int cx = x;
  int cy = y;

  if (VAlign == vcentre) cy -= Font->TextHeight(String)/2;
  if (VAlign == vbottom) cy -= Font->TextHeight(String);

  // need this for correct cursor position with empty strings
  LastX = cx;
  LastY = cy;

  for (int i = 0; i < String.length(); ++i) {
    if (String[i] == '\n') {
      VStr cs(String, start, i-start);
      DrawString(cx, cy, cs, NormalColour, BoldColour, Alpha);
      cy += Font->GetHeight();
      start = i+1;
    }
    if (i == String.length()-1) {
      DrawString(cx, cy, VStr(String, start, String.Length()-start), NormalColour, BoldColour, Alpha);
    }
  }
  unguard;
}


//==========================================================================
//
//  VWidget::DrawCursor
//
//==========================================================================
void VWidget::DrawCursor () {
  DrawCursorAt(LastX, LastY);
}


//==========================================================================
//
//  VWidget::DrawCursorAt
//
//==========================================================================
void VWidget::DrawCursorAt (int x, int y) {
  guard(VWidget::DrawCursorAt);
  if ((int)(host_time*4)&1) {
    int w;
    bool oldflt = gl_pic_filtering;
    gl_pic_filtering = gl_font_filtering;
    DrawPic(x, y, Font->GetChar('_', &w, CR_UNTRANSLATED));
    gl_pic_filtering = oldflt;
  }
  unguard;
}


//==========================================================================
//
//  Natives
//
//==========================================================================
IMPLEMENT_FUNCTION(VWidget, NewChild) {
  P_GET_PTR(VClass, ChildClass);
  P_GET_SELF;
  RET_REF(CreateNewWidget(ChildClass, Self));
}

IMPLEMENT_FUNCTION(VWidget, Destroy) {
  P_GET_SELF;
  //k8: don't delete it, GC will do
  //delete Self;
  //Self = nullptr;
  if (Self && (Self->GetFlags()&_OF_Destroyed) == 0) {
    Self->SetCleanupFlag();
    Self->Destroy();
  }
}

IMPLEMENT_FUNCTION(VWidget, MarkDead) {
  P_GET_SELF;
  if (Self && (Self->GetFlags()&_OF_Destroyed) == 0) {
    //Self->SetCleanupFlag();
    //Self->WidgetFlags |= WF_DeadManWalking;
    Self->MarkDead();
  }
}

IMPLEMENT_FUNCTION(VWidget, DestroyAllChildren) {
  P_GET_SELF;
  Self->DestroyAllChildren();
}

IMPLEMENT_FUNCTION(VWidget, GetRootWidget) {
  P_GET_SELF;
  RET_REF(Self->GetRootWidget());
}

IMPLEMENT_FUNCTION(VWidget, Lower) {
  P_GET_SELF;
  Self->Lower();
}

IMPLEMENT_FUNCTION(VWidget, Raise) {
  P_GET_SELF;
  Self->Raise();
}

IMPLEMENT_FUNCTION(VWidget, MoveBefore) {
  P_GET_REF(VWidget, Other);
  P_GET_SELF;
  Self->MoveBefore(Other);
}

IMPLEMENT_FUNCTION(VWidget, MoveAfter) {
  P_GET_REF(VWidget, Other);
  P_GET_SELF;
  Self->MoveAfter(Other);
}

IMPLEMENT_FUNCTION(VWidget, SetPos) {
  P_GET_INT(NewY);
  P_GET_INT(NewX);
  P_GET_SELF;
  Self->SetPos(NewX, NewY);
}

IMPLEMENT_FUNCTION(VWidget, SetX) {
  P_GET_INT(NewX);
  P_GET_SELF;
  Self->SetX(NewX);
}

IMPLEMENT_FUNCTION(VWidget, SetY) {
  P_GET_INT(NewY);
  P_GET_SELF;
  Self->SetY(NewY);
}

IMPLEMENT_FUNCTION(VWidget, SetOfsX) {
  P_GET_INT(NewX);
  P_GET_SELF;
  Self->SetOfsX(NewX);
}

IMPLEMENT_FUNCTION(VWidget, SetOfsY) {
  P_GET_INT(NewY);
  P_GET_SELF;
  Self->SetOfsY(NewY);
}

IMPLEMENT_FUNCTION(VWidget, SetSize) {
  P_GET_INT(NewHeight);
  P_GET_INT(NewWidth);
  P_GET_SELF;
  Self->SetSize(NewWidth, NewHeight);
}

IMPLEMENT_FUNCTION(VWidget, SetWidth) {
  P_GET_INT(NewWidth);
  P_GET_SELF;
  Self->SetWidth(NewWidth);
}

IMPLEMENT_FUNCTION(VWidget, SetHeight) {
  P_GET_INT(NewHeight);
  P_GET_SELF;
  Self->SetHeight(NewHeight);
}

IMPLEMENT_FUNCTION(VWidget, SetScale) {
  P_GET_FLOAT(NewScaleY);
  P_GET_FLOAT(NewScaleX);
  P_GET_SELF;
  Self->SetScale(NewScaleX, NewScaleY);
}

IMPLEMENT_FUNCTION(VWidget, SetConfiguration) {
  P_GET_FLOAT_OPT(NewScaleY, 1.0);
  P_GET_FLOAT_OPT(NewScaleX, 1.0);
  P_GET_INT(NewHeight);
  P_GET_INT(NewWidth);
  P_GET_INT(NewY);
  P_GET_INT(NewX);
  P_GET_SELF;
  Self->SetConfiguration(NewX, NewY, NewWidth, NewHeight, NewScaleX, NewScaleY);
}

IMPLEMENT_FUNCTION(VWidget, SetVisibility) {
  P_GET_BOOL(bNewVisibility);
  P_GET_SELF;
  Self->SetVisibility(bNewVisibility);
}

IMPLEMENT_FUNCTION(VWidget, Show) {
  P_GET_SELF;
  Self->Show();
}

IMPLEMENT_FUNCTION(VWidget, Hide) {
  P_GET_SELF;
  Self->Hide();
}

IMPLEMENT_FUNCTION(VWidget, IsVisible) {
  P_GET_BOOL_OPT(Recurse, true);
  P_GET_SELF;
  RET_BOOL(Self->IsVisible(Recurse));
}

IMPLEMENT_FUNCTION(VWidget, SetEnabled) {
  P_GET_BOOL(bNewEnabled);
  P_GET_SELF;
  Self->SetEnabled(bNewEnabled);
}

IMPLEMENT_FUNCTION(VWidget, Enable) {
  P_GET_SELF;
  Self->Enable();
}

IMPLEMENT_FUNCTION(VWidget, Disable) {
  P_GET_SELF;
  Self->Disable();
}

IMPLEMENT_FUNCTION(VWidget, IsEnabled) {
  P_GET_BOOL_OPT(Recurse, true);
  P_GET_SELF;
  RET_BOOL(Self->IsEnabled(Recurse));
}

IMPLEMENT_FUNCTION(VWidget, SetFocusable) {
  P_GET_BOOL(bNewFocusable);
  P_GET_SELF;
  Self->SetFocusable(bNewFocusable);
}

IMPLEMENT_FUNCTION(VWidget, IsFocusable) {
  P_GET_SELF;
  RET_BOOL(Self->IsFocusable());
}

IMPLEMENT_FUNCTION(VWidget, SetCurrentFocusChild) {
  P_GET_REF(VWidget, NewFocus);
  P_GET_SELF;
  Self->SetCurrentFocusChild(NewFocus);
}

IMPLEMENT_FUNCTION(VWidget, GetCurrentFocus) {
  P_GET_SELF;
  RET_REF(Self->GetCurrentFocus());
}

IMPLEMENT_FUNCTION(VWidget, IsFocus) {
  P_GET_BOOL_OPT(Recurse, true);
  P_GET_SELF;
  RET_BOOL(Self->IsFocus(Recurse));
}

IMPLEMENT_FUNCTION(VWidget, SetFocus) {
  P_GET_SELF;
  Self->SetFocus();
}

IMPLEMENT_FUNCTION(VWidget, DrawPic) {
  P_GET_INT_OPT(Translation, 0);
  P_GET_FLOAT_OPT(Alpha, 1.0);
  P_GET_INT(Handle);
  P_GET_INT(Y);
  P_GET_INT(X);
  P_GET_SELF;
  Self->DrawPic(X, Y, Handle, Alpha, Translation);
}

IMPLEMENT_FUNCTION(VWidget, DrawShadowedPic) {
  P_GET_INT(Handle);
  P_GET_INT(Y);
  P_GET_INT(X);
  P_GET_SELF;
  Self->DrawShadowedPic(X, Y, Handle);
}

IMPLEMENT_FUNCTION(VWidget, FillRectWithFlat) {
  P_GET_NAME(Name);
  P_GET_INT(Height);
  P_GET_INT(Width);
  P_GET_INT(Y);
  P_GET_INT(X);
  P_GET_SELF;
  Self->FillRectWithFlat(X, Y, Width, Height, Name);
}

IMPLEMENT_FUNCTION(VWidget, FillRectWithFlatRepeat) {
  P_GET_NAME(Name);
  P_GET_INT(Height);
  P_GET_INT(Width);
  P_GET_INT(Y);
  P_GET_INT(X);
  P_GET_SELF;
  Self->FillRectWithFlatRepeat(X, Y, Width, Height, Name);
}

IMPLEMENT_FUNCTION(VWidget, FillRect) {
  P_GET_INT(color);
  P_GET_INT(Height);
  P_GET_INT(Width);
  P_GET_INT(Y);
  P_GET_INT(X);
  P_GET_SELF;
  Self->FillRect(X, Y, Width, Height, color);
}

IMPLEMENT_FUNCTION(VWidget, ShadeRect) {
  P_GET_FLOAT(Shade);
  P_GET_INT(Height);
  P_GET_INT(Width);
  P_GET_INT(Y);
  P_GET_INT(X);
  P_GET_SELF;
  Self->ShadeRect(X, Y, Width, Height, Shade);
}

IMPLEMENT_FUNCTION(VWidget, SetFont) {
  P_GET_NAME(FontName);
  P_GET_SELF;
  Self->SetFont(FontName);
}

IMPLEMENT_FUNCTION(VWidget, SetTextAlign) {
  P_GET_INT(valign);
  P_GET_INT(halign);
  P_GET_SELF;
  Self->SetTextAlign((halign_e)halign, (valign_e)valign);
}

IMPLEMENT_FUNCTION(VWidget, SetTextShadow) {
  P_GET_BOOL(State);
  P_GET_SELF;
  Self->SetTextShadow(State);
}

IMPLEMENT_FUNCTION(VWidget, TextWidth) {
  P_GET_STR(text);
  P_GET_SELF;
  RET_INT(Self->Font->TextWidth(text));
}

IMPLEMENT_FUNCTION(VWidget, TextHeight) {
  P_GET_STR(text);
  P_GET_SELF;
  RET_INT(Self->Font->TextHeight(text));
}

IMPLEMENT_FUNCTION(VWidget, SplitText) {
  P_GET_BOOL_OPT(trimRight, true);
  P_GET_INT(MaxWidth);
  P_GET_PTR(TArray<VSplitLine>, Lines);
  P_GET_STR(Text);
  P_GET_SELF;
  RET_INT(Self->Font->SplitText(Text, *Lines, MaxWidth, trimRight));
}

IMPLEMENT_FUNCTION(VWidget, SplitTextWithNewlines) {
  P_GET_BOOL_OPT(trimRight, true);
  P_GET_INT(MaxWidth);
  P_GET_STR(Text);
  P_GET_SELF;
  RET_STR(Self->Font->SplitTextWithNewlines(Text, MaxWidth, trimRight));
}

IMPLEMENT_FUNCTION(VWidget, DrawText) {
  P_GET_FLOAT_OPT(Alpha, 1.0);
  P_GET_INT_OPT(BoldColour, CR_UNTRANSLATED);
  P_GET_INT_OPT(Colour, CR_UNTRANSLATED);
  P_GET_STR(String);
  P_GET_INT(Y);
  P_GET_INT(X);
  P_GET_SELF;
  Self->DrawText(X, Y, String, Colour, BoldColour, Alpha);
}

IMPLEMENT_FUNCTION(VWidget, DrawCursor) {
  P_GET_SELF;
  Self->DrawCursor();
}

IMPLEMENT_FUNCTION(VWidget, FindTextColour) {
  P_GET_STR(Name);
  RET_INT(VFont::FindTextColour(*Name.ToLower()));
}


IMPLEMENT_FUNCTION(VWidget, TranslateXY) {
  P_GET_PTR(float, py);
  P_GET_PTR(float, px);
  P_GET_SELF;
  if (px) *px = (Self->ClipRect.ScaleX*(*px)+Self->ClipRect.OriginX)/fScaleX;
  if (py) *py = (Self->ClipRect.ScaleY*(*py)+Self->ClipRect.OriginY)/fScaleY;
}
