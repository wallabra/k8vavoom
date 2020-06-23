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

#include <windows.h>
#include <commctrl.h>

#include "wdb.h"

#include "resource.h"


// ////////////////////////////////////////////////////////////////////////// //
static HINSTANCE myHInst = NULL;


// ////////////////////////////////////////////////////////////////////////// //
static bool longReport = false;


#if 0
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
#endif


static VStr dbname;
static bool doneOptions = false;
static bool doRegisterWads = false;
static bool wantInfo = false;
static TArray<VStr> flist;


//==========================================================================
//
//  parseCmdLine
//
//==========================================================================
static void parseCmdLine (int argc, char **argv) {
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
    MessageBoxA(/*hwndDlg*/0, va("unknown option '%s'", *arg), "WADCheck ERROR", MB_OK|/*MB_ICONINFORMATION|*/MB_ICONERROR|MB_APPLMODAL);
    ExitProcess(1);
  }
}


//==========================================================================
//
//  mainXX
//
//==========================================================================
#if 0
int mainXX (int argc, char **argv) {
  GLog.Logf("WADCHECK build date: %s  %s", __DATE__, __TIME__);

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
#endif


//==========================================================================
//
//  checkEnableGoButton
//
//==========================================================================
static void checkEnableGoButton (HWND hwnd) {
  bool enabled = false;
  /*if (wadlist.length() != 0)*/ {
    static char fname[4096];
    GetDlgItemText(hwnd, IDM_DB_EDIT, fname, sizeof(fname));
    if (fname[0]) {
      GetDlgItemText(hwnd, IDM_WAD_EDIT, fname, sizeof(fname));
      if (fname[0]) {
        enabled = true;
      }
    }
  }
  //fprintf(stderr, "enabled=%d\n", (int)enabled);
  //SendDlgItemMessageA(hwnd, IDM_BT_GO, WM_ENABLE, (enabled ? 1 : 0), 0);
  EnableWindow(GetDlgItem(hwnd, IDM_BT_GO), (enabled ? 1 : 0));
  EnableWindow(GetDlgItem(hwnd, IDM_BT_REGISTER), (enabled ? 1 : 0));
}


//==========================================================================
//
//  ShowDBInfo
//
//==========================================================================
static void ShowDBInfo (HWND hwnd) {
  SetDlgItemText(hwnd, IDM_DB_EDIT, *dbname);
  VStr snfo;
  snfo += va("WADCHECK build date: %s  %s\n", __DATE__, __TIME__);
  snfo += va("using database:\n%s\n", *dbname);
  snfo += "WADs in database:\n";
  for (auto &&wn : wadnames) snfo += va("  %s\n", *wn);
  SetDlgItemText(hwnd, IDM_ED_RESULTS, *snfo);
  EnableWindow(GetDlgItem(hwnd, IDM_BT_SAVE), 0);
}


//==========================================================================
//
//  MainDialogProc
//
//==========================================================================
INT_PTR CALLBACK MainDialogProc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  //fprintf(stderr, "msg=0x%04x\n", uMsg);
  OPENFILENAMEA ofn;
  static char fname[4096];
  switch (uMsg) {
    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case IDM_DB_BT_SELECT:
          memset(fname, 0, sizeof(fname));
          GetDlgItemText(hwnd, IDM_DB_EDIT, fname, sizeof(fname));
          memset(&ofn, 0, sizeof(ofn));
          ofn.lStructSize = sizeof(ofn);
          ofn.hwndOwner = hwnd;
          ofn.hInstance = myHInst;
          ofn.lpstrFilter = "WDB database files\0*.wdb\0All files\0*.*\0\0";
          ofn.lpstrFile = fname;
          ofn.nMaxFile = (DWORD)sizeof(fname);
          ofn.lpstrInitialDir = ".";
          ofn.lpstrTitle = "Select WDB database file";
          ofn.Flags = OFN_DONTADDTORECENT|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
          // GetSaveFileNameA
          if (GetOpenFileNameA(&ofn)) {
            //MessageBoxA(hwnd, ofn.lpstrFile, "Selected WAD", MB_OK|MB_ICONINFORMATION|MB_APPLMODAL);
            dbname = VStr(fname);
            wdbClear();
            VStream *fi = FL_OpenSysFileRead(dbname);
            if (fi) {
              wdbRead(fi);
              fi->Close();
              delete fi;
            }
            ShowDBInfo(hwnd);
            checkEnableGoButton(hwnd);
          }
          return (INT_PTR)TRUE;

        case IDM_WAD_BT_SELECT:
          memset(fname, 0, sizeof(fname));
          GetDlgItemText(hwnd, IDM_WAD_EDIT, fname, sizeof(fname));
          memset(&ofn, 0, sizeof(ofn));
          ofn.lStructSize = sizeof(ofn);
          ofn.hwndOwner = hwnd;
          ofn.hInstance = myHInst;
          ofn.lpstrFilter = "WAD files\0*.wad\0All files\0*.*\0\0";
          ofn.lpstrFile = fname;
          ofn.nMaxFile = (DWORD)sizeof(fname);
          ofn.lpstrInitialDir = ".";
          ofn.lpstrTitle = "Select IWAD file to process";
          ofn.Flags = OFN_DONTADDTORECENT|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
          // GetSaveFileNameA
          if (GetOpenFileNameA(&ofn)) {
            //MessageBoxA(hwnd, ofn.lpstrFile, "Selected WAD", MB_OK|MB_ICONINFORMATION|MB_APPLMODAL);
            SetDlgItemText(hwnd, IDM_WAD_EDIT, fname);
            checkEnableGoButton(hwnd);
          }
          return (INT_PTR)TRUE;

        case IDM_BT_SAVE:
          if (wdbFindResults.length()) {
            memset(fname, 0, sizeof(fname));
            GetDlgItemText(hwnd, IDM_WAD_EDIT, fname, sizeof(fname));
            char *lastsl = strchr(fname, '.');
            if (!lastsl) lastsl = fname+strlen(fname);
            strcpy(lastsl, ".report.txt");
            memset(&ofn, 0, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.hInstance = myHInst;
            ofn.lpstrFilter = "Text files\0*.txt\0All files\0*.*\0\0";
            ofn.lpstrFile = fname;
            ofn.nMaxFile = (DWORD)sizeof(fname);
            ofn.lpstrInitialDir = ".";
            ofn.lpstrTitle = "Select file to save the report";
            ofn.Flags = OFN_DONTADDTORECENT|OFN_PATHMUSTEXIST;
            if (GetSaveFileNameA(&ofn)) {
              wdbSortResults();
              VStr res;
              res += va("%d duplicate lump%s found!\r\n", wdbFindResults.length(), (wdbFindResults.length() != 1 ? "s" : ""));
              for (auto &&hit : wdbFindResults) res += va("LUMP HIT FOR '%s': %s\r\n", *hit.origName, *hit.dbName);
              VStream *fo = FL_OpenSysFileWrite(ofn.lpstrFile);
              if (fo) {
                fo->Serialise(*res, res.length());
                fo->Close();
                delete fo;
                MessageBoxA(hwnd, va("Report saved to\n%s", ofn.lpstrFile), "Report Saved", MB_OK|MB_ICONINFORMATION|MB_APPLMODAL);
              } else {
                MessageBoxA(hwnd, va("FAILED saving to\n%s", ofn.lpstrFile), "Report Save Error", MB_OK|MB_ICONERROR|MB_APPLMODAL);
              }
            }
          }
          return (INT_PTR)TRUE;

        case IDM_BT_REGISTER:
          EnableWindow(GetDlgItem(hwnd, IDM_BT_GO), 0);
          EnableWindow(GetDlgItem(hwnd, IDM_BT_REGISTER), 0);

          if (MessageBoxA(hwnd, "Do you want to append WAD contents\nto the database?", "Database Update Confirmation", MB_YESNOCANCEL|MB_ICONQUESTION|MB_APPLMODAL) == IDYES) {
            // registering new wads
            //for (int f = 0; f < flist.length(); ++f) W_AddDiskFile(flist[f]);
            GetDlgItemText(hwnd, IDM_WAD_EDIT, fname, sizeof(fname));
            //W_AddDiskFile(fname);
            fsys_EnableAuxSearch = true;
            W_StartAuxiliary();
            /*
            if (W_OpenAuxiliary(VStr(fname).fixSlashes()) < 0) {
              W_CloseAuxiliary();
              MessageBoxA(hwnd, va("WAD NOT FOUND:\n%s", fname), "Database Update Error", MB_OK|MB_ICONERROR|MB_APPLMODAL);
              return (INT_PTR)TRUE;
            }
            */
            {
              VStream *ws = FL_OpenSysFileRead(fname);
              bool ok = false;
              if (ws) {
                if (W_AddAuxiliaryStream(ws, VFS_Wad) >= 0) {
                  ok = true;
                }
              }
              if (!ok) {
                W_CloseAuxiliary();
                MessageBoxA(hwnd, va("WAD NOT FOUND:\n%s", fname), "Database Update Error", MB_OK|MB_ICONERROR|MB_APPLMODAL);
                EnableWindow(GetDlgItem(hwnd, IDM_BT_GO), 1);
                EnableWindow(GetDlgItem(hwnd, IDM_BT_REGISTER), 1);
                return (INT_PTR)TRUE;
              }
            }

            int oldlen = wadlist.length();
            //GLog.Logf("calculating hashes...");
            for (int lump = W_IterateNS(-1, WADNS_Any); lump >= 0; lump = W_IterateNS(lump, WADNS_Any)) {
              if (W_LumpLength(lump) < 8) continue;
              TArray<vuint8> data;
              W_LoadLumpIntoArrayIdx(lump, data);
              XXH64_hash_t hash = XXH64(data.ptr(), data.length(), 0x29a);
              //GLog.Logf("calculated hash for '%s': 0x%16llx", *W_FullLumpName(lump), hash);
              wdbAppend(hash, W_FullLumpName(lump), W_LumpLength(lump));
            }
            W_CloseAuxiliary();

            if (/*oldlen != wadlist.length()*/true) {
              if (MessageBoxA(hwnd, va("Added %d new lump%s\nSave new database?", wadlist.length()-oldlen, (wadlist.length()-oldlen != 1 ? "s" : "")), "Database Update Confirmation", MB_YESNOCANCEL|MB_ICONQUESTION|MB_APPLMODAL) == IDYES) {
                VStream *fo = FL_OpenSysFileWrite(dbname);
                if (!fo) Sys_Error("cannot create output database '%s'", *dbname);
                wdbWrite(fo);
                fo->Close();
                delete fo;
              }
            }
          }

          ShowDBInfo(hwnd);

          EnableWindow(GetDlgItem(hwnd, IDM_BT_GO), 1);
          EnableWindow(GetDlgItem(hwnd, IDM_BT_REGISTER), 1);
          return (INT_PTR)TRUE;

        case IDM_BT_GO:
          EnableWindow(GetDlgItem(hwnd, IDM_BT_GO), 0);
          EnableWindow(GetDlgItem(hwnd, IDM_BT_REGISTER), 0);

          // check for duplicates
          if (wadlist.length() == 0) {
            MessageBoxA(hwnd, "Database not found or empty!", "Database Error", MB_OK|MB_ICONERROR|MB_APPLMODAL);
            return (INT_PTR)TRUE;
          }

          fsys_EnableAuxSearch = true;
          W_StartAuxiliary();

          GetDlgItemText(hwnd, IDM_WAD_EDIT, fname, sizeof(fname));
          {
            VStream *ws = FL_OpenSysFileRead(fname);
            bool ok = false;
            if (ws) {
              if (W_AddAuxiliaryStream(ws, VFS_Wad) >= 0) {
                ok = true;
              }
            }
            if (!ok) {
              W_CloseAuxiliary();
              MessageBoxA(hwnd, va("WAD NOT FOUND:\n%s", fname), "Check Error", MB_OK|MB_ICONERROR|MB_APPLMODAL);
              EnableWindow(GetDlgItem(hwnd, IDM_BT_GO), 1);
              EnableWindow(GetDlgItem(hwnd, IDM_BT_REGISTER), 1);
              return (INT_PTR)TRUE;
            }
          }

          wdbClearResults();
          for (int lump = W_IterateNS(-1, WADNS_Any); lump >= 0; lump = W_IterateNS(lump, WADNS_Any)) {
            if (W_LumpLength(lump) < 8) continue;
            TArray<vuint8> data;
            W_LoadLumpIntoArrayIdx(lump, data);
            XXH64_hash_t hash = XXH64(data.ptr(), data.length(), 0x29a);
            wdbFind(hash, W_FullLumpName(lump), W_LumpLength(lump));
          }
          W_CloseAuxiliary();

          if (wdbFindResults.length()) {
            wdbSortResults();
            VStr res;
            res += va("%d duplicate lump%s found!\n", wdbFindResults.length(), (wdbFindResults.length() != 1 ? "s" : ""));
            for (auto &&hit : wdbFindResults) res += va("LUMP HIT FOR '%s': %s\n", *hit.origName, *hit.dbName);
            SetDlgItemText(hwnd, IDM_ED_RESULTS, *res);
            EnableWindow(GetDlgItem(hwnd, IDM_ED_RESULTS), 1);
            EnableWindow(GetDlgItem(hwnd, IDM_BT_SAVE), 1);
          }

          EnableWindow(GetDlgItem(hwnd, IDM_BT_GO), 1);
          EnableWindow(GetDlgItem(hwnd, IDM_BT_REGISTER), 1);
          return (INT_PTR)TRUE;
      }
      //fprintf(stderr, "  cmd=%d\n", (int)(LOWORD(wParam)));
      break;
    case WM_SYSCOMMAND:
      //fprintf(stderr, "  syscmd=%d\n", (int)(LOWORD(wParam)));
      if (LOWORD(wParam) == SC_CLOSE) {
        EndDialog(hwnd, (INT_PTR)LOWORD(wParam));
        return (INT_PTR)TRUE;
      }
      break;
    case WM_INITDIALOG:
      //GLog.Logf("using database file '%s'", *dbname);
      ShowDBInfo(hwnd);
      return (INT_PTR)TRUE;
  }
  return (INT_PTR)FALSE;
}


