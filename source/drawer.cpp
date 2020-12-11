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
float VDrawer::mWindowAspect = 1.0f;
VMatrix4 VDrawer::LightViewMatrix[6];


static const TVec LightViewForward[6] = {
  TVec(-1.0f,  0.0f,  0.0f), // positive x
  TVec( 1.0f,  0.0f,  0.0f), // negative x
  TVec( 0.0f,  1.0f,  0.0f), // positive y
  TVec( 0.0f, -1.0f,  0.0f), // negative y
  TVec( 0.0f,  0.0f, -1.0f), // positive z
  TVec( 0.0f,  0.0f,  1.0f), // negative z
};

static const TVec LightViewUp[6] = {
  TVec( 0.0f, -1.0f,  0.0f), // positive x
  TVec( 0.0f, -1.0f,  0.0f), // negative x
  TVec( 0.0f,  0.0f, -1.0f), // positive y
  TVec( 0.0f,  0.0f,  1.0f), // negative y
  TVec( 0.0f, -1.0f,  0.0f), // positive z
  TVec( 0.0f, -1.0f,  0.0f), // negative z
};


//==========================================================================
//
//  CalculateLightViewMatrix
//
//==========================================================================
static void CalculateLightViewMatrix (VMatrix4 &ModelMat, const unsigned int facenum) {
  ModelMat.SetIdentity();

  const TVec forward = LightViewForward[facenum];
  const TVec left = LightViewUp[facenum].cross(forward).normalised();
  const TVec up = forward.cross(left).normalised();

  ModelMat.m[0][0] = left.x;
  ModelMat.m[0][1] = left.y;
  ModelMat.m[0][2] = left.z;
  ModelMat.m[1][0] = up.x;
  ModelMat.m[1][1] = up.y;
  ModelMat.m[1][2] = up.z;
  ModelMat.m[2][0] = forward.x;
  ModelMat.m[2][1] = forward.y;
  ModelMat.m[2][2] = forward.z;
}

//TVec VDrawer::GetLightViewForward (unsigned int facenum) noexcept { return TVec(LightViewMatrix[facenum].m[2][0], LightViewMatrix[facenum].m[2][1], LightViewMatrix[facenum].m[2][2]); }
TVec VDrawer::GetLightViewForward (unsigned int facenum) noexcept { return LightViewForward[facenum]; }
TVec VDrawer::GetLightViewUp (unsigned int facenum) noexcept { return TVec(LightViewMatrix[facenum].m[1][0], LightViewMatrix[facenum].m[1][1], LightViewMatrix[facenum].m[1][2]); }
TVec VDrawer::GetLightViewRight (unsigned int facenum) noexcept { return TVec(-LightViewMatrix[facenum].m[0][0], -LightViewMatrix[facenum].m[0][1], -LightViewMatrix[facenum].m[0][2]); }


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
  , DepthZeroOne(false)
  , canRenderShadowmaps(false)
  , updateFrame(0)
  , needCrosshair(false)
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
  mWindowAspect = 1.0f;

  vieworg = TVec(0.0f, 0.0f, 0.0f);
  viewangles = TAVec(0.0f, 0.0f, 0.0f);
  viewforward = TVec(0.0f, 0.0f, 0.0f);
  viewright = TVec(0.0f, 0.0f, 0.0f);
  viewup = TVec(0.0f, 0.0f, 0.0f);
  MirrorFlip = MirrorClip = false;

  for (unsigned int facenum = 0; facenum < 6; ++facenum) CalculateLightViewMatrix(LightViewMatrix[facenum], facenum);
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


//==========================================================================
//
//  VDrawer::CalcRealHexHeight
//
//==========================================================================
float VDrawer::CalcRealHexHeight (float h) noexcept {
  if (h < 1.0f) return 0.0f;
  h = h/3.0f*2.0f;
  const float dividery = 3.0f;
  const float hdiv = h/dividery;
  return h+hdiv;
}


//==========================================================================
//
//  VDrawer::CalcHexVertices
//
//==========================================================================
void VDrawer::CalcHexVertices (float vx[6], float vy[6], float x0, float y0, float w, float h) noexcept {
  h = h/3.0f*2.0f;
  const float dividery = 3.0f;
  const float hdiv = h/dividery;
  const float wdiv = w/2.0f;
  y0 += hdiv;
  const float yr0 = y0;
  const float yr1 = y0+h;
  const float ytop = yr0-hdiv;
  const float ybot = yr1+hdiv;
  vx[0] = x0;      vy[0] = yr0;
  vx[1] = x0+wdiv; vy[1] = ytop;
  vx[2] = x0+w;    vy[2] = yr0;
  vx[3] = x0+w;    vy[3] = yr1;
  vx[4] = x0+wdiv; vy[4] = ybot;
  vx[5] = x0;      vy[5] = yr1;
}


