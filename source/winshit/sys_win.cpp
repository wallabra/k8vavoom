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

#define INITGUID
#include "winshit/winlocal.h"
#include <objbase.h>
#include <mmsystem.h>
#include "gamedefs.h"
#include <signal.h>
#include <fcntl.h>
#include <io.h>
#include <direct.h>
#include <sys/stat.h>

//  Generate all GUIDs
#define DIRECTINPUT_VERSION   0x0800
#define DIRECTSOUND_VERSION   0x0900
#include <dinput.h>
#include <ddraw.h>
//#include <dsound.h>

//#define R_OK  (4)

// sleep time on pause or minimization
#define PAUSE_SLEEP      (50)
// sleep time when not focus
#define NOT_FOCUS_SLEEP  (20)


HWND hwnd; // needed for all DirectX interfaces
HINSTANCE hInst; // needed for DirectInput
VWinMessageHandler *GCDMsgHandler;


static HANDLE tevent;

static bool ActiveApp, Minimized;

static VCvarI win_priority("win_priority", "0", "shitdoze.", CVAR_Archive);
static VCvarI win_sys_keys("win_sys_keys", "1", "shitdoze.", CVAR_Archive);


//==========================================================================
//
//  Sys_Shutdown
//
//==========================================================================
void Sys_Shutdown () {
  CoUninitialize();
  ShowCursor(TRUE);
  if (tevent) CloseHandle(tevent);
}


//==========================================================================
//
// Sys_Quit
//
// Shuts down net game, saves defaults, prints the exit text message,
// goes to text mode, and exits.
//
//==========================================================================
void Sys_Quit (const char *) {
  dprintf("==========================================================================\n");
  dprintf("     Shuting down VAVOOM\n");
  dprintf("==========================================================================\n");
  // shutdown system
  Host_Shutdown();
  // exit
  SendMessage(hwnd, WM_CLOSE, 0, 0);
  exit(0);
}


//==========================================================================
//
//  signal_handler
//
//  Shuts down system, on error signal
//
//==========================================================================
static void signal_handler (int s) {
  signal(s, SIG_IGN); // ignore future instances of this signal.
  switch (s) {
    case SIGINT: throw VavoomError("Interrupted by User");
    case SIGILL: throw VavoomError("Illegal Instruction");
    case SIGFPE: throw VavoomError("Floating Point Exception");
    case SIGSEGV: throw VavoomError("Segmentation Violation");
    case SIGTERM: throw VavoomError("Software termination signal from kill");
    case SIGBREAK: throw VavoomError("Ctrl-Break sequence");
    case SIGABRT: throw VavoomError("Abnormal termination triggered by abort call");
    default: throw VavoomError("Terminated by signal");
  }
}


//==========================================================================
//
//  Sys_ConsoleInput
//
//==========================================================================
char *Sys_ConsoleInput () {
  //FIXME
  return nullptr;
}


//==========================================================================
//
//  WndProc
//
//==========================================================================
static LRESULT CALLBACK WndProc (HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam) {
  switch (iMsg) {
    case WM_KEYDOWN:
      if (!(lParam & 0x40000000) && wParam == VK_PAUSE) GInput->KeyEvent(K_PAUSE, true);
      return 0;
    case WM_KEYUP:
      if (wParam == VK_PAUSE) GInput->KeyEvent(K_PAUSE, false);
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    case MM_MCINOTIFY:
      if (GCDMsgHandler) return GCDMsgHandler->OnMessage(hwnd, iMsg, wParam, lParam);
      break;
    case WM_SYSKEYDOWN:
      // pressing Alt+Enter can toggle between fullscreen and windowed
      if (wParam == VK_RETURN && !(lParam&0x40000000)) {
        //TODO: add something here...
      }
      // pressing Alt+F4 quits the program
      if (wParam == VK_F4 && !(lParam&0x40000000)) PostQuitMessage(0);
      break;
    case WM_SYSCOMMAND:
      // check for maximize being hit
      switch (wParam&~0x0F) {
        case SC_SCREENSAVE:
        case SC_MONITORPOWER:
        case SC_KEYMENU:
        case SC_HOTKEY:
          // don't call DefWindowProc() because we don't want to start the screen saver fullscreen
          return 0;
      }
      break;
    case WM_ACTIVATE:
      ActiveApp = !(LOWORD(wParam) == WA_INACTIVE);
      if ((BOOL)HIWORD(wParam)) Minimized = true; else Minimized = false;
      break;
    case WM_KILLFOCUS:
      SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
      break;
    case WM_SETFOCUS:
      if (win_priority) {
        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
      } else {
        SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
      }
      break;
  }
  return DefWindowProc(hwnd, iMsg, wParam, lParam);
}


