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

#include "../../../render/hwgl/gl_local.h"


// ////////////////////////////////////////////////////////////////////////// //
enum {
  WGL_CONTEXT_MAJOR_VERSION_ARB = 0x2091,
  WGL_CONTEXT_MINOR_VERSION_ARB = 0x2092,
  WGL_CONTEXT_LAYER_PLANE_ARB = 0x2093,
  WGL_CONTEXT_FLAGS_ARB = 0x2094,
  WGL_CONTEXT_PROFILE_MASK_ARB = 0x9126,

  WGL_CONTEXT_DEBUG_BIT_ARB = 0x0001,
  WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB = 0x0002,

  WGL_CONTEXT_CORE_PROFILE_BIT_ARB = 0x00000001,
  WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB = 0x00000002,
};


// ////////////////////////////////////////////////////////////////////////// //
class VWin32OpenGLDrawer : public VOpenGLDrawer {
public:
  bool Windowed;
  HDC DeviceContext;
  HGLRC RenderContext;
  HWND RenderWindow;

  virtual void Init () override;
  virtual bool SetResolution (int AWidth, int AHeight, bool AWindowed) override;
  virtual void *GetExtFuncPtr (const char *name) override;
  virtual void Update () override;
  virtual void Shutdown () override;

  virtual void WarpMouseToWindowCenter () override;
  virtual bool SetAdaptiveSwap () override;
};

IMPLEMENT_DRAWER(VWin32OpenGLDrawer, DRAWER_OpenGL, "OpenGL", "Win32 OpenGL rasteriser device", "-opengl");


//==========================================================================
//
//  VWin32OpenGLDrawer::Init
//
//  Determine the hardware configuration
//
//==========================================================================
void VWin32OpenGLDrawer::Init () {
  Windowed = true;
  DeviceContext = nullptr;
  RenderContext = nullptr;
  RenderWindow = nullptr;
}


//==========================================================================
//
//  VWin32OpenGLDrawer::WarpMouseToWindowCenter
//
//  k8: somebody should fix this; i don't care
//
//==========================================================================
void VWin32OpenGLDrawer::WarpMouseToWindowCenter () {
}


//==========================================================================
//
//  VWin32OpenGLDrawer::SetAdaptiveSwap
//
//  k8: somebody should fix this; i don't care
//
//==========================================================================
bool VWin32OpenGLDrawer::SetAdaptiveSwap () {
  return false;
}


