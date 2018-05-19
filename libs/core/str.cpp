//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
//
//  Dynamic string class.
//
//**************************************************************************

//#include <cctype>

#include "core.h"

#if !defined _WIN32 && !defined DJGPP
#undef stricmp  //  Allegro defines them
#undef strnicmp
#define stricmp   strcasecmp
#define strnicmp  strncasecmp
#endif


// ////////////////////////////////////////////////////////////////////////// //
static char va_buffer[16][65536];
static int va_bufnum = 0;


// ////////////////////////////////////////////////////////////////////////// //
VStr::VStr (const VStr& instr, int start, int len) noexcept(false) : data(nullptr) {
  guard(VStr::VStr);
  check(start >= 0);
  check(start <= (int)instr.Length());
  check(len >= 0);
  check(start+len <= (int)instr.Length());
  if (len) {
    Resize(len);
    memcpy(data, instr.data+start, len);
  }
  unguard;
}


void VStr::MakeMutable () {
  guard(VStr::MakeMutable);
  if (!data || *refp() == 1) return; // nothing to do
  // allocate new string
  size_t newsize = (size_t)*lenp()+sizeof(int)*2+1;
  char *newdata = new char[newsize];
  memcpy(newdata, data-sizeof(int)*2, newsize);
  --(*refp()); // decrement old refcounter
  data = newdata;
  data += sizeof(int)*2; // skip bookkeeping data
  *refp() = 1; // set new refcounter
  unguard;
}


void VStr::Resize (int newlen) {
  guard(VStr::Resize);

  check(newlen >= 0);

  if (newlen <= 0) {
    // free string
    if (data) {
      if (--(*refp()) == 0) delete[](data-sizeof(int)*2);
      data = nullptr;
    }
    return;
  }

  if ((size_t)newlen == Length()) {
    // same length, make string unique (just in case)
    MakeMutable();
    return;
  }

  // allocate new memory buffer
  size_t newsize = newlen+sizeof(int)*2+1;
  char *newdata = new char[newsize];
  char *newstrdata = newdata+sizeof(int)*2;

  // copy old contents, if any
  if (data) {
    // has old content, copy it
    int oldlen = *lenp();
    if (newlen < oldlen) {
      // new string is smaller
      memcpy(newstrdata, data, newlen);
    } else {
      // new string is bigger
      memcpy(newstrdata, data, oldlen+1); // take zero too; it should be overwritten by the caller later
    }
    // decref old string, and free it, if necessary
    if (--(*refp()) == 0) delete[](data-sizeof(int)*2);
  } else {
    // no old content
    newstrdata[0] = 0; // to be on a safe side
  }
  newstrdata[newlen] = 0; // set trailing zero, just in case

  // setup new pointer and bookkeeping
  data = newstrdata;
  *lenp() = newlen;
  *refp() = 1;

  unguard;
}


void VStr::SetContents (const char *s, int len) {
  guard(VStr::SetContents);
  if (s && s[0] && len != 0) {
    if (len < 0) len = (s ? (int)strlen(s) : 0);
    if (len) {
      // check for pathological case: is `s` inside our data?
      if (s == data && (size_t)len == length()) {
        // this is prolly `VStr(*otherstr)` case, so just increment refcount
        ++(*refp());
      } else if (isMyData(s, len)) {
        // make temporary copy
        char *temp = new char[len];
        memcpy(temp, s, len);
        Resize(len);
        memcpy(data, temp, len);
        delete[] temp;
      } else {
        Resize(len);
        memcpy(data, s, len);
      }
    } else {
      Clear();
    }
  } else {
    Clear();
  }
  unguard;
}


bool VStr::StartsWith (const char* s) const {
  guard(VStr::StartsWith);
  if (!s || !s[0]) return false;
  size_t l = Length(s);
  if (l > Length()) return false;
  return (NCmp(**this, s, l) == 0);
  unguard;
}


bool VStr::StartsWith (const VStr& s) const {
  guard(VStr::StartsWith);
  size_t l = s.Length();
  if (l > Length()) return false;
  return (NCmp(**this, *s, l) == 0);
  unguard;
}


bool VStr::EndsWith (const char* s) const {
  guard(VStr::EndsWith);
  if (!s || !s[0]) return false;
  size_t l = Length(s);
  if (l > Length()) return false;
  return (NCmp(**this+Length()-l, s, l) == 0);
  unguard;
}


