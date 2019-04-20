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
    Published = 0x020000,
  };

  static int Parse (VLexer &Lex);
  static const char *Name (int Modifier);
  static int Check (int Modifers, int Allowed, const TLocation &l);
  static int MethodAttr (int Modifers);
  static int ClassAttr (int Modifers);
  static int FieldAttr (int Modifers);
  static int PropAttr (int Modifers);
  static int ParmAttr (int Modifers);
};
