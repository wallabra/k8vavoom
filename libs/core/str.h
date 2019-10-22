//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
//**  Copyright (C) 2018-2019 Ketmar Dark
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
//
// dynamic string class
//
//**************************************************************************


// ////////////////////////////////////////////////////////////////////////// //
#define TEXT_COLOR_ESCAPE      '\034'
#define TEXT_COLOR_ESCAPE_STR  "\034"


extern char *va (const char *text, ...) noexcept __attribute__((format(printf, 1, 2))) __attribute__((warn_unused_result));
extern char *vavarg (const char *text, va_list ap) noexcept __attribute__((warn_unused_result));


// ////////////////////////////////////////////////////////////////////////// //
// WARNING! this cannot be bigger than one pointer, or VM will break!
// WARNING! this is NOT MT-SAFE! if you want to use it from multiple threads,
//          make sure to `cloneUnique()` it, and pass to each thread its own VStr!
class VStr {
public:
  struct __attribute__((packed)) Store {
    vint32 length;
    vint32 alloted;
    vint32 rc; // negative number means "immutable string"
    // actual string data starts after this struct; and this is where `data` points
    vint32 dummy; // this is to get natural align on 8-byte boundary
  };
  static_assert(sizeof(Store) == 8*2, "invalid size for `VStr::Store` struct");
  static_assert(__builtin_offsetof(Store, rc)%8 == 0, "invalid rc alignent for `VStr::Store` struct");

  char *dataptr; // string, 0-terminated (0 is not in length); can be null

protected:
  __attribute__((warn_unused_result)) inline Store *store () noexcept { return (dataptr ? (Store *)(dataptr-sizeof(Store)) : nullptr); }
  __attribute__((warn_unused_result)) inline Store *store () const noexcept { return (dataptr ? (Store *)(dataptr-sizeof(Store)) : nullptr); }

  // should be called only when storage is available
  __attribute__((warn_unused_result)) inline int atomicGetRC () const noexcept { return __atomic_load_n(&((const Store *)(dataptr-sizeof(Store)))->rc, __ATOMIC_SEQ_CST); }
  // should be called only when storage is available
  inline void atomicSetRC (int newval) noexcept { __atomic_store_n(&((Store *)(dataptr-sizeof(Store)))->rc, newval, __ATOMIC_SEQ_CST); }
  // should be called only when storage is available
  __attribute__((warn_unused_result)) inline bool atomicIsImmutable () const noexcept { return (__atomic_load_n(&((const Store *)(dataptr-sizeof(Store)))->rc, __ATOMIC_SEQ_CST) < 0); }
  // should be called only when storage is available
  // immutable strings aren't unique
  __attribute__((warn_unused_result)) inline bool atomicIsUnique () const noexcept { return (__atomic_load_n(&((const Store *)(dataptr-sizeof(Store)))->rc, __ATOMIC_SEQ_CST) == 1); }
  // should be called only when storage is available
  // returns new value
  // WARNING: will happily modify immutable RC!
  inline void atomicIncRC () const noexcept { (void)__atomic_add_fetch(&((Store *)(dataptr-sizeof(Store)))->rc, 1, __ATOMIC_SEQ_CST); }
  // should be called only when storage is available
  // returns new value
  // WARNING: will happily modify immutable RC!
  inline int atomicDecRC () const noexcept { return __atomic_sub_fetch(&((Store *)(dataptr-sizeof(Store)))->rc, 1, __ATOMIC_SEQ_CST); }

  __attribute__((warn_unused_result)) inline char *getData () noexcept { return dataptr; }
  __attribute__((warn_unused_result)) inline const char *getData () const noexcept { return dataptr; }

  inline void incref () const noexcept { if (dataptr && !atomicIsImmutable()) atomicIncRC(); }

  // WARNING! may free `data` contents!
  // this also clears `data`
  inline void decref () noexcept {
    if (dataptr) {
      if (atomicGetRC() > 0) {
        if (atomicDecRC() == 0) {
          #ifdef VAVOOM_TEST_VSTR
          fprintf(stderr, "VStr: freeing %p\n", dataptr);
          #endif
          Z_Free(store());
        }
      }
      dataptr = nullptr;
    }
  }

  __attribute__((warn_unused_result)) inline bool isMyData (const char *buf, int len) const noexcept { return (dataptr && buf && (uintptr_t)buf < (uintptr_t)dataptr+length() && (uintptr_t)buf+len >= (uintptr_t)dataptr); }

  inline void assign (const VStr &instr) noexcept {
    if (&instr != this) {
      if (instr.dataptr) {
        if (instr.dataptr != dataptr) {
          instr.incref();
          decref();
          dataptr = (char *)instr.dataptr;
        }
      } else {
        clear();
      }
    }
  }

  void makeMutable () noexcept; // and unique
  void resize (int newlen) noexcept; // always makes string unique; also, always sets [length] to 0; clears string on `newlen == 0`
  void setContent (const char *s, int len=-1) noexcept;

  void setNameContent (const VName InName) noexcept;

public:
  // debul
  inline int dbgGetRef () const noexcept { return (dataptr ? atomicGetRC() : 0); }

public:
  // some utilities
  static bool convertInt (const char *s, int *outv, bool loose=false) noexcept;
  static bool convertFloat (const char *s, float *outv, const float *defval=nullptr) noexcept;

  static float atof (const char *s) noexcept { float res = 0; convertFloat(s, &res, nullptr); return res; }
  static float atof (const char *s, float defval) noexcept { float res = 0; convertFloat(s, &res, &defval); return res; }

  static int atoiStrict (const char *s) noexcept { int res = 0; if (!convertInt(s, &res)) res = 0; return res; }
  static int atoiStrict (const char *s, int defval) noexcept { int res = 0; if (!convertInt(s, &res)) res = defval; return res; }

  static int atoi (const char *s) noexcept { int res = 0; convertInt(s, &res, true); return res; }

  enum { FloatBufSize = 16 };
  static int float2str (char *buf, float v) noexcept; // 0-terminated

