//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
//**  Copyright (C) 2018-2019 Ketmar Dark
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
//**  Vavoom global name types.
//**
//**************************************************************************
class VStream;
class VStr;


// maximum length of a name (without trailing zero)
enum { NAME_SIZE = 255 };
// maximum length of a predefined name (without trailing zero)
enum { PREDEFINED_NAME_SIZE = 63 };


// names are stored as indexes in the global name table.
// they are stored once and only once.
// all names are case-sensitive.
class VName {
public:
  // entry in the names table
  // WARNING! this should be kept in sync with `VStr` storage!
  struct __attribute__((packed)) VNameEntry {
    enum {
      FLAG_LOCASE = 1u<<0, // set if name doesn't contain upper-case ASCII letters
      FLAG_NAME8  = 1u<<1, // set if name is no more than 8 chars
    };
    VNameEntry *HashNext; // next name for this hash list
    vint32 Index; // index of the name
    vuint32 Flags;
    // `VStr` data follows (WARNING! it is invalid prior to calling `StaticInit()`)
    // this is first, so `rc` will be naturally aligned
    vint32 length __attribute__((aligned(8)));
    vint32 alloted;
    vint32 rc; // negative number means "immutable string" (and it is always negative here)
    vint32 dummy;
    char Name[PREDEFINED_NAME_SIZE+1]; // name value

    inline bool IsLoCase () const noexcept { return !!(Flags&FLAG_LOCASE); }
    inline void SetLoCase () noexcept { Flags |= FLAG_LOCASE; }

    inline bool IsName8 () const noexcept { return !!(Flags&FLAG_NAME8); }
    inline void SetName8 () noexcept { Flags |= FLAG_NAME8; }
  };

private:
  vint32 Index;

private:
  static VNameEntry **Names;
  static size_t NamesAlloced;
  static size_t NamesCount;
  static bool Initialised;

  static VNameEntry AutoNames[];

  static int AppendNameEntry (VNameEntry *e) noexcept;
  static int GetAutoNameCounter () noexcept;

public:
  // different types of finding a name
  enum ENameFindType {
    Find,      // find a name, return 0 if it doesn't exist
    Add,       // find a name, add it if it doesn't exist
    AddLower8, // find or add lowercased, max length 8 name
    AddLower,  // find or add lowercased
    FindLower, // find a name, return 0 if it doesn't exist
    FindLower8, // find a name, lowercased, max length is 8, return 0 if it doesn't exist
  };

  // constructors
  VName ()  noexcept: Index(0) {}
  VName (ENoInit)  noexcept{}
  VName (EName N)  noexcept: Index(N) {}
  VName (const char *, ENameFindType=Add) noexcept;

  static VVA_CHECKRESULT inline bool IsInitialised () noexcept { return Initialised; }

  // accessors
  VVA_CHECKRESULT inline const char *operator * () const noexcept {
    if (Initialised) {
      vassert(Index >= 0 && Index < (int)NamesCount);
      return Names[Index]->Name;
    } else {
      vassert(Index >= 0 && Index < GetAutoNameCounter());
      return AutoNames[Index].Name;
    }
  }

  // won't abort on invalid index
  VVA_CHECKRESULT const char *getCStr () const noexcept;

  VVA_CHECKRESULT inline vint32 GetIndex () const noexcept { return Index; }

  VVA_CHECKRESULT inline bool isValid () const noexcept {
    if (Initialised) {
      return (Index >= 0 && Index < (int)NamesCount);
    } else {
      return (Index >= 0 && Index < GetAutoNameCounter());
    }
  }

  // returns lower-cased name (creating it if necessary); cannot be called before `StaticInit()`
  VVA_CHECKRESULT inline VName GetLower () const noexcept {
    if (Index == NAME_None) return *this;
    vassert(Initialised);
    if (Names[Index]->IsLoCase()) return *this;
    return VName(Names[Index]->Name, VName::AddLower);
  }

  // returns lower-cased name8 (creating it if necessary); cannot be called before `StaticInit()`
  VVA_CHECKRESULT inline VName GetLower8 () const noexcept {
    if (Index == NAME_None) return *this;
    vassert(Initialised);
    if (Names[Index]->IsLoCase() && Names[Index]->IsName8()) return *this;
    return VName(Names[Index]->Name, VName::AddLower8);
  }

  // returns lower-cased name, or NAME_None; cannot be called before `StaticInit()`
  VVA_CHECKRESULT inline VName GetLowerNoCreate () const noexcept {
    if (Index == NAME_None) return *this;
    vassert(Initialised);
    if (Names[Index]->IsLoCase()) return *this;
    return VName(NAME_None);
  }

  // returns lower-cased name, or NAME_None; cannot be called before `StaticInit()`
  VVA_CHECKRESULT inline VName GetLower8NoCreate () const noexcept {
    if (Index == NAME_None) return *this;
    vassert(Initialised);
    if (Names[Index]->IsLoCase() && Names[Index]->IsName8()) return *this;
    return VName(NAME_None);
  }

  VVA_CHECKRESULT inline bool IsLower () const noexcept {
    if (Index == NAME_None) return false;
    if (Initialised) {
      return Names[Index]->IsLoCase();
    } else {
      for (const char *s = AutoNames[Index].Name; *s; ++s) if (s[0] >= 'A' && s[0] <= 'Z') return false;
      return true;
    }
  }

  VVA_CHECKRESULT inline bool IsLower8 () const noexcept {
    if (Index == NAME_None) return false;
    if (Initialised) {
      return (Names[Index]->IsLoCase() && Names[Index]->IsName8());
    } else {
      for (const char *s = AutoNames[Index].Name; *s; ++s) if (s[0] >= 'A' && s[0] <= 'Z') return false;
      return (strlen(AutoNames[Index].Name) <= 8);
    }
  }

  // comparisons
  inline bool operator == (const VName &Other) const noexcept { return (Index == Other.Index); }
  inline bool operator != (const VName &Other) const noexcept { return (Index != Other.Index); }
  bool operator == (const VStr &s) const noexcept; // alas, it has to be `&` due to include order
  inline bool operator != (const VStr &s) const noexcept { return !(*this == s); }
  bool operator == (const char *s) const noexcept; // alas, it has to be `&` due to include order
  inline bool operator != (const char *s) const noexcept { return !(*this == s); }

  // global functions
  static void StaticInit () noexcept;
  //static void StaticExit () noexcept;

  static VVA_CHECKRESULT inline int GetNumNames () noexcept { return (Initialised ? (int)NamesCount : GetAutoNameCounter()); }

  static VVA_CHECKRESULT const char *SafeString (EName N) noexcept;

  static VVA_CHECKRESULT VName CreateWithIndex (int i) noexcept {
    if (Initialised) {
      vassert(i >= 0 && i < (int)NamesCount);
    } else {
      vassert(i >= 0 && i < GetAutoNameCounter());
    }
    VName res;
    res.Index = i;
    return res;
  }

  static void DebugDumpHashStats () noexcept;
};

static_assert(sizeof(VName) == sizeof(vint32), "invalid VName class size!"); // for VavoomC


inline vuint32 GetTypeHash (const VName &N) { return hashU32((vuint32)(N.GetIndex())); }