//==========================================================================
//
//  VWin32OpenGLDrawer::SetResolution
//
//  Set up the video mode
//
//==========================================================================
bool VWin32OpenGLDrawer::SetResolution (int AWidth, int AHeight, bool AWindowed) {
  guard(VWin32OpenGLDrawer::SetResolution);

  typedef HGLRC (APIENTRY *wglCreateContextAttribsARB_fna) (HDC hDC, HGLRC hShareContext, const int *attribList);
  wglCreateContextAttribsARB_fna wglCreateContextAttribsARB = nullptr;

  int Width = AWidth;
  int Height = AHeight;
  int pixelformat;
  MSG msg;

  if (!Width || !Height) {
    // set defaults
    Width = 640;
    Height = 480;
  }

  // shut down current mode
  Shutdown();

  Windowed = AWindowed;

  if (!Windowed) {
    // try to switch to the new mode
    DEVMODE dmScreenSettings;
    memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
    dmScreenSettings.dmSize = sizeof(dmScreenSettings);
    dmScreenSettings.dmPelsWidth = Width;
    dmScreenSettings.dmPelsHeight = Height;
    dmScreenSettings.dmBitsPerPel = 32/*BPP*/;
    dmScreenSettings.dmFields = DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT;
    if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL) {
      memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
      dmScreenSettings.dmSize = sizeof(dmScreenSettings);
      dmScreenSettings.dmPelsWidth = Width;
      dmScreenSettings.dmPelsHeight = Height;
      dmScreenSettings.dmBitsPerPel = 24/*BPP*/;
      dmScreenSettings.dmFields = DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT;
      if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL) return false;
    }
  }

  // create window
  RenderWindow = CreateWindow("VAVOOM", "VAVOOM for Windows",
    (Windowed ? (WS_OVERLAPPEDWINDOW&~WS_MAXIMIZEBOX)|WS_CLIPCHILDREN|WS_CLIPSIBLINGS : WS_POPUP)|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
    0, 0, 2, 2, hwnd, nullptr, hInst, nullptr);
  if (!RenderWindow) {
    GCon->Log(NAME_Init, "Couldn't create window");
    return false;
  }

  // make the window visible & update its client area
  ShowWindow(RenderWindow, SW_SHOWDEFAULT);
  UpdateWindow(RenderWindow);

  // switch input to this window
  IN_SetActiveWindow(RenderWindow);

  // Now we try to make sure we get the focus on the mode switch, because
  // sometimes in some systems we don't. We grab the foreground, pump all
  // our messages, and sleep for a little while to let messages finish
  // bouncing around the system, then we put ourselves at the top of the z
  // order, then grab the foreground again.
  // Who knows if it helps, but it probably doesn't hurt
  SetForegroundWindow(RenderWindow);

  while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  Sleep(10);

  if (Windowed) {
    RECT WindowRect;
    WindowRect.left = (long)0;
    WindowRect.right = (long)Width;
    WindowRect.top = (long)0;
    WindowRect.bottom = (long)Height;
    AdjustWindowRectEx(&WindowRect, WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_CLIPSIBLINGS, FALSE, WS_EX_APPWINDOW|WS_EX_WINDOWEDGE);
    SetWindowPos(RenderWindow, HWND_TOP, 0, 0, WindowRect.right-WindowRect.left, WindowRect.bottom-WindowRect.top, SWP_NOMOVE);
  } else {
    SetWindowPos(RenderWindow, HWND_TOP, 0, 0, Width, Height, SWP_NOMOVE);
  }

  SetForegroundWindow(RenderWindow);

  // get device context
  DeviceContext = GetDC(RenderWindow);
  if (!DeviceContext) {
    GCon->Log(NAME_Init, "Failed to get device context");
    return false;
  }

  // Because we have set the background brush for the window to nullptr
  // (to avoid flickering when re-sizing the window on the desktop), we
  // clear the window to black when created, otherwise it will be
  // empty while Vavoom starts up.
  PatBlt(DeviceContext, 0, 0, Width, Height, BLACKNESS);

  //  Set up pixel format
  PIXELFORMATDESCRIPTOR pfd = { // pfd Tells Windows How We Want Things To Be
    sizeof(PIXELFORMATDESCRIPTOR),  // Size Of This Pixel Format Descriptor
    1,                // Version Number
    PFD_DRAW_TO_WINDOW |      // Format Must Support Window
    PFD_SUPPORT_OPENGL |      // Format Must Support OpenGL
    PFD_DOUBLEBUFFER,       // Must Support Double Buffering
    PFD_TYPE_RGBA,          // Request An RGBA Format
    byte(24/*32*//*BPP*/),            // Select Our Colour Depth
    0, 0, 0, 0, 0, 0,       // Colour Bits Ignored
    0,                // No Alpha Buffer
    0,                // Shift Bit Ignored
    0,                // No Accumulation Buffer
    0, 0, 0, 0,           // Accumulation Bits Ignored
    24,               // 24 bit Z-Buffer (Depth Buffer)
    8,                // 8 bit Stencil Buffer
    0,                // No Auxiliary Buffer
    PFD_MAIN_PLANE,         // Main Drawing Layer
    0,                // Reserved
    0, 0, 0             // Layer Masks Ignored
  };

  pixelformat = ChoosePixelFormat(DeviceContext, &pfd);
  if (pixelformat == 0) {
    pfd = {
      sizeof(PIXELFORMATDESCRIPTOR),  // Size Of This Pixel Format Descriptor
      1,                // Version Number
      PFD_DRAW_TO_WINDOW |      // Format Must Support Window
      PFD_SUPPORT_OPENGL |      // Format Must Support OpenGL
      PFD_DOUBLEBUFFER,       // Must Support Double Buffering
      PFD_TYPE_RGBA,          // Request An RGBA Format
      byte(32/*BPP*/),            // Select Our Colour Depth
      0, 0, 0, 0, 0, 0,       // Colour Bits Ignored
      0,                // No Alpha Buffer
      0,                // Shift Bit Ignored
      0,                // No Accumulation Buffer
      0, 0, 0, 0,           // Accumulation Bits Ignored
      24,               // 24 bit Z-Buffer (Depth Buffer)
      8,                // 8 bit Stencil Buffer
      0,                // No Auxiliary Buffer
      PFD_MAIN_PLANE,         // Main Drawing Layer
      0,                // Reserved
      0, 0, 0             // Layer Masks Ignored
    };
    pixelformat = ChoosePixelFormat(DeviceContext, &pfd);
    if (pixelformat == 0) Sys_Error("ChoosePixelFormat failed");
  }

  if (SetPixelFormat(DeviceContext, pixelformat, &pfd) == FALSE) Sys_Error("SetPixelFormat failed");

  // k8: windoze is idiotic: we have to have OpenGL context to get function addresses
  // so we will create fake context to get that stupid address
  {
    auto tmpcc = wglCreateContext(DeviceContext);
    if (!tmpcc) Sys_Error("Failed to create OpenGL context");
    wglMakeCurrent(DeviceContext, tmpcc);
    wglCreateContextAttribsARB = (wglCreateContextAttribsARB_fna)GetExtFuncPtr("wglCreateContextAttribsARB");
    if (!wglCreateContextAttribsARB) Sys_Error("Can't get address of `wglCreateContextAttribsARB`");
    wglMakeCurrent(DeviceContext, nullptr);
    wglDeleteContext(tmpcc);
  }

  int contextAttribs[7] = {
    WGL_CONTEXT_MAJOR_VERSION_ARB, 2,
    WGL_CONTEXT_MINOR_VERSION_ARB, 1,
    WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB, // allow "deprecated" API
    0
  };

  // create rendering context
  //RenderContext = wglCreateContext(DeviceContext);
  RenderContext = wglCreateContextAttribsARB(DeviceContext, nullptr, contextAttribs);
  if (!RenderContext) {
    GCon->Log(NAME_Init, "Failed to create context");
    return false;
  }

  // make this context current
  if (!wglMakeCurrent(DeviceContext, RenderContext)) {
    GCon->Log(NAME_Init, "Make current failed");
    return false;
  }

  // swap control extension (VSync)
  if (CheckExtension("WGL_EXT_swap_control")) {
    GCon->Log(NAME_Init, "Swap control extension found.");
    typedef bool (APIENTRY *PFNWGLSWAPINTERVALFARPROC)(int);
    PFNWGLSWAPINTERVALFARPROC wglSwapIntervalEXT;
    wglSwapIntervalEXT = (PFNWGLSWAPINTERVALFARPROC)GetExtFuncPtr("wglSwapIntervalEXT");
    if (wglSwapIntervalEXT) wglSwapIntervalEXT(r_vsync);
  }

  // everything is fine, set some globals and finish
  ScreenWidth = Width;
  ScreenHeight = Height;

  return true;
  unguard;
}


