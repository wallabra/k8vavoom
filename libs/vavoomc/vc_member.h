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

// ////////////////////////////////////////////////////////////////////////// //
class VExpression;
class VStatement;
class VLexer;

//k8: this should work for both 32-bit and 64-bit systems
#define ANY_PACKAGE  ((VPackage *)((uintptr_t)(~(0ULL))))
#define ANY_MEMBER   (255)

/*k8: and this check doesn't work; sigh.
static_assert(
  (sizeof(VPackage *) == 4 && (uintptr_t)ANY_PACKAGE == 0xffffffffU) ||
  (sizeof(VPackage *) == 8 && (uintptr_t)ANY_PACKAGE == 0xffffffffffffffffULL), "oopsie!");
*/

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
  static int optDeprecatedLaxOverride; // true: don't require `override` on overriden methods
  static int optDeprecatedLaxStates; // true: ignore missing states in state resolver
  static int unsafeCodeAllowed; // true by default
  static int unsafeCodeWarning; // true by default
  static int koraxCompatibility; // false by default
  static int koraxCompatibilityWarnings; // true by default

protected:
  vuint32 mMemberId; // never zero, monotonically increasing
  static vuint32 lastUsedMemberId; // monotonically increasing

public:
  // internal variables
  vuint8 MemberType;
  vint32 MemberIndex;
  VName Name;
  VMemberBase *Outer;
  TLocation Loc;

private:
  VMemberBase *HashNext;
  VMemberBase *HashNextAnyLC;
  VMemberBase *HashNextClassLC;
  VMemberBase *HashNextPropLC;
  VMemberBase *HashNextConstLC;

  static TMapNC<VName, VMemberBase *> gMembersMap;
  static TMapNC<VName, VMemberBase *> gMembersMapAnyLC; // lower-cased names
  static TMapNC<VName, VMemberBase *> gMembersMapClassLC; // lower-cased class names
  static TMapNC<VName, VMemberBase *> gMembersMapPropLC; // lower-cased property names
  static TMapNC<VName, VMemberBase *> gMembersMapConstLC; // lower-cased constant names
  static TArray<VPackage *> gPackageList;

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

public:
  static inline const char *GetMemberTypeStringName (vuint8 mtype) {
    switch (mtype) {
      case MEMBER_Package: return "package";
      case MEMBER_Field: return "field";
      case MEMBER_Property: return "property";
      case MEMBER_Method: return "method";
      case MEMBER_State: return "state";
      case MEMBER_Const: return "constant";
      case MEMBER_Struct: return "struct";
      case MEMBER_Class: return "class";
      case MEMBER_DecorateClass: return "deco-class";
    }
    return "<unknown>";
  }

  inline const char *GetMemberTypeString () const { return GetMemberTypeStringName(MemberType); }

public:
  inline bool isPackageMember () const { return (MemberType == MEMBER_Package); }
  inline bool isFieldMember () const { return (MemberType == MEMBER_Field); }
  inline bool isPropertyMember () const { return (MemberType == MEMBER_Property); }
  inline bool isMethodMember () const { return (MemberType == MEMBER_Method); }
  inline bool isStateMember () const { return (MemberType == MEMBER_State); }
  inline bool isConstantMember () const { return (MemberType == MEMBER_Const); }
  inline bool isStructMember () const { return (MemberType == MEMBER_Struct); }
  inline bool isClassMember () const { return (MemberType == MEMBER_Class); }
  inline bool isDecoClassMember () const { return (MemberType == MEMBER_DecorateClass); }

public:
  VMemberBase (vuint8, VName, VMemberBase *, const TLocation &);
  virtual ~VMemberBase ();

  virtual void CompilerShutdown ();

  // unique object id, will not repeat itself.
  // if 32-bit integer overflows, the game will abort.
  // will never be zero.
  inline vuint32 GetMemberId () const { return mMemberId; }

  // for each name
  // WARNING! don't add/remove ANY named members from callback!
  // return `FERes::FOREACH_STOP` from callback to stop (and return current member)
  // template function should accept `VMemberBase *`, and return `FERes`
  // templated, so i can use lambdas
  // k8: don't even ask me. fuck shitplusplus.
  template<typename TDg> static VMemberBase *ForEachNamed (VName aname, TDg &&dg, bool caseSensitive=true) {
    decltype(dg((VMemberBase *)nullptr)) test_ = FERes::FOREACH_NEXT;
    (void)test_;
    if (aname == NAME_None) return nullptr; // oops
    if (!caseSensitive) {
      // use lower-case map
      aname = VName(*aname, VName::FindLower);
      if (aname == NAME_None) return nullptr; // no such name, no chance to find a member
      VMemberBase **mpp = gMembersMapAnyLC.find(aname);
      if (!mpp) return nullptr;
      for (VMemberBase *m = *mpp; m; m = m->HashNextAnyLC) {
        FERes res = dg(m);
        if (res == FERes::FOREACH_STOP) return m;
      }
    } else {
      // use normal map
      VMemberBase **mpp = gMembersMap.find(aname);
      if (!mpp) return nullptr;
      for (VMemberBase *m = *mpp; m; m = m->HashNext) {
        FERes res = dg(m);
        if (res == FERes::FOREACH_STOP) return m;
      }
    }
    return nullptr;
  }
  template<typename TDg> static inline VMemberBase *ForEachNamedCI (VName aname, TDg &&dg) { return ForEachNamed(aname, dg, false); }

  // accessors
  inline const char *GetName () const { return *Name; }
  inline const VName GetVName () const { return Name; }
  VStr GetFullName () const; // full name includes package, and class
  VPackage *GetPackage () const noexcept;
  VPackage *GetPackageRelaxed () const noexcept;
  bool IsIn (VMemberBase *) const noexcept;

  virtual void Serialise (VStream &);
  // this performs the final tasks for various object types (see the respective source for more info)
  virtual void PostLoad ();
  virtual void Shutdown ();

  static void DumpNameMaps ();

  static void StaticInit (); // don't call directly!
  static void StaticExit (); // don't call directly!

  static void StaticAddPackagePath (const char *);
  static VPackage *StaticLoadPackage (VName, const TLocation &);
  static VMemberBase *StaticFindMember (VName AName, VMemberBase *AOuter, vuint8 AType, VName EnumName=NAME_None);
  static VMemberBase *StaticFindMemberNoCase (VStr AName, VMemberBase *AOuter, vuint8 AType, VName EnumName=NAME_None);

  //FIXME: this looks ugly
  static VFieldType StaticFindType (VClass *, VName);
  static VClass *StaticFindClass (const char *AName, bool caseSensitive=true);
  static VClass *StaticFindClass (VName AName, bool caseSensitive=true);
  static inline VClass *StaticFindClassNoCase (const char *AName) { return StaticFindClass(AName, false); }

  // will not clear `list`
  static void StaticGetClassListNoCase (TArray<VStr> &list, VStr prefix, VClass *isaClass=nullptr);

  static VClass *StaticFindClassByGameObjName (VName aname, VName pkgname);

  static void StaticSplitStateLabel (VStr LabelName, TArray<VName> &Parts, bool appendToParts=false);

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
  static VStream *(*dgOpenFile) (VStr filename, void *userdata);

private:
  static TArray<VStr> incpathlist;
  static TArray<VStr> definelist;
};

inline vuint32 GetTypeHash (const VMemberBase *M) { return (M ? hashU32(M->GetMemberId()) : 0); }
