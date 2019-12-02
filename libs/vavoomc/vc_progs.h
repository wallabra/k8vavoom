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

class VFieldType;

extern volatile unsigned vmAbortBySignal;

extern void PR_Init ();
extern void PR_OnAbort ();
extern VStr PF_FormatString ();

extern void PR_WriteOne (const VFieldType &type);
extern void PR_WriteFlush ();

// if `buf` is `nullptr`, it means "flush"
extern void (*PR_WriterCB) (const char *buf, bool debugPrint, VName wrname);

// calls `PR_WriterCB` if it is not empty, or does default printing
// if `buf` is `nullptr`, it means "flush"
extern void PR_DoWriteBuf (const char *buf, bool debugPrint=false, VName wrname=NAME_None);
