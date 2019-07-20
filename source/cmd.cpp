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
#include "gamedefs.h"
#include "net/network.h"
#ifdef CLIENT
# include "drawer.h"
#endif


VCmdBuf GCmdBuf;

bool VCommand::ParsingKeyConf;

bool VCommand::Initialised = false;
VStr VCommand::Original;

bool VCommand::rebuildCache = true;
TMapDtor<VStr, VCommand *> VCommand::locaseCache;

TArray<VStr> VCommand::Args;
VCommand::ECmdSource VCommand::Source;
VBasePlayer *VCommand::Player;

TArray<VStr> VCommand::AutoCompleteTable;
static TMapNC<VStr, bool> AutoCompleteTableBSet; // quicksearch

VCommand *VCommand::Cmds = nullptr;
//VCommand::VAlias *VCommand::Alias = nullptr;
TArray<VCommand::VAlias> VCommand::AliasList;
TMap<VStr, int> VCommand::AliasMap;

bool VCommand::cliInserted = false;
VStr VCommand::cliPreCmds;

void (*VCommand::onShowCompletionMatch) (bool isheader, const VStr &s);

static const char *KeyConfCommands[] = {
  "alias",
  "bind",
  "defaultbind",
  "addkeysection",
  "addmenukey",
  "addslotdefault",
  "weaponsection",
  "setslot",
  "addplayerclass",
  "clearplayerclasses"
};


extern "C" {
  static int sortCmpVStrCI (const void *a, const void *b, void *udata) {
    if (a == b) return 0;
    VStr *sa = (VStr *)a;
    VStr *sb = (VStr *)b;
    return sa->ICmp(*sb);
  }
}


//**************************************************************************
//
//  Commands, alias
//
//**************************************************************************

//==========================================================================
//
//  CheatAllowed
//
//==========================================================================
static bool CheatAllowed (VBasePlayer *Player, bool allowDead=false) {
  if (!Player) return false;
  if (sv.intermission) {
    Player->Printf("You are not in game!");
    return false;
  }
  if (GGameInfo->NetMode >= NM_DedicatedServer) {
    Player->Printf("You cannot cheat in a network game!");
    return false;
  }
  if (GGameInfo->WorldInfo->Flags&VWorldInfo::WIF_SkillDisableCheats) {
    Player->Printf("You are too good to cheat!");
    //k8: meh, if i want to cheat, i want to cheat!
    //return false;
    return true;
  }
  if (!allowDead && Player->Health <= 0) {
    // dead players can't cheat
    Player->Printf("You must be alive to cheat");
    return false;
  }
  return true;
}


//==========================================================================
//
//  VCommand::VCommand
//
//==========================================================================
VCommand::VCommand (const char *name) {
  Next = Cmds;
  Name = name;
  Cmds = this;
  if (Initialised) AddToAutoComplete(Name);
}


//==========================================================================
//
//  VCommand::~VCommand
//
//==========================================================================
VCommand::~VCommand () {
}


//==========================================================================
//
//  VCommand::AutoCompleteArg
//
//  return non-empty string to replace arg
//
//==========================================================================
VStr VCommand::AutoCompleteArg (const TArray<VStr> &args, int aidx) {
  return VStr::EmptyString;
}


//==========================================================================
//
//  VCommand::Init
//
//==========================================================================
void VCommand::Init () {
  for (VCommand *cmd = Cmds; cmd; cmd = cmd->Next) AddToAutoComplete(cmd->Name);

  // add configuration file execution
  GCmdBuf.Insert("exec startup.vs\n__run_cli_commands__\n");

  Initialised = true;
}


