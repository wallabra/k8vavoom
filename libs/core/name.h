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
    VNameEntry *HashNext; // next name for this hash list
    vint32 Index; // index of the name
    // `VStr` data follows (WARNING! it is invalid prior to calling `StaticInit()`)
    // this is first, so `rc` will be naturally aligned
    vint32 length __attribute__((aligned(8)));
    vint32 alloted;
    vint32 rc; // negative number means "immutable string" (and it is always negative here)
    vint32 dummy;
    char Name[PREDEFINED_NAME_SIZE+1]; // name value
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

  static inline bool IsInitialised () noexcept { return Initialised; }

  // accessors
  inline const char *operator * () const noexcept {
    if (Initialised) {
      vassert(Index >= 0 && Index < (int)NamesCount);
      return Names[Index]->Name;
    } else {
      vassert(Index >= 0 && Index < GetAutoNameCounter());
      return AutoNames[Index].Name;
    }
  }

  // won't abort on invalid index
  const char *getCStr () const noexcept;

  inline vint32 GetIndex () const noexcept { return Index; }

  inline bool isValid () const noexcept {
    if (Initialised) {
      return (Index >= 0 && Index < (int)NamesCount);
    } else {
      return (Index >= 0 && Index < GetAutoNameCounter());
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

  static inline int GetNumNames () noexcept { return (Initialised ? (int)NamesCount : GetAutoNameCounter()); }

  static const char *SafeString (EName N) noexcept;

  static VName CreateWithIndex (int i) noexcept {
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
