//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš, dj_jl
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
#include <limits.h>
#include <float.h>

#include "../gamedefs.h"
#include "r_local.h"

// with this defined, it glitches
// you can see it on map01, for example, by firing imp fireball
//#define VV_LADV_CLIPCHECK_REGIONS_LIGHT
//#define VV_LADV_CLIPCHECK_REGIONS_SHADOW

#define VV_LADV_STRANGE_REGION_SORTING


/*
  possible shadow volume optimisations:

  for things like pillars (i.e. sectors that has solid walls facing outwards),
  we can construct a shadow silhouette. that is, all connected light-facing
  walls can be replaced with one. this will render much less sides.
  that is, we can effectively turn our pillar into cube.

  we can prolly use sector lines to render this (not segs). i think that it
  will be better to use sector lines to render shadow volumes in any case,
  'cause we don't really need to render one line in several segments.
  that is, one line split by BSP gives us two surfaces, which adds two
  unnecessary polygons to shadow volume. by using linedefs instead, we can
  avoid this. there is no need to create texture coordinates and surfaces
  at all: we can easily calculate all required vertices.

  note that we cannot do the same with floors and ceilings: they can have
  arbitrary shape.

  actually, we can solve this in another way. note that we need to extrude
  only silhouette edges. so we can collect surfaces from connected subsectors
  into "shape", and run silhouette extraction on it. this way we can extrude
  only a silhouette. and we can avoid rendering caps if our camera is not
  lie inside any shadow volume (and volume is not clipped by near plane).

  we can easily determine if the camera is inside a volume by checking if
  it lies inside any extruded subsector. as subsectors are convex, this is a
  simple set of point-vs-plane checks. first check if light and camera are
  on a different sides of a surface, and do costly checks only if they are.

  if the camera is outside of shadow volume, we can use faster z-pass method.

  if a light is behind a camera, we can move back frustum plane, so it will
  contain light origin, and clip everything behind it. the same can be done
  for all other frustum planes. or we can build full light-vs-frustum clip.
*/


extern VCvarB r_darken;
extern VCvarB r_draw_mobjs;
extern VCvarB r_model_shadows;
extern VCvarB r_draw_pobj;
extern VCvarB r_chasecam;
extern VCvarB r_glow_flat;
extern VCvarB clip_use_1d_clipper;
extern VCvarB r_disable_world_update;

extern VCvarI r_max_lights;
extern VCvarI r_max_light_segs_all;
extern VCvarI r_max_light_segs_one;
extern VCvarI r_max_shadow_segs_all;
extern VCvarI r_max_model_shadows;

extern VCvarB r_advlight_opt_optimise_scissor;

extern VCvarB dbg_adv_light_notrace_mark;

extern VCvarB gl_dbg_wireframe;

extern VCvarB dbg_adv_light_notrace_mark;


extern VCvarB r_adv_use_collector;
