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
#ifndef VCCRUN_HEADER_FILE
#define VCCRUN_HEADER_FILE

#include <stdio.h>

//#include "../libs/core/core.h"

//#define Random()  ((float)(rand()&0x7fff)/(float)0x8000)
/*
float Random () {
  unsigned int rn;
  ed25519_randombytes(&rn, sizeof(rn));
  fprintf(stderr, "rn=0x%08x\n", rn);
  return (rn&0x3ffff)/(float)0x3ffff;
}
*/


//#define OPCODE_STATS

//#include "convars.h"
//#include "filesys/fsys.h"

//#include "vcc_netobj.h"
#include "vcc_run_vc.h"
#include "../source/vc/vc_public.h"
#include "../source/scripts.h"
//#include "../source/misc.h"


//extern VStream *OpenFile (const VStr &Name);


#endif
