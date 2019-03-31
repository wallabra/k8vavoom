#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistr.h>


// ////////////////////////////////////////////////////////////////////////// //
#define nullptr  NULL


// ////////////////////////////////////////////////////////////////////////// //
void *xalloc (size_t size) {
  void *res = malloc(size ? size : size+1);
  if (!res) { fprintf(stderr, "FATAL: out of memory!\n"); abort(); }
  memset(res, 0, size ? size : size+1);
  return res;
}

void xfree (void *p) {
  if (p) free(p);
}


char *xstrdup (const char *s) {
  if (!s) return xalloc(1);
  char *res = xalloc(strlen(s)+1);
  strcpy(res, s);
  return res;
}


char *xstrdupNull (const char *s) {
  if (!s) return nullptr;
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


int strIsBlankEmptyToEOL (const char *s) {
  if (!s) return 1;
  while (*s) {
    if (*s == '\n') return 1;
    if (*((const unsigned char *)s) > ' ') return 0;
    ++s;
  }
  return 1;
}


char *strReplaceExt (const char *s, const char *ext) {
  char *res = malloc(strlen(s)+strlen(ext)+64);
  strcpy(res, s);
  char *e = res+strlen(res);
  while (e > res) {
    if (e[-1] == '/') break;
    if (e[-1] == '.') { e[-1] = 0; break; }
    --e;
  }
  strcat(res, ext);
  return res;
}


int strEqu (const char *s0, const char *s1) {
  if (!s0) s0 = "";
  if (!s1) s1 = "";
  return (strcmp(s0, s1) == 0);
}


char *loadWholeFile (const char *fname) {
  FILE *fi = fopen(fname, "r");
  if (!fi) { fprintf(stderr, "FATAL: cannot open file '%s'\n", fname); abort(); }
  fseek(fi, 0, SEEK_END);
  if (ftell(fi) > 0x3ffff) { fprintf(stderr, "FATAL: file '%s' too big\n", fname); abort(); }
  size_t size = (size_t)ftell(fi);
  char *text = xalloc(size+16); // we need some space
  fseek(fi, 0, SEEK_SET);
  if (fread(text, size, 1, fi) != 1) { fprintf(stderr, "FATAL: cannot read file '%s'\n", fname); abort(); }
  fclose(fi);
  return text;
}


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
#define VABUF_COUNT  (8)
char *vavarg (const char *text, va_list ap) {
  static char *bufset[VABUF_COUNT] = {nullptr};
  static size_t bufsizes[VABUF_COUNT] = {0};
  static unsigned bufidx = 0;
  va_list apcopy;
  va_copy(apcopy, ap);
  int size = vsnprintf(bufset[bufidx], bufsizes[bufidx], text, apcopy);
  va_end(apcopy);
  if (size < 0) abort();
  if (size >= bufsizes[bufidx]) {
    bufsizes[bufidx] = size+16;
    bufset[bufidx] = realloc(bufset[bufidx], bufsizes[bufidx]);
    va_copy(apcopy, ap);
    vsnprintf(bufset[bufidx], bufsizes[bufidx], text, apcopy);
    va_end(apcopy);
  }
  char *res = bufset[bufidx++];
  bufidx %= VABUF_COUNT;
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
__attribute__((format(printf, 1, 2))) char *va (const char *text, ...) {
  va_list ap;
  va_start(ap, text);
  char *res = vavarg(text, ap);
  va_end(ap);
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
struct Parser {
  char *text;
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
  res->text = xstrdupNull(par->text);
  res->textend = (par->textend ? res->text+(ptrdiff_t)(par->textend-par->text) : nullptr);
  res->nextpos = (par->nextpos ? res->text+(ptrdiff_t)(par->nextpos-par->text) : nullptr);
  res->tkpos = (par->tkpos ? res->text+(ptrdiff_t)(par->tkpos-par->text) : nullptr);
  res->token = xstrdupNull(par->token);
  return res;
}


void prFreePos (Parser **pos) {
  if (!pos || !pos[0]) return;
  xfree(pos[0]->text);
  xfree(pos[0]->token);
  free(pos[0]);
  pos[0] = nullptr;
}


void prRestoreAndFreePos (Parser *par, Parser **pos) {
  if (par == *pos) { *pos = nullptr; return; }
  xfree(par->token);
  xfree(par->text);
  memcpy(par, *pos, sizeof(Parser));
  memset(*pos, 0, sizeof(Parser));
  xfree(*pos);
  *pos = nullptr;
}


// ////////////////////////////////////////////////////////////////////////// //
Parser *prSimpleSavePos (const Parser *par) {
  Parser *res = xalloc(sizeof(Parser));
  memcpy(res, par, sizeof(Parser));
  res->token = xstrdupNull(par->token);
  return res;
}


void prSimpleFreePos (Parser **pos) {
  if (!pos || !pos[0]) return;
  xfree(pos[0]->token);
  memset(*pos, 0, sizeof(Parser));
  free(pos[0]);
  pos[0] = nullptr;
}


void prSimpleRestoreAndFreePos (Parser *par, Parser **pos) {
  if (par == *pos) { *pos = nullptr; return; }
  xfree(par->token);
  memcpy(par, *pos, sizeof(Parser));
  memset(*pos, 0, sizeof(Parser));
  xfree(*pos);
  *pos = nullptr;
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


int prCheckEOF (Parser *par) {
  const char *np = par->nextpos;
  prSkipBlanks(par);
  int res = prIsEOF(par);
  par->nextpos = np;
  return res;
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
         if (ch == '[' && prPeekChar(par) == '[') tokbuf[tkbpos++] = prGetChar(par);
    else if (ch == ']' && prPeekChar(par) == ']') tokbuf[tkbpos++] = prGetChar(par);
  }

  tokbuf[tkbpos++] = 0;
  par->token = xalloc(tkbpos);
  memcpy(par->token, tokbuf, tkbpos);
  return par->token;
}


// ////////////////////////////////////////////////////////////////////////// //
// `{` already eaten
void prSkipCompound (Parser *par) {
  int level = 1;
  for (;;) {
    const char *tk = prGetToken(par);
    if (!tk) prError(par, "unexpected EOF");
    if (strEqu(tk, "{")) {
      ++level;
    } else if (strEqu(tk, "}")) {
      if (--level == 0) break;
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
void prExpect (Parser *par, const char *checktok) {
  const char *tk = prGetToken(par);
  if (!tk || !strEqu(tk, checktok) || par->isString) {
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
  Parser *state = prSimpleSavePos(par);
  const char *tk = prGetToken(par);
  if (tk && !par->isString && !strEqu(tk, checktok) == 0) {
    prSimpleFreePos(&state);
    return 1;
  }
  prSimpleRestoreAndFreePos(par, &state);
  return 0;
}


// ////////////////////////////////////////////////////////////////////////// //
void prDestroy (Parser **par) {
  if (!par) return;
  if (!par[0]) return;
  xfree(par[0]->text);
  xfree(par[0]->token);
  xfree(par[0]);
  memset(*par, 0, sizeof(Parser));
  par[0] = nullptr;
}


Parser *prCreateFromFile (const char *fname) {
  Parser *par = xalloc(sizeof(Parser));
  par->text = loadWholeFile(fname);
  par->nextpos = par->text;
  par->tkpos = par->text;
  par->textend = par->text+strlen(par->text);
  return par;
}


// this is used to paste include file
// includes at the current position, so next token will be from include
// also, inserts "\n"
void prIncludeHere (Parser *par, const char *fname) {
  char *inctext = loadWholeFile(fname);
  size_t tlen = strlen(inctext);
  if (!tlen) return;
  // we have some room there, so it is safe
  memmove(inctext+1, inctext, tlen+1);
  inctext[0] = '\n';
  strcat(inctext, "\n");
  tlen = strlen(inctext);
  size_t epos = (ptrdiff_t)(par->textend-par->text);
  size_t npos = (ptrdiff_t)(par->nextpos-par->text);
  size_t tpos = (ptrdiff_t)(par->tkpos-par->text);
  char *newtext = xalloc(epos+tlen+16);
  if (npos) memcpy(newtext, par->text, npos);
  memcpy(newtext+npos, inctext, tlen);
  if (npos < epos) memcpy(newtext+npos+tlen, par->text+npos, epos-npos);
  par->text = newtext;
  par->textend = par->text+epos+tlen;
  par->nextpos = par->text+npos;
  par->tkpos = par->text+tpos;
  xfree(inctext);
}


// ////////////////////////////////////////////////////////////////////////// //
typedef struct LocInfo LocInfo;

struct LocInfo {
  char *name;
  int isAttr; // is this an attribute?
  char *glslType;
  LocInfo *next;
  // for sets
  int forbidden;
  int inset;
};


LocInfo *newLoc (LocInfo **loclist, const char *aname) {
  LocInfo *loc = xalloc(sizeof(LocInfo));
  loc->name = xstrdup(aname);
  //if (strEqu(aname, "_")) loc->name[0] = 0;
  // append to list
  LocInfo *last = *loclist;
  if (last) {
    while (last->next) last = last->next;
    last->next = loc;
  } else {
    *loclist = loc;
  }
  return loc;
}


LocInfo *findLoc (LocInfo *loclist, const char *aname) {
  if (!aname || !aname[0]) return nullptr;
  for (; loclist; loclist = loclist->next) {
    if (strEqu(loclist->name, aname)) return loclist;
  }
  return nullptr;
}


void clearLocs (LocInfo **loclist) {
  while (*loclist) {
    LocInfo *curr = *loclist;
    *loclist = curr->next;
    xfree(curr->name);
    xfree(curr->glslType);
    xfree(curr);
  }
}


// ////////////////////////////////////////////////////////////////////////// //
typedef struct SetInfo SetInfo;

struct SetInfo {
  char *name;
  LocInfo *locs;
  char *code;
  SetInfo *next;
  LocInfo *badsets; // forbidden sets
  // temporary list
  SetInfo *nextTemp;
};


SetInfo *setlist = nullptr;


// `Set` already parsed
void parseSet (Parser *par) {
  SetInfo *set = xalloc(sizeof(SetInfo));
  set->name = xstrdup(prExpectId(par));
  printf("[  parsing set '%s']\n", set->name);
  prExpect(par, "{");
  while (!prCheck(par, "}")) {
    if (prCheck(par, ";")) continue;
    // code section?
    if (prCheck(par, "code")) {
      if (set->code) prError(par, "duplicate code section");
      prExpect(par, "[[");
      const char *start = par->nextpos;
      const char *end = nullptr;
      for (;;) {
        if (prIsEOF(par)) prError(par, "unexpected EOF");
        char ch = prGetChar(par);
        if (ch == ']' && prPeekChar(par) == ']') {
          end = par->nextpos-1;
          prGetChar(par);
          break;
        }
      }
      while (start < end) {
        if (!strIsBlankEmptyToEOL(start)) break;
        while (*start != '\n') ++start;
        ++start;
      }
      while (end[-1] != '\n' && ((const unsigned char *)end)[-1] <= ' ') --end;
      if (start >= end) {
        set->code = xstrdup("");
      } else {
        set->code = xalloc((ptrdiff_t)(end-start)+1);
        memcpy(set->code, start, (ptrdiff_t)(end-start));
      }
      //fprintf(stderr, "=== code ===\n%s======\n", set->code);
      continue;
    }
    // variable declaration
    int forbidden = 0;
    if (prCheck(par, "forbidden")) {
      forbidden = 1;
      if (prCheck(par, "set")) {
        char *name = prExpectId(par);
        (void)newLoc(&set->badsets, name);
        prExpect(par, ";");
        continue;
      }
    }
    int isAttr = 0;
         if (prCheck(par, "uniform")) isAttr = 0;
    else if (prCheck(par, "attribute")) isAttr = 1;
    else prError(par, "`uniform` or `attribute` expected");
    char *type = xstrdup(prExpectId(par));
    char *name = xstrdup(prExpectId(par));
    prExpect(par, ";");
    LocInfo *loc = newLoc(&set->locs, name);
    loc->isAttr = isAttr;
    loc->glslType = type;
    loc->forbidden = forbidden;
    xfree(name); // it is copied in loc ctor
  }
  if (!set->locs) prError(par, "no variables");
  if (!set->code) prError(par, "no code");
  // append to list
  if (setlist) {
    SetInfo *last = setlist;
    while (last->next) last = last->next;
    last->next = set;
  } else {
    setlist = set;
  }
}


int checkSet (SetInfo *set, LocInfo *locs) {
  if (!set || !locs || !set->locs) return 0;
  for (LocInfo *sl = set->locs; sl; sl = sl->next) {
    LocInfo *l = findLoc(locs, sl->name);
    if (!l) {
      if (!sl->forbidden) return 0;
      continue;
    }
    if (sl->forbidden) return 0;
    if (!strEqu(l->glslType, sl->glslType)) return 0;
  }
  return 1;
}


// ////////////////////////////////////////////////////////////////////////// //
typedef struct ShaderInfo ShaderInfo;

struct ShaderInfo {
  char *name; // variable name
  LocInfo *locs;
  // vertex and fragment shader sources
  char *vssrc;
  char *fssrc;
  // basedir for includes (can be nullptr)
  char *basedir;
  // defines (nullptr, or array of chars, each define terminates with '\0')
  char *defines;
  size_t defsize;
  ShaderInfo *next;
};


ShaderInfo *shaderlist = nullptr;


void appendDefine (ShaderInfo *si, const char *defstr) {
  if (!defstr) return;
  while (*defstr && *(const unsigned char *)defstr <= ' ') ++defstr;
  if (!defstr[0]) return;
  size_t slen = strlen(defstr);
  if (!si->defines) {
    si->defines = xalloc(slen+2);
    strcpy(si->defines, defstr);
    si->defsize = slen+2;
  } else {
    si->defines = realloc(si->defines, si->defsize+slen+1);
    strcpy(si->defines+si->defsize-1, defstr);
    si->defsize += slen+1;
    si->defines[si->defsize-1] = 0;
  }
}


int hasDefine (ShaderInfo *si, const char *defstr) {
  if (!defstr) return 0;
  const char *dss = si->defines;
  while (*defstr && *(const unsigned char *)defstr <= ' ') ++defstr;
  if (!defstr[0]) return 0;
  if (!dss) return 0;
  while (*dss) {
    if (strEqu(dss, defstr)) return 1;
    dss += strlen(dss)+1;
  }
  return 0;
}


// ////////////////////////////////////////////////////////////////////////// //
// `ShaderList` eaten
void parseShaderList (Parser *par) {
  char *basedir = nullptr;
  if (prCheck(par, "basedir")) {
    prExpect(par, "=");
    basedir = xstrdup(prExpectString(par));
  }
  prExpect(par, "{");
  while (!prCheck(par, "}")) {
    if (prCheck(par, ";")) continue;
    prExpect(par, "Shader");
    // attributes (unused for now)
    if (prCheck(par, "[")) {
      prExpect(par, "advanced");
      prExpect(par, "]");
    }
    ShaderInfo *si = xalloc(sizeof(ShaderInfo));
    // name
    si->name = xstrdup(prExpectId(par));
    printf("[  found shader info '%s']\n", si->name);
    // shader source
    if (prCheck(par, "both")) {
      prExpect(par, "=");
      si->vssrc = xstrdup(prExpectString(par));
      si->fssrc = xstrdup(si->vssrc);
    } else if (prCheck(par, "vertex")) {
      prExpect(par, "=");
      si->vssrc = xstrdup(prExpectString(par));
      prExpect(par, "fragment");
      prExpect(par, "=");
      si->fssrc = xstrdup(prExpectString(par));
    } else if (prCheck(par, "fragment")) {
      prExpect(par, "=");
      si->fssrc = xstrdup(prExpectString(par));
      prExpect(par, "vertex");
      prExpect(par, "=");
      si->vssrc = xstrdup(prExpectString(par));
    } else {
      prError(par, "expected vertex source option");
    }
    si->basedir = xstrdupNull(basedir);
    // parameters
    if (prCheck(par, "{")) {
      // only "defines" for now
      while (!prCheck(par, "}")) {
        if (prCheck(par, "define")) {
          appendDefine(si, prExpectString(par));
          prExpect(par, ";");
        } else {
          prExpect(par, "defines");
          prCheck(par, "{");
          if (!prCheck(par, "}")) {
            for (;;) {
              appendDefine(si, prExpectString(par));
              if (prCheck(par, "}")) break;
              prExpect(par, ",");
            }
          }
        }
      }
    } else {
      prExpect(par, ";");
    }
    // append to list
    if (shaderlist) {
      ShaderInfo *last = shaderlist;
      while (last->next) last = last->next;
      last->next = si;
    } else {
      shaderlist = si;
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// called after parsed `#if...`
void skipConditional (Parser *par) {
  int level = 1;
  for (;;) {
    char *tk = prGetToken(par);
    if (!tk) prError(par, "unexpected EOF");
    if (!strEqu(tk, "#")) continue;
    if (prCheck(par, "if")) { ++level; continue; }
    if (prCheck(par, "ifdef")) { ++level; continue; }
    if (prCheck(par, "ifndef")) { ++level; continue; }
    if (prCheck(par, "else")) continue;
    if (prCheck(par, "endif")) {
      if (--level == 0) break;
    }
  }
}


// called after parsed `#if...`
// return `0` if no `#else` found
int skipConditionalToElse (Parser *par) {
  int level = 1;
  for (;;) {
    char *tk = prGetToken(par);
    if (!tk) prError(par, "unexpected EOF");
    if (!strEqu(tk, "#")) continue;
    if (prCheck(par, "if")) { ++level; continue; }
    if (prCheck(par, "ifdef")) { ++level; continue; }
    if (prCheck(par, "ifndef")) { ++level; continue; }
    if (prCheck(par, "else")) {
      if (level == 1) return 1;
    }
    if (prCheck(par, "endif")) {
      if (--level == 0) break;
    }
  }
  return 0;
}


int parseGLSL (Parser *par, ShaderInfo *si) {
  prExpect(par, "#");
  prExpect(par, "version");
  prExpect(par, "120");

  size_t olddefsize = si->defsize;
  char *olddef = (si->defines ? xalloc(olddefsize) : nullptr);
  if (olddefsize) memcpy(olddef, si->defines, olddefsize);

  int brclevel = 0;

  while (!prCheckEOF(par)) {
    if (prCheck(par, "{")) {
      ++brclevel;
      continue;
    }

    if (prCheck(par, "}")) {
      if (--brclevel < 0) prError(par, "unbalanced compounds");
      continue;
    }

    if (prCheck(par, "$")) {
      prExpect(par, "include");
      if (!si->basedir) prError(par, "no base dir, cannot process includes");
      char *tk = prExpectString(par);
      //fprintf(stderr, "<include: %s>\n", tk);
      prIncludeHere(par, va("%s/%s", si->basedir, tk));
      //fprintf(stderr, "======\n%s\n======\n", par->text);
      continue;
    }

    if (prCheck(par, "#")) {
      // else
      if (prCheck(par, "else")) {
        // skip to endif
        skipConditional(par);
        continue;
      }
      // endif
      if (prCheck(par, "endif")) {
        // do nothing
        continue;
      }
      // define
      if (prCheck(par, "define")) {
        char *tk = prExpectId(par);
        //fprintf(stderr, "<newdef: %s>\n", tk);
        appendDefine(si, tk);
        continue;
      }
      // if
      if (prCheck(par, "if")) {
        if (prCheck(par, "0")) {
          // skip to else part
          skipConditionalToElse(par);
          continue;
        }
        if (prCheck(par, "1")) {
          // go on, the parser will skip `#else`
          continue;
        }
        prError(par, "complex preprocessor expressions aren't supported yet");
      }
      // ifdef
      if (prCheck(par, "ifdef")) {
        char *tk = prExpectId(par);
        if (!hasDefine(si, tk)) {
          // skip to else part
          skipConditionalToElse(par);
        }
        continue;
      }
      // ifndef
      if (prCheck(par, "ifndef")) {
        char *tk = prExpectId(par);
        if (hasDefine(si, tk)) {
          // skip to else part
          skipConditionalToElse(par);
        }
        continue;
      }
      // pragma
      if (prCheck(par, "pragma")) {
        for (;;) {
          char ch = prGetChar(par);
          if (!ch || ch == '\n') break;
        }
        continue;
      }
      char *tk = prGetToken(par);
      prError(par, va("unknown preprocessor directive '%s'", tk));
    }

    if (brclevel == 0) {
      int isAttr = -1;
           if (prCheck(par, "uniform")) isAttr = 0;
      else if (prCheck(par, "attribute")) isAttr = 1;
      if (isAttr >= 0) {
        char *glslType = xstrdup(prExpectId(par));
        char *idname = xstrdup(prExpectId(par));
        prExpect(par, ";");
        LocInfo *loc = newLoc(&si->locs, idname);
        loc->isAttr = isAttr;
        loc->glslType = glslType;
        xfree(idname); // it is copied in loc ctor
        //fprintf(stderr, "::: %s %s %s;\n", (loc->isAttr ? "attribute" : "uniform"), loc->glslType, loc->name);
        continue;
      }
    }

    prGetToken(par);
  }

  if (brclevel != 0) prError(par, "unbalanced compounds");

  // restore defines
  xfree(si->defines);
  si->defines = olddef;
  si->defsize = olddefsize;

  return 1;
}


// ////////////////////////////////////////////////////////////////////////// //
const char *getShitppType (const char *glslType) {
  if (strEqu(glslType, "float")) return "float";
  if (strEqu(glslType, "bool")) return "bool";
  if (strEqu(glslType, "vec3")) return "TVec";
  if (strEqu(glslType, "mat4")) return "VMatrix4";
  // non-standard types
  if (strEqu(glslType, "vec4")) return "float *";
  if (strEqu(glslType, "vec2")) return "float *";
  if (strEqu(glslType, "mat3")) return "float *";
  // samplers
  if (strEqu(glslType, "sampler2D")) return "vuint32";
  fprintf(stderr, "FATAL: unknown GLSL type `%s`!\n", glslType);
  abort();
}


const char *getShitppAmp (const char *shitppType) {
  if (shitppType[strlen(shitppType)-1] == '*') return "";
  if (strEqu(shitppType, "TVec")) return "&";
  if (strEqu(shitppType, "VMatrix4")) return "&";
  return "";
}


// ////////////////////////////////////////////////////////////////////////// //
int main (int argc, char **argv) {
  char *infname = nullptr;
  char *outdir = ".";
  bool toStdout = false;

  for (int aidx = 1; aidx < argc; ++aidx) {
    char *arg = argv[aidx];
    if (strEqu(arg, "-o")) {
      if (aidx+1 >= argc) { fprintf(stderr, "FATAL: '-o' expects argument!\n"); abort(); }
      outdir = xstrdup(argv[++aidx]);
      //if (outdir[strlen(outdir)-1] != '/') outdir = strcat(outdir, "/");
      continue;
    }
    if (strEqu(arg, "-")) { toStdout = true; continue; }
    if (infname) { fprintf(stderr, "FATAL: duplicate file name!\n"); abort(); }
    infname = xstrdup(arg);
  }

  if (!infname) { fprintf(stderr, "FATAL: file name expected!\n"); abort(); }

  printf("[parsing '%s'...]\n", infname);

  char *outhname = createFileName(outdir, infname, ".hi");
  char *outcname = createFileName(outdir, infname, ".ci");
  printf("[ generating '%s'...]\n", outhname);
  printf("[ generating '%s'...]\n", outcname);

  FILE *foh = nullptr;
  if (!toStdout) {
    foh = fopen(outhname, "w");
    if (!foh) { fprintf(stderr, "FATAL: cannot create output file '%s'", outhname); abort(); }
  } else {
    foh = stdout;
  }

  FILE *foc = nullptr;
  if (!toStdout) {
    foc = fopen(outcname, "w");
    if (!foc) { fprintf(stderr, "FATAL: cannot create output file '%s'", outcname); abort(); }
  } else {
    foc = stdout;
  }

  Parser *par = prCreateFromFile(infname);

  while (!prIsEOF(par)) {
    if (prCheck(par, "Set")) {
      parseSet(par);
      continue;
    }
    if (prCheck(par, "ShaderList")) {
      parseShaderList(par);
      continue;
    }
    char *tk = prGetToken(par);
    if (!tk) break;
    prError(par, va("wut?! '%s'", tk));
  }

  prDestroy(&par);

  // parse shader files
  for (ShaderInfo *si = shaderlist; si; si = si->next) {
    if (!si->basedir) abort();
    printf("[  parsing vert shader '%s.vs']\n", si->vssrc);
    par = prCreateFromFile(va("%s/%s.vs", si->basedir, si->vssrc));
    parseGLSL(par, si);
    prDestroy(&par);
    printf("[  parsing frag shader '%s.fs']\n", si->fssrc);
    par = prCreateFromFile(va("%s/%s.fs", si->basedir, si->fssrc));
    parseGLSL(par, si);
    prDestroy(&par);
    //for (LocInfo *loc = si->locs; loc; loc = loc->next) fprintf(stderr, "   %s %s %s;\n", (loc->isAttr ? "attribute" : "uniform"), loc->glslType, loc->name);
    SetInfo *foundSets = nullptr;
    for (SetInfo *set = setlist; set; set = set->next) {
      if (checkSet(set, si->locs)) {
        // check if we don't have forbidden sets
        int fbfound = 0;
        for (SetInfo *css = foundSets; css; css = css->nextTemp) {
          //fprintf(stderr, "...checking <%s>\n", css->name);
          //for (LocInfo *sli = css->badsets; sli; sli = sli->next) fprintf(stderr, "   <%s>\n", sli->name);
          if (findLoc(set->badsets, css->name)) {
            //fprintf(stderr, "    <set: %s is forbidden by '%s'>\n", set->name, css->name);
            fbfound = 1;
            break;
          }
        }
        if (fbfound) continue;
        //fprintf(stderr, "    <set: %s>\n", set->name);
        // check if this set is not a subset of any other found set
        set->nextTemp = foundSets;
        foundSets = set;
      }
    }
    for (SetInfo *css = foundSets; css; css = css->nextTemp) {
      //fprintf(stderr, "    <set: %s>\n", css->name);
      for (LocInfo *lc = css->locs; lc; lc = lc->next) {
        if (lc->forbidden) continue;
        LocInfo *shloc = findLoc(si->locs, lc->name);
        shloc->inset = 1; // do not generate setter for this var
      }
    }
    // generate header info
    fprintf(foh, "  class VShaderDef_%s : public VGLShader {\n", si->name);
    fprintf(foh, "  public:\n");
    for (const LocInfo *loc = si->locs; loc; loc = loc->next) {
      fprintf(foh, "    GLint loc_%s; // %s %s -> %s\n", loc->name, (loc->isAttr ? "attribute" : "uniform"), loc->glslType, getShitppType(loc->glslType));
    }
    fprintf(foh, "\n");
    fprintf(foh, "  public:\n");
    fprintf(foh, "    VShaderDef_%s ();\n", si->name);
    fprintf(foh, "\n");
    fprintf(foh, "    virtual void Setup (VOpenGLDrawer *aowner) override;\n");
    fprintf(foh, "    virtual void LoadUniforms () override;\n");
    fprintf(foh, "\n");
    // generate setters
    for (const LocInfo *loc = si->locs; loc; loc = loc->next) {
      if (loc->inset) continue; // will be generated later
      const char *shitppType = getShitppType(loc->glslType);
      if (!loc->isAttr) {
        if (strEqu(loc->glslType, "float")) {
          fprintf(foh, "    inline void Set%s (const %s %sv) { ", loc->name, shitppType, getShitppAmp(shitppType));
          fprintf(foh, "if (loc_%s >= 0) ", loc->name);
          fprintf(foh, "owner->p_glUniform1fARB(loc_%s, v);", loc->name);
          fprintf(foh, " }\n");
        } else if (strEqu(loc->glslType, "bool")) {
          fprintf(foh, "    inline void Set%s (const %s %sv) { ", loc->name, shitppType, getShitppAmp(shitppType));
          fprintf(foh, "if (loc_%s >= 0) ", loc->name);
          fprintf(foh, "owner->p_glUniform1iARB(loc_%s, (v ? GL_TRUE : GL_FALSE));", loc->name);
          fprintf(foh, " }\n");
        } else if (strEqu(loc->glslType, "sampler2D")) {
          fprintf(foh, "    inline void Set%s (const %s %sv) { ", loc->name, shitppType, getShitppAmp(shitppType));
          fprintf(foh, "if (loc_%s >= 0) ", loc->name);
          fprintf(foh, "owner->p_glUniform1iARB(loc_%s, (GLint)v);", loc->name);
          fprintf(foh, " }\n");
        } else if (strEqu(loc->glslType, "vec3")) {
          fprintf(foh, "    inline void Set%s (const %s %sv) { ", loc->name, shitppType, getShitppAmp(shitppType));
          fprintf(foh, "if (loc_%s >= 0) ", loc->name);
          fprintf(foh, "owner->p_glUniform3fvARB(loc_%s, 1, &v.x);", loc->name);
          fprintf(foh, " }\n");
          fprintf(foh, "    inline void Set%s (const float x, const float y, const float z) { ", loc->name);
          fprintf(foh, "if (loc_%s >= 0) ", loc->name);
          fprintf(foh, "owner->p_glUniform3fARB(loc_%s, x, y, z);", loc->name);
          fprintf(foh, " }\n");
        } else if (strEqu(loc->glslType, "mat4")) {
          fprintf(foh, "    inline void Set%s (const %s %sv) { ", loc->name, shitppType, getShitppAmp(shitppType));
          fprintf(foh, "if (loc_%s >= 0) ", loc->name);
          fprintf(foh, "owner->p_glUniformMatrix4fvARB(loc_%s, 1, GL_FALSE, &v.m[0][0]);", loc->name);
          fprintf(foh, " }\n");
          fprintf(foh, "    inline void Set%s (const float * v) { ", loc->name);
          fprintf(foh, "if (loc_%s >= 0) ", loc->name);
          fprintf(foh, "owner->p_glUniformMatrix4fvARB(loc_%s, 1, GL_FALSE, v);", loc->name);
          fprintf(foh, " }\n");
        } else if (strEqu(loc->glslType, "vec4")) {
          fprintf(foh, "    inline void Set%s (const %s %sv) { ", loc->name, shitppType, getShitppAmp(shitppType));
          fprintf(foh, "if (loc_%s >= 0) ", loc->name);
          fprintf(foh, "owner->p_glUniform4fvARB(loc_%s, 1, v);", loc->name);
          fprintf(foh, " }\n");
          fprintf(foh, "    inline void Set%s (const float x, const float y, const float z, const float w) { ", loc->name);
          fprintf(foh, "if (loc_%s >= 0) ", loc->name);
          fprintf(foh, "owner->p_glUniform4fARB(loc_%s, x, y, z, w);", loc->name);
          fprintf(foh, " }\n");
        } else if (strEqu(loc->glslType, "vec2")) {
          fprintf(foh, "    inline void Set%s (const %s %sv) { ", loc->name, shitppType, getShitppAmp(shitppType));
          fprintf(foh, "if (loc_%s >= 0) ", loc->name);
          fprintf(foh, "owner->p_glUniform2fvARB(loc_%s, 1, v);", loc->name);
          fprintf(foh, " }\n");
          fprintf(foh, "    inline void Set%s (const float x, const float y) { ", loc->name);
          fprintf(foh, "if (loc_%s >= 0) ", loc->name);
          fprintf(foh, "owner->p_glUniform2fARB(loc_%s, x, y);", loc->name);
          fprintf(foh, " }\n");
        } else if (strEqu(loc->glslType, "mat3")) {
          fprintf(foh, "    inline void Set%s (const %s %sv) { ", loc->name, shitppType, getShitppAmp(shitppType));
          fprintf(foh, "if (loc_%s >= 0) ", loc->name);
          fprintf(foh, "owner->p_glUniformMatrix3fvARB(loc_%s, 1, GL_FALSE, v);", loc->name);
          fprintf(foh, " }\n");
        } else {
          fprintf(stderr, "FATAL: cannot emit setter for GLSL type '%s'\n", loc->glslType);
          abort();
        }
      } else {
        // attrs
        if (strEqu(loc->glslType, "float")) {
          fprintf(foh, "    inline void Set%s (const %s %sv) { ", loc->name, shitppType, getShitppAmp(shitppType));
          fprintf(foh, "if (loc_%s >= 0) ", loc->name);
          fprintf(foh, "owner->p_glVertexAttrib1fARB(loc_%s, v);", loc->name);
          fprintf(foh, " }\n");
        } else if (strEqu(loc->glslType, "vec2")) {
          fprintf(foh, "    inline void Set%s (const %s %sv) { ", loc->name, shitppType, getShitppAmp(shitppType));
          fprintf(foh, "if (loc_%s >= 0) ", loc->name);
          fprintf(foh, "owner->p_glVertexAttrib2fvARB(loc_%s, v);", loc->name);
          fprintf(foh, " }\n");
          fprintf(foh, "    inline void Set%s (const float x, const float y) { ", loc->name);
          fprintf(foh, "if (loc_%s >= 0) ", loc->name);
          fprintf(foh, "owner->p_glVertexAttrib2fARB(loc_%s, x, y);", loc->name);
          fprintf(foh, " }\n");
        } else if (strEqu(loc->glslType, "vec4")) {
          fprintf(foh, "    inline void Set%s (const %s %sv) { ", loc->name, shitppType, getShitppAmp(shitppType));
          fprintf(foh, "if (loc_%s >= 0) ", loc->name);
          fprintf(foh, "owner->p_glVertexAttrib4fvARB(loc_%s, v);", loc->name);
          fprintf(foh, " }\n");
          fprintf(foh, "    inline void Set%s (const float x, const float y, const float z, const float w) { ", loc->name);
          fprintf(foh, "if (loc_%s >= 0) ", loc->name);
          fprintf(foh, "owner->p_glVertexAttrib4fARB(loc_%s, x, y, z, w);", loc->name);
          fprintf(foh, " }\n");
        } else if (strEqu(loc->glslType, "vec3")) {
          fprintf(foh, "    inline void Set%s (const %s %sv) { ", loc->name, shitppType, getShitppAmp(shitppType));
          fprintf(foh, "if (loc_%s >= 0) ", loc->name);
          fprintf(foh, "owner->p_glVertexAttrib3fvARB(loc_%s, &v.x);", loc->name);
          fprintf(foh, " }\n");
          fprintf(foh, "    inline void Set%s (const float x, const float y, const float z) { ", loc->name);
          fprintf(foh, "if (loc_%s >= 0) ", loc->name);
          fprintf(foh, "owner->p_glVertexAttrib3fARB(loc_%s, x, y, z);", loc->name);
          fprintf(foh, " }\n");
        } else {
          fprintf(stderr, "FATAL: cannot emit attribute setter for GLSL type '%s'\n", loc->glslType);
          abort();
        }
      }
    }
    // generate set setters
    for (SetInfo *css = foundSets; css; css = css->nextTemp) {
      fprintf(foh, css->code);
    }
    fprintf(foh, "  };\n");

    // generate code
    // generate constructor
    fprintf(foc, "VOpenGLDrawer::VShaderDef_%s::VShaderDef_%s ()\n", si->name, si->name);
    fprintf(foc, "  : VGLShader()\n");
    for (const LocInfo *loc = si->locs; loc; loc = loc->next) {
      fprintf(foc, "  , loc_%s(-1)\n", loc->name);
    }
    fprintf(foc, "{}\n");
    fprintf(foc, "\n");

    fprintf(foc, "void VOpenGLDrawer::VShaderDef_%s::Setup (VOpenGLDrawer *aowner) {\n", si->name);
    fprintf(foc, "  MainSetup(aowner, \"%s\", \"glshaders/%s.vs\", \"glshaders/%s.fs\");\n", si->name, si->vssrc, si->fssrc);
    fprintf(foc, "}\n");
    fprintf(foc, "\n");

    // generate uniform loader
    fprintf(foc, "void VOpenGLDrawer::VShaderDef_%s::LoadUniforms () {\n", si->name);
    for (const LocInfo *loc = si->locs; loc; loc = loc->next) {
      //HACK!
      if (strEqu(loc->name, "FogEnabled")) {
        fprintf(foc, "  loc_%s = owner->glGet%sLoc(progname, prog, \"%s\", true);\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
      } else {
        fprintf(foc, "  loc_%s = owner->glGet%sLoc(progname, prog, \"%s\");\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name);
      }
    }
    fprintf(foc, "}\n");
  }

  // now write list of known shaders
  fprintf(foh, "protected:\n");
  for (ShaderInfo *si = shaderlist; si; si = si->next) {
    fprintf(foh, "  VShaderDef_%s %s;\n", si->name, si->name);
  }

  // write loader method
  fprintf(foh, "\n");
  fprintf(foh, "  void LoadAllShaders ();\n");

  fprintf(foc, "void VOpenGLDrawer::LoadAllShaders () {\n");
  for (ShaderInfo *si = shaderlist; si; si = si->next) {
    fprintf(foc, "  %s.Setup(this);", si->name);
    const char *dss = si->defines;
    if (dss) {
      while (*dss) {
        fprintf(foc, " %s.defines.append(\"%s\");", si->name, dss);
        dss += strlen(dss)+1;
      }
    }
    fprintf(foc, "\n");
  }
  fprintf(foc, "}\n");


  if (!toStdout) {
    fclose(foh);
    fclose(foc);
  }

  return 0;
}