#define MAX_ARGV  (128)


//==========================================================================
//
//  WinMain
//
//==========================================================================
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  myHInst = hInstance;

  if (!lpCmdLine) lpCmdLine = (char *)"";

  int argc = 1;
  char *argv[MAX_ARGV];

  static char myDir[4096];
  GetModuleFileName(GetModuleHandle(NULL), myDir, sizeof(myDir)-1);
  char *p = strrchr(myDir, '\\');
  if (!p) strcpy(myDir, "."); else *p = '\0';
  //for (char *tmps = myDir; *tmps; ++tmps) if (*tmps == '\\') *tmps = '/';
  argv[0] = myDir;

  char *cmdline = (char *)malloc(strlen(lpCmdLine)*4+1);
  strcpy(cmdline, lpCmdLine);

  //fprintf(stderr, "<%s>\n", cmdline);
  while (*cmdline) {
    const char ch = *cmdline++;
    if ((unsigned)(ch&0xff) <= 32) continue;
    if (ch == '"') {
      argv[argc++] = cmdline;
      while (*cmdline && *cmdline != ch) {
        if (*cmdline == '\\') {
          memmove(cmdline, cmdline+1, strlen(cmdline));
          if (*cmdline) ++cmdline;
        } else if (cmdline[0] == ch && cmdline[1] == ch) {
          memmove(cmdline, cmdline+1, strlen(cmdline+1)+1);
          if (*cmdline) ++cmdline;
        } else {
          ++cmdline;
        }
      }
      if (*cmdline) *cmdline++ = 0;
    } else {
      argv[argc++] = cmdline-1;
      while (*cmdline && (unsigned)(cmdline[0]&0xff) > 32) {
        if (cmdline[0] == '"') break;
        if (cmdline[0] == '\\') {
          memmove(cmdline, cmdline+1, strlen(cmdline));
          if (*cmdline) ++cmdline;
          continue;
        }
        ++cmdline;
      }
      if (*cmdline) {
        if (*cmdline == '"') {
          memmove(cmdline+1, cmdline, strlen(cmdline)+1);
        }
        *cmdline++ = 0;
      }
    }
  }

  /*
  fprintf(stderr, "argc=%d\n", argc);
  for (int f = 0; f < argc; ++f) fprintf(stderr, "%d: <%s>\n", f, argv[f]);
  */

  parseCmdLine(argc, argv);

  if (dbname.isEmpty()) dbname = ".wadhash.wdb";
  // prepend binary path to db name
  if (!dbname.IsAbsolutePath()) {
    dbname = VStr(argv[0])+"\\"+dbname;
  }

  // load database
  {
    VStream *fi = FL_OpenSysFileRead(dbname);
    if (fi) {
      wdbRead(fi);
      fi->Close();
      delete fi;
    }
  }

  INITCOMMONCONTROLSEX icc;
  icc.dwSize = sizeof(icc);
  icc.dwICC = ICC_WIN95_CLASSES;
  InitCommonControlsEx(&icc);

  DialogBox(hInstance, MAKEINTRESOURCE(IDD_MAINWIN), /*hWnd*/0, &MainDialogProc);
  ExitProcess(0);
  return 0;
}
