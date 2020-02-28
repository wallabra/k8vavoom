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
//**
//**  Game completion, final screen animation.
//**
//**************************************************************************
#include "gamedefs.h"
#include "drawer.h"


VCvarF gl_maxdist("gl_maxdist", "8192", "Max view distance (too big values will cause z-buffer issues).", CVAR_Archive);


TArray<void (*) (int phase)> VDrawer::cbInitDeinit;

float VDrawer::LightFadeMult = 1.0f;


//**************************************************************************
//
// VDrawer methods
//
//**************************************************************************

//==========================================================================
//
//  VDrawer::VDrawer
//
//==========================================================================
VDrawer::VDrawer () noexcept
  : mInitialized(false)
  , isShittyGPU(false)
  , shittyGPUCheckDone(false)
  , useReverseZ(false)
  , HaveDepthClamp(false)
  , RendLev(nullptr)
{
  #ifdef CLIENT
  ScrWdt = max2(1, ScreenWidth);
  ScrHgt = max2(1, ScreenHeight);
  #else
  // it doesn't matter
  ScrWdt = 320;
  ScrHgt = 200;
  #endif
  LightFadeMult = 1.0f;
}


//==========================================================================
//
//  VDrawer::~VDrawer
//
//==========================================================================
VDrawer::~VDrawer () {
}


//==========================================================================
//
//  VDrawer::RegisterICB
//
//==========================================================================
void VDrawer::RegisterICB (void (*cb) (int phase)) {
  if (!cb) return;
  for (int f = 0; f < cbInitDeinit.length(); ++f) {
    if (cbInitDeinit[f] == cb) return;
  }
  cbInitDeinit.append(cb);
}


//==========================================================================
//
//  VDrawer::callICB
//
//==========================================================================
void VDrawer::callICB (int phase) {
  for (int f = 0; f < cbInitDeinit.length(); ++f) cbInitDeinit[f](phase);
}


//==========================================================================
//
//  VDrawer::ResetTextureUpdateFrames
//
//==========================================================================
void VDrawer::ResetTextureUpdateFrames () noexcept {
  for (int i = 0; i < GTextureManager.GetNumTextures(); ++i) {
    VTexture *tex = GTextureManager.getIgnoreAnim(i);
    if (tex) tex->lastUpdateFrame = 0;
  }
  for (int i = 0; i < GTextureManager.GetNumMapTextures(); ++i) {
    VTexture *tex = GTextureManager.getMapTexIgnoreAnim(i);
    if (tex) tex->lastUpdateFrame = 0;
  }
}


//**************************************************************************
//
// calculate matrices
//
//**************************************************************************

//==========================================================================
//
//  VDrawer::CalcProjectionMatrix
//
//==========================================================================
void VDrawer::CalcProjectionMatrix (VMatrix4 &ProjMat, VRenderLevelDrawer *rlev, const refdef_t *rd) {
  if (!CanUseRevZ()) {
    // normal
    //glClearDepth(1.0f);
    //glDepthFunc(GL_LEQUAL);
    ProjMat.SetIdentity();
    ProjMat[0][0] = 1.0f/rd->fovx;
    ProjMat[1][1] = 1.0f/rd->fovy;
    ProjMat[2][3] = -1.0f;
    ProjMat[3][3] = 0.0f;
    if (!HaveDepthClamp && rlev && rlev->IsShadowVolumeRenderer()) {
      ProjMat[2][2] = -1.0f;
      ProjMat[3][2] = -2.0f;
    } else {
      float maxdist = gl_maxdist.asFloat();
      if (maxdist < 1.0f || !isFiniteF(maxdist)) maxdist = 32767.0f;
      ProjMat[2][2] = -(maxdist+1.0f)/(maxdist-1.0f);
      ProjMat[3][2] = -2.0f*maxdist/(maxdist-1.0f);
    }
  } else {
    // reversed
    // see https://nlguillemot.wordpress.com/2016/12/07/reversed-z-in-opengl/
    //glClearDepth(0.0f);
    //glDepthFunc(GL_GEQUAL);
    ProjMat.SetZero();
    ProjMat[0][0] = 1.0f/rd->fovx;
    ProjMat[1][1] = 1.0f/rd->fovy;
    ProjMat[2][3] = -1.0f;
    ProjMat[3][2] = 1.0f; // zNear
  }
  //RestoreDepthFunc();
}