//==========================================================================
//
//  SleepUntilInput
//
//==========================================================================
void SleepUntilInput (int time) {
  MsgWaitForMultipleObjects(1, &tevent, FALSE, time, QS_ALLINPUT);
}


//==========================================================================
//
//  WinMain
//
//  Main program
//
//==========================================================================
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE, PSTR, int iCmdShow) {
  WNDCLASSEX wndclass;
  MSG msg;
  HACCEL ghAccel;

  try {
    GArgs.Init(__argc, __argv);

    hInst = hInstance;

    // create window class
    wndclass.cbSize        = sizeof(wndclass);
    wndclass.style         = CS_VREDRAW|CS_HREDRAW|CS_OWNDC;
    wndclass.lpfnWndProc   = WndProc;
    wndclass.cbClsExtra    = 0;
    wndclass.cbWndExtra    = 0;
    wndclass.hInstance     = hInst;
    wndclass.hIcon         = LoadIcon(hInstance, "APPICON");
    wndclass.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wndclass.hbrBackground = nullptr;
    wndclass.lpszMenuName  = nullptr;
    wndclass.lpszClassName = "VAVOOM";
    wndclass.hIconSm       = LoadIcon(hInstance, "APPICON");

    if (!RegisterClassEx(&wndclass)) {
      MessageBox(nullptr, "Failed to register class", "Error", MB_OK);
      return 1;
    }

    // create window
    hwnd = CreateWindowEx(WS_EX_APPWINDOW, "VAVOOM", "VAVOOM for Windows",
      (WS_OVERLAPPEDWINDOW&~WS_MAXIMIZEBOX)|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
      0, 0, 2, 2, nullptr, nullptr, hInst, nullptr);
    if (!hwnd) {
      MessageBox(nullptr, "Couldn't create window", "Error", MB_OK);
      return 1;
    }

    // make the window visible & update its client area
    ShowWindow(hwnd, iCmdShow);
    UpdateWindow(hwnd);

    // initialise COM
    if (FAILED(CoInitialize(nullptr))) {
      MessageBox(hwnd, "Failed to initialise COM", "Error", MB_OK);
      return 1;
    }

    // create event
    tevent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!tevent) {
      CoUninitialize();
      MessageBox(hwnd, "Couldn't create event", "Error", MB_OK);
      return 1;
    }

    ghAccel = LoadAccelerators(hInst, "AppAccel");

    ShowCursor(FALSE);

#ifndef _DEBUG
    // install signal handlers
    signal(SIGINT,  signal_handler);
    signal(SIGILL,  signal_handler);
    signal(SIGFPE,  signal_handler);
    signal(SIGSEGV, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGBREAK,signal_handler);
    signal(SIGABRT, signal_handler);
#endif

    (void)Sys_Time(); // initialize timer

    Host_Init();
    for (;;) {
      while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
          dprintf("Quit message\n");
          Sys_Quit(nullptr);
        } else if (!win_sys_keys && (msg.message == WM_SYSKEYDOWN || msg.message == WM_SYSKEYUP)) {
        } else if (!TranslateAccelerator(msg.hwnd, ghAccel, &msg)) {
          TranslateMessage(&msg);
          DispatchMessage(&msg);
        }
      }
      if (Minimized || !ActiveApp) {
        SleepUntilInput(PAUSE_SLEEP);
        continue;
      }
      Host_Frame();
    }
  } catch (VavoomError &e) {
    char *tmp_msg;
    Host_Shutdown();
    dprintf("\n\nERROR: %s\n", e.message);
    tmp_msg = new char[VStr::Length(e.message)+VStr::Length(Host_GetCoreDump())+128];
    sprintf(tmp_msg, "%s\n\n%s", e.message, Host_GetCoreDump());
    MessageBox(hwnd, tmp_msg, "Error", MB_OK);
    delete[] tmp_msg;
    tmp_msg = nullptr;
    SendMessage(hwnd, WM_CLOSE, 0, 0);
    return 1;
  }
#ifndef _DEBUG
  catch (...) {
    char *tmp_msg;
    Host_Shutdown();
    dprintf("\n\nExiting due to external exception\n");
    tmp_msg = new char[VStr::Length(Host_GetCoreDump())+128];
    sprintf(tmp_msg, "Received external exception\n\n%s", Host_GetCoreDump());
    MessageBox(hwnd, tmp_msg, "Error", MB_OK);
    delete[] tmp_msg;
    tmp_msg = nullptr;
    //throw;
    SendMessage(hwnd, WM_CLOSE, 0, 0);
    return 1;
  }
#endif
}
