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
//**  Copyright (C) 2018-2020 Ketmar Dark
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

// a console command
class VCommand {
public:
  enum ECmdSource {
    SRC_Command,
    SRC_Client,
  };

public: // fuck you, shitplusplus
  // console command alias
  struct VAlias {
    VStr Name;
    VStr CmdLine;
    bool Save;
  };

private:
  const char *Name;
  VCommand *Next;

  static bool Initialised;
  static VStr Original;

  static TArray<VStr> AutoCompleteTable;

  static VCommand *Cmds;
  static TArray<VAlias> AliasList;
  static TMap<VStr, int> AliasMap;

  static bool cliInserted;

protected:
  static bool execLogInit;

private:
  static void TokeniseString (VStr);

  static void rebuildCommandCache ();

  static void rebuildAliasMap ();

protected:
  static TArray<VStr> Args;
  static ECmdSource Source;
  static VBasePlayer *Player; // for SRC_Client

  static bool rebuildCache;
  static TMapDtor<VStr, VCommand *> locaseCache;

public:
  static bool ParsingKeyConf;
  static VStr CurrKeyConfKeySection;
  static VStr CurrKeyConfWeaponSection;
  static void (*onShowCompletionMatch) (bool isheader, VStr s);

  // will be added before real CLI console commands
  static VStr cliPreCmds;

public:
  VCommand (const char *);
  virtual ~VCommand ();

  virtual void Run () = 0;

  // return non-empty string to replace arg
  // note that aidx can be equal to `args.length()`, and will never be 0!
  // args[0] is a command itself
  // return string ends with space to move to next argument
  virtual VStr AutoCompleteArg (const TArray<VStr> &args, int aidx);

  static void Init ();
  static void InsertCLICommands (); // should be called after loading startup scripts
  static void WriteAlias (VStream *st);
  static void Shutdown ();
  static void ProcessKeyConf ();

  static void AddToAutoComplete (const char *string);
  static void RemoveFromAutoComplete (const char *string);
  static VStr GetAutoComplete (VStr prefix);

  // returns empty string if no matches found, or list is empty
  // if there is one exact match, return it with trailing space
  // otherwise, return longest prefix
  // if longest prefix is the same as input prefix, show all matches
  // case-insensitive
  // if `unchangedAsEmpty` is `true`, return empty string if result is equal to input prefix
  static VStr AutoCompleteFromList (VStr prefix, const TArray <VStr> &list, bool unchangedAsEmpty=false, bool doSortHint=true);

  enum {
    CT_UNKNOWN = 0,
    CT_COMMAND,
    CT_CVAR,
    CT_ALIAS,
  };
  static int GetCommandType (VStr cmd);

  static void ExecuteString (VStr, ECmdSource, VBasePlayer *);
  static void ForwardToServer ();
  static int CheckParm (const char *);

  static int GetArgC ();
  static VStr GetArgV (int idx);

  friend class VCmdBuf;
  friend class TCmdCmdList;
  friend class TCmdAlias;
};


// ////////////////////////////////////////////////////////////////////////// //
// macro for declaring a console command
#define COMMAND(name) \
static class TCmd ## name : public VCommand { \
public: \
  TCmd ## name() : VCommand(#name) { rebuildCache = true; } \
  virtual void Run () override; \
} name ## _f; \
\
void TCmd ## name::Run ()


#define COMMAND_WITH_AC(name) \
static class TCmd ## name : public VCommand { \
public: \
  TCmd ## name() : VCommand(#name) { rebuildCache = true; } \
  virtual void Run () override; \
  virtual VStr AutoCompleteArg (const TArray<VStr> &args, int aidx) override; \
} name ## _f; \
\
void TCmd ## name::Run()

#define COMMAND_AC(name) \
VStr TCmd ## name::AutoCompleteArg (const TArray<VStr> &args, int aidx)

// usage:
//   COMMAND_AC_SIMPLE_LIST(TeleportNewMap, "forced")
#define COMMAND_AC_SIMPLE_LIST(name,...) \
VStr TCmd ## name::AutoCompleteArg (const TArray<VStr> &args, int aidx) { \
  VStr prefix = (aidx < args.length() ? args[aidx] : VStr()); \
  if (aidx == 1) { \
    /*static*/ const char *acstrings[] = { __VA_ARGS__ , nullptr }; \
    TArray<VStr> list; \
    for (const char **ssp = acstrings; *ssp; ++ssp) list.append(VStr(*ssp)); \
    return AutoCompleteFromList(prefix, list, true); /* return unchanged as empty */ \
  } else { \
    return VStr::EmptyString; \
  } \
}


// ////////////////////////////////////////////////////////////////////////// //
// a command buffer
class VCmdBuf {
private:
  VStr Buffer;
  double WaitEndTime;

private:
  // returns `false` if wait expired
  bool checkWait ();

public:
  VCmdBuf () : WaitEndTime(0) {}
  void Insert (const char *);
  void Insert (VStr);
  void Print (const char *);
  void Print (VStr);
  void Exec ();

  inline VCmdBuf &operator << (const char *data) {
    Print(data);
    return *this;
  }

  inline VCmdBuf &operator << (VStr data) {
    Print(data);
    return *this;
  }

  friend class TCmdWait;
};

// main command buffer
extern VCmdBuf GCmdBuf;