//==========================================================================
//
//  VWin32OpenGLDrawer::GetExtFuncPtr
//
//==========================================================================
void *VWin32OpenGLDrawer::GetExtFuncPtr (const char *name) {
  guard(VWin32OpenGLDrawer::GetExtFuncPtr);
  if (!name || !name[0]) return nullptr;
  void *res = (void *)wglGetProcAddress(name);
  if (!res) {
    static HINSTANCE dll = nullptr;
    if (!dll) {
      dll = LoadLibraryA("opengl32.dll");
      if (!dll) return nullptr; // <32, but idc
    }
    res = (void *)GetProcAddress(dll, name);
  }
  return res;
  unguard;
}


//==========================================================================
//
//  VWin32OpenGLDrawer::Update
//
//  Blit to the screen / Flip surfaces
//
//==========================================================================
void VWin32OpenGLDrawer::Update () {
  guard(VWin32OpenGLDrawer::Update);
  SwapBuffers(DeviceContext);
  unguard;
}


//==========================================================================
//
//  VWin32OpenGLDrawer::Shutdown
//
//  Close the graphics
//
//==========================================================================
void VWin32OpenGLDrawer::Shutdown () {
  guard(VWin32OpenGLDrawer::Shutdown);
  DeleteTextures();

  if (RenderContext) {
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(RenderContext);
    RenderContext = 0;
  }

  if (DeviceContext) {
    ReleaseDC(RenderWindow, DeviceContext);
    DeviceContext = 0;
  }

  if (RenderWindow) {
    IN_SetActiveWindow(hwnd);
    SetForegroundWindow(hwnd);
    DestroyWindow(RenderWindow);
    RenderWindow = nullptr;
  }

  if (!Windowed) ChangeDisplaySettings(nullptr, 0);

  MSG msg;

  while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  unguard;
}
