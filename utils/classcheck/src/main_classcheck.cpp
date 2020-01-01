//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2020 Ketmar Dark
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "libs/core.h"

#include "sempai.h"


// ////////////////////////////////////////////////////////////////////////// //
struct Field;

struct Type {
  VName name;
  bool shitpp;
  bool pointer;
  bool delegate;
  int dimension;
  Field *fields;
  Type *next; // for template main, this is first templated type; for templated types, this is link chain
  VName parentName;
  VStr srcfile;
  bool isClassDef;

  Type () : name(NAME_None), shitpp(false), pointer(false), delegate(false), dimension(0), fields(nullptr), next(nullptr), parentName(NAME_None), srcfile(), isClassDef(false) {}

  Type (const Type *src) : name(src->name), shitpp(src->shitpp), pointer(src->pointer), delegate(src->delegate), dimension(src->dimension), fields(src->fields), next(src->next), parentName(src->parentName), srcfile(src->srcfile), isClassDef(src->isClassDef) {}

  Type *clone () const { return new Type(*this); }

  inline bool isBool () const {
    return
      name == "bool" &&
      pointer == false &&
      dimension == 0 &&
      fields == nullptr &&
      next == nullptr &&
      parentName == NAME_None;
  }

  void appendField (Field *fld);

  Type *getParent () const;

  int fieldCount () const;
  Field *fieldAt (int idx) const;

  bool operator == (const Type &b) const;

  bool equal (const Type &b, bool allowFallback=true) const;

  void dumpFields () const;

  VStr toString () const;
};

struct Field {
  VName name;
  Type *type;
  Type *owner;
  Field *next;

  Field () : name(NAME_None), type(nullptr), owner(nullptr), next(nullptr) {}
};


// ////////////////////////////////////////////////////////////////////////// //
TMap<VName, Type *> shitppTypes;
TMap<VName, Type *> vcTypes;


//==========================================================================
//
//  findShitppType
//
//==========================================================================
Type *findShitppType (const Type *tp) {
  if (!tp) return nullptr;
  vassert(!tp->shitpp);
  vassert(tp->name != NAME_None);
  auto spp = shitppTypes.find(tp->name);
  if (spp) return *spp;
  // try mangled name
  VStr tname = VStr(tp->name);
  tname = VStr("V")+tname;
  VName xn = VName(*tname, VName::Find);
  if (xn != NAME_None) {
    spp = shitppTypes.find(xn);
    if (spp) return *spp;
  }
  if (tp->name == "AmbientSound") return nullptr;
  // another mangled name
  tname = VStr(tp->name);
  tname = VStr("F")+tname;
  xn = VName(*tname, VName::Find);
  if (xn == NAME_None) return nullptr;
  spp = shitppTypes.find(xn);
  if (spp) return *spp;
  return nullptr;
}


//==========================================================================
//
//  Type::toString
//
//==========================================================================
VStr Type::toString () const {
  VStr res = VStr(name);
  if (isClassDef) res = va("<cdef>%s", *res);
  if (next) {
    if (shitpp) {
      res += '<';
      bool first = true;
      for (Type *t = next; t; t = t->next) {
        if (first) first = false; else res += ", ";
        res += t->toString();
      }
      res += '>';
    } else {
      res += '!';
      if (next->next) res += '(';
      bool first = true;
      for (Type *t = next; t; t = t->next) {
        if (first) first = false; else res += ", ";
        res += t->toString();
      }
      if (next->next) res += ')';
    }
  }
  if (dimension) res += va("[%d]", dimension);
  if (pointer) res += " *";
  return res;
}


//==========================================================================
//
//  Type::appendField
//
//==========================================================================
void Type::appendField (Field *fld) {
  vassert(!fld->next);
  vassert(!fld->owner);
  fld->owner = this;
  if (!fields) { fields = fld; return; }
  Field *f = fields;
  while (f->next) f = f->next;
  f->next = fld;
}


//==========================================================================
//
//  Type::getParent
//
//==========================================================================
Type *Type::getParent () const {
  if (parentName == NAME_None) return nullptr;
  Type **pp = (shitpp ? shitppTypes.find(parentName) : vcTypes.find(parentName));
  if (pp) return *pp;
  Sys_Error("parent type `%s` for %s type `%s` not found!", *parentName, (shitpp ? "shitpp" : "VavoomC"), *name);
}


//==========================================================================
//
//  Type::fieldCount
//
//==========================================================================
int Type::fieldCount () const {
  int res = 0;
  for (const Field *f = fields; f; f = f->next) ++res;
  return res;
}


//==========================================================================
//
//  Type::fieldAt
//
//==========================================================================
Field *Type::fieldAt (int idx) const {
  if (idx < 0) return nullptr;
  for (Field *f = fields; f; f = f->next) {
    if (idx-- == 0) return f;
  }
  return nullptr;
}


