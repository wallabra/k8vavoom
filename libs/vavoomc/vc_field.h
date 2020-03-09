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
//**  Copyright (C) 2018-2020 Ketmar Dark
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

//==========================================================================
//
//  Field flags
//
//==========================================================================
enum {
  FIELD_Native    = 0x0001, // native serialisation
  FIELD_Transient = 0x0002, // not to be saved
  FIELD_Private   = 0x0004, // private field
  FIELD_ReadOnly  = 0x0008, // read-only field
  FIELD_Net       = 0x0010, // network replicated field
  FIELD_Repnotify = 0x0020, // your C++ code should call `void ReplicatedEvent (name fldname)` after getting a new value
  FIELD_Protected = 0x0100,
  FIELD_Published = 0x0200, // this is "published" field that can be found with RTTI
  FIELD_Internal  = 0x4000, // this field should not be initialized when creating new object, nor copied on object copy
};


//==========================================================================
//
//  VField
//
//==========================================================================
class VField : public VMemberBase {
public:
  // persistent fields
  VField *Next;
  VFieldType Type;
  VMethod *Func;
  vuint32 Flags;
  VMethod *ReplCond;
  VStr Description; // field description

  // compiler fields
  VExpression *TypeExpr;

  // run-time fields
  VField *NextReference; // linked list of reference fields
  VField *DestructorLink;
  VField *NextNetField;
  vint32 Ofs;
  vint32 NetIndex;

  VField (VName, VMemberBase *, TLocation);
  virtual ~VField () override;
  virtual void CompilerShutdown () override;

  virtual void Serialise (VStream &) override;
  bool NeedsDestructor () const;
  bool Define ();

  static void SerialiseFieldValue (VStream &, vuint8 *, const VFieldType &);
  static void SkipSerialisedType (VStream &);
  static void SkipSerialisedValue (VStream &);
  // if `vecprecise` is false, use 16 bits for coords and for angles (only for saving, loading is automatic)
  static bool NetSerialiseValue (VStream &Strm, VNetObjectsMapBase *Map, vuint8 *Data, const VFieldType &Type, bool vecprecise=true);
  static void CopyFieldValue (const vuint8 *Src, vuint8 *Dst, const VFieldType &);
  static bool CleanField (vuint8 *, const VFieldType &); // returns `true` if something was cleaned
  static void DestructField (vuint8 *, const VFieldType &, bool zeroIt=false);
  static bool NeedToDestructField (const VFieldType &);
  static bool NeedToCleanField (const VFieldType &);
  static bool IdenticalValue (const vuint8 *, const vuint8 *, const VFieldType &);

  friend inline VStream & operator << (VStream &Strm, VField *&Obj) { return Strm << *(VMemberBase **)&Obj; }

  inline vuint8 *GetFieldPtr (VObject *Obj) const noexcept { return (vuint8 *)Obj+Ofs; }

  inline float GetFloat (const VObject *Obj) const noexcept { return *(const float *)((const vuint8 *)Obj+Ofs); }
  inline TVec GetVec (const VObject *Obj) const noexcept { return *(const TVec *)((const vuint8 *)Obj+Ofs); }
  inline bool GetBool (const VObject *Obj) const noexcept { return !!(*(const vuint32 *)((const vuint8 *)Obj+Ofs)&Type.BitMask); }
  inline vint32 GetInt (const VObject *Obj) const noexcept { return *(const vint32 *)((const vuint8 *)Obj+Ofs); }
  inline vuint8 GetByte (const VObject *Obj) const noexcept { return *(const vuint8 *)((const vuint8 *)Obj+Ofs); }
  inline VName GetNameValue (const VObject *Obj) const noexcept { return *(const VName *)((const vuint8 *)Obj+Ofs); }
  inline VStr GetStr (const VObject *Obj) const noexcept { return *(const VStr *)((const vuint8 *)Obj+Ofs); }
  inline VClass *GetClassValue (const VObject *Obj) const noexcept { return *(VClass *const *)((const vuint8 *)Obj+Ofs); }
  inline VObject *GetObjectValue (const VObject *Obj) const noexcept { return *(VObject *const *)((const vuint8 *)Obj+Ofs); }

  inline void SetByte (VObject *Obj, vuint8 Value) const noexcept { *((vuint8 *)Obj+Ofs) = Value; }
  inline void SetInt (VObject *Obj, int Value) const noexcept { *(vint32 *)((vuint8 *)Obj+Ofs) = Value; }
  inline void SetInt (VObject *Obj, int Value, int Idx) const noexcept { ((vint32 *)((vuint8 *)Obj+Ofs))[Idx] = Value; }
  inline void SetFloat (VObject *Obj, float Value) const noexcept { *(float *)((vuint8 *)Obj+Ofs) = Value; }
  inline void SetFloat (VObject *Obj, float Value, int Idx) const noexcept { ((float *)((vuint8 *)Obj+Ofs))[Idx] = Value; }
  inline void SetNameValue (VObject *Obj, VName Value) const noexcept { *(VName *)((vuint8 *)Obj+Ofs) = Value; }
  inline void SetStr (VObject *Obj, VStr Value) const noexcept { *(VStr *)((vuint8 *)Obj+Ofs) = Value; }
  inline void SetBool (VObject *Obj, int Value) const noexcept {
    if (Value) {
      *(vuint32 *)((vuint8 *)Obj+Ofs) |= Type.BitMask;
    } else {
      *(vuint32 *)((vuint8 *)Obj+Ofs) &= ~Type.BitMask;
    }
  }
  inline void SetVec (VObject *Obj, const TVec &Value) const noexcept { *(TVec *)((vuint8 *)Obj+Ofs) = Value; }
  inline void SetClassValue (VObject *Obj, VClass *Value) const noexcept { *(VClass **)((vuint8 *)Obj+Ofs) = Value; }
  inline void SetObjectValue (VObject *Obj, VObject *Value) const noexcept { *(VObject **)((vuint8 *)Obj+Ofs) = Value; }
};
