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


// ////////////////////////////////////////////////////////////////////////// //
#define TEXT_COLOUR_ESCAPE    '\034'


// ////////////////////////////////////////////////////////////////////////// //
// WARNING! this cannot be bigger than one pointer, or VM will break!
class VStr {
private:
  struct Store {
    int length;
    int alloted;
    int rc; // has no meaning for slices
    // actual string data starts after this struct; and this is where `data` points
  };

  char *data; // string, 0-terminated (0 is not in length); can be null

private:
  inline Store *store () { return (data ? (Store *)(data-sizeof(Store)) : nullptr); }
  inline Store *store () const { return (data ? (Store *)(data-sizeof(Store)) : nullptr); }

  inline void incref () const {
    if (data) ++store()->rc;
  }

  // WARNING! may free `data` contents!
  // this also clears `data`
  inline void decref () {
    if (data) {
      if (--store()->rc == 0) {
        #ifdef VAVOOM_TEST_VSTR
        fprintf(stderr, "VStr: freeing %p\n", data);
        #endif
        Z_Free(store());
      }
      data = nullptr;
    }
  }

  inline bool isMyData (const char *buf, int len) const { return (data && buf && buf < data+length() && buf+len >= data); }

  inline void assign (const VStr &instr) {
    if (&instr != this) {
      if (instr.data) {
        if (instr.data != data) {
          instr.incref();
          decref();
          data = (char *)instr.data;
        }
      } else {
        clear();
      }
    }
  }

  void makeMutable (); // and unique
  void resize (int newlen); // always makes string unique; also, always sets [length] to 0; clears string on `newlen == 0`
  void setContent (const char *s, int len=-1);

public:
  VStr (ENoInit) {}
  VStr () : data(nullptr) {}
  VStr (const VStr &instr) : data(nullptr) { data = instr.data; incref(); }
  VStr (const char *instr, int len=-1) : data(nullptr) { setContent(instr, len); }
  VStr (const VStr &instr, int start, int len) : data(nullptr) { assign(instr.mid(start, len)); }

  explicit VStr (const VName &InName) : data(nullptr) { setContent(*InName); }

  explicit VStr (char v) : data(nullptr) { setContent(&v, 1); }
  explicit VStr (bool v) : data(nullptr) { setContent(v ? "true" : "false"); }
  explicit VStr (int v);
  explicit VStr (unsigned v);
  explicit VStr (float v);
  explicit VStr (double v);

  ~VStr () { clear(); }

  // clears the string
  inline void Clean () { decref(); }
  inline void Clear () { decref(); }
  inline void clear () { decref(); }

  // returns length of the string
  inline int Length () const { return (data ? store()->length : 0); }
  inline int length () const { return (data ? store()->length : 0); }

  inline void setLength (int len, char fillChar=' ') {
    if (len < 0) len = 0;
    resize(len);
    if (len > 0) memset(data, fillChar&0xff, len);
  }
  inline void SetLength (int len, char fillChar=' ') { setLength(len, fillChar); }

  inline int getRC () const { return (data ? store()->rc : 0); }
  inline int getReserved () const { return (data ? store()->alloted : 0); }

  // returns number of characters in a UTF-8 string
  inline int Utf8Length () const { return (data ? Utf8Length(data, store()->length) : 0); }
  inline int utf8Length () const { return (data ? Utf8Length(data, store()->length) : 0); }
  inline int utf8length () const { return (data ? Utf8Length(data, store()->length) : 0); }

  // returns C string
  inline const char *operator * () const { return (data ? data : ""); }
  inline const char *getCStr () const { return (data ? data : ""); }

  //inline bool isUnuqie () const { return (!data || *refp() == 1); }

  // checks if string is empty
  inline bool IsEmpty () const { return !data; }
  inline bool isEmpty () const { return !data; }
  inline bool IsNotEmpty () const { return !!data; }
  inline bool isNotEmpty () const { return !!data; }

  // character accessors
  inline char operator [] (int idx) const { return data[idx]; }
  inline char *GetMutableCharPointer (int idx) { makeMutable(); return &data[idx]; }

  VStr mid (int start, int len) const;
  VStr left (int len) const;
  VStr right (int len) const;
  void chopLeft (int len);
  void chopRight (int len);

  // assignement operators
  inline VStr &operator = (const char *instr) { setContent(instr); return *this; }
  inline VStr &operator = (const VStr &instr) { assign(instr); return *this; }

