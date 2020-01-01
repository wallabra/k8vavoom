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
#include "gamedefs.h"
#include "snd_local.h"


VCvarB snd_sf2_autoload("snd_sf2_autoload", true, "Automatically load SF2 from a set of predefined directories?", CVAR_Archive|CVAR_PreInit);
VCvarS snd_sf2_file("snd_sf2_file", "", "Use this soundfont file.", CVAR_Archive|CVAR_PreInit);


TArray<VStr> sf2FileList;
static bool diskScanned = false;


static const char *SF2SearchPathes[] = {
  "!",
#if !defined(__SWITCH__)
  "!/sf2",
  "!/dls",
  "!/soundfonts",
#endif
#if defined(_WIN32)
  "!/share",
  "!/share/sf2",
  "!/share/dls",
  "!/share/soundfonts",
#endif
#if !defined(_WIN32) && !defined(__SWITCH__)
  "~/.k8vavoom",
  "~/.k8vavoom/sf2",
  "~/.k8vavoom/dls",
  "~/.k8vavoom/soundfonts",

  "/opt/vavoom/sf2",
  "/opt/vavoom/dls",
  "/opt/vavoom/soundfonts",

  "/opt/vavoom/share",
  "/opt/vavoom/share/sf2",
  "/opt/vavoom/share/dls",
  "/opt/vavoom/share/soundfonts",

  "/opt/vavoom/share/k8vavoom",
  "/opt/vavoom/share/k8vavoom/sf2",
  "/opt/vavoom/share/k8vavoom/dls",
  "/opt/vavoom/share/k8vavoom/soundfonts",

  "/usr/local/share/k8vavoom",
  "/usr/local/share/k8vavoom/sf2",
  "/usr/local/share/k8vavoom/dls",
  "/usr/local/share/k8vavoom/soundfonts",

  "/usr/share/k8vavoom",
  "/usr/share/k8vavoom/sf2",
  "/usr/share/k8vavoom/dls",
  "/usr/share/k8vavoom/soundfonts",

  "!/../share",
  "!/../share/sf2",
  "!/../share/dls",
  "!/../share/soundfonts",

  "!/../share/k8vavoom",
  "!/../share/k8vavoom/sf2",
  "!/../share/k8vavoom/dls",
  "!/../share/k8vavoom/soundfonts",
#endif
#if defined(__SWITCH__)
  "/switch/k8vavoom",
  "/switch/k8vavoom/sf2",
  "/switch/k8vavoom/dls",
  "/switch/k8vavoom/soundfonts",
#endif
  nullptr,
};


//==========================================================================
//
//  SF2_NeedDiskScan
//
//==========================================================================
bool SF2_NeedDiskScan () {
  return !diskScanned;
}


//==========================================================================
//
//  SF2_SetDiskScanned
//
//==========================================================================
void SF2_SetDiskScanned (bool v) {
  diskScanned = !v;
}


//==========================================================================
//
//  SF2_ScanDiskBanks
//
//  this fills `sf2FileList`
//
//==========================================================================
void SF2_ScanDiskBanks () {
  if (sf2FileList.length() == 0 || sf2FileList[0] != snd_sf2_file.asStr()) {
    diskScanned = false;
  }

  if (diskScanned) return;

  // try to find sf2 in binary dir
  diskScanned = true;

  // collect banks
  sf2FileList.reset();
  sf2FileList.append(snd_sf2_file.asStr());

  if (snd_sf2_autoload) {
    for (const char **sfdir = SF2SearchPathes; *sfdir; ++sfdir) {
      VStr dirname = VStr(*sfdir);
      if (dirname.isEmpty()) continue;
      if (dirname[0] == '!') { dirname.chopLeft(1); dirname = GParsedArgs.getBinDir()+dirname; }
      #if !defined(_WIN32) && !defined(__SWITCH__)
      else if (dirname[0] == '~') {
        const char *home = getenv("HOME");
        if (!home || !home[0]) continue;
        dirname.chopLeft(1);
        dirname = VStr(home)+dirname;
      }
      #endif
      //GCon->Logf("Timidity: scanning '%s'...", *dirname);
      auto dir = Sys_OpenDir(dirname);
      for (;;) {
        auto fname = Sys_ReadDir(dir);
        if (fname.isEmpty()) break;
        VStr ext = fname.extractFileExtension();
        if (ext.strEquCI(".sf2") || ext.strEquCI(".dls")) sf2FileList.append(dirname+"/"+fname);
      }
      Sys_CloseDir(dir);
    }
  }

#if defined(__SWITCH__)
  // try "/switch/k8vavoom/gzdoom.sf2"
  if (Sys_FileExists("/switch/k8vavoom/gzdoom.sf2")) {
    bool found = false;
    for (auto &&fn : sf2FileList) {
      if (fn.strEquCI("/switch/k8vavoom/gzdoom.sf2")) {
        found = true;
        break;
      }
    }
    if (!found) sf2FileList.append("/switch/k8vavoom/gzdoom.sf2");
  }
#endif

#ifdef _WIN32
  {
    /*static*/ const char *shitdozeShit[] = {
      "ct4mgm.sf2",
      "ct2mgm.sf2",
      "drivers\\gm.dls",
      nullptr,
    };
    bool delimeterPut = false;
    for (const char **ssp = shitdozeShit; *ssp; ++ssp) {
      static char sysdir[65536];
      memset(sysdir, 0, sizeof(sysdir));
      if (!GetSystemDirectoryA(sysdir, sizeof(sysdir)-1)) break;
      //VStr gmpath = VStr(getenv("WINDIR"))+"/system32/drivers/gm.dls";
      VStr gmpath = VStr(sysdir)+"\\"+(*ssp);
      //GCon->Logf("::: trying <%s> :::", *gmpath);
      if (Sys_FileExists(*gmpath)) {
        bool found = false;
        for (auto &&fn : sf2FileList) {
          if (fn.strEquCI(gmpath)) {
            found = true;
            break;
          }
        }
        if (!found) {
          if (!delimeterPut) sf2FileList.append(""); // delimiter
          delimeterPut = true;
          sf2FileList.append(gmpath);
        }
      }
    }
  }
#endif
}
