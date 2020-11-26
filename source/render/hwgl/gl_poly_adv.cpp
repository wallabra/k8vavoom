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
#include "gl_local.h"


extern VCvarB gl_enable_depth_bounds;
extern VCvarB gl_dbg_advlight_debug;
extern VCvarI gl_dbg_advlight_color;

static VCvarB gl_smart_dirty_rects("gl_smart_dirty_rects", true, "Use dirty rectangles list to check for stencil buffer dirtyness?", CVAR_Archive);

static VCvarB gl_dbg_vbo_adv_ambient("gl_dbg_vbo_adv_ambient", false, "dump some VBO statistics for advrender abmient pass VBO utilisation?", CVAR_PreInit);


#include "gl_poly_adv_shadowopt.cpp"
#include "gl_poly_adv_zpass.cpp"

#include "gl_poly_adv_dirty_rect.cpp"
#include "gl_poly_adv_sort_compare.cpp"

#include "gl_poly_adv_render_pre.cpp"
#include "gl_poly_adv_render_svol.cpp"
#include "gl_poly_adv_render_vbomac.cpp"
#include "gl_poly_adv_render_ambient.cpp"
#include "gl_poly_adv_render_textures.cpp"
#include "gl_poly_adv_render_fog.cpp"
#include "gl_poly_adv_render_light.cpp"
