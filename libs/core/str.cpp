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
const VStr VStr::EmptyString = VStr();


// ////////////////////////////////////////////////////////////////////////// //
const vuint16 VStr::cp1251[128] = {
  0x0402,0x0403,0x201A,0x0453,0x201E,0x2026,0x2020,0x2021,0x20AC,0x2030,0x0409,0x2039,0x040A,0x040C,0x040B,0x040F,
  0x0452,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,0x003F,0x2122,0x0459,0x203A,0x045A,0x045C,0x045B,0x045F,
  0x00A0,0x040E,0x045E,0x0408,0x00A4,0x0490,0x00A6,0x00A7,0x0401,0x00A9,0x0404,0x00AB,0x00AC,0x00AD,0x00AE,0x0407,
  0x00B0,0x00B1,0x0406,0x0456,0x0491,0x00B5,0x00B6,0x00B7,0x0451,0x2116,0x0454,0x00BB,0x0458,0x0405,0x0455,0x0457,
  0x0410,0x0411,0x0412,0x0413,0x0414,0x0415,0x0416,0x0417,0x0418,0x0419,0x041A,0x041B,0x041C,0x041D,0x041E,0x041F,
  0x0420,0x0421,0x0422,0x0423,0x0424,0x0425,0x0426,0x0427,0x0428,0x0429,0x042A,0x042B,0x042C,0x042D,0x042E,0x042F,
  0x0430,0x0431,0x0432,0x0433,0x0434,0x0435,0x0436,0x0437,0x0438,0x0439,0x043A,0x043B,0x043C,0x043D,0x043E,0x043F,
  0x0440,0x0441,0x0442,0x0443,0x0444,0x0445,0x0446,0x0447,0x0448,0x0449,0x044A,0x044B,0x044C,0x044D,0x044E,0x044F,
};

char VStr::wc2shitmap[65536];


void VStr::vstrInitr_fuck_you_gnu_binutils_fuck_you_fuck_you_fuck_you () {
  memset(VStr::wc2shitmap, '?', sizeof(VStr::wc2shitmap));
  for (int f = 0; f < 128; ++f) VStr::wc2shitmap[f] = (char)f;
  for (int f = 0; f < 128; ++f) VStr::wc2shitmap[VStr::cp1251[f]] = (char)(f+128);
}


// ////////////////////////////////////////////////////////////////////////// //
// fast state-machine based UTF-8 decoder; using 8 bytes of memory
// code points from invalid range will never be valid, this is the property of the state machine
// see http://bjoern.hoehrmann.de/utf-8/decoder/dfa/
// [0..$16c-1]
// maps bytes to character classes
const vuint8 VUtf8DecoderFast::utf8dfa[0x16c] = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 00-0f
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 10-1f
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 20-2f
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 30-3f
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 40-4f
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 50-5f
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 60-6f
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 70-7f
  0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01, // 80-8f
  0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09, // 90-9f
  0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07, // a0-af
  0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07, // b0-bf
  0x08,0x08,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02, // c0-cf
  0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02, // d0-df
  0x0a,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x04,0x03,0x03, // e0-ef
  0x0b,0x06,0x06,0x06,0x05,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08, // f0-ff
  // maps a combination of a state of the automaton and a character class to a state
  0x00,0x0c,0x18,0x24,0x3c,0x60,0x54,0x0c,0x0c,0x0c,0x30,0x48,0x0c,0x0c,0x0c,0x0c, // 100-10f
  0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x00,0x0c,0x0c,0x0c,0x0c,0x0c,0x00, // 110-11f
  0x0c,0x00,0x0c,0x0c,0x0c,0x18,0x0c,0x0c,0x0c,0x0c,0x0c,0x18,0x0c,0x18,0x0c,0x0c, // 120-12f
  0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x18,0x0c,0x0c,0x0c,0x0c,0x0c,0x18,0x0c,0x0c, // 130-13f
  0x0c,0x0c,0x0c,0x0c,0x0c,0x18,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x24, // 140-14f
  0x0c,0x24,0x0c,0x0c,0x0c,0x24,0x0c,0x0c,0x0c,0x0c,0x0c,0x24,0x0c,0x24,0x0c,0x0c, // 150-15f
  0x0c,0x24,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c};