  enum { DoubleBufSize = 26 };
  static int double2str (char *buf, double v) noexcept; // 0-terminated

public:
  VStr (ENoInit) noexcept {}
  VStr () noexcept : dataptr(nullptr) {}
  VStr (const VStr &instr) noexcept : dataptr(nullptr) { dataptr = instr.dataptr; incref(); }
  VStr (const char *instr, int len=-1) noexcept : dataptr(nullptr) { setContent(instr, len); }
  VStr (const VStr &instr, int start, int len) noexcept : dataptr(nullptr) { assign(instr.mid(start, len)); }

  explicit VStr (const VName InName) noexcept : dataptr(nullptr) { setNameContent(InName); }

  explicit VStr (char v) noexcept : dataptr(nullptr) { setContent(&v, 1); }
  explicit VStr (bool v) noexcept : dataptr(nullptr) { setContent(v ? "true" : "false"); }
  explicit VStr (int v) noexcept;
  explicit VStr (unsigned v) noexcept;
  explicit VStr (float v) noexcept;
  explicit VStr (double v) noexcept;

  ~VStr () noexcept { clear(); }

  // this will create an unique copy of the string, which (copy) can be used in other threads
  __attribute__((warn_unused_result)) inline VStr cloneUnique () const noexcept {
    if (!dataptr) return VStr();
    int len = length();
    vassert(len > 0);
    VStr res;
    res.setLength(len, 0); // fill with zeroes, why not?
    memcpy(res.dataptr, dataptr, len);
    return res;
  }

  void makeImmutable () noexcept;
  __attribute__((warn_unused_result)) VStr &makeImmutableRetSelf () noexcept;

  // clears the string
  inline void Clean () noexcept { decref(); }
  inline void Clear () noexcept { decref(); }
  inline void clear () noexcept { decref(); }

  // returns length of the string
  __attribute__((warn_unused_result)) inline int Length () const noexcept { return (dataptr ? store()->length : 0); }
  __attribute__((warn_unused_result)) inline int length () const noexcept { return (dataptr ? store()->length : 0); }

  inline void setLength (int len, char fillChar=' ') noexcept {
    if (len < 0) len = 0;
    resize(len);
    if (len > 0) memset(getData(), fillChar&0xff, len);
  }
  inline void SetLength (int len, char fillChar=' ') noexcept { setLength(len, fillChar); }

  __attribute__((warn_unused_result)) inline int getCapacity () const noexcept { return (dataptr ? store()->alloted : 0); }

  // returns number of characters in a UTF-8 string
  __attribute__((warn_unused_result)) inline int Utf8Length () const noexcept { return Utf8Length(getCStr(), length()); }
  __attribute__((warn_unused_result)) inline int utf8Length () const noexcept { return Utf8Length(getCStr(), length()); }
  __attribute__((warn_unused_result)) inline int utf8length () const noexcept { return Utf8Length(getCStr(), length()); }

  // returns C string
  // `*` can be used in some dummied-out macros
  /*__attribute__((warn_unused_result))*/ inline const char *operator * () const noexcept { return (dataptr ? getData() : ""); }
  __attribute__((warn_unused_result)) inline const char *getCStr () const noexcept { return (dataptr ? getData() : ""); }
  __attribute__((warn_unused_result)) inline char *getMutableCStr () noexcept { makeMutable(); return (dataptr ? getData() : nullptr); }

  // character accessors
  __attribute__((warn_unused_result)) inline char operator [] (int idx) const noexcept { return (dataptr && idx >= 0 && idx < length() ? getData()[idx] : 0); }
  __attribute__((warn_unused_result)) inline char *GetMutableCharPointer (int idx) noexcept { makeMutable(); return (dataptr ? &dataptr[idx] : nullptr); }

  // checks if string is empty
  __attribute__((warn_unused_result)) inline bool IsEmpty () const noexcept { return (length() == 0); }
  __attribute__((warn_unused_result)) inline bool isEmpty () const noexcept { return (length() == 0); }
  __attribute__((warn_unused_result)) inline bool IsNotEmpty () const noexcept { return (length() != 0); }
  __attribute__((warn_unused_result)) inline bool isNotEmpty () const noexcept { return (length() != 0); }

  __attribute__((warn_unused_result)) VStr mid (int start, int len) const noexcept;
  __attribute__((warn_unused_result)) VStr left (int len) const noexcept;
  __attribute__((warn_unused_result)) VStr right (int len) const noexcept;
  void chopLeft (int len) noexcept;
  void chopRight (int len) noexcept;

  // assignement operators
  inline VStr &operator = (const char *instr) noexcept { setContent(instr); return *this; }
  inline VStr &operator = (const VStr &instr) noexcept { assign(instr); return *this; }

  VStr &appendCStr (const char *instr, int len=-1) noexcept {
    if (len < 0) len = (int)(instr && instr[0] ? strlen(instr) : 0);
    if (len) {
      if (isMyData(instr, len)) {
        VStr s(instr, len);
        operator+=(s);
      } else {
        int l = length();
        resize(l+len);
        memcpy(dataptr+l, instr, len+1);
      }
    }
    return *this;
  }

  // concatenation operators
  inline VStr &operator += (const char *instr) noexcept { return appendCStr(instr, -1); }

  VStr &operator += (const VStr &instr) noexcept {
    int inl = instr.length();
    if (inl) {
      int l = length();
      if (l) {
        VStr s(instr); // this is cheap
        resize(l+inl);
        memcpy(dataptr+l, s.getData(), inl+1);
      } else {
        assign(instr);
      }
    }
    return *this;
  }

  inline VStr &operator += (char inchr) noexcept {
    int l = length();
    resize(l+1);
    dataptr[l] = inchr;
    return *this;
  }

  inline VStr &operator += (bool v) noexcept { return operator+=(v ? "true" : "false"); }
  inline VStr &operator += (int v) noexcept { char buf[64]; snprintf(buf, sizeof(buf), "%d", v); return operator+=(buf); }
  inline VStr &operator += (unsigned v) noexcept { char buf[64]; snprintf(buf, sizeof(buf), "%u", v); return operator+=(buf); }
  //inline VStr &operator += (float v) noexcept { char buf[64]; snprintf(buf, sizeof(buf), "%f", v); return operator+=(buf); }
  //inline VStr &operator += (double v) noexcept { char buf[64]; snprintf(buf, sizeof(buf), "%f", v); return operator+=(buf); }
  VStr &operator += (float v) noexcept;
  VStr &operator += (double v) noexcept;
  inline VStr &operator += (const VName &v) noexcept { return operator+=(*v); }

