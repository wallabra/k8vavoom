//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2019 Ketmar Dark
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
//**
//**  typed and named field
//**
//**************************************************************************
class VClass;


// ////////////////////////////////////////////////////////////////////////// //
// WARNING! this *SHOULD* *NOT* have any virtual methods!
struct VNTValue {
public:
  enum {
    T_Invalid, // zero, so we can `memset(0)` this
    T_Int,
    T_Float,
    T_Vec, // 3 floats
    T_Name,
    T_Str,
    T_Class,
    T_Obj, // VObject
    T_XObj, // VSerialisable
    T_Blob, // byte array
  };

protected:
  vuint8 type; // T_XXX
  VName name;
  // some values are moved out of union for correct finalisation
  union {
    vint32 ival;
    float fval;
    VMemberBase *cval;
    VObject *oval;
    VSerialisable *xoval;
    //void *ptr; // `VStr *`, or `vuint8 *`
  } ud;
  TVec vval;
  VName nval;
  VStr sval;
  // blob data
  vuint32 *blobRC; // blob data refcounter; `nullptr` if data isn't copied
  vuint8 *blob;
  vint32 blobSize;

private:
  inline void zeroSelf () noexcept { memset((void *)this, 0, sizeof(VNTValue)); }

  inline void incRef () const noexcept {
    if (blobRC) ++(*blobRC);
  }

  inline void decRef () {
    if (blobRC) {
      if (--(*blobRC) == 0) {
        Z_Free(blob);
        Z_Free(blobRC);
      }
    }
    blobRC = nullptr;
    blob = nullptr;
    blobSize = 0;
  }

  inline void newBlob (int len) {
    vassert(blobRC == nullptr);
    vassert(blob == nullptr);
    vassert(blobSize == 0);
    vassert(len >= 0);
    if (len > 0) {
      blobRC = (vuint32 *)Z_Malloc(sizeof(vuint32));
      *blobRC = 1;
      blob = (vuint8 *)Z_Malloc(len);
    } else {
      blobRC = nullptr;
      blob = nullptr;
    }
    blobSize = len;
  }

  // should be cleared, type should be set
  void serialiseValueInternal (VStream &strm);

public:
  inline VNTValue () noexcept { memset((void *)this, 0, sizeof(VNTValue)); }
  inline ~VNTValue () { clear(); }

  inline VNTValue (const VNTValue &s) noexcept {
    zeroSelf();
    type = s.type;
    name = s.name;
    switch (s.type) {
      case T_Vec: vval = s.vval; break;
      case T_Name: nval = s.nval; break;
      case T_Str: sval = s.sval; break;
      case T_Blob: blobRC = s.blobRC; blob = s.blob; blobSize = s.blobSize; incRef(); break;
      default: memcpy((void *)&ud, &s.ud, sizeof(ud)); break;
    }
  }

  VNTValue (VName aname, vint32 val) : vval(0, 0, 0), nval(NAME_None), sval() { zeroSelf(); type = T_Int; name = aname; ud.ival = val; }
  VNTValue (VName aname, vuint32 val) : vval(0, 0, 0), nval(NAME_None), sval() { zeroSelf(); type = T_Int; name = aname; ud.ival = (vint32)val; }
  VNTValue (VName aname, float val) : vval(0, 0, 0), nval(NAME_None), sval() { zeroSelf(); type = T_Float; name = aname; ud.fval = val; }
  VNTValue (VName aname, const TVec &val) : vval(0, 0, 0), nval(NAME_None), sval() { zeroSelf(); type = T_Vec; name = aname; vval = val; }
  VNTValue (VName aname, VName val) : vval(0, 0, 0), nval(NAME_None), sval() { zeroSelf(); type = T_Name; name = aname; nval = val; }
  VNTValue (VName aname, VStr val) : vval(0, 0, 0), nval(NAME_None), sval() { zeroSelf(); type = T_Str; name = aname; sval = val; }
  VNTValue (VName aname, VClass *val) : vval(0, 0, 0), nval(NAME_None), sval() { zeroSelf(); type = T_Class; name = aname; ud.cval = (VMemberBase *)val; }
  VNTValue (VName aname, VObject *val) : vval(0, 0, 0), nval(NAME_None), sval() { zeroSelf(); type = T_Obj; name = aname; ud.oval = val; }
  VNTValue (VName aname, VSerialisable *val) : vval(0, 0, 0), nval(NAME_None), sval() { zeroSelf(); type = T_XObj; name = aname; ud.xoval = val; }

