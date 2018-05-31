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

/*
bool Sys_FileExists(const VStr&);
int Sys_FileTime(const VStr&);

bool Sys_CreateDirectory(const VStr&);
bool Sys_OpenDir(const VStr&);
VStr Sys_ReadDir();
void Sys_CloseDir();
bool Sys_DirExists(const VStr&);

double Sys_Time();
void Sys_Yield();
*/

void __attribute__((noreturn)) __declspec(noreturn) Sys_Quit(const char*);
void Sys_Shutdown();

char *Sys_ConsoleInput();