  friend __attribute__((warn_unused_result)) VStr operator + (const VStr &S1, const char *S2) noexcept { VStr res(S1); res += S2; return res; }
  friend __attribute__((warn_unused_result)) VStr operator + (const VStr &S1, const VStr &S2) noexcept { VStr res(S1); res += S2; return res; }
  friend __attribute__((warn_unused_result)) VStr operator + (const VStr &S1, char S2) noexcept { VStr res(S1); res += S2; return res; }
  friend __attribute__((warn_unused_result)) VStr operator + (const VStr &S1, bool v) noexcept { VStr res(S1); res += v; return res; }
  friend __attribute__((warn_unused_result)) VStr operator + (const VStr &S1, int v) noexcept { VStr res(S1); res += v; return res; }
  friend __attribute__((warn_unused_result)) VStr operator + (const VStr &S1, unsigned v) noexcept { VStr res(S1); res += v; return res; }
  friend __attribute__((warn_unused_result)) VStr operator + (const VStr &S1, float v) noexcept { VStr res(S1); res += v; return res; }
  friend __attribute__((warn_unused_result)) VStr operator + (const VStr &S1, double v) noexcept { VStr res(S1); res += v; return res; }
  friend __attribute__((warn_unused_result)) VStr operator + (const VStr &S1, const VName &v) noexcept { VStr res(S1); res += v; return res; }

  // comparison operators
  friend __attribute__((warn_unused_result)) bool operator == (const VStr &S1, const char *S2) noexcept { return (Cmp(*S1, S2) == 0); }
  friend __attribute__((warn_unused_result)) bool operator == (const VStr &S1, const VStr &S2) noexcept { return (S1.getData() == S2.getData() ? true : (Cmp(*S1, *S2) == 0)); }
  friend __attribute__((warn_unused_result)) bool operator != (const VStr &S1, const char *S2) noexcept { return (Cmp(*S1, S2) != 0); }
  friend __attribute__((warn_unused_result)) bool operator != (const VStr &S1, const VStr &S2) noexcept { return (S1.getData() == S2.getData() ? false : (Cmp(*S1, *S2) != 0)); }
  friend __attribute__((warn_unused_result)) bool operator < (const VStr &S1, const char *S2) noexcept { return (Cmp(*S1, S2) < 0); }
  friend __attribute__((warn_unused_result)) bool operator < (const VStr &S1, const VStr &S2) noexcept { return (S1.getData() == S2.getData() ? false : (Cmp(*S1, *S2) < 0)); }
  friend __attribute__((warn_unused_result)) bool operator > (const VStr &S1, const char *S2) noexcept { return (Cmp(*S1, S2) > 0); }
  friend __attribute__((warn_unused_result)) bool operator > (const VStr &S1, const VStr &S2) noexcept { return (S1.getData() == S2.getData() ? false : (Cmp(*S1, *S2) > 0)); }
  friend __attribute__((warn_unused_result)) bool operator <= (const VStr &S1, const char *S2) noexcept { return (Cmp(*S1, S2) <= 0); }
  friend __attribute__((warn_unused_result)) bool operator <= (const VStr &S1, const VStr &S2) noexcept { return (S1.getData() == S2.getData() ? true : (Cmp(*S1, *S2) <= 0)); }
  friend __attribute__((warn_unused_result)) bool operator >= (const VStr &S1, const char *S2) noexcept { return (Cmp(*S1, S2) >= 0); }
  friend __attribute__((warn_unused_result)) bool operator >= (const VStr &S1, const VStr &S2) noexcept { return (S1.getData() == S2.getData() ? true : (Cmp(*S1, *S2) >= 0)); }

  // comparison functions
  __attribute__((warn_unused_result)) inline int Cmp (const char *S2) const noexcept { return Cmp(getData(), S2); }
  __attribute__((warn_unused_result)) inline int Cmp (const VStr &S2) const noexcept { return Cmp(getData(), *S2); }
  __attribute__((warn_unused_result)) inline int ICmp (const char *S2) const noexcept { return ICmp(getData(), S2); }
  __attribute__((warn_unused_result)) inline int ICmp (const VStr &S2) const noexcept { return ICmp(getData(), *S2); }

  __attribute__((warn_unused_result)) inline int cmp (const char *S2) const noexcept { return Cmp(getData(), S2); }
  __attribute__((warn_unused_result)) inline int cmp (const VStr &S2) const noexcept { return Cmp(getData(), *S2); }
  __attribute__((warn_unused_result)) inline int icmp (const char *S2) const noexcept { return ICmp(getData(), S2); }
  __attribute__((warn_unused_result)) inline int icmp (const VStr &S2) const noexcept { return ICmp(getData(), *S2); }

  __attribute__((warn_unused_result)) inline bool StrEqu (const char *S2) const noexcept { return (Cmp(getData(), S2) == 0); }
  __attribute__((warn_unused_result)) inline bool StrEqu (const VStr &S2) const noexcept { return (Cmp(getData(), *S2) == 0); }
  __attribute__((warn_unused_result)) inline bool StrEquCI (const char *S2) const noexcept { return (ICmp(getData(), S2) == 0); }
  __attribute__((warn_unused_result)) inline bool StrEquCI (const VStr &S2) const noexcept { return (ICmp(getData(), *S2) == 0); }

  __attribute__((warn_unused_result)) inline bool strequ (const char *S2) const noexcept { return (Cmp(getData(), S2) == 0); }
  __attribute__((warn_unused_result)) inline bool strequ (const VStr &S2) const noexcept { return (Cmp(getData(), *S2) == 0); }
  __attribute__((warn_unused_result)) inline bool strequCI (const char *S2) const noexcept { return (ICmp(getData(), S2) == 0); }
  __attribute__((warn_unused_result)) inline bool strequCI (const VStr &S2) const noexcept { return (ICmp(getData(), *S2) == 0); }

  __attribute__((warn_unused_result)) inline bool strEqu (const char *S2) const noexcept { return (Cmp(getData(), S2) == 0); }
  __attribute__((warn_unused_result)) inline bool strEqu (const VStr &S2) const noexcept { return (Cmp(getData(), *S2) == 0); }
  __attribute__((warn_unused_result)) inline bool strEquCI (const char *S2) const noexcept { return (ICmp(getData(), S2) == 0); }
  __attribute__((warn_unused_result)) inline bool strEquCI (const VStr &S2) const noexcept { return (ICmp(getData(), *S2) == 0); }

