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

// HEADER FILES ------------------------------------------------------------

#include "gamedefs.h"
#include "network.h"
#include "sv_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

VCvar*	VCvar::Variables = NULL;
bool	VCvar::Initialised = false;
bool	VCvar::Cheating;

#define CVAR_HASH_SIZE  (8192)
static VCvar* cvhBuckets[CVAR_HASH_SIZE] = {NULL};


// CODE --------------------------------------------------------------------

static vuint32 djbhash (const char *s) {
  vuint32 hash = 5381;
  if (s) {
    for (; *s; ++s) {
      vuint32 ch = (vuint32)(*s&0xff);
      if (ch >= 'A' && ch <= 'Z') ch += 32; // poor man's tolower
      hash = ((hash<<5)+hash)+ch;
    }
  }
  return hash;
}


//==========================================================================
//
//  VCvar::VCvar
//
//==========================================================================

VCvar::VCvar(const char* AName, const char* ADefault, int AFlags)
: Name(AName)
, DefaultString(ADefault)
, Flags(AFlags)
, IntValue(0)
, FloatValue(0)
, BoolValue(false)
, Next(NULL)
, nextInBucket(NULL)
{
	guard(VCvar::VCvar);

	if (Name && Name[0]) {
		insertIntoHash(); // insert into hash (this leaks on duplicate vars)
		insertIntoList(); // insert into linked list
		if (Initialised) Register();
	}

	unguard;
}

//==========================================================================
//
//  VCvar::VCvar
//
//==========================================================================

VCvar::VCvar(const char* AName, const VStr& ADefault, int AFlags)
: Name(AName)
, Flags(AFlags | CVAR_Delete)
, IntValue(0)
, FloatValue(0)
, BoolValue(false)
, Next(NULL)
, nextInBucket(NULL)
{
	guard(VCvar::VCvar);
	char* Tmp = new char[ADefault.Length() + 1];
	VStr::Cpy(Tmp, *ADefault);
	DefaultString = Tmp;

	if (Name && Name[0]) {
		insertIntoHash(); // insert into hash (this leaks on duplicate vars)
		insertIntoList(); // insert into linked list
		check(Initialised);
		Register();
	}

	unguard;
}


// returns replaced cvar, or NULL
VCvar *VCvar::insertIntoHash () {
  if (!this->Name || !this->Name[0]) return NULL;
  vuint32 nhash = djbhash(this->Name);
  this->lnhash = nhash;
  VCvar* prev = NULL;
  for (VCvar* cvar = cvhBuckets[nhash%CVAR_HASH_SIZE]; cvar; prev = cvar, cvar = cvar->nextInBucket) {
    if (cvar->lnhash == nhash && !VStr::ICmp(this->Name, cvar->Name)) {
      // replace it
      if (prev) {
        prev->nextInBucket = this;
      } else {
        cvhBuckets[nhash%CVAR_HASH_SIZE] = this;
      }
      this->nextInBucket = cvar->nextInBucket;
      return cvar;
    }
  }
  // new one
  this->nextInBucket = cvhBuckets[nhash%CVAR_HASH_SIZE];
  cvhBuckets[nhash%CVAR_HASH_SIZE] = this;
  return NULL;
}


// insert into sorted list
void VCvar::insertIntoList () {
  if (!Name || !Name[0]) return;

  while (Variables && VStr::ICmp(Variables->Name, Name) == 0) Variables = Variables->Next;

  VCvar *prev = NULL;
  for (VCvar *var = Variables; var; var = var->Next) {
    if (VStr::ICmp(var->Name, Name) < 0) {
      prev = var;
      VCvar *cont = prev->Next;
      if (cont && VStr::ICmp(cont->Name, Name) == 0) {
        while (cont && VStr::ICmp(cont->Name, Name) == 0) cont = cont->Next;
        prev->Next = cont;
      }
    }
  }

  if (prev) {
    Next = prev->Next;
    prev->Next = this;
  } else {
    Next = Variables;
    Variables = this;
  }
}


//==========================================================================
//
//  VCvar::Register
//
//==========================================================================

