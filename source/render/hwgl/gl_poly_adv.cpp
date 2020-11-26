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
#include "gl_poly_adv_render.h"


VCvarB gl_dbg_vbo_adv_ambient("gl_dbg_vbo_adv_ambient", false, "dump some VBO statistics for advrender abmient pass VBO utilisation?", CVAR_PreInit);


//TODO: re-check and reimplement smart rejects
//      also, "r_shadowvol_optimise_flats" seems to do the same as "gl_smart_reject_svol_flats"
VCvarB gl_smart_reject_shadows("gl_smart_reject_shadows", false, "Reject some surfaces that cannot possibly produce shadows?", CVAR_Archive);

VCvarB gl_smart_reject_svol_segs("gl_smart_reject_svol_segs", true, "Reject some surfaces that cannot possibly produce shadows?", CVAR_Archive);
VCvarB gl_smart_reject_svol_flats("gl_smart_reject_svol_flats", true, "Reject some surfaces that cannot possibly produce shadows?", CVAR_Archive);


// this also sorts by fade, so we can avoid resorting in fog pass
int glAdvRenderDrawListItemCmpByTextureAndFade (const void *a, const void *b, void * /*udata*/) {
  if (a == b) return 0;
  const surface_t *sa = *(const surface_t **)a;
  const surface_t *sb = *(const surface_t **)b;
  const texinfo_t *ta = sa->texinfo;
  const texinfo_t *tb = sb->texinfo;
  // sort by texture id (just use texture pointer)
  if ((uintptr_t)ta->Tex < (uintptr_t)ta->Tex) return -1;
  if ((uintptr_t)tb->Tex > (uintptr_t)tb->Tex) return 1;
  // by fade
  if (sa->Fade < sb->Fade) return -1;
  if (sa->Fade > sb->Fade) return 1;
  // and by colormap, why not?
  return ((int)ta->ColorMap)-((int)tb->ColorMap);
}


// full shader and texture sorting
int glAdvRenderDrawListItemCmpByShaderTexture (const void *a, const void *b, void * /*udata*/) {
  if (a == b) return 0;
  const surface_t *sa = *(const surface_t **)a;
  const surface_t *sb = *(const surface_t **)b;
  // shader class first
  const int stp = sa->shaderClass-sb->shaderClass;
  if (stp) return stp;
  // here shader classes are equal
  // invalid shader classes are sorted by element address
  if (sa->shaderClass < 0 || sa->shaderClass >= SFST_MAX) {
    if ((uintptr_t)a < (uintptr_t)b) return -1;
    if ((uintptr_t)a > (uintptr_t)b) return 1;
    return 0;
  }
  // here both surfaces are valid to render
  const texinfo_t *ta = sa->texinfo;
  const texinfo_t *tb = sb->texinfo;
  // sort by texture id (just use texture pointer)
  if ((uintptr_t)ta->Tex < (uintptr_t)ta->Tex) return -1;
  if ((uintptr_t)tb->Tex > (uintptr_t)tb->Tex) return 1;
  // by light level/color
  if (sa->Light < sb->Light) return -1;
  if (sa->Light > sb->Light) return 1;
  // and by colormap, why not?
  return ((int)ta->ColorMap)-((int)tb->ColorMap);
}


// only shaders and brightmap textures will be sorted
int glAdvRenderDrawListItemCmpByShaderBMTexture (const void *a, const void *b, void * /*udata*/) {
  if (a == b) return 0;
  const surface_t *sa = *(const surface_t **)a;
  const surface_t *sb = *(const surface_t **)b;
  // shader class first
  const int stp = sa->shaderClass-sb->shaderClass;
  if (stp) return stp;
  // here shader classes are equal
  // invalid shader classes are sorted by element address
  if (sa->shaderClass < 0 || sa->shaderClass >= SFST_MAX) {
    if ((uintptr_t)a < (uintptr_t)b) return -1;
    if ((uintptr_t)a > (uintptr_t)b) return 1;
    return 0;
  }
  // non-brightmap classes are sorted by light, and then by element address
  if (sa->shaderClass != SFST_BMap && sa->shaderClass != SFST_BMapGlow) {
    // by light level/color
    if (sa->Light < sb->Light) return -1;
    if (sa->Light > sb->Light) return 1;
    // same light level
    if ((uintptr_t)a < (uintptr_t)b) return -1;
    if ((uintptr_t)a > (uintptr_t)b) return 1;
    return 0;
  }
  // here both surfaces are valid to render
  const texinfo_t *ta = sa->texinfo;
  const texinfo_t *tb = sb->texinfo;
  // sort by texture id (just use texture pointer)
  if ((uintptr_t)ta->Tex < (uintptr_t)ta->Tex) return -1;
  if ((uintptr_t)tb->Tex > (uintptr_t)tb->Tex) return 1;
  // by light level/color
  if (sa->Light < sb->Light) return -1;
  if (sa->Light > sb->Light) return 1;
  // and by colormap, why not?
  return ((int)ta->ColorMap)-((int)tb->ColorMap);
}


//#include "gl_poly_adv_dirty_rect.cpp"
//#include "gl_poly_adv_sort_compare.cpp"

//#include "gl_poly_adv_render_pre.cpp"
//#include "gl_poly_adv_render_svol.cpp"
//#include "gl_poly_adv_render_vbomac.cpp"
//#include "gl_poly_adv_render_ambient.cpp"
//#include "gl_poly_adv_render_textures.cpp"
//#include "gl_poly_adv_render_fog.cpp"
//#include "gl_poly_adv_render_light.cpp"