// ////////////////////////////////////////////////////////////////////////// //
VStr::VStr (int v) : data(nullptr) {
  char buf[64];
  int len = (int)snprintf(buf, sizeof(buf), "%d", v);
  setContent(buf, len);
}

VStr::VStr (unsigned v) : data(nullptr) {
  char buf[64];
  int len = (int)snprintf(buf, sizeof(buf), "%u", v);
  setContent(buf, len);
}

VStr::VStr (float v) : data(nullptr) {
  char buf[64];
  int len = (int)snprintf(buf, sizeof(buf), "%f", v);
  setContent(buf, len);
}

VStr::VStr (double v) : data(nullptr) {
  char buf[64];
  int len = (int)snprintf(buf, sizeof(buf), "%f", v);
  setContent(buf, len);
}


// ////////////////////////////////////////////////////////////////////////// //
VStr &VStr::utf8Append (vuint32 code) {
  if (code < 0 || code > 0x10FFFF) return operator+=('?');
  if (code <= 0x7f) return operator+=((char)(code&0xff));
  if (code <= 0x7FF) {
    operator+=((char)(0xC0|(code>>6)));
    return operator+=((char)(0x80|(code&0x3F)));
  }
  if (code <= 0xFFFF) {
    operator+=((char)(0xE0|(code>>12)));
    operator+=((char)(0x80|((code>>6)&0x3f)));
    return operator+=((char)(0x80|(code&0x3F)));
  }
  if (code <= 0x10FFFF) {
    operator+=((char)(0xF0|(code>>18)));
    operator+=((char)(0x80|((code>>12)&0x3f)));
    operator+=((char)(0x80|((code>>6)&0x3f)));
    return operator+=((char)(0x80|(code&0x3F)));
  }
  return operator+=('?');
}


bool VStr::isUtf8Valid () const {
  int slen = length();
  if (slen < 1) return true;
  int pos = 0;
  while (pos < slen) {
    int len = utf8CodeLen(data[pos]);
    if (len < 1) return false; // invalid sequence start
    if (pos+len-1 > slen) return false; // out of chars in string
    --len;
    ++pos;
    // check other sequence bytes
    while (len > 0) {
      if ((data[pos]&0xC0) != 0x80) return false;
      --len;
      ++pos;
    }
  }
  return true;
}


// ////////////////////////////////////////////////////////////////////////// //
VStr VStr::toLowerCase1251 () const {
  int slen = length();
  if (slen < 1) return VStr();
  for (int f = 0; f < slen; ++f) {
    if (locase1251(data[f]) != data[f]) {
      VStr res(*this);
      res.makeMutable();
      for (int c = 0; c < slen; ++c) res.data[c] = locase1251(res.data[c]);
      return res;
    }
  }
  return VStr(*this);
}


VStr VStr::toUpperCase1251 () const {
  int slen = length();
  if (slen < 1) return VStr();
  for (int f = 0; f < slen; ++f) {
    if (upcase1251(data[f]) != data[f]) {
      VStr res(*this);
      res.makeMutable();
      for (int c = 0; c < slen; ++c) res.data[c] = upcase1251(res.data[c]);
      return res;
    }
  }
  return VStr(*this);
}


// ////////////////////////////////////////////////////////////////////////// //
VStr VStr::utf2win () const {
  VStr res;
  if (length()) {
    VUtf8DecoderFast dc;
    for (int f = 0; f < length(); ++f) {
      if (dc.put(data[f])) res += wchar2win(dc.codepoint);
    }
  }
  return res;
}