bool VStr::EndsWith (const VStr& s) const {
  guard(VStr::EndsWith);
  size_t l = s.Length();
  if (l > Length()) return false;
  return (NCmp(**this+Length()-l, *s, l) == 0);
  unguard;
}


VStr VStr::ToLower () const {
  guard(VStr::ToLower);
  if (!data) return VStr();
  bool hasWork = false;
  int l = Length();
  for (int i = 0; i < l; ++i) if (data[i] >= 'A' && data[i] <= 'Z') { hasWork = true; break; }
  if (hasWork) {
    VStr res;
    res = *this;
    res.MakeMutable();
    for (int i = 0; i < l; ++i) if (res.data[i] >= 'A' && res.data[i] <= 'Z') res.data[i] += 32; // poor man's tolower()
    return res;
  } else {
    return VStr(*this);
  }
  unguard;
}


VStr VStr::ToUpper () const {
  guard(VStr::ToUpper);
  if (!data) return VStr();
  bool hasWork = false;
  int l = Length();
  for (int i = 0; i < l; ++i) if (data[i] >= 'a' && data[i] <= 'z') { hasWork = true; break; }
  if (hasWork) {
    VStr res;
    res = *this;
    res.MakeMutable();
    for (int i = 0; i < l; ++i) if (res.data[i] >= 'a' && res.data[i] <= 'z') res.data[i] -= 32; // poor man's toupper()
    return res;
  } else {
    return VStr(*this);
  }
  unguard;
}


int VStr::IndexOf (char c) const {
  guard(VStr::IndexOf);
  if (data) {
    const char *pos = (const char *)memchr(data, c, *lenp());
    return (pos ? (int)(pos-data) : -1);
  } else {
    return -1;
  }
  unguard;
}


int VStr::IndexOf (const char* s) const {
  guard(VStr::IndexOf);
  if (!s || !s[0]) return -1;
  int sl = int(Length(s));
  int l = int(Length());
  for (int i = 0; i <= l-sl; ++i) if (NCmp(data+i, s, sl) == 0) return i;
  return -1;
  unguard;
}


int VStr::IndexOf (const VStr& s) const {
  guard(VStr::IndexOf);
  int sl = int(s.Length());
  if (!sl) return -1;
  int l = int(Length());
  for (int i = 0; i <= l-sl; ++i) if (NCmp(data+i, *s, sl) == 0) return i;
  return -1;
  unguard;
}


int VStr::LastIndexOf (char c) const {
  guard(VStr::LastIndexOf);
  if (data) {
    const char *pos = (const char *)memrchr(data, c, *lenp());
    return (pos ? (int)(pos-data) : -1);
  } else {
    return -1;
  }
  unguard;
}


int VStr::LastIndexOf (const char* s) const {
  guard(VStr::LastIndexOf);
  if (!s || !s[0]) return -1;
  int sl = int(Length(s));
  int l = int(Length());
  for (int i = l-sl; i >= 0; --i) if (NCmp(data+i, s, sl) == 0) return i;
  return -1;
  unguard;
}


int VStr::LastIndexOf (const VStr& s) const {
  guard(VStr::LastIndexOf);
  int sl = int(s.Length());
  if (!sl) return -1;
  int l = int(Length());
  for (int i = l-sl; i >= 0; --i) if (NCmp(data + i, *s, sl) == 0) return i;
  return -1;
  unguard;
}


VStr VStr::Replace (const char *Search, const char *Replacement) const {
  guard(VStr::Replace);
  if (!Length()) return VStr(*this); // nothing to replace in an empty string

  size_t SLen = Length(Search);
  size_t RLen = Length(Replacement);
  if (!SLen) return VStr(*this); // nothing to search for

  VStr res = VStr(*this);
  size_t i = 0;
  while (i <= res.Length()-SLen) {
    if (NCmp(res.data+i, Search, SLen) == 0) {
      // if search and replace strings are of the same size,
      // we can just copy the data and avoid memory allocations
      if (SLen == RLen) {
        res.MakeMutable();
        memcpy(res.data+i, Replacement, RLen);
      } else {
        //FIXME: optimize this!
        res = VStr(res, 0, int(i))+Replacement+VStr(res, int(i+SLen), int(res.Length()-i-SLen));
      }
      i += RLen;
    } else {
      ++i;
    }
  }

  return res;
  unguard;
}


