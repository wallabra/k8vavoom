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
#include "ajbsp/bsp.h"

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


// ////////////////////////////////////////////////////////////////////////// //
#include "p_setup_nodes_aj.cpp"
#include "p_setup_nodes_zd.cpp"
#include "p_setup_pvs.cpp"


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