//==========================================================================
//
//  VCommand::InsertCLICommands
//
//==========================================================================
void VCommand::InsertCLICommands () {
  VStr cstr;

  if (!cliPreCmds.isEmpty()) {
    cstr = cliPreCmds;
    if (!cstr.endsWith("\n")) cstr += '\n';
    cliPreCmds.clear();
  }

  // add console commands from command line
  // these are params, that start with + and continue until the end or until next param that starts with - or +
  bool in_cmd = false;
  for (int i = 1; i < GArgs.Count(); ++i) {
    if (in_cmd) {
      // check for number
      if (GArgs[i] && (GArgs[i][0] == '-' || GArgs[i][0] == '+')) {
        float v;
        if (VStr::convertFloat(GArgs[i], &v)) {
          cstr += ' ';
          cstr += '"';
          cstr += VStr(GArgs[i]).quote();
          cstr += '"';
          continue;
        }
      }
      if (!GArgs[i] || GArgs[i][0] == '-' || GArgs[i][0] == '+') {
        in_cmd = false;
        //GCmdBuf << "\n";
        cstr += '\n';
      } else {
        //GCmdBuf << " \"" << VStr(GArgs[i]).quote() << "\"";
        cstr += ' ';
        cstr += '"';
        cstr += VStr(GArgs[i]).quote();
        cstr += '"';
        continue;
      }
    }
    if (GArgs[i][0] == '+') {
      in_cmd = true;
      //GCmdBuf << (GArgs[i]+1);
      cstr += (GArgs[i]+1);
    }
  }
  //if (in_cmd) GCmdBuf << "\n";
  if (in_cmd) cstr += '\n';

  //GCon->Logf("===\n%s\n===", *cstr);

  if (!cstr.isEmpty()) GCmdBuf.Insert(cstr);
}


// ////////////////////////////////////////////////////////////////////////// //
extern "C" {
  static int vapcmp (const void *aa, const void *bb, void *udata) {
    const VCommand::VAlias *a = *(const VCommand::VAlias **)aa;
    const VCommand::VAlias *b = *(const VCommand::VAlias **)bb;
    if (a == b) return 0;
    return a->Name.ICmp(b->Name);
  }

  static int vstrptrcmpci (const void *aa, const void *bb, void *udata) {
    const VStr *a = (const VStr *)aa;
    const VStr *b = (const VStr *)bb;
    if (a == b) return 0;
    return a->ICmp(*b);
  }
}


//==========================================================================
//
//  VCommand::WriteAlias
//
//==========================================================================
void VCommand::WriteAlias (VStream *st) {
  // build list
  TArray<VAlias *> alist;
  for (auto &&al : AliasList) if (al.Save) alist.append(&al);
  if (alist.length() == 0) return;
  // sort list
  timsort_r(alist.ptr(), alist.length(), sizeof(VAlias *), &vapcmp, nullptr);
  // write list
  for (auto &&al : alist) {
    st->writef("alias %s \"%s\"\n", *al->Name, *al->CmdLine.quote());
  }
}


//==========================================================================
//
//  VCommand::Shutdown
//
//==========================================================================
void VCommand::Shutdown () {
  AliasMap.clear();
  AliasList.clear();
  AutoCompleteTable.Clear();
  AutoCompleteTableBSet.clear();
  Args.Clear();
  Original.Clean();
}


//==========================================================================
//
//  VCommand::ProcessKeyConf
//
//==========================================================================
void VCommand::ProcessKeyConf () {
  // enable special mode for console commands
  ParsingKeyConf = true;

  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) == NAME_keyconf) {
      // read it
      VStream *Strm = W_CreateLumpReaderNum(Lump);
      VStr buf;
      buf.setLength(Strm->TotalSize(), 0);
      Strm->Serialize(buf.getMutableCStr(), buf.length());
      if (Strm->IsError()) buf.clear();
      delete Strm;

      // parse it
      VCmdBuf CmdBuf;
      TArray<VStr> lines;
      TArray<VStr> args;
      buf.split('\n', lines);
      for (auto &&s : lines) {
        s = s.xstrip();
        if (s.length() == 0 || s[0] == '#' || s[0] == '/') continue;
        args.reset();
        s.tokenise(args);
        if (args.length() == 0) continue;
        if (!args[0].strEquCI("ClearPlayerClasses") && !args[0].strEquCI("AddPlayerClass") && !args[0].strEquCI("alias")) {
          if (!args[0].strEquCI("addkeysection") &&
              !args[0].strEquCI("addmenukey") &&
              /*!args[0].strEquCI("alias") &&*/
              !args[0].strEquCI("defaultbind"))
          {
            GCon->Logf(NAME_Warning, "ignored keyconf command: %s", *s);
          }
        } else {
          CmdBuf << s << "\n";
        }
      }
      CmdBuf.Exec();
    }
  }

  // back to normal console command execution
  ParsingKeyConf = false;
}