//==========================================================================
//
//  Type::equal
//
//==========================================================================
bool Type::equal (const Type &b, bool allowFallback) const {
  if (&b == this) return true;
  // check for `void *`
  if (pointer && b.pointer) {
    if (name == "void" || b.name == "void") return true;
  }

  if (name != b.name) {
    //if (name == "ClipRect") abort();
    // check for class type
    if (allowFallback && !shitpp && b.shitpp) {
      if (!pointer && b.pointer) {
        Type *st2 = findShitppType(this);
        if (st2 && st2->isClassDef) {
          Type *ntt = b.clone();
          ntt->name = name;
          ntt->pointer = false;
          bool ok = equal(*ntt, false);
          delete ntt;
          if (ok) return true;
        } else if (st2) {
          GLog.Logf(NAME_Debug, "*** %s : %s", *name, *st2->name);
        } else if (!st2 && name == "EntityEx") {
          Type *ntt = clone();
          ntt->name = "Entity";
          bool ok = ntt->equal(b);
          delete ntt;
          if (ok) return true;
          //
          ntt = clone();
          ntt->name = "VClass";
          ntt->pointer = true;
          ok = ntt->equal(b, false);
          delete ntt;
          if (ok) return true;
        }
      }
    }
    return false;
  }

  if (pointer != b.pointer) return false;
  if (delegate != b.delegate) return false;
  if (dimension != b.dimension) return false;
  //if (!!fields != !!b.fields) return false;
  Type *bnext = b.next;
  for (Type *mnext = next; mnext; mnext = mnext->next) {
    if (!bnext) return false;
    if (*bnext == *mnext) {
    } else {
      return false;
    }
    bnext = bnext->next;
  }
  if (bnext) return false;
  return true;
}


//==========================================================================
//
//  Type::operator ==
//
//==========================================================================
bool Type::operator == (const Type &b) const {
  return equal(b);
}


//==========================================================================
//
//  Type::dumpFields
//
//==========================================================================
void Type::dumpFields () const {
  GLog.Logf("========= %s ========= (%s)", *name, *srcfile);
  for (Field *f = fields; f; f = f->next) {
    GLog.Logf("  %s: %s", *f->name, *f->type->toString());
  }
}


//==========================================================================
//
//  createVCIntType
//
//==========================================================================
Type *createVCIntType (const VStr &srcfile) {
  Type *tp = new Type();
  tp->name = VName("vint32");
  tp->shitpp = false;
  tp->srcfile = srcfile;
  return tp;
}


//==========================================================================
//
//  compressBools
//
//==========================================================================
void compressBools (Type *tp) {
  if (!tp || !tp->fields) return;
  Field *fld = tp->fields;
  while (fld) {
    //GLog.Logf(NAME_Debug, "%s: field `%s` (%s)", *tp->toString(), *fld->name, *fld->type->toString());
    if (!fld->type->isBool()) { fld = fld->next; continue; }
    //GLog.Logf(NAME_Debug, "%s: zero bit boolean is `%s`", *tp->toString(), *fld->name);
    fld->name = VName(*(VStr(fld->name)+"_bool"), VName::Add);
    fld->type = createVCIntType(fld->type->srcfile);
    int bitnum = 0;
    Field *fln = fld->next;
    while (fln && fln->type->isBool()) {
      ++bitnum;
      //GLog.Logf(NAME_Debug, "%s:   %d bit boolean is `%s` (removed)", *tp->toString(), bitnum, *fln->name);
      fld->next = fln->next;
      fln = fln->next;
      if (bitnum == 32) break;
    }
  }
  // also, expand slices
  fld = tp->fields;
  while (fld) {
    // slice has `-1` dimension
    if (fld->type->dimension != -1) { fld = fld->next; continue; }
    Field *cf = new Field();
    cf->name = VName(*(VStr("Num")+VStr(fld->name)), VName::Add);
    cf->type = createVCIntType(fld->type->srcfile);
    cf->owner = fld->owner;
    cf->next = fld->next;
    fld->next = cf;
    fld->type->pointer = true;
    fld->type->dimension = 0;
  }
  // fix some special VC cases
  if (!tp->shitpp && tp->name == "node_t") {
    vassert(tp->fields->type->toString() == "float[6]");
    vassert(tp->fields->next->type->toString() == "float[6]");
    tp->fields->type->dimension *= 2;
    tp->fields->next = tp->fields->next->next;
  }
}


/*
//==========================================================================
//
//  usage
//
//==========================================================================
static __attribute__((noreturn)) void usage () {
  GLog.Write("%s",
    "USAGE:\n"
    "  classcheck dunno\n"
    "");
  Z_Exit(1);
}
*/


//==========================================================================
//
//  scanSources
//
//==========================================================================
void scanSources (TArray<VStr> &list, VStr path, const VStr &mask1, const VStr &mask2=VStr::EmptyString) {
  auto dir = Sys_OpenDir(path, true); // want directories
  if (!dir) return;
  if (!path.isEmpty() && !path.endsWith("/")) path += "/";
  for (;;) {
    VStr name = Sys_ReadDir(dir);
    if (name.isEmpty()) break;
    if (name[0] == '_') continue;
    if (name.endsWith("/")) {
      if (name == "bsp/") continue;
      if (name == "vccrun/") continue;
      scanSources(list, path+name, mask1, mask2);
    } else {
      if (name.startsWith("stb_")) continue;
           if (!mask1.isEmpty() && name.globMatch(mask1)) list.append(path+name);
      else if (!mask2.isEmpty() && name.globMatch(mask2)) list.append(path+name);
    }
  }
}


//==========================================================================
//
//  skipUntilSemi
//
//==========================================================================
void skipUntilSemi (SemParser *par) {
  for (;;) {
    par->skipToken();
    if (par->token.isEmpty()) break;
    if (par->token.strEqu(";")) break;
  }
}


