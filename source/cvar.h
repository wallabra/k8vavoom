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

enum
{
	CVAR_Archive	= 0x0001,	//	Set to cause it to be saved to config.cfg
	CVAR_UserInfo	= 0x0002,	//	Added to userinfo  when changed
	CVAR_ServerInfo	= 0x0004,	//	Added to serverinfo when changed
	CVAR_Init		= 0x0008,	//	Don't allow change from console at all,
								// but can be set from the command line
	CVAR_Latch		= 0x0010,	//	Save changes until server restart
	CVAR_Rom		= 0x0020,	//	Display only, cannot be set by user at all
	CVAR_Cheat		= 0x0040,	//	Can not be changed if cheats are disabled
	CVAR_Modified	= 0x0080,	//	Set each time the cvar is changed
};

//
//	Console variable
//
class VCvar
{
protected:
	const char*	Name;				//	Variable's name
	const char*	DefaultString;		//	Default value
	const char* HelpString; // this *can* be owned, but as we never deleting cvar objects, it doesn't matter
	bool defstrOwned; // `true` if `DefaultString` is owned and should be deleted
	VStr		StringValue;		//	Current value
	int			Flags;				//	CVAR_ flags
	int			IntValue;			//	atoi(string)
	float		FloatValue;			//	atof(string)
	bool		BoolValue;			//	interprets various "true" strings
	VStr		LatchedString;		//	For CVAR_Latch variables
	VCvar* nextInBucket; // next cvar in this bucket
	vuint32 lnhash; // hash of lo-cased variable name

public:
	VCvar(const char* AName, const char* ADefault, const char *AHelp, int AFlags = 0);
	VCvar(const char* AName, const VStr& ADefault, const VStr& AHelp, int AFlags = 0);
	void Register();
	void Set(int value);
	void Set(float value);
	void Set(const VStr& value);
	bool IsModified();

	inline const char *GetName () const { return Name; }
	inline const char *GetHelp () const { return (HelpString ? HelpString : "no help yet."); }

	static void Init();
	static void Shutdown();

	static bool HasVar(const char* var_name);
	static void CreateNew(const char* var_name, const VStr& ADefault, const VStr& AHelp, int AFlags);

	static int GetInt(const char* var_name);
	static float GetFloat(const char* var_name);
	static bool GetBool(const char* var_name);
	static const char* GetCharp(const char* var_name);
	static VStr GetString(const char* var_name);
	static const char* GetHelp(const char* var_name); // returns NULL if there is no such cvar

	static void Set(const char* var_name, int value);
	static void Set(const char* var_name, float value);
	static void Set(const char* var_name, const VStr& value);

	static bool Command(const TArray<VStr>& Args);
	static void WriteVariablesToFile(FILE* f);

	static void Unlatch();
	static void SetCheating(bool);

	static VCvar* FindVariable(const char* name);

	friend class TCmdCvarList;

private:
	static void dumpHashStats ();
	static vuint32 countCVars ();
	static VCvar** getSortedList (); // contains `countCVars()` elements, must be `delete[]`d

	void insertIntoList ();
	VCvar *insertIntoHash ();
	void DoSet(const VStr& value);

	static bool		Initialised;
	static bool		Cheating;
};

//	Cvar, that can be used as int variable
class VCvarI : public VCvar
{
public:
	VCvarI(const char* AName, const char* ADefault, const char *AHelp, int AFlags = 0)
		: VCvar(AName, ADefault, AHelp, AFlags)
	{
	}

	operator int() const
	{
		return IntValue;
	}

	VCvarI &operator = (int AValue)
	{
		Set(AValue);
		return *this;
	}
};

//	Cvar, that can be used as float variable
class VCvarF : public VCvar
{
public:
	VCvarF(const char* AName, const char* ADefault, const char *AHelp, int AFlags = 0)
		: VCvar(AName, ADefault, AHelp, AFlags)
	{
	}

	operator float() const
	{
		return FloatValue;
	}

	VCvarF &operator = (float AValue)
	{
		Set(AValue);
		return *this;
	}
};

//	Cvar, that can be used as char* variable
class VCvarS : public VCvar
{
public:
	VCvarS(const char* AName, const char* ADefault, const char *AHelp, int AFlags = 0)
		: VCvar(AName, ADefault, AHelp, AFlags)
	{
	}

	operator const char*() const
	{
		return *StringValue;
	}

	VCvarS &operator = (const char* AValue)
	{
		Set(AValue);
		return *this;
	}
};
