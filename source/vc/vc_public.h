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
// should be included in "gamedefs.h"
#ifndef VAVOOM_VAVOOMC_PUBLIC_HEADER
#define VAVOOM_VAVOOMC_PUBLIC_HEADER

#ifdef VC_PUBLIC_WANT_CORE
# if defined(IN_VCC)
#  include "../../utils/vcc/vcc_vc.h"
# elif defined(VCC_STANDALONE_EXECUTOR)
#  include "../../vccrun/vcc_run_vc.h"
# else
#  include "../../libs/core/core.h"
#  include "../common.h"
# endif
#endif

#include "vc_progs.h"
#include "vc_location.h"
#include "vc_type.h"
#include "vc_member.h"
#include "vc_field.h"
#include "vc_property.h"
#include "vc_method.h"
#include "vc_constant.h"
#include "vc_struct.h"
#include "vc_state.h"
#include "vc_class.h"
#include "vc_package.h"
#include "vc_object.h"

#include "vc_error.h"

#endif