//==========================================================================
//
//  VDrawer::CalcModelMatrix
//
//==========================================================================
void VDrawer::CalcModelMatrix (VMatrix4 &ModelMat, const TVec &origin, const TAVec &angles, bool MirrorFlip) {
  ModelMat.SetIdentity();
  ModelMat *= VMatrix4::RotateX(-90); //glRotatef(-90, 1, 0, 0);
  ModelMat *= VMatrix4::RotateZ(90); //glRotatef(90, 0, 0, 1);
  if (MirrorFlip) ModelMat *= VMatrix4::Scale(TVec(1, -1, 1)); //glScalef(1, -1, 1);
  ModelMat *= VMatrix4::RotateX(-angles.roll); //glRotatef(-viewangles.roll, 1, 0, 0);
  ModelMat *= VMatrix4::RotateY(-angles.pitch); //glRotatef(-viewangles.pitch, 0, 1, 0);
  ModelMat *= VMatrix4::RotateZ(-angles.yaw); //glRotatef(-viewangles.yaw, 0, 0, 1);
  ModelMat *= VMatrix4::Translate(-origin); //glTranslatef(-vieworg.x, -vieworg.y, -vieworg.z);
}


//**************************************************************************
//
// VRenderLevelDrawer methods
//
//**************************************************************************

//==========================================================================
//
//  VRenderLevelDrawer::ResetDrawStack
//
//  should be called before rendering a frame
//  (i.e. in initial render, `RenderPlayerView()`)
//  creates 0th element of the stack
//
//==========================================================================
void VRenderLevelDrawer::ResetDrawStack () {
  DrawListStack.reset();
  DrawLists &dls = DrawListStack.alloc();
  dls.resetAll();
  vassert(DrawListStack.length() > 0);
}


//==========================================================================
//
//  VRenderLevelDrawer::PushDrawLists
//
//==========================================================================
void VRenderLevelDrawer::PushDrawLists () {
  DrawLists &dls = DrawListStack.alloc();
  dls.resetAll();
}


//==========================================================================
//
//  VRenderLevelDrawer::PopDrawLists
//
//==========================================================================
void VRenderLevelDrawer::PopDrawLists () {
  vensure(DrawListStack.length() != 0);
  GetCurrentDLS().resetAll();
  DrawListStack.setLength(DrawListStack.length()-1, false); // don't resize
  vassert(DrawListStack.length() > 0);
}


// code that tells windows we're High DPI aware so it doesn't scale our windows
// taken from Yamagi Quake II

typedef enum D3_PROCESS_DPI_AWARENESS {
  D3_PROCESS_DPI_UNAWARE = 0,
  D3_PROCESS_SYSTEM_DPI_AWARE = 1,
  D3_PROCESS_PER_MONITOR_DPI_AWARE = 2
} YQ2_PROCESS_DPI_AWARENESS;


void R_FuckOffShitdoze () {
#ifdef _WIN32
  /* for Vista, Win7 and Win8 */
  BOOL(WINAPI *SetProcessDPIAware)(void) = nullptr;
  HINSTANCE userDLL = LoadLibrary("USER32.DLL");
  if (userDLL) SetProcessDPIAware = (BOOL(WINAPI *)(void))(void *)GetProcAddress(userDLL, "SetProcessDPIAware");

  /* Win8.1 and later */
  HRESULT(WINAPI *SetProcessDpiAwareness)(D3_PROCESS_DPI_AWARENESS dpiAwareness) = nullptr;
  HINSTANCE shcoreDLL = LoadLibrary("SHCORE.DLL");
  if (shcoreDLL) SetProcessDpiAwareness = (HRESULT(WINAPI *)(D3_PROCESS_DPI_AWARENESS))(void *)GetProcAddress(shcoreDLL, "SetProcessDpiAwareness");

  if (SetProcessDpiAwareness) {
    GCon->Log(NAME_Init, "DPI GTFO 8.1+");
    SetProcessDpiAwareness(D3_PROCESS_PER_MONITOR_DPI_AWARE);
  } else if (SetProcessDPIAware) {
    GCon->Log(NAME_Init, "DPI GTFO 8");
    SetProcessDPIAware();
  }
#endif
}