VStr VStr::win2utf () const {
  VStr res;
  if (length()) {
    for (int f = 0; f < length(); ++f) {
      vuint8 ch = (vuint8)data[f];
      if (ch > 127) res.utf8Append(cp1251[ch-128]); else res += (char)ch;
    }
  }
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
bool VStr::fnameEqu1251CI (const char *s) const {
  size_t slen = length();
  if (!s || !s[0]) return (slen == 0);
  size_t pos = 0;
  while (pos < slen && *s) {
    if (data[pos] == '/') {
      if (*s != '/') return false;
      while (pos < slen && data[pos] == '/') ++pos;
      while (*s == '/') ++s;
      continue;
    }
    if (locase1251(data[pos]) != locase1251(*s)) return false;
    ++pos;
    ++s;
  }
  return (*s == 0);
}


// ////////////////////////////////////////////////////////////////////////// //
VStr VStr::mid (int start, int len) const {
  guard(VStr::mid);
  int mylen = length();
  if (mylen == 0) return VStr();
  if (len <= 0 || start >= mylen) return VStr();
  if (start < 0) {
    if (start+len <= 0) return VStr();
    len += start;
    start = 0;
  }
  if (start+len > mylen) {
    if ((len = mylen-start) < 1) return VStr();
  }
  if (start == 0 && len == mylen) return VStr(*this);
  return VStr(data+start, len);
  unguard;
}


VStr VStr::left (int len) const {
  if (len < 1) return VStr();
  if (len >= length()) return VStr(*this);
  return mid(0, len);
}


VStr VStr::right (int len) const {
  if (len < 1) return VStr();
  if (len >= length()) return VStr(*this);
  return mid(length()-len, len);
}


void VStr::chopLeft (int len) {
  if (len < 1) return;
  if (len >= length()) { clear(); return; }
  memmove(data, data+len, length()-len);
  resize(length()-len);
}


void VStr::chopRight (int len) {
  if (len < 1) return;
  if (len >= length()) { clear(); return; }
  resize(length()-len);
}


// ////////////////////////////////////////////////////////////////////////// //
void VStr::makeMutable () {
  guard(VStr::makeMutable);
  if (!data || store()->rc == 1) return; // nothing to do
  // allocate new string
  Store *oldstore = store();
  const char *olddata = data;
  size_t olen = (size_t)oldstore->length;
  size_t newsz = olen+64; // overallocate a little
  Store *newdata = (Store *)Z_Malloc(sizeof(Store)+newsz+1);
  if (!newdata) Sys_Error("Out of memory");
  newdata->length = (int)olen;
  newdata->alloted = (int)newsz;
  newdata->rc = 1;
  data = ((char *)newdata)+sizeof(Store);
  // copy old data
  memcpy(data, olddata, olen+1);
  --oldstore->rc; // decrement old refcounter
#ifdef VAVOOM_TEST_VSTR
  fprintf(stderr, "VStr: makeMutable: old=%p(%d); new=%p(%d)\n", oldstore+1, oldstore->rc, data, newdata->rc);
#endif
  unguard;
}


void VStr::resize (int newlen) {
  guard(VStr::resize);

  // free string?
  if (newlen <= 0) {
    decref();
    return;
  }

  int oldlen = length();

  if (newlen == oldlen) {
    // same length, make string unique (just in case)
    makeMutable();
    return;
  }

  // new allocation?
  if (!data) {
    size_t newsz = (size_t)(newlen+64);
    Store *ns = (Store *)Z_Malloc(sizeof(Store)+newsz+1);
    if (!ns) Sys_Error("Out of memory");
    ns->length = newlen;
    ns->alloted = newsz;
    ns->rc = 1;
    #ifdef VAVOOM_TEST_VSTR
    fprintf(stderr, "VStr: realloced(new): old=%p(%d); new=%p(%d)\n", data, 0, ns+1, ns->rc);
    #endif
    data = ((char *)ns)+sizeof(Store);
    data[newlen] = 0;
    return;
  }

  // unique?
  if (store()->rc == 1) {
    // do in-place reallocs
    if (newlen < oldlen) {
      // shrink
      if (newlen < store()->alloted/2) {
        // realloc
        store()->alloted = newlen+64;
        Store *ns = (Store *)Z_Realloc(store(), sizeof(Store)+(size_t)store()->alloted+1);
        if (!ns) Sys_Error("Out of memory");
        #ifdef VAVOOM_TEST_VSTR
        fprintf(stderr, "VStr: realloced(shrink): old=%p(%d); new=%p(%d)\n", data, store()->rc, ns+1, ns->rc);
        #endif
        data = ((char *)ns)+sizeof(Store);
      }
    } else {
      // grow
      if (newlen > store()->alloted) {
        // need more room
        size_t newsz = (size_t)(newlen+(newlen < 0x0fffffff ? newlen/2 : 0));
        Store *ns = (Store *)Z_Realloc(store(), sizeof(Store)+newsz+1);
        if (!ns) {
          // try exact
          ns = (Store *)Z_Realloc(store(), sizeof(Store)+(size_t)newlen+1);
          if (!ns) Sys_Error("Out of memory");
          newsz = (size_t)newlen;
        }
        ns->alloted = newsz;
        #ifdef VAVOOM_TEST_VSTR
        fprintf(stderr, "VStr: realloced(grow): old=%p(%d); new=%p(%d)\n", data, store()->rc, ns+1, ns->rc);
        #endif
        data = ((char *)ns)+sizeof(Store);
      }
    }
    store()->length = newlen;
  } else {
    // not unique, have to allocate new data
    int alloclen;

    if (newlen < oldlen) {
      // shrink
      alloclen = newlen+64;
    } else {
      // grow
      alloclen = newlen+(newlen < 0x0fffffff ? newlen/2 : 0);
    }

    // allocate new storage
    Store *ns = (Store *)Z_Malloc(sizeof(Store)+alloclen+1);
    if (!ns) {
      // try exact
      ns = (Store *)Z_Malloc(sizeof(Store)+(size_t)newlen+1);
      if (!ns) Sys_Error("Out of memory");
      alloclen = newlen;
    }

    // copy data
    if (newlen > oldlen) {
      memcpy(((char *)ns)+sizeof(Store), data, oldlen+1);
    } else {
      memcpy(((char *)ns)+sizeof(Store), data, newlen+1);
    }
    // setup info
    ns->length = newlen;
    ns->alloted = alloclen;
    ns->rc = 1;
    // decrement old rc
    --store()->rc;
    #ifdef VAVOOM_TEST_VSTR
    fprintf(stderr, "VStr: realloced(new): old=%p(%d); new=%p(%d)\n", data, store()->rc, ns+1, ns->rc);
    #endif
    // use new data
    data = ((char *)ns)+sizeof(Store);
  }

  // some functions expects this
  data[newlen] = 0;

  unguard;
}


void VStr::setContent (const char *s, int len) {
  guard(VStr::setContent);
  if (s && s[0]) {
    if (len < 0) len = (int)strlen(s);
    size_t newsz = len+64;
    Store *ns = (Store *)Z_Malloc(sizeof(Store)+(size_t)newsz+1);
    if (!ns) Sys_Error("Out of memory");
    ns->length = len;
    ns->alloted = (int)newsz;
    ns->rc = 1;
    memcpy(((char *)ns)+sizeof(Store), s, len);
    // free this string
    clear();
    // use new data
    data = ((char *)ns)+sizeof(Store);
    data[len] = 0;
    #ifdef VAVOOM_TEST_VSTR
    fprintf(stderr, "VStr: setContent: new=%p(%d)\n", data, store()->rc);
    #endif
  } else {
    clear();
  }
  unguard;
}


bool VStr::StartsWith (const char *s) const {
  guard(VStr::StartsWith);
  if (!s || !s[0]) return false;
  int l = length(s);
  if (l > length()) return false;
  return (NCmp(data, s, l) == 0);
  unguard;
}


bool VStr::StartsWith (const VStr &s) const {
  guard(VStr::StartsWith);
  int l = s.length();
  if (l > length()) return false;
  return (NCmp(data, *s, l) == 0);
  unguard;
}


bool VStr::EndsWith (const char *s) const {
  guard(VStr::EndsWith);
  if (!s || !s[0]) return false;
  int l = Length(s);
  if (l > length()) return false;
  return (NCmp(data+length()-l, s, l) == 0);
  unguard;
}


bool VStr::EndsWith (const VStr &s) const {
  guard(VStr::EndsWith);
  int l = s.length();
  if (l > length()) return false;
  return (NCmp(data+length()-l, *s, l) == 0);
  unguard;
}


VStr VStr::ToLower () const {
  guard(VStr::ToLower);
  if (!data) return VStr();
  bool hasWork = false;
  int l = length();
  for (int i = 0; i < l; ++i) if (data[i] >= 'A' && data[i] <= 'Z') { hasWork = true; break; }
  if (hasWork) {
    VStr res(*this);
    res.makeMutable();
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
  int l = length();
  for (int i = 0; i < l; ++i) if (data[i] >= 'a' && data[i] <= 'z') { hasWork = true; break; }
  if (hasWork) {
    VStr res(*this);
    res.makeMutable();
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
    const char *pos = (const char *)memchr(data, c, length());
    return (pos ? (int)(pos-data) : -1);
  } else {
    return -1;
  }
  unguard;
}


int VStr::IndexOf (const char *s) const {
  guard(VStr::IndexOf);
  if (!s || !s[0]) return -1;
  int sl = int(Length(s));
  int l = int(length());
  for (int i = 0; i <= l-sl; ++i) if (NCmp(data+i, s, sl) == 0) return i;
  return -1;
  unguard;
}


int VStr::IndexOf (const VStr &s) const {
  guard(VStr::IndexOf);
  int sl = int(s.length());
  if (!sl) return -1;
  int l = int(length());
  for (int i = 0; i <= l-sl; ++i) if (NCmp(data+i, *s, sl) == 0) return i;
  return -1;
  unguard;
}


int VStr::LastIndexOf (char c) const {
  guard(VStr::LastIndexOf);
  if (data) {
#if !defined(WIN32) && !defined(NO_MEMRCHR)
    const char *pos = (const char *)memrchr(data, c, length());
    return (pos ? (int)(pos-data) : -1);
#else
    size_t pos = length();
    while (pos > 0 && data[pos-1] != c) --pos;
    return (pos ? (int)(pos-1) : -1);
#endif
  } else {
    return -1;
  }
  unguard;
}


int VStr::LastIndexOf (const char *s) const {
  guard(VStr::LastIndexOf);
  if (!s || !s[0]) return -1;
  int sl = int(Length(s));
  int l = int(length());
  for (int i = l-sl; i >= 0; --i) if (NCmp(data+i, s, sl) == 0) return i;
  return -1;
  unguard;
}


int VStr::LastIndexOf (const VStr &s) const {
  guard(VStr::LastIndexOf);
  int sl = int(s.length());
  if (!sl) return -1;
  int l = int(length());
  for (int i = l-sl; i >= 0; --i) if (NCmp(data + i, *s, sl) == 0) return i;
  return -1;
  unguard;
}


VStr VStr::Replace (const char *Search, const char *Replacement) const {
  guard(VStr::Replace);
  if (length() == 0) return VStr(); // nothing to replace in an empty string

  size_t SLen = Length(Search);
  size_t RLen = Length(Replacement);
  if (!SLen) return VStr(*this); // nothing to search for

  VStr res = VStr(*this);
  size_t i = 0;
  while (i <= res.length()-SLen) {
    if (NCmp(res.data+i, Search, SLen) == 0) {
      // if search and replace strings are of the same size,
      // we can just copy the data and avoid memory allocations
      if (SLen == RLen) {
        res.makeMutable();
        memcpy(res.data+i, Replacement, RLen);
      } else {
        //FIXME: optimize this!
        res = VStr(res, 0, int(i))+Replacement+VStr(res, int(i+SLen), int(res.length()-i-SLen));
      }
      i += RLen;
    } else {
      ++i;
    }
  }

  return res;
  unguard;
}


VStr VStr::Replace (const VStr &Search, const VStr &Replacement) const {
  guard(VStr::Replace);
  if (length() == 0) return VStr(); // nothing to replace in an empty string

  size_t SLen = Search.length();
  size_t RLen = Replacement.length();
  if (!SLen) return VStr(*this); // nothing to search for

  VStr res(*this);
  size_t i = 0;
  while (i <= res.length()-SLen) {
    if (NCmp(res.data+i, *Search, SLen) == 0) {
      // if search and replace strings are of the same size,
      // we can just copy the data and avoid memory allocations
      if (SLen == RLen) {
        res.makeMutable();
        memcpy(res.data+i, *Replacement, RLen);
      } else {
        //FIXME: optimize this!
        res = VStr(res, 0, int(i))+Replacement+VStr(res, int(i+SLen), int(res.length()-i-SLen));
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


void VStr::Split (char c, TArray<VStr> &A) const {
  guard(VStr::Split);
  A.Clear();
  if (!data) return;
  int start = 0;
  int len = int(length());
  for (int i = 0; i <= len; ++i) {
    if (i == len || data[i] == c) {
      if (start != i) A.Append(VStr(*this, start, i-start));
      start = i+1;
    }
  }
  unguard;
}


void VStr::Split (const char *chars, TArray<VStr> &A) const {
  guard(VStr::Split);
  A.Clear();
  if (!data) return;
  int start = 0;
  int len = int(length());
  for (int i = 0; i <= len; ++i) {
    bool DoSplit = (i == len);
    for (const char *pChar = chars; !DoSplit && *pChar; ++pChar) DoSplit = (data[i] == *pChar);
    if (DoSplit) {
      if (start != i) A.Append(VStr(*this, start, i-start));
      start = i+1;
    }
  }
  unguard;
}


void VStr::SplitOnBlanks (TArray<VStr> &A, bool doQuotedStrings) const {
  guard(VStr::SplitOnBlanks);
  A.Clear();
  if (!data) return;
  int len = int(length());
  int pos = 0;
  while (pos < len) {
    vuint8 ch = (vuint8)data[pos++];
    if (ch <= ' ') continue;
    int start = pos-1;
    if (doQuotedStrings && (ch == '\'' || ch == '"')) {
      vuint8 ech = ch;
      while (pos < len) {
        ch = (vuint8)data[pos++];
        if (ch == ech) break;
        if (ch == '\\') { if (pos < len) ++pos; }
      }
    } else {
      while (pos < len && (vuint8)data[pos] > ' ') ++pos;
    }
    A.append(VStr(*this, start, pos-start));
  }
  unguard;
}


// split string to path components; first component can be '/', others has no slashes
void VStr::SplitPath (TArray<VStr>& arr) const {
  guard(VStr::SplitPath);

  arr.Clear();
  if (!data) return;

  int pos = 0;
  int len = length();

#if !defined(_WIN32)
  if (data[0] == '/') arr.Append(VStr("/"));
  while (pos < len) {
    if (data[pos] == '/') { ++pos; continue; }
    int epos = pos+1;
    while (epos < len && data[epos] != '/') ++epos;
    arr.Append(mid(pos, epos-pos));
    pos = epos+1;
  }
#else
  if (data[0] == '/' || data[0] == '\\') {
    arr.Append(VStr("/"));
  } else if (data[1] == ':' && (data[2] == '/' || data[2] == '\\')) {
    arr.Append(mid(0, 3));
    pos = 3;
  }
  while (pos < len) {
    if (data[pos] == '/' || data[pos] == '\\') { ++pos; continue; }
    int epos = pos+1;
    while (epos < len && data[epos] != '/' && data[epos] != '\\') ++epos;
    arr.Append(mid(pos, epos-pos));
    pos = epos+1;
  }
#endif

  unguard;
}


bool VStr::IsValidUtf8 () const {
  guard(VStr::IsValidUtf8);
  if (!data) return true;
  for (const char *c = data; *c;) {
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
  for (int i = 0; i < length(); ++i) res += FromChar((vuint8)data[i]);
  return res;
  unguard;
}


VStr VStr::EvalEscapeSequences () const {
  guard(VStr::EvalEscapeSequences);
  VStr res;
  if (!data || !data[0]) return res;
  char val;
  for (const char *c = data; *c; ++c) {
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


bool VStr::MustBeSanitized () const {
  int len = (int)length();
  if (len < 1) return false;
  for (const vuint8 *s = (const vuint8 *)data; *s; ++s) {
    if (*s < ' ' || *s == 127) return true;
  }
  return false;
}


bool VStr::MustBeSanitized (const char *str) {
  if (!str) return false;
  for (const vuint8 *s = (const vuint8 *)str; *s; ++s) {
    if (*s != '\n' && *s != '\t') {
      if (*s < ' ' || *s == 127) return true;
    }
  }
  return false;
}


VStr VStr::RemoveColours () const {
  guard(VStr::RemoveColours);
  if (!data) return VStr();
  const int oldlen = (int)length();
  // calculate new length
  int newlen = 0;
  int pos = 0;
  while (pos < oldlen) {
    char c = data[pos++];
    if (c == TEXT_COLOUR_ESCAPE) {
      if (data[pos] == '[') {
        ++pos;
        while (pos < oldlen && data[pos] != ']') ++pos;
        ++pos;
      }
    } else {
      if (!c) break;
      if (c != '\n' && c != '\t') {
        if ((vuint8)c < ' ' || c == 127) continue;
      }
      ++newlen;
    }
  }
  if (newlen == oldlen) return VStr(*this);
  // build new string
  VStr res;
  res.resize(newlen);
  pos = 0;
  newlen = 0;
  while (pos < oldlen) {
    char c = data[pos++];
    if (c == TEXT_COLOUR_ESCAPE) {
      if (data[pos] == '[') {
        ++pos;
        while (pos < oldlen && data[pos] != ']') ++pos;
        ++pos;
      }
    } else {
      if (!c) break;
      if (newlen >= res.length()) break; // oops
      if (c != '\n' && c != '\t') {
        if ((vuint8)c < ' ' || c == 127) continue;
      }
      res.data[newlen++] = c;
    }
  }
  return res;
  unguard;
}


VStr VStr::ExtractFilePath () const {
  guard(FL_ExtractFilePath);
  const char *src = data+length();
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
  const char *src = data+length();
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
  int i = int(length());

  if (i == 0) return VStr();

#if !defined(_WIN32)
  // back up until a \ or the start
  while (i && data[i-1] != '/') --i;
#else
  while (i && data[i-1] != '/' && data[i-1] != '\\' && data[i-1] != ':') --i;
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


VStr VStr::ExtractFileBaseName () const {
  guard(VStr::ExtractFileBaseName);
  int i = int(length());

  if (i == 0) return VStr();

#if !defined(_WIN32)
  // back up until a \ or the start
  while (i && data[i-1] != '/') --i;
#else
  while (i && data[i-1] != '/' && data[i-1] != '\\' && data[i-1] != ':') --i;
#endif

  return VStr(*this, i, length()-i);
  unguard;
}


VStr VStr::ExtractFileExtension () const {
  guard(VStr::ExtractFileExtension);
  const char *src = data+length();
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
  const char *src = data+length();
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


VStr VStr::DefaultPath (const VStr &basepath) const {
  guard(VStr::DefaultPath);
#if !defined(_WIN32)
  if (data && data[0] == '/') return *this; // absolute path location
#else
  if (data && data[0] == '/') return *this; // absolute path location
  if (data && data[0] == '\\') return *this; // absolute path location
  if (data && data[1] == ':' && (data[2] == '/' || data[2] == '\\')) return *this; // absolute path location
#endif
  return basepath+(*this);
  unguard;
}


// if path doesn't have a .EXT, append extension (extension should include the leading dot)
VStr VStr::DefaultExtension (const VStr &extension) const {
  guard(VStr::DefaultExtension);
  const char *src = data+length();
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
  for (const char *c = data; *c; ++c) if (*c == '\\') { hasWork = true; break; }
  if (hasWork) {
    VStr res(*this);
    res.makeMutable();
    for (char *c = res.data; *c; ++c) if (*c == '\\') *c = '/';
    return res;
  } else {
    return VStr(*this);
  }
  unguard;
}


int VStr::Utf8Length (const char *s, int len) {
  guard(VStr::Utf8Length);
  if (len < 0) len = (s && s[0] ? (int)strlen(s) : 0);
  int count = 0;
  while (len-- > 0) if (((*s++)&0xc0) != 0x80) ++count;
  return count;
  unguard;
}


size_t VStr::ByteLengthForUtf8 (const char *s, size_t N) {
  guard(VStr::ByteLengthForUtf8);
  if (s) {
    size_t count = 0;
    const char *c;
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


int VStr::GetChar (const char *&s) {
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


VStr VStr::quote () const {
  int len = length();
  char hexb[6];
  for (int f = 0; f < len; ++f) {
    vuint8 ch = (vuint8)data[f];
    if (ch < ' ' || ch == '\\' || ch == '\'' || ch == '"' || ch >= 127) {
      // need to quote it
      VStr res;
      for (int c = 0; c < len; ++c) {
        ch = (vuint8)data[c];
        if (ch < ' ') {
          switch (ch) {
            case '\t': res += "\\t"; break;
            case '\n': res += "\\n"; break;
            default:
              snprintf(hexb, sizeof(hexb), "\\x%02x", ch);
              res += hexb;
              break;
          }
        } else if (ch == '\\' || ch == '\'' || ch == '"') {
          res += "\\";
          res += (char)ch;
        } else if (ch >= 127) {
          snprintf(hexb, sizeof(hexb), "\\x%02x", ch);
          res += hexb;
        } else {
          res += (char)ch;
        }
      }
      return res;
    }
  }
  return VStr(*this);
}


//==========================================================================
//
//  VStr::buf2hex
//
//==========================================================================
VStr VStr::buf2hex (const void *buf, int buflen) {
  static const char *hexd = "0123456789abcdef";
  VStr res;
  if (buflen < 0 || !buf) return res;
  const vuint8 *b = (const vuint8 *)buf;
  buflen *= 2;
  res.resize(buflen);
  char *str = res.data;
  for (int f = 0; f < buflen; f += 2, ++b) {
    *str++ = hexd[((*b)>>4)&0x0f];
    *str++ = hexd[(*b)&0x0f];
  }
  return res;
}


//==========================================================================
//
//  VStr::convertInt
//
//==========================================================================
bool VStr::convertInt (const char *s, int *outv) {
  bool neg = false;
  int dummy = 0;
  if (!outv) outv = &dummy;
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


//==========================================================================
//
//  VStr::convertFloat
//
//==========================================================================
bool VStr::convertFloat (const char *s, float *outv) {
  float dummy = 0;
  if (!outv) outv = &dummy;
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


//==========================================================================
//
// va
//
// Very useful function from Quake.
// Does a varargs printf into a temp buffer, so I don't need to have
// varargs versions of all text functions.
//
//==========================================================================
__attribute__((format(printf, 1, 2))) char *va (const char *text, ...) {
  static char va_buffer[32][32768];
  static int va_bufnum = 0;
  va_list args;
  va_bufnum = (va_bufnum+1)&31;
  va_start(args, text);
  vsnprintf(va_buffer[va_bufnum], 32767, text, args);
  va_end(args);
  return va_buffer[va_bufnum];
}
