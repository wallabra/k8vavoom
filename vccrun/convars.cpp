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
#define USE_SIMPLE_HASHFN

bool VCvar::Initialised = false;
bool VCvar::Cheating;
void (*VCvar::logfn) (const char *fmt, va_list ap) = nullptr;

#define CVAR_HASH_SIZE  (512)
static VCvar *cvhBuckets[CVAR_HASH_SIZE] = {nullptr};


static __attribute__((format(printf, 1, 2))) void logf (const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  if (VCvar::logfn) {
    VCvar::logfn(fmt, ap);
  } else {
    vfprintf(stderr, fmt, ap);
  }
  va_end(ap);
}


// ////////////////////////////////////////////////////////////////////////// //
static inline vuint32 cvnamehash (const char *buf) {
  if (!buf || !buf[0]) return 1;
#ifdef USE_SIMPLE_HASHFN
  return djbHashBufCI(buf, strlen(buf));
#else
  return fnvHashBufCI(buf, strlen(buf));
#endif
}


static bool xstrcmpCI (const char *s, const char *pat) {
  if (!s || !pat || !s[0] || !pat[0]) return false;
  while (*s && *s <= ' ') ++s;
  while (*s && *pat) {
    char c0 = *s++;
    char c1 = *pat++;
    if (c0 != c1) {
      if (c0 >= 'A' && c0 <= 'Z') c0 += 32; // poor man's tolower
      if (c1 >= 'A' && c1 <= 'Z') c1 += 32; // poor man's tolower
      if (c0 != c1) return false;
    }
  }
  if (*pat || *s > ' ') return false;
  while (*s && *s <= ' ') ++s;
  return (s[0] == 0);
}


static bool convertInt (const char *s, int *outv) {
  bool neg = false;
  *outv = 0;
  if (!s || !s[0]) return false;
  while (*s && *s <= ' ') ++s;
  if (*s == '+') ++s; else if (*s == '-') { neg = true; ++s; }
  if (!s[0]) return false;
  if (s[0] < '0' || s[0] > '9') return false;
  while (*s) {
    char ch = *s++;
    if (ch < '0' || ch > '9') { *outv = 0; return false; }
    *outv = (*outv)*10+ch-'0';
  }
  while (*s && *s <= ' ') ++s;
  if (*s) { *outv = 0; return false; }
  if (neg) *outv = -(*outv);
  return true;
}


static bool convertFloat (const char *s, float *outv) {
  *outv = 0.0f;
  if (!s || !s[0]) return false;
  while (*s && *s <= ' ') ++s;
  bool neg = (s[0] == '-');
  if (s[0] == '+' || s[0] == '-') ++s;
  if (!s[0]) return false;
  // int part
  bool wasNum = false;
  if (s[0] >= '0' && s[0] <= '9') {
    wasNum = true;
    while (s[0] >= '0' && s[0] <= '9') *outv = (*outv)*10+(*s++)-'0';
  }
  // fractional part
  if (s[0] == '.') {
    ++s;
    if (s[0] >= '0' && s[0] <= '9') {
      wasNum = true;
      float v = 0, div = 1.0f;
      while (s[0] >= '0' && s[0] <= '9') {
        div *= 10.0f;
        v = v*10+(*s++)-'0';
      }
      *outv += v/div;
    }
  }
  // 'e' part
  if (wasNum && (s[0] == 'e' || s[0] == 'E')) {
    ++s;
    bool negexp = (s[0] == '-');
    if (s[0] == '-' || s[0] == '+') ++s;
    if (s[0] < '0' || s[0] > '9') { *outv = 0; return false; }
    int exp = 0;
    while (s[0] >= '0' && s[0] <= '9') exp = exp*10+(*s++)-'0';
    while (exp != 0) {
      if (negexp) *outv /= 10.0f; else *outv *= 10.0f;
      --exp;
    }
  }
  // skip trailing 'f', if any
  if (wasNum && s[0] == 'f') ++s;
  // trailing spaces
  while (*s && *s <= ' ') ++s;
  if (*s || !wasNum) { *outv = 0; return false; }
  if (neg) *outv = -(*outv);
  return true;
}


// ////////////////////////////////////////////////////////////////////////// //
VCvar::VCvar(const char *AName, const char *ADefault, const char *AHelp, int AFlags)
  : cvname(AName)
  , defaultString(ADefault)
  , helpString(AHelp)
  , defstrOwned(false)
  , flags(AFlags)
  , intValue(0)
  , floatValue(0)
  , boolValue(false)
  , nextInBucket(nullptr)
{
  if (!defaultString) defaultString = ""; // 'cause why not?
  if (!helpString || !helpString[0]) helpString = "no help yet (FIXME!)";
  if (cvname && cvname[0]) {
    insertIntoHash(); // insert into hash (this leaks on duplicate vars)
    if (Initialised) Register();
  }
}


