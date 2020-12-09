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
//**
//**  OpenGL driver, main module
//**
//**************************************************************************
#include <limits.h>
#include <float.h>
#include <stdarg.h>

#include "gl_local.h"
#include "../r_local.h" /* for VRenderLevelShared */


extern VCvarI gl_shadowmap_faster_check;

static VCvarB gl_shader_on_demand("gl_shader_on_demand", true, "Compile shaders on demand?", CVAR_PreInit|CVAR_Archive);


//==========================================================================
//
//  VOpenGLDrawer::registerShader
//
//==========================================================================
void VOpenGLDrawer::registerShader (VGLShader *shader) {
  if (developer) GCon->Logf(NAME_Dev, "registering shader '%s'", shader->progname);
  shader->owner = this;
  shader->next = shaderHead;
  shaderHead = shader;
}


//==========================================================================
//
//  VOpenGLDrawer::CompileShaders
//
//==========================================================================
void VOpenGLDrawer::CompileShaders (int glmajor, int glminor, bool canCubemaps) {
  for (VGLShader *shad = shaderHead; shad; shad = shad->next) {
    shad->compiled = false;
    if (shad->CheckOpenGLVersion(glmajor, glminor, canCubemaps)) {
      if (gl_shader_on_demand) shad->Compile();
    } else {
      GCon->Logf(NAME_Init, "skipped shader '%s' due to OpenGL version constraint", shad->progname);
    }
  }
}


//==========================================================================
//
//  VOpenGLDrawer::DestroyShaders
//
//==========================================================================
void VOpenGLDrawer::DestroyShaders () {
  for (VGLShader *shad = shaderHead; shad; shad = shad->next) shad->Unload();
  shaderHead = nullptr;
}


//==========================================================================
//
//  VOpenGLDrawer::DeactivateShader
//
//==========================================================================
void VOpenGLDrawer::DeactivateShader () {
  p_glUseProgramObjectARB(0);
  currentActiveShader = nullptr;
}


//==========================================================================
//
//  VOpenGLDrawer::glGetUniLoc
//
//==========================================================================
GLint VOpenGLDrawer::glGetUniLoc (const char *prog, GLhandleARB pid, const char *name, bool optional) {
  vassert(name);
  if (!pid) Sys_Error("shader program '%s' not loaded", prog);
  GLDRW_RESET_ERROR();
  GLint res = p_glGetUniformLocationARB(pid, name);
  //if (glGetError() != 0 || res == -1) Sys_Error("shader program '%s' has no uniform '%s'", prog, name);
  const GLenum glerr = glGetError();
  if (optional) {
    if (glerr != 0 || res == -1) res = -1;
  } else {
    if (glerr != 0 || res == -1) GCon->Logf(NAME_Error, "shader program '%s' has no uniform '%s'", prog, name);
  }
  return res;
}


//==========================================================================
//
//  VOpenGLDrawer::glGetAttrLoc
//
//==========================================================================
GLint VOpenGLDrawer::glGetAttrLoc (const char *prog, GLhandleARB pid, const char *name, bool optional) {
  vassert(name);
  if (!pid) Sys_Error("shader program '%s' not loaded", prog);
  GLDRW_RESET_ERROR();
  GLint res = p_glGetAttribLocationARB(pid, name);
  //if (glGetError() != 0 || res == -1) Sys_Error("shader program '%s' has no attribute '%s'", prog, name);
  const GLenum glerr = glGetError();
  if (optional) {
    if (glerr != 0 || res == -1) res = -1;
  } else {
    if (glerr != 0 || res == -1) GCon->Logf(NAME_Error, "shader program '%s' has no attribute '%s'", prog, name);
  }
  return res;
}



//**************************************************************************
//
// VOpenGLDrawer::VGLShader
//
//**************************************************************************