void VCvar::Register()
{
	guard(VCvar::Register);
	VCommand::AddToAutoComplete(Name);
	DoSet(DefaultString);
	unguard;
}

//==========================================================================
//
//  VCvar::Set
//
//==========================================================================

void VCvar::Set(int value)
{
	guard(VCvar::Set);
	Set(VStr(value));
	unguard;
}

//==========================================================================
//
//  VCvar::Set
//
//==========================================================================

void VCvar::Set(float value)
{
	guard(VCvar::Set);
	Set(VStr(value));
	unguard;
}

//==========================================================================
//
//  VCvar::Set
//
//==========================================================================

void VCvar::Set(const VStr& AValue)
{
	guard(VCvar::Set);
	if (Flags & CVAR_Latch)
	{
		LatchedString = AValue;
		return;
	}

	if (Flags & CVAR_Cheat && !Cheating)
	{
		GCon->Log(VStr(Name) + " cannot be changed while cheating is disabled");
		return;
	}

	DoSet(AValue);

	Flags |= CVAR_Modified;
	unguard;
}

//==========================================================================
//
//	VCvar::DoSet
//
//	Does the actual value assignement
//
//==========================================================================

static bool xstrcmpCI (const char* s, const char *pat) {
  if (!s || !pat || !s[0] || !pat[0]) return false;
  while (*s && *s <= ' ') ++s;
  while (*s && *pat) {
    char c0 = *s++;
    char c1 = *pat++;
    if (c0 != c1) {
      if (c0 >= 'A' && c0 <= 'Z') c0 += 32; // poor man's tolower
      if (c1 >= 'A' && c1 <= 'Z') c1 += 32; // poor man's tolower
      if (c0 != c1) return false;
    }
  }
  if (*pat || *s > ' ') return false;
  while (*s && *s <= ' ') ++s;
  return (s[0] == 0);
}


void VCvar::DoSet(const VStr& AValue)
{
	guard(VCvar::DoSet);
	StringValue = AValue;
	IntValue = superatoi(*StringValue);
	FloatValue = atof(*StringValue);

	// interpret boolean
	if (IntValue != 0) {
		// easy
		BoolValue = true;
	} else {
		// check various strings
		BoolValue =
		  xstrcmpCI(*StringValue, "true") ||
		  xstrcmpCI(*StringValue, "on") ||
		  xstrcmpCI(*StringValue, "tan") ||
		  xstrcmpCI(*StringValue, "yes");
	}

#ifdef CLIENT
	if (Flags & CVAR_UserInfo)
	{
		Info_SetValueForKey(cls.userinfo, Name, *StringValue);
		if (cl)
		{
			if (GGameInfo->NetMode == NM_TitleMap ||
				GGameInfo->NetMode == NM_Standalone ||
				GGameInfo->NetMode == NM_ListenServer)
			{
				VCommand::ExecuteString(VStr("setinfo \"") + Name + "\" \"" +
					StringValue + "\"\n", VCommand::SRC_Client, cl);
			}
			else if (cl->Net)
			{
				cl->Net->SendCommand(VStr("setinfo \"") + Name + "\" \"" +
					StringValue + "\"\n");
			}
		}
	}
#endif

#ifdef SERVER
	if (Flags & CVAR_ServerInfo)
	{
		Info_SetValueForKey(svs.serverinfo, Name, *StringValue);
		if (GGameInfo && GGameInfo->NetMode != NM_None &&
			GGameInfo->NetMode != NM_Client)
		{
			for (int i = 0; i < MAXPLAYERS; i++)
			{
				if (GGameInfo->Players[i])
				{
					GGameInfo->Players[i]->eventClientSetServerInfo(
						Name, StringValue);
				}
			}
		}
	}
#endif
	unguard;
}

//==========================================================================
//
//	VCvar::IsModified
//
//==========================================================================

bool VCvar::IsModified()
{
	guard(VCvar::IsModified);
	bool ret = !!(Flags & CVAR_Modified);
	//	Clear modified flag.
	Flags &= ~CVAR_Modified;
	return ret;
	unguard;
}

//==========================================================================
//
//	VCvar::Init
//
//==========================================================================

