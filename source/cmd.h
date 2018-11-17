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

// a console command
class VCommand {
public:
  enum ECmdSource {
    SRC_Command,
    SRC_Client,
  };

private:
  // console command alias
  struct VAlias {
    VStr Name;
    VStr CmdLine;
    VAlias *Next;
    bool Save;
  };

  const char *Name;
  VCommand *Next;

  static bool Initialised;
  static VStr Original;

  static TArray<VStr> AutoCompleteTable;

  static VCommand *Cmds;
  static VAlias *Alias;

  static void TokeniseString (const VStr &);

protected:
  static TArray<VStr> Args;
  static ECmdSource Source;
  static VBasePlayer *Player; // for SRC_Client

public:
  static bool ParsingKeyConf;
  static void (*onShowCompletionMatch) (bool isheader, const VStr &s);

public:
  VCommand (const char *);
  virtual ~VCommand ();

  virtual void Run () = 0;

  static void Init ();
  static void WriteAlias (FILE *);
  static void Shutdown ();
  static void ProcessKeyConf ();

  static void AddToAutoComplete (const char *);
  static VStr GetAutoComplete (const VStr &prefix);

  static void ExecuteString (const VStr &, ECmdSource, VBasePlayer *);
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
static class TCmd ## name : public VCommand \
{ \
public: \
  TCmd ## name() : VCommand(#name) { } \
  virtual void Run () override; \
} name ## _f; \
\
void TCmd ## name::Run()


// ////////////////////////////////////////////////////////////////////////// //
// a command buffer
class VCmdBuf {
private:
  VStr Buffer;
  bool Wait;

public:
  VCmdBuf () : Wait(false) {}
  void Insert (const char *);
  void Insert (const VStr &);
  void Print (const char *);
  void Print (const VStr &);
  void Exec ();

  inline VCmdBuf &operator << (const char *data) {
    Print(data);
    return *this;
  }

  inline VCmdBuf &operator << (const VStr &data) {
    Print(data);
    return *this;
  }

  friend class TCmdWait;
};

// main command buffer
extern VCmdBuf GCmdBuf;
