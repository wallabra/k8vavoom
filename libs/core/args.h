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
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
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

class VArgs {
private:
  int Argc;
  char **Argv;

  char **fopts;
  int foptsCount;

private:
  void FindResponseFile ();
  void InsertFileArg (const char *filearg);

  void InsertArgAt (int idx, const char *arg);

public:
  VArgs () : Argc(0), Argv(nullptr), fopts(nullptr), foptsCount(0) {}

  // specify `filearg` to insert before disk files without one
  void Init (int argc, char **argv, const char *filearg=nullptr);

  inline int Count () const { return Argc; }
  inline const char *operator [] (int i) const { return (i >= 0 && i < Argc ? Argv[i] : ""); }

  // add options which doesn't interrupt "-file"
  void AddFileOption (const char *optname);

  // returns the position of the given parameter in the arg list (0 if not found)
  // if `takeFirst` is true, take first found, otherwise take last found
  int CheckParm (const char *check, bool takeFirst=true) const;
  // returns the value of the given parameter in the arg list, or `nullptr`
  // if `takeFirst` is true, take first found, otherwise take last found
  const char *CheckValue (const char *check, bool takeFirst=true) const;

  // return `true` if given index is valid, and arg starts with '-' or '+'
  bool IsCommand (int idx) const;
};


extern VArgs GArgs;
