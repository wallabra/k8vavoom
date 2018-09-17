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
//**
//**  Low-level OS-dependent functions
//**
//**************************************************************************


bool Sys_FileExists (const VStr &filename);
bool Sys_DirExists (const VStr &path);
int Sys_FileTime (const VStr &path); // returns -1 if not present
bool Sys_CreateDirectory (const VStr &path);

void Sys_FileDelete (const VStr &filename);

// can return `nullptr` for invalid path
void *Sys_OpenDir (const VStr &path); // nullptr: error
// never returns directories; returns empty string on end-of-dir; returns names w/o path
VStr Sys_ReadDir (void *adir);
void Sys_CloseDir (void *adir);

double Sys_Time ();
void Sys_Yield ();