//==========================================================================
//
//  VCommand::AddToAutoComplete
//
//==========================================================================
void VCommand::AddToAutoComplete (const char *string) {
  if (!string || !string[0] || string[0] == '_') return;

  VStr vs(string);
  VStr vslow = vs.toLowerCase();

  if (AutoCompleteTableBSet.has(vslow)) return;
  /*
  for (int i = 0; i < AutoCompleteTable.length(); ++i) {
    if (AutoCompleteTable[i].ICmp(string) == 0) return; //Sys_Error("C_AddToAutoComplete: %s is allready registered.", string);
  }
  */

  AutoCompleteTableBSet.put(vslow, true);
  AutoCompleteTable.Append(vs);

  // alphabetic sort
  for (int i = AutoCompleteTable.Num()-1; i && AutoCompleteTable[i-1].ICmp(AutoCompleteTable[i]) > 0; --i) {
    VStr swap = AutoCompleteTable[i];
    AutoCompleteTable[i] = AutoCompleteTable[i-1];
    AutoCompleteTable[i-1] = swap;
  }
}


//==========================================================================
//
//  VCommand::AutoCompleteFromList
//
//==========================================================================
VStr VCommand::AutoCompleteFromList (const VStr &prefix, const TArray <VStr> &list, bool unchangedAsEmpty, bool doSortHint) {
  if (list.length() == 0) return (unchangedAsEmpty ? VStr::EmptyString : prefix);

  VStr bestmatch;
  int matchcount = 0;

  // first, get longest match
  for (int f = 0; f < list.length(); ++f) {
    VStr mt = list[f];
    if (mt.length() < prefix.length()) continue;
    if (VStr::NICmp(*prefix, *mt, prefix.length()) != 0) continue;
    ++matchcount;
    if (bestmatch.length() < mt.length()) bestmatch = mt;
  }

  if (matchcount == 0) return (unchangedAsEmpty ? VStr::EmptyString : prefix); // alas
  if (matchcount == 1) { bestmatch += " "; return bestmatch; } // done

  // trim match
  for (int f = 0; f < list.length(); ++f) {
    VStr mt = list[f];
    if (mt.length() < prefix.length()) continue;
    if (VStr::NICmp(*prefix, *mt, prefix.Length()) != 0) continue;
    // cannot be longer than this
    if (bestmatch.length() > mt.length()) bestmatch = bestmatch.left(mt.length());
    int mlpos = 0;
    while (mlpos < bestmatch.length()) {
      if (VStr::upcase1251(bestmatch[mlpos]) != VStr::upcase1251(mt[mlpos])) {
        bestmatch = bestmatch.left(mlpos);
        break;
      }
      ++mlpos;
    }
  }

  // if match equals to prefix, this is second tab tap, so show all possible matches
  if (bestmatch == prefix) {
    // show all possible matches
    if (onShowCompletionMatch) {
      onShowCompletionMatch(true, "=== possible matches ===");
      bool skipPrint = false;
      if (doSortHint && list.length() > 1) {
        bool needSorting = false;
        for (int f = 1; f < list.length(); ++f) if (list[f-1].ICmp(list[f]) > 0) { needSorting = true; break; }
        if (needSorting) {
          TArray<VStr> sortedlist;
          sortedlist.resize(list.length());
          for (int f = 0; f < list.length(); ++f) sortedlist.append(list[f]);
          timsort_r(sortedlist.ptr(), sortedlist.length(), sizeof(VStr), &vstrptrcmpci, nullptr);
          for (int f = 0; f < sortedlist.length(); ++f) {
            VStr mt = sortedlist[f];
            if (mt.length() < prefix.length()) continue;
            if (VStr::NICmp(*prefix, *mt, prefix.Length()) != 0) continue;
            onShowCompletionMatch(false, mt);
          }
          skipPrint = true;
        }
      }
      if (!skipPrint) {
        for (int f = 0; f < list.length(); ++f) {
          VStr mt = list[f];
          if (mt.length() < prefix.length()) continue;
          if (VStr::NICmp(*prefix, *mt, prefix.Length()) != 0) continue;
          onShowCompletionMatch(false, mt);
        }
      }
    }
    return (unchangedAsEmpty ? VStr::EmptyString : prefix);
  }

  // found extended match
  return bestmatch;
}


