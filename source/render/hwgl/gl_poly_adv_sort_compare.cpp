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
// directly included from "gl_poly_adv.cpp"
//**************************************************************************

enum {
  SFST_Normal,
  SFST_NormalGlow,
  SFST_BMap,
  SFST_BMapGlow,
  SFST_MAX,
};


extern "C" {
  // this also sorts by fade, so we can avoid resorting in fog pass
  static int drawListItemCmpByTextureAndFade (const void *a, const void *b, void * /*udata*/) {
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
  static int drawListItemCmpByShaderTexture (const void *a, const void *b, void * /*udata*/) {
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
  static int drawListItemCmpByShaderBMTexture (const void *a, const void *b, void * /*udata*/) {
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
}


//==========================================================================
//
//  ClassifySurfaceShader
//
//  do not call on invisible or texture-less surfaces
//
//==========================================================================
static VVA_OKUNUSED inline int ClassifySurfaceShader (const surface_t *surf) {
  if (r_brightmaps && surf->texinfo->Tex->Brightmap) return (surf->gp.isActive() ? SFST_BMapGlow : SFST_BMap);
  return (surf->gp.isActive() ? SFST_NormalGlow : SFST_Normal);
}


//==========================================================================
//
//  CheckListSortValidity
//
//==========================================================================
#if 0
static void CheckListSortValidity (TArray<surface_t *> &list, const char *listname) {
  const int len = list.length();
  const surface_t *const *sptr = list.ptr();
  // find first valid surface
  int idx;
  for (idx = 0; idx < len; ++idx, ++sptr) {
    const surface_t *surf = *sptr;
    if (surf->IsPlVisible()) break;
  }
  if (idx >= len) return; // nothing to do
  int phase = list[idx]->shaderClass;
  int previdx = idx;
  // check surfaces
  for (; idx < len; ++idx, ++sptr) {
    const surface_t *surf = *sptr;
    if (!surf->IsPlVisible()) continue; // viewer is in back side or on plane
    vassert(surf->texinfo->Tex);
    const int newphase = surf->shaderClass;
    if (newphase < phase) {
      Sys_Error("CheckListSortValidity (%s): shader order check failed at %d of %d; previdx is %d; prevphase is %d; phase is %d", listname, idx, len, previdx, phase, newphase);
    }
    previdx = idx;
    phase = newphase;
  }
}
#endif
