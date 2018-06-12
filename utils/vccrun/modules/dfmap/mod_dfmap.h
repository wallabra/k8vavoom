//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2018 Ketmar Dark
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
#ifndef VCCMOD_DFMAP_HEADER_FILE
#define VCCMOD_DFMAP_HEADER_FILE

#include "../../vcc_run.h"
#include "dfmap.h"


// ////////////////////////////////////////////////////////////////////////// //
class VDFMap : public VObject {
  DECLARE_CLASS(VDFMap, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VDFMap)

public:
  Header *header;
  Texture **textures;
  int textureCount;
  Panel **panels;
  int panelCount;
  Item **items;
  int itemCount;
  Monster **monsters;
  int monsterCount;
  Area **areas;
  int areaCount;
  Trigger **triggers;
  int triggerCount;

private:
  void initialize ();
  bool loadFrom (VStream &strm);

public:
  virtual void Destroy () override;

  bool load (const VStr &fname);
  void clear ();

  DECLARE_FUNCTION(Load) // static
};


#endif
