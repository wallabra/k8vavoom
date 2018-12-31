//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************


// input device class, handles actual reading of the input
class VInputDevice : public VInterface {
public:
  // VInputDevice interface
  virtual void ReadInput() = 0;
  virtual void RegrabMouse () = 0; // called by UI when mouse cursor is turned off

  // implemented in corresponding system module
  static VInputDevice *CreateDevice ();
};


// input subsystem, handles all input events
class VInputPublic : public VInterface {
public:
  enum { MAX_KBCHEAT_LENGTH = 128 };

  struct CheatCode {
    VStr keys;
    VStr concmd;
  };

public:
  int ShiftDown;
  int CtrlDown;
  int AltDown;
  static TArray<CheatCode> kbcheats;
  static char currkbcheat[MAX_KBCHEAT_LENGTH+1];

  VInputPublic () : ShiftDown(0), CtrlDown(0), AltDown(0) { currkbcheat[0] = 0; }

  // system device related functions
  virtual void Init () = 0;
  virtual void Shutdown () = 0;

  // input event handling
  virtual bool PostEvent (const event_t &ev) = 0; // false: queue is full
  virtual void KeyEvent (int key, int press) = 0;
  virtual void ProcessEvents () = 0;
  virtual int ReadKey () = 0;

  // handling of key bindings
  virtual void GetBindingKeys (const VStr &Binding, int &Key1, int &Key2) = 0;
  virtual void GetBinding (int KeyNum, VStr &Down, VStr &Up) = 0;
  virtual void SetBinding (int KeyNum, const VStr &Down, const VStr &Up, bool Save=true) = 0;
  virtual void WriteBindings (FILE *f) = 0;

  virtual int TranslateKey (int ch) = 0;

  virtual int KeyNumForName (const VStr &Name) = 0;
  virtual VStr KeyNameForNum (int KeyNr) = 0;

  virtual void RegrabMouse () = 0; // called by UI when mouse cursor is turned off

  static void KBCheatClearAll ();
  static void KBCheatAppend (VStr keys, VStr concmd);
  static bool KBCheatProcessor (event_t *ev);

  static VInputPublic *Create ();
};


// global input handler
extern VInputPublic *GInput;
