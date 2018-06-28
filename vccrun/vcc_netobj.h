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


// ////////////////////////////////////////////////////////////////////////// //
class VNetConnection {};
class VClass;


// ////////////////////////////////////////////////////////////////////////// //
class VNetObjectsMap {
private:
  TArray<VName> NameLookup;
  TArray<int> NameMap;

  TArray<VClass *> ClassLookup;
  TMap<VClass *, vuint32> ClassMap;

public:
  VNetConnection *Connection;

public:
  VNetObjectsMap ();
  VNetObjectsMap (VNetConnection *AConnection);

  void SetUpClassLookup ();
  bool CanSerialiseObject (VObject *);
  bool SerialiseName (VStream &, VName &);
  bool SerialiseObject (VStream &, VObject *&);
  bool SerialiseClass (VStream &, VClass *&);
  bool SerialiseState (VStream &, VState *&);

  //friend class VObjectMapChannel;
};