//==========================================================================
//
//  skipUntilCommaOrSemi
//
//  returns `true` for comma
//
//==========================================================================
bool skipUntilCommaOrSemi (SemParser *par) {
  int brc = 0;
  int stline = par->getTokenLine();
  for (;;) {
    par->skipToken();
    if (par->token.isEmpty()) break;
    if (par->token.strEqu(";")) {
      if (brc) Sys_Error("%s:%d: unbalanced brackets", *par->srcfile, stline);
      break;
    }
    if (par->token.strEqu("(")) { ++brc; continue; }
    if (par->token.strEqu(")")) {
      --brc;
      if (brc < 0) Sys_Error("%s:%d: unbalanced brackets", *par->srcfile, stline);
      continue;
    }
    if (brc == 0 && par->token.strEqu(",")) return true;
  }
  return false;
}


//==========================================================================
//
//  skipBrackets
//
//  "{" is already eaten
//
//==========================================================================
void skipBrackets (SemParser *par) {
  int level = 1;
  for (;;) {
    par->skipToken();
    if (par->token.isEmpty()) break;
    if (par->token.strEqu("{")) {
      ++level;
    } else if (par->token.strEqu("}")) {
      if (--level == 0) break;
    }
  }
}


//==========================================================================
//
//  skipParens
//
//  "(" is already eaten
//
//==========================================================================
void skipParens (SemParser *par) {
  int level = 1;
  for (;;) {
    par->skipToken();
    if (par->token.isEmpty()) break;
    //GLog.Logf("X: line #%d: <%s> (level=%d)", par->getTokenLine(), *par->token, level);
    if (par->token.strEqu("(")) {
      ++level;
    } else if (par->token.strEqu(")")) {
      if (--level == 0) break;
    }
  }
}


//==========================================================================
//
//  skipMethod
//
//==========================================================================
void skipMethod (SemParser *par) {
  for (;;) {
    par->skipToken();
    if (par->token.isEmpty()) break;
    //GLog.Logf(":<%s>", *par->token);
    if (par->token.strEqu(";")) break;
    if (par->token.strEqu("=")) continue;
    if (par->token.strEqu("{")) { skipBrackets(par); break; }
  }
}


//==========================================================================
//
//  skipPreprocessor
//
//  "#" is already eaten
//
//==========================================================================
void skipPreprocessor (SemParser *par) {
  for (;;) {
    char ch = par->getChar();
    if (!ch) break;
    if (ch == '/' || ch == '\\') {
      par->skipBlanks();
      ch = par->getChar();
    }
    if (ch == '\n') break;
  }
}


//==========================================================================
//
//  parseTypeDims
//
//==========================================================================
void parseTypeDims (SemParser *par, Type *tp) {
  // parse dimensions
  if (par->eat("[")) {
    tp->dimension = par->expectInt();
    par->expect("]");
  }
  // skip other dims
  while (par->eat("[")) {
    tp->dimension *= par->expectInt();
    par->expect("]");
    //while (!par->eat("]")) par->skipToken();
  }
}


//==========================================================================
//
//  parseShitppType
//
//==========================================================================
Type *parseShitppType (SemParser *par, VStr tpname) {
  if (tpname.strEqu("const")) tpname = par->expectId();
  if (par->eat("&")) return nullptr;
  if (tpname == "vuint32") tpname = "vint32";
  if (tpname == "vint8") tpname = "vuint8";
  if (tpname == "TArray") tpname = "array";
  if (tpname == "VTextureID") tpname = "vint32";
  Type *tp = new Type();
  tp->shitpp = true;
  tp->srcfile = par->srcfile;
  tp->name = VName(*tpname, VName::Add);
  // check for template
  if (par->eat("<")) {
    Type *last = nullptr;
    for (;;) {
      tpname = par->expectId();
      Type *tpp = parseShitppType(par, tpname);
      if (last) last->next = tpp; else tp->next = tpp;
      last = tpp;
      if (par->eat(">")) break;
      par->expect(",");
    }
  }
  // check for pointer
  tp->pointer = par->eat("*");
  while (par->eat("*")) {} //FIXME
  // parse dimensions
  if (par->eat("[")) {
    tp->dimension = par->expectInt();
    par->expect("]");
  }
  // skip other dims
  while (par->eat("[")) {
    while (!par->eat("]")) par->skipToken();
  }
  if (tp->name == "VObjectDelegate") tp->delegate = true;
  return tp;
}


