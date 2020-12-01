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
// directly included from "r_light_adv.cpp"
//**************************************************************************
extern VCvarB r_darken;
extern VCvarB r_draw_mobjs;
extern VCvarB r_model_shadows;
extern VCvarB r_draw_pobj;
extern VCvarB r_chasecam;
extern VCvarB r_glow_flat;
extern VCvarB clip_use_1d_clipper;
extern VCvarB r_disable_world_update;

extern VCvarB gl_dbg_wireframe;

static VCvarB clip_adv_regions_shadow("clip_adv_regions_shadow", false, "Clip (1D) shadow regions?", CVAR_PreInit);
static VCvarB clip_adv_regions_light("clip_adv_regions_light", false, "Clip (1D) light regions?", CVAR_PreInit);

static VCvarB r_shadowvol_use_pofs("r_shadowvol_use_pofs", true, "Use PolygonOffset for shadow volumes to reduce some flickering (WARNING: BUGGY!)?", CVAR_Archive);
static VCvarF r_shadowvol_pofs("r_shadowvol_pofs", "20", "DEBUG");
static VCvarF r_shadowvol_pslope("r_shadowvol_pslope", "-0.2", "DEBUG");

static VCvarB r_shadowvol_optimise_flats("r_shadowvol_optimise_flats", true, "Drop some floors/ceilings that can't possibly cast shadow?", CVAR_Archive);
#ifdef VV_CHECK_1S_CAST_SHADOW
static VCvarB r_shadowvol_optimise_lines_1s("r_shadowvol_optimise_lines_1s", true, "Drop some 1s walls that can't possibly cast shadow? (glitchy)");
#endif

//VCvarI r_max_model_lights("r_max_model_lights", "32", "Maximum lights that can affect one model when we aren't using model shadows.", CVAR_Archive);
VCvarI r_max_model_shadows("r_max_model_shadows", "16", "Maximum number of shadows one model can cast.", CVAR_Archive);

VCvarI r_max_lights("r_max_lights", "256", "Total maximum lights for shadow volume renderer.", CVAR_Archive);
VCvarI r_dynlight_minimum("r_dynlight_minimum", "6", "Render at least this number of dynamic lights, regardless of total limit.", CVAR_Archive);

static VCvarI r_max_light_segs_all("r_max_light_segs_all", "-1", "Maximum light segments for all lights.", CVAR_Archive);
static VCvarI r_max_light_segs_one("r_max_light_segs_one", "-1", "Maximum light segments for one light.", CVAR_Archive);
// was 128, but with scissored light, there is no sense to limit it anymore
static VCvarI r_max_shadow_segs_all("r_max_shadow_segs_all", "-1", "Maximum shadow segments for all lights.", CVAR_Archive);
static VCvarI r_max_shadow_segs_one("r_max_shadow_segs_one", "-1", "Maximum shadow segments for one light.", CVAR_Archive);

VCvarF r_light_filter_static_coeff("r_light_filter_static_coeff", "0.2", "How close static lights should be to be filtered out?\n(0.1-0.3 is usually ok).", CVAR_Archive);
VCvarB r_allow_static_light_filter("r_allow_static_light_filter", true, "Allow filtering of static lights?", CVAR_Archive);
VCvarI r_static_light_filter_mode("r_static_light_filter_mode", "0", "Filter only decorations(0), or all lights(1)?", CVAR_Archive);

VCvarB dbg_adv_light_notrace_mark("dbg_adv_light_notrace_mark", false, "Mark notrace lights red?", CVAR_PreInit);

//static VCvarB r_advlight_opt_trace("r_advlight_opt_trace", true, "Try to skip shadow volumes when a light can cast no shadow.", CVAR_Archive|CVAR_PreInit);
static VCvarB r_advlight_opt_scissor("r_advlight_opt_scissor", true, "Use scissor rectangle to limit light overdraws.", CVAR_Archive);
// this is wrong for now
static VCvarB r_advlight_opt_frustum_full("r_advlight_opt_frustum_full", false, "Optimise 'light is in frustum' case.", CVAR_Archive);
static VCvarB r_advlight_opt_frustum_back("r_advlight_opt_frustum_back", false, "Optimise 'light is in frustum' case.", CVAR_Archive);

VCvarB r_advlight_opt_optimise_scissor("r_advlight_opt_optimise_scissor", true, "Optimise scissor with lit geometry bounds.", CVAR_Archive);