void VCvar::Init()
{
	guard(VCvar::Init);
	for (VCvar *var = Variables; var; var = var->Next)
	{
		var->Register();
	}
	Initialised = true;
	unguard;
}


void VCvar::dumpHashStats () {
  vuint32 bkused = 0, maxchain = 0;
  for (vuint32 bkn = 0; bkn < CVAR_HASH_SIZE; ++bkn) {
    VCvar *cvar = cvhBuckets[bkn];
    if (!cvar) continue;
    ++bkused;
    vuint32 chlen = 0;
    for (; cvar; cvar = cvar->nextInBucket) ++chlen;
    if (chlen > maxchain) maxchain = chlen;
  }
  printf("CVAR statistics: %u buckets used, %u items in longest chain\n", bkused, maxchain);
}


//==========================================================================
//
//	VCvar::Shutdown
//
//==========================================================================

void VCvar::Shutdown()
{
	guard(VCvar::Shutdown);
	dumpHashStats();
	for (VCvar* var = Variables; var;)
	{
		VCvar* Next = var->Next;
		var->StringValue.Clean();
		var->LatchedString.Clean();
		if (var->Flags & CVAR_Delete)
		{
			delete[] const_cast<char*>(var->DefaultString);
			var->DefaultString = NULL;
			delete var;
			var = NULL;
		}
		var = Next;
	}
	Initialised = false;
	unguard;
}

//==========================================================================
//
//	VCvar::Unlatch
//
//==========================================================================

void VCvar::Unlatch()
{
	guard(VCvar::Unlatch);
	for (VCvar* cvar = Variables; cvar; cvar = cvar->Next)
	{
		if (cvar->LatchedString.IsNotEmpty())
		{
			cvar->DoSet(cvar->LatchedString);
			cvar->LatchedString.Clean();
		}
	}
	unguard;
}

//==========================================================================
//
//	VCvar::SetCheating
//
//==========================================================================

void VCvar::SetCheating(bool new_state)
{
	guard(VCvar::SetCheating);
	Cheating = new_state;
	if (!Cheating)
	{
		for (VCvar *cvar = Variables; cvar; cvar = cvar->Next)
		{
			if (cvar->Flags & CVAR_Cheat)
			{
				cvar->DoSet(cvar->DefaultString);
			}
		}
	}
	unguard;
}


bool VCvar::HasVar (const char* var_name) {
  return (FindVariable(var_name) != NULL);
}


void VCvar::CreateNew (const char* var_name, const VStr& ADefault, int AFlags) {
  VCvar* cvar = FindVariable(var_name);
  if (!cvar) {
    new VCvar(var_name, ADefault, AFlags|CVAR_Delete);
  } else {
    char* Tmp = new char[ADefault.Length() + 1];
    VStr::Cpy(Tmp, *ADefault);
    cvar->DoSet(ADefault);
    delete[] const_cast<char*>(cvar->DefaultString);
    cvar->DefaultString = Tmp;
    cvar->Flags = AFlags/*|CVAR_Delete*/;
  }
}


//==========================================================================
//
//  VCvar::FindVariable
//
//==========================================================================

VCvar* VCvar::FindVariable(const char* name)
{
	guard(VCvar::FindVariable);
	if (!name || name[0] == 0) return NULL;
	vuint32 nhash = djbhash(name);
	for (VCvar *cvar = cvhBuckets[nhash%CVAR_HASH_SIZE]; cvar; cvar = cvar->nextInBucket) {
		if (cvar->lnhash == nhash && !VStr::ICmp(name, cvar->Name)) return cvar;
	}
	return NULL;
	unguard;
}

//==========================================================================
//
//  VCvar::GetInt
//
//==========================================================================

int VCvar::GetInt(const char* var_name)
{
	guard(VCvar::GetInt);
	VCvar* var = FindVariable(var_name);
	if (!var)
		return 0;
	return var->IntValue;
	unguard;
}

//==========================================================================
//
//  VCvar::GetFloat
//
//==========================================================================