//==========================================================================
//
//  ignoreClass
//
//==========================================================================
bool ignoreClass (const VStr &name, VName parent) {
  if (name == "VObject") return false; // explicit exception
  static const char *ignoreCN[] = {
    "FDrawerDesc",
    "TCmdKeyDown",
    "TCmdKeyUp",
    "VInput",
    "VTexture",
    "VTextureManager",
    "pcx_t",
    "VNetPollProcedure",
    "VNetworkPublic",
    "VChannel",
    "VNetConnection",
    "VNetDriver",
    "VCVFSSaver",
    "subsec_extra_t",
    "TagHashBucket",
    "TagHash",
    "VAcsGlobal",
    "VWeaponSlotFixups",
    "VPropDef",
    "VFlagDef",
    "VFlagList",
    "VMethod",
    "VExpression",
    "VInvocation",
    "VTypeExpr",
    "VStatement",
    "VCodePtrInfo",
    "VDehFlag",
    "MemInfo",
    "VMCOptimizer",
    "Instr",
    "VMemberBase",
    "VMiAStarGraphBase",
    "VLexer",
    "VProgsImport",
    "VProgsExport",
    "event_t", // should be checked separately!
    "VFieldType",
    "TILine",
    "VOpenGLTexture",
    "PcfFont",
    "VStream",
    "VNTValueIO",
    "VInputDevice",
    "VAudioCodec",
    "VOpenALDevice",
    "FAudioCodecDesc",
    "VStreamMusicPlayer",
    "FRolloffInfo",
    "VSoundManager",
    "VAudio",
    "VAudioPublic",
    "VSky",
    "VRenderLevelDrawer",
    "VRenderLevelShared",
    "VScriptSubModel",
    "StLightInfo",
    //"FAmbientSound",
    "MapInfoCommand",
    "VCacheBlockPoolEntry",
    "VCacheBlockPool",
    "VLMapCache",
    "SurfaceInfoBlock",
    "VAcs",
    nullptr,
  };
  for (const char *const *np = ignoreCN; *np; ++np) {
    if (name.strEqu(*np) || parent == *np) return true;
  }
  return false;
}


//==========================================================================
//
//  skipAttribute
//
//==========================================================================
/*
void skipAttribute (SemParser *par) {
  while (par->eat("__attribute__")) {
    par->expect("(");
    skipParens(par);
  }
}
*/


//==========================================================================
//
//  skipAttributeOrPreprocessor
//
//==========================================================================
void skipAttributeOrPreprocessor (SemParser *par) {
  for (;;) {
    if (par->eat("__attribute__")) {
      par->expect("(");
      skipParens(par);
    } else if (par->eatStartsWith("VVA_")) {
      // attribute macro
    } else if (par->eat("#")) {
      skipPreprocessor(par);
    } else {
      break;
    }
  }
}


//==========================================================================
//
//  parseShitppClassStruct
//
//==========================================================================
void parseShitppClassStruct (SemParser *par, bool isClass, bool isTypedefStruct=false) {
  VStr name;

  skipAttributeOrPreprocessor(par);
  if (!isTypedefStruct) {
    name = par->expectId();
    if (par->eat(";")) return;
    if (name.endsWith("_ClassChecker")) name.chopRight(13);
  } else {
    if (!par->check("{")) par->expectId();
  }

  if (name == "VMiAStarGraphIntr" || name == "VLanguage" ||
      name == "VLMapCache")
  {
    while (!par->eat("{")) par->skipToken();
    skipBrackets(par);
    return;
  }

  //GLog.Logf("%s '%s' found", (isClass ? "class" : "struct"), *name);
  //if (name == "VMapSpecialAction") name = "MapSpecialAction";
  Type *tp = new Type();
  tp->shitpp = true;
  tp->isClassDef = isClass;
  tp->srcfile = par->srcfile;
  tp->name = VName(*name, VName::Add);

  // parse inheritance
  if (par->eat(":")) {
    par->eat("public");
    par->eat("protected");
    par->eat("private");
    tp->parentName = VName(*par->expectId(), VName::Add);
  }

  par->expect("{");
  if (ignoreClass(name, tp->parentName)) {
    delete tp;
    skipBrackets(par);
    return;
  }

  for (;;) {
    skipAttributeOrPreprocessor(par);
    if (par->eat("}")) break;
    if (par->eat(";")) continue;
    //if (par->eat("#")) { skipPreprocessor(par); continue; }
    if (false) {
      auto pos = par->savePos();
      par->skipToken();
      GLog.Logf("%d: <%s>", par->getTokenLine(), *par->token);
      par->restorePos(pos);
    }
    par->eat("~"); // dtor
    VStr tpname = par->expectId();
    // macro or ctor?
    if (par->eat("(")) {
      skipParens(par);
      for (;;) {
        if (par->eat("override")) continue;
        if (par->eat("const")) continue;
        if (par->eat("noexcept")) continue;
        break;
      }
      if (par->eat(":")) {
        // ctor
        for (;;) {
          if (par->eat(";")) break;
          if (par->eat("{")) { skipBrackets(par); break; }
          par->skipToken();
        }
      } else {
        if (par->eat("=")) par->expect("delete");
        if (par->eat(";")) continue;
        if (par->eat("{")) skipBrackets(par);
      }
      continue;
    }
    if (tpname == "private" || tpname == "protected" || tpname == "public") {
      par->expect(":");
      continue;
    }
    if (tpname == "friend" || tpname == "typedef") { skipUntilSemi(par); continue; }
    if (tpname == "class" || tpname == "struct" || tpname == "enum") {
      int stline = par->getTokenLine();
      for (;;) {
        if (par->eat("{")) { skipBrackets(par); par->eat(";"); break; }
        if (par->eat(";")) break;
        par->skipToken();
        if (par->token.isEmpty()) Sys_Error("%s:%d: skip '%s' fucked!", *par->srcfile, stline, *tpname);
      }
      continue;
    }
    if (tpname == "static" || tpname == "virtual" || tpname == "inline") {
      // definitely a method
      skipMethod(par);
      continue;
    }
    // should be a type
    Type *fldtype = parseShitppType(par, tpname);
    if (!fldtype) { skipMethod(par); continue; }
    fldtype->shitpp = true;
    // parse field name
    VStr fldname = par->expectId();
    if (fldname == "operator") {
      delete fldtype;
      skipMethod(par);
      continue;
    }
    parseTypeDims(par, fldtype);
    //GLog.Logf("<%s> <%s>", *tpname, *fldname);
    // is this a method?
    if (par->eat("(")) {
      delete fldtype;
      skipParens(par);
      skipMethod(par);
      continue;
    }
    // this looks like a legitimate type
    // create fields
    bool semiEaten = false;
    for (;;) {
      Field *fld = new Field();
      fld->name = VName(*fldname, VName::Add);
      fld->type = fldtype;
      tp->appendField(fld);
      //if (!par->eat(",")) break;
      if (par->eat("=")) {
        if (!skipUntilCommaOrSemi(par)) { semiEaten = true; break; } // break on semicolon
      } else {
        if (!par->eat(",")) break;
      }
      fldname = par->expectId();
    }
    if (!semiEaten) par->expect(";");
  }
  if (isTypedefStruct) {
    name = par->expectId();
    if (name.endsWith("_ClassChecker")) name.chopRight(13);
    tp->name = VName(*name, VName::Add);
  }
  par->eat(";");
  // register type
  //GLog.Logf(NAME_Debug, "shitpp: %s", *tp->toString());
  shitppTypes.put(tp->name, tp);
}


