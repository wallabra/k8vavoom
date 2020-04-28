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
class VRootWidget : public VWidget {
  DECLARE_CLASS(VRootWidget, VWidget, 0)
  NO_DEFAULT_CONSTRUCTOR(VRootWidget)

private:
  struct SavedEventParts {
    int type; // 0: none; 1: x,y; 2: d/m
    int x, y, dx, dy, msx, msy;
  };

private:
  enum {
    // true if mouse cursor is currently enabled
    RWF_MouseEnabled = 1u<<0,
  };
  vuint32 RootFlags;

  // current mouse cursor position (screen coords)
  vint32 MouseX;
  vint32 MouseY;

  // current mouse cursor graphic
  vint32 MouseCursorPic;

private:
  // used in `InternalResponder()`
  // [0] is the topmost widget, [1] is child, and so on
  TArray<VWidget *> EventPath;

private:
  // this generates mouse leave/enter
  void MouseMoveEvent (const event_t *evt, int OldMouseX, int OldMouseY);

  void MouseClickEvent (const event_t *evt);

  // adds `lastOne` if it is not the last one already
  void BuildEventPath (VWidget *lastOne=nullptr) noexcept;

  // the path should be already built
  // returns `true` if any handler returned `true`
  // sets `eaten`  if any handler returned `true`
  bool DispatchEvent (event_t *evt);

  void UpdateMousePosition (int NewX, int NewY) noexcept;

  void FixEventCoords (VWidget *w, event_t *evt, SavedEventParts &svparts) noexcept;
  void RestoreEventCoords (event_t *evt, const SavedEventParts &svparts) noexcept;

  // this is called by the engine to dispatch the event
  bool InternalResponder (event_t *evt);

public:
  void Init ();
  virtual void Init (VWidget *) override { Sys_Error("Root cannot have a parent"); }

  void DrawWidgets ();
  void TickWidgets (float DeltaTime);

  // this is called by the engine to dispatch the event
  bool Responder (event_t *evt);

  void SetMouse (bool MouseOn);
  inline bool IsMouseAllowed () const noexcept { return !!(RootFlags&RWF_MouseEnabled); }

  void RefreshScale ();

  VWidget *GetWidgetAtScreenXY (int x, int y) noexcept;

  static void StaticInit ();

  DECLARE_FUNCTION(SetMouse)
};

extern VRootWidget *GRoot;
