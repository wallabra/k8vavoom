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
//**  Streams for loading and saving data.
//**
//**************************************************************************


class VStr;
class VLevel;
class VLevelInfo;
class VStream;


// eh... need to be here, so we can virtual method for it
class VLevelScriptThinker {
public:
  bool destroyed; // script `Destroy()` method should set this (and check to avoid double destroying)
  VLevel *XLevel; // level object
  VLevelInfo *Level; // level info object

public:
  VLevelScriptThinker () : destroyed(false), XLevel(nullptr), Level(nullptr) {}

  virtual ~VLevelScriptThinker () {
    if (!destroyed) Sys_Error("trying to delete unfinalized Acs script");
  }

  inline void DestroyThinker () { Destroy(); }

  // it is guaranteed that `Destroy()` will be called by master before deleting the object
  virtual void Destroy () = 0;
  virtual void Serialise (VStream &) = 0;
  virtual void ClearReferences () = 0;
  virtual void Tick (float DeltaTime) = 0;

  virtual VStr DebugDumpToString () = 0;
};


// base class for various streams
class VStream {
protected:
  bool bLoading; // are we loading or saving?
  bool bError; // did we have any errors?
  static const VStr mEmptyName;

public:
  VStream () : bLoading(false) , bError(false) {}
  virtual ~VStream ();

  // status requests
  inline bool IsLoading () const { return bLoading;}
  inline bool IsSaving () const { return !bLoading; }
  inline bool IsError () const { return bError; }

  inline void Serialize (void *buf, int len) { Serialise(buf, len); }
  inline void Serialize (const void *buf, int len) { Serialise(buf, len); }
  void Serialise (const void *buf, int len); // only write

  // stream interface
  virtual const VStr &GetName () const;
  virtual void Serialise (void *Data, int Length);
  virtual void SerialiseBits (void *Data, int Length);
  virtual void SerialiseInt (vuint32 &, vuint32);
  virtual void Seek (int);
  virtual int Tell ();
  virtual int TotalSize ();
  virtual bool AtEnd ();
  virtual void Flush ();
  virtual bool Close ();

  // interface functions for objects and classes streams
  virtual VStream &operator << (VName &);
  virtual VStream &operator << (VObject *&);
  virtual void SerialiseStructPointer (void *&, VStruct *);
  virtual VStream &operator << (VMemberBase *&);

  virtual VStream &operator << (VLevelScriptThinker *&);

  // serialise integers in particular byte order
  void SerialiseLittleEndian (void *, int);
  void SerialiseBigEndian (void*, int);

  // stream serialisation operators
  friend VStream &operator << (VStream &Strm, vint8 &Val) { Strm.Serialise(&Val, 1); return Strm; }
  friend VStream &operator << (VStream &Strm, vuint8 &Val) { Strm.Serialise(&Val, 1); return Strm; }
  friend VStream &operator << (VStream &Strm, vint16 &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
  friend VStream &operator << (VStream &Strm, vuint16 &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
  friend VStream &operator << (VStream &Strm, vint32 &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
  friend VStream &operator << (VStream &Strm, vuint32 &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
  friend VStream &operator << (VStream &Strm, float &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
  friend VStream &operator << (VStream &Strm, double &Val) { Strm.SerialiseLittleEndian(&Val, sizeof(Val)); return Strm; }
};


// stream reader helper
template<class T> T Streamer (VStream &Strm) {
  T Val;
  Strm << Val;
  return Val;
}


// class for serialising integer values in a compact way
class VStreamCompactIndex {
public:
  vint32 Val;
  friend VStream &operator << (VStream &, VStreamCompactIndex &);
};
#define STRM_INDEX(val)  (*(VStreamCompactIndex *)&(val))

class VStreamCompactIndexU {
public:
  vuint32 Val;
  friend VStream &operator << (VStream &, VStreamCompactIndexU &);
};
#define STRM_INDEX_U(val)  (*(VStreamCompactIndexU *)&(val))