  VStr &appendCStr (const char *instr, int len=-1) {
    if (len < 0) len = (int)(instr && instr[0] ? strlen(instr) : 0);
    if (len) {
      if (isMyData(instr, len)) {
        VStr s(instr, len);
        operator+=(s);
      } else {
        int l = length();
        resize(l+len);
        memcpy(data+l, instr, len+1);
      }
    }
    return *this;
  }

  // concatenation operators
  inline VStr &operator += (const char *instr) { return appendCStr(instr, -1); }

  VStr &operator += (const VStr &instr) {
    int inl = instr.length();
    if (inl) {
      int l = length();
      if (l) {
        VStr s(instr); // this is cheap
        resize(l+inl);
        memcpy(data+l, s.data, inl+1);
      } else {
        assign(instr);
      }
    }
    return *this;
  }

  VStr &operator += (char inchr) {
    int l = length();
    resize(l+1);
    data[l] = inchr;
    return *this;
  }

  inline VStr &operator += (bool v) { return operator+=(v ? "true" : "false"); }
  inline VStr &operator += (int v) { char buf[64]; snprintf(buf, sizeof(buf), "%d", v); return operator+=(buf); }
  inline VStr &operator += (unsigned v) { char buf[64]; snprintf(buf, sizeof(buf), "%u", v); return operator+=(buf); }
  inline VStr &operator += (float v) { char buf[64]; snprintf(buf, sizeof(buf), "%f", v); return operator+=(buf); }
  inline VStr &operator += (double v) { char buf[64]; snprintf(buf, sizeof(buf), "%f", v); return operator+=(buf); }
  inline VStr &operator += (const VName &v) { return operator+=(*v); }

  friend VStr operator + (const VStr &S1, const char *S2) { VStr res(S1); res += S2; return res; }
  friend VStr operator + (const VStr &S1, const VStr &S2) { VStr res(S1); res += S2; return res; }
  friend VStr operator + (const VStr &S1, char S2) { VStr res(S1); res += S2; return res; }
  friend VStr operator + (const VStr &S1, bool v) { VStr res(S1); res += v; return res; }
  friend VStr operator + (const VStr &S1, int v) { VStr res(S1); res += v; return res; }
  friend VStr operator + (const VStr &S1, unsigned v) { VStr res(S1); res += v; return res; }
  friend VStr operator + (const VStr &S1, float v) { VStr res(S1); res += v; return res; }
  friend VStr operator + (const VStr &S1, double v) { VStr res(S1); res += v; return res; }
  friend VStr operator + (const VStr &S1, const VName &v) { VStr res(S1); res += v; return res; }

  // comparison operators
  friend bool operator == (const VStr &S1, const char *S2) { return (Cmp(*S1, S2) == 0); }
  friend bool operator == (const VStr &S1, const VStr &S2) { return (S1.data == S2.data ? true : (Cmp(*S1, *S2) == 0)); }
  friend bool operator != (const VStr &S1, const char *S2) { return (Cmp(*S1, S2) != 0); }
  friend bool operator != (const VStr &S1, const VStr &S2) { return (S1.data == S2.data ? false : (Cmp(*S1, *S2) != 0)); }
  friend bool operator < (const VStr &S1, const char *S2) { return (Cmp(*S1, S2) < 0); }
  friend bool operator < (const VStr &S1, const VStr &S2) { return (S1.data == S2.data ? false : (Cmp(*S1, *S2) < 0)); }
  friend bool operator > (const VStr &S1, const char *S2) { return (Cmp(*S1, S2) > 0); }
  friend bool operator > (const VStr &S1, const VStr &S2) { return (S1.data == S2.data ? false : (Cmp(*S1, *S2) > 0)); }
  friend bool operator <= (const VStr &S1, const char *S2) { return (Cmp(*S1, S2) <= 0); }
  friend bool operator <= (const VStr &S1, const VStr &S2) { return (S1.data == S2.data ? true : (Cmp(*S1, *S2) <= 0)); }
  friend bool operator >= (const VStr &S1, const char *S2) { return (Cmp(*S1, S2) >= 0); }
  friend bool operator >= (const VStr &S1, const VStr &S2) { return (S1.data == S2.data ? true : (Cmp(*S1, *S2) >= 0)); }

  // comparison functions
  inline int Cmp (const char *S2) const { return Cmp(data, S2); }
  inline int Cmp (const VStr &S2) const { return Cmp(data, *S2); }
  inline int ICmp (const char *S2) const { return ICmp(data, S2); }
  inline int ICmp (const VStr &S2) const { return ICmp(data, *S2); }

