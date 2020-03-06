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


IMPLEMENT_CLASS(V, Widget);


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
}


//==========================================================================
//
//  VWidget::AddChild
//
//==========================================================================
void VWidget::AddChild (VWidget *NewChild) {
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
}


//==========================================================================
//
//  VWidget::RemoveChild
//
//==========================================================================
void VWidget::RemoveChild (VWidget *InChild) {
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
VRootWidget *VWidget::GetRootWidget () {
  VWidget *W = this;
  while (W->ParentWidget) W = W->ParentWidget;
  return (VRootWidget *)W;
}


//==========================================================================
//
//  VWidget::Lower
//
//==========================================================================
void VWidget::Lower () {
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
}


//==========================================================================
//
//  VWidget::Raise
//
//==========================================================================
void VWidget::Raise () {
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
}


//==========================================================================
//
//  VWidget::MoveBefore
//
//==========================================================================
void VWidget::MoveBefore (VWidget *Other) {
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
}


//==========================================================================
//
//  VWidget::MoveAfter
//
//==========================================================================
void VWidget::MoveAfter (VWidget *Other) {
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
}


//==========================================================================
//
//  VWidget::ClipTree
//
//==========================================================================
void VWidget::ClipTree () {
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
}


//==========================================================================
//
//  VWidget::SetEnabled
//
//==========================================================================
void VWidget::SetEnabled (bool NewEnabled) {
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
}


//==========================================================================
//
//  VWidget::SetFocusable
//
//==========================================================================
void VWidget::SetFocusable (bool NewFocusable) {
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
}


//==========================================================================
//
//  VWidget::SetCurrentFocusChild
//
//==========================================================================
void VWidget::SetCurrentFocusChild (VWidget *NewFocus) {
  // check if it's already focused
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
}


//==========================================================================
//
//  VWidget::IsFocus
//
//==========================================================================
bool VWidget::IsFocus (bool Recurse) const {
  // root is always focused
  if (!ParentWidget) return true;
  if (Recurse) {
    const VWidget *W = this;
    while (W->ParentWidget && W->ParentWidget->CurrentFocusChild == W) W = W->ParentWidget;
    return !W->ParentWidget;
  } else {
    return (ParentWidget->CurrentFocusChild == this);
  }
}


//==========================================================================
//
//  VWidget::SetFocus
//
//==========================================================================
void VWidget::SetFocus () {
  if (ParentWidget) ParentWidget->SetCurrentFocusChild(this);
}


//==========================================================================
//
//  VWidget::FindNewFocus
//
//==========================================================================
void VWidget::FindNewFocus () {
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
}


//==========================================================================
//
//  VWidget::GetWidgetAt
//
//==========================================================================
VWidget *VWidget::GetWidgetAt (float X, float Y) {
  for (VWidget *W = LastChildWidget; W; W = W->PrevWidget) {
    if (!(W->WidgetFlags&WF_IsVisible)) continue;
    if (X >= W->ClipRect.ClipX1 && X < W->ClipRect.ClipX2 &&
        Y >= W->ClipRect.ClipY1 && Y < W->ClipRect.ClipY2)
    {
      return W->GetWidgetAt(X, Y);
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
}


//==========================================================================
//
//  VWidget::TickTree
//
//==========================================================================
void VWidget::TickTree (float DeltaTime) {
  if (GetFlags()&_OF_Destroyed) return;
  if (WidgetFlags&WF_TickEnabled) Tick(DeltaTime);
  for (VWidget *c = FirstChildWidget; c; c = c->NextWidget) {
    if (c->GetFlags()&_OF_Destroyed) continue;
    c->TickTree(DeltaTime);
  }
}


//==========================================================================
//
//  VWidget::TransferAndClipRect
//
//==========================================================================
bool VWidget::TransferAndClipRect (float &X1, float &Y1, float &X2, float &Y2,
  float &S1, float &T1, float &S2, float &T2) const
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
    if (nn < 0) {
      // no flat: fill rect with gray color
      FillRect(X, Y, Width, Height, 0x222222, 1.0f);
      return;
    }
    //tex = GTextureManager.getIgnoreAnim(GTextureManager.NumForName(Name, TEXTYPE_Flat, true));
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
void VWidget::SetFont(VName FontName) {
  VFont *F = VFont::GetFont(FontName, FontName);
  if (F) {
    Font = F;
  } else {
    if (!reportedMissingFonts.find(FontName)) {
      reportedMissingFonts.put(FontName, true);
      GCon->Logf("No such font '%s'", *FontName);
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
  if (String.length() == 0) return;

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
  if ((int)(host_time*4)&1) {
    int w;
    bool oldflt = gl_pic_filtering;
    gl_pic_filtering = gl_font_filtering;
    DrawPic(x, y, Font->GetChar('_', &w, CR_UNTRANSLATED));
    gl_pic_filtering = oldflt;
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
  //delete Self;
  //Self = nullptr;
  if (Self && (Self->GetFlags()&_OF_Destroyed) == 0) {
    Self->SetCleanupFlag();
    Self->Destroy();
  }
}

IMPLEMENT_FUNCTION(VWidget, MarkDead) {
  vobjGetParamSelf();
  if (Self && (Self->GetFlags()&_OF_Destroyed) == 0) {
    //Self->SetCleanupFlag();
    //Self->WidgetFlags |= WF_DeadManWalking;
    Self->MarkDead();
  }
}

IMPLEMENT_FUNCTION(VWidget, DestroyAllChildren) {
  vobjGetParamSelf();
  if (Self) Self->DestroyAllChildren();
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

IMPLEMENT_FUNCTION(VWidget, IsFocus) {
  VOptParamBool Recurse(true);
  vobjGetParamSelf(Recurse);
  RET_BOOL(Self ? Self->IsFocus(Recurse) : false);
}

IMPLEMENT_FUNCTION(VWidget, SetFocus) {
  vobjGetParamSelf();
  if (Self) Self->SetFocus();
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

IMPLEMENT_FUNCTION(VWidget, FillRectWithFlatRepeat) {
  int X, Y, Width, Height;
  VName Name;
  vobjGetParamSelf(X, Y, Width, Height, Name);
  if (Self) Self->FillRectWithFlatRepeat(X, Y, Width, Height, Name);
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

IMPLEMENT_FUNCTION(VWidget, DrawCursor) {
  vobjGetParamSelf();
  if (Self) Self->DrawCursor();
}

IMPLEMENT_FUNCTION(VWidget, DrawCursorAt) {
  int cx, cy;
  vobjGetParamSelf(cx, cy);
  if (Self) Self->DrawCursorAt(cx, cy);
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
  if (px) *px = (Self->ClipRect.ScaleX*(*px)+Self->ClipRect.OriginX)/fScaleX;
  if (py) *py = (Self->ClipRect.ScaleY*(*py)+Self->ClipRect.OriginY)/fScaleY;
}