//==========================================================================
//
//  VDrawer::IsPointInsideHex
//
//==========================================================================
bool VDrawer::IsPointInsideHex (float x, float y, float x0, float y0, float w, float h) noexcept {
  if (w <= 0.0f || h <= 0.0f) return false;
  float vx[6];
  float vy[6];
  CalcHexVertices(vx, vy, x0, y0, w, h);
  return IsPointInside2DPoly(x, y, 6, vx, vy);
}


//==========================================================================
//
//  VDrawer::IsPointInside2DPolyInternal
//
//  polygon must be convex
//
//==========================================================================
bool VDrawer::IsPointInside2DPolyInternal (const float x, const float y, int vcount, const float vx[], size_t xstride, const float vy[], size_t ystride) noexcept {
  if (vcount < 2) return false;
  // this whole thing can be optimised by using differentials, but meh...
  float lastx = vx[(vcount-1)*xstride], lasty = vy[(vcount-1)*ystride];
  while (vcount--) {
    // calculates the area of the parallelogram of the three points
    // this is actually the same as the area of the triangle defined by the three points, multiplied by 2
    const float bx = vx[0];
    const float by = vy[0];
    const float area = (lastx-x)*(by-y)-(lasty-y)*(bx-x);
    if (area < 0) return false; // wrong side
    lastx = bx;
    lasty = by;
    vx += xstride;
    vy += ystride;
  }
  return true;
}


//==========================================================================
//
//  VDrawer::IsPointInside2DPoly
//
//  polygon must be convex
//
//==========================================================================
bool VDrawer::IsPointInside2DPoly (const float x, const float y, int vcount, const float vx[], const float vy[]) noexcept {
  return IsPointInside2DPolyInternal(x, y, vcount, vx, 1, vy, 1);
}