  __attribute__((warn_unused_result)) bool StartsWith (const char *) const noexcept;
  __attribute__((warn_unused_result)) bool StartsWith (const VStr &) const noexcept;
  __attribute__((warn_unused_result)) bool EndsWith (const char *) const noexcept;
  __attribute__((warn_unused_result)) bool EndsWith (const VStr &) const noexcept;

  __attribute__((warn_unused_result)) inline bool startsWith (const char *s) const noexcept { return StartsWith(s); }
  __attribute__((warn_unused_result)) inline bool startsWith (const VStr &s) const noexcept { return StartsWith(s); }
  __attribute__((warn_unused_result)) inline bool endsWith (const char *s) const noexcept { return EndsWith(s); }
  __attribute__((warn_unused_result)) inline bool endsWith (const VStr &s) const noexcept { return EndsWith(s); }

  __attribute__((warn_unused_result)) bool startsWithNoCase (const char *s) const noexcept;
  __attribute__((warn_unused_result)) bool startsWithNoCase (const VStr &s) const noexcept;
  __attribute__((warn_unused_result)) bool endsWithNoCase (const char *s) const noexcept;
  __attribute__((warn_unused_result)) bool endsWithNoCase (const VStr &s) const noexcept;

  __attribute__((warn_unused_result)) inline bool StartsWithNoCase (const char *s) const noexcept { return startsWithNoCase(s); }
  __attribute__((warn_unused_result)) inline bool StartsWithNoCase (const VStr &s) const noexcept { return startsWithNoCase(s); }
  __attribute__((warn_unused_result)) inline bool EndsWithNoCase (const char *s) const noexcept { return endsWithNoCase(s); }
  __attribute__((warn_unused_result)) inline bool EndsWithNoCase (const VStr &s) const noexcept { return endsWithNoCase(s); }

  __attribute__((warn_unused_result)) inline bool StartsWithCI (const char *s) const noexcept { return startsWithNoCase(s); }
  __attribute__((warn_unused_result)) inline bool StartsWithCI (const VStr &s) const noexcept { return startsWithNoCase(s); }
  __attribute__((warn_unused_result)) inline bool EndsWithCI (const char *s) const noexcept { return endsWithNoCase(s); }
  __attribute__((warn_unused_result)) inline bool EndsWithCI (const VStr &s) const noexcept { return endsWithNoCase(s); }

  __attribute__((warn_unused_result)) inline bool startsWithCI (const char *s) const noexcept { return startsWithNoCase(s); }
  __attribute__((warn_unused_result)) inline bool startsWithCI (const VStr &s) const noexcept { return startsWithNoCase(s); }
  __attribute__((warn_unused_result)) inline bool endsWithCI (const char *s) const noexcept { return endsWithNoCase(s); }
  __attribute__((warn_unused_result)) inline bool endsWithCI (const VStr &s) const noexcept { return endsWithNoCase(s); }

  static __attribute__((warn_unused_result)) bool startsWith (const char *str, const char *part) noexcept;
  static __attribute__((warn_unused_result)) bool endsWith (const char *str, const char *part) noexcept;
  static __attribute__((warn_unused_result)) bool startsWithNoCase (const char *str, const char *part) noexcept;
  static __attribute__((warn_unused_result)) bool endsWithNoCase (const char *str, const char *part) noexcept;

  static __attribute__((warn_unused_result)) inline bool StartsWith (const char *str, const char *part) noexcept { return startsWith(str, part); }
  static __attribute__((warn_unused_result)) inline bool SndsWith (const char *str, const char *part) noexcept { return endsWith(str, part); }
  static __attribute__((warn_unused_result)) inline bool startsWithCI (const char *str, const char *part) noexcept { return startsWithNoCase(str, part); }
  static __attribute__((warn_unused_result)) inline bool endsWithCI (const char *str, const char *part) noexcept { return endsWithNoCase(str, part); }

  __attribute__((warn_unused_result)) VStr ToLower () const noexcept;
  __attribute__((warn_unused_result)) VStr ToUpper () const noexcept;

  __attribute__((warn_unused_result)) inline VStr toLowerCase () const noexcept { return ToLower(); }
  __attribute__((warn_unused_result)) inline VStr toUpperCase () const noexcept { return ToUpper(); }

  __attribute__((warn_unused_result)) inline bool isLowerCase () const noexcept {
    const char *dp = getData();
    for (int f = length()-1; f >= 0; --f, ++dp) {
      if (*dp >= 'A' && *dp <= 'Z') return false;
    }
    return true;
  }

  __attribute__((warn_unused_result)) inline static bool isLowerCase (const char *s) noexcept {
    if (!s) return true;
    while (*s) {
      if (*s >= 'A' && *s <= 'Z') return false;
      ++s;
    }
    return true;
  }

  __attribute__((warn_unused_result)) int IndexOf (char pch, int stpos=0) const noexcept;
  __attribute__((warn_unused_result)) int IndexOf (const char *ps, int stpos=0) const noexcept;
  __attribute__((warn_unused_result)) int IndexOf (const VStr &ps, int stpos=0) const noexcept;
  __attribute__((warn_unused_result)) int LastIndexOf (char pch, int stpos=0) const noexcept;
  __attribute__((warn_unused_result)) int LastIndexOf (const char *ps, int stpos=0) const noexcept;
  __attribute__((warn_unused_result)) int LastIndexOf (const VStr &ps, int stpos=0) const noexcept;

  __attribute__((warn_unused_result)) inline int indexOf (char v, int stpos=0) const noexcept { return IndexOf(v, stpos); }
  __attribute__((warn_unused_result)) inline int indexOf (const char *v, int stpos=0) const noexcept { return IndexOf(v, stpos); }
  __attribute__((warn_unused_result)) inline int indexOf (const VStr &v, int stpos=0) const noexcept { return IndexOf(v, stpos); }
  __attribute__((warn_unused_result)) inline int lastIndexOf (char v, int stpos=0) const noexcept { return LastIndexOf(v, stpos); }
  __attribute__((warn_unused_result)) inline int lastIndexOf (const char *v, int stpos=0) const noexcept { return LastIndexOf(v, stpos); }
  __attribute__((warn_unused_result)) inline int lastIndexOf (const VStr &v, int stpos=0) const noexcept { return LastIndexOf(v, stpos); }

