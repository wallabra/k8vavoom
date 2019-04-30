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

#include "../libs/core/core.h"

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

#include "convars.h"
#include "filesys/fsys.h"


#include "../source/common.h"
#include "vcc_netobj.h"
#include "../source/vc/vc_location.h"
#include "../source/vc/vc_type.h"
#include "../source/vc/vc_member.h"
#include "../source/progdefs.h"
#include "../source/progs.h"
#include "../source/vc/vc_field.h"
#include "../source/vc/vc_property.h"
#include "../source/vc/vc_method.h"
#include "../source/vc/vc_constant.h"
#include "../source/vc/vc_struct.h"
#include "../source/vc/vc_state.h"
#include "../source/vc/vc_class.h"
#include "../source/vc/vc_package.h"
#include "../source/vc/vc_emit_context.h"
#include "../source/vc/vc_expr_base.h"
#include "../source/vc/vc_expr_literal.h"
#include "../source/vc/vc_expr_unary_binary.h"
#include "../source/vc/vc_expr_cast.h"
#include "../source/vc/vc_expr_type.h"
#include "../source/vc/vc_expr_field.h"
#include "../source/vc/vc_expr_array.h"
#include "../source/vc/vc_expr_invoke.h"
#include "../source/vc/vc_expr_assign.h"
#include "../source/vc/vc_expr_local.h"
#include "../source/vc/vc_expr_misc.h"
#include "../source/vc/vc_statement.h"
#include "../source/vc/vc_error.h"
#include "../source/vc/vc_lexer.h"
#include "../source/vc/vc_modifiers.h"
#include "../source/vc/vc_parser.h"
#include "../source/vc/vc_object.h"
#include "../source/vc/vc_zastar.h"

#include "../source/scripts.h"

#include "../source/misc.h"


extern int dprintf (const char *text, ...) __attribute__((format(printf, 1, 2)));
extern char *va (const char *text, ...) __attribute__((format(printf, 1, 2)));

void Host_Error (const char *error, ...) __attribute__((noreturn, format(printf, 1, 2)));

//extern VStream *OpenFile (const VStr &Name);


#endif