//==========================================================================
//
//  findPlayer
//
//==========================================================================
static VBasePlayer *findPlayer () {
  if (sv.intermission) return nullptr;
  if (GGameInfo->NetMode < NM_Standalone) return nullptr; // not playing
  // find any active player
  for (int f = 0; f < MAXPLAYERS; ++f) {
    VBasePlayer *plr = GGameInfo->Players[f];
    if (!plr) continue;
    if ((plr->PlayerFlags&VBasePlayer::PF_IsBot) ||
        !(plr->PlayerFlags&VBasePlayer::PF_Spawned))
    {
      continue;
    }
    if (plr->PlayerState != PST_LIVE || plr->Health <= 0) continue;
    return plr;
  }
  return nullptr;
}


//==========================================================================
//
//  VCommand::GetAutoComplete
//
//  if returned string ends with space, this is the only match
//
//==========================================================================
VStr VCommand::GetAutoComplete (const VStr &prefix) {
  if (prefix.length() == 0) return prefix; // oops

  TArray<VStr> args;
  prefix.tokenize(args);
  int aidx = args.length();
  if (aidx == 0) return prefix; // wtf?!

  bool endsWithBlank = ((vuint8)prefix[prefix.length()-1] <= ' ');

  if (aidx == 1 && !endsWithBlank) {
    VBasePlayer *plr = findPlayer();
    if (plr) {
      auto otbllen = AutoCompleteTable.length();
      plr->ListConCommands(AutoCompleteTable, prefix);
      //GCon->Logf("***PLR: pfx=<%s>; found=%d", *prefix, AutoCompleteTable.length()-otbllen);
      if (AutoCompleteTable.length() > otbllen) {
        // copy and sort
        TArray<VStr> newlist;
        newlist.setLength(AutoCompleteTable.length());
        for (int f = 0; f < AutoCompleteTable.length(); ++f) newlist[f] = AutoCompleteTable[f];
        AutoCompleteTable.setLength(otbllen, false); // don't resize
        timsort_r(newlist.ptr(), newlist.length(), sizeof(VStr), &sortCmpVStrCI, nullptr);
        return AutoCompleteFromList(prefix, newlist);
      }
    }
    return AutoCompleteFromList(prefix, AutoCompleteTable);
  }

  // autocomplete new arg?
  if (aidx > 1 && !endsWithBlank) --aidx; // nope, last arg

  // check for command
  if (rebuildCache) rebuildCommandCache();
  VStr loname = args[0].toLowerCase();
  auto cptr = locaseCache.find(loname);
  if (cptr) {
    VCommand *cmd = *cptr;
    VStr ac = cmd->AutoCompleteArg(args, aidx);
    if (ac.length()) {
      // autocompleted, rebuild string
      //if (aidx < args.length()) args[aidx] = ac; else args.append(ac);
      bool addSpace = ((vuint8)ac[ac.length()-1] <= ' ');
      if (addSpace) ac.chopRight(1);
      VStr res;
      for (int f = 0; f < aidx; ++f) {
        res += args[f].quote(true); // add quote chars if necessary
        res += ' ';
      }
      if (ac.length()) {
        ac = ac.quote(true);
        if (!addSpace && ac[ac.length()-1] == '"') ac.chopRight(1);
      }
      res += ac;
      if (addSpace) res += ' ';
      return res;
    }
    // cannot complete, nothing's changed
    return prefix;
  }

  // try player
  {
    VBasePlayer *plr = findPlayer();
    if (plr) {
      TArray<VStr> aclist;
      if (plr->ExecConCommandAC(args, endsWithBlank, aclist)) {
        if (aclist.length() == 0) return prefix; // nothing's found
        // rebuild string
        VStr res;
        for (int f = 0; f < aidx; ++f) {
          res += args[f].quote(true); // add quote chars if necessary
          res += ' ';
        }
        // several matches
        // sort
        timsort_r(aclist.ptr(), aclist.length(), sizeof(VStr), &sortCmpVStrCI, nullptr);
        //for (int f = 0; f < aclist.length(); ++f) GCon->Logf(" %d:<%s>", f, *aclist[f]);
        VStr ac = AutoCompleteFromList((endsWithBlank ? VStr() : args[args.length()-1]), aclist);
        bool addSpace = ((vuint8)ac[ac.length()-1] <= ' ');
        if (addSpace) ac.chopRight(1);
        if (ac.length()) {
          ac = ac.quote(true);
          if (!addSpace && ac[ac.length()-1] == '"') ac.chopRight(1);
        }
        if (ac.length()) {
          res += ac;
          if (addSpace) res += ' ';
        }
        return res;
      }
    }
  }

  // Cvar
  if (aidx == 1) {
    // show cvar help, why not?
    if (onShowCompletionMatch) {
      VCvar *var = VCvar::FindVariable(*args[0]);
      if (var) {
        VStr help = var->GetHelp();
        if (help.length() && !VStr(help).startsWithNoCase("no help yet")) {
          onShowCompletionMatch(false, args[0]+": "+help);
        }
      }
    }
  }

  // nothing's found
  return prefix;
}


