#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>


int verbose = 0;


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
  if (!s0) s0 = "";
  if (!s1) s1 = "";
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
  FILE *fi = fopen(fname, "rb");
  if (!fi) { fprintf(stderr, "FATAL: cannot open file '%s'\n", fname); abort(); }
  fseek(fi, 0, SEEK_END);
  if (ftell(fi) > 0x3fffff) { fprintf(stderr, "FATAL: file '%s' too big (%d)\n", fname, (int)ftell(fi)); abort(); }
  size_t size = (size_t)ftell(fi);
  char *text = xalloc(size+16); // we need some space
  fseek(fi, 0, SEEK_SET);
  if (fread(text, size, 1, fi) != 1) { fprintf(stderr, "FATAL: cannot read file '%s'\n", fname); abort(); }
  fclose(fi);
  return text;
}


char *createFileName (const char *outdir, const char *infname, const char *ext) {
  if (!outdir) outdir = "";
  if (!infname) { fprintf(stderr, "FATAL: wut?!\n"); abort(); }
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
static void copyFile (const char *fnameDest, const char *fnameSrc) {
  fprintf(stderr, "copying '%s' to '%s'...\n", fnameSrc, fnameDest);

  FILE *fsrc = fopen(fnameSrc, "rb");
  if (!fsrc) { fprintf(stderr, "FATAL: cannot open file '%s'\n", fnameSrc); abort(); }
  fseek(fsrc, 0, SEEK_END);
  int size = ftell(fsrc);
  if (size < 0 || size > 0x03ffffff) { fprintf(stderr, "FATAL: file '%s' is too big (%d)\n", fnameSrc, size); abort(); }
  char *data = malloc(size+1);
  if (!data) { fprintf(stderr, "FATAL: cannot allocate memory for file '%s'\n", fnameSrc); abort(); }
  if (size > 0) {
    fseek(fsrc, 0, SEEK_SET);
    if (fread(data, size, 1, fsrc) != 1) { fprintf(stderr, "FATAL: cannot read file '%s'\n", fnameSrc); abort(); }
  }
  fclose(fsrc);

  FILE *fdst = fopen(fnameDest, "wb");
  if (!fdst) { fprintf(stderr, "FATAL: cannot create file '%s'\n", fnameDest); abort(); }
  if (size > 0) {
    if (fwrite(data, size, 1, fdst) != 1) { fprintf(stderr, "FATAL: cannot write file '%s'\n", fnameSrc); abort(); }
  }
  fclose(fdst);
}


static int compareFiles (const char *fname0, const char *fname1) {
  FILE *f0 = fopen(fname0, "rb");
  if (!f0) return 0;
  FILE *f1 = fopen(fname1, "rb");
  if (!f1) { fclose(f0); return 0; }
  fseek(f0, 0, SEEK_END);
  fseek(f1, 0, SEEK_END);
  if (ftell(f0) != ftell(f1)) { fclose(f0); fclose(f1); return 0; }
  if (ftell(f0) > 0x3fffff) { fclose(f0); fclose(f1); return 0; }
  int size = (int)ftell(f0);
  fclose(f0);
  fclose(f1);

  if (size == 0) return 1;

  char *t0 = loadWholeFile(fname0);
  char *t1 = loadWholeFile(fname1);
  int res = (memcmp(t0, t1, size) == 0 ? 1 : 0);
  xfree(t0);
  xfree(t1);
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
#define VABUF_COUNT  (8)
static char *va_intr_bufset[VABUF_COUNT] = {nullptr};
static size_t va_intr_bufsizes[VABUF_COUNT] = {0};
static unsigned va_intr_bufidx = 0;

void vaShutdown () {
  for (unsigned f = 0; f < VABUF_COUNT; ++f) {
    xfree(va_intr_bufset[f]);
    va_intr_bufset[f] = 0;
    va_intr_bufsizes[f] = 0;
  }
  va_intr_bufidx = 0;
}


char *vavarg (const char *text, va_list ap) {
  va_list apcopy;
  va_copy(apcopy, ap);
  int size = vsnprintf(va_intr_bufset[va_intr_bufidx], va_intr_bufsizes[va_intr_bufidx], text, apcopy);
  va_end(apcopy);
  if (size < 0) { fprintf(stderr, "FUCK YOU SHITDOWS!\n"); /*abort();*/ size = 1024*1024; }
  if (size >= va_intr_bufsizes[va_intr_bufidx]) {
    va_intr_bufsizes[va_intr_bufidx] = size+16;
    va_intr_bufset[va_intr_bufidx] = realloc(va_intr_bufset[va_intr_bufidx], va_intr_bufsizes[va_intr_bufidx]);
    va_copy(apcopy, ap);
    vsnprintf(va_intr_bufset[va_intr_bufidx], va_intr_bufsizes[va_intr_bufidx], text, apcopy);
    va_end(apcopy);
  }
  char *res = va_intr_bufset[va_intr_bufidx++];
  va_intr_bufidx %= VABUF_COUNT;
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
void prPrintTokenLine (const Parser *par) {
  const char *s = par->tkpos;
  if (!s) return;
  const char *start = s;
  while (start > par->text && start[-1] != '\n') --start;
  const char *end = s;
  while (end < par->textend && *end != '\n') ++end;
  fprintf(stderr, "=================\n");
  while (start < end) { fputc(*start, stderr); ++start; }
  fprintf(stderr, "\n=================\n");
}

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
  prPrintTokenLine(par);
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
    else if (ch == '<' && prPeekChar(par) == '=') tokbuf[tkbpos++] = prGetChar(par);
    else if (ch == '>' && prPeekChar(par) == '=') tokbuf[tkbpos++] = prGetChar(par);
    else if (ch == '!' && prPeekChar(par) == '=') tokbuf[tkbpos++] = prGetChar(par);
    else if (ch == '&' && prPeekChar(par) == '&') tokbuf[tkbpos++] = prGetChar(par);
    else if (ch == '|' && prPeekChar(par) == '|') tokbuf[tkbpos++] = prGetChar(par);
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
  if (tk && !par->isString && strEqu(tk, checktok)) {
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
  memset(*par, 0, sizeof(Parser));
  xfree(par[0]);
  par[0] = nullptr;
}


Parser *prCreateFromFile (const char *fname) {
  if (verbose > 1) printf("[   creating parser for '%s'...]\n", fname);
  Parser *par = xalloc(sizeof(Parser));
  par->text = loadWholeFile(fname);
  par->nextpos = par->text;
  par->tkpos = par->text;
  par->textend = par->text+strlen(par->text);
  if (verbose > 1) printf("[   created parser for '%s'...]\n", fname);
  return par;
}


// this is used to paste include file
// includes at the current position, so next token will be from include
// also, inserts "\n"
void prIncludeHere (Parser *par, const char *fname) {
  if (verbose > 1) printf("[    including '%s'...]\n", fname);
  char *inctext = loadWholeFile(fname);
  size_t tlen = strlen(inctext);
  if (!tlen) { xfree(inctext); return; }
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
  xfree(par->text);
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


void clearSetList (SetInfo **setlist) {
  if (!setlist) return;
  while (*setlist) {
    SetInfo *si = *setlist;
    *setlist = si->next;
    xfree(si->name);
    xfree(si->code);
    clearLocs(&si->locs);
    clearLocs(&si->badsets);
    xfree(si);
  }
}


// `Set` already parsed
void parseSet (Parser *par) {
  SetInfo *set = xalloc(sizeof(SetInfo));
  set->name = xstrdup(prExpectId(par));
  if (verbose) printf("[  parsing set '%s']\n", set->name);
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
// defines (nullptr, or array of chars, each define terminates with '\0')
typedef struct DefineTable_t {
  char *defines;
  size_t defsize;
} DefineTable;

static DefineTable globaldefines = {NULL, 0};


static void clearTblDefine (DefineTable *tbl) {
  if (!tbl) return;
  xfree(tbl->defines);
  tbl->defines = NULL;
  tbl->defsize = 0;
}


static void initTblDefineFrom (DefineTable *dest, const DefineTable *src) {
  if (!dest || dest == src) return;
  if (!src || !src->defines || !src->defsize) {
    dest->defines = NULL;
    dest->defsize = 0;
  } else {
    dest->defines = xalloc(src->defsize);
    memcpy(dest->defines, src->defines, src->defsize);
    dest->defsize = src->defsize;
  }
}


static void addTblDefine (DefineTable *tbl, const char *defstr) {
  if (!defstr) return;
  while (*defstr && *(const unsigned char *)defstr <= ' ') ++defstr;
  if (!defstr[0]) return;
  size_t slen = strlen(defstr);
  while (slen > 0 && *(const unsigned char *)(defstr+slen-1) <= ' ') --slen;
  if (slen == 0) return;
  if (!tbl->defines) {
    tbl->defines = xalloc(slen+2);
    memcpy(tbl->defines, defstr, slen);
    tbl->defines[slen] = 0;
    tbl->defines[slen+1] = 0;
    tbl->defsize = slen+2;
  } else {
    tbl->defines = realloc(tbl->defines, tbl->defsize+slen+1);
    memcpy(tbl->defines+tbl->defsize-1, defstr, slen);
    tbl->defines[tbl->defsize-1+slen] = 0;
    tbl->defines[tbl->defsize-1+slen+1] = 0;
    tbl->defsize += slen+1;
  }
}


static int hasTblDefine (DefineTable *tbl, const char *defstr) {
  if (!tbl || !defstr) return 0;
  if (tbl->defsize == 0 || !tbl->defines) return 0;
  while (*defstr && *(const unsigned char *)defstr <= ' ') ++defstr;
  if (!defstr[0]) return 0;
  size_t slen = strlen(defstr);
  while (slen > 0 && *(const unsigned char *)(defstr+slen-1) <= ' ') --slen;
  if (slen == 0) return 0;
  const char *dss = tbl->defines;
  while (*dss) {
    size_t xlen = strlen(dss);
    if (xlen == slen) {
      if (memcmp(dss, defstr, slen) == 0) return 1;
    }
    dss += xlen+1;
  }
  return 0;
}


static void removeTblDefine (DefineTable *tbl, const char *defstr) {
  if (!tbl || !defstr) return;
  if (tbl->defsize == 0 || !tbl->defines) return;
  while (*defstr && *(const unsigned char *)defstr <= ' ') ++defstr;
  if (!defstr[0]) return;
  size_t slen = strlen(defstr);
  while (slen > 0 && *(const unsigned char *)(defstr+slen-1) <= ' ') --slen;
  if (slen == 0) return;
  char *dss = tbl->defines;
  while (*dss) {
    size_t xlen = strlen(dss);
    if (xlen == slen) {
      if (memcmp(dss, defstr, slen) == 0) {
        // we found it, replace it with all underscores
        memset(dss, '_', slen);
      }
    }
    dss += xlen+1;
  }
}


typedef struct ShaderInfo ShaderInfo;

typedef enum {
  CondLess,
  CondLessEqu,
  CondEqu,
  CondGreater,
  CondGreaterEqu,
  CondNotEqu,
} CondCode;


static const char *getCondName (CondCode cc) {
  switch (cc) {
    case CondLess: return "CondLess";
    case CondLessEqu: return "CondLessEqu";
    case CondEqu: return "CondEqu";
    case CondGreater: return "CondGreater";
    case CondGreaterEqu: return "CondGreaterEqu";
    case CondNotEqu: return "CondNotEqu";
  }
  return "WTF";
}


struct ShaderInfo {
  char *name; // variable name
  LocInfo *locs;
  // vertex and fragment shader sources
  char *vssrc;
  char *fssrc;
  // basedir for includes (can be nullptr)
  char *basedir;
  // inclide dir (build from basedir)
  char *incdir;
  // defines (nullptr, or array of chars, each define terminates with '\0')
  DefineTable defines;
  // opengl version
  CondCode oglVersionCond;
  int oglVersion;  // high*100+low
  int forCubemaps;
  ShaderInfo *next;
};


ShaderInfo *shaderlist = nullptr;


void clearShaderList (ShaderInfo **slist) {
  if (!slist) return;
  while (*slist) {
    ShaderInfo *si = *slist;
    *slist = si->next;
    xfree(si->name);
    xfree(si->vssrc);
    xfree(si->fssrc);
    xfree(si->basedir);
    xfree(si->incdir);
    clearTblDefine(&si->defines);
    clearLocs(&si->locs);
    xfree(si);
  }
}


void appendDefine (ShaderInfo *si, const char *defstr) {
  addTblDefine(&si->defines, defstr);
}

void removeDefine (ShaderInfo *si, const char *defstr) {
  removeTblDefine(&si->defines, defstr);
}

int hasDefine (ShaderInfo *si, const char *defstr) {
  if (hasTblDefine(&globaldefines, defstr)) return 1;
  return hasTblDefine(&si->defines, defstr);
}


// ////////////////////////////////////////////////////////////////////////// //
int parseDigit (Parser *par) {
  //Parser *state = prSimpleSavePos(par);
  const char *tk = prGetToken(par);
  if (tk && !par->isString && !tk[1] && isdigit(tk[0])) {
    int dig = tk[0]-'0';
    //prSimpleFreePos(&state);
    return dig;
  }
  //prSimpleRestoreAndFreePos(par, &state);
  static char errmsg[1024];
  snprintf(errmsg, sizeof(errmsg), "expected digit, but got '%s'", (tk ? tk : "<EOF>"));
  prError(par, errmsg);
  return 0;
}


CondCode parseCondCode (Parser *par) {
  if (prCheck(par, "<")) return CondLess;
  if (prCheck(par, "<=")) return CondLessEqu;
  if (prCheck(par, ">")) return CondGreater;
  if (prCheck(par, ">=")) return CondGreaterEqu;
  if (prCheck(par, "!=")) return CondNotEqu;
  if (prCheck(par, "=")) return CondEqu;
  const char *tk = prGetToken(par);
  static char errmsg[1024];
  snprintf(errmsg, sizeof(errmsg), "expected comparison, but got '%s'", (tk ? tk : "<EOF>"));
  prError(par, errmsg);
  return CondEqu;
}


// `ShaderList` eaten
void parseShaderList (Parser *par, const char *basebase) {
  char *basedir = nullptr;
  char *incdir = nullptr;
  if (prCheck(par, "basedir")) {
    prExpect(par, "=");
    basedir = va("%s/%s", basebase, prExpectString(par));
  }
  if (!basedir) basedir = xstrdup(basebase);
  if (prCheck(par, "pakdir")) {
    prExpect(par, "=");
    incdir = xstrdupNull(prExpectString(par));
  }
  prExpect(par, "{");
  while (!prCheck(par, "}")) {
    if (prCheck(par, ";")) continue;
    prExpect(par, "Shader");
    CondCode oglVersionCond = CondGreaterEqu;
    int oglVersion = 0;  // high*100+low
    int forCubemaps = 0;
    // attributes (unused for now)
    while (prCheck(par, "[")) {
      if (prCheck(par, "advanced")) {
        // do nothing
      } else if (prCheck(par, "cubemaps")) {
        forCubemaps = 1;
      } else if (prCheck(par, "OpenGL")) {
        oglVersionCond = parseCondCode(par);
        int hi = parseDigit(par);
        prExpect(par, ".");
        int lo = parseDigit(par);
        oglVersion = hi*100+lo;
      } else {
        prError(par, "shader attribute expected");
      }
      prExpect(par, "]");
    }
    ShaderInfo *si = xalloc(sizeof(ShaderInfo));
    si->oglVersionCond = oglVersionCond;
    si->oglVersion = oglVersion;
    si->forCubemaps = forCubemaps;
    // name
    si->name = xstrdup(prExpectId(par));
    if (verbose) printf("[  found shader info '%s']\n", si->name);
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
    si->incdir = xstrdupNull(incdir);
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
  xfree(incdir);
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
  //if (!prCheck(par, "130") && !prCheck(par, "666")) prExpect(par, "120");
  // skip version token
  (void)prGetToken(par);

  DefineTable savedtbl;
  initTblDefineFrom(&savedtbl, &si->defines);

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
      // undef
      if (prCheck(par, "undef")) {
        char *tk = prExpectId(par);
        //fprintf(stderr, "<newdef: %s>\n", tk);
        removeDefine(si, tk);
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

        LocInfo *oloc = findLoc(si->locs, idname);
        if (oloc) {
          if (oloc->isAttr != isAttr) prError(par, va("conflicting uniform/attribyte for '%s'", idname));
          if (!strEqu(oloc->glslType, glslType)) prError(par, va("conflicting type for '%s'", idname));
        } else {
          LocInfo *loc = newLoc(&si->locs, idname);
          loc->isAttr = isAttr;
          loc->glslType = glslType;
        }
        xfree(idname); // it is copied in loc ctor
        //fprintf(stderr, "::: %s %s %s;\n", (loc->isAttr ? "attribute" : "uniform"), loc->glslType, loc->name);
        continue;
      }
    }

    prGetToken(par);
  }

  if (brclevel != 0) prError(par, "unbalanced compounds");

  // restore defines
  clearTblDefine(&si->defines);
  initTblDefineFrom(&si->defines, &savedtbl);
  clearTblDefine(&savedtbl);

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
  if (strEqu(glslType, "sampler2DShadow")) return "vuint32";
  if (strEqu(glslType, "samplerCube")) return "vuint32";
  if (strEqu(glslType, "samplerCubeShadow")) return "vuint32";
  fprintf(stderr, "FATAL: unknown GLSL type `%s`!\n", glslType);
  abort();
}


const char *getShitppStoreType (const char *glslType) {
  if (strEqu(glslType, "float")) return "float";
  if (strEqu(glslType, "bool")) return "bool";
  if (strEqu(glslType, "vec3")) return "TVec";
  if (strEqu(glslType, "mat4")) return "VMatrix4";
  // non-standard types
  if (strEqu(glslType, "vec4")) return "glsl_float4";
  if (strEqu(glslType, "vec2")) return "glsl_float2";
  if (strEqu(glslType, "mat3")) return "glsl_float9";
  // samplers
  if (strEqu(glslType, "sampler2D")) return "vuint32";
  if (strEqu(glslType, "sampler2DShadow")) return "vuint32";
  if (strEqu(glslType, "samplerCube")) return "vuint32";
  if (strEqu(glslType, "samplerCubeShadow")) return "vuint32";
  fprintf(stderr, "FATAL: unknown GLSL type `%s`!\n", glslType);
  abort();
}


const char *getShitppAmp (const char *shitppType) {
  if (shitppType[strlen(shitppType)-1] == '*') return "";
  if (strEqu(shitppType, "TVec")) return "&";
  if (strEqu(shitppType, "VMatrix4")) return "&";
  return "";
}


void writeInitValueNoChecks (FILE *fo, const LocInfo *loc) {
  if (!loc || !fo) return;
  if (strEqu(loc->glslType, "float")) {
    if (loc->isAttr) {
      fprintf(fo, "curr_%s = 0.0f;", loc->name);
    } else {
      fprintf(fo, "last_%s = curr_%s = 0.0f;", loc->name, loc->name);
    }
  } else if (strEqu(loc->glslType, "bool")) {
    if (loc->isAttr) {
      fprintf(fo, "curr_%s = false;", loc->name);
    } else {
      fprintf(fo, "last_%s = curr_%s = false;", loc->name, loc->name);
    }
  } else if (strEqu(loc->glslType, "sampler2D") ||
             strEqu(loc->glslType, "sampler2DShadow") ||
             strEqu(loc->glslType, "samplerCube") ||
             strEqu(loc->glslType, "samplerCubeShadow"))
  {
    if (loc->isAttr) {
      fprintf(fo, "curr_%s = 0;", loc->name);
    } else {
      fprintf(fo, "last_%s = curr_%s = 0;", loc->name, loc->name);
    }
  } else {
    if (strEqu(loc->glslType, "vec3")) {
      if (loc->isAttr) {
        fprintf(fo, "curr_%s = TVec(0.0f, 0.0f, 0.0f);", loc->name);
      } else {
        fprintf(fo, "last_%s = curr_%s = TVec(0.0f, 0.0f, 0.0f);", loc->name, loc->name);
      }
    } else if (strEqu(loc->glslType, "mat4")) {
      if (!loc->isAttr) fprintf(fo, "last_%s.SetZero(); ", loc->name);
      fprintf(fo, "curr_%s.SetZero();", loc->name);
    } else if (strEqu(loc->glslType, "vec4") || strEqu(loc->glslType, "vec2") || strEqu(loc->glslType, "mat3")) {
      if (!loc->isAttr) fprintf(fo, "memset((void *)last_%s, 0, sizeof(last_%s)); ", loc->name, loc->name);
      fprintf(fo, "memset((void *)curr_%s, 0, sizeof(curr_%s));", loc->name, loc->name);
    } else {
      fprintf(stderr, "FATAL: cannot emit initialiser for GLSL type '%s'\n", loc->glslType);
      abort();
    }
  }
}


void writeUploadNoChecks (FILE *fo, const LocInfo *loc) {
  if (!loc || !fo) return;
  fprintf(fo, "owner->%s", (loc->isAttr ? "p_glVertexAttrib" : "p_glUniform"));
       if (strEqu(loc->glslType, "float")) fprintf(fo, "1fARB(loc_%s, curr_%s);", loc->name, loc->name);
  else if (strEqu(loc->glslType, "bool")) fprintf(fo, "1iARB(loc_%s, (curr_%s ? GL_TRUE : GL_FALSE));", loc->name, loc->name);
  else if (strEqu(loc->glslType, "sampler2D")) fprintf(fo, "1iARB(loc_%s, (GLint)curr_%s);", loc->name, loc->name);
  else if (strEqu(loc->glslType, "sampler2DShadow")) fprintf(fo, "1iARB(loc_%s, (GLint)curr_%s);", loc->name, loc->name);
  else if (strEqu(loc->glslType, "samplerCube")) fprintf(fo, "1iARB(loc_%s, (GLint)curr_%s);", loc->name, loc->name);
  else if (strEqu(loc->glslType, "samplerCubeShadow")) fprintf(fo, "1iARB(loc_%s, (GLint)curr_%s);", loc->name, loc->name);
  else {
    const char *onestr = (loc->isAttr ? "" : "1,");
    if (strEqu(loc->glslType, "vec3")) fprintf(fo, "3fvARB(loc_%s, %s &curr_%s.x);", loc->name, onestr, loc->name);
    else if (strEqu(loc->glslType, "mat4")) fprintf(fo, "Matrix4fvARB(loc_%s, %s GL_FALSE, &curr_%s.m[0][0]);", loc->name, onestr, loc->name);
    else if (strEqu(loc->glslType, "vec4")) fprintf(fo, "4fvARB(loc_%s, %s &curr_%s[0]);", loc->name, onestr, loc->name);
    else if (strEqu(loc->glslType, "vec2")) fprintf(fo, "2fvARB(loc_%s, %s &curr_%s[0]);", loc->name, onestr, loc->name);
    else if (strEqu(loc->glslType, "mat3")) fprintf(fo, "Matrix3fvARB(loc_%s, %s GL_FALSE, &curr_%s[0]);", loc->name, onestr, loc->name);
    else { fprintf(stderr, "FATAL: cannot emit setter for GLSL type '%s'\n", loc->glslType); abort(); }
  }
}


void writeUploadWithChecks (FILE *fo, const LocInfo *loc) {
  if (!loc || !fo) return;
  fprintf(fo, "if (loc_%s >=0 ) { ", loc->name);
  writeUploadNoChecks(fo, loc);
  fprintf(fo, " }");
}


void checkSpecialModes (void) {
  const char *spmode = getenv("VSHADPP_MODE");
  if (!spmode || !spmode[0]) return;
  if (strEqu(spmode, "GLES")) addTblDefine(&globaldefines, "GL4ES_HACKS");
}


// ////////////////////////////////////////////////////////////////////////// //
int main (int argc, char **argv) {
  char *infname = nullptr;
  char *outdir = xstrdup(".");
  char *basebase = xstrdup(".");
  int toStdout = 0;

  for (int aidx = 1; aidx < argc; ++aidx) {
    char *arg = argv[aidx];
    if (strEqu(arg, "-v") || strEqu(arg, "--verbose")) { ++verbose; continue; }
    if (strEqu(arg, "-o")) {
      if (aidx+1 >= argc) { fprintf(stderr, "FATAL: '-o' expects argument!\n"); abort(); }
      xfree(outdir);
      outdir = xstrdup(argv[++aidx]);
      continue;
    }
    if (strEqu(arg, "-b")) {
      if (aidx+1 >= argc) { fprintf(stderr, "FATAL: '-b' expects argument!\n"); abort(); }
      xfree(basebase);
      basebase = xstrdup(argv[++aidx]);
      continue;
    }
    if (strEqu(arg, "-")) { toStdout = 1; continue; }
    if (arg[0] == '-' && arg[1] == 'D') {
      // new define
      arg += 2;
      addTblDefine(&globaldefines, arg);
      continue;
    }
    if (infname) { fprintf(stderr, "FATAL: duplicate file name!\n"); abort(); }
    infname = xstrdup(arg);
  }

  checkSpecialModes();

  if (!infname) { fprintf(stderr, "FATAL: file name expected!\n"); abort(); }

  if (verbose) printf("[parsing '%s'...]\n", infname);

  //char *outhname = createFileName(outdir, infname, ".hi");
  //char *outcname = createFileName(outdir, infname, ".ci");
  char *outhname = createFileName(outdir, infname, ".hi1");
  char *outcname = createFileName(outdir, infname, ".ci1");
  char *outhnameReal = createFileName(outdir, infname, ".hi");
  char *outcnameReal = createFileName(outdir, infname, ".ci");
  if (verbose) printf("[ generating '%s'...]\n", outhname);
  if (verbose) printf("[ generating '%s'...]\n", outcname);

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
      parseShaderList(par, basebase);
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
    if (verbose) printf("[  parsing vert shader '%s.vs']\n", si->vssrc);
    par = prCreateFromFile(va("%s/%s.vs", si->basedir, si->vssrc));
    parseGLSL(par, si);
    prDestroy(&par);
    if (verbose) printf("[  parsing frag shader '%s.fs']\n", si->fssrc);
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
      if (!loc->isAttr) fprintf(foh, "    %s last_%s;\n", getShitppStoreType(loc->glslType), loc->name);
      fprintf(foh, "    %s curr_%s;\n", getShitppStoreType(loc->glslType), loc->name);
      if (!loc->isAttr) fprintf(foh, "    bool changed_%s;\n", loc->name);
    }
    fprintf(foh, "\n");
    fprintf(foh, "  public:\n");
    fprintf(foh, "    VShaderDef_%s ();\n", si->name);
    fprintf(foh, "\n");
    fprintf(foh, "    virtual void Setup (VOpenGLDrawer *aowner) override;\n");
    fprintf(foh, "    virtual void LoadUniforms () override;\n");
    fprintf(foh, "    virtual void UnloadUniforms () override;\n");
    fprintf(foh, "    virtual void UploadChangedUniforms (bool forced=false) override;\n");
    /*fprintf(foh, "    virtual void UploadChangedAttrs () override;\n");*/
    fprintf(foh, "\n");

    // generate setters
    for (const LocInfo *loc = si->locs; loc; loc = loc->next) {
      if (loc->inset) continue; // will be generated later
      const char *shitppType = getShitppType(loc->glslType);
      fprintf(foh, "    inline void Set%s%s (const %s %sv) { ", loc->name, (loc->isAttr ? "Attr" : ""), shitppType, getShitppAmp(shitppType));
           if (strEqu(loc->glslType, "float")) fprintf(foh, "curr_%s = v; ", loc->name);
      else if (strEqu(loc->glslType, "bool")) fprintf(foh, "curr_%s = (v ? GL_TRUE : GL_FALSE);", loc->name);
      else if (strEqu(loc->glslType, "sampler2D")) fprintf(foh, "curr_%s = (GLint)v;", loc->name);
      else if (strEqu(loc->glslType, "sampler2DShadow")) fprintf(foh, "curr_%s = (GLint)v;", loc->name);
      else if (strEqu(loc->glslType, "samplerCube")) fprintf(foh, "curr_%s = (GLint)v;", loc->name);
      else if (strEqu(loc->glslType, "samplerCubeShadow")) fprintf(foh, "curr_%s = (GLint)v;", loc->name);
      else if (strEqu(loc->glslType, "vec2")) fprintf(foh, "memcpy(&curr_%s[0], v, sizeof(float)*2);", loc->name);
      else if (strEqu(loc->glslType, "vec3")) fprintf(foh, "curr_%s = v;", loc->name);
      else if (strEqu(loc->glslType, "vec4")) fprintf(foh, "memcpy(&curr_%s[0], v, sizeof(float)*4);", loc->name);
      else if (strEqu(loc->glslType, "mat3")) fprintf(foh, "memcpy(&curr_%s[0], v, sizeof(float)*9);", loc->name);
      else if (strEqu(loc->glslType, "mat4")) fprintf(foh, "memcpy(&curr_%s.m[0][0], &v.m[0][0], sizeof(float)*16);", loc->name);
      else { fprintf(stderr, "FATAL: cannot emit setter for GLSL type '%s'\n", loc->glslType); abort(); }
      //fprintf(foh, " changed_%s = true;", loc->name);
      // vertex attributes should be uploaded regardless of their value
      if (loc->isAttr) {
        // immediately upload attributes
        if (loc->isAttr) writeUploadWithChecks(foh, loc);
      }
      fprintf(foh, " }\n");
      // additional setters
      if (strEqu(loc->glslType, "vec3")) {
        fprintf(foh, "    inline void Set%s%s (const float x, const float y, const float z) { curr_%s = TVec(x, y, z); ", loc->name, (loc->isAttr ? "Attr" : ""), loc->name);
        // immediately upload attributes
        if (loc->isAttr) writeUploadWithChecks(foh, loc);
        fprintf(foh, " }\n");
      } else if (strEqu(loc->glslType, "mat4")) {
        fprintf(foh, "    inline void Set%s%s (const float * v) { memcpy(&curr_%s.m[0][0], v, sizeof(float)*16); ", loc->name, (loc->isAttr ? "Attr" : ""), loc->name);
        // immediately upload attributes
        if (loc->isAttr) writeUploadWithChecks(foh, loc);
        fprintf(foh, " }\n");
      } else if (strEqu(loc->glslType, "vec4")) {
        fprintf(foh, "    inline void Set%s%s (const float x, const float y, const float z, const float w) { ", loc->name, (loc->isAttr ? "Attr" : ""));
        fprintf(foh, "curr_%s[0] = x; curr_%s[1] = y; curr_%s[2] = z; curr_%s[3] = w;", loc->name, loc->name, loc->name, loc->name);
        // immediately upload attributes
        if (loc->isAttr) writeUploadWithChecks(foh, loc);
        fprintf(foh, " }\n");
      } else if (strEqu(loc->glslType, "vec2")) {
        fprintf(foh, "    inline void Set%s%s (const float x, const float y) { curr_%s[0] = x; curr_%s[1] = y; ", loc->name, (loc->isAttr ? "Attr" : ""), loc->name, loc->name);
        // immediately upload attributes
        if (loc->isAttr) writeUploadWithChecks(foh, loc);
        fprintf(foh, " }\n");
      }
    }

    // generate forced uploaders for all fields
    /*
    for (const LocInfo *loc = si->locs; loc; loc = loc->next) {
      fprintf(foh, "    inline void ForceUpload%s () { ", loc->name);
      fprintf(foh, " if (loc_%s >= 0) { ", loc->name);
      writeUploadNoChecks(foh, loc);
      fprintf(foh, " changed_%s = false; copyValue_%s(last_%s, curr_%s); }", loc->name, loc->glslType, loc->name, loc->name);
      fprintf(foh, " }\n");
    }
    */

    // generate set setters
    for (SetInfo *css = foundSets; css; css = css->nextTemp) {
      fprintf(foh, "%s", css->code);
    }
    fprintf(foh, "  };\n");

    // generate code
    // generate constructor
    fprintf(foc, "VOpenGLDrawer::VShaderDef_%s::VShaderDef_%s ()\n", si->name, si->name);
    fprintf(foc, "  : VGLShader()\n");
    for (const LocInfo *loc = si->locs; loc; loc = loc->next) {
      fprintf(foc, "  , loc_%s(-1)\n", loc->name);
      if (!loc->isAttr) fprintf(foc, "  , changed_%s(false)\n", loc->name);
    }
    fprintf(foc, "{\n");
    if (!(si->oglVersion == 0 && si->oglVersionCond == CondGreaterEqu)) {
      fprintf(foc, "  SetOpenGLVersion(%s, %d);\n", getCondName(si->oglVersionCond), (int)si->oglVersion);
    }
    if (si->forCubemaps) fprintf(foc, "  SetForCubemaps();\n");
    for (const LocInfo *loc = si->locs; loc; loc = loc->next) {
      fprintf(foc, "  ");
      writeInitValueNoChecks(foc, loc);
      fprintf(foc, "\n");
    }
    fprintf(foc, "}\n");

    fprintf(foc, "void VOpenGLDrawer::VShaderDef_%s::Setup (VOpenGLDrawer *aowner) {\n", si->name);
    fprintf(foc, "  MainSetup(aowner, \"%s\", \"%s\", \"glshaders/%s.vs\", \"glshaders/%s.fs\");\n", si->name, (si->incdir ? si->incdir : ""), si->vssrc, si->fssrc);
    fprintf(foc, "}\n");
    fprintf(foc, "\n");

    // generate uniform loader
    fprintf(foc, "void VOpenGLDrawer::VShaderDef_%s::LoadUniforms () {\n", si->name);
    for (const LocInfo *loc = si->locs; loc; loc = loc->next) {
      //HACK!
      const int optional = (strEqu(loc->name, "FogEnabled") || strEqu(loc->name, "CubeSize"));
      fprintf(foc, "  loc_%s = owner->glGet%sLoc(progname, prog, \"%s\"%s);\n", loc->name, (loc->isAttr ? "Attr" : "Uni"), loc->name, (optional ? ", true" : ""));
      if (!loc->isAttr) fprintf(foc, "  changed_%s = true;\n", loc->name);
    }
    fprintf(foc, "}\n");
    fprintf(foc, "\n");

    // generate uniform unloader
    fprintf(foc, "void VOpenGLDrawer::VShaderDef_%s::UnloadUniforms () {\n", si->name);
    for (const LocInfo *loc = si->locs; loc; loc = loc->next) {
      fprintf(foc, "  loc_%s = -1;\n", loc->name);
    }
    fprintf(foc, "}\n");

    // generate uniform uploader
    fprintf(foc, "void VOpenGLDrawer::VShaderDef_%s::UploadChangedUniforms (bool forced) {\n", si->name);
    for (const LocInfo *loc = si->locs; loc; loc = loc->next) {
      if (loc->isAttr) continue;
      fprintf(foc, "  if (loc_%s >= 0 && (forced || changed_%s || notEqual_%s(last_%s, curr_%s))) {\n", loc->name, loc->name, loc->glslType, loc->name, loc->name);
      fprintf(foc, "    ");
      writeUploadNoChecks(foc, loc);
      fprintf(foc, "\n    changed_%s = false;\n", loc->name);
      fprintf(foc, "    copyValue_%s(last_%s, curr_%s);\n", loc->glslType, loc->name, loc->name);
      fprintf(foc, "  }\n");
    }
    fprintf(foc, "}\n");
    fprintf(foc, "\n");

    // generate attribute uploader
    /* not needed
    fprintf(foc, "void VOpenGLDrawer::VShaderDef_%s::UploadChangedAttrs () {\n", si->name);
    for (const LocInfo *loc = si->locs; loc; loc = loc->next) {
      if (!loc->isAttr) continue;
      fprintf(foc, "  ");
      writeUploadWithChecks(foc, loc);
      fprintf(foc, "\n");
    }
    fprintf(foc, "}\n");
    fprintf(foc, "\n");
    */
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
    const char *dss = si->defines.defines;
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
    if (!compareFiles(outhname, outhnameReal) || !compareFiles(outcname, outcnameReal)) {
      copyFile(outhnameReal, outhname);
      copyFile(outcnameReal, outcname);
    }
  }

  clearSetList(&setlist);
  clearShaderList(&shaderlist);

  xfree(outdir);
  xfree(basebase);
  xfree(infname);
  xfree(outhname);
  xfree(outcname);

  vaShutdown();

  return 0;
}
