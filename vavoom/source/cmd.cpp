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
//**	Copyright (C) 1999-2002 J�nis Legzdi��
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

// HEADER FILES ------------------------------------------------------------

#include "gamedefs.h"

// MACROS ------------------------------------------------------------------

#define CMD_LINE_SIZE	1024
#define CMD_NUM_ARGS	40

// TYPES -------------------------------------------------------------------

struct alias_t
{
	VStr		Name;
	VStr		CmdLine;
	alias_t*	Next;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

#ifdef CLIENT
void C_AddToAutoComplete(const char* string);
#endif

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

TCmdBuf				CmdBuf;
cmd_source_t		cmd_source;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static TCommand*	Cmds = NULL;
static alias_t*		alias = NULL;

static bool			cmd_wait = false;
static bool			cmd_initialised = false;

static char			cmd_line[CMD_LINE_SIZE];
static const char*	cmd_original;
static int			cmd_argc;
static char*		cmd_argv[CMD_NUM_ARGS];
static char*		cmd_args;

// CODE --------------------------------------------------------------------

//**************************************************************************
//
//	Commands, alias
//
//**************************************************************************

//==========================================================================
//
//  Cmd_Init
//
//==========================================================================

void Cmd_Init()
{
	int			i;
	boolean		in_cmd = false;

#ifdef CLIENT
	for (TCommand *cmd = Cmds; cmd; cmd = cmd->Next)
	{
		C_AddToAutoComplete(cmd->Name);
	}
#endif

	CmdBuf.Init();

	//	Add configuration file execution
	CmdBuf << "exec startup.vs\n";

	//	Add consloe commands from command line
	// These are params, that start with + and continues untill the end
	// or untill next param, that starts with - or +
	for (i = 1; i < myargc; i++)
	{
		if (in_cmd)
		{
			if (!myargv[i] || myargv[i][0] == '-' || myargv[i][0] == '+')
			{
				in_cmd = false;
				CmdBuf << "\n";
			}
			else
			{
				CmdBuf << " " << myargv[i];
				continue;
			}
		}
		if (myargv[i][0] == '+')
		{
			in_cmd = true;
			CmdBuf << (myargv[i] + 1);
		}
	}
	if (in_cmd)
	{
		CmdBuf << "\n";
	}

	cmd_initialised = true;
}

//**************************************************************************
//
//	Parsing of a command, command arg handling
//
//**************************************************************************

//==========================================================================
//
//	Cmd_TokenizeString
//
//==========================================================================

void Cmd_TokeniseString(const char *str)
{
	char		*p;

	cmd_original = str;
	cmd_argc = 0;
	cmd_argv[0] = NULL;
	cmd_args = NULL;
	strcpy(cmd_line, str);
	p = cmd_line;

	while (*p)
	{
		//	Whitespace
		if (*p <= ' ')
		{
			*p = 0;
			p++;
			continue;
		}

		if (cmd_argc == 1)
		{
			cmd_args = (char*)cmd_original + (p - cmd_line);
		}

		// String
		if (*p == '\"')
		{
			p++;
			cmd_argv[cmd_argc] = p;
			// Checks for end of string
			while (*p && *p != '\"')
			{
				p++;
			}
			if (!*p)
			{
				GCon->Log("ERROR: Missing closing quote!");
				return;
			}
			//	Erase closing quote
			*p = 0;
			p++;
		}
		else
		{
			// Simple arg
			cmd_argv[cmd_argc] = p;
			p++;
			while (*p > ' ')
			{
				p++;
			}
		}
		// Next will be NULL
		cmd_argc++;
		cmd_argv[cmd_argc] = NULL;
	}
}

//==========================================================================
//
//  Cmd_CheckParm
//
//==========================================================================

int Cmd_CheckParm(const char *check)
{
	int		i;

	for (i = 1; i < cmd_argc; i++)
	{
		if (!stricmp(check, cmd_argv[i]))
			return i;
	}

	return 0;
}

//==========================================================================
//
//  Cmd_Argc
//
//==========================================================================

int Cmd_Argc()
{
	return cmd_argc;
}

//==========================================================================
//
//  Cmd_Argv
//
//==========================================================================

char **Cmd_Argv()
{
	return cmd_argv;
}

//==========================================================================
//
//  Cmd_Argv
//
//==========================================================================

char *Cmd_Argv(int parm)
{
	static char		null_string[] = "";

	if (parm < 0 || parm >= cmd_argc)
		return null_string;
	return cmd_argv[parm];
}

//==========================================================================
//
//	Cmd_Args
//
//==========================================================================

char *Cmd_Args()
{
	static char		null_string[] = "";
	if (cmd_args)
		return cmd_args;
	else
		return null_string;
}

//**************************************************************************
//
//	Some commands
//
//**************************************************************************

//==========================================================================
//
//	COMMAND CmdList
//
//==========================================================================

COMMAND(CmdList)
{
	const char *prefix = Argv(1);
	int pref_len = strlen(prefix);
	int count = 0;
	for (TCommand *cmd = Cmds; cmd; cmd = cmd->Next)
	{
		if (pref_len && strnicmp(cmd->Name, prefix, pref_len))
			continue;
		GCon->Logf(" %s", cmd->Name);
		count++;
	}
	GCon->Logf("%d commands.", count);
}

//==========================================================================
//
//  Alias_f
//
//==========================================================================

COMMAND(Alias)
{
	alias_t*	a;
	VStr		tmp;
	int			i;
	int			c;

	if (Argc() == 1)
	{
		GCon->Log("Current alias:");
		for (a = alias; a; a = a->Next)
		{
			GCon->Log(a->Name + ": " + a->CmdLine);
		}
		return;
	}

	c = Argc();
	for (i = 2; i < c; i++)
	{
		if (i != 2)
			tmp += " ";
		tmp += Argv(i);
	}

	for (a = alias; a; a = a->Next)
	{
		if (!a->Name.ICmp(Argv(1)))
		{
			break;
		}
	}

	if (!a)
	{
		a = new(PU_STRING) alias_t;
		a->Name = Argv(1);
		a->Next = alias;
		alias = a;
	}
	a->CmdLine = tmp;
}

//==========================================================================
//
//	Cmd_WriteAlias
//
//==========================================================================

void Cmd_WriteAlias(FILE *f)
{
	for (alias_t *a = alias; a; a = a->Next)
	{
		fprintf(f, "alias %s \"%s\"\n", *a->Name, *a->CmdLine);
	}
}

//==========================================================================
//
//  Echo_f
//
//==========================================================================

COMMAND(Echo)
{
#ifdef CLIENT
	C_NotifyMessage(Args());
#else
	GCon->Log(Args());
#endif
}

//==========================================================================
//
//  Exec_f
//
//==========================================================================

COMMAND(Exec)
{
	char	*buf;

	if (Argc() != 2)
	{
		GCon->Log("Exec <filename> : execute script file");
		return;
	}

	VStr path = FL_FindFile(Argv(1));
	if (!path)
	{
		GCon->Logf("Can't find \"%s\".", Argv(1));
		return;
	}

	GCon->Logf("Executing \"%s\".", *path);

	M_ReadFile(*path, (byte**)&buf);
	CmdBuf.Insert(buf);
	Z_Free(buf);
}

//==========================================================================
//
//	COMMAND Wait
//
//==========================================================================

COMMAND(Wait)
{
	cmd_wait = true;
}

//**************************************************************************
//
//	Commands
//
//**************************************************************************

//==========================================================================
//
//  TCommand::TCommand
//
//==========================================================================

TCommand::TCommand(const char *name)
{
	Next = Cmds;
	Name = name;
	Cmds = this;
#ifdef CLIENT
	if (cmd_initialised)
	{
		C_AddToAutoComplete(Name);
	}
#endif
}

//==========================================================================
//
//  TCommand::~TCommand
//
//==========================================================================

TCommand::~TCommand()
{
}

//**************************************************************************
//
//	Command buffer
//
//**************************************************************************

//==========================================================================
//
//  TCmdBuf::Init
//
//==========================================================================

void TCmdBuf::Init()
{
}

//==========================================================================
//
//	TCmdBuf::Insert
//
//==========================================================================

void TCmdBuf::Insert(const char* text)
{
	Buffer = VStr(text) + Buffer;
}

//==========================================================================
//
//	TCmdBuf::Insert
//
//==========================================================================

void TCmdBuf::Insert(const VStr& text)
{
	Buffer = text + Buffer;
}

//==========================================================================
//
//  TCmdBuf::Print
//
//==========================================================================

void TCmdBuf::Print(const char* data)
{
	Buffer += data;
}

//==========================================================================
//
//  TCmdBuf::Print
//
//==========================================================================

void TCmdBuf::Print(const VStr& data)
{
	Buffer += data;
}

//==========================================================================
//
//  TCmdBuf::Exec
//
//==========================================================================

void TCmdBuf::Exec()
{
	int			len;
	int			quotes;
	bool		comment;
	VStr		ParsedCmd;

	do
	{
		quotes = 0;
		comment = false;
		ParsedCmd.Clean();

		for (len = 0; len < Buffer.Length(); len++)
		{
			if (Buffer[len] == '\n')
				break;
			if (comment)
				continue;
			if (Buffer[len] == ';' && !(quotes & 1))
				break;
			if (Buffer[len] == '/' && Buffer[len + 1] == '/' && !(quotes & 1))
			{
				// Comment, all till end is ignored
				comment = true;
				continue;
			}
			if (Buffer[len] == '\"')
				quotes++;
			ParsedCmd += Buffer[len];
		}

		if (len < Buffer.Length())
		{
			len++;	//	Skip seperator symbol
		}

		Buffer = VStr(Buffer, len, Buffer.Length() - len);

		Cmd_ExecuteString(*ParsedCmd, src_command);
		
		if (cmd_wait)
		{
			// skip out while text still remains in buffer, leaving it
			// for next frame
			cmd_wait = false;
			break;
		}
	} while (len);
}

//**************************************************************************
//
//  Parsed command
//
//**************************************************************************

//==========================================================================
//
//	Cmd_ExecuteString
//
//==========================================================================

void Cmd_ExecuteString(const char* Acmd, cmd_source_t src)
{
	Cmd_TokeniseString(Acmd);
	cmd_source = src;

	if (!cmd_argc)
		return;

	//
	//	Check for command
	//
	for (TCommand *cmd = Cmds; cmd; cmd = cmd->Next)
	{
		if (!stricmp(cmd_argv[0], cmd->Name))
		{
			cmd->Run();
			return;
		}
	}

	//
	//	Cvar
	//
	if (TCvar::Command(cmd_argc, (const char **)cmd_argv))
		return;

	//
	// Command defined with ALIAS.
	//
	for (alias_t *a = alias; a; a = a->Next)
	{
		if (!stricmp(cmd_argv[0], *a->Name))
		{
			CmdBuf.Insert("\n");
			CmdBuf.Insert(a->CmdLine);
			return;
		}
	}

	//
	// Unknown command.
	//
#ifndef CLIENT
	if (host_initialized)
#endif
		GCon->Logf("Unknown command \"%s\".", cmd_argv[0]);
}

#ifdef CLIENT

//==========================================================================
//
//	Cmd_ForwardToServer
//
//==========================================================================

void Cmd_ForwardToServer()
{
	cls.message << (byte)clc_stringcmd << cmd_original;
}

#endif

//**************************************************************************
//
//	$Log$
//	Revision 1.14  2006/03/29 22:32:27  dj_jl
//	Changed console variables and command buffer to use dynamic strings.
//
//	Revision 1.13  2006/03/04 16:01:34  dj_jl
//	File system API now uses strings.
//	
//	Revision 1.12  2005/04/28 07:16:11  dj_jl
//	Fixed some warnings, other minor fixes.
//	
//	Revision 1.11  2003/12/19 17:36:58  dj_jl
//	Dedicated server fix
//	
//	Revision 1.10  2003/10/31 07:49:52  dj_jl
//	echo uses notify messages
//	
//	Revision 1.9  2003/10/22 06:24:35  dj_jl
//	Access to the arguments vector
//	
//	Revision 1.8  2002/07/23 16:29:55  dj_jl
//	Replaced console streams with output device class.
//	
//	Revision 1.7  2002/01/07 12:16:41  dj_jl
//	Changed copyright year
//	
//	Revision 1.6  2001/12/18 19:05:03  dj_jl
//	Made TCvar a pure C++ class
//	
//	Revision 1.5  2001/10/04 17:20:25  dj_jl
//	Saving config using streams
//	
//	Revision 1.4  2001/08/29 17:50:09  dj_jl
//	Renamed command Commands to CmdList
//	
//	Revision 1.3  2001/07/31 17:16:30  dj_jl
//	Just moved Log to the end of file
//	
//	Revision 1.2  2001/07/27 14:27:54  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************
