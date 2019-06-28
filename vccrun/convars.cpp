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
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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
#include "convars.h"


// ////////////////////////////////////////////////////////////////////////// //
static char *ccmdBuf = nullptr;
static size_t ccmdBufSize = 0;
static size_t ccmdBufUsed = 0;
static char snbuf[32768];

#define CCMD_MAX_ARGS  (256)

static VStr ccmdArgv[CCMD_MAX_ARGS];
static int ccmdArgc = 0; // current argc


void ccmdClearText () { ccmdBufUsed = 0; }

void ccmdClearCommand () {
  for (int f = ccmdArgc-1; f >= 0; --f) ccmdArgv[f].clear();
  ccmdArgc = 0;
}


int ccmdGetArgc () { return ccmdArgc; }
const VStr &ccmdGetArgv (int idx) { return (idx >= 0 && idx < ccmdArgc ? ccmdArgv[idx] : VStr::EmptyString); }

// return number of unparsed bytes left in
int ccmdTextSize () { return (int)ccmdBufUsed; }


// ////////////////////////////////////////////////////////////////////////// //
// parse one command
/*
enum CCResult {
  CCMD_EMPTY = -1, // no more commands (nothing was parsed)
  CCMD_NORMAL = 0, // one command parsed, line is not complete
  CCMD_EOL = 1, // one command parsed, line is complete
};
*/

static inline void ccmdShrink (size_t cpos) {
  if (cpos >= ccmdBufUsed) { ccmdBufUsed = 0; return; }
  if (cpos > 0) {
    memmove(ccmdBuf, ccmdBuf+cpos, ccmdBufUsed-cpos);
    ccmdBufUsed -= cpos;
  }
}


static inline int hdig (char ch) {
  if (ch >= '0' && ch <= '9') return ch-'0';
  if (ch >= 'A' && ch <= 'F') return ch-'A'+10;
  if (ch >= 'a' && ch <= 'f') return ch-'a'+10;
  return -1;
}


CCResult ccmdParseOne () {
  ccmdClearCommand();
  size_t cpos = 0;
  // find command start
  while (cpos < ccmdBufUsed) {
    vuint8 ch = (vuint8)ccmdBuf[cpos];
    if (ch == '\n') { ++cpos; ccmdShrink(cpos); return CCMD_EOL; }
    if (ch <= ' ') { ++cpos; continue; }
    if (ch == '#') {
      // comment
      while (cpos < ccmdBufUsed && ccmdBuf[cpos] != '\n') ++cpos;
      ++cpos;
      ccmdShrink(cpos);
      return CCMD_EOL;
    }
    if (ch == ';') { ++cpos; continue; }
    break;
  }
  if (cpos >= ccmdBufUsed) { ccmdBufUsed = 0; return CCMD_EMPTY; }
  // found something; parse it
  while (cpos < ccmdBufUsed) {
    vuint8 ch = (vuint8)ccmdBuf[cpos];
    if (ch == '\n' || ch == '#') break; // end-of-command
    if (ch == ';') { ++cpos; break; }
    if (ch <= ' ') { ++cpos; continue; }
    VStr tk;
    int n;
    // found a token
    if (ch == '\'' || ch == '"') {
      // quoted token
      vuint8 qch = ch;
      ++cpos;
      while (cpos < ccmdBufUsed) {
        ch = (vuint8)ccmdBuf[cpos++];
        if (ch == qch) break;
        if (ch != '\\') { tk += (char)ch; continue; }
        if (cpos >= ccmdBufUsed) break;
        ch = (vuint8)ccmdBuf[cpos++];
        switch (ch) {
          case 't': tk += '\t'; break;
          case 'n': tk += '\n'; break;
          case 'r': tk += '\r'; break;
          case 'e': tk += '\x1b'; break;
          case 'x':
            if (cpos >= ccmdBufUsed) break;
            n = hdig(ccmdBuf[cpos]);
            if (n < 0) break;
            ++cpos;
            if (cpos < ccmdBufUsed && hdig(ccmdBuf[cpos]) >= 0) n = n*16+hdig(ccmdBuf[cpos++]);
            if (n == 0) n = 32;
            tk += (char)n;
            break;
          default:
            tk += (char)ch;
            break;
        }
      }
    } else {
      // space-delimited
      while (cpos < ccmdBufUsed) {
        ch = (vuint8)ccmdBuf[cpos];
        if (ch <= ' ' || ch == '#' || ch == ';') break;
        tk += (char)ch;
        ++cpos;
      }
    }
    if (ccmdArgc < CCMD_MAX_ARGS) ccmdArgv[ccmdArgc++] = tk;
  }
  ccmdShrink(cpos);
  return CCMD_NORMAL;
}


