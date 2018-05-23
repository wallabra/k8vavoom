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

#define TEXT_COLOUR_ESCAPE    '\034'

// WARNING! this cannot be bigger than one pointer, or VM will break!
class VStr {
private:
  // int at [-1]: length
  // int at [-2]: refcount
  char *data; // string, 0-terminated (0 is not in length); can be null

  inline int *refp () { return (data ? ((int*)data)-2 : nullptr); }
  inline int *lenp () { return (data ? ((int*)data)-1 : nullptr); }

  inline const int *refp () const { return (data ? ((const int*)data)-2 : nullptr); }
  inline const int *lenp () const { return (data ? ((const int*)data)-1 : nullptr); }

  void MakeMutable (); // and unique
  void Resize (int newlen); // always makes string unique

  void SetContent (const char *s, int len=-1); // not necessarily copies anything
  void SetStaticContent (const char *s);

  inline bool isMyData (const char *buf) const { return (data && buf && buf >= data && buf < data+length()); }
  inline bool isMyData (const char *buf, int len) const { return (data && buf && buf < data+length() && buf+len >= data); }

public:
  VStr () : data(nullptr) {}
  VStr (ENoInit) {}
  VStr (const VStr &instr, int start, int len) : data(nullptr) { Assign(instr.mid(start, len)); }
  //VStr (const VStr *instr, int start, int len) : data(nullptr) { if (instr && instr->data) Assign(instr->mid(start, len)); }

  VStr (const VStr &instr) : data(nullptr) { if (instr.data) { data = (char*)instr.data; ++(*refp()); } }
  VStr (const char *instr, int len=-1) : data(nullptr) { SetContent(instr, len); }
  //VStr (const VStr *instr) : data(nullptr) { if (instr && instr->data) { data = (char*)instr->data; ++(*refp()); } }

  explicit VStr (char inchr) : data(nullptr) { SetContent(&inchr, 1); }
  explicit VStr (bool InBool) : data(nullptr) { SetContent(InBool ? "true" : "false"); }

  explicit VStr (int InInt) : data(nullptr) {
    char buf[64];
    int len = (int)snprintf(buf, sizeof(buf), "%d", InInt);
    SetContent(buf, len);
  }

  explicit VStr (unsigned InInt) : data(nullptr) {
    char buf[64];
    int len = (int)snprintf(buf, sizeof(buf), "%u", InInt);
    SetContent(buf, len);
  }

  explicit VStr (float InFloat) : data(nullptr) {
    char buf[64];
    int len = (int)snprintf(buf, sizeof(buf), "%f", InFloat);
    SetContent(buf, len);
  }

  explicit VStr (const VName &InName) : data(nullptr) { SetContent(*InName); }

  ~VStr () { Clean(); }

  inline void Assign (const VStr &instr) {
    if (instr.data) {
      if (instr.data != data) {
        int *xrp = (int*)instr.data;
        ++(xrp[-2]); // increment refcounter
        Clear();
        data = (char*)instr.data;
      } else {
        ++(*refp());
      }
    } else {
      Clear();
    }
  }

  // clears the string
  inline void Clean () { Resize(0); }
  // clears the string
  inline void Clear () { Resize(0); }
  // clears the string
  inline void clear () { Resize(0); }

  // returns length of the string
  inline size_t Length () const { return (data ? *lenp() : 0); }
  // returns length of the string
  inline size_t length () const { return (data ? *lenp() : 0); }

  // returns number of characters in a UTF-8 string
  inline size_t Utf8Length () const { return (data ? Utf8Length(data) : 0); }
  // returns number of characters in a UTF-8 string
  inline size_t utf8Length () const { return (data ? Utf8Length(data) : 0); }

  // returns C string
  inline const char *operator * () const { return (data ? data : ""); }

  inline bool isUnuqie () const { return (data && *refp() == 1); }

  // checks if string is empty
  inline bool IsEmpty () const { return !data; }
  inline bool IsNotEmpty () const { return !!data; }

  inline bool isEmpty () const { return !data; }
  inline bool isNotEmpty () const { return !!data; }

  // character accessors
  inline char operator [] (int Idx) const { return data[Idx]; }
  //char& operator [] (int Idx) { return data[Idx]; }

  inline char *GetMutableCharPointer (int Idx) { MakeMutable(); return &data[Idx]; }

  VStr mid (int start, int len) const;

  // assignement operators
  inline VStr &operator = (const char *instr) { SetContent(instr); return *this; }
  inline VStr &operator = (const VStr &instr) { Assign(instr); return *this; }
  //inline VStr &operator = (const VStr *instr) { if (instr) Assign(*instr); else Clear(); return *this; }

  // concatenation operators
  VStr &operator += (const char *instr) {
    int inl = int(Length(instr));
    if (inl) {
      if (isMyData(instr, inl)) {
        VStr s(instr);
        operator+=(s);
      } else {
        int l = int(Length());
        Resize(int(l+inl));
        memcpy(data+l, instr, inl+1);
      }
    }
    return *this;
  }