//**************************************************************************
//
//  Parsing of a command, command arg handling
//
//**************************************************************************

//==========================================================================
//
//  VCommand::TokeniseString
//
//==========================================================================
void VCommand::TokeniseString (const VStr &str) {
  Original = str;
  Args.reset();
  str.tokenize(Args);
}


//==========================================================================
//
//  VCommand::rebuildCommandCache
//
//==========================================================================
void VCommand::rebuildCommandCache () {
  locaseCache.clear();
  for (VCommand *cmd = Cmds; cmd; cmd = cmd->Next) {
    VStr loname = VStr(cmd->Name).toLowerCase();
    locaseCache.put(loname, cmd);
  }
}


//==========================================================================
//
//  VCommand::ExecuteString
//
//==========================================================================
void VCommand::ExecuteString (const VStr &Acmd, ECmdSource src, VBasePlayer *APlayer) {
  //fprintf(stderr, "+++ command BEFORE tokenizing: <%s>\n", *Acmd);
  TokeniseString(Acmd);
  Source = src;
  Player = APlayer;

  //fprintf(stderr, "+++ command argc=%d (<%s>)\n", Args.length(), *Acmd);
  //for (int f = 0; f < Args.length(); ++f) fprintf(stderr, "  #%d: <%s>\n", f, *Args[f]);

  if (!Args.Num()) return;

  if (Args[0] == "__run_cli_commands__") {
    FL_ProcessPreInits(); // override configs
    FL_ClearPreInits();
    if (!cliInserted) {
#ifdef CLIENT
      if (!Drawer || !Drawer->IsInited()) {
        GCmdBuf.Insert("wait\n__run_cli_commands__\n");
        return;
      }
#endif
      cliInserted = true;
      InsertCLICommands();
    }
    return;
  }

  if (ParsingKeyConf) {
    // verify that it's a valid keyconf command
    bool Found = false;
    for (unsigned i = 0; i < ARRAY_COUNT(KeyConfCommands); ++i) {
      if (Args[0].strEquCI(KeyConfCommands[i])) {
        Found = true;
        break;
      }
    }
    if (!Found) {
      GCon->Logf(NAME_Warning, "Invalid KeyConf command: %s", *Acmd);
      return;
    }
  }

  // check for command
  if (rebuildCache) rebuildCommandCache();
  VStr loname = Args[0].toLowerCase();
  auto cptr = locaseCache.find(loname);
  if (cptr) {
    (*cptr)->Run();
    return;
  }

  // check for player command
  if (Source == SRC_Command) {
    VBasePlayer *plr = findPlayer();
    if (plr && plr->IsConCommand(Args[0])) {
      ForwardToServer();
      return;
    }
  } else if (Player && Player->IsConCommand(Args[0])) {
    if (CheatAllowed(Player)) Player->ExecConCommand();
    return;
  }

  // Cvar
  if (FL_HasPreInit(Args[0])) return;
  // this hack allows to set cheating variables from command line or autoexec
  {
    bool oldCheating = VCvar::GetCheating();
    VBasePlayer *plr = findPlayer();
    if (!plr) VCvar::SetCheating(VCvar::GetBool("sv_cheats"));
    //GCon->Logf("sv_cheats: %d; plr is %shere", (int)VCvar::GetBool("sv_cheats"), (plr ? "" : "not "));
    bool doneCvar = VCvar::Command(Args);
    if (!plr) VCvar::SetCheating(oldCheating);
    if (doneCvar) {
      if (!plr) VCvar::Unlatch();
      return;
    }
  }

  // command defined with ALIAS
  /*
  for (VAlias *a = Alias; a; a = a->Next) {
    if (!Args[0].ICmp(a->Name)) {
      GCmdBuf.Insert("\n");
      GCmdBuf.Insert(a->CmdLine);
      return;
    }
  }
  */
  //FIXME: make it better (do not alloc new locased string)
  if (Args[0].length()) {
    VStr lcn = Args[0].toLowerCase();
    auto idp = AliasMap.find(lcn);
    if (idp) {
      VAlias &al = AliasList[*idp];
      GCmdBuf.Insert("\n");
      GCmdBuf.Insert(al.CmdLine);
      return;
    }
  }

  // unknown command
#ifndef CLIENT
  if (host_initialised)
#endif
    GCon->Logf("Unknown command '%s'", *Args[0]);
}


