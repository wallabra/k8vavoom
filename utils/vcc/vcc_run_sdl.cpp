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
//**  Copyright (C) 2018 Ketmar Dark
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

#include "vcc_run_sdl.h"


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_CLASS(V, VideoMode);


// ////////////////////////////////////////////////////////////////////////// //
bool VVideoMode::canInit () {
#ifdef VCCRUN_HAS_SDL
  return true;
#else
  return false;
#endif
}


bool VVideoMode::hasOpenGL () {
#if defined(VCCRUN_HAS_SDL) && defined(VCCRUN_HAS_OPENGL)
  return true;
#else
  return false;
#endif
}


bool VVideoMode::isInitialized () {
  return false;
}


int VVideoMode::getWidth () {
  return 0;
}


int VVideoMode::getHeight () {
  return 0;
}


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_FUNCTION(VVideoMode, canInit) { RET_BOOL(VVideoMode::canInit()); }
IMPLEMENT_FUNCTION(VVideoMode, hasOpenGL) { RET_BOOL(VVideoMode::hasOpenGL()); }
IMPLEMENT_FUNCTION(VVideoMode, isInitialized) { RET_BOOL(VVideoMode::isInitialized()); }
IMPLEMENT_FUNCTION(VVideoMode, getWidth) { RET_INT(VVideoMode::getWidth()); }
IMPLEMENT_FUNCTION(VVideoMode, getHeight) { RET_INT(VVideoMode::getHeight()); }
