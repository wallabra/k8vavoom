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


//==========================================================================
//
//  VMemberBase
//
//  The base class of all objects.
//
//==========================================================================
class VMemberBase {
public:
  // internal variables
  vuint8 MemberType;
  vint32 MemberIndex;
  VName Name;
  VMemberBase *Outer;
  TLocation Loc;
  VMemberBase *HashNext;

  static bool GObjInitialised;
  static TArray<VMemberBase *> GMembers;
  static VMemberBase *GMembersHash[4096];

  static TArray<VStr> GPackagePath;
  static TArray<VPackage *> GLoadedPackages;
  static TArray<VClass *> GDecorateClassImports;

  static VClass *GClasses; // linked list of all classes

  static bool unsafeCodeAllowed; // true by default
  static bool unsafeCodeWarning; // false by default

  // srtuctors
  VMemberBase (vuint8, VName, VMemberBase *, const TLocation &);
  virtual ~VMemberBase ();

  virtual void CompilerShutdown ();

  // accessors
  inline const char *GetName () const { return *Name; }
  inline const VName GetVName () const { return Name; }
  VStr GetFullName () const;
  VPackage *GetPackage () const;
  bool IsIn (VMemberBase *) const;

  virtual void Serialise (VStream &);
  virtual void PostLoad ();
  virtual void Shutdown ();

  static void StaticInit ();
  static void StaticExit ();
  static void StaticAddPackagePath (const char *);
  static VPackage *StaticLoadPackage (VName, const TLocation &);
  static VMemberBase *StaticFindMember (VName AName, VMemberBase *AOuter, vuint8 AType, VName EnumName=NAME_None);
  static VMemberBase *StaticFindMemberNoCase (VName AName, VMemberBase *AOuter, vuint8 AType, VName EnumName=NAME_None);

  //FIXME: this looks ugly
  static VFieldType StaticFindType (VClass *, VName);
  static VClass *StaticFindClass (VName);

  static VClass *StaticFindClassByGameObjName (VName aname, VName pkgname);

  static void StaticSplitStateLabel (const VStr &, TArray<VName> &);

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

inline vuint32 GetTypeHash (const VMemberBase *M) { return (M ? M->MemberIndex : 0); }