//==========================================================================
//
//  parseShitppSkipTemplateShit
//
//==========================================================================
void parseShitppSkipTemplateShit (SemParser *par) {
  int stline = par->getTokenLine();
  for (;;) {
    if (par->eat("{")) { skipBrackets(par); par->eat(";"); break; }
    if (par->eat(";")) break;
    par->skipToken();
    if (par->token.isEmpty()) Sys_Error("%s:%d: skip 'template' fucked!", *par->srcfile, stline);
  }
}


//==========================================================================
//
//  parseShitppSource
//
//==========================================================================
void parseShitppSource (const VStr &filename) {
  //GLog.Logf("parsing '%s'...", *filename);
  auto par = new SemParser(new VStdFileStreamRead(fopen(*filename, "r"), filename));
  for (;;) {
    par->skipToken();
    if (par->token.isEmpty()) break;
         if (par->token.strEqu("class")) parseShitppClassStruct(par, true);
    else if (par->token.strEqu("struct")) parseShitppClassStruct(par, false);
    else if (par->token.strEqu("template")) parseShitppSkipTemplateShit(par);
    else if (par->token.strEqu("IMPLEMENT_FUNCTION")) skipMethod(par);
    else if (par->token.strEqu("#")) skipPreprocessor(par);
    else if (par->token.strEqu("{")) skipBrackets(par);
    else if (par->token.strEqu("static")) parseShitppSkipTemplateShit(par);
    else if (par->token.strEqu("typedef")) {
      if (par->eat("struct")) {
        skipAttributeOrPreprocessor(par);
        if (par->eat("ChaChaR_Type")) {
          parseShitppSkipTemplateShit(par);
        } else {
          //GLog.Logf("line #%d: <%s>", par->getTokenLine(), *par->token);
          parseShitppClassStruct(par, false, true);
        }
      } else {
        parseShitppSkipTemplateShit(par);
      }
    }
    //GLog.Logf("line #%d: <%s>", par->getTokenLine(), *par->token);
  }
  delete par;
}


//==========================================================================
//
//  parseVCTypeDim
//
//==========================================================================
void parseVCTypeDim (SemParser *par, Type *tp) {
  // parse dimensions
  if (par->eat("[")) {
    if (par->eat("]")) {
      // slice
      tp->dimension = -1;
    } else {
      tp->dimension = par->expectInt();
      if (par->eat("+")) {
        tp->dimension += par->expectInt();
      } else if (par->eat("*")) {
        tp->dimension *= par->expectInt();
      }
      par->expect("]");
    }
  }
  // skip other dims
  while (par->eat("[")) {
    while (!par->eat("]")) par->skipToken();
  }
  if (tp->delegate) {
    par->expect("(");
    skipParens(par);
  }
}