  inline int cmp (const char *S2) const { return Cmp(data, S2); }
  inline int cmp (const VStr &S2) const { return Cmp(data, *S2); }
  inline int icmp (const char *S2) const { return ICmp(data, S2); }
  inline int icmp (const VStr &S2) const { return ICmp(data, *S2); }

  bool StartsWith (const char *) const;
  bool StartsWith (const VStr &) const;
  bool EndsWith (const char *) const;
  bool EndsWith (const VStr &) const;

  inline bool startsWith (const char *s) const { return StartsWith(s); }
  inline bool startsWith (const VStr &s) const { return StartsWith(s); }
  inline bool endsWith (const char *s) const { return EndsWith(s); }
  inline bool endsWith (const VStr &s) const { return EndsWith(s); }

  VStr ToLower () const;
  VStr ToUpper () const;

  inline VStr toLowerCase () const { return ToLower(); }
  inline VStr toUpperCase () const { return ToUpper(); }

  int IndexOf (char) const;
  int IndexOf (const char *) const;
  int IndexOf (const VStr &) const;
  int LastIndexOf (char) const;
  int LastIndexOf (const char *) const;
  int LastIndexOf (const VStr &) const;

  inline int indexOf (char v) const { return IndexOf(v); }
  inline int indexOf (const char *v) const { return IndexOf(v); }
  inline int indexOf (const VStr &v) const { return IndexOf(v); }
  inline int lastIndexOf (char v) const { return LastIndexOf(v); }
  inline int lastIndexOf (const char *v) const { return LastIndexOf(v); }
  inline int lastIndexOf (const VStr &v) const { return LastIndexOf(v); }

  VStr Replace (const char *, const char *) const;
  VStr Replace (const VStr &, const VStr &) const;

  inline VStr replace (const char *s0, const char *s1) const { return Replace(s0, s1); }
  inline VStr replace (const VStr &s0, const VStr &s1) const { return Replace(s0, s1); }

  VStr Utf8Substring (int start, int len) const;
  inline VStr utf8Substring (int start, int len) const { return Utf8Substring(start, len); }
  inline VStr utf8substring (int start, int len) const { return Utf8Substring(start, len); }

  void Split (char, TArray<VStr> &) const;
  void Split (const char *, TArray<VStr> &) const;

  inline void split (char c, TArray<VStr> &a) const { Split(c, a); }
  inline void split (const char *s, TArray<VStr> &a) const { Split(s, a); }

  // split string to path components; first component can be '/', others has no slashes
  void SplitPath (TArray<VStr> &) const;
  inline void splitPath (TArray<VStr> &a) const { SplitPath(a); }

  bool IsValidUtf8 () const;
  inline bool isValidUtf8 () const { return IsValidUtf8(); }
  bool isUtf8Valid () const;

  VStr Latin1ToUtf8 () const;

  // serialisation operator
  friend VStream &operator << (VStream &Strm, VStr &S) {
    if (Strm.IsLoading()) {
      vint32 len;
      Strm << STRM_INDEX(len);
      if (len < 0) len = 0;
      S.resize(len);
      if (len) {
        Strm.Serialise(S.data, len+1);
        S.data[len] = 0; // just in case
      }
    } else {
      vint32 len = vint32(S.Length());
      Strm << STRM_INDEX(len);
      if (len) Strm.Serialise(S.data, len+1);
    }
    return Strm;
  }

  VStr quote () const;

  VStr EvalEscapeSequences () const;

  VStr RemoveColours () const;

  VStr ExtractFilePath () const;
  VStr ExtractFileName () const;
  VStr ExtractFileBase () const;
  VStr ExtractFileBaseName () const;
  VStr ExtractFileExtension () const;
  VStr StripExtension () const;
  VStr DefaultPath (const VStr &basepath) const;
  VStr DefaultExtension (const VStr &extension) const;
  VStr FixFileSlashes () const;

  inline VStr extractFilePath () const { return ExtractFilePath(); }
  inline VStr extractFileName () const { return ExtractFileName(); }
  inline VStr extractFileBase () const { return ExtractFileBase(); }
  inline VStr extractFileBaseName () const { return ExtractFileBaseName(); }
  inline VStr extractFileExtension () const { return ExtractFileExtension(); }
  inline VStr stripExtension () const { return StripExtension(); }
  inline VStr defaultPath (const VStr &basepath) const { return DefaultPath(basepath); }
  inline VStr defaultExtension (const VStr &extension) const { return DefaultExtension(extension); }
  inline VStr fixSlashes () const { return FixFileSlashes(); }