  __attribute__((warn_unused_result)) VStr Replace (const char *, const char *) const noexcept;
  __attribute__((warn_unused_result)) VStr Replace (VStr, VStr) const noexcept;

  __attribute__((warn_unused_result)) inline VStr replace (const char *s0, const char *s1) const noexcept { return Replace(s0, s1); }
  __attribute__((warn_unused_result)) inline VStr replace (const VStr &s0, const VStr &s1) const noexcept { return Replace(s0, s1); }

  __attribute__((warn_unused_result)) VStr Utf8Substring (int start, int len) const noexcept;
  __attribute__((warn_unused_result)) inline VStr utf8Substring (int start, int len) const noexcept { return Utf8Substring(start, len); }
  __attribute__((warn_unused_result)) inline VStr utf8substring (int start, int len) const noexcept { return Utf8Substring(start, len); }

  void Split (char, TArray<VStr> &) const noexcept;
  void Split (const char *, TArray<VStr> &) const noexcept;
  void SplitOnBlanks (TArray<VStr> &, bool doQuotedStrings=false) const noexcept;

  inline void split (char c, TArray<VStr> &a) const noexcept { Split(c, a); }
  inline void split (const char *s, TArray<VStr> &a) const noexcept { Split(s, a); }
  inline void splitOnBlanks (TArray<VStr> &a, bool doQuotedStrings=false) const noexcept { SplitOnBlanks(a, doQuotedStrings); }

  // split string to path components; first component can be '/', others has no slashes
  void SplitPath (TArray<VStr> &) const noexcept;
  inline void splitPath (TArray<VStr> &a) const noexcept { SplitPath(a); }

  __attribute__((warn_unused_result)) bool IsValidUtf8 () const noexcept;
  __attribute__((warn_unused_result)) inline bool isValidUtf8 () const noexcept { return IsValidUtf8(); }
  __attribute__((warn_unused_result)) bool isUtf8Valid () const noexcept;

  __attribute__((warn_unused_result)) VStr Latin1ToUtf8 () const noexcept;

  // serialisation operator
  VStream &Serialise (VStream &Strm);
  VStream &Serialise (VStream &Strm) const;

  // if `addQCh` is `true`, add '"' if something was quoted
  __attribute__((warn_unused_result)) VStr quote (bool addQCh=false) const noexcept;
  __attribute__((warn_unused_result)) bool needQuoting () const noexcept;

  __attribute__((warn_unused_result)) VStr xmlEscape () const noexcept;
  __attribute__((warn_unused_result)) VStr xmlUnescape () const noexcept;

  __attribute__((warn_unused_result)) VStr EvalEscapeSequences () const noexcept;

  __attribute__((warn_unused_result)) VStr RemoveColors () const noexcept;
  __attribute__((warn_unused_result)) bool MustBeSanitized () const noexcept;
  static __attribute__((warn_unused_result)) bool MustBeSanitized (const char *str) noexcept;

  __attribute__((warn_unused_result)) VStr ExtractFilePath () const noexcept;
  __attribute__((warn_unused_result)) VStr ExtractFileName () const noexcept;
  __attribute__((warn_unused_result)) VStr ExtractFileBase (bool doSysError=true) const noexcept; // this tries to get only name w/o extension, and calls `Sys_Error()` on too long names
  __attribute__((warn_unused_result)) VStr ExtractFileBaseName () const noexcept;
  __attribute__((warn_unused_result)) VStr ExtractFileExtension () const noexcept; // with a dot
  __attribute__((warn_unused_result)) VStr StripExtension () const noexcept;
  __attribute__((warn_unused_result)) VStr DefaultPath (VStr basepath) const noexcept;
  __attribute__((warn_unused_result)) VStr DefaultExtension (VStr extension) const noexcept;
  __attribute__((warn_unused_result)) VStr FixFileSlashes () const noexcept;

  __attribute__((warn_unused_result)) inline VStr extractFilePath () const noexcept { return ExtractFilePath(); }
  __attribute__((warn_unused_result)) inline VStr extractFileName () const noexcept { return ExtractFileName(); }
  __attribute__((warn_unused_result)) inline VStr extractFileBase (bool doSysError=true) const noexcept { return ExtractFileBase(doSysError); }
  __attribute__((warn_unused_result)) inline VStr extractFileBaseName () const noexcept { return ExtractFileBaseName(); }
  __attribute__((warn_unused_result)) inline VStr extractFileExtension () const noexcept { return ExtractFileExtension(); }
  __attribute__((warn_unused_result)) inline VStr stripExtension () const noexcept { return StripExtension(); }
  __attribute__((warn_unused_result)) inline VStr defaultPath (VStr basepath) const noexcept { return DefaultPath(basepath); }
  __attribute__((warn_unused_result)) inline VStr defaultExtension (VStr extension) const noexcept { return DefaultExtension(extension); }
  __attribute__((warn_unused_result)) inline VStr fixSlashes () const noexcept { return FixFileSlashes(); }

  // removes all blanks
  __attribute__((warn_unused_result)) VStr trimRight () const noexcept;
  __attribute__((warn_unused_result)) VStr trimLeft () const noexcept;
  __attribute__((warn_unused_result)) VStr trimAll () const noexcept;

  // from my iv.strex
  __attribute__((warn_unused_result)) inline VStr xstrip () const noexcept { return trimAll(); }
  __attribute__((warn_unused_result)) inline VStr xstripleft () const noexcept { return trimLeft(); }
  __attribute__((warn_unused_result)) inline VStr xstripright () const noexcept { return trimRight(); }

  __attribute__((warn_unused_result)) bool IsAbsolutePath () const noexcept;
  __attribute__((warn_unused_result)) inline bool isAbsolutePath () const noexcept { return IsAbsolutePath(); }

  // reject absolute names, names with ".", and names with "..", and names ends with path delimiter
  __attribute__((warn_unused_result)) bool isSafeDiskFileName () const noexcept;