//==========================================================================
//
//  parseVCType
//
//==========================================================================
Type *parseVCType (SemParser *par, bool basic=false) {
  if (par->eat(";")) return nullptr;
  if (par->eat("#")) { skipPreprocessor(par); return nullptr; }

  for (;;) {
    if (par->eat("native")) continue;
    if (par->eat("transient")) continue;
    if (par->eat("readonly")) continue;
    if (par->eat("private")) continue;
    if (par->eat("protected")) continue;
    if (par->eat("public")) continue;
    if (par->eat("[")) { while (!par->eat("]")) par->skipToken(); continue; }
    if (par->eat("static") || par->eat("override") || par->eat("final")) {
      // definitely a method
      skipMethod(par);
      return nullptr;
    }
    // currently we cannot
    if (par->eat("delegate")) {
      Type *tpg = parseVCType(par);
      vassert(tpg);
      tpg->delegate = true;
      tpg->name = "VObjectDelegate";
      //GLog.Logf(NAME_Debug, "%s:*: %s", *par->srcfile, *tpg->toString());
      return tpg;
    }
    break;
  }

  {
    auto pos = par->savePos();
    par->skipToken();
    if (par->token.isEmpty()) return nullptr;
    //GLog.Logf("%d: <%s>", par->getTokenLine(), *par->token);
    par->restorePos(pos);
  }

  VStr tpname = par->expectId();
  Type *tp = new Type();
  tp->shitpp = false;
  tp->srcfile = par->srcfile;

  if (tpname == "class" && par->eat("!")) {
    tpname = par->expectId();
    tp->name = VName(*tpname, VName::Add);
  } else {

         if (tpname == "int") tpname = "vint32";
    else if (tpname == "ubyte") tpname = "vuint8";
    else if (tpname == "string") tpname = "VStr";
    //else if (tpname == "state") tpname = "VState";
    else if (tpname == "name") tpname = "VName";

    tp->name = VName(*tpname, VName::Add);
    if (par->eat("::")) {
      VStr n2 = par->expectId();
      tp->name = VName(*(VStr(tp->name)+"::"+n2), VName::Add);
    }
    // check for template
    if (!basic && par->eat("!")) {
      if (par->eat("(")) {
        Type *last = nullptr;
        for (;;) {
          Type *tpp = parseVCType(par, true);
          if (!tpp) continue;
          if (last) last->next = tpp; else tp->next = tpp;
          last = tpp;
          if (par->eat(")")) break;
          par->expect(",");
        }
      } else {
        do {
          tp->next = parseVCType(par, true);
        } while (!tp->next);
      }
    }
  }
  // check for pointer
  tp->pointer = par->eat("*");
  while (par->eat("*")) {} //FIXME
  if (!basic) {
    // parse dimensions
    parseVCTypeDim(par, tp);
  }
  // convert `class`
  if (!tp->pointer && tp->name == "class") {
    tp->pointer = true;
    tp->name = VName("VClass", VName::Add);
  }
  if (!tp->pointer && tp->name == "state") {
    tp->name = "VState";
    tp->pointer = true;
  }
       if (tp->name == "StateCall") tp->name = "VStateCall";
  else if (tp->name == "SectorLink") tp->name = "VSectorLink";
  else if (tp->name == "Ctl2DestLink") tp->name = "VCtl2DestLink";
  else if (tp->name == "SkillPlayerClassName") tp->name = "VSkillPlayerClassName";
  else if (tp->name == "SkillMonsterReplacement") tp->name = "VSkillMonsterReplacement";
  else if (tp->name == "LockGroup") tp->name = "VLockGroup";
  else if (tp->name == "MapMarkerInfo") tp->name = "VMapMarkerInfo";
  else if (tp->name == "IM_Phase") tp->name = "vint32";
  //GLog.Logf(NAME_Debug, "%d: <%s>", par->getTokenLine(), *tp->toString());
  return tp;
}


