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
//**  Copyright (C) 2018-2019 Ketmar Dark
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

// ////////////////////////////////////////////////////////////////////////// //
class VExpression;
class VStatement;
class VLexer;

#define ANY_PACKAGE  ((VPackage*)-1)
#define ANY_MEMBER   (255)


enum {
  MEMBER_Package,
  MEMBER_Field,
  MEMBER_Property,
  MEMBER_Method,
  MEMBER_State,
  MEMBER_Const,
  MEMBER_Struct,
  MEMBER_Class,

  // a fake type for DECORATE class imports
  MEMBER_DecorateClass,
};

enum FERes {
  FOREACH_NEXT,
  FOREACH_STOP,
};


//==========================================================================
//
//  VMemberBase
//
//  The base class of all objects.
//
//==========================================================================
class VMemberBase {
public:
  // some global options
  static bool optDeprecatedLaxOverride; // true: don't require `override` on overriden methods
  static bool optDeprecatedLaxStates; // true: ignore missing states in state resolver

public:
  // internal variables
  vuint8 MemberType;
  vint32 MemberIndex;
  VName Name;
  VMemberBase *Outer;
  TLocation Loc;

private:
  VMemberBase *HashNext;
  VMemberBase *HashNextLC;

  static void PutToNameHash (VMemberBase *self);
  static void RemoveFromNameHash (VMemberBase *self);

  static void DumpNameMap (TMapNC<VName, VMemberBase *> &map, bool caseSensitive);

public:
  static bool GObjInitialised;
  static bool GObjShuttingDown;
  static TArray<VMemberBase *> GMembers;

  static TArray<VStr> GPackagePath;
  static TArray<VPackage *> GLoadedPackages;
  static TArray<VClass *> GDecorateClassImports;

  static VClass *GClasses; // linked list of all classes

  static bool unsafeCodeAllowed; // true by default
  static bool unsafeCodeWarning; // false by default

public:
  VMemberBase (vuint8, VName, VMemberBase *, const TLocation &);
  virtual ~VMemberBase ();

  virtual void CompilerShutdown ();

  // for each name
  // WARNING! don't add/remove ANY named members from callback!
  // return `FOREACH_STOP` from callback to stop (and return current member)
  static VMemberBase *ForEachNamed (VName aname, FERes (*dg) (VMemberBase *m), bool caseSensitive=true);
  static inline VMemberBase *ForEachNamedCI (VName aname, FERes (*dg) (VMemberBase *m)) { return ForEachNamed(aname, dg, false); }

  // accessors
  inline const char *GetName () const { return *Name; }
  inline const VName GetVName () const { return Name; }
  VStr GetFullName () const;
  VPackage *GetPackage () const;
  bool IsIn (VMemberBase *) const;

  virtual void Serialise (VStream &);
  virtual void PostLoad ();
  virtual void Shutdown ();

  static void DumpNameMaps ();

  static void StaticInit (); // don't call directly!
  static void StaticExit (); // don't call directly!

  static void StaticAddPackagePath (const char *);
  static VPackage *StaticLoadPackage (VName, const TLocation &);
  static VMemberBase *StaticFindMember (VName AName, VMemberBase *AOuter, vuint8 AType, VName EnumName=NAME_None/*, bool caseSensitive=true*/);
  //static inline VMemberBase *StaticFindMemberNoCase (VName AName, VMemberBase *AOuter, vuint8 AType, VName EnumName=NAME_None) { return StaticFindMember(AName, AOuter, AType, EnumName, false); }

  //FIXME: this looks ugly
  static VFieldType StaticFindType (VClass *, VName);
  static VClass *StaticFindClass (VName AName, bool caseSensitive=true);
  static inline VClass *StaticFindClassNoCase (VName AName) { return StaticFindClass(AName, false); }

  // will not clear `list`
  static void StaticGetClassListNoCase (TArray<VStr> &list, const VStr &prefix, VClass *isaClass=nullptr);

  static VClass *StaticFindClassByGameObjName (VName aname, VName pkgname);

  static void StaticSplitStateLabel (const VStr &LabelName, TArray<VName> &Parts, bool appendToParts=false);

  static void StaticAddIncludePath (const char *);
  static void StaticAddDefine (const char *);

  static void InitLexer (VLexer &lex);

  // call this when you will definitely won't compile anything anymore
  // this will free some AST leftovers and other intermediate compiler data
  static void StaticCompilerShutdown ();

  static bool doAsmDump;

public:
  // virtual file system callback for reading source files (can be empty, and is empty by default)
  static void *userdata; // arbitrary pointer, not used by the lexer
  // should return `null` if file not found
  // should NOT fail if file not found
  static VStream *(*dgOpenFile) (const VStr &filename, void *userdata);

private:
  static TArray<VStr> incpathlist;
  static TArray<VStr> definelist;
};

inline vuint32 GetTypeHash (const VMemberBase *M) { return (M ? M->MemberIndex+1 : 0); }