  VNTValue (VName aname, const vuint8 *buf, int bufsz, bool doCopyData=true);

  VNTValue &operator = (const VNTValue &s) {
    if ((void *)this == (void *)&s) return *this;
    if (s.type == T_Blob) s.incRef();
    // clear this value
    clear();
    type = s.type;
    name = s.name;
    // copy other value
    switch (s.type) {
      case T_Vec: vval = s.vval; break;
      case T_Name: nval = s.nval; break;
      case T_Str: sval = s.sval; break;
      case T_Blob: blobRC = s.blobRC; blob = s.blob; blobSize = s.blobSize; incRef(); break;
      default: memcpy((void *)&ud, &s.ud, sizeof(ud)); break;
    }
    return *this;
  }

  bool operator == (const VNTValue &) = delete;
  bool operator != (const VNTValue &) = delete;

  inline void clear () {
    switch (type) {
      case T_Str: sval = VStr::EmptyString; break;
      case T_Blob: decRef(); break;
    }
    zeroSelf();
  }

  void Serialise (VStream &strm);
  void Serialise (VStream &strm) const;

  // this can optinally return value type and name
  static void SkipSerialised (VStream &strm, vuint8 *otype=nullptr, VName *oname=nullptr);
  // doesn't call `Sys_Error()` on invalid type
  // returns `false` on invalid type
  static bool ReadTypeName (VStream &strm, vuint8 *otype, VName *oname);
  // call this after `ReadTypeName()`
  // doesn't call `Sys_Error()` on errors
  // returns `false` on invalid type, or on other error
  static bool SkipValue (VStream &strm, vuint8 atype);
  // call this after `ReadTypeName()`
  // doesn't call `Sys_Error()` on errors
  // returns invalid value on any error
  static VNTValue ReadValue (VStream &strm, vuint8 atype, VName aname);

  static inline bool isValidType (vuint8 atype) noexcept { return (atype > T_Invalid && atype <= T_Blob); }

  inline operator bool () const noexcept { return isValid(); }

  // checkers
  inline bool isValid () const noexcept { return (type > T_Invalid && type <= T_Blob); }
  inline bool isInt () const noexcept { return (type == T_Int); }
  inline bool isFloat () const noexcept { return (type == T_Float); }
  inline bool isVec () const noexcept { return (type == T_Vec); }
  inline bool isName () const noexcept { return (type == T_Name); }
  inline bool isStr () const noexcept { return (type == T_Str); }
  inline bool isClass () const noexcept { return (type == T_Class); }
  inline bool isObj () const noexcept { return (type == T_Obj); }
  inline bool isXObj () const noexcept { return (type == T_XObj); }
  inline bool isBlob () const noexcept { return (type == T_Blob); }

  // getters and setters
  inline VName getName () const noexcept { return name; }
  inline vuint8 getType () const noexcept { return type; }

  inline vint32 getInt () const noexcept { return (type == T_Int ? ud.ival : 0); }
  inline float getFloat () const noexcept { return (type == T_Float ? ud.fval : 0); }
  inline TVec getVec () const noexcept { return (type == T_Vec ? vval : TVec(0, 0, 0)); }
  inline VName getVName () const noexcept { return (type == T_Name ? nval : VName(NAME_None)); }
  inline VStr getStr () const noexcept { return (type == T_Str ? sval : VStr::EmptyString); }
  inline VClass *getClass () const noexcept { return (type == T_Class ? (VClass *)ud.cval : nullptr); }
  inline VObject *getObj () const noexcept { return (type == T_Obj ? (VObject *)ud.oval : nullptr); }
  inline VSerialisable *getXObj () const noexcept { return (type == T_XObj ? (VSerialisable *)ud.xoval : nullptr); }

