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


//WARNING! don't forget to flush VBO on each shader uniform change! this includes glow changes (glow values aren't cached yet)
#define SADV_FLUSH_VBO()  do { \
  if (vboCountIdx) { \
    if (gl_dbg_vbo_adv_ambient) GCon->Logf(NAME_Debug, "flushing ambsurface VBO: vboCountIdx=%d", vboCountIdx); \
    vboAdvSurf.setupAttribNoEnable(attribPosition, 3); \
    p_glMultiDrawArrays(GL_TRIANGLE_FAN, vboStartInds.ptr(), vboCounters.ptr(), (GLsizei)vboCountIdx); \
    vboCountIdx = 0; \
  } \
} while (0)


#define SADV_DO_RENDER()  do { \
  if (surf->drawflags&surface_t::DF_NO_FACE_CULL) { \
    if (lastCullFace) { \
      SADV_FLUSH_VBO(); \
      lastCullFace = false; \
      glDisable(GL_CULL_FACE); \
    } \
  } else { \
    if (!lastCullFace) { \
      SADV_FLUSH_VBO(); \
      lastCullFace = true; \
      glEnable(GL_CULL_FACE); \
    } \
  } \
  currentActiveShader->UploadChangedUniforms(); \
  /* remember counter */ \
  vboCounters.ptr()[vboCountIdx] = (GLsizei)surf->count; \
  /* remember first index */ \
  vboStartInds.ptr()[vboCountIdx] = (GLint)surf->firstIndex; \
  /* advance array positions */ \
  ++vboCountIdx; \
} while (0)


#define SADV_CHECK_TEXTURE(shader_)  \
  const texinfo_t *currTexinfo = surf->texinfo; \
  do { \
    const bool textureChanged = lastTexinfo.needChange(*currTexinfo, updateFrame); \
    if (textureChanged) { \
      SADV_FLUSH_VBO(); \
      lastTexinfo.updateLastUsed(*currTexinfo); \
      SetTexture(currTexinfo->Tex, currTexinfo->ColorMap); \
      (shader_).SetTex(currTexinfo); \
    } \
  } while (0)
