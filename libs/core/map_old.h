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
//**
//**  Template for mapping keys to values.
//**
//**************************************************************************
template<class TK, class TV> class TMap {
protected:
  struct TPair {
    TK Key;
    TV Value;
    vint32 HashNext;
  };

  TArray<TPair> Pairs;
  vint32 *HashTable;
  vint32 HashSize;

  void Rehash () {
    checkSlow(HashSize >= 16);
    if (HashTable) {
      delete[] HashTable;
      HashTable = nullptr;
    }
    HashTable = new vint32[HashSize];
    for (int i = 0; i < HashSize; ++i) HashTable[i] = -1;
    for (int i = 0; i < Pairs.Num(); ++i) {
      int Hash = GetTypeHash(Pairs[i].Key)&(HashSize-1);
      Pairs[i].HashNext = HashTable[Hash];
      HashTable[Hash] = i;
    }
  }

  void Relax () {
    while (HashSize > Pairs.Num()+16) HashSize >>= 1;
    Rehash();
  }

public:
  TMap () : HashTable(nullptr), HashSize(16) {
    Rehash();
  }

  TMap (TMap &Other) : Pairs(Other.Pairs), HashTable(nullptr), HashSize(Other.HashSize) {
    Rehash();
  }

  ~TMap () {
    if (HashTable) {
      delete[] HashTable;
      HashTable = nullptr;
    }
    HashTable = nullptr;
    HashSize = 0;
  }

  TMap &operator = (const TMap &Other) {
    Pairs = Other.Pairs;
    HashSize = Other.HashSize;
    Rehash();
    return *this;
  }

  void Set (const TK &Key, const TV &Value) {
    int HashIndex = GetTypeHash(Key) & (HashSize - 1);
    for (int i = HashTable[HashIndex]; i != -1; i = Pairs[i].HashNext) {
      if (Pairs[i].Key == Key) {
        Pairs[i].Value = Value;
        return;
      }
    }
    TPair &Pair = Pairs.Alloc();
    Pair.HashNext = HashTable[HashIndex];
    HashTable[HashIndex] = Pairs.Num()-1;
    Pair.Key = Key;
    Pair.Value = Value;
    if (HashSize*2+16 < Pairs.Num()) {
      HashSize <<= 1;
      Rehash();
    }
  }
  inline void set (const TK &Key, const TV &Value) { Set(Key, Value); }
  inline void put (const TK &Key, const TV &Value) { Set(Key, Value); }

  //FIXME: this is dog slow!
  bool Remove (const TK &Key) {
    bool Removed = false;
    int HashIndex = GetTypeHash(Key)&(HashSize-1);
    for (int i = HashTable[HashIndex]; i != -1; i = Pairs[i].HashNext) {
      if (Pairs[i].Key == Key) {
        Pairs.RemoveIndex(i);
        Removed = true;
        break;
      }
    }
    if (Removed) Relax();
    return Removed;
  }
  inline bool remove (const TK &Key) { return Remove(Key); }

  bool has (const TK &Key) const {
    int HashIndex = GetTypeHash(Key)&(HashSize-1);
    for (int i = HashTable[HashIndex]; i != -1; i = Pairs[i].HashNext) {
      if (Pairs[i].Key == Key) return true;
    }
    return false;
  }

  //WARNING! returned pointer will be invalidated by any map mutation
  TV *Find (const TK &Key) {
    int HashIndex = GetTypeHash(Key)&(HashSize-1);
    for (int i = HashTable[HashIndex]; i != -1; i = Pairs[i].HashNext) {
      if (Pairs[i].Key == Key) return &Pairs[i].Value;
    }
    return nullptr;
  }
  inline TV *find (const TK &Key) { return Find(Key); }

  //WARNING! returned pointer will be invalidated by any map mutation
  const TV *Find (const TK &Key) const {
    int HashIndex = GetTypeHash(Key)&(HashSize-1);
    for (int i = HashTable[HashIndex]; i != -1; i = Pairs[i].HashNext) {
      if (Pairs[i].Key == Key) return &Pairs[i].Value;
    }
    return nullptr;
  }
  inline const TV *find (const TK &Key) const { return Find(Key); }

  const TV FindPtr (const TK &Key) const {
    int HashIndex = GetTypeHash(Key)&(HashSize-1);
    for (int i = HashTable[HashIndex]; i != -1; i = Pairs[i].HashNext) {
      if (Pairs[i].Key == Key) return Pairs[i].Value;
    }
    return nullptr;
  }
  inline const TV *findptr (const TK &Key) const { return FindPtr(Key); }

  class TIterator {
  private:
    TMap &Map;
    vint32 Index;
  public:
    TIterator (TMap &AMap) : Map(AMap), Index(0) {}
    inline operator bool () const { return Index < Map.Pairs.Num(); }
    inline void operator ++ () { ++Index; }
    inline const TK &GetKey () const { return Map.Pairs[Index].Key; }
    inline const TV &GetValue () const { return Map.Pairs[Index].Value; }
    inline void RemoveCurrent () { Map.Pairs.RemoveIndex(Index); Map.Relax(); --Index; }
  };

  friend class TIterator;
};
