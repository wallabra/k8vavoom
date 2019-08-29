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
  int CheckParm (const char *check, bool takeFirst=true, bool startsWith=false) const;

  // returns the value of the given parameter in the arg list, or `nullptr`
  // if `takeFirst` is true, take first found, otherwise take last found
  const char *CheckValue (const char *check, bool takeFirst=true, bool startsWith=false) const;

  // returns the position of the given parameter in the arg list (0 if not found)
  int CheckParmFrom (const char *check, int stidx=-1, bool startsWith=false) const;

  // return `true` if given index is valid, and arg starts with '-' or '+'
  bool IsCommand (int idx) const;

  void removeAt (int idx);

public:
  static char *GetBinaryDir ();
};


extern VArgs GArgs;


// ////////////////////////////////////////////////////////////////////////// //
// parsed arguments should be used instead of `GArgs`
class VParsedArgs {
public:
  // return next index (if you eat some args), or 0 to continue as normal
  // idx points at the argument name
  typedef int (*ArgCB) (VArgs &args, int idx);

  // handler types
  enum {
    AT_Callback, // just a nornal arg with a callback, nothing special
    AT_StringOption, // just a string option, latest is used
    AT_FlagSet, // "set flag" argument
    AT_FlagReset, // "reset flag" argument
    AT_FlagToggle, // "toggle flag" arguments
    AT_Ignore,
  };

protected:
  struct ArgInfo {
    const char *name; // full, i.e. "-argname"
    const char *help; // short help (nullptr to hide)
    int *flagptr; // can be `nullptr`
    const char **strptr; // can be `nullptr`
    char *strarg;
    int type; // AT_xxx
    ArgCB cb;
    ArgInfo *next;
  };

  static ArgInfo *argInfoHead;
  static ArgInfo *argInfoFileArg;
  static ArgInfo *argInfoCmdArg;

  static ArgInfo *allocArgInfo (const char *argname, const char *shorthelp);

  static ArgInfo *findNamedArgInfo (const char *argname);

protected:
  char *mBinPath;

protected:
  void clear ();

public:
  static bool IsArgBreaker (VArgs &args, int idx);

public:
  VParsedArgs ();

  void parse (VArgs &args);

  // `nullptr` as name means "file argument handler"
  // "+" as name means "command handler"
  static bool RegisterCallback (const char *argname, const char *shorthelp, ArgCB acb);
  // simple string option, later will override
  static bool RegisterStringOption (const char *argname, const char *shorthelp, const char **strptr);
  // flags are int, because caller may set them to `-1`, for example, to indicate "no flag arg was seen"
  // the parser itself will only use `0` or `1`
  // toggling negative flag will turn it into `1`
  static bool RegisterFlagSet (const char *argname, const char *shorthelp, int *flagptr);
  static bool RegisterFlagReset (const char *argname, const char *shorthelp, int *flagptr);
  static bool RegisterFlagToggle (const char *argname, const char *shorthelp, int *flagptr);

  // `oldname` should be already registered
  static bool RegisterAlias (const char *argname, const char *oldname);

  // with ending slash
  inline VStr getBinPath () const { return VStr(mBinPath); }

public:
  struct ArgHelp {
    const char *argname;
    const char *arghelp;
  };

  // will not clear the list
  static void GetArgList (TArray<ArgHelp> &list);
};


extern VParsedArgs GParsedArgs;
