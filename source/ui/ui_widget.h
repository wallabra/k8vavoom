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

struct VClipRect {
  // origin of the widget, in absolute coordinates
  float OriginX;
  float OriginY;

  // accumulative scale
  float ScaleX;
  float ScaleY;

  // clipping rectangle, in absolute coordinates
  float ClipX1;
  float ClipY1;
  float ClipX2;
  float ClipY2;

  inline bool HasArea () const { return (ClipX1 < ClipX2 && ClipY1 < ClipY2); }
};


class VWidget : public VObject {
  DECLARE_CLASS(VWidget, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VWidget)

private:
  // parent container widget
  VWidget *ParentWidget;
  // linked list of child widgets
  VWidget *FirstChildWidget;
  VWidget *LastChildWidget;
  // links in the linked list of widgets
  VWidget *PrevWidget;
  VWidget *NextWidget;

  // position of the widget in the parent widget
  int PosX;
  int PosY;
  // offset for children
  int OfsX;
  int OfsY;
  // size of the child area of the widget
  int SizeWidth;
  int SizeHeight;
  // scaling of the widget
  float SizeScaleX;
  float SizeScaleY;

  VClipRect ClipRect;

  // currently focused child widget
  VWidget *CurrentFocusChild;

  VFont *Font;

  // text alignements
  vuint8 HAlign;
  vuint8 VAlign;

  // text cursor
  int LastX;
  int LastY;

  // Booleans
  enum {
    // is this widget visible?
    WF_IsVisible    = 0x0001,
    // a flag that enables or disables Tick event
    WF_TickEnabled  = 0x0002,
    // is this widget enabled and can receive input
    WF_IsEnabled    = 0x0004,
    // can this widget be focused?
    WF_IsFocusable  = 0x0008,
    // mouse button state for click events
    WF_LMouseDown   = 0x0010,
    WF_MMouseDown   = 0x0020,
    WF_RMouseDown   = 0x0040,
    // shadowed text
    WF_TextShadowed = 0x0080,
    // marked as dead
    WF_DeadManWalking = 0x4000,
    WF_NeedCleanup = 0x8000,
  };
  vuint32 WidgetFlags;

  VObjectDelegate FocusLost;
  VObjectDelegate FocusReceived;

  VObjectDelegate KeyDown;
  VObjectDelegate KeyUp;

  void AddChild (VWidget *);
  void RemoveChild (VWidget *);

  void ClipTree ();
  void DrawTree ();
  void TickTree (float DeltaTime);

  void FindNewFocus ();

  VWidget *GetWidgetAt (float, float);

  bool TransferAndClipRect (float&, float&, float&, float&, float&, float&, float&, float&) const;
  void DrawString (int, int, const VStr&, int, int, float);

  void cleanupWidgets ();

  inline void SetCleanupFlag () { for (VWidget *w = this; w; w = w->ParentWidget) w->WidgetFlags |= WF_NeedCleanup; }
  inline bool IsNeedCleanup () const { return ((WidgetFlags&WF_NeedCleanup) != 0); }

  void MarkDead ();

  friend class VRootWidget;

public:
  // destroys all child widgets
  virtual void Init (VWidget *);
  virtual void Destroy () override;

  inline bool IsDeadManWalking () const { return ((WidgetFlags&WF_DeadManWalking) != 0); }

  void DestroyAllChildren ();

  VRootWidget *GetRootWidget();

  // methods to move widget on top or bottom
  void Lower ();
  void Raise ();
  void MoveBefore (VWidget *);
  void MoveAfter (VWidget *);

  // methods to set position, size and scale
  inline void SetPos (int NewX, int NewY) { SetConfiguration(NewX, NewY, SizeWidth, SizeHeight, SizeScaleX, SizeScaleY); }
  inline void SetX (int NewX) { SetPos(NewX, PosY); }
  inline void SetY (int NewY) { SetPos(PosX, NewY); }
  inline void SetOfsX (int NewX) { if (OfsX != NewX) { OfsX = NewX; SetConfiguration(PosX, PosY, SizeWidth, SizeHeight, SizeScaleX, SizeScaleY); } }
  inline void SetOfsY (int NewY) { if (OfsY != NewY) { OfsY = NewY; SetConfiguration(PosX, PosY, SizeWidth, SizeHeight, SizeScaleX, SizeScaleY); } }
  inline void SetSize (int NewWidth, int NewHeight) { SetConfiguration(PosX, PosY, NewWidth, NewHeight, SizeScaleX, SizeScaleY); }
  inline void SetWidth (int NewWidth) { SetSize(NewWidth, SizeHeight); }
  inline void SetHeight (int NewHeight) { SetSize(SizeWidth, NewHeight); }
  inline void SetScale (float NewScaleX, float NewScaleY) { SetConfiguration(PosX, PosY, SizeWidth, SizeHeight, NewScaleX, NewScaleY); }
  void SetConfiguration(int, int, int, int, float = 1.0f, float = 1.0f);