  static __attribute__((warn_unused_result)) inline int Length (const char *s) noexcept { return (s ? (int)strlen(s) : 0); }
  static __attribute__((warn_unused_result)) inline int length (const char *s) noexcept { return (s ? (int)strlen(s) : 0); }
  static __attribute__((warn_unused_result)) int Utf8Length (const char *s, int len=-1) noexcept;
  static __attribute__((warn_unused_result)) inline int utf8Length (const char *s, int len=-1) noexcept { return (int)Utf8Length(s, len); }
  static __attribute__((warn_unused_result)) size_t ByteLengthForUtf8 (const char *, size_t) noexcept;
  // get utf8 char; advances pointer, returns '?' on invalid char
  static __attribute__((warn_unused_result)) int Utf8GetChar (const char *&s) noexcept;
  static __attribute__((warn_unused_result)) VStr FromUtf8Char (int) noexcept;

  static __attribute__((warn_unused_result)) inline int Cmp (const char *S1, const char *S2) noexcept { return (S1 == S2 ? 0 : strcmp((S1 ? S1 : ""), (S2 ? S2 : ""))); }
  static __attribute__((warn_unused_result)) inline int NCmp (const char *S1, const char *S2, size_t N) noexcept { return (S1 == S2 ? 0 : strncmp((S1 ? S1 : ""), (S2 ? S2 : ""), N)); }

  static __attribute__((warn_unused_result)) int ICmp (const char *s0, const char *s1) noexcept;
  static __attribute__((warn_unused_result)) int NICmp (const char *s0, const char *s1, size_t max) noexcept;

  static __attribute__((warn_unused_result)) inline bool strequ (const char *S1, const char *S2) noexcept { return (Cmp(S1, S2) == 0); }
  static __attribute__((warn_unused_result)) inline bool strequCI (const char *S1, const char *S2) noexcept { return (ICmp(S1, S2) == 0); }
  static __attribute__((warn_unused_result)) inline bool nstrequ (const char *S1, const char *S2, size_t max) noexcept { return (NCmp(S1, S2, max) == 0); }
  static __attribute__((warn_unused_result)) inline bool nstrequCI (const char *S1, const char *S2, size_t max) noexcept { return (NICmp(S1, S2, max) == 0); }

  static __attribute__((warn_unused_result)) inline bool StrEqu (const char *S1, const char *S2) noexcept { return (Cmp(S1, S2) == 0); }
  static __attribute__((warn_unused_result)) inline bool StrEquCI (const char *S1, const char *S2) noexcept { return (ICmp(S1, S2) == 0); }
  static __attribute__((warn_unused_result)) inline bool NStrEqu (const char *S1, const char *S2, size_t max) noexcept { return (NCmp(S1, S2, max) == 0); }
  static __attribute__((warn_unused_result)) inline bool NStrEquCI (const char *S1, const char *S2, size_t max) noexcept { return (NICmp(S1, S2, max) == 0); }

  static __attribute__((warn_unused_result)) inline bool strEqu (const char *S1, const char *S2) noexcept { return (Cmp(S1, S2) == 0); }
  static __attribute__((warn_unused_result)) inline bool strEquCI (const char *S1, const char *S2) noexcept { return (ICmp(S1, S2) == 0); }
  static __attribute__((warn_unused_result)) inline bool nstrEqu (const char *S1, const char *S2, size_t max) noexcept { return (NCmp(S1, S2, max) == 0); }
  static __attribute__((warn_unused_result)) inline bool nstrEquCI (const char *S1, const char *S2, size_t max) noexcept { return (NICmp(S1, S2, max) == 0); }

  static inline void Cpy (char *dst, const char *src) noexcept {
    if (dst) { if (src) strcpy(dst, src); else *dst = 0; }
  }

  // will write terminating zero; buffer should be at leasn [N+1] bytes long
  static inline void NCpy (char *dst, const char *src, size_t N) noexcept {
    if (dst && src && N && src[0]) {
      size_t slen = strlen(src);
      if (slen > N) slen = N;
      memcpy(dst, src, slen);
      dst[slen] = 0;
    } else {
      if (dst) *dst = 0;
    }
  }

  static __attribute__((warn_unused_result)) inline char ToUpper (char c) noexcept { return (c >= 'a' && c <= 'z' ? c-32 : c); }
  static __attribute__((warn_unused_result)) inline char ToLower (char c) noexcept { return (c >= 'A' && c <= 'Z' ? c+32 : c); }

  static __attribute__((warn_unused_result)) inline char toupper (char c) noexcept { return (c >= 'a' && c <= 'z' ? c-32 : c); }
  static __attribute__((warn_unused_result)) inline char tolower (char c) noexcept { return (c >= 'A' && c <= 'Z' ? c+32 : c); }

  // append codepoint to this string, in utf-8
  VStr &utf8Append (vuint32 code) noexcept;

  __attribute__((warn_unused_result)) VStr utf2win () const noexcept;
  __attribute__((warn_unused_result)) VStr win2utf () const noexcept;

  __attribute__((warn_unused_result)) VStr utf2koi () const noexcept;
  __attribute__((warn_unused_result)) VStr koi2utf () const noexcept;

  __attribute__((warn_unused_result)) VStr toLowerCase1251 () const noexcept;
  __attribute__((warn_unused_result)) VStr toUpperCase1251 () const noexcept;

  __attribute__((warn_unused_result)) VStr toLowerCaseKOI () const noexcept;
  __attribute__((warn_unused_result)) VStr toUpperCaseKOI () const noexcept;

  __attribute__((warn_unused_result)) inline bool equ1251CI (const VStr &s) const noexcept {
    size_t slen = (size_t)length();
    if (slen != (size_t)s.length()) return false;
    for (size_t f = 0; f < slen; ++f) if (locase1251(getData()[f]) != locase1251(s[f])) return false;
    return true;
  }

  __attribute__((warn_unused_result)) inline bool equ1251CI (const char *s) const noexcept {
    size_t slen = length();
    if (!s || !s[0]) return (slen == 0);
    if (slen != strlen(s)) return false;
    for (size_t f = 0; f < slen; ++f) if (locase1251(getData()[f]) != locase1251(s[f])) return false;
    return true;
  }

  __attribute__((warn_unused_result)) inline bool equKOICI (const VStr &s) const noexcept {
    size_t slen = (size_t)length();
    if (slen != (size_t)s.length()) return false;
    for (size_t f = 0; f < slen; ++f) if (locaseKOI(getData()[f]) != locaseKOI(s[f])) return false;
    return true;
  }

