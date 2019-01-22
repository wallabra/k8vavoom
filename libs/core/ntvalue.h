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
//**
//**  typed and named field
//**
//**************************************************************************
class VClass;

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
  vuint8 *blob; // (blob-sizeof(vint32) is refcounter
  vint32 blobSize;

private:
  inline void incRef () const {
    if (blob) {
      vuint32 *rc = (vuint32 *)(blob-sizeof(vuint32));
      if (*rc != 0xffffffffU) ++(*rc);
    }
  }

  inline void decRef () {
    if (blob) {
      vuint32 *rc = (vuint32 *)(blob-sizeof(vuint32));
      if (*rc != 0xffffffffU) {
        if (--(*rc) == 0) {
          Z_Free(blob);
        }
      }
    }
    blob = nullptr;
    blobSize = 0;
  }

public:
  VNTValue () { memset((void *)this, 0, sizeof(VNTValue)); }
  ~VNTValue () { clear(); }

  VNTValue (const VNTValue &s) {
    memset((void *)this, 0, sizeof(VNTValue));
    type = s.type;
    name = s.name;
    switch (s.type) {
      case T_Vec: vval = s.vval; break;
      case T_Name: nval = s.nval; break;
      case T_Str: sval = s.sval; break;
      case T_Blob: blob = (vuint8 *)s.blob; blobSize = s.blobSize; incRef(); break;
      default: memcpy((void *)&ud, &s.ud, sizeof(ud)); break;
    }
  }

  VNTValue (VName aname, vint32 val) : vval(0, 0, 0), nval(NAME_None), sval() { memset((void *)this, 0, sizeof(VNTValue)); type = T_Int; name = aname; ud.ival = val; }
  VNTValue (VName aname, vuint32 val) : vval(0, 0, 0), nval(NAME_None), sval() { memset((void *)this, 0, sizeof(VNTValue)); type = T_Int; name = aname; ud.ival = (vint32)val; }
  VNTValue (VName aname, float val) : vval(0, 0, 0), nval(NAME_None), sval() { memset((void *)this, 0, sizeof(VNTValue)); type = T_Float; name = aname; ud.fval = val; }
  VNTValue (VName aname, const TVec &val) : vval(0, 0, 0), nval(NAME_None), sval() { memset((void *)this, 0, sizeof(VNTValue)); type = T_Vec; name = aname; vval = val; }
  VNTValue (VName aname, VName val) : vval(0, 0, 0), nval(NAME_None), sval() { memset((void *)this, 0, sizeof(VNTValue)); type = T_Name; name = aname; nval = val; }
  VNTValue (VName aname, const VStr &val) : vval(0, 0, 0), nval(NAME_None), sval() { memset((void *)this, 0, sizeof(VNTValue)); type = T_Str; name = aname; sval = val; }
  VNTValue (VName aname, VClass *val) : vval(0, 0, 0), nval(NAME_None), sval() { memset((void *)this, 0, sizeof(VNTValue)); type = T_Class; name = aname; ud.cval = (VMemberBase *)val; }
  VNTValue (VName aname, VObject *val) : vval(0, 0, 0), nval(NAME_None), sval() { memset((void *)this, 0, sizeof(VNTValue)); type = T_Obj; name = aname; ud.oval = val; }
  VNTValue (VName aname, VSerialisable *val) : vval(0, 0, 0), nval(NAME_None), sval() { memset((void *)this, 0, sizeof(VNTValue)); type = T_XObj; name = aname; ud.xoval = val; }

  VNTValue (VName aname, const vuint8 *buf, int bufsz, bool doCopyData=false);

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
      case T_Blob: blob = s.blob; blobSize = s.blobSize; incRef(); break;
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
    memset((void *)this, 0, sizeof(VNTValue));
  }

  void Serialise (VStream &strm);
  static void SkipSerialised (VStream &strm);

  // checkers
  inline bool isValid () const { return (type > T_Invalid && type <= T_Blob); }
  inline bool isInt () const { return (type == T_Int); }
  inline bool isFloat () const { return (type == T_Float); }
  inline bool isVec () const { return (type == T_Vec); }
  inline bool isName () const { return (type == T_Name); }
  inline bool isStr () const { return (type == T_Str); }
  inline bool isClass () const { return (type == T_Class); }
  inline bool isObj () const { return (type == T_Obj); }
  inline bool isXObj () const { return (type == T_XObj); }
  inline bool isBlob () const { return (type == T_Blob); }

  // getters and setters
  inline VName getName () const { return name; }
  inline vuint8 getType () const { return type; }

  inline vint32 getInt () const { return (type == T_Int ? ud.ival : 0); }
  inline float getFloat () const { return (type == T_Float ? ud.fval : 0); }
  inline TVec getVec () const { return (type == T_Vec ? vval : TVec(0, 0, 0)); }
  inline VName getVName () const { return (type == T_Name ? nval : VName(NAME_None)); }
  inline VStr getStr () const { return (type == T_Str ? sval : VStr::EmptyString); }
  inline VClass *getClass () const { return (type == T_Class ? (VClass *)ud.cval : nullptr); }
  inline VObject *getObj () const { return (type == T_Obj ? (VObject *)ud.oval : nullptr); }
  inline VSerialisable *getXObj () const { return (type == T_XObj ? (VSerialisable *)ud.xoval : nullptr); }

  inline vint32 getBlobSize () const { return (type == T_Blob ? blobSize : 0); }
  inline const vuint8 *getBlobPtr () const { return (type == T_Blob ? blob : nullptr); }
};