//==========================================================================
//
//  parseVCSource
//
//==========================================================================
void parseVCSource (VStr filename, VStr className=VStr::EmptyString) {
  //GLog.Logf("parsing '%s'...", *filename);
  auto par = new SemParser(new VStdFileStreamRead(fopen(*filename, "r"), filename));

 again:
  for (;;) {
    if (par->eat("#")) { skipPreprocessor(par); continue; }
    if (par->eat("import")) { skipUntilSemi(par); continue; }
    par->skipBlanks();
    if (par->isEOF()) { delete par; return; }
    break;
  }

  bool dontSave = false;

  Type *tp = nullptr;
  if (!par->eat("class")) {
    //GLog.Logf(NAME_Warning, "file '%s' doesn't start with `class`!", *filename);
    if (filename.extractFileBaseName() != "Object_vavoom.vc" &&
        filename.extractFileBaseName() != "Object_common.vc")
    {
      delete par;
      return;
    }
    dontSave = true;
    tp = *vcTypes.find(VName("Object"));
  } else {
    VStr name = par->expectId();

    //GLog.Logf("%s '%s' found", (isClass ? "class" : "struct"), *name);
    tp = new Type();
    tp->shitpp = false;
    tp->isClassDef = true;
    tp->srcfile = par->srcfile;
    tp->name = VName(*name, VName::Add);

    // parse inheritance
    if (par->eat(":")) {
      if (par->eat("replaces")) {
        par->expect("(");
        tp->parentName = VName(*par->expectId(), VName::Add);
        par->expect(")");
      } else {
        tp->parentName = VName(*par->expectId(), VName::Add);
      }
    }

    for (;;) {
      if (par->eat("native")) continue;
      if (par->eat("abstract")) continue;
      if (par->eat("transient")) continue;
      if (par->eat("game") || par->eat("__mobjinfo__") || par->eat("__scriptid__")) { par->expect("("); skipParens(par); continue; }
      if (par->eat("[")) { while (!par->eat("]")) par->skipToken(); continue; }
      break;
    }
    if (par->eat("decorate")) {
      par->expect(";");
      goto again;
    }

    par->expect(";");
  }

  for (;;) {
    if (false) {
      auto pos = par->savePos();
      par->skipToken();
      GLog.Logf("%d: <%s>", par->getTokenLine(), *par->token);
      par->restorePos(pos);
    }

    auto tkpos = par->savePos();
    if (par->eat("class")) {
      par->skipBlanks();
      if (par->peekChar() != '!') {
        par->restorePos(tkpos);
        if (!dontSave) { compressBools(tp); if (vcTypes.put(tp->name, tp)) GLog.Logf(NAME_Warning, "duplicate type `%s`", *tp->toString()); }
        goto again;
      }
      par->restorePos(tkpos);
    }

    if (par->eat("{")) { skipBrackets(par); continue; }
    if (par->eat("const")) { skipUntilSemi(par); continue; }
    if (par->eat("alias")) { skipUntilSemi(par); continue; }

    if (par->eat("struct")) {
      Type *stp = new Type();
      stp->srcfile = par->srcfile;
      stp->shitpp = false;
      stp->name = VName(*par->expectId(), VName::Add);
      if (par->eat(":")) {
        stp->parentName = VName(*par->expectId(), VName::Add);
      }
      par->expect("{");
      while (!par->eat("}")) {
        if (par->eat("alias")) {
          skipUntilSemi(par);
          continue;
        }
        Type *ftx = parseVCType(par);
        if (!ftx) { vassert(!par->isEOF()); continue; }
        // new field
        for (;;) {
          VStr fldname = par->expectId();
          Field *fld = new Field();
          fld->name = VName(*fldname, VName::Add);
          fld->type = ftx->clone();
          parseVCTypeDim(par, fld->type);
          stp->appendField(fld);
          if (!par->eat(",")) break;
        }
        delete ftx;
        par->expect(";");
      }
      compressBools(stp);
      if (vcTypes.put(stp->name, stp)) GLog.Logf(NAME_Warning, "duplicate type `%s`", *tp->toString());
      //skipBrackets(par);
      continue;
    }

    if (par->eat("enum") || par->eat("bitenum")) {
      while (!par->eat("{")) par->skipToken();
      skipBrackets(par);
      continue;
    }

    if (par->eat("states") || par->eat("replication")) {
      par->expect("{");
      skipBrackets(par);
      continue;
    }

    if (par->eat("defaultproperties")) {
      par->expect("{");
      skipBrackets(par);
      if (!dontSave) { compressBools(tp); if (vcTypes.put(tp->name, tp)) GLog.Logf(NAME_Warning, "duplicate type `%s`", *tp->toString()); }
      goto again;
      continue;
    }


    Type *fldtype = parseVCType(par);

    /*
    if (tp->name == "Widget") {
      auto pos = par->savePos();
      par->skipToken();
      GLog.Logf("%d: <%s> <%s> (%d)", par->getTokenLine(), *par->token, (fldtype ? *fldtype->toString() : "fuck"), (fldtype ? (int)fldtype->delegate : -1));
      par->restorePos(pos);
    }
    */
    if (!fldtype) {
      auto pos = par->savePos();
      par->skipToken();
      if (par->token.isEmpty()) break;
      par->restorePos(pos);
      continue;
    }

    VStr fldname = par->expectId();

    if (!fldtype->delegate) {
      // method?
      if (par->eat("(")) {
        skipParens(par);
        skipMethod(par);
        continue;
      }

      // property?
      if (par->eat("{")) {
        skipBrackets(par);
        continue;
      }
    }

    //if (fldtype->delegate) GLog.Logf(NAME_Debug, "vc: %s -- %s", *fldtype->toString(), *fldname);
    //skipUntilSemi(par);

    // this looks like a legitimate type
    // create fields
    bool semiEaten = false;
    for (;;) {
      Field *fld = new Field();
      fld->name = VName(*fldname, VName::Add);
      fld->type = fldtype->clone();
      vassert(fld->type->delegate == fldtype->delegate);
      parseVCTypeDim(par, fld->type);
      tp->appendField(fld);
      //if (tp->name == "Widget") GLog.Logf(NAME_Debug, "WIDGET: %s : %s", *fld->name, *fld->type->toString());
      if (par->eat("=")) {
        if (!skipUntilCommaOrSemi(par)) { semiEaten = true; break; } // break on semicolon
      } else {
        if (!par->eat(",")) break;
      }
      fldname = par->expectId();
    }
    delete fldtype;
    if (!semiEaten) par->expect(";");
  }
  // register type
  //GLog.Logf(NAME_Debug, "vc: %s", *tp->toString());
  if (!dontSave) {
    if (vcTypes.put(tp->name, tp)) GLog.Logf(NAME_Warning, "duplicate type `%s`", *tp->toString());
  }

  delete par;
}