//==========================================================================
//
//  VCommand::ForwardToServer
//
//==========================================================================
void VCommand::ForwardToServer () {
#ifdef CLIENT
  if (!cl) {
    GCon->Log("You must be in a game to execute this command");
    return;
  }
  if (cl->Net) {
    //fprintf(stderr, "*** sending over the network: <%s>\n", *Original);
    cl->Net->SendCommand(Original);
  } else {
    //fprintf(stderr, "*** local executing: <%s>\n", *Original);
    VCommand::ExecuteString(Original, VCommand::SRC_Client, cl);
  }
#endif
}


//==========================================================================
//
//  VCommand::CheckParm
//
//==========================================================================
int VCommand::CheckParm (const char *check) {
  for (int i = 1; i < Args.Num(); ++i) {
    if (Args[i].ICmp(check) == 0) return i;
  }
  return 0;
}


//==========================================================================
//
//  VCommand::GetArgC
//
//==========================================================================
int VCommand::GetArgC () {
  return Args.Num();
}


//==========================================================================
//
//  VCommand::GetArgV
//
//==========================================================================
VStr VCommand::GetArgV (int idx) {
  if (idx < 0 || idx >= Args.Num()) return VStr();
  return Args[idx];
}


//**************************************************************************
//
//  Command buffer
//
//**************************************************************************

//==========================================================================
//
//  VCmdBuf::Insert
//
//==========================================================================
void VCmdBuf::Insert (const char *text) {
  Buffer = VStr(text)+Buffer;
}


//==========================================================================
//
//  VCmdBuf::Insert
//
//==========================================================================
void VCmdBuf::Insert (const VStr &text) {
  Buffer = text+Buffer;
}


//==========================================================================
//
//  VCmdBuf::Print
//
//==========================================================================
void VCmdBuf::Print (const char *data) {
  Buffer += data;
}


//==========================================================================
//
//  VCmdBuf::Print
//
//==========================================================================
void VCmdBuf::Print (const VStr &data) {
  Buffer += data;
}


