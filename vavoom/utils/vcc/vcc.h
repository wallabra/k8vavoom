//**************************************************************************
//**
//**	##   ##    ##    ##   ##   ####     ####   ###     ###
//**	##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**	 ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**	 ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**	  ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**	   #    ##    ##    #      ####     ####   ##       ##
//**
//**	$Id$
//**
//**	Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**	This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**	This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

#ifndef __VCC_H__
#define __VCC_H__

// HEADER FILES ------------------------------------------------------------

//	C headers
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#include "../../source/common.h"

void* Malloc(size_t size);
void Free(void* ptr);

//	For compatibility with engine files.
#define Z_Malloc	Malloc
#define Z_Free		Free
#define Sys_Error	FatalError

#include "../../source/array.h"
#include "../../source/stream.h"
#include "../../source/names.h"
#include "../../source/name.h"
#include "../../source/str.h"
#include "../../source/vector.h"
#include "../../source/text.h"
#include "../../source/vc_location.h"
#include "../../source/vc_type.h"
#include "../../source/vc_member.h"
#include "../../source/progdefs.h"
#include "../../source/vc_field.h"
#include "../../source/vc_property.h"
typedef void (*builtin_t)();
#include "../../source/vc_method.h"
#include "../../source/vc_constant.h"
#include "../../source/vc_struct.h"
#include "../../source/vc_state.h"
#include "../../source/vc_class.h"
#include "../../source/vc_package.h"
#include "../../source/vc_emit_context.h"
#include "../../source/vc_expr_base.h"
#include "../../source/vc_expr_literal.h"
#include "../../source/vc_expr_unary_binary.h"
#include "../../source/vc_expr_cast.h"
#include "../../source/vc_expr_type.h"
#include "../../source/vc_expr_field.h"
#include "../../source/vc_expr_array.h"
#include "../../source/vc_expr_invoke.h"
#include "../../source/vc_expr_assign.h"
#include "../../source/vc_expr_local.h"
#include "../../source/vc_expr_misc.h"
#include "../../source/vc_statement.h"
#include "../../source/vc_error.h"
#include "../../source/vc_lexer.h"

// MACROS ------------------------------------------------------------------

#define OPCODE_STATS

// TYPES -------------------------------------------------------------------

class TModifiers
{
public:
	enum
	{
		Native			= 0x0001,
		Static			= 0x0002,
		Abstract		= 0x0004,
		Private			= 0x0008,
		ReadOnly		= 0x0010,
		Transient		= 0x0020,
		Final			= 0x0040,
		Optional		= 0x0080,
		Out				= 0x0100,
		Spawner			= 0x0200,
	};

	static int Parse(VLexer&);
	static const char* Name(int);
	static int Check(int, int, TLocation);
	static int MethodAttr(int);
	static int ClassAttr(int);
	static int FieldAttr(int);
	static int PropAttr(int);
	static int ParmAttr(int);
};

class VClass;
class VStruct;
class VMethod;
class VEmitContext;
class VPackage;

class VParser
{
private:
	VLexer&			Lex;
	VPackage*		Package;
	bool			CheckForLocal;

	VExpression* ParseDotMethodCall(VExpression*, VName, TLocation);
	VExpression* ParseBaseMethodCall(VName, TLocation);
	VExpression* ParseMethodCallOrCast(VName, TLocation);
	VLocalDecl* ParseLocalVar(VExpression* TypeExpr);
	VExpression* ParseExpressionPriority0();
	VExpression* ParseExpressionPriority1();
	VExpression* ParseExpressionPriority2();
	VExpression* ParseExpressionPriority3();
	VExpression* ParseExpressionPriority4();
	VExpression* ParseExpressionPriority5();
	VExpression* ParseExpressionPriority6();
	VExpression* ParseExpressionPriority7();
	VExpression* ParseExpressionPriority8();
	VExpression* ParseExpressionPriority9();
	VExpression* ParseExpressionPriority10();
	VExpression* ParseExpressionPriority11();
	VExpression* ParseExpressionPriority12();
	VExpression* ParseExpressionPriority13();
	VExpression* ParseExpressionPriority14();
	VExpression* ParseExpression();
	VStatement* ParseStatement();
	VCompound* ParseCompoundStatement();
	VExpression* ParseType();
	void ParseMethodDef(VExpression*, VName, TLocation, VClass*, vint32, bool);
	void ParseDelegate(VExpression*, VField*);
	void ParseDefaultProperties(VClass*);
	void ParseStruct(VClass*, bool);
	VName ParseStateString();
	void ParseStates(VClass*);
	void ParseReplication(VClass*);
	void ParseClass();

public:
	VParser(VLexer& ALex, VPackage* APackage)
	: Lex(ALex)
	, Package(APackage)
	{}
	void Parse();
};

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int dprintf(const char *text, ...);

char *va(const char *text, ...) __attribute__ ((format(printf, 1, 2)));

VStream* OpenFile(const VStr& Name);

// PUBLIC DATA DECLARATIONS ------------------------------------------------

extern bool						GBigEndian;

#endif
