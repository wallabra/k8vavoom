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
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
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
class VObject;
class VMethod;


// ////////////////////////////////////////////////////////////////////////// //
enum EType {
  TYPE_Void,
  TYPE_Int,
  TYPE_Byte,
  TYPE_Bool,
  TYPE_Float,
  TYPE_Name,
  TYPE_String,
  TYPE_Pointer,
  TYPE_Reference,
  TYPE_Class,
  TYPE_State,
  TYPE_Delegate,
  TYPE_Struct,
  TYPE_Vector,
  TYPE_Array,
  TYPE_DynamicArray,
  TYPE_SliceArray, // array consisting of pointer and length, with immutable length
  TYPE_Unknown,
  TYPE_Automatic, // this is valid only for variable declarations, and will be resolved to actual type

  NUM_BASIC_TYPES
};


// ////////////////////////////////////////////////////////////////////////// //
class VFieldType {
public:
  vuint8 Type;
  vuint8 InnerType; // for pointers
  vuint8 ArrayInnerType; // for arrays
  vuint8 PtrLevel;
  // you should never access `ArrayDimInternal` directly!
  // sign bit is used to mark "2-dim array"
  vint32 ArrayDimInternal;
  union {
    vuint32 BitMask;
    VClass *Class; // class of the reference
    VStruct *Struct; // struct data
    VMethod *Function; // function of the delegate type
  };

  VFieldType ();
  VFieldType (EType Atype);
  explicit VFieldType (VClass *InClass);
  explicit VFieldType (VStruct *InStruct);

  friend VStream &operator << (VStream &, VFieldType &);

  bool Equals (const VFieldType &) const;

  // used for `==` and `!=` -- allow substructs and subclasses
  bool IsCompatiblePointerRelaxed (const VFieldType &) const;

  VFieldType MakePointerType () const;
  VFieldType GetPointerInnerType () const;
  VFieldType MakeArrayType (int, const TLocation &) const;
  VFieldType MakeArray2DType (int d0, int d1, const TLocation &l) const;
  VFieldType MakeDynamicArrayType (const TLocation &) const;
  VFieldType MakeSliceType (const TLocation &) const;
  VFieldType GetArrayInnerType () const;

  // this is used in VM, don't touch it
  inline void SetArrayDimIntr (vint32 v) { ArrayDimInternal = v; }

  inline bool IsArray1D () const { return (ArrayDimInternal >= 0); }
  inline bool IsArray2D () const { return (ArrayDimInternal < 0); }
  // get 1d array dim (for 2d arrays this will be correctly calculated)
  inline vint32 GetArrayDim () const { return (ArrayDimInternal >= 0 ? ArrayDimInternal : GetFirstDim()*GetSecondDim()); }
  // get first dimension (or the only one for 1d array)
  inline vint32 GetFirstDim () const { return (ArrayDimInternal >= 0 ? ArrayDimInternal : ArrayDimInternal&0x7fff); }
  // get second dimension (or 1 for 1d array)
  inline vint32 GetSecondDim () const { return (ArrayDimInternal >= 0 ? 1 : (ArrayDimInternal>>16)&0x7fff); }

  int GetStackSize () const;
  int GetSize () const;
  int GetAlignment () const;
  bool CheckPassable (const TLocation &, bool raiseError=true) const;
  bool CheckReturnable (const TLocation &, bool raiseError=true) const;
  bool CheckMatch (bool asRef, const TLocation &loc, const VFieldType &, bool raiseError=true) const;
  VStr GetName () const;

  bool IsAnyArray () const;
  inline bool IsPointer () const { return (Type == TYPE_Pointer); } // useless, but nice
  inline bool IsVoidPointer () const { return (Type == TYPE_Pointer && PtrLevel == 1 && InnerType == TYPE_Void); }

  bool IsReusingDisabled () const;
  bool IsReplacableWith (const VFieldType &atype) const;
};


// ////////////////////////////////////////////////////////////////////////// //
struct VObjectDelegate {
  VObject *Obj;
  VMethod *Func;
};