VStr VStr::Replace (const VStr& Search, const VStr& Replacement) const {
  guard(VStr::Replace);
  if (!Length()) return VStr(*this); // nothing to replace in an empty string

  size_t SLen = Search.Length();
  size_t RLen = Replacement.Length();
  if (!SLen) return VStr(*this); // nothing to search for

  VStr res = *this;
  size_t i = 0;
  while (i <= res.Length()-SLen) {
    if (NCmp(res.data+i, *Search, SLen) == 0) {
      // if search and replace strings are of the same size,
      // we can just copy the data and avoid memory allocations
      if (SLen == RLen) {
        res.MakeMutable();
        memcpy(res.data+i, *Replacement, RLen);
      } else {
        //FIXME: optimize this!
        res = VStr(res, 0, int(i))+Replacement+VStr(res, int(i+SLen), int(res.Length()-i-SLen));
      }
      i += RLen;
    } else {
      ++i;
    }
  }

  return res;
  unguard;
}


VStr VStr::Utf8Substring (int start, int len) const {
  check(start >= 0);
  check(start <= (int)Utf8Length());
  check(len >= 0);
  check(start+len <= (int)Utf8Length());
  if (!len) return VStr();
  int RealStart = int(ByteLengthForUtf8(data, start));
  int RealLen = int(ByteLengthForUtf8(data, start+len)-RealStart);
  return VStr(*this, RealStart, RealLen);
}


void VStr::Split (char c, TArray<VStr>& A) const {
  guard(VStr::Split);

  A.Clear();
  if (!data) return;

  int start = 0;
  int len = int(Length());
  for (int i = 0; i <= len; ++i) {
    if (i == len || data[i] == c) {
      if (start != i) A.Append(VStr(*this, start, i-start));
      start = i+1;
    }
  }

  unguard;
}


void VStr::Split (const char *chars, TArray<VStr>& A) const {
  guard(VStr::Split);

  A.Clear();
  if (!data) return;

  int start = 0;
  int len = int(Length());
  for (int i = 0; i <= len; ++i) {
    bool DoSplit = (i == len);
    for (const char* pChar = chars; !DoSplit && *pChar; ++pChar) DoSplit = (data[i] == *pChar);
    if (DoSplit) {
      if (start != i) A.Append(VStr(*this, start, i-start));
      start = i+1;
    }
  }

  unguard;
}


// split string to path components; first component can be '/', others has no slashes
void VStr::SplitPath (TArray<VStr>& arr) const {
  guard(VStr::SplitPath);

  arr.Clear();
  if (!data) return;

  int pos = 0;
  int len = *lenp();

#if !defined(_WIN32)
  if (data[0] == '/') arr.Append(VStr("/"));
  while (pos < len) {
    if (data[pos] == '/') { ++pos; continue; }
    int epos = pos+1;
    while (epos < len && data[epos] != '/') ++epos;
    arr.Append(VStr(*this, pos, epos-pos));
    pos = epos+1;
  }
#else
  if (data[0] == '/' || data[0] == '\\') {
    arr.Append(VStr("/"));
  } else if (data[1] == ':' && (data[2] == '/' || data[2] == '\\')) {
    arr.Append(VStr(*this, 0, 3));
    pos = 3;
  }
  while (pos < len) {
    if (data[pos] == '/' || data[pos] == '\\') { ++pos; continue; }
    int epos = pos+1;
    while (epos < len && data[epos] != '/' && data[epos] != '\\') ++epos;
    arr.Append(VStr(*this, pos, epos-pos));
    pos = epos+1;
  }
#endif

  unguard;
}


bool VStr::IsValidUtf8 () const {
  guard(VStr::IsValidUtf8);
  if (!data) return true;
  for (const char* c = data; *c;) {
    if ((*c&0x80) == 0) {
      ++c;
    } else if ((*c&0xe0) == 0xc0) {
      if ((c[1]&0xc0) != 0x80) return false;
      c += 2;
    } else if ((*c&0xf0) == 0xe0) {
      if ((c[1]&0xc0) != 0x80 || (c[2]&0xc0) != 0x80) return false;
      c += 3;
    } else if ((*c&0xf8) == 0xf0) {
      if ((c[1]&0xc0) != 0x80 || (c[2]&0xc0) != 0x80 || (c[3]&0xc0) != 0x80) return false;
      c += 4;
    } else {
      return false;
    }
  }
  return true;
  unguard;
}


VStr VStr::Latin1ToUtf8 () const {
  guard(VStr::Latin1ToUtf8);
  VStr res;
  for (size_t i = 0; i < Length(); ++i) res += FromChar((vuint8)data[i]);
  return res;
  unguard;
}