  __attribute__((warn_unused_result)) inline bool equKOICI (const char *s) const noexcept {
    size_t slen = length();
    if (!s || !s[0]) return (slen == 0);
    if (slen != strlen(s)) return false;
    for (size_t f = 0; f < slen; ++f) if (locaseKOI(getData()[f]) != locaseKOI(s[f])) return false;
    return true;
  }

  __attribute__((warn_unused_result)) inline bool fnameEqu1251CI (const VStr &s) const noexcept { return fnameEqu1251CI(s.getData()); }
  __attribute__((warn_unused_result)) bool fnameEqu1251CI (const char *s) const noexcept;

  __attribute__((warn_unused_result)) inline bool fnameEquKOICI (const VStr &s) const noexcept { return fnameEquKOICI(s.getData()); }
  __attribute__((warn_unused_result)) bool fnameEquKOICI (const char *s) const noexcept;

  static __attribute__((warn_unused_result)) VStr buf2hex (const void *buf, int buflen) noexcept;

  inline bool convertInt (int *outv) const noexcept { return convertInt(getCStr(), outv); }
  inline bool convertFloat (float *outv) const noexcept { return convertFloat(getCStr(), outv); }

  static __attribute__((warn_unused_result)) bool globmatch (const char *str, const char *pat, bool caseSensitive=true) noexcept;
  __attribute__((warn_unused_result)) inline bool globmatch (const char *pat, bool caseSensitive=true) const noexcept { return globmatch(getData(), pat, caseSensitive); }
  __attribute__((warn_unused_result)) inline bool globmatch (const VStr &pat, bool caseSensitive=true) const noexcept { return globmatch(getData(), *pat, caseSensitive); }

  static __attribute__((warn_unused_result)) inline bool globMatch (const char *str, const char *pat, bool caseSensitive=true) noexcept { return globmatch(str, pat, caseSensitive); }
  __attribute__((warn_unused_result)) inline bool globMatch (const char *pat, bool caseSensitive=true) const noexcept { return globmatch(getData(), pat, caseSensitive); }
  __attribute__((warn_unused_result)) inline bool globMatch (const VStr &pat, bool caseSensitive=true) const noexcept { return globmatch(getData(), *pat, caseSensitive); }

  static __attribute__((warn_unused_result)) inline bool globmatchCI (const char *str, const char *pat) noexcept { return globmatch(str, pat, false); }
  __attribute__((warn_unused_result)) inline bool globmatchCI (const char *pat) const noexcept { return globmatch(getData(), pat, false); }
  __attribute__((warn_unused_result)) inline bool globmatchCI (const VStr &pat) const noexcept { return globmatch(getData(), *pat, false); }

  static __attribute__((warn_unused_result)) inline bool globMatchCI (const char *str, const char *pat) noexcept { return globmatch(str, pat, false); }
  __attribute__((warn_unused_result)) inline bool globMatchCI (const char *pat) const noexcept { return globmatch(getData(), pat, false); }
  __attribute__((warn_unused_result)) inline bool globMatchCI (const VStr &pat) const noexcept { return globmatch(getData(), *pat, false); }

  // will not clear `args`
  void Tokenise (TArray <VStr> &args) const noexcept;
  inline void Tokenize (TArray <VStr> &args) const noexcept { Tokenise(args); }
  inline void tokenise (TArray <VStr> &args) const noexcept { Tokenise(args); }
  inline void tokenize (TArray <VStr> &args) const noexcept { Tokenise(args); }

  // this finds start of the next command
  // commands are ';'-delimited
  // processes quotes as `tokenise` does
  // skips all leading spaces by default (i.e. result can be >0 even if there are no ';')
  int findNextCommand (int stpos=0, bool skipLeadingSpaces=true) const noexcept;

public:
  static __attribute__((warn_unused_result)) inline char wchar2win (vuint32 wc) noexcept { return (wc < 65536 ? wc2shitmap[wc] : '?'); }
  static __attribute__((warn_unused_result)) inline char wchar2koi (vuint32 wc) noexcept { return (wc < 65536 ? wc2koimap[wc] : '?'); }

  static __attribute__((warn_unused_result)) inline __attribute__((pure)) int digitInBase (char ch, int base=10) noexcept {
    if (base < 1 || base > 36 || ch < '0') return -1;
    if (base <= 10) return (ch < 48+base ? ch-48 : -1);
    if (ch >= '0' && ch <= '9') return ch-48;
    if (ch >= 'a' && ch <= 'z') ch -= 32; // poor man tolower()
    if (ch < 'A' || ch >= 65+(base-10)) return -1;
    return ch-65+10;
  }

  static __attribute__((warn_unused_result)) inline __attribute__((pure)) char upcase1251 (char ch) noexcept {
    if ((vuint8)ch < 128) return ch-(ch >= 'a' && ch <= 'z' ? 32 : 0);
    if ((vuint8)ch >= 224 /*&& (vuint8)ch <= 255*/) return (vuint8)ch-32;
    if ((vuint8)ch == 184 || (vuint8)ch == 186 || (vuint8)ch == 191) return (vuint8)ch-16;
    if ((vuint8)ch == 162 || (vuint8)ch == 179) return (vuint8)ch-1;
    return ch;
  }

  static __attribute__((warn_unused_result)) inline __attribute__((pure)) char locase1251 (char ch) noexcept {
    if ((vuint8)ch < 128) return ch+(ch >= 'A' && ch <= 'Z' ? 32 : 0);
    if ((vuint8)ch >= 192 && (vuint8)ch <= 223) return (vuint8)ch+32;
    if ((vuint8)ch == 168 || (vuint8)ch == 170 || (vuint8)ch == 175) return (vuint8)ch+16;
    if ((vuint8)ch == 161 || (vuint8)ch == 178) return (vuint8)ch+1;
    return ch;
  }

  static __attribute__((warn_unused_result)) inline __attribute__((pure)) bool isAlpha2151 (char ch) noexcept {
    if (ch >= 'A' && ch <= 'Z') return true;
    if (ch >= 'a' && ch <= 'z') return true;
    if ((vuint8)ch >= 191) return true;
    switch ((vuint8)ch) {
      case 161: case 162: case 168: case 170: case 175:
      case 178: case 179: case 184: case 186:
        return true;
    }
    return false;
  }

