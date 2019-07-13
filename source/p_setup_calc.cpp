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
//**
//**    Build nodes using ajbsp.
//**
//**************************************************************************
#include "gamedefs.h"
#include "filesys/fwaddefs.h"
#include "bsp/ajbsp/bsp.h"

#ifdef CLIENT
# include "drawer.h"
# include "cl_local.h"
#endif


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB loader_build_pvs("pvs_build_on_loading", false, "Build simple PVS on node rebuilding?", CVAR_Archive);
static VCvarB loader_build_pvs_force("__pvs_force", false, "Force PVS building for all nodebuilders?", CVAR_Archive);
static VCvarI loader_pvs_builder_threads("pvs_threads", "0", "Number of threads to use in PVS builder (0: use number of CPU cores online).", CVAR_Archive);

static VCvarB nodes_fast_mode("nodes_fast_mode", false, "Do faster rebuild, but generate worser BSP tree?", CVAR_Archive);
static VCvarB nodes_show_warnings("nodes_show_warnings", true, "Show various node builder warnings?", CVAR_Archive);

VCvarI nodes_builder_type("nodes_builder_type", "0", "Which internal node builder to use (0:auto; 1:ajbsp; 2:zdbsp)?", CVAR_Archive);
// default nodes builder for UDMF is still AJBSP, because it seems that i fixed UDMF bugs
static VCvarI nodes_builder_normal("nodes_builder_normal", "1", "Which internal node builder to use for non-UDMF maps (0:auto; 1:ajbsp; 2:zdbsp)?", CVAR_Archive);
static VCvarI nodes_builder_udmf("nodes_builder_udmf", "1", "Which internal node builder to use for UDMF maps (0:auto; 1:ajbsp; 2:zdbsp)?", CVAR_Archive);


//==========================================================================
//
//  ajRoundOffVertexI32
//
//  round vertex coordinates
//
//==========================================================================
static inline vint32 ajRoundoffVertexI32 (const double v) {
  return (vint32)(v*65536.0);
  //return vxs_ToFix16_16(v);
}


//==========================================================================
//
//  ajRoundOffVertex
//
//  round vertex coordinates
//
//==========================================================================
static inline float ajRoundoffVertex (const double v) {
  vint32 iv = ajRoundoffVertexI32(v);
  return (float)(((double)iv)/65536.0);
}


// ////////////////////////////////////////////////////////////////////////// //
#include "p_setup_nodes_aj.cpp"
#include "p_setup_nodes_zd.cpp"
#include "p_setup_pvs.cpp"


//==========================================================================
//
//  VLevel::GetNodesBuilder
//
//  valid only after `LevelFlags` are set
//
//==========================================================================
int VLevel::GetNodesBuilder () const {
  int nbt = nodes_builder_type;
  if (nbt <= 0) nbt = (LevelFlags&LF_TextMap ? nodes_builder_udmf : nodes_builder_normal);
  if (nbt == 2) return BSP_ZD;
  switch (nbt) {
    case 1: return BSP_AJ;
    case 2: return BSP_ZD;
  }
  // something strange, use old heuristic
  return (LevelFlags&LF_TextMap ? BSP_ZD : BSP_AJ);
}


//==========================================================================
//
//  VLevel::BuildNodes
//
//==========================================================================
void VLevel::BuildNodes () {
#ifdef CLIENT
  R_LdrMsgShowSecondary("BUILDING NODES...");
  R_PBarReset();
#endif
  switch (GetNodesBuilder()) {
    case BSP_AJ: BuildNodesAJ(); break;
    case BSP_ZD: BuildNodesZD(); break;
    default: Sys_Error("cannot determine nodes builder (this is internal engine bug!)");
  }
#ifdef CLIENT
  R_PBarUpdate("BSP", 42, 42, true); // final update
#endif
}
