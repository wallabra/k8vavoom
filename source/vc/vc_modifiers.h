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

class TModifiers {
public:
  enum {
    Native    = 0x000001,
    Static    = 0x000002,
    Abstract  = 0x000004,
    Private   = 0x000008,
    ReadOnly  = 0x000010,
    Transient = 0x000020,
    Final     = 0x000040,
    Optional  = 0x000080,
    Out       = 0x000100,
    Spawner   = 0x000200,
    Override  = 0x000400,
    Ref       = 0x000800,
    Protected = 0x001000,
    Const     = 0x002000,
    Repnotify = 0x004000,
    Scope     = 0x008000,
    Internal  = 0x010000,
  };

  static int Parse (VLexer &);
  static const char *Name (int);
  static int Check (int, int, const TLocation &);
  static int MethodAttr (int);
  static int ClassAttr (int);
  static int FieldAttr (int);
  static int PropAttr (int);
  static int ParmAttr (int);
};
