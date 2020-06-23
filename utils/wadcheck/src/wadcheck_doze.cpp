//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2019-2020 Ketmar Dark
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "libs/core.h"

#include "wdb.h"


// ////////////////////////////////////////////////////////////////////////// //
static bool longReport = false;


//==========================================================================
//
//  usage
//
//==========================================================================
static __attribute__((noreturn)) void usage () {
  GLog.Write("%s",
    "USAGE:\n"
    "  wadcheck [--db dbfilename] --register wad/pk3\n"
    "  wadcheck [--db dbfilename] wad/pk3\n"
    "  wadcheck [--db dbfilename] --info\n"
    "use `--long` option to show matches on separate lines\n"
    "");
  Z_Exit(1);
}


//==========================================================================
//
//  showDBWadList
//
//  show list of wads in database
//
//==========================================================================
static void showDBWadList () {
  //GLog.Logf("database file: %s", *dbname);
  GLog.Log("registered database wads:");
  for (auto &&wad : wadnames) GLog.Logf("  %s", *wad);
}


//==========================================================================
//
//  main
//
//==========================================================================
int main (int argc, char **argv) {
  GLog.Logf("WADCHECK build date: %s  %s", __DATE__, __TIME__);

  VStr dbname;
  bool doneOptions = false;
  bool doRegisterWads = false;
  bool wantInfo = false;
  TArray<VStr> flist;

  for (int f = 1; f < argc; ++f) {
    VStr arg = VStr(argv[f]);
    if (arg.isEmpty()) continue;
    if (doneOptions || arg[0] != '-') {
      bool found = false;
      for (int c = 0; c < flist.length(); ++c) {
        int cres;
        cres = arg.Cmp(flist[c]);
        if (cres == 0) { found = true; break; }
      }
      if (!found) flist.append(arg);
      continue;
    }
    if (arg == "--") { doneOptions = true; continue; }
    // "--register"
    if (arg.strEquCI("-r") || arg.strEquCI("-register") || arg.strEquCI("--register")) {
      if (flist.length() != 0) Sys_Error("cannot both register and check files, do that sequentially, please!");
      doRegisterWads = true;
      continue;
    }
    // "--db"
    if (arg.strEquCI("-db") || arg.strEquCI("--db")) {
      if (f >= argc) Sys_Error("missing database name");
      if (!dbname.isEmpty()) Sys_Error("duplicate database name");
      dbname = VStr(argv[++f]);
      dbname = dbname.DefaultExtension(".wdb");
      continue;
    }
    if (arg.strEquCI("-long") || arg.strEquCI("--long")) {
      longReport = true;
      continue;
    }
    if (arg.strEquCI("-info") || arg.strEquCI("--info")) {
      wantInfo = true;
      continue;
    }
    Sys_Error("unknown option '%s'", *arg);
  }

  if (flist.length() == 0 && !wantInfo) usage();

  if (dbname.isEmpty()) dbname = ".wadhash.wdb";
  // prepend binary path to db name
  if (!dbname.IsAbsolutePath()) {
    dbname = VStr(VArgs::GetBinaryDir())+"/"+dbname;
  }
  GLog.Logf("using database file '%s'", *dbname);

  // load database
  {
    VStream *fi = FL_OpenSysFileRead(dbname);
    if (fi) {
      wdbRead(fi);
      delete fi;
    }
  }

  if (doRegisterWads) {
    // registering new wads
    for (int f = 0; f < flist.length(); ++f) W_AddDiskFile(flist[f]);
    GLog.Logf("calculating hashes...");
    for (int lump = W_IterateNS(-1, WADNS_Any); lump >= 0; lump = W_IterateNS(lump, WADNS_Any)) {
      if (W_LumpLength(lump) < 8) continue;
      TArray<vuint8> data;
      W_LoadLumpIntoArrayIdx(lump, data);
      XXH64_hash_t hash = XXH64(data.ptr(), data.length(), 0x29a);
      //GLog.Logf("calculated hash for '%s': 0x%16llx", *W_FullLumpName(lump), hash);
      wdbAppend(hash, W_FullLumpName(lump), W_LumpLength(lump));
    }

    {
      VStream *fo = FL_OpenSysFileWrite(dbname);
      if (!fo) Sys_Error("cannot create output database '%s'", *dbname);
      wdbWrite(fo);
      delete fo;
    }

    if (wantInfo) showDBWadList();
  } else {
    // check for duplicates
    if (wadlist.length() == 0) Sys_Error("database file '%s' not found or empty", *dbname);

    if (wantInfo) showDBWadList();

    if (flist.length()) {
      for (int f = 0; f < flist.length(); ++f) W_AddDiskFile(flist[f]);

      wdbClearResults();
      for (int lump = W_IterateNS(-1, WADNS_Any); lump >= 0; lump = W_IterateNS(lump, WADNS_Any)) {
        if (W_LumpLength(lump) < 8) continue;
        TArray<vuint8> data;
        W_LoadLumpIntoArrayIdx(lump, data);
        XXH64_hash_t hash = XXH64(data.ptr(), data.length(), 0x29a);
        wdbFind(hash, W_FullLumpName(lump), W_LumpLength(lump));
      }

      if (wdbFindResults.length()) {
        wdbSortResults();
        if (longReport) {
          for (auto &&hit : wdbFindResults) {
            fprintf(stderr, "LUMP HIT FOR '%s'\n", *hit.origName);
            fprintf(stderr, "  %s\n", *hit.dbName);
          }
        } else {
          for (auto &&hit : wdbFindResults) fprintf(stderr, "LUMP HIT FOR '%s': %s\n", *hit.origName, *hit.dbName);
        }
      }
    }
  }

  //W_AddFile("/home/ketmar/DooMz/wads/doom2.wad");

  Z_ShuttingDown();
  return 0;
}