float VCvar::GetFloat(const char* var_name)
{
	guard(VCvar::GetFloat);
	VCvar* var = FindVariable(var_name);
	if (!var)
		return 0;
	return var->FloatValue;
	unguard;
}

//==========================================================================
//
//  VCvar::GetBool
//
//==========================================================================

bool VCvar::GetBool (const char* var_name) {
	guard(VCvar::GetBool);
	VCvar* var = FindVariable(var_name);
	return (var ? var->BoolValue : false);
	unguard;
}

//==========================================================================
//
//  GetCharp
//
//==========================================================================

const char* VCvar::GetCharp(const char* var_name)
{
	guard(VCvar::GetCharp);
	VCvar* var = FindVariable(var_name);
	if (!var)
	{
		return "";
	}
	return *var->StringValue;
	unguard;
}

//==========================================================================
//
//  VCvar::GetString
//
//==========================================================================

VStr VCvar::GetString(const char* var_name)
{
	guard(VCvar::GetString);
	VCvar* var = FindVariable(var_name);
	if (!var)
	{
		return VStr();
	}
	return var->StringValue;
	unguard;
}

//==========================================================================
//
//  VCvar::Set
//
//==========================================================================

void VCvar::Set(const char* var_name, int value)
{
	guard(VCvar::Set);
	VCvar* var = FindVariable(var_name);
	if (!var)
	{
		Sys_Error("Cvar_Set: variable %s not found\n", var_name);
	}
	var->Set(value);
	unguard;
}

//==========================================================================
//
//  VCvar::Set
//
//==========================================================================

void VCvar::Set(const char* var_name, float value)
{
	guard(VCvar::Set);
	VCvar* var = FindVariable(var_name);
	if (!var)
	{
		Sys_Error("Cvar_Set: variable %s not found\n", var_name);
	}
	var->Set(value);
	unguard;
}

//==========================================================================
//
//  VCvar::Set
//
//==========================================================================

void VCvar::Set(const char* var_name, const VStr& value)
{
	guard(VCvar::Set);
	VCvar* var = FindVariable(var_name);
	if (!var)
	{
		Sys_Error("Cvar_SetString: variable %s not found\n", var_name);
	}
	var->Set(value);
	unguard;
}

//==========================================================================
//
//	VCvar::Command
//
//==========================================================================

bool VCvar::Command(const TArray<VStr>& Args)
{
	guard(VCvar::Command);
	VCvar* cvar = FindVariable(*Args[0]);
	if (!cvar)
	{
		return false;
	}

	// perform a variable print or set
	if (Args.Num() == 1)
	{
		GCon->Log(VStr(cvar->Name) + " is \"" + cvar->StringValue + "\"");
		if (cvar->Flags & CVAR_Latch && cvar->LatchedString.IsNotEmpty())
			GCon->Log(VStr("Latched \"") + cvar->LatchedString + "\"");
	}
	else
	{
		if (cvar->Flags & CVAR_Rom)
		{
			GCon->Logf("%s is read-only", cvar->Name);
		}
		else if (cvar->Flags & CVAR_Init && host_initialised)
		{
			GCon->Logf("%s can be set only from command-line", cvar->Name);
		}
		else
		{
			cvar->Set(Args[1]);
		}
	}
	return true;
	unguard;
}

//==========================================================================
//
//	VCvar::WriteVariables
//
//==========================================================================

void VCvar::WriteVariables(FILE* f)
{
	guard(VCvar::WriteVariables);
	for (VCvar* cvar = Variables; cvar; cvar = cvar->Next)
	{
		if (cvar->Flags & CVAR_Archive)
		{
			fprintf(f, "%s\t\t\"%s\"\n", cvar->Name, *cvar->StringValue);
		}
	}
	unguard;
}

//==========================================================================
//
//	COMMAND CvarList
//
//==========================================================================

COMMAND(CvarList)
{
	guard(COMMAND CvarList);
	int count = 0;
	for (VCvar *cvar = VCvar::Variables; cvar; cvar = cvar->Next)
	{
		GCon->Log(VStr(cvar->Name) + " - \"" + cvar->StringValue + "\"");
		count++;
	}
	GCon->Logf("%d variables.", count);
	unguard;
}