  inline vint32 getBlobSize () const noexcept { return (type == T_Blob ? blobSize : 0); }
  inline vuint8 *getBlobPtr () noexcept { return (type == T_Blob ? blob : nullptr); }
  inline const vuint8 *getBlobPtr () const noexcept { return (type == T_Blob ? blob : nullptr); }
};

VVA_OKUNUSED inline VStream &operator << (VStream &strm, VNTValue &val) { val.Serialise(strm); return strm; }
VVA_OKUNUSED inline VStream &operator << (VStream &strm, const VNTValue &val) { val.Serialise(strm); return strm; }


// ////////////////////////////////////////////////////////////////////////// //
// doesn't own srcstream
// note that reading the same field more than once is not guaranteed, it is UB
// also, having more than one field with the same name is not allowed, it is UB
class VNTValueReader {
private:
  struct ValInfo {
    VName name;
    vuint8 type;
    vint32 ofs; // after `ReadTypeName()`
  };

private:
  VStream *srcStream;
  vint32 valleft; // number of unread values left
  TArray<ValInfo> vlist; // usually we don't have enough values to justify hashtable
  int strmendofs; // 0: unknown; otherwise: after last read value
  bool bError;

private:
  void setError ();

  // returns invalid value on error
  // `T_Invalid` `vtype` means "don't coerce"
  VNTValue coerceTo (VNTValue v, vuint8 vtype);

public:
  // doesn't own passed stream; starts reading from current stream position
  VNTValueReader (VStream *ASrcStream);
  ~VNTValueReader ();

  VNTValueReader (const VNTValueReader &) = delete;
  VNTValueReader &operator = (const VNTValueReader &) = delete;

  inline bool IsError () const { return bError; }

  // if `vtype` is not `T_Invalid`, returns invalid VNTValue if it is not possible to convert
  // returns invalid VNTValue if not found
  VNTValue readValue (VName vname, vuint8 vtype=VNTValue::T_Invalid);

  vint32 readInt (VName vname, bool *notfound=nullptr);
  float readFloat (VName vname, bool *notfound=nullptr);
  TVec readVec (VName vname, bool *notfound=nullptr);
  VName readName (VName vname, bool *notfound=nullptr);
  VStr readStr (VName vname, bool *notfound=nullptr);
  VClass *readClass (VName vname, bool *notfound=nullptr);
  VObject *readObj (VName vname, bool *notfound=nullptr);
  VSerialisable *readXObj (VName vname, bool *notfound=nullptr);
};


// ////////////////////////////////////////////////////////////////////////// //
class VNTValueWriter {
private:
  VStream *strm;
  TArray<VNTValue> vlist; // usually we don't have enough values to justify hashtable

private:
  void WriteTo (VStream &strm);

public:
  VNTValueWriter (VStream *astrm);
  ~VNTValueWriter ();

  VNTValueWriter (const VNTValueWriter &) = delete;
  VNTValueWriter &operator = (const VNTValueWriter &) = delete;

  // returns `true` if value was replaced
  bool putValue (const VNTValue &v);
};


// ////////////////////////////////////////////////////////////////////////// //
// reads or writes named values
class VNTValueIO {
protected:
  VNTValueReader *rd;
  VNTValueWriter *wr;
  bool bError;

public:
  VNTValueIO ();
  VNTValueIO (VStream *astrm);
  virtual ~VNTValueIO ();

  virtual void setup (VStream *astrm);

  bool IsError ();
  inline bool IsLoading () const { return !!rd; }

  virtual void iodef (VName vname, vint32 &v, vint32 defval);

  virtual void io (VName vname, vint32 &v);
  virtual void io (VName vname, vuint32 &v);
  virtual void io (VName vname, float &v);
  virtual void io (VName vname, TVec &v);
  virtual void io (VName vname, VName &v);
  virtual void io (VName vname, VStr &v);
  virtual void io (VName vname, VClass *&v);
  virtual void io (VName vname, VObject *&v);
  virtual void io (VName vname, VSerialisable *&v);

  //TODO: find a good way to work with blobs
  virtual VNTValue readBlob (VName vname);
  virtual void writeBlob (VName vname, const void *buf, int len);
};
