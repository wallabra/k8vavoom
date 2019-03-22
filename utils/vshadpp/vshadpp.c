#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistr.h>


// ////////////////////////////////////////////////////////////////////////// //
#define nullptr  NULL

void *xalloc (size_t size) {
  void *res = malloc(size ? size : size+1);
  if (!res) { fprintf(stderr, "FATAL: out of memory!\n"); abort(); }
  memset(res, 0, size ? size : size+1);
  return res;
}


char *xstrdup (const char *s) {
  if (!s) return xalloc(1);
  char *res = xalloc(strlen(s)+1);
  strcpy(res, s);
  return res;
}


char *xstrcat (const char *s0, const char *s1) {
  if (!s0 && !s1) return xalloc(1);
  if (!s0[0] && !s1[0]) return xalloc(1);
  char *res = xalloc(strlen(s0)+strlen(s1)+1);
  strcpy(res, s0);
  strcat(res, s1);
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
struct Parser {
  const char *text;
  const char *textend;
  const char *nextpos;
  const char *tkpos; // token start
  int isString;
  int isId;

  char *token; // malloced, because i don't care
};

typedef struct Parser Parser;


// ////////////////////////////////////////////////////////////////////////// //
Parser *prSavePos (const Parser *par) {
  Parser *res = xalloc(sizeof(Parser));
  memcpy(res, par, sizeof(Parser));
  if (par->token) {
    res->token = xalloc(strlen(par->token)+1);
    strcpy(res->token, par->token);
  }
  return res;
}


void prFreePos (Parser **pos) {
  if (!pos || !pos[0]) return;
  if (pos[0]->token) free(pos[0]->token);
  memset(pos[0], 0, sizeof(Parser));
  free(pos[0]);
  *pos = nullptr;
}


void prRestorePos (Parser *par, const Parser *pos) {
  if (par == pos) return;
  if (par->token) { free(par->token); par->token = nullptr; }
  memcpy(par, pos, sizeof(Parser));
  if (pos->token) {
    par->token = xalloc(strlen(pos->token)+1);
    strcpy(par->token, pos->token);
  }
}


// ////////////////////////////////////////////////////////////////////////// //
int prGetTokenLine (const Parser *par) {
  int res = 1;
  for (const char *s = par->text; s < par->tkpos; ++s) {
    if (*s == '\n') ++res;
  }
  return res;
}

int prGetTokenCol (const Parser *par) {
  int res = 1;
  for (const char *s = par->text; s < par->tkpos; ++s) {
    if (*s == '\n') res = 1; else ++res;
  }
  return res;
}


void prError (const Parser *par, const char *msg) {
  int line = prGetTokenLine(par);
  int col = prGetTokenCol(par);
  fprintf(stderr, "PARSE ERROR (%d:%d): %s\n", line, col, msg);
  abort();
}


int prIsEOF (const Parser *par) {
  return ((ptrdiff_t)(par->textend-par->nextpos) <= 0);
}


char prPeekCharAt (const Parser *par, int ofs) {
  if (ofs < 0) abort();
  int tlen = (int)(ptrdiff_t)(par->textend-par->nextpos);
  if (tlen <= ofs) return 0; //EOF
  char res = par->nextpos[ofs];
  if (!res) res = ' ';
  return res;
}

char prPeekChar (const Parser *par) { return prPeekCharAt(par, 0); }

void prSkipChar (Parser *par) {
  if (!prIsEOF(par)) ++par->nextpos;
}

char prGetChar (Parser *par) {
  if (prIsEOF(par)) return 0; //EOF
  char res = *par->nextpos++;
  if (!res) res = ' ';
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
void prSkipBlanks (Parser *par) {
  for (;;) {
    char ch = prPeekChar(par);
    if (!ch) break; //EOF
    if ((unsigned)(ch&0xff) <= ' ') { prSkipChar(par); continue; }
    if (ch == '/' && prPeekCharAt(par, 1) == '/') {
      // single-line comment
      while (ch && ch != '\n') ch = prGetChar(par);
      continue;
    }
    if (ch == '/' && prPeekCharAt(par, 1) == '*') {
      // multiline comment: skip comment start
      prSkipChar(par);
      prSkipChar(par);
      for (;;) {
        ch = prGetChar(par);
        if (!ch) break;
        if (ch == '*' && prPeekChar(par) == '/') { prSkipChar(par); break; }
      }
      continue;
    }
    if (ch == '/' && prPeekCharAt(par, 1) == '+') {
      // multiline nested comment: skip comment start
      prSkipChar(par);
      prSkipChar(par);
      int level = 1;
      for (;;) {
        ch = prGetChar(par);
        if (!ch) break;
        if (ch == '/' && prPeekChar(par) == '+') {
          prSkipChar(par);
          ++level;
        } else if (ch == '+' && prPeekChar(par) == '/') {
          prSkipChar(par);
          if (--level == 0) break;
        }
      }
      continue;
    }
    break;
  }
}


// returns `nullptr` on EOF
char *prGetToken (Parser *par) {
  if (par->token) { free(par->token); par->token = nullptr; }
  par->isString = 0;
  par->isId = 0;
  prSkipBlanks(par);
  //printf("<%c>\n", prPeekChar(par));
  if (prIsEOF(par)) return nullptr;

  char tokbuf[1024];
  size_t tkbpos = 0;

  par->tkpos = par->nextpos;
  char ch = prGetChar(par);

  // identifier?
  if ((ch >= '0' && ch <= '9') ||
      (ch >= 'A' && ch <= 'Z') ||
      (ch >= 'a' && ch <= 'z') ||
      ch == '_')
  {
    par->isId = 1;
    tokbuf[tkbpos++] = ch;
    for (;;) {
      ch = prPeekChar(par);
      if (!ch) break;
      if ((ch >= '0' && ch <= '9') ||
          (ch >= 'A' && ch <= 'Z') ||
          (ch >= 'a' && ch <= 'z') ||
          ch == '_')
      {
        if (tkbpos >= sizeof(tokbuf)-1) prError(par, "token too long");
        tokbuf[tkbpos++] = ch;
        prSkipChar(par);
      } else {
        break;
      }
    }
  }

  // string?
  else if (ch == '"' || ch == '\'') {
    par->isString = 1;
    char qch = ch;
    for (;;) {
      ch = prGetChar(par);
      if (!ch) prError(par, "unterminated string literal");
      if (ch == qch) break; // done
      if (ch == '\\') {
        ch = prGetChar(par);
        if (!ch) prError(par, "unterminated string literal");
        switch (ch) {
          case 'r': ch = '\r'; break;
          case 'n': ch = '\n'; break;
          case 't': ch = '\t'; break;
          case '\\': ch = '\\'; break;
          case '\'': ch = '\''; break;
          case '"': ch = '"'; break;
          default: prError(par, "invalid escape in string literal");
        }
      }
      if (tkbpos >= sizeof(tokbuf)-1) prError(par, "token too long");
      tokbuf[tkbpos++] = ch;
    }
  }

  // delimiter
  else {
    tokbuf[tkbpos++] = ch;
    tokbuf[tkbpos++] = 0;
  }

  tokbuf[tkbpos++] = 0;
  par->token = xalloc(tkbpos);
  memcpy(par->token, tokbuf, tkbpos);
  return par->token;
}


// ////////////////////////////////////////////////////////////////////////// //
void prExpect (Parser *par, const char *checktok) {
  const char *tk = prGetToken(par);
  if (!tk || strcmp(tk, checktok) || par->isString) {
    static char errmsg[1024];
    snprintf(errmsg, sizeof(errmsg), "expected '%s', got '%s'", checktok, (tk ? tk : "<EOF>"));
    prError(par, errmsg);
  }
}


char *prExpectId (Parser *par) {
  char *tk = prGetToken(par);
  if (!tk || !par->isId) {
    static char errmsg[1024];
    snprintf(errmsg, sizeof(errmsg), "expected string, got '%s'", (tk ? tk : "<EOF>"));
    prError(par, errmsg);
  }
  return tk;
}


char *prExpectString (Parser *par) {
  char *tk = prGetToken(par);
  if (!tk || !par->isString) {
    static char errmsg[1024];
    snprintf(errmsg, sizeof(errmsg), "expected string, got '%s'", (tk ? tk : "<EOF>"));
    prError(par, errmsg);
  }
  return tk;
}


int prCheck (Parser *par, const char *checktok) {
  Parser *state = prSavePos(par);
  const char *tk = prGetToken(par);
  if (tk && !par->isString && strcmp(tk, checktok) == 0) {
    prFreePos(&state);
    return 1;
  }
  prRestorePos(par, state);
  prFreePos(&state);
  return 0;
}


// ////////////////////////////////////////////////////////////////////////// //
void prSetup (Parser *par, const char *text) {
  memset(par, 0, sizeof(*par));
  par->text = text;
  par->nextpos = text;
  par->tkpos = text;
  par->textend = text+strlen(text);
}


// ////////////////////////////////////////////////////////////////////////// //
const char *getShitppType (const Parser *par, const char *glslType) {
  if (strcmp(glslType, "float") == 0) return "float";
  if (strcmp(glslType, "bool") == 0) return "bool";
  if (strcmp(glslType, "vec3") == 0) return "TVec";
  if (strcmp(glslType, "mat4") == 0) return "VMatrix4";
  // non-standard types
  if (strcmp(glslType, "vec4") == 0) return "float *";
  if (strcmp(glslType, "vec2") == 0) return "float *";
  if (strcmp(glslType, "mat3") == 0) return "float *";
  // samplers
  if (strcmp(glslType, "sampler2D") == 0) return "vuint32";
  // other
  if (strcmp(glslType, "TextureSTSet") == 0) return "TextureSTSet";
  if (strcmp(glslType, "LightmapSTSet") == 0) return "LightmapSTSet";
  if (strcmp(glslType, "PureLightmapSTSet") == 0) return "PureLightmapSTSet";
  if (strcmp(glslType, "FogSet") == 0) return "FogSet";
  // oops
  static char errmsg[1024];
  snprintf(errmsg, sizeof(errmsg), "unknown type `%s`", glslType);
  prError(par, errmsg);
  abort();
}


const char *getShitppAmp (const char *shitppType) {
  if (shitppType[strlen(shitppType)-1] == '*') return "";
  if (strcmp(shitppType, "TVec") == 0) return "&";
  if (strcmp(shitppType, "VMatrix4") == 0) return "&";
  return "";
}


// ////////////////////////////////////////////////////////////////////////// //
typedef struct LocInfo LocInfo;

struct LocInfo {
  char *name;
  int isAttr; // is this an attribute?
  char *glslType;
  const char *shitppType; // shitplusplus type name
  LocInfo *next;
};


LocInfo *loclist = nullptr;

LocInfo *newLoc (const char *aname) {
  LocInfo *loc = xalloc(sizeof(LocInfo));
  loc->name = xalloc(strlen(aname)+1);
  strcpy(loc->name, aname);
  if (strcmp(aname, "_") == 0) loc->name[0] = 0;
  // append to list
  LocInfo *last = loclist;
  if (last) {
    while (last->next) last = last->next;
    last->next = loc;
  } else {
    loclist = loc;
  }
  return loc;
}


// ////////////////////////////////////////////////////////////////////////// //
char *shadname = nullptr;
char *vssrc = nullptr;
char *fssrc = nullptr;


void clearAll () {
  while (loclist) {
    LocInfo *curr = loclist;
    loclist = curr->next;
    free(curr->name);
    free(curr->glslType);
  }
  free(shadname); shadname = nullptr;
  free(vssrc); vssrc = nullptr;
  free(fssrc); fssrc = nullptr;
}


// ////////////////////////////////////////////////////////////////////////// //
void genHeader (FILE *fo) {
  fprintf(fo, "  class VShaderDef_%s : public VGLShader {\n", shadname);
  fprintf(fo, "  public:\n");

  for (const LocInfo *loc = loclist; loc; loc = loc->next) {
    if (strcmp(loc->glslType, "TextureSTSet") == 0) {
      fprintf(fo, "    // texture\n");
      fprintf(fo, "    GLint loc_%sSAxis;\n", loc->name);
      fprintf(fo, "    GLint loc_%sTAxis;\n", loc->name);
      fprintf(fo, "    GLint loc_%sSOffs;\n", loc->name);
      fprintf(fo, "    GLint loc_%sTOffs;\n", loc->name);
      fprintf(fo, "    GLint loc_%sTexIW;\n", loc->name);
      fprintf(fo, "    GLint loc_%sTexIH;\n", loc->name);
    } else if (strcmp(loc->glslType, "TextureSTSetNoSize") == 0) {
      fprintf(fo, "    // texture\n");
      fprintf(fo, "    GLint loc_%sSAxis;\n", loc->name);
      fprintf(fo, "    GLint loc_%sTAxis;\n", loc->name);
      fprintf(fo, "    GLint loc_%sSOffs;\n", loc->name);
      fprintf(fo, "    GLint loc_%sTOffs;\n", loc->name);
    } else if (strcmp(loc->glslType, "LightmapSTSet") == 0) {
      fprintf(fo, "    // lightmap\n");
      fprintf(fo, "    GLint loc_%sTexMinS;\n", loc->name);
      fprintf(fo, "    GLint loc_%sTexMinT;\n", loc->name);
      fprintf(fo, "    GLint loc_%sCacheS;\n", loc->name);
      fprintf(fo, "    GLint loc_%sCacheT;\n", loc->name);
    } else if (strcmp(loc->glslType, "PureLightmapSTSet") == 0) {
      fprintf(fo, "    // pure lightmap (without texture)\n");
      fprintf(fo, "    GLint loc_%sSAxis;\n", loc->name);
      fprintf(fo, "    GLint loc_%sTAxis;\n", loc->name);
      fprintf(fo, "    GLint loc_%sSOffs;\n", loc->name);
      fprintf(fo, "    GLint loc_%sTOffs;\n", loc->name);
      fprintf(fo, "    GLint loc_%sTexMinS;\n", loc->name);
      fprintf(fo, "    GLint loc_%sTexMinT;\n", loc->name);
      fprintf(fo, "    GLint loc_%sCacheS;\n", loc->name);
      fprintf(fo, "    GLint loc_%sCacheT;\n", loc->name);
    } else if (strcmp(loc->glslType, "FogSet") == 0) {
      fprintf(fo, "    // fog variable locations\n");
      fprintf(fo, "    GLint loc_%sFogEnabled; // may be missing\n", loc->name);
      fprintf(fo, "    GLint loc_%sFogColour;\n", loc->name);
      fprintf(fo, "    GLint loc_%sFogStart;\n", loc->name);
      fprintf(fo, "    GLint loc_%sFogEnd;\n", loc->name);
    } else {
      fprintf(fo, "    GLint loc_%s; // %s %s -> %s\n", loc->name, (loc->isAttr ? "attribute" : "uniform"), loc->glslType, loc->shitppType);
    }
  }

  fprintf(fo, "\n");
  fprintf(fo, "  public:\n");
  fprintf(fo, "    VShaderDef_%s ();\n", shadname);
  fprintf(fo, "\n");
  fprintf(fo, "    virtual void Setup (VOpenGLDrawer *aowner) override;\n");
  fprintf(fo, "    virtual void LoadUniforms () override;\n");
  fprintf(fo, "\n");

  // generate setters
  for (const LocInfo *loc = loclist; loc; loc = loc->next) {
    if (!loc->isAttr) {
      if (strcmp(loc->glslType, "float") == 0) {
        fprintf(fo, "    inline void Set%s (const %s %sv) { ", loc->name, loc->shitppType, getShitppAmp(loc->shitppType));
        fprintf(fo, "if (loc_%s >= 0) ", loc->name);
        fprintf(fo, "owner->p_glUniform1fARB(loc_%s, v);", loc->name);
        fprintf(fo, " }\n");
      } else if (strcmp(loc->glslType, "bool") == 0) {
        fprintf(fo, "    inline void Set%s (const %s %sv) { ", loc->name, loc->shitppType, getShitppAmp(loc->shitppType));
        fprintf(fo, "if (loc_%s >= 0) ", loc->name);
        fprintf(fo, "owner->p_glUniform1iARB(loc_%s, (v ? GL_TRUE : GL_FALSE));", loc->name);
        fprintf(fo, " }\n");
      } else if (strcmp(loc->glslType, "sampler2D") == 0) {
        fprintf(fo, "    inline void Set%s (const %s %sv) { ", loc->name, loc->shitppType, getShitppAmp(loc->shitppType));
        fprintf(fo, "if (loc_%s >= 0) ", loc->name);
        fprintf(fo, "owner->p_glUniform1iARB(loc_%s, (GLint)v);", loc->name);
        fprintf(fo, " }\n");
      } else if (strcmp(loc->glslType, "vec3") == 0) {
        fprintf(fo, "    inline void Set%s (const %s %sv) { ", loc->name, loc->shitppType, getShitppAmp(loc->shitppType));
        fprintf(fo, "if (loc_%s >= 0) ", loc->name);
        fprintf(fo, "owner->p_glUniform3fvARB(loc_%s, 1, &v.x);", loc->name);
        fprintf(fo, " }\n");
        fprintf(fo, "    inline void Set%s (const float x, const float y, const float z) { ", loc->name);
        fprintf(fo, "if (loc_%s >= 0) ", loc->name);
        fprintf(fo, "owner->p_glUniform3fARB(loc_%s, x, y, z);", loc->name);
        fprintf(fo, " }\n");
      } else if (strcmp(loc->glslType, "mat4") == 0) {
        fprintf(fo, "    inline void Set%s (const %s %sv) { ", loc->name, loc->shitppType, getShitppAmp(loc->shitppType));
        fprintf(fo, "if (loc_%s >= 0) ", loc->name);
        fprintf(fo, "owner->p_glUniformMatrix4fvARB(loc_%s, 1, GL_FALSE, v[0]);", loc->name);
        fprintf(fo, " }\n");
        fprintf(fo, "    inline void Set%s (const float * v) { ", loc->name);
        fprintf(fo, "if (loc_%s >= 0) ", loc->name);
        fprintf(fo, "owner->p_glUniformMatrix4fvARB(loc_%s, 1, GL_FALSE, v);", loc->name);
        fprintf(fo, " }\n");
      } else if (strcmp(loc->glslType, "vec4") == 0) {
        fprintf(fo, "    inline void Set%s (const %s %sv) { ", loc->name, loc->shitppType, getShitppAmp(loc->shitppType));
        fprintf(fo, "if (loc_%s >= 0) ", loc->name);
        fprintf(fo, "owner->p_glUniform4fvARB(loc_%s, 1, v);", loc->name);
        fprintf(fo, " }\n");
        fprintf(fo, "    inline void Set%s (const float x, const float y, const float z, const float w) { ", loc->name);
        fprintf(fo, "if (loc_%s >= 0) ", loc->name);
        fprintf(fo, "owner->p_glUniform4fARB(loc_%s, x, y, z, w);", loc->name);
        fprintf(fo, " }\n");
      } else if (strcmp(loc->glslType, "vec2") == 0) {
        fprintf(fo, "    inline void Set%s (const %s %sv) { ", loc->name, loc->shitppType, getShitppAmp(loc->shitppType));
        fprintf(fo, "if (loc_%s >= 0) ", loc->name);
        fprintf(fo, "owner->p_glUniform2fvARB(loc_%s, 1, v);", loc->name);
        fprintf(fo, " }\n");
        fprintf(fo, "    inline void Set%s (const float x, const float y) { ", loc->name);
        fprintf(fo, "if (loc_%s >= 0) ", loc->name);
        fprintf(fo, "owner->p_glUniform2fARB(loc_%s, x, y);", loc->name);
        fprintf(fo, " }\n");
      } else if (strcmp(loc->glslType, "mat3") == 0) {
        fprintf(fo, "    inline void Set%s (const %s %sv) { ", loc->name, loc->shitppType, getShitppAmp(loc->shitppType));
        fprintf(fo, "if (loc_%s >= 0) ", loc->name);
        fprintf(fo, "owner->p_glUniformMatrix3fvARB(loc_%s, 1, GL_FALSE, v);", loc->name);
        fprintf(fo, " }\n");
      } else if (strcmp(loc->glslType, "TextureSTSet") == 0) {
        fprintf(fo, "    inline void Set%sTex (const texinfo_t *textr) {\n", loc->name);
        fprintf(fo, "      if (loc_%sSAxis >= 0) owner->p_glUniform3fvARB(loc_%sSAxis, 1, &textr->saxis.x);\n", loc->name, loc->name);
        fprintf(fo, "      if (loc_%sSOffs >= 0) owner->p_glUniform1fARB(loc_%sSOffs, textr->soffs);\n", loc->name, loc->name);
        fprintf(fo, "      if (loc_%sTexIW >= 0) owner->p_glUniform1fARB(loc_%sTexIW, owner->tex_iw);\n", loc->name, loc->name);
        fprintf(fo, "      if (loc_%sTAxis >= 0) owner->p_glUniform3fvARB(loc_%sTAxis, 1, &textr->taxis.x);\n", loc->name, loc->name);
        fprintf(fo, "      if (loc_%sTOffs >= 0) owner->p_glUniform1fARB(loc_%sTOffs, textr->toffs);\n", loc->name, loc->name);
        fprintf(fo, "      if (loc_%sTexIH >= 0) owner->p_glUniform1fARB(loc_%sTexIH, owner->tex_ih);\n", loc->name, loc->name);
        fprintf(fo, "    }\n");
      } else if (strcmp(loc->glslType, "TextureSTSetNoSize") == 0) {
        fprintf(fo, "    inline void Set%sTexNoSize (const texinfo_t *textr) {\n", loc->name);
        fprintf(fo, "      if (loc_%sSAxis >= 0) owner->p_glUniform3fvARB(loc_%sSAxis, 1, &textr->saxis.x);\n", loc->name, loc->name);
        fprintf(fo, "      if (loc_%sSOffs >= 0) owner->p_glUniform1fARB(loc_%sSOffs, textr->soffs);\n", loc->name, loc->name);
        fprintf(fo, "      if (loc_%sTAxis >= 0) owner->p_glUniform3fvARB(loc_%sTAxis, 1, &textr->taxis.x);\n", loc->name, loc->name);
        fprintf(fo, "      if (loc_%sTOffs >= 0) owner->p_glUniform1fARB(loc_%sTOffs, textr->toffs);\n", loc->name, loc->name);
        fprintf(fo, "    }\n");
      } else if (strcmp(loc->glslType, "LightmapSTSet") == 0) {
        fprintf(fo, "    inline void Set%sLMap (const surface_t *surf, const surfcache_t *cache) {\n", loc->name);
        fprintf(fo, "      if (loc_%sTexMinS >= 0) owner->p_glUniform1fARB(loc_%sTexMinS, surf->texturemins[0]);\n", loc->name, loc->name);
        fprintf(fo, "      if (loc_%sTexMinT >= 0) owner->p_glUniform1fARB(loc_%sTexMinT, surf->texturemins[1]);\n", loc->name, loc->name);
        fprintf(fo, "      if (loc_%sCacheS >= 0) owner->p_glUniform1fARB(loc_%sCacheS, cache->s);\n", loc->name, loc->name);
        fprintf(fo, "      if (loc_%sCacheT >= 0) owner->p_glUniform1fARB(loc_%sCacheT, cache->t);\n", loc->name, loc->name);
        fprintf(fo, "    }\n");
      } else if (strcmp(loc->glslType, "PureLightmapSTSet") == 0) {
        fprintf(fo, "    inline void Set%sLMapOnly (const texinfo_t *textr, const surface_t *surf, const surfcache_t *cache) {\n", loc->name);
        fprintf(fo, "      if (loc_%sSAxis >= 0) owner->p_glUniform3fvARB(loc_%sSAxis, 1, &textr->saxis.x);\n", loc->name, loc->name);
        fprintf(fo, "      if (loc_%sSOffs >= 0) owner->p_glUniform1fARB(loc_%sSOffs, textr->soffs);\n", loc->name, loc->name);
        fprintf(fo, "      if (loc_%sTAxis >= 0) owner->p_glUniform3fvARB(loc_%sTAxis, 1, &textr->taxis.x);\n", loc->name, loc->name);
        fprintf(fo, "      if (loc_%sTOffs >= 0) owner->p_glUniform1fARB(loc_%sTOffs, textr->toffs);\n", loc->name, loc->name);
        fprintf(fo, "      if (loc_%sTexMinS >= 0) owner->p_glUniform1fARB(loc_%sTexMinS, surf->texturemins[0]);\n", loc->name, loc->name);
        fprintf(fo, "      if (loc_%sTexMinT >= 0) owner->p_glUniform1fARB(loc_%sTexMinT, surf->texturemins[1]);\n", loc->name, loc->name);
        fprintf(fo, "      if (loc_%sCacheS >= 0) owner->p_glUniform1fARB(loc_%sCacheS, cache->s);\n", loc->name, loc->name);
        fprintf(fo, "      if (loc_%sCacheT >= 0) owner->p_glUniform1fARB(loc_%sCacheT, cache->t);\n", loc->name, loc->name);
        fprintf(fo, "    }\n");
      } else if (strcmp(loc->glslType, "FogSet") == 0) {
        fprintf(fo, "    inline void Set%sFogFade (vuint32 Fade, float Alpha) {\n", loc->name);
        fprintf(fo, "      if (Fade) {\n");
        fprintf(fo, "        if (loc_%sFogEnabled >= 0) owner->p_glUniform1iARB(loc_%sFogEnabled, GL_TRUE);\n", loc->name, loc->name);
        fprintf(fo, "        owner->p_glUniform4fARB(loc_%sFogColour,\n", loc->name);
        fprintf(fo, "          ((Fade>>16)&255)/255.0f,\n");
        fprintf(fo, "          ((Fade>>8)&255)/255.0f,\n");
        fprintf(fo, "          (Fade&255)/255.0f, Alpha);\n");
        fprintf(fo, "        //owner->p_glUniform1fARB(loc_%sFogDensity, Fade == FADE_LIGHT ? 0.3f : r_fog_density);\n", loc->name);
        fprintf(fo, "        owner->p_glUniform1fARB(loc_%sFogStart, Fade == FADE_LIGHT ? 1.0f : r_fog_start);\n", loc->name);
        fprintf(fo, "        owner->p_glUniform1fARB(loc_%sFogEnd, Fade == FADE_LIGHT ? 1024.0f*r_fade_factor : r_fog_end);\n", loc->name);
        fprintf(fo, "      } else {\n");
        fprintf(fo, "        if (loc_%sFogEnabled >= 0) owner->p_glUniform1iARB(loc_%sFogEnabled, GL_FALSE);\n", loc->name, loc->name);
        fprintf(fo, "      }\n");
        fprintf(fo, "    }\n");
      } else {
        fprintf(stderr, "FATAL: cannot emit setter for GLSL type '%s'\n", loc->glslType);
        abort();
      }
    } else {
      // attrs
      if (strcmp(loc->glslType, "float") == 0) {
        fprintf(fo, "    inline void Set%s (const %s %sv) { ", loc->name, loc->shitppType, getShitppAmp(loc->shitppType));
        fprintf(fo, "if (loc_%s >= 0) ", loc->name);
        fprintf(fo, "owner->p_glVertexAttrib1fARB(loc_%s, v);", loc->name);
        fprintf(fo, " }\n");
      } else if (strcmp(loc->glslType, "vec2") == 0) {
        fprintf(fo, "    inline void Set%s (const %s %sv) { ", loc->name, loc->shitppType, getShitppAmp(loc->shitppType));
        fprintf(fo, "if (loc_%s >= 0) ", loc->name);
        fprintf(fo, "owner->p_glVertexAttrib2fvARB(loc_%s, v);", loc->name);
        fprintf(fo, " }\n");
        fprintf(fo, "    inline void Set%s (const float x, const float y) { ", loc->name);
        fprintf(fo, "if (loc_%s >= 0) ", loc->name);
        fprintf(fo, "owner->p_glVertexAttrib2fARB(loc_%s, x, y);", loc->name);
        fprintf(fo, " }\n");
      } else if (strcmp(loc->glslType, "vec4") == 0) {
        fprintf(fo, "    inline void Set%s (const %s %sv) { ", loc->name, loc->shitppType, getShitppAmp(loc->shitppType));
        fprintf(fo, "if (loc_%s >= 0) ", loc->name);
        fprintf(fo, "owner->p_glVertexAttrib4fvARB(loc_%s, v);", loc->name);
        fprintf(fo, " }\n");
        fprintf(fo, "    inline void Set%s (const float x, const float y, const float z, const float w) { ", loc->name);
        fprintf(fo, "if (loc_%s >= 0) ", loc->name);
        fprintf(fo, "owner->p_glVertexAttrib4fARB(loc_%s, x, y, z, w);", loc->name);
        fprintf(fo, " }\n");
      } else if (strcmp(loc->glslType, "vec3") == 0) {
        fprintf(fo, "    inline void Set%s (const %s %sv) { ", loc->name, loc->shitppType, getShitppAmp(loc->shitppType));
        fprintf(fo, "if (loc_%s >= 0) ", loc->name);
        fprintf(fo, "owner->p_glVertexAttrib3fvARB(loc_%s, &v.x);", loc->name);
        fprintf(fo, " }\n");
        fprintf(fo, "    inline void Set%s (const float x, const float y, const float z) { ", loc->name);
        fprintf(fo, "if (loc_%s >= 0) ", loc->name);
        fprintf(fo, "owner->p_glVertexAttrib3fARB(loc_%s, x, y, z);", loc->name);
        fprintf(fo, " }\n");
      } else {
        fprintf(stderr, "FATAL: cannot emit attribute setter for GLSL type '%s'\n", loc->glslType);
        abort();
      }
    }
  }

  fprintf(fo, "  };\n");
}


// ////////////////////////////////////////////////////////////////////////// //
void genCode (FILE *fo) {
  // generate constructor
  fprintf(fo, "VOpenGLDrawer::VShaderDef_%s::VShaderDef_%s ()\n", shadname, shadname);
  fprintf(fo, "  : VGLShader()\n");
  for (const LocInfo *loc = loclist; loc; loc = loc->next) {
    if (strcmp(loc->glslType, "TextureSTSet") == 0) {
      fprintf(fo, "  , loc_%sSAxis(-1)\n", loc->name);
      fprintf(fo, "  , loc_%sTAxis(-1)\n", loc->name);
      fprintf(fo, "  , loc_%sSOffs(-1)\n", loc->name);
      fprintf(fo, "  , loc_%sTOffs(-1)\n", loc->name);
      fprintf(fo, "  , loc_%sTexIW(-1)\n", loc->name);
      fprintf(fo, "  , loc_%sTexIH(-1)\n", loc->name);
    } else if (strcmp(loc->glslType, "TextureSTSetNoSize") == 0) {
      fprintf(fo, "  , loc_%sSAxis(-1)\n", loc->name);
      fprintf(fo, "  , loc_%sTAxis(-1)\n", loc->name);
      fprintf(fo, "  , loc_%sSOffs(-1)\n", loc->name);
      fprintf(fo, "  , loc_%sTOffs(-1)\n", loc->name);
    } else if (strcmp(loc->glslType, "LightmapSTSet") == 0) {
      fprintf(fo, "  , loc_%sTexMinS(-1)\n", loc->name);
      fprintf(fo, "  , loc_%sTexMinT(-1)\n", loc->name);
      fprintf(fo, "  , loc_%sCacheS(-1)\n", loc->name);
      fprintf(fo, "  , loc_%sCacheT(-1)\n", loc->name);
    } else if (strcmp(loc->glslType, "PureLightmapSTSet") == 0) {
      fprintf(fo, "  , loc_%sSAxis(-1)\n", loc->name);
      fprintf(fo, "  , loc_%sTAxis(-1)\n", loc->name);
      fprintf(fo, "  , loc_%sSOffs(-1)\n", loc->name);
      fprintf(fo, "  , loc_%sTOffs(-1)\n", loc->name);
      fprintf(fo, "  , loc_%sTexMinS(-1)\n", loc->name);
      fprintf(fo, "  , loc_%sTexMinT(-1)\n", loc->name);
      fprintf(fo, "  , loc_%sCacheS(-1)\n", loc->name);
      fprintf(fo, "  , loc_%sCacheT(-1)\n", loc->name);
    } else if (strcmp(loc->glslType, "FogSet") == 0) {
      fprintf(fo, "  , loc_%sFogEnabled(-1)\n", loc->name);
      fprintf(fo, "  , loc_%sFogColour(-1)\n", loc->name);
      fprintf(fo, "  , loc_%sFogStart(-1)\n", loc->name);
      fprintf(fo, "  , loc_%sFogEnd(-1)\n", loc->name);
    } else {
      fprintf(fo, "  , loc_%s(-1)\n", loc->name);
    }
  }
  fprintf(fo, "{}\n");
  fprintf(fo, "\n");

  fprintf(fo, "void VOpenGLDrawer::VShaderDef_%s::Setup (VOpenGLDrawer *aowner) {\n", shadname);
  fprintf(fo, "  MainSetup(aowner, \"%s\", \"glshaders/%s.vs\", \"glshaders/%s.fs\");\n", shadname, vssrc, fssrc);
  fprintf(fo, "}\n");
  fprintf(fo, "\n");

  // generate uniform loader
  fprintf(fo, "void VOpenGLDrawer::VShaderDef_%s::LoadUniforms () {\n", shadname);
  for (const LocInfo *loc = loclist; loc; loc = loc->next) {
    if (strcmp(loc->glslType, "TextureSTSet") == 0) {
      fprintf(fo, "  loc_%sSAxis = owner->glGet%sLoc(progname, prog, \"%sSAxis\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
      fprintf(fo, "  loc_%sTAxis = owner->glGet%sLoc(progname, prog, \"%sTAxis\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
      fprintf(fo, "  loc_%sSOffs = owner->glGet%sLoc(progname, prog, \"%sSOffs\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
      fprintf(fo, "  loc_%sTOffs = owner->glGet%sLoc(progname, prog, \"%sTOffs\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
      fprintf(fo, "  loc_%sTexIW = owner->glGet%sLoc(progname, prog, \"%sTexIW\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
      fprintf(fo, "  loc_%sTexIH = owner->glGet%sLoc(progname, prog, \"%sTexIH\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
    } else if (strcmp(loc->glslType, "TextureSTSetNoSize") == 0) {
      fprintf(fo, "  loc_%sSAxis = owner->glGet%sLoc(progname, prog, \"%sSAxis\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
      fprintf(fo, "  loc_%sTAxis = owner->glGet%sLoc(progname, prog, \"%sTAxis\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
      fprintf(fo, "  loc_%sSOffs = owner->glGet%sLoc(progname, prog, \"%sSOffs\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
      fprintf(fo, "  loc_%sTOffs = owner->glGet%sLoc(progname, prog, \"%sTOffs\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
    } else if (strcmp(loc->glslType, "LightmapSTSet") == 0) {
      fprintf(fo, "  loc_%sTexMinS = owner->glGet%sLoc(progname, prog, \"%sTexMinS\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
      fprintf(fo, "  loc_%sTexMinT = owner->glGet%sLoc(progname, prog, \"%sTexMinT\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
      fprintf(fo, "  loc_%sCacheS = owner->glGet%sLoc(progname, prog, \"%sCacheS\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
      fprintf(fo, "  loc_%sCacheT = owner->glGet%sLoc(progname, prog, \"%sCacheT\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
    } else if (strcmp(loc->glslType, "PureLightmapSTSet") == 0) {
      fprintf(fo, "  loc_%sSAxis = owner->glGet%sLoc(progname, prog, \"%sSAxis\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
      fprintf(fo, "  loc_%sTAxis = owner->glGet%sLoc(progname, prog, \"%sTAxis\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
      fprintf(fo, "  loc_%sSOffs = owner->glGet%sLoc(progname, prog, \"%sSOffs\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
      fprintf(fo, "  loc_%sTOffs = owner->glGet%sLoc(progname, prog, \"%sTOffs\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
      fprintf(fo, "  loc_%sTexMinS = owner->glGet%sLoc(progname, prog, \"%sTexMinS\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
      fprintf(fo, "  loc_%sTexMinT = owner->glGet%sLoc(progname, prog, \"%sTexMinT\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
      fprintf(fo, "  loc_%sCacheS = owner->glGet%sLoc(progname, prog, \"%sCacheS\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
      fprintf(fo, "  loc_%sCacheT = owner->glGet%sLoc(progname, prog, \"%sCacheT\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
    } else if (strcmp(loc->glslType, "FogSet") == 0) {
      fprintf(fo, "  loc_%sFogEnabled = owner->glGet%sLoc(progname, prog, \"%sFogEnabled\", true);\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
      fprintf(fo, "  loc_%sFogColour = owner->glGet%sLoc(progname, prog, \"%sFogColour\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
      fprintf(fo, "  loc_%sFogStart = owner->glGet%sLoc(progname, prog, \"%sFogStart\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
      fprintf(fo, "  loc_%sFogEnd = owner->glGet%sLoc(progname, prog, \"%sFogEnd\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
    } else {
      fprintf(fo, "  loc_%s = owner->glGet%sLoc(progname, prog, \"%s\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
    }
  }
  fprintf(fo, "}\n");
}


// ////////////////////////////////////////////////////////////////////////// //
#if 0
static const char *srccode =
  "Shader ShadowsModelAmbient both=\"shadows_model_ambient\"\n"
  "/* or: vertex=\"name\" fragment=\"name\" */\n"
  "{\n"
  "  /* directly copied from shader code */\n"
  "  uniform mat4 ModelToWorldMat;\n"
  "  uniform mat3 NormalToWorldMat;\n"
  "  uniform vec3 ViewOrigin;\n"
  "  uniform float Inter;\n"
  "\n"
  "  attribute vec4 Vert2;\n"
  "  attribute vec3 VertNormal;\n"
  "  attribute vec3 Vert2Normal;\n"
  "  attribute vec2 TexCoord;\n"
  "\n"
  "  uniform vec4 Light;\n"
  "  uniform sampler2D Texture;\n"
  "  uniform float InAlpha;\n"
  "  uniform bool AllowTransparency;\n"
  "}\n";
#endif


// ////////////////////////////////////////////////////////////////////////// //
char *loadWholeFile (const char *fname) {
  FILE *fi = fopen(fname, "r");
  if (!fi) { fprintf(stderr, "FATAL: cannot open file '%s'\n", fname); abort(); }
  fseek(fi, 0, SEEK_END);
  if (ftell(fi) > 0x3ffff) { fprintf(stderr, "FATAL: file '%s' too big\n", fname); abort(); }
  size_t size = (size_t)ftell(fi);
  char *text = xalloc(size+1);
  fseek(fi, 0, SEEK_SET);
  if (fread(text, size, 1, fi) != 1) { fprintf(stderr, "FATAL: cannot read file '%s'\n", fname); abort(); }
  fclose(fi);
  return text;
}


// ////////////////////////////////////////////////////////////////////////// //
char *createFileName (const char *outdir, const char *infname, const char *ext) {
  if (!outdir) outdir = "";
  if (!infname) abort();
  if (!ext) ext = "";
  char *res = xalloc(strlen(outdir)+strlen(infname)+strlen(ext)+128);
  strcpy(res, outdir);
  if (outdir[strlen(outdir)-1] != '/') strcat(res, "/");
  if (strchr(infname, '/')) infname = strrchr(infname, '/')+1;
  strcat(res, infname);
  char *e = res+strlen(res);
  while (e > res) {
    if (e[-1] == '.') { e[-1] = 0; break; }
    if (e[-1] == '/') break;
    --e;
  }
  strcat(res, ext);
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
int parseOneShaderDef (Parser *par) {
  {
    Parser *save = prSavePos(par);
    if (!prGetToken(par)) {
      prFreePos(&save);
      return 0;
    }
    prRestorePos(par, save);
    prFreePos(&save);
  }

  prExpect(par, "Shader");
  shadname = xstrdup(prExpectId(par));

  if (prCheck(par, "both")) {
    prExpect(par, "=");
    vssrc = xstrdup(prExpectString(par));
    fssrc = xstrdup(vssrc);
  } else {
    if (prCheck(par, "vertex")) {
      prExpect(par, "=");
      vssrc = xstrdup(prExpectString(par));
      prExpect(par, "fragment");
      prExpect(par, "=");
      fssrc = xstrdup(prExpectString(par));
    } else {
      prExpect(par, "fragment");
      prExpect(par, "=");
      fssrc = xstrdup(prExpectString(par));
      prExpect(par, "vertex");
      prExpect(par, "=");
      vssrc = xstrdup(prExpectString(par));
    }
  }

  prExpect(par, "{");

  for (;;) {
    if (prIsEOF(par)) prError(par, "unexpected EOF");
    if (prCheck(par, "}")) break;
    int isAttr = 0;
    if (prCheck(par, "uniform")) {
      isAttr = 0;
    } else if (prCheck(par, "attribute")) {
      isAttr = 1;
    } else {
      prGetToken(par);
      prError(par, "`uniform` or `attribute` expected");
    }

    char *glslType = xstrdup(prExpectId(par));
    const char *shitppType = getShitppType(par, glslType);
    char *idname = xstrdup(prExpectId(par));
    prExpect(par, ";");

    LocInfo *loc = newLoc(idname);
    loc->isAttr = isAttr;
    loc->glslType = glslType;
    loc->shitppType = shitppType;
  }

  return 1;
}


// ////////////////////////////////////////////////////////////////////////// //
int main (int argc, char **argv) {
  char *infname = nullptr;
  char *outdir = ".";

  for (int aidx = 1; aidx < argc; ++aidx) {
    char *arg = argv[aidx];
    if (strcmp(arg, "-o") == 0) {
      if (aidx+1 >= argc) { fprintf(stderr, "FATAL: '-o' expects argument!\n"); abort(); }
      outdir = xstrdup(argv[++aidx]);
      //if (outdir[strlen(outdir)-1] != '/') outdir = strcat(outdir, "/");
      continue;
    }
    if (infname) { fprintf(stderr, "FATAL: duplicate file name!\n"); abort(); }
    infname = xstrdup(arg);
  }

  if (!infname) { fprintf(stderr, "FATAL: file name expected!\n"); abort(); }

  printf("[parsing '%s'...]\n", infname);
  char *text = loadWholeFile(infname);

  Parser par;
  prSetup(&par, text);

  char *outhname = createFileName(outdir, infname, ".hi");
  char *outcname = createFileName(outdir, infname, ".ci");
  printf("[  generating '%s'...]\n", outhname);
  printf("[  generating '%s'...]\n", outcname);

  FILE *foh = fopen(outhname, "w");
  if (!foh) { fprintf(stderr, "FATAL: cannot create output file '%s'", outhname); abort(); }

  FILE *foc = fopen(outcname, "w");
  if (!foc) { fprintf(stderr, "FATAL: cannot create output file '%s'", outcname); abort(); }

  while (parseOneShaderDef(&par)) {
    genHeader(foh);
    genCode(foc);
    clearAll();
  }

  fclose(foh);
  fclose(foc);

  return 0;
}


// ////////////////////////////////////////////////////////////////////////// //
#if 0
Shader ShadowsModelAmbient both="shadows_model_ambient"
/* or: vertex="name" fragment="name" */
{
  /* directly copied from shader code */
  uniform mat4 ModelToWorldMat;
  uniform mat3 NormalToWorldMat;
  uniform vec3 ViewOrigin;
  uniform float Inter;

  attribute vec4 Vert2;
  attribute vec3 VertNormal;
  attribute vec3 Vert2Normal;
  attribute vec2 TexCoord;

  uniform vec4 Light;
  uniform sampler2D Texture;
  uniform float InAlpha;
  uniform bool AllowTransparency;
}


/*
generated object:

class VShaderDef_ShadowsModelAmbient : VGLShader {
  GLint loc_ModelToWorldMat;
  // and so on

  VShaderDef_ShadowsModelAmbient (VOpenGLDrawer *aowner);
    : VGLShader(aowner, "ShadowsModelAmbient", "glshaders/shadows_model_ambient.vs", "glshaders/shadows_model_ambient.fs")
    , loc_ModelToWorldMat(-1)
  {}

  virtual void LoadUniforms () override;

  inline void setModelToWorldMat (const VMatrix4 &v) { if (loc_ModelToWorldMat >= 0) p_glUniformMatrix4fvARB(prog, 1, GL_FALSE, v[0]); }
};


// ////////////////////////////////////////////////////////////////////////// //

void VOpenGLDrawer::VShaderDef_ShadowsModelAmbient::LoadUniforms () {
  loc_ModelToWorldMat = owner->glGetUniLoc(progname, prog, "ModelToWorldMat");
}
*/
#endif
