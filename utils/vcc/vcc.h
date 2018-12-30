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
#ifndef VCC_HEADER_FILE
#define VCC_HEADER_FILE


#define OPCODE_STATS

#include "../../libs/core/core.h"
#include "../../source/common.h"
#include "../../source/vc/vc_location.h"
#include "../../source/vc/vc_type.h"
#include "../../source/vc/vc_member.h"
#include "../../source/progdefs.h"
#include "../../source/vc/vc_field.h"
#include "../../source/vc/vc_property.h"
#include "../../source/vc/vc_method.h"
#include "../../source/vc/vc_constant.h"
#include "../../source/vc/vc_struct.h"
#include "../../source/vc/vc_state.h"
#include "../../source/vc/vc_class.h"
#include "../../source/vc/vc_package.h"
#include "../../source/vc/vc_emit_context.h"
#include "../../source/vc/vc_expr_base.h"
#include "../../source/vc/vc_expr_literal.h"
#include "../../source/vc/vc_expr_unary_binary.h"
#include "../../source/vc/vc_expr_cast.h"
#include "../../source/vc/vc_expr_type.h"
#include "../../source/vc/vc_expr_field.h"
#include "../../source/vc/vc_expr_array.h"
#include "../../source/vc/vc_expr_invoke.h"
#include "../../source/vc/vc_expr_assign.h"
#include "../../source/vc/vc_expr_local.h"
#include "../../source/vc/vc_expr_misc.h"
#include "../../source/vc/vc_statement.h"
#include "../../source/vc/vc_error.h"
#include "../../source/vc/vc_lexer.h"
#include "../../source/vc/vc_modifiers.h"
#include "../../source/vc/vc_parser.h"


extern int dprintf (const char *text, ...) __attribute__((format(printf, 1, 2)));
extern char *va (const char *text, ...) __attribute__((format(printf, 1, 2)));

extern VStream *OpenFile (const VStr &Name);

#define fsysOpenFile  OpenFile


#endif
