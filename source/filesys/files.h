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


void FL_Init ();
void FL_Shutdown ();

bool FL_FileExists (const VStr &fname);
void FL_CreatePath (const VStr &Path);

VStream *FL_OpenFileRead (const VStr &Name);
// set `isFullName` to `true` to prevent adding anything to file name
VStream *FL_OpenFileWrite (const VStr &Name, bool isFullName=false);

VStream *FL_OpenSysFileRead (const VStr &Name);
VStream *FL_OpenSysFileWrite (const VStr &Name);

TArray<VStr> GetWadPk3List (bool fullpath=false);


//extern bool fl_devmode;
extern VStr fl_basedir;
extern VStr fl_savedir;
extern VStr fl_gamedir;
