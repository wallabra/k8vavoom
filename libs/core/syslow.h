//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
//**  Copyright (C) 2018-2021 Ketmar Dark
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
//**
//**  Low-level OS-dependent functions
//**
//**************************************************************************


// takes case-insensitive path, traverses it, and rewrites it to the case-sensetive one
// (using real on-disk names). returns fixed path.
// if some dir or file wasn't found, returns empty string.
// last name assumed to be a file, not directory (unless `lastIsDir` flag is set).
VStr Sys_FindFileCI (VStr path, bool lastIsDir=false);

bool Sys_FileExists (VStr filename);
bool Sys_DirExists (VStr path);
int Sys_FileTime (VStr path); // returns -1 if not present, or time in unix epoch
bool Sys_Touch (VStr path);
int Sys_CurrFileTime (); // return current time in unix epoch
bool Sys_CreateDirectory (VStr path);

void Sys_FileDelete (VStr filename);

// can return `nullptr` for invalid path
void *Sys_OpenDir (VStr path, bool wantDirs=false); // nullptr: error
// returns empty string on end-of-dir; returns names w/o path
// if `wantDirs` is true, dir names are ends with "/"; never returns "." and ".."
VStr Sys_ReadDir (void *adir);
void Sys_CloseDir (void *adir);

double Sys_Time_CPU (); // this tries to return CPU time used by the process; never returns 0
double Sys_Time (); // never returns 0
void Sys_Yield ();

// has any meaning only for shitdoze, returns 0 otherwise
int Sys_TimeMinPeriodMS ();
int Sys_TimeMaxPeriodMS ();

int Sys_GetCPUCount ();

// get time (start point is arbitrary) in nanoseconds
// never returns 0
vuint64 Sys_GetTimeNano ();
vuint64 Sys_GetTimeCPUNano (); // this tries to return CPU time used by the process; never returns 0

/* not used for now
#define Sys_EmptyTID  ((vuint32)0xffffffffu)

// get unique ID of the current thread
// let's hope that 32 bits are enough for thread ids on all OSes, lol
vuint32 Sys_GetCurrentTID ();
*/

// returns system user name suitable for using as player name
// never returns empty string
VStr Sys_GetUserName ();
