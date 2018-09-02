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
//**  Copyright (C) 2018 Ketmar Dark
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
class ObjectSaveMap;
class ObjectSaveChecker;
class ObjectSaver;


// ////////////////////////////////////////////////////////////////////////// //
class ObjectSaveChecker {
public:
  TMap<VClass *, bool> classmap;
  TMap<VStruct *, bool> structmap;
  TMap<VField *, bool> fieldmap;

  // saveable class either has no fields, or all fields are saveable
  static bool isSkip (VClass *cls, bool checkName) {
    if (!cls) return true;
    if (checkName && cls->Name == NAME_None) return true;
    if (cls->ClassFlags&(CLASS_Transient/*|CLASS_Abstract*/)) return true;
    return false;
  }

  static bool isSkip (VField *fld) {
    if (!fld || fld->Name == NAME_None) return true;
    if (fld->Flags&(FIELD_Transient|FIELD_ReadOnly)) return true;
    return false;
  }

  bool canIO (const VFieldType &type) {
    switch (type.Type) {
      case TYPE_Int:
      case TYPE_Byte:
      case TYPE_Bool:
      case TYPE_Float:
      case TYPE_Name:
      case TYPE_String:
      case TYPE_Vector:
      case TYPE_Class:
        return true;
      case TYPE_Reference:
        if (isSkip(type.Class, true)) return true;
        return canIO(type.Class, false);
      case TYPE_Struct:
        return canIO(type.Struct);
      case TYPE_Array:
      case TYPE_DynamicArray:
        return canIO(type.GetArrayInnerType());
    }
    return false;
  }

  bool canIO (VField *fld) {
    if (isSkip(fld)) return true;
    auto ck = fieldmap.find(fld);
    if (ck) return *ck;
    fieldmap.put(fld, true); // while we are checking
    bool res = canIO(fld->Type);
    if (!res) fieldmap.put(fld, false);
    return res;
  }

  // saveable struct either has no fields, or all fields are saveable
  bool canIO (VStruct *st) {
    if (!st || st->Name == NAME_None) return true;
    auto ck = structmap.find(st);
    if (ck) return *ck;
    structmap.put(st, true); // while we are checking
    for (VStruct *cst = st; cst; cst = cst->ParentStruct) {
      for (VField *fld = cst->Fields; fld; fld = fld->Next) {
        if (isSkip(fld)) continue;
        if (!canIO(fld)) {
          structmap.put(st, false);
          return false;
        }
      }
    }
    return true;
  }