// ////////////////////////////////////////////////////////////////////////// //
static void ccmdPrependChar (char ch) {
  if (!ch) return;
  if (ccmdBufUsed >= 1024*1024*32) Sys_Error("Command buffer overflow!");
  if (ccmdBufUsed+1 > ccmdBufSize) {
    size_t newsize = ((ccmdBufUsed+1)|0xffffU)+1;
    ccmdBuf = (char *)Z_Realloc(ccmdBuf, newsize);
    ccmdBufSize = newsize;
  }
  if (ccmdBufUsed) memmove(ccmdBuf+1, ccmdBuf, ccmdBufUsed);
  ccmdBuf[0] = ch;
  ++ccmdBufUsed;
}


void ccmdPrepend (const char *str) {
  if (!str || !str[0]) return;
  size_t slen = strlen(str);
  if (slen > 1024*1024*32 || ccmdBufUsed+slen > 1024*1024*32) Sys_Error("Command buffer overflow!");
  if (ccmdBufUsed+slen > ccmdBufSize) {
    size_t newsize = ((ccmdBufUsed+slen)|0xffffU)+1;
    ccmdBuf = (char *)Z_Realloc(ccmdBuf, newsize);
    if (!ccmdBuf) Sys_Error("Out of memory for command buffer!");
    ccmdBufSize = newsize;
  }
  if (ccmdBufUsed) memmove(ccmdBuf+slen, ccmdBuf, ccmdBufUsed);
  memcpy(ccmdBuf, str, slen);
  ccmdBufUsed += slen;
}


void ccmdPrepend (const VStr &str) {
  if (str.length()) ccmdPrepend(*str);
}


__attribute__((format(printf,1,2))) void ccmdPrependf (const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  snbuf[0] = 0;
  snbuf[sizeof(snbuf)-1] = 0;
  vsnprintf(snbuf, sizeof(snbuf), fmt, ap);
  va_end(ap);
  ccmdPrepend(snbuf);
}


void ccmdPrependQuoted (const char *str) {
  if (!str || !str[0]) return;
  bool needQuote = false;
  for (const vuint8 *s = (const vuint8 *)str; *s; ++s) {
    if (*s <= ' ' || *s == '\'' || *s == '"' || *s == '\\' || *s == 127) {
      needQuote = true;
      break;
    }
  }
  if (!needQuote) { ccmdPrepend(str); return; }
  for (const vuint8 *s = (const vuint8 *)str; *s; ++s) {
    if (*s < ' ') {
      char xbuf[6];
      snprintf(xbuf, sizeof(xbuf), "\\x%02x", *s);
      ccmdPrepend(xbuf);
      continue;
    }
    if (*s == ' ' || *s == '\'' || *s == '"' || *s == '\\' || *s == 127) ccmdPrependChar('\\');
    ccmdPrependChar((char)*s);
  }
}


void ccmdPrependQuoted (const VStr &str) {
  if (str.length()) ccmdPrependQuoted(*str);
}

__attribute__((format(printf,1,2))) void ccmdPrependQuotdedf (const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  snbuf[0] = 0;
  snbuf[sizeof(snbuf)-1] = 0;
  vsnprintf(snbuf, sizeof(snbuf), fmt, ap);
  va_end(ap);
  ccmdPrependQuoted(snbuf);
}


// ////////////////////////////////////////////////////////////////////////// //
static inline void ccmdAppendChar (char ch) {
  if (!ch) return;
  if (ccmdBufUsed >= 1024*1024*32) Sys_Error("Command buffer overflow!");
  if (ccmdBufUsed+1 > ccmdBufSize) {
    size_t newsize = ((ccmdBufUsed+1)|0xffffU)+1;
    ccmdBuf = (char *)Z_Realloc(ccmdBuf, newsize);
    ccmdBufSize = newsize;
  }
  ccmdBuf[ccmdBufUsed++] = ch;
}


void ccmdAppend (const char *str) {
  if (!str || !str[0]) return;
  size_t slen = strlen(str);
  if (slen > 1024*1024*32 || ccmdBufUsed+slen > 1024*1024*32) Sys_Error("Command buffer overflow!");
  if (ccmdBufUsed+slen > ccmdBufSize) {
    size_t newsize = ((ccmdBufUsed+slen)|0xffffU)+1;
    ccmdBuf = (char *)Z_Realloc(ccmdBuf, newsize);
    ccmdBufSize = newsize;
  }
  memcpy(ccmdBuf+ccmdBufUsed, str, slen);
  ccmdBufUsed += slen;
}


void ccmdAppend (const VStr &str) {
  if (str.length() > 0) ccmdAppend(*str);
}


__attribute__((format(printf,1,2))) void ccmdAppendf (const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  snbuf[0] = 0;
  snbuf[sizeof(snbuf)-1] = 0;
  vsnprintf(snbuf, sizeof(snbuf), fmt, ap);
  va_end(ap);
  ccmdAppend(snbuf);
}