  // visibility methods
  void SetVisibility (bool);
  inline void Show () { SetVisibility(true); }
  inline void Hide () { SetVisibility(false); }

  inline bool IsVisible (bool bRecurse = true) const {
    if (bRecurse) {
      const VWidget *pParent = this;
      while (pParent) {
        if (!(pParent->WidgetFlags&WF_IsVisible)) break;
        pParent = pParent->ParentWidget;
      }
      return (pParent ? false : true);
    } else {
      return !!(WidgetFlags&WF_IsVisible);
    }
  }

  // enable state methods
  void SetEnabled (bool);
  inline void Enable () { SetEnabled(true); }
  inline void Disable () { SetEnabled(false); }

  inline bool IsEnabled (bool bRecurse = true) const {
    if (bRecurse) {
      const VWidget *pParent = this;
      while (pParent) {
        if (!(pParent->WidgetFlags&WF_IsEnabled)) break;
        pParent = pParent->ParentWidget;
      }
      return (pParent ? false : true);
    } else {
      return !!(WidgetFlags&WF_IsEnabled);
    }
  }

  // focusable state methods
  void SetFocusable (bool);
  inline bool IsFocusable () const { return !!(WidgetFlags&WF_IsFocusable); }

  // focus methods
  void SetCurrentFocusChild (VWidget *);
  inline VWidget *GetCurrentFocus () const { return CurrentFocusChild; }
  bool IsFocus (bool Recurse = true) const;
  void SetFocus ();

  void DrawPic (int, int, int, float = 1.0f, int = 0);
  void DrawPicScaled (int X, int Y, int Handle, float scaleX, float scaleY, float Alpha=1.0f, int Trans=0);
  void DrawPicScaled (int X, int Y, VTexture *Tex, float scaleX, float scaleY, float Alpha=1.0f, int Trans=0);
  void DrawPic (int, int, VTexture *, float = 1.0f, int = 0);
  void DrawShadowedPic (int, int, int);
  void DrawShadowedPic (int, int, VTexture *);
  void FillRectWithFlat (int, int, int, int, VName);
  void FillRectWithFlatRepeat (int, int, int, int, VName);
  void FillRect (int, int, int, int, int);
  void ShadeRect (int, int, int, int, float);

  void SetFont (VFont*);
  void SetFont (VName);
  void SetTextAlign (halign_e, valign_e);
  void SetTextShadow (bool);
  void DrawText (int, int, const VStr &, int, int, float);
  void DrawCursor ();
  void DrawCursorAt (int, int);

  static VWidget *CreateNewWidget (VClass *, VWidget *);