//==========================================================================
//
//  VDrawer::IsPointInside2DPoly
//
//  polygon must be convex
//  here `vxy` contains `x`,`y` pairs
//
//==========================================================================
bool VDrawer::IsPointInside2DPoly (const float x, const float y, int vcount, const float vxy[]) noexcept {
  return IsPointInside2DPolyInternal(x, y, vcount, vxy+0, 2, vxy+1, 2);
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
  const float zNear = 1.0f;
  if (!CanUseRevZ()) {
    // normal
    //glClearDepth(1.0f);
    //glDepthFunc(GL_LEQUAL);
    // const tanHalfFovy = tan(fovy/2.0f);
    // rd->fovx = aspect*tanHalfFovy;
    // rd->fovy = tanHalfFovy;
    ProjMat.SetZero();
    ProjMat[0][0] = 1.0f/rd->fovx;
    ProjMat[1][1] = 1.0f/rd->fovy;
    ProjMat[2][3] = -1.0f;
    //ProjMat[3][3] = 0.0f;
    if (!HaveDepthClamp && rlev && rlev->IsShadowVolumeRenderer() && !rlev->IsShadowMapRenderer()) {
      ProjMat[2][2] = -1.0f;
      ProjMat[3][2] = -2.0f;
    } else {
      float maxdist = gl_maxdist.asFloat();
      if (maxdist < 1.0f || !isFiniteF(maxdist)) maxdist = 32767.0f;
      if (DepthZeroOne) {
        ProjMat[2][2] = maxdist/(zNear-maxdist); // zFar/(zNear-zFar);
        ProjMat[3][2] = -(maxdist*zNear)/(maxdist-zNear); // -(zFar*zNear)/(zFar-zNear);
      } else {
        ProjMat[2][2] = -(maxdist+zNear)/(maxdist-zNear); // -(zFar+zNear)/(zFar-zNear);
        ProjMat[3][2] = -(2.0f*maxdist*zNear)/(maxdist-zNear); // -(2.0f*zFar*zNear)/(zFar-zNear);
      }
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
    ProjMat[3][2] = zNear;
  }
  //RestoreDepthFunc();
}


//==========================================================================
//
//  VDrawer::CalcShadowMapProjectionMatrix
//
//==========================================================================
void VDrawer::CalcShadowMapProjectionMatrix (VMatrix4 &ProjMat, float Radius, int awidth, int aheight, float aspect) {
  const float fov = 90.0f;
  const float fovx = tanf(DEG2RADF(fov)/2.0f);
  //if (aspect <= 0.0f || !isFiniteF(aspect)) aspect = 1.0f;
  const float fovy = fovx;//*aheight/awidth/aspect;
  const float zNear = 1.0f;
  ProjMat.SetZero();
  ProjMat[0][0] = 1.0f/fovx;
  ProjMat[1][1] = 1.0f/fovy;
  ProjMat[2][3] = -1.0f;
  //ProjMat[3][3] = 0.0f;
  if (Radius < 1.0f || !isFiniteF(Radius)) Radius = 32767.0f;
  if (/*DepthZeroOne*/false) {
    ProjMat[2][2] = Radius/(zNear-Radius); // zFar/(zNear-zFar);
    ProjMat[3][2] = -(Radius*zNear)/(Radius-zNear); // -(zFar*zNear)/(zFar-zNear);
  } else {
    ProjMat[2][2] = -(Radius+zNear)/(Radius-zNear); // -(zFar+zNear)/(zFar-zNear);
    ProjMat[3][2] = -(2.0f*Radius*zNear)/(Radius-zNear); // -(2.0f*zFar*zNear)/(zFar-zNear);
  }
}


//==========================================================================
//
//  VDrawer::CalcModelMatrix
//
//==========================================================================
void VDrawer::CalcModelMatrix (VMatrix4 &ModelMat, const TVec &origin, const TAVec &angles, bool MirrorFlip) {
  ModelMat.SetIdentity();
  ModelMat *= VMatrix4::RotateX(-90.0f); //glRotatef(-90, 1, 0, 0);
  ModelMat *= VMatrix4::RotateZ(90.0f); //glRotatef(90, 0, 0, 1);
  if (MirrorFlip) ModelMat *= VMatrix4::Scale(TVec(1, -1, 1)); //glScalef(1, -1, 1);
  ModelMat *= VMatrix4::RotateX(-angles.roll); //glRotatef(-viewangles.roll, 1, 0, 0);
  ModelMat *= VMatrix4::RotateY(-angles.pitch); //glRotatef(-viewangles.pitch, 0, 1, 0);
  ModelMat *= VMatrix4::RotateZ(-angles.yaw); //glRotatef(-viewangles.yaw, 0, 0, 1);
  ModelMat *= VMatrix4::Translate(-origin); //glTranslatef(-vieworg.x, -vieworg.y, -vieworg.z);
}


//==========================================================================
//
//  VDrawer::CalcSpotLightFaceView
//
//  do not even ask me. if was found by brute force.
//
//==========================================================================
void VDrawer::CalcSpotLightFaceView (VMatrix4 &ModelMat, const TVec &origin, unsigned int facenum) {
  if (facenum > 5) facenum = 0;
  /*
  ModelMat.SetIdentity();

  const TVec forward = LightViewForward[facenum].normalised();
  const TVec left = LightViewUp[facenum].cross(forward).normalised();
  const TVec up = forward.cross(left).normalised();

  ModelMat.m[0][0] = left.x;
  ModelMat.m[0][1] = left.y;
  ModelMat.m[0][2] = left.z;
  ModelMat.m[1][0] = up.x;
  ModelMat.m[1][1] = up.y;
  ModelMat.m[1][2] = up.z;
  ModelMat.m[2][0] = forward.x;
  ModelMat.m[2][1] = forward.y;
  ModelMat.m[2][2] = forward.z;
  */
  ModelMat = LightViewMatrix[facenum];
  ModelMat *= VMatrix4::Translate(-origin);
}


//==========================================================================
//
//  VDrawer::CalcOrthoMatrix
//
//==========================================================================
void VDrawer::CalcOrthoMatrix (VMatrix4 &OrthoMat, const float left, const float right, const float bottom, const float top) {
  const float nearVal = -666.0f;
  const float farVal = 666.0f;
  OrthoMat.SetZero();
  OrthoMat[0][0] = 2.0f/(right-left);
  OrthoMat[1][1] = 2.0f/(top-bottom); // [1+4]
  OrthoMat[2][2] = -2.0f/(farVal-nearVal); // [2+8]
  OrthoMat[3][0] = -(right+left)/(right-left); // [0+12]
  OrthoMat[3][1] = -(top+bottom)/(top-bottom); // [1+12]
  OrthoMat[3][2] = -(farVal+nearVal)/(farVal-nearVal); // [2+12]
  OrthoMat[3][3] = 1.0f; // [3+12]
}


//==========================================================================
//
//  VDrawer::SetOrthoProjection
//
//==========================================================================
void VDrawer::SetOrthoProjection (const float left, const float right, const float bottom, const float top) {
  VMatrix4 omat;
  CalcOrthoMatrix(omat, left, right, bottom, top);
  SetProjectionMatrix(omat);
}


//==========================================================================
//
//  VDrawer::DrawCrosshair
//
//==========================================================================
void VDrawer::DrawCrosshair () {
  if (!needCrosshair) return;
  needCrosshair = false;
  if (RendLev) RendLev->RenderCrosshair();
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