VStr VStr::EvalEscapeSequences () const {
  guard(VStr::EvalEscapeSequences);
  VStr res;
  char val;
  for (const char* c = **this; *c; ++c) {
    if (*c == '\\') {
      ++c;
      switch (*c) {
        case 't': res += '\t'; break;
        case 'n': res += '\n'; break;
        case 'r': res += '\r'; break;
        case 'c': res += TEXT_COLOUR_ESCAPE; break;
        case 'x':
          val = 0;
          ++c;
          for (int i = 0; i < 2; ++i) {
                 if (*c >= '0' && *c <= '9') val = (val<<4)+*c-'0';
            else if (*c >= 'a' && *c <= 'f') val = (val<<4)+10+*c-'a';
            else if (*c >= 'A' && *c <= 'F') val = (val<<4)+10+*c-'A';
            else break;
            ++c;
          }
          --c;
          res += val;
          break;
        case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7':
          val = 0;
          for (int i = 0; i < 3; ++i) {
            if (*c >= '0' && *c <= '7') val = (val<<3)+*c-'0'; else break;
            ++c;
          }
          --c;
          res += val;
          break;
        case '\n':
          break;
        case 0:
          --c;
          break;
        default:
          res += *c;
          break;
      }
    } else {
      res += *c;
    }
  }
  return res;
  unguard;
}


VStr VStr::RemoveColours () const {
  guard(VStr::RemoveColours);
  bool hasWork = false;
  for (const char* c = **this; *c; ++c) if (*c == TEXT_COLOUR_ESCAPE) { hasWork = true; break; }
  if (hasWork) {
    VStr res;
    for (const char *c = **this; *c; ++c) {
      if (*c == TEXT_COLOUR_ESCAPE) {
        if (c[1]) ++c;
        if (*c == '[') { while (c[1] && *c != ']') ++c; }
        continue;
      }
      res += *c;
    }
    return res;
  } else {
    return VStr(*this);
  }
  unguard;
}


VStr VStr::ExtractFilePath () const {
  guard(FL_ExtractFilePath);
  const char *src = data+Length();
#if !defined(_WIN32)
  while (src != data && src[-1] != '/') --src;
#else
  while (src != data && src[-1] != '/' && src[-1] != '\\') --src;
#endif
  return VStr(*this, 0, src-data);
  unguard;
}


VStr VStr::ExtractFileName() const {
  guard(VStr:ExtractFileName);
  const char *src = data+Length();
#if !defined(_WIN32)
  while (src != data && src[-1] != '/') --src;
#else
  while (src != data && src[-1] != '/' && src[-1] != '\\') --src;
#endif
  return VStr(src);
  unguard;
}


VStr VStr::ExtractFileBase () const {
  guard(VStr::ExtractFileBase);
  int i = int(Length());

  if (i == 0) return VStr();

#if !defined(_WIN32)
  // back up until a \ or the start
  while (i && data[i-1] != '/') --i;
#else
  while (i && data[i-1] != '/' && data[i-1] != '\\') --i;
#endif

  // copy up to eight characters
  int start = i;
  int length = 0;
  while (data[i] && data[i] != '.') {
    if (++length == 9) Sys_Error("Filename base of %s >8 chars", data);
    ++i;
  }
  return VStr(*this, start, length);
  unguard;
}


VStr VStr::ExtractFileExtension () const {
  guard(VStr::ExtractFileExtension);
  const char *src = data+Length();
  while (src != data) {
    char ch = src[-1];
    if (ch == '.') return VStr(src);
#if !defined(_WIN32)
    if (ch == '/') return VStr();
#else
    if (ch == '/' || ch == '\\') return VStr();
#endif
    --src;
  }
  return VStr();
  unguard;
}


VStr VStr::StripExtension () const {
  guard(VStr::StripExtension);
  const char *src = data+Length();
  while (src != data) {
    char ch = src[-1];
    if (ch == '.') return VStr(*this, 0, src-data-1);
#if !defined(_WIN32)
    if (ch == '/') break;
#else
    if (ch == '/' || ch == '\\') break;
#endif
    --src;
  }
  return VStr(*this);
  unguard;
}


VStr VStr::DefaultPath (const VStr& basepath) const {
  guard(VStr::DefaultPath);
#if !defined(_WIN32)
  if (data && data[0] == '/') return *this; // absolute path location
#else
  if (data && data[0] == '/') return *this; // absolute path location
  if (data && data[0] == '\\') return *this; // absolute path location
  if (data && data[1] == ':' && (data[2] == '/' || data[2] == '\\')) return *this; // absolute path location
#endif
  return basepath+*this;
  unguard;
}