//==========================================================================
//
//  VOpenGLDrawer::VGLShader::CheckOpenGLVersion
//
//==========================================================================
bool VOpenGLDrawer::VGLShader::CheckOpenGLVersion (int major, int minor, bool canCubemaps) noexcept {
  if (forCubemaps && !canCubemaps) return false;
  const int ver = major*100+minor;
  switch (oglVersionCond) {
    case CondLess: return (ver < oglVersion);
    case CondLessEqu: return (ver <= oglVersion);
    case CondEqu: return (ver == oglVersion);
    case CondGreater: return (ver > oglVersion);
    case CondGreaterEqu: return (ver >= oglVersion);
    case CondNotEqu: return (ver != oglVersion);
  }
  return true;
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShader::Setup
//
//==========================================================================
void VOpenGLDrawer::VGLShader::MainSetup (VOpenGLDrawer *aowner, const char *aprogname, const char *aincdir, const char *avssrcfile, const char *afssrcfile) {
  next = nullptr;
  owner = aowner;
  progname = aprogname;
  incdir = aincdir;
  vssrcfile = avssrcfile;
  fssrcfile = afssrcfile;
  prog = -1;
  owner->registerShader(this);
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShader::Activate
//
//==========================================================================
void VOpenGLDrawer::VGLShader::Activate () {
  if (!compiled) {
    owner->p_glUseProgramObjectARB(0);
    Compile();
    compiled = true;
  }
  vassert(prog);
  if (owner->currentActiveShader != this) {
    owner->p_glUseProgramObjectARB(prog);
    owner->currentActiveShader = this;
  }
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShader::Deactivate
//
//==========================================================================
void VOpenGLDrawer::VGLShader::Deactivate () {
  owner->p_glUseProgramObjectARB(0);
  owner->currentActiveShader = nullptr;
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShader::IsActive
//
//==========================================================================
bool VOpenGLDrawer::VGLShader::IsActive () const noexcept {
  return (owner && owner->currentActiveShader == this);
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShader::Compile
//
//==========================================================================
void VOpenGLDrawer::VGLShader::Compile () {
  /*     if (gl_shader_on_demand) GCon->Logf(NAME_Init, "compiling shader '%s'", progname);
  else*/ if (developer) GCon->Logf(NAME_Dev, "compiling shader '%s'", progname);
  GLhandleARB VertexShader = owner->LoadShader(progname, incdir, GL_VERTEX_SHADER_ARB, vssrcfile, defines);
  GLhandleARB FragmentShader = owner->LoadShader(progname, incdir, GL_FRAGMENT_SHADER_ARB, fssrcfile, defines);
  prog = owner->CreateProgram(progname, VertexShader, FragmentShader);
  LoadUniforms();
}


//==========================================================================
//
//  VOpenGLDrawer::VGLShader::Unload
//
//==========================================================================
void VOpenGLDrawer::VGLShader::Unload () {
  if (prog) {
    if (developer) GCon->Logf(NAME_Dev, "unloading shader '%s'", progname);
    // actual program object will be destroyed elsewhere
    prog = 0;
    UnloadUniforms();
    if (owner && owner->currentActiveShader == this) owner->currentActiveShader = nullptr;
  }
}


#include "gl_shaddef.ci"


//==========================================================================
//
//  readTextFile
//
//==========================================================================
static VStr readTextFile (VStr fname) {
  VStream *strm = FL_OpenFileReadBaseOnly(fname);
  if (!strm) Sys_Error("Failed to open shader '%s'", *fname);
  int size = strm->TotalSize();
  if (size == 0) return VStr();
  VStr res;
  res.setLength(size, 0);
  strm->Serialise(res.GetMutableCharPointer(0), size);
  delete strm;
  return res;
}


//==========================================================================
//
//  isEmptyLine
//
//==========================================================================
static bool isEmptyLine (VStr s) {
  int pos = 0;
  while (pos < s.length()) {
    if ((vuint8)s[pos] > ' ') return false;
    ++pos;
  }
  return true;
}


//==========================================================================
//
//  isCommentLine
//
//==========================================================================
static bool isCommentLine (VStr s) {
  if (s.length() < 2) return false;
  int pos = 0;
  while (pos+1 < s.length()) {
    if (s[pos] == '/' && s[pos+1] == '/') return true;
    if ((vuint8)s[pos] > ' ') return false;
    ++pos;
  }
  return false;
}


//==========================================================================
//
//  isVersionLine
//
//==========================================================================
static bool isVersionLine (VStr s) {
  if (s.length() < 2) return false;
  int pos = 0;
  while (pos < s.length()) {
    if ((vuint8)s[pos] == '#') {
      ++pos;
      while (pos < s.length() && (vuint8)s[pos] <= ' ') ++pos;
      if (pos >= s.length()) return false;
      if (s[pos+0] == 'v' && s[pos+1] == 'e' && s[pos+2] == 'r' && s[pos+3] == 's' &&
          s[pos+4] == 'i' && s[pos+5] == 'o' && s[pos+6] == 'n') return true;
      return false;
    }
    if ((vuint8)s[pos] > ' ') return false;
    ++pos;
  }
  return false;
}


//==========================================================================
//
//  fixVersionLine
//
//  `s` MUST be valid version line
//
//==========================================================================
static VStr fixVersionLine (VStr s, int glVerMajor, int glVerMinor) {
  s = s.xstrip();
  auto spos = s.indexOf('#');
  vassert(spos >= 0);
  ++spos;
  while (spos < s.length() && s[spos] <= ' ') ++spos;
  while (spos < s.length() && s[spos] > ' ') ++spos;
  while (spos < s.length() && s[spos] <= ' ') ++spos;
  bool addVersions = false;
  if (s.right(s.length()-spos) == "4xx") {
    s.chopRight(s.length()-spos);
         if (glVerMajor >= 4) s += "400";
    else if (glVerMajor >= 3 && glVerMinor >= 1) s += "140";
    else if (glVerMajor >= 3) s += "130";
    else s += "120";
    //GCon->Logf(NAME_Debug, "GLVER4xx: %s", *s);
    addVersions = true;
  } else if (s.right(s.length()-spos) == "3xx") {
    s.chopRight(s.length()-spos);
         if (glVerMajor >= 4 || (glVerMajor >= 3 && glVerMinor >= 1)) s += "140";
    else if (glVerMajor >= 3) s += "130";
    else s += "120";
    //GCon->Logf(NAME_Debug, "GLVER3xx: %s", *s);
    addVersions = true;
  }
  s += "\n";
  if (addVersions) {
    //FIXME: this sux!
    if (glVerMajor >= 4) {
           if (glVerMajor >= 9) s += "#define GLVER_4_9\n";
      else if (glVerMajor >= 8) s += "#define GLVER_4_8\n";
      else if (glVerMajor >= 7) s += "#define GLVER_4_7\n";
      else if (glVerMajor >= 6) s += "#define GLVER_4_6\n";
      else if (glVerMajor >= 5) s += "#define GLVER_4_5\n";
      else if (glVerMajor >= 4) s += "#define GLVER_4_4\n";
      else if (glVerMajor >= 3) s += "#define GLVER_4_3\n";
      else if (glVerMajor >= 2) s += "#define GLVER_4_2\n";
      else if (glVerMajor >= 1) s += "#define GLVER_4_1\n";
      else s += "#define GLVER_4_0\n";
      s += "#define GLVER_MAJOR_4\n";
    } else if (glVerMinor >= 3) {
           if (glVerMajor >= 9) s += "#define GLVER_3_9\n";
      else if (glVerMajor >= 8) s += "#define GLVER_3_8\n";
      else if (glVerMajor >= 7) s += "#define GLVER_3_7\n";
      else if (glVerMajor >= 6) s += "#define GLVER_3_6\n";
      else if (glVerMajor >= 5) s += "#define GLVER_3_5\n";
      else if (glVerMajor >= 4) s += "#define GLVER_3_4\n";
      else if (glVerMajor >= 3) s += "#define GLVER_3_3\n";
      else if (glVerMajor >= 2) s += "#define GLVER_3_2\n";
      else if (glVerMajor >= 1) s += "#define GLVER_3_1\n";
      else s += "#define GLVER_3_0\n";
      s += "#define GLVER_MAJOR_3\n";
    }
  }
  return s;
}


//==========================================================================
//
//  getDirective
//
//==========================================================================
static VStr getDirective (VStr s) {
  int pos = 0;
  while (pos+1 < s.length()) {
    if (s[pos] == '$') {
      ++pos;
      while (pos < s.length() && (vuint8)s[pos] <= ' ') ++pos;
      if (pos >= s.length()) return VStr("$");
      int start = pos;
      while (pos < s.length() && (vuint8)s[pos] > ' ') ++pos;
      return s.mid(start, pos-start);
    }
    if ((vuint8)s[pos] > ' ') return VStr();
    ++pos;
  }
  return VStr();
}


//==========================================================================
//
//  getDirectiveArg
//
//==========================================================================
static VStr getDirectiveArg (VStr s) {
  int pos = 0;
  while (pos+1 < s.length()) {
    if (s[pos] == '$') {
      ++pos;
      while (pos < s.length() && (vuint8)s[pos] <= ' ') ++pos;
      if (pos >= s.length()) return VStr();
      while (pos < s.length() && (vuint8)s[pos] > ' ') ++pos;
      while (pos < s.length() && (vuint8)s[pos] <= ' ') ++pos;
      if (pos >= s.length()) return VStr();
      if (s[pos] == '"') {
        int start = ++pos;
        while (pos < s.length() && s[pos] != '"') ++pos;
        return s.mid(start, pos-start);
      } else {
        int start = pos;
        while (pos < s.length() && (vuint8)s[pos] > ' ') ++pos;
        return s.mid(start, pos-start);
      }
    }
    if ((vuint8)s[pos] > ' ') return VStr();
    ++pos;
  }
  return VStr();
}


//==========================================================================
//
//  VOpenGLDrawer::LoadShader
//
//==========================================================================
GLhandleARB VOpenGLDrawer::LoadShader (const char *progname, const char *incdircs, GLenum Type, VStr FileName, const TArray<VStr> &defines) {
  // load source file
  VStr ssrc = readTextFile(FileName);

  const char *sotype = (Type == GL_VERTEX_SHADER_ARB ? "vertex" : "fragment");

  // create shader object
  GLhandleARB Shader = p_glCreateShaderObjectARB(Type);
  if (!Shader) Sys_Error("Failed to create %s shader object for shader '%s'", sotype, progname);
  CreatedShaderObjects.Append(Shader);

  // build source text
  bool needToAddRevZ = CanUseRevZ();
  bool needToAddDefines = true;
  //bool needToAddDefines = (needToAddRevZ || defines.length() > 0);

  VStr incdir(incdircs);
  incdir = incdir.fixSlashes();
  if (!incdir.isEmpty()) {
    incdir = incdir.appendTrailingSlash();
  } else {
    incdir = FileName.extractFilePath();
  }

  //GCon->Logf(NAME_Debug, "<%s>: incdir: <%s> (%s); file: <%s>", progname, incdircs, *incdir, *FileName);

  // process $include, add defines
  //FIXME: nested "$include", and proper directive parsing
  VStr res;
  while (ssrc.length()) {
    // find line end
    int epos = 0;
    while (epos < ssrc.length() && ssrc[epos] != '\n') ++epos;
    if (epos < ssrc.length()) ++epos; // skip "\n"
    // extract line
    VStr line = ssrc.left(epos);
    ssrc.chopLeft(epos);
    if (isEmptyLine(line)) { res += line; continue; }
    if (isCommentLine(line)) { res += line; continue; }
    // add "reverse z" define
    VStr cmd = getDirective(line);
    if (cmd.length() == 0) {
      if (needToAddDefines) {
        if (isVersionLine(line)) { res += fixVersionLine(line, glVerMajor, glVerMinor); continue; }
        if (needToAddRevZ) { res += "#define VAVOOM_REVERSE_Z\n"; needToAddRevZ = false; }
        #ifdef GL4ES_HACKS
        res += "#define GL4ES_HACKS\n";
        #endif
        // add other defines
        for (int didx = 0; didx < defines.length(); ++didx) {
          VStr def = defines[didx];
          if (def.isEmpty()) continue;
          res += "#define ";
          res += def;
          res += "\n";
        }
        needToAddDefines = false;
      }
      res += line;
      continue;
    }
    if (cmd != "include") Sys_Error("%s", va("invalid directive \"%s\" in shader '%s'", *cmd, *FileName));
    VStr fname = getDirectiveArg(line);
    if (fname.length() == 0) Sys_Error("%s", va("directive \"%s\" in shader '%s' expects file name", *cmd, *FileName));
    VStr incf = readTextFile(incdir+fname);
    if (incf.length() && incf[incf.length()-1] != '\n') incf += '\n';
    incf += ssrc;
    ssrc = incf;
  }
  //if (defines.length()) GCon->Logf("%s", *res);
  //fprintf(stderr, "================ %s ================\n%s\n=================================\n", *FileName, *res);
  //vsShaderSrc = res;

  // upload source text
  const GLcharARB *ShaderText = *res;
  p_glShaderSourceARB(Shader, 1, &ShaderText, nullptr);

  // compile it
  p_glCompileShaderARB(Shader);

  // check id it is compiled successfully
  GLint Status;
  p_glGetObjectParameterivARB(Shader, GL_OBJECT_COMPILE_STATUS_ARB, &Status);
  if (!Status) {
    static GLcharARB LogText[32768];
    GLsizei LogLen;
    p_glGetInfoLogARB(Shader, sizeof(LogText)-1, &LogLen, LogText);
    LogText[LogLen] = 0;
    GCon->Logf(NAME_Error, "FAILED to compile %s shader '%s'!", sotype, progname);

    TArray<VStr> sp;
    VStr(LogText).Split('\n', sp);
    for (auto &&s : sp) GCon->Log(NAME_Error, *s);

    //GCon->Logf(NAME_Error, "%s\n", LogText);

    //GCon->Logf(NAME_Error, "====\n%s\n====", *res);
    VStr(res).Split('\n', sp);
    for (int f = 0; f < sp.length(); ++f) GCon->Logf(NAME_Error, "%5d: %s", f+1, *sp[f]);

    //fprintf(stderr, "================ %s ================\n%s\n=================================\n%s\b", *FileName, *res, LogText);
    //Sys_Error("%s", va("Failed to compile shader %s: %s", *FileName, LogText));
    Sys_Error("Failed to compile %s shader %s!", sotype, progname);
  }
  return Shader;
}


//==========================================================================
//
//  VOpenGLDrawer::CreateProgram
//
//==========================================================================
GLhandleARB VOpenGLDrawer::CreateProgram (const char *progname, GLhandleARB VertexShader, GLhandleARB FragmentShader) {
  // create program object
  GLDRW_RESET_ERROR();
  GLhandleARB Program = p_glCreateProgramObjectARB();
  if (!Program) Sys_Error("Failed to create program object");
  CreatedShaderObjects.Append(Program);

  // attach shaders
  p_glAttachObjectARB(Program, VertexShader);
  p_glAttachObjectARB(Program, FragmentShader);

  // link program
  GLDRW_RESET_ERROR();
  p_glLinkProgramARB(Program);

  // check if it was linked successfully
  GLint Status;
  p_glGetObjectParameterivARB(Program, GL_OBJECT_LINK_STATUS_ARB, &Status);
  if (!Status) {
    GLcharARB LogText[1024];
    GLsizei LogLen;
    p_glGetInfoLogARB(Program, sizeof(LogText)-1, &LogLen, LogText);
    LogText[LogLen] = 0;
    Sys_Error("Failed to link program '%s'", LogText);
  }

  GLenum glerr = glGetError();
  if (glerr != 0) Sys_Error("Failed to link program '%s' for unknown reason (error is %s)", progname, VGetGLErrorStr(glerr));

  return Program;
}


#define VV_CREATE_SMAP_SHADER(shad_,blur_,defs_)  \
  (shad_##Blur)[blur_].Setup(this); \
  (shad_##Blur)[blur_].defines = (shad_).defines; \
  (shad_##Blur)[blur_].defines.append(defs_); \
       if (gl_shadowmap_faster_check.asInt() == 1) (shad_##Blur)[blur_].defines.append("VV_CMP_FASTEST_CHECKS"); \
  else if (gl_shadowmap_faster_check.asInt() == 2) (shad_##Blur)[blur_].defines.append("VV_CMP_SHITTY_CHECKS"); \
  else if (gl_shadowmap_faster_check.asInt() == 3) (shad_##Blur)[blur_].defines.append("VV_CMP_SUPER_SHITTY_CHECKS"); \
  if (shittyBilinear) (shad_##Blur)[blur_].defines.append("VV_SMAP_SHITTY_BILINEAR");

#define VV_CREATE_SMAP_SHADER_LEVEL(blur_,defs_) \
  VV_CREATE_SMAP_SHADER(ShadowsLightSMap,blur_,defs_) \
  VV_CREATE_SMAP_SHADER(ShadowsLightSMapTex,blur_,defs_) \
  VV_CREATE_SMAP_SHADER(ShadowsLightSMapSpot,blur_,defs_) \
  VV_CREATE_SMAP_SHADER(ShadowsLightSMapSpotTex,blur_,defs_) \
  VV_CREATE_SMAP_SHADER(ShadowsModelLightSMap,blur_,defs_) \
  VV_CREATE_SMAP_SHADER(ShadowsModelLightSMapSpot,blur_,defs_)


//==========================================================================
//
//  VOpenGLDrawer::LoadShadowmapShaders
//
//==========================================================================
void VOpenGLDrawer::LoadShadowmapShaders () {
  bool shittyBilinear = false;
  VV_CREATE_SMAP_SHADER_LEVEL(SMAP_NOBLUR, "VV_SMAP_NOBLUR")
  VV_CREATE_SMAP_SHADER_LEVEL(SMAP_BLUR4BI, "VV_SMAP_BLUR4_BI")
  shittyBilinear = true;
  VV_CREATE_SMAP_SHADER_LEVEL(SMAP_BLUR4SBI, "VV_SMAP_BLUR4_BI")
  shittyBilinear = false;
  VV_CREATE_SMAP_SHADER_LEVEL(SMAP_BLUR4, "VV_SMAP_BLUR4")
  VV_CREATE_SMAP_SHADER_LEVEL(SMAP_BLUR8, "VV_SMAP_BLUR8")
  VV_CREATE_SMAP_SHADER_LEVEL(SMAP_BLUR8_FAST, "VV_SMAP_BLUR8_FAST")
  VV_CREATE_SMAP_SHADER_LEVEL(SMAP_BLUR16, "VV_SMAP_BLUR16")
  VV_CREATE_SMAP_SHADER_LEVEL(SMAP_BLUR16_FAST, "VV_SMAP_BLUR16_FAST")
}