  VStr &operator += (const VStr &instr) {
    int inl = int(instr.Length());
    if (inl) {
      int l = int(Length());
      if (isMyData(instr.data, inl)) {
        VStr s(instr);
        s.MakeMutable(); // ensure unique
        Resize(int(l+inl));
        memcpy(data+l, s.data, inl+1);
      } else {
        Resize(int(l+inl));
        memcpy(data+l, instr.data, inl+1);
      }
    }
    return *this;
  }

  VStr &operator += (char inchr) {
    int l = int(Length());
    Resize(int(l+1));
    data[l] = inchr;
    return *this;
  }

  inline VStr &operator += (bool InBool) { return operator+=(InBool ? "true" : "false"); }

  VStr &operator += (int InInt) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%d", InInt);
    return operator+=(buf);
  }

  VStr &operator += (unsigned InInt) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%u", InInt);
    return operator+=(buf);
  }

  VStr &operator += (float InFloat) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%f", InFloat);
    return operator+=(buf);
  }

  inline VStr &operator += (const VName &InName) { return operator+=(*InName); }

  friend VStr operator + (const VStr &S1, const char *S2) { VStr res(S1); res += S2; return res; }
  friend VStr operator + (const VStr &S1, const VStr &S2) { VStr res(S1); res += S2; return res; }
  friend VStr operator + (const VStr &S1, char S2) { VStr res(S1); res += S2; return res; }
  friend VStr operator + (const VStr &S1, bool InBool) { VStr res(S1); res += InBool; return res; }
  friend VStr operator + (const VStr &S1, int InInt) { VStr res(S1); res += InInt; return res; }
  friend VStr operator + (const VStr &S1, unsigned InInt) { VStr res(S1); res += InInt; return res; }
  friend VStr operator + (const VStr &S1, float InFloat) { VStr res(S1); res += InFloat; return res; }
  friend VStr operator + (const VStr &S1, const VName &InName) { VStr res(S1); res += InName; return res; }

  // comparison operators
  friend bool operator == (const VStr &S1, const char* S2) { return (Cmp(*S1, S2) == 0); }
  friend bool operator == (const VStr &S1, const VStr &S2) { return (S1.data == S2.data ? true : (Cmp(*S1, *S2) == 0)); }
  friend bool operator != (const VStr &S1, const char* S2) { return (Cmp(*S1, S2) != 0); }
  friend bool operator != (const VStr &S1, const VStr &S2) { return (S1.data == S2.data ? false : (Cmp(*S1, *S2) != 0)); }

  // comparison functions
  inline int Cmp (const char *S2) const { return Cmp(data, S2); }
  inline int Cmp (const VStr &S2) const { return Cmp(data, *S2); }
  inline int ICmp (const char *S2) const { return ICmp(data, S2); }
  inline int ICmp (const VStr &S2) const { return ICmp(data, *S2); }

  bool StartsWith (const char *) const;
  bool StartsWith (const VStr&) const;
  bool EndsWith (const char *) const;
  bool EndsWith (const VStr&) const;

  VStr ToLower () const;
  VStr ToUpper () const;

  int IndexOf (char) const;
  int IndexOf (const char *) const;
  int IndexOf (const VStr&) const;
  int LastIndexOf (char) const;
  int LastIndexOf (const char *) const;
  int LastIndexOf (const VStr&) const;

  VStr Replace (const char *, const char *) const;
  VStr Replace (const VStr&, const VStr&) const;

  VStr Utf8Substring (int start, int len) const;

  void Split (char, TArray<VStr>&) const;
  void Split (const char *, TArray<VStr>&) const;

  // split string to path components; first component can be '/', others has no slashes
  void SplitPath (TArray<VStr>&) const;

  bool IsValidUtf8 () const;
  VStr Latin1ToUtf8 () const;

  // serialisation operator
  friend VStream &operator << (VStream &Strm, VStr &S) {
    if (Strm.IsLoading()) {
      vint32 len;
      Strm << STRM_INDEX(len);
      if (len < 0) len = 0;
      S.Resize(len);
      if (len) Strm.Serialise(S.data, len+1);
    } else {
      vint32 len = vint32(S.Length());
      Strm << STRM_INDEX(len);
      if (len) Strm.Serialise(S.data, len+1);
    }
    return Strm;
  }

  VStr EvalEscapeSequences () const;

  VStr RemoveColours () const;

  VStr ExtractFilePath () const;
  VStr ExtractFileName () const;
  VStr ExtractFileBase () const;
  VStr ExtractFileExtension () const;
  VStr StripExtension () const;
  VStr DefaultPath (const VStr &basepath) const;
  VStr DefaultExtension (const VStr &extension) const;
  VStr FixFileSlashes () const;

  static inline size_t Length (const char *s) { return (s ? strlen(s) : 0); }
  static inline size_t length (const char *s) { return (s ? strlen(s) : 0); }
  static size_t Utf8Length (const char *);
  static inline size_t utf8Length (const char *s) { return Utf8Length(s); }
  static size_t ByteLengthForUtf8 (const char *, size_t);
  static int GetChar (const char *&);
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
};

char *va (const char *text, ...) __attribute__ ((format(printf, 1, 2)));

inline vuint32 GetTypeHash (const char *s) { return (s && s[0] ? fnvHashBuf(s, strlen(s)) : 1); }
inline vuint32 GetTypeHash (const VStr &s) { return (s.length() ? fnvHashBuf(*s, s.length()) : 1); }