  /* koi8-u */
  static __attribute__((warn_unused_result)) inline __attribute__((pure)) int locaseKOI (char ch) noexcept {
    if ((vuint8)ch < 128) {
      if (ch >= 'A' && ch <= 'Z') ch += 32;
    } else {
      if ((vuint8)ch >= 224 && (vuint8)ch <= 255) ch -= 32;
      else {
        switch ((vuint8)ch) {
          case 179: case 180: case 182: case 183: case 189: ch -= 16; break;
        }
      }
    }
    return ch;
  }

  static __attribute__((warn_unused_result)) inline __attribute__((pure)) int upcaseKOI (char ch) noexcept {
    if ((vuint8)ch < 128) {
      if (ch >= 'a' && ch <= 'z') ch -= 32;
    } else {
      if ((vuint8)ch >= 192 && (vuint8)ch <= 223) ch += 32;
      else {
        switch ((vuint8)ch) {
          case 163: case 164: case 166: case 167: case 173: ch += 16; break;
        }
      }
    }
    return ch;
  }

  static __attribute__((warn_unused_result)) inline __attribute__((pure)) bool isAlphaKOI (char ch) noexcept {
    if (ch >= 'A' && ch <= 'Z') return true;
    if (ch >= 'a' && ch <= 'z') return true;
    if ((vuint8)ch >= 192) return true;
    switch ((vuint8)ch) {
      case 163: case 164: case 166: case 167: case 173:
      case 179: case 180: case 182: case 183: case 189:
        return true;
    }
    return false;
  }

  static __attribute__((warn_unused_result)) inline __attribute__((pure)) bool isAlphaAscii (char ch) noexcept {
    return
      (ch >= 'A' && ch <= 'Z') ||
      (ch >= 'a' && ch <= 'z');
  }

  // returns length of the following utf-8 sequence from its first char, or -1 for invalid first char
  static __attribute__((warn_unused_result)) inline __attribute__((pure)) int utf8CodeLen (char ch) noexcept {
    if ((vuint8)ch < 0x80) return 1;
    if ((ch&0xFE) == 0xFC) return 6;
    if ((ch&0xFC) == 0xF8) return 5;
    if ((ch&0xF8) == 0xF0) return 4;
    if ((ch&0xF0) == 0xE0) return 3;
    if ((ch&0xE0) == 0xC0) return 2;
    return -1; // invalid
  }

  static __attribute__((warn_unused_result)) inline bool isPathDelimiter (const char ch) noexcept {
    #ifdef _WIN32
      return (ch == '/' || ch == '\\' || ch == ':');
    #else
      return (ch == '/');
    #endif
  }

  static __attribute__((warn_unused_result)) bool isSafeDiskFileName (const VStr &fname) noexcept { return fname.isSafeDiskFileName(); }

public:
  static const vuint16 cp1251[128];
  static char wc2shitmap[65536];

  static const vuint16 cpKOI[128];
  static char wc2koimap[65536];

  static const VStr EmptyString;

public:
  static inline __attribute__((unused)) __attribute__((warn_unused_result))
  VStr size2human (vuint32 size) noexcept {
         if (size < 1024*1024) return va("%u%s KB", size/1024, (size%1024 >= 512 ? ".5" : ""));
    else if (size < 1024*1024*1024) return va("%u%s MB", size/(1024*1024), (size%(1024*1024) >= 1024 ? ".5" : ""));
    else return va("%u%s GB", size/(1024*1024*1024), (size%(1024*1024*1024) >= 1024*1024 ? ".5" : ""));
  }
};


//inline vuint32 GetTypeHash (const char *s) { return (s && s[0] ? fnvHashBuf(s, strlen(s)) : 1); }
//inline vuint32 GetTypeHash (const VStr &s) { return (s.length() ? fnvHashBuf(*s, s.length()) : 1); }

// results MUST be equal
static inline __attribute__((unused)) vuint32 GetTypeHash (const char *s) noexcept { return fnvHashStr(s); }
static inline __attribute__((unused)) vuint32 GetTypeHash (const VStr &s) noexcept { return fnvHashStr(*s); }


// ////////////////////////////////////////////////////////////////////////// //
struct VUtf8DecoderFast {
public:
  enum {
    Replacement = 0xFFFD, // replacement char for invalid unicode
    Accept = 0,
    Reject = 12,
  };

protected:
  vuint32 state;

public:
  vuint32 codepoint; // decoded codepoint (valid only when decoder is in "complete" state)

public:
  VUtf8DecoderFast () noexcept : state(Accept), codepoint(0) {}

  inline void reset () noexcept { state = Accept; codepoint = 0; }

  // is current character valid and complete? take `codepoint` then
  __attribute__((warn_unused_result)) inline bool complete () const noexcept { return (state == Accept); }
  // is current character invalid and complete? take `Replacement` then
  __attribute__((warn_unused_result)) inline bool invalid () const noexcept { return (state == Reject); }
  // is current character complete (valid or invaluid)? take `codepoint` then
  __attribute__((warn_unused_result)) inline bool hasCodePoint () const noexcept { return (state == Accept || state == Reject); }

  // process another input byte; returns `true` if codepoint is complete
  inline bool put (vuint8 c) noexcept {
    if (state == Reject) { state = Accept; codepoint = 0; } // restart from invalid state
    vuint8 tp = utf8dfa[c];
    codepoint = (state != Accept ? (c&0x3f)|(codepoint<<6) : (0xff>>tp)&c);
    state = utf8dfa[256+state+tp];
    if (state == Reject) codepoint = Replacement;
    return (state == Accept || state == Reject);
  }

protected:
  static const vuint8 utf8dfa[0x16c];
};

// required for Vavoom C VM
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
    //Z_Free(dmn);
    // use `free()` here, because it is not allocated via zone allocator
    free(dmn);
  }
  return tpn;
}


template<class T> VStr shitppTypeNameObj (const T &o) {
  VStr tpn(typeid(o).name());
  char *dmn = abi::__cxa_demangle(*tpn, nullptr, nullptr, nullptr);
  if (dmn) {
    tpn = VStr(dmn);
    //Z_Free(dmn);
    // use `free()` here, because it is not allocated via zone allocator
    free(dmn);
  }
  return tpn;
}