//==========================================================================
//
//  VCmdBuf::Exec
//
//==========================================================================
void VCmdBuf::Exec () {
  int len;
  int quotes;
  bool comment;
  VStr ParsedCmd;

  do {
    quotes = 0;
    comment = false;
    ParsedCmd.Clean();

    for (len = 0; len < Buffer.length(); ++len) {
      if (Buffer[len] == '\n') break;
      if (comment) continue;
      if (Buffer[len] == ';' && !(quotes&1)) break;
      if (Buffer[len] == '/' && Buffer[len+1] == '/' && !(quotes&1)) {
        // comment, all till end is ignored
        comment = true;
        continue;
      }
      // screened char in string?
      if ((quotes&1) && Buffer[len] == '\\' && Buffer.length()-len > 1 && (Buffer[len+1] == '"' || Buffer[len+1] == '\\')) {
        ParsedCmd += Buffer[len++];
      } else {
        if (Buffer[len] == '\"') ++quotes;
      }
      ParsedCmd += Buffer[len];
    }

    if (len < Buffer.Length()) ++len; // skip seperator symbol

    Buffer = VStr(Buffer, len, Buffer.Length()-len);

    VCommand::ExecuteString(ParsedCmd, VCommand::SRC_Command, nullptr);

    if (host_request_exit) return;

    if (Wait) {
      // skip out while text still remains in buffer, leaving it for next frame
      Wait = false;
      break;
    }
  } while (len);
}


//**************************************************************************
//
//  Some commands
//
//**************************************************************************

//==========================================================================
//
//  COMMAND CmdList
//
//==========================================================================
COMMAND(CmdList) {
  const char *prefix = (Args.Num() > 1 ? *Args[1] : "");
  int pref_len = VStr::Length(prefix);
  int count = 0;
  for (VCommand *cmd = Cmds; cmd; cmd = cmd->Next) {
    if (pref_len && VStr::NICmp(cmd->Name, prefix, pref_len)) continue;
    GCon->Logf(" %s", cmd->Name);
    ++count;
  }
  GCon->Logf("%d commands.", count);
}


//==========================================================================
//
//  VCommand::rebuildAliasMap
//
//==========================================================================
void VCommand::rebuildAliasMap () {
  AliasMap.reset();
  for (auto &&it : AliasList.itemsIdx()) {
    VAlias &al = it.value();
    VStr aliasName = al.Name.toLowerCase();
    if (aliasName.length() == 0) continue; // just in case
    AliasMap.put(aliasName, it.index());
  }
}


//==========================================================================
//
//  Alias_f
//
//==========================================================================
COMMAND(Alias) {
  if (Args.length() == 1) {
    GCon->Logf("\034K%s", "Current aliases:");
    for (auto &&al : AliasList) {
      GCon->Logf("\034D  %s: %s%s", *al.Name, *al.CmdLine, (al.Save ? "" : " (temporary)"));
    }
    return;
  }

  VStr aliasName = Args[1].toLowerCase();
  auto idxp = AliasMap.find(aliasName);

  if (Args.length() == 2) {
    if (!idxp) {
      GCon->Logf("no named alias '%s' found", *Args[1]);
    } else {
      const VAlias &al = AliasList[*idxp];
      GCon->Logf("alias %s: %s%s", *Args[1], *al.CmdLine, (al.Save ? "" : " (temporary)"));
    }
    return;
  }

  VStr cmd;
  for (int f = 2; f < Args.length(); ++f) {
    if (Args[f].isEmpty()) continue;
    if (cmd.length()) cmd += ' ';
    cmd += Args[f];
  }
  cmd = cmd.xstrip(); // why not?

  if (cmd.isEmpty()) {
    if (Args.length() != 3 || !Args[2].isEmpty()) {
      GCon->Logf("invalid alias defintion for '%s' (empty command)", *Args[1]);
      return;
    }
    // remove alias
    if (!idxp) {
      GCon->Logf("no named alias '%s' found", *Args[1]);
    } else {
      VAlias &al = AliasList[*idxp];
      if (ParsingKeyConf && al.Save) {
        GCon->Logf("cannot remove permanent alias '%s' from keyconf", *Args[1]);
      } else {
        AliasList.removeAt(*idxp);
        rebuildAliasMap();
        GCon->Logf("removed alias '%s'", *Args[1]);
      }
    }
    return;
  }

  if (idxp) {
    // redefine alias
    VAlias &al = AliasList[*idxp];
    if (ParsingKeyConf && al.Save) {
      // add new temporary alias below
    } else {
      // redefine alias
      al.CmdLine = cmd;
      GCon->Logf("redefined alias '%s'", *Args[1]);
      return;
    }
  }

  // new alias
  VAlias &nal = AliasList.alloc();
  nal.Name = Args[1];
  nal.CmdLine = cmd;
  nal.Save = !ParsingKeyConf;

  if (ParsingKeyConf) {
    GCon->Logf("defined %salias '%s': %s", (ParsingKeyConf ? "temporary " : ""), *Args[1], *cmd);
  }

  rebuildAliasMap();
}