// ////////////////////////////////////////////////////////////////////////// //
// dynamic array object, used in script executor
class VScriptArray {
private:
  int ArrNum; // if bit 31 is set, this is 1st dim of 2d array
  int ArrSize; // if bit 31 is set in `ArrNum`, this is 2nd dim of 2d array
  vuint8 *ArrData;

public:
  VScriptArray (const TArray<VStr> &xarr);

  inline int Num () const { return (ArrNum >= 0 ? ArrNum : (ArrNum&0x7fffffff)*(ArrSize&0x7fffffff)); }
  inline int length () const { return (ArrNum >= 0 ? ArrNum : (ArrNum&0x7fffffff)*(ArrSize&0x7fffffff)); }
  inline int length1 () const { return (ArrNum&0x7fffffff); }
  inline int length2 () const { return (ArrNum >= 0 ? (ArrNum ? 1 : 0) : (ArrSize&0x7fffffff)); }
  inline vuint8 *Ptr () { return ArrData; }
  inline bool Is2D () const { return (ArrNum < 0); }
  inline void Flatten () { if (Is2D()) { vint32 oldlen = length(); ArrSize = ArrNum = oldlen; } }
  void Clear (const VFieldType &Type);
  void Reset (const VFieldType &Type); // clear array, but don't resize
  void Resize (int NewSize, const VFieldType &Type);
  void SetNum (int NewNum, const VFieldType &Type, bool doShrink=true); // will convert to 1d
  void SetNumMinus (int NewNum, const VFieldType &Type);
  void SetNumPlus (int NewNum, const VFieldType &Type);
  void Insert (int Index, int Count, const VFieldType &Type);
  void Remove (int Index, int Count, const VFieldType &Type);
  void SetSize2D (int dim1, int dim2, const VFieldType &Type);

  void SwapElements (int i0, int i1, const VFieldType &Type);
  int CallCompare (int i0, int i1, const VFieldType &Type, VObject *self, VMethod *fnless);
  // only for flat arrays
  bool Sort (const VFieldType &Type, VObject *self, VMethod *fnless);

  static int CallComparePtr (void *p0, void *p1, const VFieldType &Type, VObject *self, VMethod *fnless);
};

// required for VaVoom C VM
static_assert(sizeof(VScriptArray) <= sizeof(void *)*3, "oops");


// ////////////////////////////////////////////////////////////////////////// //
struct FReplacedString {
  int Index;
  bool Replaced;
  VStr Old;
  VStr New;
};


// ////////////////////////////////////////////////////////////////////////// //
// `ExecuteFunction()` returns this
// we cannot return arrays
// returning pointers is not implemented yet (except VClass and VObject)
struct VFuncRes {
private:
  EType type;
  bool pointer;
  /*const*/ void *ptr;
  int ival; // and bool
  float fval;
  TVec vval;
  VStr sval;
  VName nval;

public:
  VFuncRes () : type(TYPE_Void), pointer(false), ptr(nullptr), ival(0), fval(0), vval(0, 0, 0), sval(), nval(NAME_None) {}
  VFuncRes (const VFuncRes &b) : type(TYPE_Void), pointer(false), ptr(nullptr), ival(0), fval(0), vval(0, 0, 0), sval(), nval(NAME_None) { *this = b; }

  VFuncRes (bool v) : type(TYPE_Int), pointer(false), ptr(nullptr), ival(v ? 1 : 0), fval(v ? 1 : 0), vval(0, 0, 0), sval(), nval(NAME_None) {}

  VFuncRes (int v) : type(TYPE_Int), pointer(false), ptr(nullptr), ival(v), fval(v), vval(0, 0, 0), sval(), nval(NAME_None) {}
  //VFuncRes (const int *v) : type(TYPE_Int), pointer(true), ptr(v), ival(v ? *v : 0), fval(v ? float(*v) : 0.0f), vval(0, 0, 0), sval(), nval(NAME_None) {}

  VFuncRes (float v) : type(TYPE_Float), pointer(false), ptr(nullptr), ival(v), fval(v), vval(0, 0, 0), sval(), nval(NAME_None) {}
  //VFuncRes (const float *v) : type(TYPE_Float), pointer(true), ptr(v), ival(*v ? int(v) : 0), fval(v ? *v : 0.0f), vval(0, 0, 0), sval(), nval(NAME_None) {}

