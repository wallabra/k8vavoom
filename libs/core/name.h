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
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
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
//**
//**  Vavoom global name types.
//**
//**************************************************************************

// maximum length of a name
enum { NAME_SIZE = 128 };


// entry in the names table
struct VNameEntry {
  VNameEntry *HashNext; // next name for this hash list
  vint32 Index; // index of the name
  char Name[NAME_SIZE]; // name value

  friend VStream &operator << (VStream &, VNameEntry &);
  friend VNameEntry *AllocateNameEntry (const char *Name, vint32 Index, VNameEntry *HashNext);
};


// names are stored as indexes in the global name table.
// they are stored once and only once.
// all names are case-sensitive.
class VName {
private:
  enum { HASH_SIZE = 4096 };

  vint32 Index;

  static TArray<VNameEntry *> Names;
  static VNameEntry *HashTable[HASH_SIZE];
  static bool Initialised;

public:
  //  Different types of finding a name.
  enum ENameFindType {
    Find,      // Find a name, return 0 if it doesn't exist.
    Add,       // Find a name, add it if it doesn't exist.
    AddLower8, // Find or add lowercased, max length 8 name.
    AddLower,  // Find or add lowercased.
    FindLower, // Find a name, return 0 if it doesn't exist.
  };

  // constructors
  VName () : Index(0) {}
  VName (ENoInit) {}
  VName (EName N) : Index(N) {}
  VName (const char *, ENameFindType=Add);

  // accessors
  inline const char *operator * () const { return Names[Index]->Name; }
  inline vint32 GetIndex () const { return Index; }

  inline bool isValid () const { return (Index >= 0 && Index < Names.length()); }

  // comparisons
  inline bool operator == (const VName &Other) const { return (Index == Other.Index); }
  inline bool operator != (const VName &Other) const { return (Index != Other.Index); }
  bool operator == (const VStr &s) const;
  bool operator != (const VStr &s) const;
  bool operator == (const char *s) const;
  bool operator != (const char *s) const;

  // global functions
  static void StaticInit ();
  //static void StaticExit ();

  static inline int GetNumNames () { return Names.Num(); }
  static inline VNameEntry *GetEntry (int i) { return Names[i]; }
  static const char *SafeString (EName N);
};

inline vuint32 GetTypeHash (const VName &N) { return N.GetIndex(); }