  // saveable class either has no fields, or all fields are saveable
  bool canIO (VClass *cls, bool ignoreFlags) {
    if (!cls || cls->Name == NAME_None) return true;
    if (!ignoreFlags && isSkip(cls, true)) return true;
    auto ck = classmap.find(cls);
    if (ck) return *ck;
    classmap.put(cls, true);
    for (VClass *c = cls; c; c = c->ParentClass) {
      if (c != cls && isSkip(c, false)) break; // transient class breaks chain
      auto ck2 = classmap.find(c);
      if (ck2) return *ck2;
      classmap.put(c, true);
      for (VField *fld = c->Fields; fld; fld = fld->Next) {
        if (isSkip(fld)) continue;
        if (!canIO(fld)) {
          classmap.put(c, false);
          classmap.put(cls, false);
          return false;
        }
      }
    }
    return true;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
static const char *BinStorageSignature = "VaVoom C Binary Data Storage v0"; // 32 bytes

// build map of objects, strings and names
class ObjectSaveMap {
public:
  TMap<VObject *, vuint32> objmap;
  TArray<VObject *> objarr;
  TMap<VName, vuint32> namemap;
  TArray<VName> namearr;
  TMapDtor<VStr, vuint32> strmap;
  TArray<VStr> strarr;
  vuint32 objCount;
  vuint32 nameCount;
  vuint32 strCount;
  bool wasError;
  ObjectSaveChecker ck;

public:
  ObjectSaveMap (VObject *ao)
    : objmap()
    , objarr()
    , namemap()
    , namearr()
    , strmap()
    , strarr()
    , objCount(0)
    , nameCount(0)
    , strCount(0)
    , wasError(false)
  {
    buildSaveMap(ao);
  }

  inline bool IsError () const { return wasError; }

  VObject *getByIndex (vuint32 idx) {
    if (idx < 1 || idx > objCount) return nullptr;
    return objarr[(vint32)idx-1];
  }

  vuint32 getObjectId (VObject *o) {
    if (!o) return 0;
    auto idp = objmap.find(o);
    if (!idp) FatalError("tried to save unregistered object (%s)", *o->GetClass()->Name);
    return *idp;
  }

  vuint32 getNameId (VName n) {
    if (n == NAME_None) return 0;
    auto idp = namemap.find(n);
    if (!idp) {
      for (int f = 0; f < namearr.length(); ++f) {
        if (namearr[f] == n) abort();
      }
      FatalError("tried to save unregistered name '%s'", *n);
    }
    return *idp;
  }

  vuint32 getStrId (const VStr &s) {
    if (s.isEmpty()) return 0;
    auto idp = strmap.find(s);
    if (!idp) FatalError("tried to save unregistered string");
    return *idp;
  }

  bool saveMap (VStream &strm) {
    if (wasError || strm.IsError() || strm.IsLoading()) return false;
    // signature
    if (strlen(BinStorageSignature) != 31) return false;
    strm.Serialise(BinStorageSignature, 32);
    // counters
    strm << STRM_INDEX_U(objCount);
    strm << STRM_INDEX_U(nameCount);
    strm << STRM_INDEX_U(strCount);
    if (strm.IsError()) return false;
    // names
    for (int f = 0; f < namearr.length(); ++f) {
      VStr s = *namearr[f];
      if (s.isEmpty()) return false;
      strm << s;
      if (strm.IsError()) return false;
    }
    // strings
    for (int f = 0; f < strarr.length(); ++f) {
      VStr s = strarr[f];
      if (s.isEmpty()) return false;
      strm << s;
      if (strm.IsError()) return false;
    }
    // object classes
    for (int f = 0; f < objarr.length(); ++f) {
      VObject *o = objarr[f];
      if (!o) return false;
      vuint32 n = getNameId(o->GetClass()->Name);
      strm << STRM_INDEX_U(n);
    }
    return !strm.IsError();
  }

private:
  void buildSaveMap (VObject *o) {
    if (!o) return;
    processObject(o, true);
  }

  void putName (VName n) {
    if (n == NAME_None || namemap.has(n)) return;
    if ((vint32)nameCount != namearr.length()) FatalError("oops: putName");
    //fprintf(stderr, "PN: <%s>\n", *n);
    namemap.put(n, ++nameCount);
    namearr.append(n);
  }

  void putStr (const VStr &s) {
    if (s.isEmpty() || strmap.has(s)) return;
    if ((vint32)strCount != strarr.length()) FatalError("oops: putStr");
    strmap.put(s, ++strCount);
    strarr.append(s);
  }

  void putObject (VObject *obj) {
    if (!obj) FatalError("oops(0): putObject");
    if ((vint32)objCount != objarr.length()) FatalError("oops(1): putObject");
    //fprintf(stderr, "object %p of class `%s`\n", obj, *obj->GetClass()->Name);
    objmap.put(obj, ++objCount);
    objarr.append(obj);
    putName(obj->GetClass()->Name);
  }

  void processValue (vuint8 *data, const VFieldType &type) {
    if (!ck.canIO(type)) {
      //fprintf(stderr, "FUCKED type '%s'\n", *type.GetName());
      wasError = true;
      return;
    }
    VObject *o;
    VClass *c;
    switch (type.Type) {
      case TYPE_Name:
        putName(*(*(VName *)data));
        break;
      case TYPE_String:
        putStr(*(VStr *)data);
        break;
      case TYPE_Reference:
        // save desired class name
        if (type.Class) putName(type.Class->Name);
        o = *(VObject **)data;
        if (o) processObject(o);
        break;
      case TYPE_Class:
        c = *(VClass **)data;
        if (c) putName(c->Name);
        break;
      case TYPE_Struct:
        // save desired struct name
        if (ck.canIO(type.Struct)) {
          putName(type.Struct->Name);
          processStruct(data, type.Struct);
        } else {
          wasError = true;
        }
        break;
      case TYPE_Array:
        if (ck.canIO(type.GetArrayInnerType())) {
          VFieldType intType = type;
          intType.Type = type.ArrayInnerType;
          vint32 innerSize = intType.GetSize();
          vint32 dim = type.GetArrayDim();
          for (int f = 0; f < dim; ++f) {
            processValue(data+f*innerSize, intType);
            if (wasError) break;
          }
        } else {
          wasError = true;
        }
        break;
      case TYPE_DynamicArray:
        if (ck.canIO(type.GetArrayInnerType())) {
          VScriptArray *a = (VScriptArray *)data;
          VFieldType intType = type;
          intType.Type = type.ArrayInnerType;
          vint32 innerSize = intType.GetSize();
          for (int f = 0; f < a->length(); ++f) {
            processValue(a->Ptr()+f*innerSize, intType);
            if (wasError) break;
          }
        } else {
          wasError = true;
        }
        break;
    }
  }

  void processFields (vuint8 *data, VField *fields) {
    for (VField *fld = fields; fld; fld = fld->Next) {
      if (ck.isSkip(fld)) continue;
      if (ck.canIO(fld)) {
        putName(fld->Name);
        processValue(data+fld->Ofs, fld->Type);
      } else {
        wasError = false;
        break;
      }
    }
  }

  void processStruct (vuint8 *data, VStruct *st) {
    if (!ck.canIO(st)) { wasError = true; return; }
    putName(st->Name);
    for (VStruct *cst = st; cst; cst = cst->ParentStruct) {
      //putName(cst->Name); // why not?
      processFields(data, cst->Fields);
      if (wasError) break;
    }
  }

  void processObject (VObject *obj, bool ignoreFlags=false) {
    if (!obj) { wasError = true; return; }
    VClass *cls = obj->GetClass();
    //if (!obj->GetClass()->IsChildOf(cls)) { wasError = true; return; }
    if (!obj || !ck.canIO(cls, ignoreFlags)) {
      //fprintf(stderr, "CANNOT save class '%s'\n", *cls->Name);
      wasError = true;
      return;
    }
    // check for cycles
    if (objmap.has(obj)) {
      //fprintf(stderr, "DUP object '%s' (%p)\n", *cls->Name, obj);
      return;
    }
    //fprintf(stderr, "DOING object '%s' (%p)\n", *cls->Name, obj);
    // mark as processed
    putObject(obj);
    // do this class and superclasses
    for (VClass *c = cls; c; c = c->ParentClass) {
      //putName(c->Name); // why not?
      if (c != cls && ck.isSkip(c, false)) break; // transient class breaks chain
      processFields((vuint8 *)obj, c->Fields);
      if (wasError) break;
    }
  }
};


// ////////////////////////////////////////////////////////////////////////// //
class ObjectSaver {
public:
  VStream &strm;
  ObjectSaveMap &smap;

public:
  ObjectSaver (VStream &astrm, ObjectSaveMap &asmap) : strm(astrm), smap(asmap) {}

  inline bool IsError () const { return (smap.wasError || strm.IsError()); }

  bool saveAll () {
    if (strm.IsLoading()) { smap.wasError = true; return false; }
    if (!smap.saveMap(strm)) { smap.wasError = true; return false; }
    // objects
    for (int f = 0; f < smap.objarr.length(); ++f) {
      VObject *o = smap.objarr[f];
      if (!o) return false;
      if (!saveIt(o, true)) return false;
    }
    return !strm.IsError();
  }

private:
  bool countSaveableFields (vuint32 &fcount, VField *fields) {
    for (VField *fld = fields; fld; fld = fld->Next) {
      if (smap.ck.isSkip(fld)) continue;
      if (!smap.ck.canIO(fld)) return false;
      ++fcount;
    }
    return true;
  }

  bool saveFields (vuint8 *data, VField *fields) {
    // save fields
    for (VField *fld = fields; fld; fld = fld->Next) {
      if (smap.ck.isSkip(fld)) continue;
      if (!smap.ck.canIO(fld)) return false;
      vuint32 n = smap.getNameId(fld->Name);
      strm << STRM_INDEX_U(n);
      if (strm.IsError()) return false;
      if (!saveIt(data+fld->Ofs, fld->Type)) return false;
    }
    return !strm.IsError();
  }

  bool saveIt (vuint8 *data, const VFieldType &type) {
    vuint8 typetag = (vuint8)type.Type;
    vuint32 u32;
    vuint8 u8;
    VObject *o;
    VClass *c;
    strm << typetag;
    switch (type.Type) {
      case TYPE_Int:
        strm << STRM_INDEX(*(vint32 *)data);
        break;
      case TYPE_Byte:
        strm << *(vuint8 *)data;
        break;
      case TYPE_Bool:
        //strm << STRM_INDEX(type.BitMask);
        if (type.BitMask == 0) {
          u8 = ((*(vuint32 *)data) != 0 ? 1 : 0);
        } else {
          u8 = (((*(vuint32 *)data)&type.BitMask) != 0 ? 1 : 0);
        }
        strm << u8;
        break;
      case TYPE_Float:
        strm << *(float *)data;
        break;
      case TYPE_Name:
        u32 = smap.getNameId(*(VName *)data);
        strm << STRM_INDEX_U(u32);
        break;
      case TYPE_String:
        u32 = smap.getStrId(*(VStr *)data);
        strm << STRM_INDEX_U(u32);
        break;
      case TYPE_Reference:
        o = *(VObject **)data;
        if (o) {
          // class to cast
          u32 = smap.getNameId(type.Class->Name);
          strm << STRM_INDEX_U(u32);
          if (strm.IsError()) return false;
          u32 = smap.getObjectId(o);
          strm << STRM_INDEX_U(u32);
        } else {
          u32 = smap.getNameId(NAME_None);
          strm << STRM_INDEX_U(u32);
        }
        break;
      case TYPE_Class:
        c = *(VClass **)data;
        u32 = smap.getNameId(c ? c->Name : NAME_None);
        strm << STRM_INDEX_U(u32);
        break;
      case TYPE_Struct:
        if (!smap.ck.canIO(type.Struct)) return false;
        return saveIt(data, type.Struct);
      case TYPE_Vector:
        strm << *(float *)data;
        strm << *(float *)(data+sizeof(float));
        strm << *(float *)(data+sizeof(float)*2);
        break;
      case TYPE_Array:
        if (smap.ck.canIO(type.GetArrayInnerType())) {
          VFieldType intType = type;
          intType.Type = type.ArrayInnerType;
          vint32 innerSize = intType.GetSize();
          vint32 dim = type.GetArrayDim();
          vint8 ittag = (vint8)intType.Type;
          strm << STRM_INDEX_U(ittag);
          strm << STRM_INDEX_U(innerSize);
          strm << STRM_INDEX_U(dim);
          if (strm.IsError()) return false;
          for (int f = 0; f < dim; ++f) {
            if (!saveIt(data+f*innerSize, intType)) return false;
          }
        } else {
          return false;
        }
        break;
      case TYPE_DynamicArray:
        if (smap.ck.canIO(type.GetArrayInnerType())) {
          VScriptArray *a = (VScriptArray *)data;
          VFieldType intType = type;
          intType.Type = type.ArrayInnerType;
          vint32 innerSize = intType.GetSize();
          vint32 d = a->length();
          vuint8 is2d = (a->Is2D() ? 1 : 0);
          vuint8 ittag = (vuint8)intType.Type;
          strm << STRM_INDEX_U(is2d);
          strm << STRM_INDEX_U(ittag);
          strm << STRM_INDEX_U(innerSize);
          if (is2d) {
            vuint32 d1 = a->length1();
            vuint32 d2 = a->length2();
            strm << STRM_INDEX_U(d1);
            strm << STRM_INDEX_U(d2);
          } else {
            strm << STRM_INDEX_U(d);
          }
          for (int f = 0; f < (int)d; ++f) {
            if (!saveIt(a->Ptr()+f*innerSize, intType)) return false;
          }
        } else {
          return false;
        }
        break;
      default:
        return false;
    }
    return !strm.IsError();
  }

  bool saveIt (vuint8 *data, VStruct *st) {
    if (!smap.ck.canIO(st)) return false;
    // struct name
    vuint32 nn = smap.getNameId(st->Name);
    strm << STRM_INDEX_U(nn);
    if (strm.IsError()) return false;
    // field count
    vuint32 fcount = 0;
    for (VStruct *cst = st; cst; cst = cst->ParentStruct) {
      if (!countSaveableFields(fcount, cst->Fields)) return false;
    }
    strm << STRM_INDEX_U(fcount);
    if (strm.IsError()) return false;
    // fields
    for (VStruct *cst = st; cst; cst = cst->ParentStruct) {
      if (!saveFields(data, cst->Fields)) return false;
    }
    return !strm.IsError();
  }

  bool saveIt (VObject *obj, bool ignoreFlags) {
    if (!obj || !smap.ck.canIO(obj->GetClass(), ignoreFlags)) return false;
    // object class
    VClass *cls = obj->GetClass();
    vuint32 nn = smap.getNameId(cls->Name);
    strm << STRM_INDEX_U(nn);
    if (strm.IsError()) return false;
    // field count
    vuint32 fcount = 0;
    for (VClass *c = cls; c; c = c->ParentClass) {
      if (c != cls && smap.ck.isSkip(c, false)) break; // transient class breaks chain
      if (!countSaveableFields(fcount, c->Fields)) return false;
    }
    strm << STRM_INDEX_U(fcount);
    // fields
    for (VClass *c = obj->GetClass(); c; c = c->ParentClass) {
      if (c != cls && smap.ck.isSkip(c, false)) break; // transient class breaks chain
      if (!saveFields((vuint8 *)obj, c->Fields)) continue;
    }
    return !strm.IsError();
  }
};


// ////////////////////////////////////////////////////////////////////////// //
class ObjectLoader {
public:
  VStream &strm;
  VClass *mainClass;
  TArray<VObject *> objarr;
  TArray<VName> namearr;
  TArray<VStr> strarr;
  bool skipping;

public:
  ObjectLoader (VStream &astrm, VClass *aMainClass)
    : strm(astrm)
    , mainClass(aMainClass)
    , objarr()
    , namearr()
    , strarr()
    , skipping(false)
  {}

  void clear () {
    for (int f = 0; f < objarr.length(); ++f) {
      if (objarr[f]) {
        objarr[f]->ConditionalDestroy();
        objarr[f] = nullptr;
      }
    }
  }

  bool loadAll () {
    if (!strm.IsLoading()) return false;
    if (strm.IsError() || !strm.IsLoading()) return false;
    // signature
    char sign[32];
    if (strlen(BinStorageSignature) != 31) return false;
    strm.Serialise(sign, 32);
    if (strm.IsError()) return false;
    if (memcmp(sign, BinStorageSignature, 32) != 0) return false;
    // counters
    vuint32 objCount;
    vuint32 nameCount;
    vuint32 strCount;
    strm << STRM_INDEX_U(objCount);
    strm << STRM_INDEX_U(nameCount);
    strm << STRM_INDEX_U(strCount);
    if (strm.IsError()) return false;
    if (objCount < 1 || nameCount < 1) return false;
    if (objCount > 1024*1024*32 || nameCount > 1024*1024*32 || strCount > 1024*1024*32) return false;
    // names
    namearr.setLength(nameCount+1);
    namearr[0] = NAME_None;
    for (int f = 1; f <= (vint32)nameCount; ++f) {
      VStr s;
      strm << s;
      if (strm.IsError()) return false;
      if (s.isEmpty()) return false;
      namearr[f] = VName(*s);
    }
    // strings
    strarr.setLength(strCount+1);
    strarr[0] = VStr::EmptyString;
    for (int f = 1; f <= (vint32)strCount; ++f) {
      VStr s;
      strm << s;
      if (strm.IsError()) return false;
      if (s.isEmpty()) return false;
      strarr[f] = s;
    }
    // create objects to load by their class names
    objarr.setLength(objCount+1);
    for (int f = 0; f < (vint32)objCount; ++f) objarr[f] = nullptr;
    for (int f = 1; f <= (vint32)objCount; ++f) {
      vuint32 n;
      strm << STRM_INDEX_U(n);
      if (n == 0 || n > nameCount || strm.IsError()) return false;
      VClass *c = VMemberBase::StaticFindClass(namearr[n]);
      if (!c) { fprintf(stderr, "Object Loader: class `%s` not found!\n", *namearr[n]); return false; }
      objarr[f] = VObject::StaticSpawnObject(c, true); // no replacement
      if (!objarr[f]) abort();
    }
    // objects
    for (int f = 1; f <= (vint32)objCount; ++f) {
      VObject *o = objarr[f];
      if (!o) return false;
      if (!loadObject(o)) return false;
    }
    return !strm.IsError();
  }

private:
  static VField *findField (VName aname, VField *fields) {
    for (VField *fld = fields; fld; fld = fld->Next) {
      if (fld->Name == aname) return fld;
    }
    return nullptr;
  }

  bool loadTypedData (vuint8 *data, const VFieldType &type) {
#ifdef DEBUG_OBJECT_LOADER
    fprintf(stderr, "loading type `%s` at 0x%08x...\n", *type.GetName(), (vuint32)strm.Tell());
#endif
    vuint8 typetag;
    strm << typetag;
    if (strm.IsError()) return false;
    if (!skipping) {
      if (typetag != type.Type) {
#ifdef DEBUG_OBJECT_LOADER
        fprintf(stderr, "  bad type tag (%u)\n", (vuint32)typetag);
#endif
        return false;
      }
    }
    float f32;
    vint32 i32;
    vuint32 u32;
    vuint8 u8;
    VClass *c;
    switch (typetag) {
      case TYPE_Int:
        strm << STRM_INDEX(i32);
        if (!skipping) *(vint32 *)data = i32;
        break;
      case TYPE_Byte:
        strm << u8;
        if (!skipping) *(vuint8 *)data = u8;
        break;
      case TYPE_Bool:
        strm << u8;
        if (strm.IsError()) return false;
        if (u8 != 0 && u8 != 1) return false;
        if (!skipping) {
          if (type.BitMask == 0) {
            *(vuint32 *)data = u8;
          } else {
            if (u8) {
              *(vuint32 *)data |= type.BitMask;
            } else {
              *(vuint32 *)data &= ~type.BitMask;
            }
          }
        }
        break;
      case TYPE_Float:
        strm << f32;
        if (!skipping) *(float *)data = f32;
        break;
      case TYPE_Name:
        strm << STRM_INDEX_U(u32);
        if (strm.IsError()) return false;
        if (u32 >= (vuint32)namearr.length()) return false;
        if (!skipping) *(VName *)data = namearr[u32];
        break;
      case TYPE_String:
        strm << STRM_INDEX_U(u32);
        if (strm.IsError()) return false;
        if (u32 >= (vuint32)strarr.length()) return false;
        if (!skipping) *(VStr *)data = strarr[u32];
        break;
      case TYPE_Reference:
        // class name
        strm << STRM_INDEX_U(u32);
        if (strm.IsError()) return false;
        if (u32 >= (vuint32)namearr.length()) return false;
        if (u32) {
          // class to cast
          if (!skipping) {
            c = VMemberBase::StaticFindClass(namearr[u32]);
            if (!c) { fprintf(stderr, "Object Loader: class `%s` not found\n", *namearr[u32]); return false; }
            if (!c->IsChildOf(type.Class)) { fprintf(stderr, "Object Loader: class `%s` is not a subclass of `%s`\n", *namearr[u32], *type.Class->Name); return false; }
            // object id
            strm << STRM_INDEX_U(u32);
            if (u32 >= (vuint32)objarr.length()) return false;
            *(VObject **)data = objarr[u32];
          } else {
            strm << STRM_INDEX_U(u32);
          }
        } else {
          if (!skipping) *(VObject **)data = nullptr; // none
        }
        break;
      case TYPE_Class:
        strm << STRM_INDEX_U(u32);
        if (strm.IsError()) return false;
        if (u32 >= (vuint32)namearr.length()) return false;
        if (!skipping) {
          if (u32) {
            c = VMemberBase::StaticFindClass(namearr[u32]);
            if (!c) { fprintf(stderr, "Object Loader: class `%s` not found\n", *namearr[u32]); return false; }
            if (!c->IsChildOf(type.Class)) { fprintf(stderr, "Object Loader: class `%s` is not a subclass of `%s`\n", *namearr[u32], *type.Class->Name); return false; }
            *(VClass **)data = c;
          } else {
            *(VClass **)data = nullptr; // none
          }
        }
        break;
      case TYPE_Struct:
        return loadStruct(data, type.Struct);
      case TYPE_Vector:
        if (!skipping) {
          strm << *(float *)data;
          strm << *(float *)(data+sizeof(float));
          strm << *(float *)(data+sizeof(float)*2);
        } else {
          strm << f32;
          strm << f32;
          strm << f32;
        }
        break;
      case TYPE_Array:
        {
          vint8 itype;
          vint32 innerSize, dim;
          strm << STRM_INDEX_U(itype);
          strm << STRM_INDEX_U(innerSize);
          strm << STRM_INDEX_U(dim);
          if (strm.IsError()) return false;
          if (!skipping) {
            VFieldType intType = type;
            intType.Type = type.ArrayInnerType;
            if (intType.Type != itype) return false;
            if (innerSize != intType.GetSize()) return false;
            if (dim > type.GetArrayDim()) return false;
            for (int f = 0; f < dim; ++f) {
              if (!loadTypedData(data+f*innerSize, intType)) return false;
            }
          } else {
            for (int f = 0; f < dim; ++f) {
              if (!loadTypedData(data, type)) return false;
            }
          }
        }
        break;
      case TYPE_DynamicArray:
        {
          vuint8 is2d, itype;
          vint32 innerSize = 0, dim = 0, d1 = 0, d2 = 0;
          strm << STRM_INDEX_U(is2d);
          strm << STRM_INDEX_U(itype);
          strm << STRM_INDEX_U(innerSize);
          if (is2d) {
            strm << STRM_INDEX_U(d1);
            strm << STRM_INDEX_U(d2);
          } else {
            strm << STRM_INDEX_U(dim);
          }
          if (strm.IsError()) return false;
          if (is2d) {
            if (d1 == 0 || d2 == 0 || d1 > 1024*1024*512 || d2 > 1024*1024*512) return false;
            if (1024*1024*512/d1 < d2) return false;
          } else {
            if (dim > 1024*1024*512) return false;
          }
          if (!skipping) {
            VScriptArray *a = (VScriptArray *)data;
            VFieldType intType = type;
            intType.Type = type.ArrayInnerType;
            if (intType.Type != itype) return false;
            if (innerSize != intType.GetSize()) return false;
            if (is2d) {
              a->SetSize2D((vint32)d1, (vint32)d2, intType);
            } else {
              a->SetNum((vint32)dim, intType);
            }
            for (int f = 0; f < a->length(); ++f) {
              if (!loadTypedData(a->Ptr()+f*innerSize, intType)) return false;
            }
          } else {
            if (is2d) dim = d1*d2;
            for (int f = 0; f < dim; ++f) {
              if (!loadTypedData(data, type)) return false;
            }
          }
        }
        break;
      default:
        return false;
    }
    return !strm.IsError();
  }

  bool loadStruct (vuint8 *data, VStruct *st) {
    if (!st) return false;
#ifdef DEBUG_OBJECT_LOADER
    fprintf(stderr, "loading struct `%s`...\n", *st->GetFullName());
#endif
    // struct name
    vuint32 nn;
    strm << STRM_INDEX_U(nn);
    if (strm.IsError()) return false;
    if (nn == 0 || nn >= (vuint32)namearr.length()) return false;
    if (!skipping) {
      bool found = false;
      for (VStruct *cst = st; cst; cst = cst->ParentStruct) {
        if (cst->Name == namearr[nn]) { found = true; break; }
      }
      if (!found) {
        fprintf(stderr, "Object Loader: tried to load struct `%s`, which is not a subset of `%s`\n", *namearr[nn], *st->Name);
        return false;
      }
    }
    // field count
    vuint32 fcount;
    strm << STRM_INDEX_U(fcount);
    if (strm.IsError()) return false;
    // fields
    while (fcount--) {
      vuint32 n;
      strm << STRM_INDEX_U(n);
      if (n == 0 || n >= (vuint32)namearr.length()) return false;
#ifdef DEBUG_OBJECT_LOADER
      fprintf(stderr, "  loading field `%s` in struct `%s`...\n", *namearr[n], *st->GetFullName());
#endif
      if (!skipping) {
        VField *fld = nullptr;
        for (VStruct *cst = st; cst; cst = cst->ParentStruct) {
          fld = findField(namearr[n], cst->Fields);
          if (fld) break;
        }
        //FIXME: skip unknown fields
        if (!fld) {
          fprintf(stderr, "Object Loader: field `%s` is not found in struct `%s`\n", *namearr[n], *st->GetFullName());
          auto oldskip = skipping;
          skipping = true;
          if (!loadTypedData(nullptr, VFieldType())) return false;
          skipping = oldskip;
        } else {
          if (!loadTypedData(data+fld->Ofs, fld->Type)) {
            fprintf(stderr, "Object Loader: failed to load field `%s` in struct `%s`\n", *namearr[n], *st->GetFullName());
            return false;
          }
        }
      } else {
        if (!loadTypedData(nullptr, VFieldType())) return false;
      }
    }
#ifdef DEBUG_OBJECT_LOADER
    fprintf(stderr, "DONE! struct `%s` (%d)\n", *st->GetFullName(), (int)strm.IsError());
#endif
    return !strm.IsError();
  }

  bool loadObject (VObject *obj) {
    if (!obj) return false;
    // object class
    VClass *cls = obj->GetClass();
#ifdef DEBUG_OBJECT_LOADER
    fprintf(stderr, "loading object of class `%s`...\n", *cls->Name);
#endif
    vuint32 nn;
    strm << STRM_INDEX_U(nn);
    if (strm.IsError()) return false;
    if (nn == 0 || nn >= (vuint32)namearr.length()) return false;
    if (!skipping) {
      VClass *c = VMemberBase::StaticFindClass(namearr[nn]);
      if (!c) { fprintf(stderr, "Object Loader: class `%s` not found\n", *namearr[nn]); return false; }
      if (!c->IsChildOf(cls)) { fprintf(stderr, "Object Loader: class `%s` is not a subclass of `%s`\n", *namearr[nn], *cls->Name); return false; }
      // field count
      vuint32 fcount;
      strm << STRM_INDEX_U(fcount);
      if (strm.IsError()) return false;
      // fields
      while (fcount--) {
        vuint32 n;
        strm << STRM_INDEX_U(n);
        if (n == 0 || n >= (vuint32)namearr.length()) return false;
        VField *fld = nullptr;
#ifdef DEBUG_OBJECT_LOADER
        fprintf(stderr, "  loading field `%s` in class `%s`...\n", *namearr[n], *cls->Name);
#endif
        for (VClass *ccl = cls; ccl; ccl = ccl->ParentClass) {
          fld = findField(namearr[n], ccl->Fields);
          if (fld) break;
        }
        //FIXME: skip unknown fields
        if (!fld) {
          fprintf(stderr, "Object Loader: field `%s` is not found in class `%s`\n", *namearr[n], *cls->Name);
          auto oldskip = skipping;
          skipping = true;
          if (!loadTypedData(nullptr, VFieldType())) return false;
          skipping = oldskip;
        } else {
          if (!loadTypedData((vuint8 *)obj+fld->Ofs, fld->Type)) {
            fprintf(stderr, "Object Loader: failed to load field `%s` in class `%s`\n", *namearr[n], *cls->Name);
            return false;
          }
        }
      }
#ifdef DEBUG_OBJECT_LOADER
      fprintf(stderr, "done loading object of class `%s`...\n", *cls->Name);
#endif
    } else {
      // field count
      vuint32 fcount;
      strm << STRM_INDEX_U(fcount);
      if (strm.IsError()) return false;
      while (fcount--) {
        vuint32 n;
        strm << STRM_INDEX_U(n);
        if (n == 0 || n >= (vuint32)namearr.length()) return false;
        if (!loadTypedData(nullptr, VFieldType())) return false;
      }
    }
    return !strm.IsError();
  }
};