void ccmdAppendQuoted (const char *str) {
  if (!str || !str[0]) return;
  bool needQuote = false;
  for (const vuint8 *s = (const vuint8 *)str; *s; ++s) {
    if (*s < ' ' || *s == '"' || *s == '\\' || *s == 127) {
      needQuote = true;
      break;
    }
  }
  if (!needQuote) { ccmdAppend(str); return; }
  ccmdAppendChar('"');
  for (const vuint8 *s = (const vuint8 *)str; *s; ++s) {
    if (*s == '\t') { ccmdAppend("\\t"); continue; }
    if (*s == '\n') { ccmdAppend("\\n"); continue; }
    if (*s == '\r') { ccmdAppend("\\r"); continue; }
    if (*s == 0x1b) { ccmdAppend("\\e"); continue; }
    if (*s < ' ' || *s == 127) {
      char xbuf[6];
      snprintf(xbuf, sizeof(xbuf), "\\x%02x", *s);
      ccmdAppend(xbuf);
      continue;
    }
    if (*s == '"' || *s == '\\') ccmdAppendChar('\\');
    ccmdAppendChar((char)*s);
  }
  ccmdAppendChar('"');
}


void ccmdAppendQuoted (const VStr &str) {
  if (str.length()) ccmdAppendQuoted(*str);
}

__attribute__((format(printf,1,2))) void ccmdAppendQuotedf (const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  snbuf[0] = 0;
  snbuf[sizeof(snbuf)-1] = 0;
  vsnprintf(snbuf, sizeof(snbuf), fmt, ap);
  va_end(ap);
  ccmdAppendQuoted(snbuf);
}


// ////////////////////////////////////////////////////////////////////////// //
static char *cbuf = nullptr;
static vuint32 cbufHead = 0;
static vuint32 cbufTail = 0; // `cbuftail` points *at* last char
//static bool cbufLastWasCR = false;
static vuint32 cbufcursize = 0;
static vuint32 cbufmaxsize = 256*1024;


static inline vuint32 textsize () {
  return
    cbufTail == cbufHead ? cbufcursize :
    cbufTail > cbufHead ? cbufTail-cbufHead :
    cbufcursize-cbufHead+cbufTail;
}

/*
static char *cbufPos (vuint32 pos) {
  return
    pos >= textsize() ? nullptr :
    pos < cbufcursize-cbufHead ? cbuf+cbufHead+pos :
    cbuf+(pos-(cbufcursize-cbufHead));
}
*/

static char cbufAt (vuint32 pos) {
  return
    pos >= textsize() ? '\0' :
    *(pos < cbufcursize-cbufHead ? cbuf+cbufHead+pos :
      cbuf+(pos-(cbufcursize-cbufHead)));
}

static vuint32 getLastLineStart () {
  vuint32 sz = textsize();
  while (sz--) {
    if (cbufAt(sz) == '\n') return sz+1;
  }
  return 0;
}


//==========================================================================
//
//  PutCharInternal
//
//==========================================================================
static void PutCharInternal (char ch, bool doDump) {
  if (ch == '\r') return;

  if (cbufcursize != cbufmaxsize) {
    cbuf = (char *)Z_Realloc(cbuf, cbufmaxsize);
    //FIXME
    cbufHead = 0;
    cbufTail = 1;
    cbuf[0] = '\n';
    cbufcursize = cbufmaxsize;
  }

  if (ch == '\t') {
    if (doDump) putc(ch, stdout);
    auto lslen = (textsize()-getLastLineStart())%8;
    for (; lslen < 8; ++lslen) PutCharInternal(' ', doDump);
    return;
  }

  if (ch != '\n' && (vuint8)ch < 32) ch = ' ';

  // has room?
  if (cbufTail == cbufHead) {
    // oops, no room; remove top line then
    /*
    bool foundNL = false;
    vuint32 cbsz = cbufcursize;
    vuint32 cbh = cbufHead;
    cbh = (cbh+cbsz-1)%cbsz; // one char back, unconditionally
    for (vuint32 cnt = cbsz-1; cnt > 0; --cnt) {
      cbh = (cbh+cbsz-1)%cbsz;
      if (cncbuf[cbh] == '\n') {
        cbufHead = cbh;
        foundNL = true;
        break;
      }
    }
    if (!foundNL) {
      //FIXME: no newline, just clear it all
      cbufHead = 0;
      cbufTail = 1;
      cbuf[0] = ch;
      return;
    }
    */
    cbufHead = (cbufHead+1)%cbufcursize;
  }
  cbuf[cbufTail++] = ch;
  cbufTail %= cbufcursize;

  if (doDump) putc(ch, stdout);
}


//==========================================================================
//
//  conWriteStr
//
//==========================================================================
void conWriteStr (const VStr &str) {
  if (!str.isEmpty()) {
    fprintf(stdout, "%.*s", (vuint32)str.length(), *str);
    const char *strd = *str;
    int slen = str.length();
    while (slen--) PutCharInternal(*strd++, false);
  }
}


//==========================================================================
//
//  conWriteStr
//
//==========================================================================
void conWriteStr (const char *str, size_t strlen) {
  if (strlen) fprintf(stdout, "%.*s", (vuint32)strlen, str);
  while (strlen--) PutCharInternal(*str++, false);
}


//==========================================================================
//
//  conPutChar
//
//==========================================================================
void conPutChar (char ch) {
  PutCharInternal(ch, true);
}