// if path doesn't have a .EXT, append extension (extension should include the leading dot)
VStr VStr::DefaultExtension (const VStr& extension) const {
  guard(VStr::DefaultExtension);
  const char *src = data+Length();
  while (src != data) {
    char ch = src[-1];
    if (ch == '.') return VStr(*this);
#if !defined(_WIN32)
    if (ch == '/') break;
#else
    if (ch == '/' || ch == '\\') break;
#endif
    --src;
  }
  return VStr(*this)+extension;
  unguard;
}


VStr VStr::FixFileSlashes () const {
  guard(VStr::FixFileSlashes);
  bool hasWork = false;
  for (const char* c = **this; *c; ++c) if (*c == '\\') { hasWork = true; break; }
  if (hasWork) {
    VStr res(*this);
    res.MakeMutable();
    for (char* c = res.data; *c; ++c) if (*c == '\\') *c = '/';
    return res;
  } else {
    return VStr(*this);
  }
  unguard;
}


size_t VStr::Utf8Length (const char *s) {
  guard(VStr::Utf8Length);
  size_t count = 0;
  if (s) {
    for (const char* c = s; *c; ++c) if ((*c&0xc0) != 0x80) ++count;
  }
  return count;
  unguard;
}


size_t VStr::ByteLengthForUtf8 (const char *s, size_t N) {
  guard(VStr::ByteLengthForUtf8);
  if (s) {
    size_t count = 0;
    const char* c;
    for (c = s; *c; ++c) {
      if ((*c&0xc0) != 0x80) {
        if (count == N) return c-s;
        ++count;
      }
    }
    check(N == count);
    return c-s;
  } else {
    return 0;
  }
  unguard;
}


int VStr::GetChar (const char*& s) {
  guard(VStr::GetChar);
  if ((vuint8)*s < 128) return *s++;
  int cnt, val;
  if ((*s&0xe0) == 0xc0) {
    val = *s&0x1f;
    cnt = 1;
  } else if ((*s&0xf0) == 0xe0) {
    val = *s&0x0f;
    cnt = 2;
  } else if ((*s&0xf8) == 0xf0) {
    val = *s&0x07;
    cnt = 3;
  } else {
    Sys_Error("Not a valid UTF-8");
    return 0;
  }
  ++s;

  do {
    if ((*s&0xc0) != 0x80) Sys_Error("Not a valid UTF-8");
    val = (val<<6)|(*s&0x3f);
    ++s;
  } while (--cnt);

  return val;
  unguard;
}


VStr VStr::FromChar (int c) {
  guard(VStr::FromChar);
  char res[8];
  if (c < 0x80) {
    res[0] = c;
    res[1] = 0;
  } else if (c < 0x800) {
    res[0] = 0xc0|(c&0x1f);
    res[1] = 0x80|((c>>5)&0x3f);
    res[2] = 0;
  } else if (c < 0x10000) {
    res[0] = 0xe0|(c&0x0f);
    res[1] = 0x80|((c>>4)&0x3f);
    res[2] = 0x80|((c>>10)&0x3f);
    res[3] = 0;
  } else {
    res[0] = 0xf0|(c&0x07);
    res[1] = 0x80|((c>>3)&0x3f);
    res[2] = 0x80|((c>>9)&0x3f);
    res[3] = 0x80|((c>>15)&0x3f);
    res[4] = 0;
  }
  return res;
  unguard;
}


/*
int VStr::ICmp (const char* S1, const char* S2) {
#ifdef WIN32
  return _stricmp(S1, S2);
#else
  return stricmp(S1, S2);
#endif
}


int VStr::NICmp (const char *S1, const char *S2, size_t N) {
  #ifdef WIN32
    return _strnicmp(S1, S2, N);
  #else
    return strnicmp(S1, S2, N);
  #endif
}
*/


//==========================================================================
//
//  va
//
// Very usefull function from QUAKE
// Does a varargs printf into a temp buffer, so I don't need to have
// varargs versions of all text functions.
// FIXME: make this buffer size safe someday
//
//==========================================================================

char *va (const char *text, ...) {
  va_list args;
  va_bufnum = (va_bufnum+1)&15;
  va_start(args, text);
  vsnprintf(va_buffer[va_bufnum], 65535, text, args);
  va_end(args);
  return va_buffer[va_bufnum];
}
