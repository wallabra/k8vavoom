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

#include <signal.h>
#include <time.h>

#include "vcc_run.h"

#include "modules/mod_sound/sound.h"
#include "modules/mod_console.h"


// ////////////////////////////////////////////////////////////////////////// //
//#define DEBUG_OBJECT_LOADER


// ////////////////////////////////////////////////////////////////////////// //
VObject *mainObject = nullptr;
VStr appName;
bool compileOnly = false;
bool writeToConsole = true; //FIXME


// ////////////////////////////////////////////////////////////////////////// //
static VStr buildConfigName (const VStr &optfile) {
  for (int f = 0; f < optfile.length(); ++f) {
    char ch = optfile[f];
    if (ch >= '0' && ch <= '9') continue;
    if (ch >= 'A' && ch <= 'Z') continue;
    if (ch >= 'a' && ch <= 'z') continue;
    if (ch == '_' || ch == ' ' || ch == '.') continue;
    return VStr();
  }
  if (optfile.length()) {
    return VStr(".")+optfile+".cfg";
  } else {
    return VStr(".options.cfg");
  }
}


// ////////////////////////////////////////////////////////////////////////// //
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
      objarr[f] = VObject::StaticSpawnObject(c);
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
          *(VObject **)data = nullptr; // none
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
          skipping = true;
          if (!loadTypedData(nullptr, VFieldType())) return false;
          skipping = false;
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
        for (VClass *c = cls; c; c = c->ParentClass) {
          fld = findField(namearr[n], c->Fields);
          if (fld) break;
        }
        //FIXME: skip unknown fields
        if (!fld) {
          fprintf(stderr, "Object Loader: field `%s` is not found in class `%s`\n", *namearr[n], *cls->Name);
          skipping = true;
          if (!loadTypedData(nullptr, VFieldType())) return false;
          skipping = false;
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


// ////////////////////////////////////////////////////////////////////////// //
#if 0
// replicator
static void evalCondValues (VObject *obj, VClass *Class, vuint8 *values) {
  if (Class->GetSuperClass()) evalCondValues(obj, Class->GetSuperClass(), values);
  int len = Class->RepInfos.length();
  for (int i = 0; i < len; ++i) {
    P_PASS_REF(obj);
    vuint8 val = (VObject::ExecuteFunction(Class->RepInfos[i].Cond).i ? 1 : 0);
    int rflen = Class->RepInfos[i].RepFields.length();
    for (int j = 0; j < rflen; ++j) {
      if (Class->RepInfos[i].RepFields[j].Member->MemberType != MEMBER_Field) continue;
      values[((VField *)Class->RepInfos[i].RepFields[j].Member)->NetIndex] = val;
    }
  }
}


static vuint8 *createEvalConds (VObject *obj) {
  return new vuint8[obj->NumNetFields];
}


static vuint8 *createOldData (VClass *Class) {
  vuint8 *oldData = new vuint8[Class->ClassSize];
  memset(oldData, 0, Class->ClassSize);
  vuint8 *def = (vuint8 *)Class->Defaults;
  for (VField *F = Class->NetFields; F; F = F->NextNetField) {
    VField::CopyFieldValue(def+F->Ofs, oldData+F->Ofs, F->Type);
  }
}


static void deleteOldData (VClass *Class, vuint8 *oldData) {
  if (oldData) {
    for (VField *F = Class->NetFields; F; F = F->NextNetField) {
      VField::DestructField(oldData+F->Ofs, F->Type);
    }
    delete[] oldData;
}


static void replicateObj (VObject *obj, vuint8 *oldData) {
  // set up thinker flags that can be used by field condition
  //if (NewObj) Thinker->ThinkerFlags |= VThinker::TF_NetInitial;
  //if (Ent != nullptr && Ent->GetTopOwner() == Connection->Owner->MO) Thinker->ThinkerFlags |= VThinker::TF_NetOwner;

  auto condv = createEvalConds(obj);
  memset(condv, 0, obj->NumNetFields);
  evalCondValues(obj, obj->GetClass(), condv);

  vuint8 *data = (vuint8 *)obj;
  VObject *nullObj = nullptr;

  /*
  if (NewObj) {
    Msg.bOpen = true;
    VClass *TmpClass = Thinker->GetClass();
    Connection->ObjMap->SerialiseClass(Msg, TmpClass);
    NewObj = false;
  }
  */

  /*
  TAVec SavedAngles;
  if (Ent) {
    SavedAngles = Ent->Angles;
    if (Ent->EntityFlags & VEntity::EF_IsPlayer) {
      // clear look angles, because they must not affect model orientation
      Ent->Angles.pitch = 0;
      Ent->Angles.roll = 0;
    }
  } else {
    // shut up compiler warnings
    SavedAngles.yaw = 0;
    SavedAngles.pitch = 0;
    SavedAngles.roll = 0;
  }
  */

  for (VField *F = obj->GetClass()->NetFields; F; F = F->NextNetField) {
    if (!condv[F->NetIndex]) continue;

    // set up pointer to the value and do swapping for the role fields
    vuint8 *fieldData = data+F->Ofs;
    /*
         if (F == Connection->Context->RoleField) fieldData = data+Connection->Context->RemoteRoleField->Ofs;
    else if (F == Connection->Context->RemoteRoleField) fieldData = data+Connection->Context->RoleField->Ofs;
    */

    if (VField::IdenticalValue(fieldData, oldData+F->Ofs, F->Type)) continue;

    if (F->Type.Type == TYPE_Array) {
      VFieldType intrType = F->Type;
      intrType.Type = F->Type.ArrayInnerType;
      int innerSize = intrType.GetSize();
      for (int i = 0; i < F->Type.GetArrayDim(); ++i) {
        vuint8 *val = fieldData+i*innerSize;
        vuint8 *oldval = oldData+F->Ofs+i*innerSize;
        if (VField::IdenticalValue(val, oldval, intrType)) continue;
        // if it's an object reference that cannot be serialised, send it as nullptr reference
        if (intrType.Type == TYPE_Reference && !Connection->ObjMap->CanSerialiseObject(*(VObject **)val)) {
          if (!*(VObject **)oldval) continue; // already sent as nullptr
          val = (vuint8 *)&nullObj;
        }

        Msg.WriteInt(F->NetIndex, obj->GetClass()->NumNetFields);
        Msg.WriteInt(i, F->Type.GetArrayDim());
        if (VField::NetSerialiseValue(Msg, Connection->ObjMap, val, intrType)) {
          VField::CopyFieldValue(val, oldval, intrType);
        }
      }
    } else {
      // if it's an object reference that cannot be serialised, send it as nullptr reference
      if (F->Type.Type == TYPE_Reference && !Connection->ObjMap->CanSerialiseObject(*(VObject**)fieldData)) {
        if (!*(VObject **)(oldData+F->Ofs)) continue; // already sent as nullptr
        fieldData = (vuint8 *)&nullObj;
      }

      Msg.WriteInt(F->NetIndex, obj->GetClass()->NumNetFields);
      if (VField::NetSerialiseValue(Msg, Connection->ObjMap, fieldData, F->Type)) {
        VField::CopyFieldValue(fieldData, oldData + F->Ofs, F->Type);
      }
    }
  }

  if (Ent && (Ent->EntityFlags & VEntity::EF_IsPlayer)) Ent->Angles = SavedAngles;
  UpdatedThisFrame = true;

  if (Msg.GetNumBits()) SendMessage(&Msg);

  // clear temporary networking flags
  obj->ThinkerFlags &= ~VThinker::TF_NetInitial;
  obj->ThinkerFlags &= ~VThinker::TF_NetOwner;

  unguard;
}
#endif



// ////////////////////////////////////////////////////////////////////////// //
static bool onExecuteNetMethod (VObject *aself, VMethod *func) {
  /*
  if (GDemoRecordingContext) {
    // find initial version of the method
    VMethod *Base = func;
    while (Base->SuperMethod) Base = Base->SuperMethod;
    // execute it's replication condition method
    check(Base->ReplCond);
    P_PASS_REF(this);
    vuint32 SavedFlags = PlayerFlags;
    PlayerFlags &= ~VBasePlayer::PF_IsClient;
    bool ShouldSend = false;
    if (VObject::ExecuteFunction(Base->ReplCond).i) ShouldSend = true;
    PlayerFlags = SavedFlags;
    if (ShouldSend) {
      // replication condition is true, the method must be replicated
      GDemoRecordingContext->ClientConnections[0]->Channels[CHANIDX_Player]->SendRpc(func, this);
    }
  }
  */

  /*
#ifdef CLIENT
  if (GGameInfo->NetMode == NM_TitleMap ||
    GGameInfo->NetMode == NM_Standalone ||
    (GGameInfo->NetMode == NM_ListenServer && this == cl))
  {
    return false;
  }
#endif
  */

  // find initial version of the method
  VMethod *Base = func;
  while (Base->SuperMethod) Base = Base->SuperMethod;
  // execute it's replication condition method
  check(Base->ReplCond);
  P_PASS_REF(aself);
  if (!VObject::ExecuteFunction(Base->ReplCond).i) {
    //fprintf(stderr, "rpc call to `%s` (%s) is not done\n", aself->GetClass()->GetName(), *func->GetFullName());
    return false;
  }

  /*
  if (Net) {
    // replication condition is true, the method must be replicated
    Net->Channels[CHANIDX_Player]->SendRpc(func, this);
  }
  */

  // clean up parameters
  func->CleanupParams();

  fprintf(stderr, "rpc call to `%s` (%s) is DONE!\n", aself->GetClass()->GetName(), *func->GetFullName());

  // it's been handled here
  return true;
}


// ////////////////////////////////////////////////////////////////////////// //
__attribute__((noreturn, format(printf, 1, 2))) void Host_Error (const char *error, ...) {
  fprintf(stderr, "FATAL: ");
  va_list argPtr;
  va_start(argPtr, error);
  vfprintf(stderr, error, argPtr);
  va_end(argPtr);
  fprintf(stderr, "\n");
  exit(1);
}


static const char *comatoze (vuint32 n) {
  static char buf[128];
  int bpos = (int)sizeof(buf);
  buf[--bpos] = 0;
  int xcount = 0;
  do {
    if (xcount == 3) { buf[--bpos] = ','; xcount = 0; }
    buf[--bpos] = '0'+n%10;
    ++xcount;
  } while ((n /= 10) != 0);
  return &buf[bpos];
}


// ////////////////////////////////////////////////////////////////////////// //
void PR_WriteOne (const VFieldType &type) {
  char buf[128];
  size_t blen = 0;

  switch (type.Type) {
    case TYPE_Int: blen = snprintf(buf, sizeof(buf), "%d", PR_Pop()); break;
    case TYPE_Byte: blen = snprintf(buf, sizeof(buf), "%d", PR_Pop()); break;
    case TYPE_Bool: blen = snprintf(buf, sizeof(buf), "%s", (PR_Pop() ? "true" : "false")); break;
    case TYPE_Float: blen = snprintf(buf, sizeof(buf), "%f", PR_Popf()); break;
    case TYPE_Name: blen = snprintf(buf, sizeof(buf), "%s", *PR_PopName()); break;
    case TYPE_String:
      {
        VStr s = PR_PopStr();
        if (writeToConsole) VConsole::WriteStr(s); else printf("%s", *s);
      }
      return;
    case TYPE_Vector: { TVec v = PR_Popv(); blen = snprintf(buf, sizeof(buf), "(%f,%f,%f)", v.x, v.y, v.z); } break;
    case TYPE_Pointer: blen = snprintf(buf, sizeof(buf), "<%s>(%p)", *type.GetName(), PR_PopPtr()); break;
    case TYPE_Class: if (PR_PopPtr()) blen = snprintf(buf, sizeof(buf), "<%s>", *type.GetName()); else blen = snprintf(buf, sizeof(buf), "<none>"); break;
    case TYPE_State:
      {
        VState *st = (VState *)PR_PopPtr();
        if (st) {
          blen = snprintf(buf, sizeof(buf), "<state:%s %d %f>", *st->SpriteName, st->Frame, st->Time);
        } else {
          blen = snprintf(buf, sizeof(buf), "<state>");
        }
      }
      break;
    case TYPE_Reference: blen = snprintf(buf, sizeof(buf), "<%s>", (type.Class ? *type.Class->Name : "none")); break;
    case TYPE_Delegate: blen = snprintf(buf, sizeof(buf), "<%s:%p:%p>", *type.GetName(), PR_PopPtr(), PR_PopPtr()); break;
    case TYPE_Struct: PR_PopPtr(); blen = snprintf(buf, sizeof(buf), "<%s>", *type.Struct->Name); break;
    case TYPE_Array: PR_PopPtr(); blen = snprintf(buf, sizeof(buf), "<%s>", *type.GetName()); break;
    case TYPE_SliceArray: blen = snprintf(buf, sizeof(buf), "<%s:%d>", *type.GetName(), PR_Pop()); PR_PopPtr(); break;
    case TYPE_DynamicArray:
      {
        VScriptArray *a = (VScriptArray *)PR_PopPtr();
        blen = snprintf(buf, sizeof(buf), "%s(%d)", *type.GetName(), a->Num());
      }
      break;
    default: Sys_Error(va("Tried to print something strange: `%s`", *type.GetName()));
  }

  if (blen) {
    if (writeToConsole) VConsole::WriteStr(buf, blen); else printf("%s", buf);
  }
}


void PR_WriteFlush () {
  if (writeToConsole) VConsole::PutChar('\n'); else printf("\n");
}


// ////////////////////////////////////////////////////////////////////////// //
class VVccLog : public VLogListener {
public:
  virtual void Serialise (const char* text, EName event) override {
    dprintf("%s", text);
  }
};


// ////////////////////////////////////////////////////////////////////////// //
static VStr SourceFileName;
static TArray<VStr> scriptArgs;

static int num_dump_asm;
static const char *dump_asm_names[1024];
static bool DebugMode = false;
static FILE *DebugFile;

static VLexer Lex;
static VVccLog VccLog;


//==========================================================================
//
//  dprintf
//
//==========================================================================
__attribute__((format(printf, 1, 2))) int dprintf (const char *text, ...) {
  if (!DebugMode) return 0;

  va_list argPtr;
  FILE* fp = stderr; //(DebugFile ? DebugFile : stdout);
  va_start(argPtr, text);
  int ret = vfprintf(fp, text, argPtr);
  va_end(argPtr);
  fflush(fp);
  return ret;
}


//==========================================================================
//
//  Malloc
//
//==========================================================================
void* Malloc (size_t size) {
  if (!size) return nullptr;
  void *ptr = Z_Malloc(size);
  if (!ptr) FatalError("FATAL: couldn't alloc %d bytes", (int)size);
  memset(ptr, 0, size);
  return ptr;
}


//==========================================================================
//
//  Free
//
//==========================================================================
void Free (void* ptr) {
  if (ptr) Z_Free(ptr);
}


//==========================================================================
//
//  OpenFile
//
//==========================================================================
VStream* OpenFile (const VStr& Name) {
  return fsysOpenFile(Name);
  /*
  FILE* file = fopen(*Name, "rb");
  return (file ? new VFileReader(file) : nullptr);
  */
}


//==========================================================================
//
//  OpenDebugFile
//
//==========================================================================
static void OpenDebugFile (const VStr& name) {
  DebugFile = fopen(*name, "w");
  if (!DebugFile) FatalError("FATAL: can\'t open debug file \"%s\".", *name);
}


//==========================================================================
//
//  PC_DumpAsm
//
//==========================================================================
static void PC_DumpAsm (const char* name) {
  char buf[1024];
  char *cname;
  char *fname;

  snprintf(buf, sizeof(buf), "%s", name);

  //FIXME! PATH WITH DOTS!
  if (strstr(buf, ".")) {
    cname = buf;
    fname = strstr(buf, ".")+1;
    fname[-1] = 0;
  } else {
    dprintf("Dump ASM: Bad name %s\n", name);
    return;
  }

  //printf("<%s>.<%s>\n", cname, fname);

  for (int i = 0; i < VMemberBase::GMembers.Num(); ++i) {
    //if (VMemberBase::GMembers[i]->MemberType == MEMBER_Method) printf("O:<%s>; N:<%s>\n", *VMemberBase::GMembers[i]->Outer->Name, *VMemberBase::GMembers[i]->Name);
    if (VMemberBase::GMembers[i]->MemberType == MEMBER_Method &&
        !VStr::Cmp(cname, *VMemberBase::GMembers[i]->Outer->Name) &&
        !VStr::Cmp(fname, *VMemberBase::GMembers[i]->Name))
    {
      ((VMethod*)VMemberBase::GMembers[i])->DumpAsm();
      return;
    }
  }

  dprintf("Dump ASM: %s not found!\n", name);
}


//==========================================================================
//
//  DumpAsm
//
//==========================================================================
static void DumpAsm () {
  for (int i = 0; i < num_dump_asm; ++i) PC_DumpAsm(dump_asm_names[i]);
}


//==========================================================================
//
//  DisplayUsage
//
//==========================================================================
static void DisplayUsage () {
  printf("\n");
  printf("VCC Version 1.%d. Copyright (c) 2000-2001 by JL, 2018 by Ketmar Dark. (" __DATE__ " " __TIME__ "; opcodes: %d)\n", PROG_VERSION, NUM_OPCODES);
  printf("Usage: vcc [options] source[.c] [object[.dat]]\n");
  printf("    -d<file>     Output debugging information into specified file\n");
  printf("    -a<function> Output function's ASM statements into debug file\n");
  printf("    -D<name>           Define macro\n");
  printf("    -I<directory>      Include files directory\n");
  printf("    -P<directory>      Package import files directory\n");
  printf("    -base <directory>  Set base directory\n");
  printf("    -file <name>       Add pak file\n");
  exit(1);
}


//==========================================================================
//
//  ProcessArgs
//
//==========================================================================
static void ProcessArgs (int ArgCount, char **ArgVector) {
  int count = 0; // number of file arguments
  bool nomore = false;

  TArray<VStr> paklist;

  for (int i = 1; i < ArgCount; ++i) {
    const char *text = ArgVector[i];
    if (count == 0 && !nomore && *text == '-') {
      ++text;
      if (*text == 0) DisplayUsage();
      if (text[0] == '-' && text[1] == 0) { nomore = true; continue; }
      const char option = *text++;
      switch (option) {
        case 'd': DebugMode = true; if (*text) OpenDebugFile(text); break;
        case 'c': compileOnly = true; break;
        case 'a': /*if (!*text) DisplayUsage(); dump_asm_names[num_dump_asm++] = text;*/ VMemberBase::doAsmDump = true; break;
        case 'I': VMemberBase::StaticAddIncludePath(text); break;
        case 'D': VMemberBase::StaticAddDefine(text); break;
        case 'P': VMemberBase::StaticAddPackagePath(text); break;
        default:
          --text;
          if (VStr::Cmp(text, "nocol") == 0) {
            vcErrorIncludeCol = false;
          } else if (VStr::Cmp(text, "base") == 0) {
            ++i;
            if (i >= ArgCount) DisplayUsage();
            fsysBaseDir = VStr(ArgVector[i]);
          } else if (VStr::Cmp(text, "file") == 0) {
            ++i;
            if (i >= ArgCount) DisplayUsage();
            paklist.append(VStr(":")+VStr(ArgVector[i]));
            //fprintf(stderr, "<%s>\n", ArgVector[i]);
          } else if (VStr::Cmp(text, "pakdir") == 0) {
            ++i;
            if (i >= ArgCount) DisplayUsage();
            paklist.append(VStr("/")+VStr(ArgVector[i]));
            //fprintf(stderr, "<%s>\n", ArgVector[i]);
          } else {
            //fprintf(stderr, "*<%s>\n", text);
            DisplayUsage();
          }
          break;
      }
      continue;
    }
    ++count;
    switch (count) {
      case 1: SourceFileName = VStr(text).DefaultExtension(".vc"); break;
      default: scriptArgs.Append(VStr(text)); break;
    }
  }

  if (count == 0) DisplayUsage();

  /*
  if (!DebugFile) {
    VStr DbgFileName;
    DbgFileName = ObjectFileName.StripExtension()+".txt";
    OpenDebugFile(DbgFileName);
    DebugMode = true;
  }
  */

  fsysInit();
  for (int f = 0; f < paklist.length(); ++f) {
    VStr pname = paklist[f];
    if (pname.length() < 2) continue;
    char type = pname[0];
    pname.chopLeft(1);
    if (type == ':') {
      if (fsysAppendPak(pname)) {
        dprintf("added pak file '%s'...\n", *pname);
      } else {
        fprintf(stderr, "CAN'T add pak file '%s'!\n", *pname);
      }
    } else if (type == '/') {
      if (fsysAppendDir(pname)) {
        dprintf("added pak directory '%s'...\n", *pname);
      } else {
        fprintf(stderr, "CAN'T add pak directory '%s'!\n", *pname);
      }
    }
  }

  SourceFileName = SourceFileName.fixSlashes();
  dprintf("Main source file: %s\n", *SourceFileName);
}


//==========================================================================
//
//  initialize
//
//==========================================================================
static void initialize () {
  DebugMode = false;
  DebugFile = nullptr;
  num_dump_asm = 0;
  VName::StaticInit();
  //VMemberBase::StaticInit();
  VObject::StaticInit();
  VMemberBase::StaticAddDefine("VCC_STANDALONE_EXECUTOR");
#ifdef VCCRUN_HAS_SDL
  VMemberBase::StaticAddDefine("VCCRUN_HAS_SDL");
#endif
#ifdef VCCRUN_HAS_OPENGL
  VMemberBase::StaticAddDefine("VCCRUN_HAS_OPENGL");
#endif
#ifdef VCCRUN_HAS_OPENAL
  VMemberBase::StaticAddDefine("VCCRUN_HAS_OPENAL");
#endif
  VMemberBase::StaticAddDefine("VCCRUN_HAS_IMAGO");
  VObject::onExecuteNetMethodCB = &onExecuteNetMethod;
}


// ////////////////////////////////////////////////////////////////////////// //
// <0: error; bit 0: has arg; bit 1: returns int
static int checkArg (VMethod *mmain) {
  if (!mmain) return -1;
  if ((mmain->Flags&FUNC_VarArgs) != 0) return -1;
  //if (mmain->NumParams > 0 && mmain->ParamFlags[0] != 0) return -1;
  int res = 0;
  if (mmain->ReturnType.Type != TYPE_Void && mmain->ReturnType.Type != TYPE_Int) return -1;
  if (mmain->ReturnType.Type == TYPE_Int) res |= 0x02;
  if (mmain->NumParams != 0) {
    if (mmain->NumParams != 1) return -1;
    if (mmain->ParamFlags[0] == 0) {
      VFieldType atp = mmain->ParamTypes[0];
      dprintf("  ptype0: %s\n", *atp.GetName());
      if (atp.Type != TYPE_Pointer) return -1;
      atp = atp.GetPointerInnerType();
      if (atp.Type != TYPE_DynamicArray) return -1;
      atp = atp.GetArrayInnerType();
      if (atp.Type != TYPE_String) return -1;
      res |= 0x01;
    } else if ((mmain->ParamFlags[0]&(FPARM_Out|FPARM_Ref)) != 0) {
      VFieldType atp = mmain->ParamTypes[0];
      dprintf("  ptype1: %s\n", *atp.GetName());
      if (atp.Type != TYPE_DynamicArray) return -1;
      atp = atp.GetArrayInnerType();
      if (atp.Type != TYPE_String) return -1;
      res |= 0x01;
    }
  }
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
/*
#include <SDL.h>
#include <GL/gl.h>

void boo () {
  int width = 800;
  int height = 600;

  Uint32 flags = SDL_WINDOW_OPENGL;

#if 0
  //k8: require OpenGL 1.5
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);

  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  //SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, r_vsync);
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
  //SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#else
  //k8: require OpenGL 2.1, sorry; non-shader renderer was removed anyway
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  //SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, r_vsync);
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#endif

  glGetError();

  auto hw_window = SDL_CreateWindow("tesT", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, flags);
  if (!hw_window) {
#ifndef WIN32
    fprintf(stderr, "ALAS: cannot create SDL2 window.\n");
#endif
    return;
  }

  auto hw_glctx = SDL_GL_CreateContext(hw_window);
  if (!hw_glctx) {
    SDL_DestroyWindow(hw_window);
    hw_window = nullptr;
#ifndef WIN32
    fprintf(stderr, "ALAS: cannot create SDL2 OpenGL context.\n");
#endif
    return;
  }

  SDL_GL_MakeCurrent(hw_window, hw_glctx);
  glGetError();

#if !defined(WIN32)
  {
    int ghi, glo;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &ghi);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &glo);
    fprintf(stderr, "OpenGL version: %d.%d\n", ghi, glo);

    int ltmp = 666;
    SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &ltmp); fprintf(stderr, "STENCIL BUFFER BITS: %d\n", ltmp);
    SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &ltmp); fprintf(stderr, "RED BITS: %d\n", ltmp);
    SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &ltmp); fprintf(stderr, "GREEN BITS: %d\n", ltmp);
    SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &ltmp); fprintf(stderr, "BLUE BITS: %d\n", ltmp);
    SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &ltmp); fprintf(stderr, "DEPTH BITS: %d\n", ltmp);
  }
#endif

  //SDL_GL_MakeCurrent(hw_window, hw_glctx);
  SDL_GL_MakeCurrent(hw_window, nullptr);
  SDL_GL_DeleteContext(hw_glctx);
  SDL_DestroyWindow(hw_window);
}
*/

// ////////////////////////////////////////////////////////////////////////// //
int main (int argc, char **argv) {
  //boo();

  VStack ret;
  ret.i = 0;

  srand(time(nullptr));

#if 0
  {
    TArray<VStr> pts;
    VStr s = "/this/is/path/";
    s.SplitPath(pts);
    for (int f = 0; f < pts.length(); ++f) printf("  %d: <%s>:%d\n", f, *pts[f], pts[f].length());
    printf("<%s>\n", *(s.ToUpper()));
    printf("<%s>\n", *(s.ToLower()));
    printf("<%s>\n", *(s.ToUpper().ToLower()));
    printf("<%s>\n", *s);
  }
#endif

  try {
    GLog.AddListener(&VccLog);

    int starttime;
    int endtime;

    starttime = time(0);

    initialize();

    ProcessArgs(argc, argv);

    /*{
      auto fl = fsysOpenFile("text/model");
      auto sz = fl->TotalSize();
      printf("SIZE: %d (%d)\n", sz, fl->Tell());
      auto buf = new char[sz];
      fl->Serialize(buf, sz);
      delete fl;
      auto fo = fsysOpenDiskFileWrite("_000.ini");
      fo->Serialize(buf, sz);
      delete fo;
    }*/

    /*{
      VStr s = fsysFileFindAnyExt("a.boo");
      fprintf(stderr, "%s\n", *s);
    }*/

    PR_Init();

    VMemberBase::StaticLoadPackage(VName("engine"), TLocation());
    //VMemberBase::StaticLoadPackage(VName("ui"), TLocation());

    VPackage *CurrentPackage = new VPackage(VName("vccrun"));

    dprintf("Compiling '%s'...\n", *SourceFileName);

    VStream *strm = OpenFile(SourceFileName);
    if (!strm) {
      FatalError("FATAL: cannot open file '%s'", *SourceFileName);
    }

    CurrentPackage->LoadSourceObject(strm, SourceFileName, TLocation());
    dprintf("Total memory used: %u\n", VExpression::TotalMemoryUsed);
    DumpAsm();
    endtime = time(0);
    dprintf("Time elapsed: %02d:%02d\n", (endtime-starttime)/60, (endtime-starttime)%60);
    // free compiler memory
    VMemberBase::StaticCompilerShutdown();
    dprintf("Peak compiler memory usage: %s bytes.\n", comatoze(VExpression::PeakMemoryUsed));
    dprintf("Released compiler memory  : %s bytes.\n", comatoze(VExpression::TotalMemoryFreed));
    if (VExpression::CurrMemoryUsed != 0) {
      dprintf("Compiler leaks %s bytes (this is harmless).\n", comatoze(VExpression::CurrMemoryUsed));
    }

    VScriptArray scargs(scriptArgs);
    VClass *mklass = VClass::FindClass("Main");
    if (mklass && !compileOnly) {
      dprintf("Found class 'Main'\n");
      VMethod *mmain = mklass->FindAccessibleMethod("main");
      if (mmain) {
        dprintf(" Found method 'main()' (return type: %u:%s)\n", mmain->ReturnType.Type, *mmain->ReturnType.GetName());
        int atp = checkArg(mmain);
        if (atp < 0) FatalError("Main::main() should be either arg-less, or have one `array!string*` argument, and should be either `void`, or return `int`!");
        auto sss = pr_stackPtr;
        mainObject = VObject::StaticSpawnObject(mklass);
        if ((mmain->Flags&FUNC_Static) == 0) {
          //auto imain = Spawn<VLevel>();
          P_PASS_REF((VObject *)mainObject);
        }
        if (atp&0x01) P_PASS_REF(&scargs);
        ret = VObject::ExecuteFunction(mmain);
        if ((atp&0x02) == 0) ret.i = 0;
        if (sss != pr_stackPtr) FatalError("FATAL: stack imbalance!");
      }
    }

    VSoundManager::StaticShutdown();
    VObject::StaticExit();
    VName::StaticExit();
  } catch (VException& e) {
    ret.i = -1;
    FatalError("FATAL: %s", e.What());
  }

  return ret.i;
}


// ////////////////////////////////////////////////////////////////////////// //
//native static final int fsysAppendDir (string path, optional string pfx);
IMPLEMENT_FUNCTION(VObject, fsysAppendDir) {
  P_GET_STR_OPT(pfx, VStr());
  P_GET_STR(fname);
  //fprintf(stderr, "pakid(%d)=%d; fname=<%s>\n", (int)specified_pakid, pakid, *fname);
  if (specified_pfx) {
    RET_INT(fsysAppendDir(fname, pfx));
  } else {
    RET_INT(fsysAppendDir(fname));
  }
}


// append archive to the list of archives
// it will be searched in the current dir, and then in `fsysBaseDir`
// returns pack id or 0
//native static final int fsysAppendPak (string fname, optional int pakid);
IMPLEMENT_FUNCTION(VObject, fsysAppendPak) {
  P_GET_INT_OPT(pakid, -1);
  P_GET_STR(fname);
  //fprintf(stderr, "pakid(%d)=%d; fname=<%s>\n", (int)specified_pakid, pakid, *fname);
  if (specified_pakid) {
    RET_INT(fsysAppendPak(fname, pakid));
  } else {
    RET_INT(fsysAppendPak(fname));
  }
}

// remove given pack from pack list
//native static final void fsysRemovePak (int pakid);
IMPLEMENT_FUNCTION(VObject, fsysRemovePak) {
  P_GET_INT(pakid);
  fsysRemovePak(pakid);
}

// remove all packs from pakid and later
//native static final void fsysRemovePaksFrom (int pakid);
IMPLEMENT_FUNCTION(VObject, fsysRemovePaksFrom) {
  P_GET_INT(pakid);
  fsysRemovePaksFrom(pakid);
}

// 0: no such pack
//native static final int fsysFindPakByPrefix (string pfx);
IMPLEMENT_FUNCTION(VObject, fsysFindPakByPrefix) {
  P_GET_STR(pfx);
  RET_BOOL(fsysFindPakByPrefix(pfx));
}

//native static final bool fsysFileExists (string fname, optional int pakid);
IMPLEMENT_FUNCTION(VObject, fsysFileExists) {
  P_GET_INT_OPT(pakid, -1);
  P_GET_STR(fname);
  if (specified_pakid) {
    RET_BOOL(fsysFileExists(fname, pakid));
  } else {
    RET_BOOL(fsysFileExists(fname));
  }
}

// find file with any extension
//native static final string fsysFileFindAnyExt (string fname, optional int pakid);
IMPLEMENT_FUNCTION(VObject, fsysFileFindAnyExt) {
  P_GET_INT_OPT(pakid, -1);
  P_GET_STR(fname);
  if (specified_pakid) {
    RET_STR(fsysFileFindAnyExt(fname, pakid));
  } else {
    RET_STR(fsysFileFindAnyExt(fname));
  }
}


// return pack file path for the given pack id (or empty string)
//native static final string fsysGetPakPath (int pakid);
IMPLEMENT_FUNCTION(VObject, fsysGetPakPath) {
  P_GET_INT(pakid);
  RET_STR(fsysGetPakPath(pakid));
}

// return pack prefix for the given pack id (or empty string)
//native static final string fsysGetPakPrefix (int pakid);
IMPLEMENT_FUNCTION(VObject, fsysGetPakPrefix) {
  P_GET_INT(pakid);
  RET_STR(fsysGetPakPrefix(pakid));
}


//native static final int fsysGetLastPakId ();
IMPLEMENT_FUNCTION(VObject, fsysGetLastPakId) {
  RET_INT(fsysGetLastPakId());
}


IMPLEMENT_FUNCTION(VObject, get_fsysKillCommonZipPrefix) {
  RET_BOOL(fsysKillCommonZipPrefix);
}

IMPLEMENT_FUNCTION(VObject, set_fsysKillCommonZipPrefix) {
  P_GET_BOOL(v);
  fsysKillCommonZipPrefix = v;
}


// native final void appSetName (string appname);
IMPLEMENT_FUNCTION(VObject, appSetName) {
  P_GET_STR(aname);
  appName = aname;
}


//native final bool appSaveOptions (Object optobj, optional string optfile);
IMPLEMENT_FUNCTION(VObject, appSaveOptions) {
  P_GET_STR_OPT(optfile, VStr());
  P_GET_REF(VObject, optobj);
  if (appName.isEmpty() || !optobj) { RET_BOOL(false); return; }
  if (optobj->GetClass()->Name == NAME_Object) { RET_BOOL(false); return; }
  ObjectSaveMap svmap(optobj);
  if (svmap.wasError) { RET_BOOL(false); return; }
  auto fname = buildConfigName(optfile);
  if (fname.isEmpty()) { RET_BOOL(false); return; }
  auto strm = fsysOpenDiskFileWrite(fname);
  if (!strm) { RET_BOOL(false); return; }
  ObjectSaver saver(*strm, svmap);
  bool res = saver.saveAll();
  delete strm;
  RET_BOOL(res && !saver.IsError());
}


//native final spawner Object appLoadOptions (class cls, optional string optfile);
IMPLEMENT_FUNCTION(VObject, appLoadOptions) {
  P_GET_STR_OPT(optfile, VStr());
  P_GET_PTR(VClass, cls);
  if (!cls) { RET_REF(nullptr); return; }
  if (cls->Name == NAME_Object) { RET_REF(nullptr); return; }
  if (appName.isEmpty()) { RET_REF(nullptr); return; }
  auto fname = buildConfigName(optfile);
  if (fname.isEmpty()) { RET_REF(nullptr); return; }
  auto strm = fsysOpenDiskFile(fname);
  if (!strm) { RET_REF(nullptr); return; }
  ObjectLoader ldr(*strm, cls);
  if (!ldr.loadAll()) {
    delete strm;
    ldr.clear();
    RET_REF(nullptr);
    return;
  }
  delete strm;
  //fprintf(stderr, "%p\n", ldr.objarr[1]);
  RET_REF(ldr.objarr[1]);
}