//==========================================================================
//
//  Echo_f
//
//==========================================================================
COMMAND(Echo) {
  if (Args.Num() < 2) return;

  VStr Text = Args[1];
  for (int i = 2; i < Args.Num(); ++i) {
    Text += " ";
    Text += Args[i];
  }
  Text = Text.EvalEscapeSequences();
#ifdef CLIENT
  if (cl) {
    cl->Printf("%s", *Text);
  }
  else
#endif
  {
    GCon->Log(Text);
  }
}


//==========================================================================
//
//  Exec_f
//
//==========================================================================
COMMAND(Exec) {
  if (Args.length() < 2 || Args.length() > 3) {
    GCon->Log("Exec <filename> : execute script file");
    return;
  }

  VStream *Strm = nullptr;

  // try disk file
  VStr dskname = Host_GetConfigDir()+"/"+Args[1];
  if (Sys_FileExists(dskname)) {
    Strm = FL_OpenSysFileRead(dskname);
    if (Strm) GCon->Logf("Executing '%s'...", *dskname);
  }

  // try wad file
  if (!Strm && FL_FileExists(Args[1])) {
    Strm = FL_OpenFileRead(Args[1]);
    if (Strm) {
      GCon->Logf("Executing '%s'...", *Args[1]);
      //GCon->Logf("<%s>", *Strm->GetName());
    }
  }

  if (!Strm) {
    if (Args.length() == 2) GCon->Logf("Can't find '%s'...", *Args[1]);
    return;
  }

  //GCon->Logf("Executing '%s'...", *Args[1]);

  int flsize = Strm->TotalSize();
  if (flsize == 0) { delete Strm; return; }

  char *buf = new char[flsize+2];
  Strm->Serialise(buf, flsize);
  if (Strm->IsError()) {
    delete Strm;
    delete[] buf;
    GCon->Logf("Error reading '%s'!", *Args[1]);
    return;
  }
  delete Strm;

  if (buf[flsize-1] != '\n') buf[flsize++] = '\n';
  buf[flsize] = 0;

  GCmdBuf.Insert(buf);

  delete[] buf;
}


//==========================================================================
//
//  COMMAND Wait
//
//==========================================================================
COMMAND(Wait) {
  GCmdBuf.Wait = true;
}


//==========================================================================
//
//  __k8_run_first_map
//
//  used for "-k8runmap" if mapinfo found
//
//==========================================================================
COMMAND(__k8_run_first_map) {
  if (P_GetNumEpisodes() == 0) {
    GCon->Logf("ERROR: No eposode info found!");
    return;
  }

  VEpisodeDef *edef = P_GetEpisodeDef(0);

  VName startMap = edef->Name;
  if (edef->TeaserName != NAME_None && !IsMapPresent(startMap)) startMap = edef->TeaserName;

  if (startMap == NAME_None) {
    GCon->Logf("ERROR: Starting map not found!");
    return;
  }

  GCmdBuf.Insert(va("map \"%s\"\n", *VStr(startMap).quote()));
}