  static inline int Length (const char *s) { return (s ? (int)strlen(s) : 0); }
  static inline int length (const char *s) { return (s ? (int)strlen(s) : 0); }
  static int Utf8Length (const char *s, int len=-1);
  static inline int utf8Length (const char *s, int len=-1) { return (int)Utf8Length(s, len); }
  static size_t ByteLengthForUtf8 (const char *, size_t);
  static int GetChar (const char *&); // utf8
  static VStr FromChar (int);

  static inline int Cmp (const char *S1, const char *S2) { return (S1 == S2 ? 0 : strcmp((S1 ? S1 : ""), (S2 ? S2 : ""))); }
  static inline int NCmp (const char *S1, const char *S2, size_t N) { return (S1 == S2 ? 0 : strncmp((S1 ? S1 : ""), (S2 ? S2 : ""), N)); }

  static inline int ICmp (const char *s0, const char *s1) {
    if (!s0) s0 = "";
    if (!s1) s1 = "";
    while (*s0 && *s1) {
      vuint8 c0 = (vuint8)(*s0++);
      vuint8 c1 = (vuint8)(*s1++);
      if (c0 >= 'A' && c0 <= 'Z') c0 += 32;
      if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
      if (c0 < c1) return -1;
      if (c0 > c1) return 1;
    }
    if (*s0) return 1;
    if (*s1) return -1;
    return 0;
  }

  static inline int NICmp (const char *s0, const char *s1, size_t max) {
    if (max == 0) return 0;
    if (!s0) s0 = "";
    if (!s1) s1 = "";
    while (*s0 && *s1) {
      vuint8 c0 = (vuint8)(*s0++);
      vuint8 c1 = (vuint8)(*s1++);
      if (c0 >= 'A' && c0 <= 'Z') c0 += 32;
      if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
      if (c0 < c1) return -1;
      if (c0 > c1) return 1;
      if (--max == 0) return 0;
    }
    if (*s0) return 1;
    if (*s1) return -1;
    return 0;
  }

  static inline void Cpy (char *dst, const char *src) {
    if (dst) { if (src) strcpy(dst, src); else *dst = 0; }
  }

  // will write terminating zero
  static inline void NCpy (char *dst, const char *src, size_t N) {
    if (dst && src && N && src[0]) {
      size_t slen = strlen(src);
      if (slen > N) slen = N;
      memcpy(dst, src, slen);
      dst[slen] = 0;
    } else {
      if (dst) *dst = 0;
    }
  }

  static inline char ToUpper (char c) { return (c >= 'a' && c <= 'z' ? c-32 : c); }
  static inline char ToLower (char c) { return (c >= 'A' && c <= 'Z' ? c+32 : c); }

  static inline char toupper (char c) { return (c >= 'a' && c <= 'z' ? c-32 : c); }
  static inline char tolower (char c) { return (c >= 'A' && c <= 'Z' ? c+32 : c); }

  // append codepoint to this string, in utf-8
  VStr &utf8Append (vuint32 code);

  VStr utf2win () const;
  VStr win2utf () const;

  VStr toLowerCase1251 () const;
  VStr toUpperCase1251 () const;

  inline bool equ1251CI (const VStr &s) const {
    size_t slen = (size_t)length();
    if (slen != (size_t)s.length()) return false;
    for (size_t f = 0; f < slen; ++f) if (locase1251(data[f]) != locase1251(s.data[f])) return false;
    return true;
  }

  inline bool equ1251CI (const char *s) const {
    size_t slen = length();
    if (!s || !s[0]) return (slen == 0);
    if (slen != strlen(s)) return false;
    for (size_t f = 0; f < slen; ++f) if (locase1251(data[f]) != locase1251(s[f])) return false;
    return true;
  }

  inline bool fnameEqu1251CI (const VStr &s) const { return fnameEqu1251CI(s.data); }
  bool fnameEqu1251CI (const char *s) const;

  static VStr buf2hex (const void *buf, int buflen);

  static bool convertInt (const char *s, int *outv);
  static bool convertFloat (const char *s, float *outv);

  inline bool convertInt (int *outv) const { return convertInt(getCStr(), outv); }
  inline bool convertFloat (float *outv) const { return convertFloat(getCStr(), outv); }

public:
  static inline char wchar2win (vuint32 wc) { return (wc < 65536 ? wc2shitmap[wc] : '?'); }

  static inline int digitInBase (char ch, int base=10) {
    if (base < 1 || base > 36 || ch < '0') return -1;
    if (base <= 10) return (ch < 48+base ? ch-48 : -1);
    if (ch >= '0' && ch <= '9') return ch-48;
    if (ch >= 'a' && ch <= 'z') ch -= 32; // poor man tolower()
    if (ch < 'A' || ch >= 65+(base-10)) return -1;
    return ch-65+10;
  }