//==========================================================================
//
//  checkVCType
//
//==========================================================================
void checkVCType (Type *tp) {
  if (!tp) return;
  vassert(!tp->shitpp);
  Type *spt = findShitppType(tp);
  if (!spt) {
    if (tp->name == "MobjByTIDIteratorInfo") return;
    if (tp->name == "CD_LinePlanes") return;
    if (tp->name == "MiAStarGraphBase") return; //FIXME: cannot parse this yet!
    if (tp->name == "event_t") return; //FIXME: cannot parse this yet!
    if (tp->name == "TPlane") return; //FIXME: absent yet!
    if (tp->srcfile.indexOf("/progs/common/uibase/") >= 0) return;
    if (tp->srcfile.indexOf("/progs/common/botai/") >= 0) return;
    if (tp->srcfile.indexOf("/progs/hexen/") >= 0) return;
    if (tp->srcfile.indexOf("/progs/heretic/") >= 0) return;
    if (tp->srcfile.indexOf("/progs/doom/") >= 0) return;
    if (tp->srcfile.indexOf("/progs/strife/") >= 0) return;
    if (tp->srcfile.indexOf("/progs/common/linespec/armor/") >= 0) return;
    if (tp->srcfile.indexOf("/progs/common/linespec/") >= 0) return;
    GLog.Logf(NAME_Debug, "no shitpp type for VavoomC type `%s` (%s)", *tp->toString(), *tp->srcfile);
    return;
  }

  int fcount = tp->fieldCount();

  if (fcount != spt->fieldCount()) {
    if (tp->name != "surface_t") {
      GLog.Logf(NAME_Error, "%s <-> %s: invalid field count (%d : %d)", *tp->toString(), *spt->toString(), fcount, spt->fieldCount());
      tp->dumpFields();
      spt->dumpFields();
      return;
    }
  }

  for (int f = 0; f < fcount; ++f) {
    Field *vcf = tp->fieldAt(f);
    Field *spf = spt->fieldAt(f);
    Type *vcType = vcf->type; //if (vcTypes.has(vcType->name)) vcType = *vcTypes.find(vcType->name);
    Type *spType = spf->type; //if (shitppTypes.has(spType->name)) spType = *shitppTypes.find(spType->name);
    if (*vcType == *spType) {
    } else {
      // check for class type
      bool ok = false;
      if (!vcType->pointer && spType->pointer) {
        Type *st2 = findShitppType(vcType);
        if (st2 && st2->isClassDef) {
          Type *ntt = spType->clone();
          ntt->name = vcType->name;
          ntt->pointer = false;
          ok = (*vcType == *ntt);
          delete ntt;
          /*
          ok = (VStr("V")+(*vcType->name)) == VStr(st2->name) &&
               vcType->delegate == spType->delegate &&
               vcType->dimension == spType->dimension &&
          */
        }
      }
      // internal hack
      if (!ok && tp->name == "Level" && (vcf->name == "ActiveSequences" || vcf->name == "CameraTextures")) {
        ok = (vcType->toString() == "array!vint32");
      }
      if (!ok) {
        GLog.Logf(NAME_Error, "%s <-> %s: field #%d (%s : %s) has different types (%s : %s)", *tp->toString(), *spt->toString(), f, *vcf->name, *spf->name, *vcType->toString(), *spType->toString());
        //return;
      }
    }
  }

  GLog.Logf("PASSED: %s <-> %s: (%d : %d)", *tp->toString(), *spt->toString(), fcount, spt->fieldCount());
}


//==========================================================================
//
//  main
//
//==========================================================================
int main (int argc, char **argv) {
  GLog.WriteLine("CLASSCHECK build date: %s  %s", __DATE__, __TIME__);
  //GLog.WriteLine("%s", "");

  /*
  bool doneOptions = false;
  TArray<VStr> flist;

  for (int f = 1; f < argc; ++f) {
    VStr arg = VStr(argv[f]);
    if (arg.isEmpty()) continue;
    if (doneOptions || arg[0] != '-') {
      bool found = false;
      for (int c = 0; c < flist.length(); ++c) {
        int cres;
        if (cres == 0) { found = true; break; }
      }
      if (!found) flist.append(arg);
      continue;
    }
    if (arg == "--") { doneOptions = true; continue; }
    Sys_Error("unknown option '%s'", *arg);
  }

  //if (flist.length() == 0) usage();
  */

  TArray<VStr> shitpplist;
  shitpplist.append("../../libs/core/prngs.h");
  shitpplist.append("../../libs/core/chachaprng_c.h");
  scanSources(shitpplist, "../../source", "*.h", "*.cpp");
  scanSources(shitpplist, "../../libs/vavoomc", "*.h", "*.cpp");
  GLog.Logf("%d shitpplist files found", shitpplist.length());

  TArray<VStr> vclist;
  scanSources(vclist, "../../progs", "*.vc");
  GLog.Logf("%d VavoomC files found", vclist.length());

  for (auto &&fname : shitpplist) parseShitppSource(fname);

  {
    Type *tpvcObj = new Type();
    tpvcObj->shitpp = false;
    tpvcObj->isClassDef = true;
    tpvcObj->srcfile = "Object.vc";
    tpvcObj->name = VName("Object");
    vcTypes.put(tpvcObj->name, tpvcObj);
  }

  for (auto &&fname : vclist) {
    VStr filename = fname.extractFileBaseName();
    /*
    if (filename == "Object_common.vc" ||
        filename == "Object_vavoom.vc" ||
        filename == "Object_vv_lumpiter.vc" ||
        filename == "RTTI.vc")
    {
      continue;
    }
    */
    if (filename == "classes.vc" ||
        filename == "botnames.vc" ||
        filename == "LevelMapFixer.vc" ||
        filename == "Object_vv_lumpiter.vc" ||
        filename == "GameObject_lspname.vc")
    {
      continue;
    }
    parseVCSource(fname);
  }

  GLog.Logf("%d shitpp types found, %d vc types found", shitppTypes.length(), vcTypes.length());

  {
    auto tpp = vcTypes.find(VName("Object"));
    Type *tpvcObj = *tpp;
    compressBools(tpvcObj);
    vassert(tpvcObj->name == "Object");
    // shitpp vmt
    vassert(tpvcObj->fields->name == "__CxxVTable");
    tpvcObj->fields = tpvcObj->fields->next;
  }

  for (auto it = vcTypes.first(); it; ++it) {
    Type *tp = it.GetValue();
    checkVCType(tp);
  }

  Z_ShuttingDown();
  return 0;
}


/*
Object_common.vc
Object_vavoom.vc
Object_vv_lumpiter.vc
RTTI.vc
*/