VCvar::VCvar(const char *AName, const VStr &ADefault, const VStr &AHelp, int AFlags)
  : cvname(AName)
  , helpString("no help yet")
  , defstrOwned(true)
  , flags(AFlags)
  , intValue(0)
  , floatValue(0)
  , boolValue(false)
  , nextInBucket(nullptr)
{
  char *tmp = new char[ADefault.Length()+1];
  VStr::Cpy(tmp, *ADefault);
  defaultString = tmp;

  if (AHelp.Length() > 0) {
    tmp = new char[AHelp.Length()+1];
    VStr::Cpy(tmp, *AHelp);
    helpString = tmp;
  }

  if (cvname && cvname[0]) {
    insertIntoHash(); // insert into hash (this leaks on duplicate vars)
    check(Initialised);
    Register();
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// returns replaced cvar, or nullptr
VCvar *VCvar::insertIntoHash () {
  if (!cvname || !cvname[0]) return nullptr;
  vuint32 nhash = cvnamehash(cvname);
  lnhash = nhash;
  VCvar *prev = nullptr;
  for (VCvar *cvar = cvhBuckets[nhash%CVAR_HASH_SIZE]; cvar; prev = cvar, cvar = cvar->nextInBucket) {
    if (cvar->lnhash == nhash && !VStr::ICmp(cvname, cvar->cvname)) {
      // replace it
      if (prev) {
        prev->nextInBucket = this;
      } else {
        cvhBuckets[nhash%CVAR_HASH_SIZE] = this;
      }
      nextInBucket = cvar->nextInBucket;
      return cvar;
    }
  }
  // new one
  nextInBucket = cvhBuckets[nhash%CVAR_HASH_SIZE];
  cvhBuckets[nhash%CVAR_HASH_SIZE] = this;
  return nullptr;
}


// ////////////////////////////////////////////////////////////////////////// //
void VCvar::Register () {
  //!!!VCommand::AddToAutoComplete(cvname);
  DoSet(defaultString);
}


// ////////////////////////////////////////////////////////////////////////// //
void VCvar::Set (int value) {
  Set(VStr(value));
}


void VCvar::Set (float value) {
  Set(VStr(value));
}


void VCvar::Set (const VStr &AValue) {
  if (flags&CVAR_Latch) {
    latchedString = AValue;
    return;
  }

  if (AValue == stringValue) return;

  if ((flags&CVAR_Cheat) != 0 && !Cheating) {
    logf("'%s' cannot be changed while cheating is disabled.\n", cvname);
    return;
  }

  DoSet(AValue);
  flags |= CVAR_Modified;
}


// ////////////////////////////////////////////////////////////////////////// //
// does the actual value assignement
void VCvar::DoSet (const VStr &AValue) {
  stringValue = AValue;
  bool validInt = convertInt(*stringValue, &intValue);
  bool validFloat = convertFloat(*stringValue, &floatValue);

  // interpret boolean
  if (validFloat) {
    // easy
    boolValue = (floatValue != 0);
  } else if (validInt) {
    // easy
    boolValue = (intValue != 0);
  } else {
    // check various strings
    boolValue =
      xstrcmpCI(*stringValue, "true") ||
      xstrcmpCI(*stringValue, "on") ||
      xstrcmpCI(*stringValue, "tan") ||
      xstrcmpCI(*stringValue, "yes");
    intValue = (boolValue ? 1 : 0);
    floatValue = intValue;
  }
  if (!validInt && validFloat) intValue = (int)floatValue;
}


// ////////////////////////////////////////////////////////////////////////// //
void VCvar::Init () {
  for (vuint32 bkn = 0; bkn < CVAR_HASH_SIZE; ++bkn) {
    for (VCvar *cvar = cvhBuckets[bkn]; cvar; cvar = cvar->nextInBucket) {
      cvar->Register();
    }
  }
  Initialised = true;
}


void VCvar::dumpHashStats () {
  vuint32 bkused = 0, maxchain = 0;
  for (vuint32 bkn = 0; bkn < CVAR_HASH_SIZE; ++bkn) {
    VCvar *cvar = cvhBuckets[bkn];
    if (!cvar) continue;
    ++bkused;
    vuint32 chlen = 0;
    for (; cvar; cvar = cvar->nextInBucket) ++chlen;
    if (chlen > maxchain) maxchain = chlen;
  }
  //logf("CVAR statistics: %u buckets used, %u items in longest chain\n", bkused, maxchain);
}


// ////////////////////////////////////////////////////////////////////////// //
// this is called only once on engine shutdown, so don't bother with deletion
void VCvar::Shutdown () {
  dumpHashStats();
  for (vuint32 bkn = 0; bkn < CVAR_HASH_SIZE; ++bkn) {
    for (VCvar *cvar = cvhBuckets[bkn]; cvar; cvar = cvar->nextInBucket) {
      // free default value
      if (cvar->defstrOwned) {
        delete[] const_cast<char*>(cvar->defaultString);
        cvar->defaultString = ""; // set to some sensible value
      }
    }
  }
  Initialised = false;
}


// ////////////////////////////////////////////////////////////////////////// //
void VCvar::Unlatch () {
  for (vuint32 bkn = 0; bkn < CVAR_HASH_SIZE; ++bkn) {
    for (VCvar *cvar = cvhBuckets[bkn]; cvar; cvar = cvar->nextInBucket) {
      if (cvar->latchedString.IsNotEmpty()) {
        cvar->DoSet(cvar->latchedString);
        cvar->latchedString.Clean();
      }
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
void VCvar::SetCheating (bool new_state) {
  Cheating = new_state;
  if (!Cheating) {
    for (vuint32 bkn = 0; bkn < CVAR_HASH_SIZE; ++bkn) {
      for (VCvar *cvar = cvhBuckets[bkn]; cvar; cvar = cvar->nextInBucket) {
        if (cvar->flags&CVAR_Cheat) cvar->DoSet(cvar->defaultString);
      }
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
void VCvar::CreateNew (VName var_name, const VStr &ADefault, const VStr &AHelp, int AFlags) {
  if (var_name == NAME_None) return;
  VCvar *cvar = FindVariable(*var_name);
  if (!cvar) {
    new VCvar(*var_name, ADefault, AHelp, AFlags);
  } else {
    // delete old default value if necessary
    if (cvar->defstrOwned) delete[] const_cast<char*>(cvar->defaultString);
    // set new default value
    {
      char *tmp = new char[ADefault.Length()+1];
      VStr::Cpy(tmp, *ADefault);
      cvar->defaultString = tmp;
      cvar->defstrOwned = true;
    }
    // set new help value
    if (AHelp.Length() > 0) {
      char *tmp = new char[AHelp.Length()+1];
      VStr::Cpy(tmp, *AHelp);
      cvar->helpString = tmp;
    } else {
      cvar->helpString = "no help yet";
    }
    // update flags
    cvar->flags = AFlags;
  }
}


// ////////////////////////////////////////////////////////////////////////// //
bool VCvar::HasVar (const char *var_name) {
  return (FindVariable(var_name) != nullptr);
}


VCvar *VCvar::FindVariable (const char *name) {
  if (!name || name[0] == 0) return nullptr;
  vuint32 nhash = cvnamehash(name);
  for (VCvar *cvar = cvhBuckets[nhash%CVAR_HASH_SIZE]; cvar; cvar = cvar->nextInBucket) {
    if (cvar->lnhash == nhash && !VStr::ICmp(name, cvar->cvname)) return cvar;
  }
  return nullptr;
}


// ////////////////////////////////////////////////////////////////////////// //
int VCvar::GetInt (const char *var_name) {
  VCvar *var = FindVariable(var_name);
  return (var ? var->intValue : 0);
}


float VCvar::GetFloat (const char *var_name) {
  VCvar *var = FindVariable(var_name);
  return (var ? var->floatValue : 0.0f);
}


bool VCvar::GetBool (const char *var_name) {
  VCvar *var = FindVariable(var_name);
  return (var ? var->boolValue : false);
}


const char *VCvar::GetCharp (const char *var_name) {
  VCvar *var = FindVariable(var_name);
  return (var ? *var->stringValue : "");
}


VStr VCvar::GetString (const char *var_name) {
  VCvar *var = FindVariable(var_name);
  if (!var) return VStr();
  return var->stringValue;
}


// ////////////////////////////////////////////////////////////////////////// //
const char *VCvar::GetHelp (const char *var_name) {
  VCvar *var = FindVariable(var_name);
  if (!var) return nullptr;
  return var->helpString;
}


// ////////////////////////////////////////////////////////////////////////// //
int VCvar::GetVarFlags (const char *var_name) {
  guard(VCvar::GetHelp);
  VCvar *var = FindVariable(var_name);
  if (!var) return -1;
  return var->getFlags();
  unguard;
}


// ////////////////////////////////////////////////////////////////////////// //
void VCvar::Set (const char *var_name, int value) {
  VCvar *var = FindVariable(var_name);
  if (!var) Sys_Error("Cvar_Set: variable %s not found\n", var_name);
  var->Set(value);
}


void VCvar::Set (const char *var_name, float value) {
  VCvar *var = FindVariable(var_name);
  if (!var) Sys_Error("Cvar_Set: variable %s not found\n", var_name);
  var->Set(value);
}


void VCvar::Set (const char *var_name, const VStr &value) {
  VCvar *var = FindVariable(var_name);
  if (!var) Sys_Error("Cvar_SetString: variable %s not found\n", var_name);
  var->Set(value);
}


// ////////////////////////////////////////////////////////////////////////// //
vuint32 VCvar::countCVars () {
  vuint32 count = 0;
  for (vuint32 bkn = 0; bkn < CVAR_HASH_SIZE; ++bkn) {
    for (VCvar *cvar = cvhBuckets[bkn]; cvar; cvar = cvar->nextInBucket) {
      ++count;
    }
  }
  return count;
}


// ////////////////////////////////////////////////////////////////////////// //
// contains `countCVars()` elements, must be `delete[]`d.
// can return `nullptr`.
VCvar **VCvar::getSortedList () {
  vuint32 count = countCVars();
  if (count == 0) return nullptr;

  // allocate array
  VCvar **list = new VCvar*[count];

  // fill it
  count = 0; // reuse counter, why not?
  for (vuint32 bkn = 0; bkn < CVAR_HASH_SIZE; ++bkn) {
    for (VCvar *cvar = cvhBuckets[bkn]; cvar; cvar = cvar->nextInBucket) {
      list[count++] = cvar;
    }
  }

  // sort it (yes, i know, bubble sort sux. idc.)
  if (count > 1) {
    // straight from wikipedia, lol
    vuint32 n = count;
    do {
      vuint32 newn = 0;
      for (vuint32 i = 1; i < n; ++i) {
        if (VStr::ICmp(list[i-1]->cvname, list[i]->cvname) > 0) {
          VCvar *tmp = list[i];
          list[i] = list[i-1];
          list[i-1] = tmp;
          newn = i;
        }
      }
      n = newn;
    } while (n != 0);
  }

  return list;
}


// ////////////////////////////////////////////////////////////////////////// //
void VCvar::WriteVariablesToFile (FILE *f) {
  if (!f) return;
  vuint32 count = countCVars();
  VCvar **list = getSortedList();
  for (vuint32 n = 0; n < count; ++n) {
    VCvar *cvar = list[n];
    if (cvar->flags&CVAR_Archive) fprintf(f, "%s\t\t\"%s\"\n", cvar->cvname, *cvar->stringValue.quote());
  }
  delete[] list;
}


// ////////////////////////////////////////////////////////////////////////// //
VCvarI &VCvarI::operator = (const VCvarB &v) { Set(v.asBool() ? 1 : 0); return *this; }
VCvarI &VCvarI::operator = (const VCvarI &v) { Set(v.intValue); return *this; }

VCvarF &VCvarF::operator = (const VCvarB &v) { Set(v.asBool() ? 1.0f : 0.0f); return *this; }
VCvarF &VCvarF::operator = (const VCvarI &v) { Set((float)v.asInt()); return *this; }
VCvarF &VCvarF::operator = (const VCvarF &v) { Set(v.floatValue); return *this; }

VCvarB &VCvarB::operator = (const VCvarB &v) { Set(v.boolValue ? 1 : 0); return *this; }
VCvarB &VCvarB::operator = (const VCvarI &v) { Set(v.asInt() ? 1 : 0); return *this; }
VCvarB &VCvarB::operator = (const VCvarF &v) { Set(v.asFloat() ? 1 : 0); return *this; }


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
    ccmdBuf = (char *)realloc(ccmdBuf, newsize);
    if (!ccmdBuf) Sys_Error("Out of memory for command buffer!");
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
    ccmdBuf = (char *)realloc(ccmdBuf, newsize);
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
    ccmdBuf = (char *)realloc(ccmdBuf, newsize);
    if (!ccmdBuf) Sys_Error("Out of memory for command buffer!");
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
    ccmdBuf = (char *)realloc(ccmdBuf, newsize);
    if (!ccmdBuf) Sys_Error("Out of memory for command buffer!");
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
    cbuf = (char *)realloc(cbuf, cbufmaxsize);
    if (!cbuf) Sys_Error("Out of memory for console buffer");
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