  static inline char upcase1251 (char ch) {
    if ((vuint8)ch < 128) return ch-(ch >= 'a' && ch <= 'z' ? 32 : 0);
    if ((vuint8)ch >= 224 && (vuint8)ch <= 255) return (vuint8)ch-32;
    if ((vuint8)ch == 184 || (vuint8)ch == 186 || (vuint8)ch == 191) return (vuint8)ch-16;
    if ((vuint8)ch == 162 || (vuint8)ch == 179) return (vuint8)ch-1;
    return ch;
  }

  static inline char locase1251 (char ch) {
    if ((vuint8)ch < 128) return ch+(ch >= 'A' && ch <= 'Z' ? 32 : 0);
    if ((vuint8)ch >= 192 && (vuint8)ch <= 223) return (vuint8)ch+32;
    if ((vuint8)ch == 168 || (vuint8)ch == 170 || (vuint8)ch == 175) return (vuint8)ch+16;
    if ((vuint8)ch == 161 || (vuint8)ch == 178) return (vuint8)ch+1;
    return ch;
  }

  // returns length of the following utf-8 sequence from its first char, or -1 for invalid first char
  static inline int utf8CodeLen (char ch) {
    if ((vuint8)ch < 0x80) return 1;
    if ((ch&0xFE) == 0xFC) return 6;
    if ((ch&0xFC) == 0xF8) return 5;
    if ((ch&0xF8) == 0xF0) return 4;
    if ((ch&0xF0) == 0xE0) return 3;
    if ((ch&0xE0) == 0xC0) return 2;
    return -1; // invalid
  }

public:
  static const vuint16 cp1251[128];
  static char wc2shitmap[65536];

  static const VStr EmptyString;

  static void vstrInitr_fuck_you_gnu_binutils_fuck_you_fuck_you_fuck_you ();
};


extern char *va (const char *text, ...) __attribute__((format(printf, 1, 2)));

inline vuint32 GetTypeHash (const char *s) { return (s && s[0] ? fnvHashBuf(s, strlen(s)) : 1); }
inline vuint32 GetTypeHash (const VStr &s) { return (s.length() ? fnvHashBuf(*s, s.length()) : 1); }


// ////////////////////////////////////////////////////////////////////////// //
struct VUtf8DecoderFast {
public:
  enum {
    Replacement = 0xFFFD, // replacement char for invalid unicode
    Accept = 0,
    Reject = 12,
  };

private:
  vuint32 state;

public:
  vuint32 codepoint; // decoded codepoint (valid only when decoder is in "complete" state)

public:
  VUtf8DecoderFast () : state(Accept), codepoint(0) {}

  inline void reset () { state = Accept; codepoint = 0; }

  // is current character valid and complete? take `codepoint` then
  inline bool complete () const { return (state == Accept); }
  // is current character invalid and complete? take `Replacement` then
  inline bool invalid () const { return (state == Reject); }
  // is current character complete (valid or invaluid)? take `codepoint` then
  inline bool hasCodePoint () const { return (state == Accept || state == Reject); }

  // process another input byte; returns `true` if codepoint is complete
  inline bool put (vuint8 c) {
    if (state == Reject) { state = Accept; codepoint = 0; } // restart from invalid state
    vuint8 tp = utf8dfa[c];
    codepoint = (state != Accept ? (c&0x3f)|(codepoint<<6) : (0xff>>tp)&c);
    state = utf8dfa[256+state+tp];
    if (state == Reject) codepoint = Replacement;
    return (state == Accept || state == Reject);
  }

private:
  static const vuint8 utf8dfa[0x16c];
};

// required for VaVoom C VM
static_assert(sizeof(VStr) <= sizeof(void *), "oops");


// ////////////////////////////////////////////////////////////////////////// //
#include <string>
#include <cstdlib>
#include <cxxabi.h>

template<typename T> VStr shitppTypeName () {
  VStr tpn(typeid(T).name());
  char *dmn = abi::__cxa_demangle(*tpn, nullptr, nullptr, nullptr);
  if (dmn) {
    tpn = VStr(dmn);
    Z_Free(dmn);
  }
  return tpn;
}


template<class T> VStr shitppTypeNameObj (const T &o) {
  VStr tpn(typeid(o).name());
  char *dmn = abi::__cxa_demangle(*tpn, nullptr, nullptr, nullptr);
  if (dmn) {
    tpn = VStr(dmn);
    Z_Free(dmn);
  }
  return tpn;
}