  // events
  void OnCreate () { static VMethodProxy method("OnCreate"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void OnDestroy () { static VMethodProxy method("OnDestroy"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  virtual void OnChildAdded (VWidget *Child) { static VMethodProxy method("OnChildAdded"); vobjPutParamSelf(Child); VMT_RET_VOID(method); }
  virtual void OnChildRemoved (VWidget *Child) { static VMethodProxy method("OnChildRemoved"); vobjPutParamSelf(Child); VMT_RET_VOID(method); }
  virtual void OnConfigurationChanged () { static VMethodProxy method("OnConfigurationChanged"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  virtual void OnVisibilityChanged (bool NewVisibility) { static VMethodProxy method("OnVisibilityChanged"); vobjPutParamSelf(NewVisibility); VMT_RET_VOID(method); }
  virtual void OnEnableChanged (bool bNewEnable) { static VMethodProxy method("OnEnableChanged"); vobjPutParamSelf(bNewEnable); VMT_RET_VOID(method); }
  virtual void OnFocusableChanged (bool bNewFocusable) { static VMethodProxy method("OnFocusableChanged"); vobjPutParamSelf(bNewFocusable); VMT_RET_VOID(method); }
  virtual void OnFocusReceived () { static VMethodProxy method("OnFocusReceived"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  virtual void OnFocusLost () { static VMethodProxy method("OnFocusLost"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  virtual void OnDraw () { static VMethodProxy method("OnDraw"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  virtual void OnPostDraw () { static VMethodProxy method("OnPostDraw"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  virtual void Tick (float DeltaTime) { if (DeltaTime <= 0.0f) return; static VMethodProxy method("Tick"); vobjPutParamSelf(DeltaTime); VMT_RET_VOID(method); }
  virtual bool OnKeyDown (int Key) { static VMethodProxy method("OnKeyDown"); vobjPutParamSelf(Key); VMT_RET_BOOL(method); }
  virtual bool OnKeyUp (int Key) { static VMethodProxy method("OnKeyUp"); vobjPutParamSelf(Key); VMT_RET_BOOL(method); }
  virtual bool OnMouseMove (int OldX, int OldY, int NewX, int NewY) { static VMethodProxy method("OnMouseMove"); vobjPutParamSelf(OldX, OldY, NewX, NewY); VMT_RET_BOOL(method); }
  virtual void OnMouseEnter () { static VMethodProxy method("OnMouseEnter"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  virtual void OnMouseLeave () { static VMethodProxy method("OnMouseLeave"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  virtual bool OnMouseDown (int X, int Y, int Button) { static VMethodProxy method("OnMouseDown"); vobjPutParamSelf(X, Y, Button); VMT_RET_BOOL(method); }
  virtual bool OnMouseUp (int X, int Y, int Button) { static VMethodProxy method("OnMouseUp"); vobjPutParamSelf(X, Y, Button); VMT_RET_BOOL(method); }
  virtual void OnMouseClick (int X, int Y) { static VMethodProxy method("OnMouseClick"); vobjPutParamSelf(X, Y); VMT_RET_VOID(method); }
  virtual void OnMMouseClick (int X, int Y) { static VMethodProxy method("OnMMouseClick"); vobjPutParamSelf(X, Y); VMT_RET_VOID(method); }
  virtual void OnRMouseClick (int X, int Y) { static VMethodProxy method("OnRMouseClick"); vobjPutParamSelf(X, Y); VMT_RET_VOID(method); }

  // script natives
  DECLARE_FUNCTION(NewChild)
  DECLARE_FUNCTION(Destroy)
  DECLARE_FUNCTION(MarkDead)
  DECLARE_FUNCTION(DestroyAllChildren)

  DECLARE_FUNCTION(GetRootWidget)

  DECLARE_FUNCTION(Raise)
  DECLARE_FUNCTION(Lower)
  DECLARE_FUNCTION(MoveBefore)
  DECLARE_FUNCTION(MoveAfter)

  DECLARE_FUNCTION(SetPos)
  DECLARE_FUNCTION(SetX)
  DECLARE_FUNCTION(SetY)
  DECLARE_FUNCTION(SetOfsX)
  DECLARE_FUNCTION(SetOfsY)
  DECLARE_FUNCTION(SetSize)
  DECLARE_FUNCTION(SetWidth)
  DECLARE_FUNCTION(SetHeight)
  DECLARE_FUNCTION(SetScale)
  DECLARE_FUNCTION(SetConfiguration)

  DECLARE_FUNCTION(SetVisibility)
  DECLARE_FUNCTION(Show)
  DECLARE_FUNCTION(Hide)
  DECLARE_FUNCTION(IsVisible)

  DECLARE_FUNCTION(SetEnabled)
  DECLARE_FUNCTION(Enable)
  DECLARE_FUNCTION(Disable)
  DECLARE_FUNCTION(IsEnabled)

  DECLARE_FUNCTION(SetFocusable)
  DECLARE_FUNCTION(IsFocusable)

  DECLARE_FUNCTION(SetCurrentFocusChild)
  DECLARE_FUNCTION(GetCurrentFocus)
  DECLARE_FUNCTION(IsFocus)
  DECLARE_FUNCTION(SetFocus)

  DECLARE_FUNCTION(DrawPic)
  DECLARE_FUNCTION(DrawPicScaled)
  DECLARE_FUNCTION(DrawShadowedPic)
  DECLARE_FUNCTION(FillRectWithFlat)
  DECLARE_FUNCTION(FillRectWithFlatRepeat)
  DECLARE_FUNCTION(FillRect)
  DECLARE_FUNCTION(ShadeRect)

  DECLARE_FUNCTION(SetFont)
  DECLARE_FUNCTION(SetTextAlign)
  DECLARE_FUNCTION(SetTextShadow)
  DECLARE_FUNCTION(TextWidth)
  DECLARE_FUNCTION(TextHeight)
  DECLARE_FUNCTION(FontHeight)
  DECLARE_FUNCTION(SplitText)
  DECLARE_FUNCTION(SplitTextWithNewlines)
  DECLARE_FUNCTION(DrawText)
  DECLARE_FUNCTION(DrawCursor)
  DECLARE_FUNCTION(FindTextColor)

  DECLARE_FUNCTION(TranslateXY)
};