  VFuncRes (const TVec &v) : type(TYPE_Vector), pointer(false), ptr(nullptr), ival(0), fval(0), vval(v.x, v.y, v.z), sval(), nval(NAME_None) {}
  //VFuncRes (const TVec *&v) : type(TYPE_Vector), pointer(true), ptr(v), ival(0), fval(0), vval(v ? v->x : 0.0f, v ? v->y : 0.0f, v ? v->z : 0.0f), sval(), nval(NAME_None) {}
  VFuncRes (const float x, const float y, const float z) : type(TYPE_Vector), pointer(false), ptr(nullptr), ival(0), fval(0), vval(x, y, z), sval(), nval(NAME_None) {}

  VFuncRes (const VStr &v) : type(TYPE_String), pointer(false), ptr(nullptr), ival(0), fval(0), vval(0, 0, 0), sval(v), nval(NAME_None) {}
  //VFuncRes (const VStr *&v) : type(TYPE_String), pointer(true), ptr(v), ival(0), fval(0), vval(0, 0, 0), sval(v ? *v : VStr::EmptyString), nval(NAME_None) {}

  VFuncRes (const VName &v) : type(TYPE_Name), pointer(false), ptr(nullptr), ival(0), fval(0), vval(0, 0, 0), sval(), nval(v) {}
  //VFuncRes (const VName *&v) : type(TYPE_Name), pointer(true), ptr(v), ival(0), fval(0), vval(0, 0, 0), sval(), nval() { if (v) nval = VName(*v); else nval = NAME_None; }

  VFuncRes (VClass *v) : type(TYPE_Class), pointer(false), ptr(v), ival(0), fval(0), vval(0, 0, 0), sval(), nval(NAME_None) {}
  VFuncRes (VObject *v) : type(TYPE_Reference), pointer(false), ptr(v), ival(0), fval(0), vval(0, 0, 0), sval(), nval(NAME_None) {}
  VFuncRes (VState *v) : type(TYPE_State), pointer(false), ptr(v), ival(0), fval(0), vval(0, 0, 0), sval(), nval(NAME_None) {}

  inline VFuncRes &operator = (const VFuncRes &b) {
    if (&b == this) return *this;
    type = b.type;
    pointer = b.pointer;
    ptr = b.ptr;
    ival = b.ival;
    fval = b.fval;
    vval = b.vval;
    sval = b.sval;
    nval = b.nval;
    return *this;
  }

  inline EType getType () const { return type; }
  inline bool isPointer () const { return pointer; }

  inline bool isVoid () const { return (type == TYPE_Void); }

  inline bool isNumber () const { return (type == TYPE_Int || type == TYPE_Float); }
  inline bool isInt () const { return (type == TYPE_Int); }
  inline bool isFloat () const { return (type == TYPE_Float); }
  inline bool isVector () const { return (type == TYPE_Vector); }
  inline bool isStr () const { return (type == TYPE_String); }
  inline bool isName () const { return (type == TYPE_Name); }
  inline bool isClass () const { return (type == TYPE_Class); }
  inline bool isObject () const { return (type == TYPE_Reference); }
  inline bool isState () const { return (type == TYPE_State); }

  inline const void *getPtr () const { return ptr; }
  inline VClass *getClass () const { return (VClass *)ptr; }
  inline VObject *getObject () const { return (VObject *)ptr; }
  inline VState *getState () const { return (VState *)ptr; }

  inline int getInt () const { return ival; }
  inline float getFloat () const { return fval; }
  inline const TVec &getVector () const { return vval; }
  inline const VStr &getStr () const { return sval; }
  inline VName getName () const { return nval; }

  inline bool getBool () const {
    switch (type) {
      case TYPE_Int: return (ival != 0);
      case TYPE_Float: return (fval != 0);
      case TYPE_Vector: return (vval.x != 0 && vval.y != 0 && vval.z != 0);
      case TYPE_String: return (sval.length() != 0);
      case TYPE_Name: return (nval != NAME_None);
      default: break;
    }
    return false;
  }

  inline operator bool () const { return getBool(); }
};
